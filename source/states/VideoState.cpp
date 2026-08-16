#include "VideoState.hpp"
#include <string.h>
#include <malloc.h>
#include "../backend/AudioEngine.hpp"
#include "backend/Conductor.hpp"
#include <zlib.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif



static void drawProgressRing(float cx, float cy, float r_in, float r_out, float progress, u32 color, float depth = 0.9f) {
    if (progress <= 0.0f) return;
    if (progress > 1.0f) progress = 1.0f;

    // Number of segments for the full circle
    const int maxSegments = 40;
    int segmentsToDraw = (int)(progress * maxSegments);
    if (segmentsToDraw < 1) segmentsToDraw = 1;

    for (int i = 0; i < segmentsToDraw; i++) {
        float theta1 = -M_PI / 2.0f + ((float)i * 2.0f * M_PI / (float)maxSegments);
        float theta2 = -M_PI / 2.0f + (((float)i + 1.0f) * 2.0f * M_PI / (float)maxSegments);

        float cos1 = cosf(theta1);
        float sin1 = sinf(theta1);
        float cos2 = cosf(theta2);
        float sin2 = sinf(theta2);

        float ix1 = cx + r_in * cos1;
        float iy1 = cy + r_in * sin1;
        float ix2 = cx + r_in * cos2;
        float iy2 = cy + r_in * sin2;
 
        float ox1 = cx + r_out * cos1;
        float oy1 = cy + r_out * sin1;
        float ox2 = cx + r_out * cos2;
        float oy2 = cy + r_out * sin2;

        C2D_DrawTriangle(ox1, oy1, color, ix1, iy1, color, ix2, iy2, color, depth);
        C2D_DrawTriangle(ox1, oy1, color, ix2, iy2, color, ox2, oy2, color, depth);
    }
}

VideoState::VideoState(const std::string& videoPath, MusicBeatState* nextState, bool debugUI) {
    std::string resolvedPath = videoPath;
    if (resolvedPath.find("romfs:/") != 0 && resolvedPath.find("sdmc:/") != 0) {
        std::string videoName = resolvedPath;
        if (videoName.size() < 6 || videoName.substr(videoName.size() - 6) != ".snaky") {
            videoName += ".snaky";
        }
        resolvedPath = Paths::getPath(videoName, "videos");
    }
    path = resolvedPath;
    m_nextState = nextState;
    file = nullptr;
    for(int i = 0; i < RING_SIZE; i++) decodeBuf[i] = nullptr;
    linearBuffer[0] = nullptr;
    linearBuffer[1] = nullptr;
    tex[0].data = nullptr;
    tex[1].data = nullptr;
    textBuf = nullptr;
    decodeThread = nullptr;
    audioBufferData = nullptr;
    currentFrame = 0;
    displayedFrame = 0;
    videoTimer = 0;
    needsGPUTransfer = false;
    hasAudio = false;
    failedToLoad = false;
    audioBufferData = nullptr;
    fillIdx = 0;
    audioSamplesPlayed = 0;
    tex[0].data = nullptr;
    tex[1].data = nullptr;
    width = 320; height = 240; fps = 24;
    totalFrames = 0; audioRate = 32000;
    channels = 1; audioFormat = 0;
    videoEnded = false;
    transitioning = false;
    isDebug = debugUI;
    isPaused = false;
    threadRunning = false;
    skipProgress = 0.0f;
    ringAlpha = 0.0f;
    
    for (int i = 0; i < RING_SIZE; i++) {
        decodeBuf[i] = nullptr;
    }
}

VideoState::~VideoState() {
    if (failedToLoad) {
        if (file) fclose(file);
        return;
    }

    threadRunning = false;
    LightEvent_Signal(&eventDecodeRequest);
    if (decodeThread) {
        threadJoin(decodeThread, U64_MAX);
        threadFree(decodeThread);
    }

    if (file) fclose(file);
    for (int i = 0; i < RING_SIZE; i++) {
        if (decodeBuf[i]) linearFree(decodeBuf[i]);
    }
    if (linearBuffer[0]) linearFree(linearBuffer[0]);
    if (linearBuffer[1]) linearFree(linearBuffer[1]);
    if (tex[0].data) C3D_TexDelete(&tex[0]);
    if (tex[1].data) C3D_TexDelete(&tex[1]);
    if (hasAudio && audioBufferData) {
        ndspChnWaveBufClear(5);
        linearFree(audioBufferData);
    }
    if (isDebug && textBuf) {
        C2D_TextBufDelete(textBuf);
    }
}

void VideoState::init() {
    file = fopen(path.c_str(), "rb");
    if (!file) {
        failedToLoad = true;
        return;
    }
    
    setvbuf(file, nullptr, _IOFBF, 512 * 1024);
    
    compressedBuf.resize(2 * 1024 * 1024);
    uncompressedBuf.resize(2 * 1024 * 1024);

    MusicPlayer::stop();

    readHeader();

    if (isDebug) {
        textBuf = C2D_TextBufNew(4096);
    }

    for (int i = 0; i < RING_SIZE; i++) {
        decodeBuf[i] = (uint16_t*)linearAlloc(width * height * 2);
        memset(decodeBuf[i], 0, width * height * 2);
    }

    linearBuffer[0] = (uint16_t*)linearAlloc(512 * 256 * 2);
    memset(linearBuffer[0], 0, 512 * 256 * 2);
    linearBuffer[1] = (uint16_t*)linearAlloc(512 * 256 * 2);
    memset(linearBuffer[1], 0, 512 * 256 * 2);
    currentLinearBuffer = 0;
 
    C3D_TexInit(&tex[0], 512, 256, GPU_RGB565);
    C3D_TexSetFilter(&tex[0], GPU_LINEAR, GPU_NEAREST);
    C3D_TexInit(&tex[1], 512, 256, GPU_RGB565);
    C3D_TexSetFilter(&tex[1], GPU_LINEAR, GPU_NEAREST);

    subtex.width  = width;
    subtex.height = height;
    subtex.left   = 0.0f;
    subtex.top    = 1.0f;
    subtex.right  = (float)width  / 512.0f;
    subtex.bottom = 1.0f - (float)height / 256.0f;

    img.tex    = &tex[0];
    img.subtex = &subtex;
    GSPGPU_FlushDataCache(linearBuffer[0], 512 * 256 * 2);
    for (int i = 0; i < 2; i++) {
        C3D_SyncDisplayTransfer(
            (u32*)linearBuffer[0], GX_BUFFER_DIM(512, 256),
            (u32*)tex[i].data,     GX_BUFFER_DIM(512, 256),
            GX_TRANSFER_FLIP_VERT(0)  | GX_TRANSFER_OUT_TILED(1) |
            GX_TRANSFER_RAW_COPY(0)   |
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
            GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    }

    if (hasAudio) {
        ndspChnReset(5);
        ndspChnSetInterp(5, NDSP_INTERP_LINEAR);
        ndspChnSetRate(5, audioRate);
        ndspChnSetFormat(5, channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
        
        int bufferSamples = audioRate / fps;
        int bufferBytes = bufferSamples * channels * 2;
        audioBufferData = (int16_t*)linearAlloc(bufferBytes * BUF_COUNT);
        
        float mix[12];
        memset(mix, 0, sizeof(mix));
        mix[0] = 1.0f;
        mix[1] = 1.0f;
        ndspChnSetMix(5, mix);
        ndspChnSetPaused(5, false);
        
        memset(waveBuf, 0, sizeof(waveBuf));
        for (int i = 0; i < BUF_COUNT; i++) {
            waveBuf[i].data_vaddr = &audioBufferData[i * bufferSamples * channels];
            waveBuf[i].status = NDSP_WBUF_FREE;
        }
    }

    threadRunning = true;
    writeIdx = 0;
    readIdx = 0;
    ringCount = 0;
    lockedReadIdx = -1;
    currentTex = 0;
    audioSamplesPlayed = 0;
    videoTimer = 0.0f;
    LightEvent_Init(&eventDecodeRequest, RESET_STICKY);
    LightEvent_Init(&eventDecodeDone, RESET_STICKY);
    LightLock_Init(&decodeLock);
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    decodeThread = threadCreate(threadMain, this, 32768, priority - 1, -2, false);
    
    LightEvent_Signal(&eventDecodeRequest);
}

void VideoState::readHeader() {
    char magic[4];
    fread(magic, 1, 4, file);
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

    hasAudio = (audioFormat == 1);
}

float VideoState::getAudioMasterTime() {
    if (!hasAudio) return videoTimer;
    updateAudioTracking();
    LightLock_Lock(&decodeLock);
    uint64_t samples = audioSamplesPlayed;
    LightLock_Unlock(&decodeLock);
    uint32_t pos = ndspChnGetSamplePos(5);
    return (float)(samples + pos) / (float)audioRate;
}

void VideoState::updateAudioTracking() {
    if (!hasAudio) return;
    LightLock_Lock(&decodeLock);
    for (int i = 0; i < BUF_COUNT; i++) {
        if (waveBuf[i].status == NDSP_WBUF_DONE) {
            audioSamplesPlayed += waveBuf[i].nsamples;
            waveBuf[i].status = NDSP_WBUF_FREE;
        }
    }
    LightLock_Unlock(&decodeLock);
}

void VideoState::decodeChunk() {
    if (!file) return;

    uint8_t type;
    if (fread(&type, 1, 1, file) != 1) {
        videoEnded = true; // EOF
        return;
    }

    uint8_t sizeBytes[3];
    fread(sizeBytes, 1, 3, file);
    uint32_t size = sizeBytes[0] | (sizeBytes[1] << 8) | (sizeBytes[2] << 16);

    if (type == 0x03) { // AUDIO
        if (hasAudio) {
            int idx = -1;
            while (threadRunning) {
                updateAudioTracking();
                for (int i = 0; i < BUF_COUNT; i++) {
                    if (waveBuf[i].status == NDSP_WBUF_FREE) {
                        idx = i;
                        break;
                    }
                }
                if (idx != -1) break;
                svcSleepThread(1000000);
            }
            
            if (idx != -1) {
                size_t readBytes = fread(waveBuf[idx].data_pcm16, 1, size, file);
                waveBuf[idx].nsamples = readBytes / (channels * 2);
                DSP_FlushDataCache(waveBuf[idx].data_pcm16, readBytes);
                ndspChnWaveBufAdd(5, &waveBuf[idx]);
            } else {
                fseek(file, size, SEEK_CUR);
            }
        } else {
            fseek(file, size, SEEK_CUR);
        }
    }
    else if (type == 0x01 || type == 0x02) { // VIDEO KEY/DELTA
        if (isZlib) {
            uint8_t uncompBytes[3];
            fread(uncompBytes, 1, 3, file);
            uint32_t uncompSize = uncompBytes[0] | (uncompBytes[1] << 8) | (uncompBytes[2] << 16);

            uLongf destLen = uncompSize;
            fread(compressedBuf.data(), 1, size, file);
            uncompress((Bytef*)uncompressedBuf.data(), &destLen, (const Bytef*)compressedBuf.data(), size);
            
            uint8_t*  data = uncompressedBuf.data();
            uint8_t*  end  = data + uncompSize;
            uint16_t* ptr  = decodeBuf[writeIdx];
            
            if (type == 0x02) { // DELTA
                int prevIdx = (writeIdx + RING_SIZE - 1) % RING_SIZE;
                memcpy(ptr, decodeBuf[prevIdx], width * height * 2);
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
        } else {
            std::vector<uint8_t> buffer(size);
            fread(buffer.data(), 1, size, file);
            
            uint8_t*  data = buffer.data();
            uint8_t*  end  = data + size;
            uint16_t* ptr  = decodeBuf[writeIdx];
            
            if (type == 0x02) { // DELTA
                int prevIdx = (writeIdx + RING_SIZE - 1) % RING_SIZE;
                memcpy(ptr, decodeBuf[prevIdx], width * height * 2);
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
        
        LightLock_Lock(&decodeLock);
        writeIdx = (writeIdx + 1) % RING_SIZE;
        ringCount++;
        LightLock_Unlock(&decodeLock);
        
        currentFrame++;
    }
}

void VideoState::threadMain(void* arg) {
    VideoState* state = (VideoState*)arg;
    while (state->threadRunning) {
        if (state->isPaused) {
            svcSleepThread(1000000);
            continue;
        }

        if (state->videoEnded) {
            // Process remaining audio buffers to track samples
            bool audioPlaying = false;
            if (state->hasAudio) {
                state->updateAudioTracking();
                for (int i = 0; i < BUF_COUNT; i++) {
                    if (state->waveBuf[i].status != NDSP_WBUF_FREE) {
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
        LightLock_Lock(&state->decodeLock);
        canWrite = (state->ringCount < RING_SIZE) && (state->writeIdx != state->lockedReadIdx);
        LightLock_Unlock(&state->decodeLock);

        if (canWrite) {
            state->decodeChunk();
        } else {
            LightEvent_Wait(&state->eventDecodeRequest);
            LightEvent_Clear(&state->eventDecodeRequest);
        }
    }
}

void VideoState::update(float dt) {
    if (transitioning) return;

    if (failedToLoad) {
        transitioning = true;
        switchState(m_nextState);
        return;
    }

    if (isDebug) {
        if (hidKeysDown() & KEY_A) {
            transitioning = true;
            switchState(m_nextState);
            return;
        }
        if (hidKeysDown() & KEY_START) {
            isPaused = !isPaused;
            if (isPaused) {
                AudioEngine::pause();
                if (hasAudio) ndspChnSetPaused(5, true);
            } else {
                AudioEngine::resume();
                if (hasAudio) ndspChnSetPaused(5, false);
            }
        }
    } else {
        u32 kHeld = hidKeysHeld();
        bool isHoldingSkip = (kHeld & (KEY_START | KEY_A));

        if (isHoldingSkip) {
            ringAlpha += dt * 4.0f;
            if (ringAlpha > 1.0f) ringAlpha = 1.0f;
 
            skipProgress += dt / 1.5f;
            if (skipProgress >= 1.0f) {
                skipProgress = 1.0f;
                transitioning = true;
                switchState(m_nextState);
                return;
            }
        } else {
            ringAlpha -= dt * 4.0f;
            if (ringAlpha < 0.0f) ringAlpha = 0.0f;
 
            skipProgress -= dt * 2.5f;
            if (skipProgress < 0.0f) skipProgress = 0.0f;
        }
    }

    if (isPaused) return;

    float masterTime = 0.0f;
    if (hasAudio) {
        masterTime = getAudioMasterTime();
        if (masterTime < videoTimer) {
            masterTime = videoTimer;
        } else {
            videoTimer = masterTime;
        }
    } else {
        videoTimer += dt;
        masterTime = videoTimer;
    }
    
    float frameTime = 1.0f / (float)(fps > 0 ? fps : 24);
    int targetFrame = (int)(masterTime / frameTime);
    
    needsGPUTransfer = false;
    int lastReadIdx = -1;

    while (displayedFrame < targetFrame) {
        bool ready = false;
        LightLock_Lock(&decodeLock);
        ready = (ringCount > 0);
        LightLock_Unlock(&decodeLock);

        if (ready) {
            needsGPUTransfer = true;
            
            LightLock_Lock(&decodeLock);
            lastReadIdx = readIdx;
            readIdx = (readIdx + 1) % RING_SIZE;
            ringCount--;
            lockedReadIdx = lastReadIdx;
            LightLock_Unlock(&decodeLock);
            
            LightEvent_Signal(&eventDecodeRequest);
            displayedFrame++;
        } else {
            break;
        }
    }

    if (videoEnded && ringCount == 0) {
        transitioning = true;
        switchState(m_nextState);
        return;
    }

    if (needsGPUTransfer && lastReadIdx != -1) {
        currentLinearBuffer = (currentLinearBuffer + 1) % 2;
        uint16_t* curBuf = linearBuffer[currentLinearBuffer];

        for (int y = 0; y < height; y++) {
            memcpy(&curBuf[y * 512], &decodeBuf[lastReadIdx][y * width], width * 2);
        }
        GSPGPU_FlushDataCache(curBuf, 512 * 256 * 2);
        
        currentTex = (currentTex + 1) % 2;
        
        C3D_SyncDisplayTransfer(
            (u32*)curBuf,  GX_BUFFER_DIM(512, 256),
            (u32*)tex[currentTex].data,      GX_BUFFER_DIM(512, 256),
            GX_TRANSFER_FLIP_VERT(0)   |
            GX_TRANSFER_OUT_TILED(1)   |
            GX_TRANSFER_RAW_COPY(0)    |
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565)  |
            GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

        LightLock_Lock(&decodeLock);
        lockedReadIdx = -1;
        LightLock_Unlock(&decodeLock);
        LightEvent_Signal(&eventDecodeRequest);
    }
}

void VideoState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    if (failedToLoad) return;

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));

    if (!isDebug && ringAlpha > 0.0f) {
        float cx = 285.0f;
        float cy = 205.0f;
        float r_in = 12.0f;
        float r_out = 16.0f;
 
        u8 alphaBack = (u8)(ringAlpha * 100.0f);
        u8 alphaFront = (u8)(ringAlpha * 255.0f);
 
        u32 colorBack = C2D_Color32(0, 0, 0, alphaBack);
        drawProgressRing(cx, cy, r_in, r_out, 1.0f, colorBack, 0.9f);
 
        u32 colorFront = C2D_Color32(255, 255, 255, alphaFront);
        drawProgressRing(cx, cy, r_in, r_out, skipProgress, colorFront, 0.91f);
    }

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    
    if (tex[currentTex].data) {
        img.tex = &tex[currentTex];
        float scaleX = 400.0f / width;
        float h = height * scaleX;
        float y = (240.0f - h) / 2.0f;
        
        C2D_DrawImageAt(img, 0, y, 0.5f, nullptr, scaleX, scaleX);
    }

    if (isDebug) {
        C2D_SceneBegin(bottom);
        C2D_TextBufClear(textBuf);
        
        float progress = 0.0f;
        if (totalFrames > 0) progress = (float)currentFrame / (float)totalFrames;
        
        C2D_DrawRectSolid(20, 100, 0, 280, 20, C2D_Color32(50, 50, 50, 255));
        C2D_DrawRectSolid(20, 100, 0, 280 * progress, 20, C2D_Color32(0, 255, 0, 255));
        
        char timeText[64];
        float totalSecs = (float)totalFrames / (float)fps;
        float curSecs = (float)currentFrame / (float)fps;
        sprintf(timeText, "Time: %02d:%02d / %02d:%02d",
            (int)curSecs / 60, (int)curSecs % 60,
            (int)totalSecs / 60, (int)totalSecs % 60);
        
        C2D_Text t;
        C2D_TextParse(&t, textBuf, timeText);
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, 20, 70, 0, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
        
        C2D_TextParse(&t, textBuf, "START: Pause/Resume\nA: Exit to Debug Menu");
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, 20, 140, 0, 0.5f, 0.5f, C2D_Color32(200, 200, 200, 255));
        
        if (isPaused) {
            C2D_TextParse(&t, textBuf, "PAUSED");
            C2D_TextOptimize(&t);
            C2D_DrawText(&t, C2D_WithColor, 130, 40, 0, 0.8f, 0.8f, C2D_Color32(255, 255, 0, 255));
        }
    }
}
