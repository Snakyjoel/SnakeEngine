#include "SnakyDecoder.hpp"
#include "AdpcmDecoder.hpp"
#include <string.h>
#include <malloc.h>
#include <algorithm>

SnakyDecoder::SnakyDecoder() {
    for (int i = 0; i < RING_SIZE; i++) {
        decodeBuf[i] = nullptr;
        ringFrameIds[i] = -1;
    }
    memset(&zstream, 0, sizeof(zstream));
    compressedBuf.resize(512 * 1024);
    uncompressedBuf.resize(PADDED_WIDTH * PADDED_HEIGHT * 2);
}

SnakyDecoder::~SnakyDecoder() {
    close();
}

bool SnakyDecoder::open(const std::string& videoPath, bool includeAudio) {
    close();
    path = videoPath;
    includeAudioTrack = includeAudio;

    file = fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }

    setvbuf(file, nullptr, _IOFBF, 512 * 1024);

    readHeader();
    if (width == 0 || height == 0) {
        fclose(file);
        file = nullptr;
        return false;
    }

    // Allocate 512x256 pitch linear buffers in 3DS FCRAM
    for (int i = 0; i < RING_SIZE; i++) {
        decodeBuf[i] = (uint16_t*)linearAlloc(PADDED_WIDTH * PADDED_HEIGHT * 2);
        if (decodeBuf[i]) memset(decodeBuf[i], 0, PADDED_WIDTH * PADDED_HEIGHT * 2);
        ringFrameIds[i] = -1;
    }

    // Initialize persistent zlib inflater
    if (isZlib) {
        zstream.zalloc = Z_NULL;
        zstream.zfree = Z_NULL;
        zstream.opaque = Z_NULL;
        zstream.avail_in = 0;
        zstream.next_in = Z_NULL;
        if (inflateInit(&zstream) == Z_OK) {
            zstreamInitialized = true;
        }
    }

    // Initialize NDSP audio buffer if present & requested
    if (hasAudio && includeAudioTrack) {
        ndspChnReset(5);
        ndspChnSetInterp(5, NDSP_INTERP_LINEAR);
        ndspChnSetRate(5, audioRate);
        ndspChnSetFormat(5, channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

        int maxChunkSamples = audioRate / 10;
        audioBufferData = (int16_t*)linearAlloc(maxChunkSamples * channels * 2 * AUDIO_BUF_COUNT);

        float mix[12];
        memset(mix, 0, sizeof(mix));
        mix[0] = 1.0f;
        mix[1] = 1.0f;
        ndspChnSetMix(5, mix);
        ndspChnSetPaused(5, false);

        memset(waveBuf, 0, sizeof(waveBuf));
        for (int i = 0; i < AUDIO_BUF_COUNT; i++) {
            waveBuf[i].data_vaddr = &audioBufferData[i * maxChunkSamples * channels];
            waveBuf[i].status = NDSP_WBUF_FREE;
        }
    }

    LightLock_Init(&decodeLock);
    LightEvent_Init(&eventDecodeRequest, RESET_ONESHOT);

    return true;
}

void SnakyDecoder::start() {
    if (!file || threadRunning) return;

    threadRunning = true;
    writeIdx = 0;
    readIdx = 0;
    ringCount = 0;
    lockedReadIdx = -1;
    currentFrame = 0;
    audioSamplesPlayed = 0;
    videoEnded = false;
    skipToNextKey = false;

    // Use THREAD_CPU_AFFINITY_ANY (-2) so threadCreate succeeds on all 3DS hardware models
    decodeThread = threadCreate(threadMain, this, 32 * 1024, 0x1A, -2, false);
}

void SnakyDecoder::stop() {
    threadRunning = false;
    LightEvent_Signal(&eventDecodeRequest);
    if (decodeThread) {
        threadJoin(decodeThread, U64_MAX);
        threadFree(decodeThread);
        decodeThread = nullptr;
    }
}

void SnakyDecoder::close() {
    stop();

    if (zstreamInitialized) {
        inflateEnd(&zstream);
        zstreamInitialized = false;
    }

    if (file) {
        fclose(file);
        file = nullptr;
    }

    for (int i = 0; i < RING_SIZE; i++) {
        if (decodeBuf[i]) {
            linearFree(decodeBuf[i]);
            decodeBuf[i] = nullptr;
        }
        ringFrameIds[i] = -1;
    }

    if (hasAudio && includeAudioTrack && audioBufferData) {
        ndspChnWaveBufClear(5);
        linearFree(audioBufferData);
        audioBufferData = nullptr;
    }
}

void SnakyDecoder::readHeader() {
    char magic[4];
    if (fread(magic, 1, 4, file) != 4) return;
    if (strncmp(magic, "SNKY", 4) != 0) return;

    uint16_t version;
    fread(&version, 2, 1, file);
    isZlib = (version >= 0x0200);
    fread(&width, 2, 1, file);
    fread(&height, 2, 1, file);
    fread(&fps, 1, 1, file);
    fread(&format, 1, 1, file);
    fread(&totalFrames, 4, 1, file);

    fread(&audioRate, 2, 1, file);
    fread(&channels, 1, 1, file);
    fread(&audioFormat, 1, 1, file);

    uint32_t offsets[3];
    fread(offsets, 4, 3, file);

    hasAudio = (audioFormat == 1 || audioFormat == 2);
}

float SnakyDecoder::getAudioMasterTime(float fallbackTimer) {
    if (!hasAudio || !includeAudioTrack) return fallbackTimer;
    updateAudioTracking();
    LightLock_Lock(&decodeLock);
    uint64_t samples = audioSamplesPlayed;
    LightLock_Unlock(&decodeLock);
    uint32_t pos = ndspChnGetSamplePos(5);
    return (float)(samples + pos) / (float)audioRate;
}

void SnakyDecoder::updateAudioTracking() {
    if (!hasAudio || !includeAudioTrack) return;
    LightLock_Lock(&decodeLock);
    for (int i = 0; i < AUDIO_BUF_COUNT; i++) {
        if (waveBuf[i].status == NDSP_WBUF_DONE) {
            audioSamplesPlayed += waveBuf[i].nsamples;
            waveBuf[i].status = NDSP_WBUF_FREE;
        }
    }
    LightLock_Unlock(&decodeLock);
}

void SnakyDecoder::setTargetFrame(int targetFrame) {
    targetFrameCached = targetFrame;
}

bool SnakyDecoder::getNextFrame(uint16_t** outBuffer, int& outFrameId, int& outRingSlot) {
    LightLock_Lock(&decodeLock);
    if (ringCount <= 0) {
        LightLock_Unlock(&decodeLock);
        return false;
    }

    outRingSlot = readIdx;
    readIdx = (readIdx + 1) % RING_SIZE;
    ringCount--;
    lockedReadIdx = outRingSlot;
    outBuffer[0] = decodeBuf[outRingSlot];
    outFrameId = ringFrameIds[outRingSlot];
    LightLock_Unlock(&decodeLock);

    LightEvent_Signal(&eventDecodeRequest);
    return true;
}

void SnakyDecoder::releaseFrame(int slotIndex) {
    LightLock_Lock(&decodeLock);
    if (lockedReadIdx == slotIndex) {
        lockedReadIdx = -1;
    }
    LightLock_Unlock(&decodeLock);
    LightEvent_Signal(&eventDecodeRequest);
}

void SnakyDecoder::decodeChunk() {
    if (!file) return;

    uint8_t type;
    if (fread(&type, 1, 1, file) != 1) {
        videoEnded = true;
        return;
    }

    uint8_t sizeBytes[3];
    if (fread(sizeBytes, 1, 3, file) != 3) {
        videoEnded = true;
        return;
    }
    uint32_t size = sizeBytes[0] | (sizeBytes[1] << 8) | (sizeBytes[2] << 16);

    if (type == 0x03) { // AUDIO
        if (hasAudio && includeAudioTrack) {
            int idx = -1;
            while (threadRunning) {
                updateAudioTracking();
                for (int i = 0; i < AUDIO_BUF_COUNT; i++) {
                    if (waveBuf[i].status == NDSP_WBUF_FREE) {
                        idx = i;
                        break;
                    }
                }
                if (idx != -1) break;
                svcSleepThread(1000000);
            }

            if (idx != -1) {
                if (audioFormat == 2) {
                    if (size > adpcmAudioBuf.size()) adpcmAudioBuf.resize(size);
                    fread(adpcmAudioBuf.data(), 1, size, file);
                    uint32_t samplesDecoded = 0;
                    AdpcmDecoder::decodeIMA(adpcmAudioBuf.data(), size, waveBuf[idx].data_pcm16, samplesDecoded, adpcmState);
                    waveBuf[idx].nsamples = samplesDecoded / channels;
                } else {
                    size_t readBytes = fread(waveBuf[idx].data_pcm16, 1, size, file);
                    waveBuf[idx].nsamples = readBytes / (channels * 2);
                }
                DSP_FlushDataCache(waveBuf[idx].data_pcm16, waveBuf[idx].nsamples * channels * 2);
                ndspChnWaveBufAdd(5, &waveBuf[idx]);
            } else {
                fseek(file, size, SEEK_CUR);
            }
        } else {
            fseek(file, size, SEEK_CUR);
        }
    }
    else if (type == 0x01 || type == 0x02) { // VIDEO KEY (0x01) / DELTA (0x02)
        // Fast-seek through DELTA chunks when behind target frame
        if (type == 0x02 && skipToNextKey) {
            if (isZlib) {
                uint8_t uncompBytes[3];
                fread(uncompBytes, 1, 3, file);
                fseek(file, size, SEEK_CUR);
            } else {
                fseek(file, size, SEEK_CUR);
            }
            currentFrame++;
            return;
        }
        if (type == 0x01) skipToNextKey = false;

        uint8_t* data = nullptr;
        uint32_t uncompSize = 0;

        if (isZlib) {
            uint8_t uncompBytes[3];
            fread(uncompBytes, 1, 3, file);
            uncompSize = uncompBytes[0] | (uncompBytes[1] << 8) | (uncompBytes[2] << 16);

            if (size > compressedBuf.size()) compressedBuf.resize(size);
            if (uncompSize > uncompressedBuf.size()) uncompressedBuf.resize(uncompSize);

            fread(compressedBuf.data(), 1, size, file);

            // Fast Inflation via persistent z_stream
            if (zstreamInitialized) {
                inflateReset(&zstream);
                zstream.next_in = compressedBuf.data();
                zstream.avail_in = size;
                zstream.next_out = uncompressedBuf.data();
                zstream.avail_out = uncompressedBuf.size();
                inflate(&zstream, Z_FINISH);
            } else {
                uLongf destLen = uncompSize;
                uncompress((Bytef*)uncompressedBuf.data(), &destLen, (const Bytef*)compressedBuf.data(), size);
            }

            data = uncompressedBuf.data();
        } else {
            if (size > uncompressedBuf.size()) uncompressedBuf.resize(size);
            fread(uncompressedBuf.data(), 1, size, file);
            data = uncompressedBuf.data();
            uncompSize = size;
        }

        uint8_t* end = data + uncompSize;
        uint16_t* destBase = decodeBuf[writeIdx];
        if (!destBase) return;

        // If KEY frame (0x01), clear padded frame buffer.
        // If DELTA frame (0x02), carry forward the previous frame buffer to avoid full 192KB memcpy!
        if (type == 0x01) {
            memset(destBase, 0, PADDED_WIDTH * PADDED_HEIGHT * 2);
        } else if (type == 0x02) {
            int prevIdx = (writeIdx + RING_SIZE - 1) % RING_SIZE;
            if (decodeBuf[prevIdx] && decodeBuf[prevIdx] != destBase) {
                memcpy(destBase, decodeBuf[prevIdx], PADDED_WIDTH * PADDED_HEIGHT * 2);
            }
        }

        // Fast Direct-Strided RLE Opcode Unpacker
        uint32_t pixelIdx = 0;
        const uint32_t framePixels = (uint32_t)width * height;

        while (data < end) {
            uint8_t op = *data++;
            if (op == 0x00) break; // End of frame

            if (op == 0x01) { // SKIP
                uint16_t count = *(uint16_t*)data;
                data += 2;
                pixelIdx += count;
            } else if (op == 0x02) { // COPY
                uint16_t count = *(uint16_t*)data;
                data += 2;

                if (format == 1) { // RGB444 packed (12-bit / 1.5 bytes per pixel)
                    uint32_t currX = pixelIdx % width;
                    uint32_t currY = pixelIdx / width;

                    for (uint32_t k = 0; k < count; k += 2) {
                        if (k + 1 < count) { // Pair of pixels (3 bytes -> 2 GPU_RGBA4 pixels)
                            uint8_t b0 = data[0];
                            uint8_t b1 = data[1];
                            uint8_t b2 = data[2];
                            data += 3;

                            uint16_t p0 = (uint16_t)((b0 << 8) | (b1 & 0xF0) | 0x0F);
                            uint16_t p1 = (uint16_t)(((b1 & 0x0F) << 12) | (b2 << 4) | 0x0F);

                            destBase[currY * PADDED_WIDTH + currX] = p0;
                            currX++;
                            if (currX == width) { currX = 0; currY++; }

                            destBase[currY * PADDED_WIDTH + currX] = p1;
                            currX++;
                            if (currX == width) { currX = 0; currY++; }
                        } else { // Single trailing odd pixel (2 bytes -> 1 GPU_RGBA4 pixel)
                            uint8_t b0 = data[0];
                            uint8_t b1 = data[1];
                            data += 2;

                            uint16_t p0 = (uint16_t)((b0 << 8) | (b1 & 0xF0) | 0x0F);
                            destBase[currY * PADDED_WIDTH + currX] = p0;
                            currX++;
                            if (currX == width) { currX = 0; currY++; }
                        }
                    }
                    pixelIdx += count;
                } else { // 16-bit per pixel formats
                    uint32_t copied = 0;
                    while (copied < count && pixelIdx < framePixels) {
                        uint32_t x = pixelIdx % width;
                        uint32_t y = pixelIdx / width;
                        uint32_t spaceInRow = width - x;
                        uint32_t chunk = std::min((uint32_t)(count - copied), spaceInRow);

                        uint16_t* rowPtr = destBase + (y * PADDED_WIDTH) + x;
                        memcpy(rowPtr, data + (copied * 2), chunk * 2);

                        copied += chunk;
                        pixelIdx += chunk;
                    }
                    data += count * 2;
                }
            }
        }

        ringFrameIds[writeIdx] = currentFrame;

        LightLock_Lock(&decodeLock);
        writeIdx = (writeIdx + 1) % RING_SIZE;
        ringCount++;
        LightLock_Unlock(&decodeLock);

        currentFrame++;
    } else {
        fseek(file, size, SEEK_CUR);
    }
}

void SnakyDecoder::threadMain(void* arg) {
    SnakyDecoder* self = (SnakyDecoder*)arg;

    while (self->threadRunning) {
        if (self->isPaused) {
            svcSleepThread(1000000);
            continue;
        }

        if (self->videoEnded) {
            bool audioPlaying = false;
            if (self->hasAudio && self->includeAudioTrack) {
                self->updateAudioTracking();
                for (int i = 0; i < AUDIO_BUF_COUNT; i++) {
                    if (self->waveBuf[i].status != NDSP_WBUF_FREE) {
                        audioPlaying = true;
                    }
                }
            }
            if (!audioPlaying) {
                break;
            }
            svcSleepThread(1000000);
            continue;
        }

        bool canWrite = false;
        LightLock_Lock(&self->decodeLock);
        canWrite = (self->ringCount < RING_SIZE) && (self->writeIdx != self->lockedReadIdx);
        LightLock_Unlock(&self->decodeLock);

        if (canWrite) {
            // Trigger skip flag when decoder thread is behind target frame
            self->skipToNextKey = (self->targetFrameCached - self->currentFrame) > RING_SIZE;
            self->decodeChunk();
        } else {
            LightEvent_Wait(&self->eventDecodeRequest);
            LightEvent_Clear(&self->eventDecodeRequest);
        }
    }
}
