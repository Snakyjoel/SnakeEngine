#include "VideoState.hpp"
#include <string.h>
#include <malloc.h>
#include "../backend/AudioEngine.hpp"
#include "backend/Conductor.hpp"
#include "backend/Paths.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static void drawProgressRing(float cx, float cy, float r_in, float r_out, float progress, u32 color, float depth = 0.9f) {
    if (progress <= 0.0f) return;
    if (progress > 1.0f) progress = 1.0f;

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
    tex[0].data = nullptr;
    tex[1].data = nullptr;
    textBuf = nullptr;
    displayedFrame = -1;
    videoTimer = 0.0f;
    failedToLoad = false;
    transitioning = false;
    isDebug = debugUI;
    isPaused = false;
    skipProgress = 0.0f;
    ringAlpha = 0.0f;
}

VideoState::~VideoState() {
    decoder.close();
    if (tex[0].data) C3D_TexDelete(&tex[0]);
    if (tex[1].data) C3D_TexDelete(&tex[1]);
    if (isDebug && textBuf) {
        C2D_TextBufDelete(textBuf);
    }
}

void VideoState::init() {
    MusicPlayer::stop();

    if (!decoder.open(path, true)) {
        failedToLoad = true;
        return;
    }

    if (isDebug) {
        textBuf = C2D_TextBufNew(4096);
    }

    GPU_TEXCOLOR texFmt = (decoder.getFormat() == 1) ? GPU_RGBA4 : GPU_RGB565;
    for (int i = 0; i < 2; i++) {
        C3D_TexInit(&tex[i], 512, 256, texFmt);
        C3D_TexSetFilter(&tex[i], GPU_LINEAR, GPU_NEAREST);
    }

    uint16_t w = decoder.getWidth();
    uint16_t h = decoder.getHeight();

    subtex.width  = w;
    subtex.height = h;
    subtex.left   = 0.0f;
    subtex.top    = 1.0f;
    subtex.right  = (float)w / 512.0f;
    subtex.bottom = 1.0f - (float)h / 256.0f;

    img.tex    = &tex[0];
    img.subtex = &subtex;

    decoder.start();
    currentTex = 0;
    videoTimer = 0.0f;
    displayedFrame = -1;
}

void VideoState::processGPUTransfer(uint16_t* frameBuf, int ringSlot) {
    if (!frameBuf) return;

    currentTex = (currentTex + 1) % 2;

    GX_TRANSFER_FORMAT transferFmt = (decoder.getFormat() == 1) ? GX_TRANSFER_FMT_RGBA4 : GX_TRANSFER_FMT_RGB565;

    // Direct GPU DMA transfer from 512-pitch decoded buffer
    GSPGPU_FlushDataCache(frameBuf, 512 * 256 * 2);
    C3D_SyncDisplayTransfer(
        (u32*)frameBuf,             GX_BUFFER_DIM(512, 256),
        (u32*)tex[currentTex].data, GX_BUFFER_DIM(512, 256),
        GX_TRANSFER_FLIP_VERT(0)   |
        GX_TRANSFER_OUT_TILED(1)   |
        GX_TRANSFER_RAW_COPY(0)    |
        GX_TRANSFER_IN_FORMAT(transferFmt)  |
        GX_TRANSFER_OUT_FORMAT(transferFmt) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
    );

    decoder.releaseFrame(ringSlot);
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
            } else {
                AudioEngine::resume();
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

    if (dt > 0.0001f) {
        currentFPS = currentFPS * 0.9f + (1.0f / dt) * 0.1f;
    }

    if (isPaused) return;

    float masterTime = 0.0f;
    if (decoder.hasAudioTrack()) {
        masterTime = decoder.getAudioMasterTime(videoTimer);
        if (masterTime < videoTimer) {
            masterTime = videoTimer;
        } else {
            videoTimer = masterTime;
        }
    } else {
        videoTimer += dt;
        masterTime = videoTimer;
    }

    float frameTime = 1.0f / (float)decoder.getFps();
    int targetFrame = (int)(masterTime / frameTime);
    decoder.setTargetFrame(targetFrame);

    uint16_t* latestFrameBuf = nullptr;
    int latestSlot = -1;

    while (displayedFrame < targetFrame) {
        uint16_t* buf = nullptr;
        int frameId = -1;
        int slot = -1;
        if (decoder.getNextFrame(&buf, frameId, slot)) {
            if (latestSlot != -1) {
                decoder.releaseFrame(latestSlot);
            }
            latestFrameBuf = buf;
            latestSlot = slot;
            displayedFrame = frameId;
        } else {
            break;
        }
    }

    if (latestFrameBuf && latestSlot != -1) {
        processGPUTransfer(latestFrameBuf, latestSlot);
    }

    if (decoder.isEnded()) {
        transitioning = true;
        switchState(m_nextState);
        return;
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
        float w = (float)decoder.getWidth();
        float h_raw = (float)decoder.getHeight();
        float scaleX = 400.0f / (w > 0 ? w : 400.0f);
        float h = h_raw * scaleX;
        float y = (240.0f - h) / 2.0f;

        C2D_DrawImageAt(img, 0, y, 0.5f, nullptr, scaleX, scaleX);
    }

    if (isDebug) {
        C2D_SceneBegin(bottom);
        C2D_TextBufClear(textBuf);

        uint32_t totalFrames = decoder.getTotalFrames();
        float progress = 0.0f;
        if (totalFrames > 0) progress = (float)displayedFrame / (float)totalFrames;

        C2D_DrawRectSolid(20, 100, 0, 280, 20, C2D_Color32(50, 50, 50, 255));
        C2D_DrawRectSolid(20, 100, 0, 280 * progress, 20, C2D_Color32(0, 255, 0, 255));

        char timeText[128];
        float fpsVal = (float)decoder.getFps();
        float totalSecs = (float)totalFrames / fpsVal;
        float curSecs = (float)(displayedFrame > 0 ? displayedFrame : 0) / fpsVal;
        sprintf(timeText, "Time: %02d:%02d / %02d:%02d\nFPS: %.1f (Video: %d)",
            (int)curSecs / 60, (int)curSecs % 60,
            (int)totalSecs / 60, (int)totalSecs % 60,
            currentFPS, (int)fpsVal);

        C2D_Text t;
        C2D_TextParse(&t, textBuf, timeText);
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, 20, 50, 0, 0.55f, 0.55f, C2D_Color32(255, 255, 255, 255));

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
