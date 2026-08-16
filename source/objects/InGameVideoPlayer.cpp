#include "InGameVideoPlayer.hpp"
#include "../backend/Conductor.hpp"
#include <zlib.h>
#include <string.h>

InGameVideoPlayer::InGameVideoPlayer(const std::string& videoPath, bool inFrontOfHUD, bool loop)
    : inFrontOfHUD(inFrontOfHUD), loop(loop), isPaused(false), finished(false), alpha(1.0f),
      path(videoPath), file(nullptr), width(0), height(0), fps(24), format(0), totalFrames(0), isZlib(true),
      currentFrame(0), displayedFrame(0), videoTimer(0.0f),
      songStartPosition(-1.0), lastTransferReadIdx(-1), gpuTransferPending(false), targetFrameCached(0), skipToNextKey(false),
      firstFrameReady(false),
      threadRunning(false), videoEnded(false)
{
    for (int i = 0; i < RING_SIZE; i++) decodeBuf[i] = nullptr;
    for (int i = 0; i < RING_SIZE; i++) ringFrameIds[i] = -1;
    linearBuffer[0] = nullptr;
    linearBuffer[1] = nullptr;
    tex[0].data = nullptr;
    tex[1].data = nullptr;

    std::string resolvedPath = videoPath;
    if (resolvedPath.find("/") == std::string::npos && resolvedPath.find("\\") == std::string::npos) {
        std::string videoName = resolvedPath;
        if (videoName.size() < 6 || videoName.substr(videoName.size() - 6) != ".snaky") {
            videoName += ".snaky";
        }
        resolvedPath = Paths::getPath(videoName, "videos");
    }

    file = fopen(resolvedPath.c_str(), "rb");
    if (!file) {
        finished = true;
        return;
    }

    readHeader();
    if (width == 0 || height == 0) {
        fclose(file);
        file = nullptr;
        finished = true;
        return;
    }

    for (int i = 0; i < RING_SIZE; i++) {
        decodeBuf[i] = (uint16_t*)linearAlloc(width * height * 2);
        if (decodeBuf[i]) memset(decodeBuf[i], 0, width * height * 2);
    }
    linearBuffer[0] = (uint16_t*)linearAlloc(512 * 256 * 2);
    linearBuffer[1] = (uint16_t*)linearAlloc(512 * 256 * 2);
    if (linearBuffer[0]) memset(linearBuffer[0], 0, 512 * 256 * 2);
    if (linearBuffer[1]) memset(linearBuffer[1], 0, 512 * 256 * 2);

    for (int i = 0; i < 2; i++) {
        C3D_TexInit(&tex[i], 512, 256, GPU_RGB565);
        C3D_TexSetFilter(&tex[i], GPU_LINEAR, GPU_LINEAR);
    }

    subtex.width  = width;
    subtex.height = height;
    subtex.left   = 0.0f;
    subtex.right  = (float)width / 512.0f;
    subtex.top    = 1.0f;
    subtex.bottom = 1.0f - (float)height / 256.0f;

    compressedBuf.resize(512 * 1024);
    uncompressedBuf.resize(width * height * 2);

    LightLock_Init(&decodeLock);
    LightEvent_Init(&eventDecodeRequest, RESET_ONESHOT);

    threadRunning = true;
    decodeThread = threadCreate(threadMain, this, 32 * 1024, 0x1A, -2, false);
}

InGameVideoPlayer::~InGameVideoPlayer() {
    threadRunning = false;
    LightEvent_Signal(&eventDecodeRequest);
    if (decodeThread) {
        threadJoin(decodeThread, U64_MAX);
        threadFree(decodeThread);
        decodeThread = nullptr;
    }

    if (file) {
        fclose(file);
        file = nullptr;
    }

    for (int i = 0; i < RING_SIZE; i++) {
        if (decodeBuf[i]) linearFree(decodeBuf[i]);
    }
    if (linearBuffer[0]) linearFree(linearBuffer[0]);
    if (linearBuffer[1]) linearFree(linearBuffer[1]);

    for (int i = 0; i < 2; i++) {
        C3D_TexDelete(&tex[i]);
    }
}

void InGameVideoPlayer::readHeader() {
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

    uint16_t audioRate;
    uint8_t channels, audioFormat;
    fread(&audioRate, 2, 1, file);
    fread(&channels, 1, 1, file);
    fread(&audioFormat, 1, 1, file);

    uint32_t offsets[3];
    fread(offsets, 4, 3, file);
}

void InGameVideoPlayer::restartVideo() {
    if (!file) return;
    fseek(file, 0, SEEK_SET);
    readHeader();
    currentFrame = 0;
    displayedFrame = 0;
    videoTimer = 0.0f;
    songStartPosition = -1.0;
    for (int i = 0; i < RING_SIZE; i++) ringFrameIds[i] = -1;
    skipToNextKey = false;
    videoEnded = false;
}

void InGameVideoPlayer::decodeChunk() {
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

    if (type == 0x03) { // AUDIO chunk — skip completely to save RAM
        fseek(file, size, SEEK_CUR);
    }
    else if (type == 0x01 || type == 0x02) { // VIDEO KEY/DELTA
        // FRAME SKIP: when far behind, skip delta frames cheaply (just seek past them).
        // KEY frames (0x01) are self-contained and MUST be decoded to reset frame state.
        // DELTA frames (0x02) can be skipped because the next KEY will give us a clean base.
        if (type == 0x02 && skipToNextKey) {
            if (isZlib) {
                uint8_t uncompBytes[3];
                fread(uncompBytes, 1, 3, file); // read+discard uncompressed size
                fseek(file, size, SEEK_CUR);    // seek past compressed payload
            } else {
                fseek(file, size, SEEK_CUR);
            }
            currentFrame++; // keep file-position counter in sync
            return;          // do NOT produce a ring entry for this frame
        }
        if (type == 0x01) skipToNextKey = false; // KEY frame clears the skip flag

        if (isZlib) {
            uint8_t uncompBytes[3];
            fread(uncompBytes, 1, 3, file);
            uint32_t uncompSize = uncompBytes[0] | (uncompBytes[1] << 8) | (uncompBytes[2] << 16);

            uLongf destLen = uncompSize;
            if (size > compressedBuf.size()) compressedBuf.resize(size);
            if (uncompSize > uncompressedBuf.size()) uncompressedBuf.resize(uncompSize);

            fread(compressedBuf.data(), 1, size, file);
            uncompress((Bytef*)uncompressedBuf.data(), &destLen, (const Bytef*)compressedBuf.data(), size);

            uint8_t*  data = uncompressedBuf.data();
            uint8_t*  end  = data + uncompSize;
            uint16_t* ptr  = decodeBuf[writeIdx];
            if (!ptr) return;

            if (type == 0x02) { // DELTA
                int prevIdx = (writeIdx + RING_SIZE - 1) % RING_SIZE;
                if (decodeBuf[prevIdx]) memcpy(ptr, decodeBuf[prevIdx], width * height * 2);
            } else if (type == 0x01) { // KEY
                memset(ptr, 0, width * height * 2);
            }

            while (data < end) {
                uint8_t op = *data++;
                if (op == 0x00) break;

                if (op == 0x01) { // SKIP
                    uint16_t count = data[0] | (data[1] << 8);
                    data += 2;
                    ptr += count;
                } else if (op == 0x02) { // COPY
                    uint16_t count = data[0] | (data[1] << 8);
                    data += 2;
                    if (count <= 16) {
                        for (int i = 0; i < count; i++) {
                            ptr[i] = data[0] | (data[1] << 8);
                            data += 2;
                        }
                    } else {
                        memcpy(ptr, data, count * 2);
                        data += count * 2;
                    }
                    ptr += count;
                }
            }
        }

        // Tag this ring slot with the actual video frame number before advancing writeIdx.
        // update() reads this to know which frame it just displayed.
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

void InGameVideoPlayer::threadMain(void* arg) {
    InGameVideoPlayer* self = (InGameVideoPlayer*)arg;
    while (self->threadRunning) {
        if (self->isPaused || self->videoEnded) {
            svcSleepThread(1000000);
            continue;
        }

        bool canWrite = false;
        LightLock_Lock(&self->decodeLock);
        canWrite = (self->ringCount < RING_SIZE) && (self->writeIdx != self->lockedReadIdx);
        LightLock_Unlock(&self->decodeLock);

        if (canWrite) {
            // Skip delta frames when the decoder is too far behind the playback target.
            // Threshold: more than RING_SIZE frames behind — those ring entries would be
            // discarded immediately by update() anyway.
            self->skipToNextKey = (self->targetFrameCached - self->currentFrame) > RING_SIZE;
            self->decodeChunk();
        } else {
            LightEvent_Wait(&self->eventDecodeRequest);
            LightEvent_Clear(&self->eventDecodeRequest);
        }
    }
}

void InGameVideoPlayer::update(float dt) {
    if (finished || isPaused) return;

    // Anchor songStartPosition the first time update() is called after start
    if (songStartPosition < 0.0) {
        songStartPosition = Conductor::songPosition;
    }

    // Compute the target frame from the song clock — this is always accurate
    // regardless of frame-rate drops, pauses, etc.
    double elapsed = (Conductor::songPosition - songStartPosition) / 1000.0; // ms -> s
    if (elapsed < 0.0) elapsed = 0.0;
    float frameTime = 1.0f / (float)(fps > 0 ? fps : 24);
    int targetFrame = (int)(elapsed / frameTime);
    // Share targetFrame with the decode thread so it can skip when behind
    targetFrameCached = targetFrame;

    // The DMA transfer at 512x256 RGB565 (~256KB) takes well under 1ms on 3DS hardware.
    // By the time the next update() fires (at least one frame later, ~16ms), it is
    // always complete. Just finalize the handback to the decode thread here.
    if (gpuTransferPending) {
        if (lastTransferReadIdx != -1) {
            LightLock_Lock(&decodeLock);
            lockedReadIdx = -1;
            LightLock_Unlock(&decodeLock);
            LightEvent_Signal(&eventDecodeRequest);
            lastTransferReadIdx = -1;
        }
        gpuTransferPending = false;
        firstFrameReady = true;
    }

    // Consume decoded frames up to targetFrame (skip frames if we're behind)
    int lastReadIdx = -1;
    while (displayedFrame < targetFrame) {
        bool ready = false;
        LightLock_Lock(&decodeLock);
        ready = (ringCount > 0);
        LightLock_Unlock(&decodeLock);

        if (ready) {
            LightLock_Lock(&decodeLock);
            lastReadIdx = readIdx;
            readIdx = (readIdx + 1) % RING_SIZE;
            ringCount--;
            lockedReadIdx = lastReadIdx;
            LightLock_Unlock(&decodeLock);
            LightEvent_Signal(&eventDecodeRequest);
            // Use the actual frame number stored in this ring slot.
            // If the decoder skipped to a KEY frame past targetFrame, displayedFrame
            // will exceed targetFrame and the while condition will stop the drain —
            // the video waits at that frame until the song clock catches up.
            displayedFrame = ringFrameIds[lastReadIdx];
        } else {
            break; // Decoder thread hasn't caught up yet
        }
    }

    if (videoEnded && ringCount == 0) {
        if (loop) {
            restartVideo();
        } else {
            finished = true;
        }
        return;
    }

    // Kick off an async GPU transfer for the latest decoded frame
    if (!gpuTransferPending && lastReadIdx != -1 && decodeBuf[lastReadIdx]) {
        currentLinearBuffer = (currentLinearBuffer + 1) % 2;
        uint16_t* curBuf = linearBuffer[currentLinearBuffer];
        if (curBuf) {
            for (int y = 0; y < height; y++) {
                memcpy(&curBuf[y * 512], &decodeBuf[lastReadIdx][y * width], width * 2);
            }
            GSPGPU_FlushDataCache(curBuf, 512 * 256 * 2);

            currentTex = (currentTex + 1) % 2;

            // Async (non-blocking) transfer — returns immediately, DMA runs in background
            GX_DisplayTransfer(
                (u32*)curBuf,            GX_BUFFER_DIM(512, 256),
                (u32*)tex[currentTex].data, GX_BUFFER_DIM(512, 256),
                GX_TRANSFER_FLIP_VERT(0) |
                GX_TRANSFER_OUT_TILED(1) |
                GX_TRANSFER_RAW_COPY(0)  |
                GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565)  |
                GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
                GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
            );

            lastTransferReadIdx = lastReadIdx;
            gpuTransferPending = true;
        }
    }
}

void InGameVideoPlayer::draw(float stageAlpha) {
    if (finished || width == 0 || height == 0 || !firstFrameReady) return;
    if (!tex[currentTex].data) return;

    img.tex = &tex[currentTex];
    img.subtex = &subtex;

    float drawAlpha = alpha * stageAlpha;
    if (drawAlpha <= 0.0f) return;

    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, drawAlpha);

    float scaleX = 400.0f / (float)width;
    float h = (float)height * scaleX;
    float y = (240.0f - h) / 2.0f;

    C2D_DrawImageAt(img, 0, y, 0.5f, &tint, scaleX, scaleX);
}
