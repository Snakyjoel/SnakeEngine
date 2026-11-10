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
        ndspChnSetPaused(5, true);
        ndspChnReset(5);
        ndspChnSetInterp(5, NDSP_INTERP_LINEAR);
        ndspChnSetRate(5, audioRate);
        ndspChnSetFormat(5, channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

        int maxChunkSamples = std::max((int)(audioRate / 5), 4096);
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
    adpcmState.predictor = 0;
    adpcmState.stepIndex = 0;
    adpcmStateR.predictor = 0;
    adpcmStateR.stepIndex = 0;

    // Prefer affinity Core 1 (SysCore with up to 80% CPU limit) so video decoding doesn't starve the main gameplay core
    decodeThread = threadCreate(threadMain, this, 32 * 1024, 0x22, 1, false);
    if (!decodeThread) {
        // Fallback to ANY core if Core 1 not allowed in current environment
        decodeThread = threadCreate(threadMain, this, 32 * 1024, 0x28, -2, false);
    }
}

void SnakyDecoder::stop() {
    threadRunning = false;
    LightEvent_Signal(&eventDecodeRequest);
    if (decodeThread) {
        threadJoin(decodeThread, 2000000000ULL); // 2s safe timeout
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
//skibidi
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

    // Read 4-byte chunk header (1 byte type + 3 bytes little-endian payload size) in one syscall
    uint32_t chunkHdr = 0;
    if (fread(&chunkHdr, 4, 1, file) != 1) {
        videoEnded = true;
        return;
    }
    uint8_t type = (uint8_t)(chunkHdr & 0xFF);
    uint32_t size = (chunkHdr >> 8) & 0xFFFFFF;

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
                    if (channels == 2) {
                        AdpcmDecoder::decodeIMAStereo(adpcmAudioBuf.data(), size, waveBuf[idx].data_pcm16, samplesDecoded, adpcmState, adpcmStateR);
                    } else {
                        AdpcmDecoder::decodeIMA(adpcmAudioBuf.data(), size, waveBuf[idx].data_pcm16, samplesDecoded, adpcmState);
                    }
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
            fseek(file, isZlib ? (size + 3) : size, SEEK_CUR);
            currentFrame++;
            return;
        }
        if (type == 0x01) skipToNextKey = false;

        uint8_t* data = nullptr;
        uint32_t uncompSize = 0;

        if (isZlib) {
            uint32_t uncompWord = 0;
            fread(&uncompWord, 3, 1, file);
            uncompSize = uncompWord & 0xFFFFFF;

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
        // If DELTA frame (0x02), carry forward the previous frame buffer (only active height rows).
        if (type == 0x01) {
            memset(destBase, 0, PADDED_WIDTH * height * 2);
        } else if (type == 0x02) {
            int prevIdx = (writeIdx + RING_SIZE - 1) % RING_SIZE;
            if (decodeBuf[prevIdx] && decodeBuf[prevIdx] != destBase) {
                memcpy(destBase, decodeBuf[prevIdx], PADDED_WIDTH * height * 2);
            }
        }

        // Fast Direct-Strided RLE Opcode Unpacker with zero per-chunk software division
        uint32_t currX = 0;
        uint32_t currY = 0;

        while (data < end) {
            uint8_t op = *data++;
            if (op == 0x00) break; // End of frame

            if (op == 0x01) { // SKIP - fast subtraction loop, zero division cycles
                uint16_t count = *(uint16_t*)data;
                data += 2;
                currX += count;
                while (currX >= width) {
                    currX -= width;
                    currY++;
                }
            } else if (op == 0x02) { // COPY
                uint16_t count = *(uint16_t*)data;
                data += 2;

                if (format == 1) { // RGB444 packed (12-bit / 1.5 bytes per pixel)
                    for (uint32_t k = 0; k < count; k += 2) {
                        uint16_t* rowPtr = destBase + (currY << 9);
                        if (k + 1 < count) { // Pair of pixels (3 bytes -> 2 GPU_RGBA4 pixels)
                            uint8_t b0 = data[0];
                            uint8_t b1 = data[1];
                            uint8_t b2 = data[2];
                            data += 3;

                            uint16_t p0 = (uint16_t)((b0 << 8) | (b1 & 0xF0) | 0x0F);
                            uint16_t p1 = (uint16_t)(((b1 & 0x0F) << 12) | (b2 << 4) | 0x0F);

                            rowPtr[currX] = p0;
                            currX++;
                            if (currX == width) { currX = 0; currY++; }

                            rowPtr = destBase + (currY << 9);
                            rowPtr[currX] = p1;
                            currX++;
                            if (currX == width) { currX = 0; currY++; }
                        } else { // Single trailing odd pixel (2 bytes -> 1 GPU_RGBA4 pixel)
                            uint8_t b0 = data[0];
                            uint8_t b1 = data[1];
                            data += 2;

                            uint16_t p0 = (uint16_t)((b0 << 8) | (b1 & 0xF0) | 0x0F);
                            rowPtr[currX] = p0;
                            currX++;
                            if (currX == width) { currX = 0; currY++; }
                        }
                    }
                } else { // 16-bit per pixel formats (RGB565) - Fast strided unpack without inner software division
                    uint32_t copied = 0;
                    while (copied < count) {
                        uint32_t spaceInRow = width - currX;
                        uint32_t chunk = std::min((uint32_t)(count - copied), spaceInRow);

                        uint16_t* rowPtr = destBase + (currY << 9) + currX;
                        const uint16_t* srcPtr = (const uint16_t*)(data + (copied * 2));

                        if (chunk == 1) {
                            rowPtr[0] = srcPtr[0];
                        } else if (chunk == 2) {
                            rowPtr[0] = srcPtr[0];
                            rowPtr[1] = srcPtr[1];
                        } else if (chunk == 3) {
                            rowPtr[0] = srcPtr[0];
                            rowPtr[1] = srcPtr[1];
                            rowPtr[2] = srcPtr[2];
                        } else if (chunk == 4) {
                            rowPtr[0] = srcPtr[0];
                            rowPtr[1] = srcPtr[1];
                            rowPtr[2] = srcPtr[2];
                            rowPtr[3] = srcPtr[3];
                        } else {
                            memcpy(rowPtr, srcPtr, chunk * 2);
                        }

                        copied += chunk;
                        currX += chunk;
                        if (currX >= width) {
                            currX = 0;
                            currY++;
                        }
                    }
                    data += count * 2;
                }
            }
        }

        // Flush CPU cache in decoder thread (Core 1) so main thread GPU transfer is immediate
        GSPGPU_FlushDataCache(destBase, PADDED_WIDTH * height * 2);

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
