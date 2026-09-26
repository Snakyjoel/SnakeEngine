#include "InGameVideoPlayer.hpp"
#include "../backend/Conductor.hpp"
#include "../backend/Paths.hpp"
#include <cstring>

InGameVideoPlayer::InGameVideoPlayer(const std::string& videoPath, bool inFrontOfHUD, bool loop)
    : inFrontOfHUD(inFrontOfHUD), loop(loop), isPaused(false), finished(false), alpha(1.0f),
      path(videoPath), displayedFrame(-1), songStartPosition(-1.0), firstFrameReady(false)
{
    tex[0].data = nullptr;
    tex[1].data = nullptr;

    std::string resolvedPath = videoPath;
    if (resolvedPath.find('/') == std::string::npos && resolvedPath.find('\\') == std::string::npos) {
        std::string videoName = resolvedPath;
        if (videoName.length() < 6 || videoName.compare(videoName.length() - 6, 6, ".snaky") != 0) {
            videoName += ".snaky";
        }
        resolvedPath = Paths::getPath(videoName, "videos");
    }
    path = resolvedPath;

    // InGameVideoPlayer passes includeAudio = false so audio isn't duplicated over song music
    if (!decoder.open(path, false)) {
        finished = true;
        return;
    }

    uint16_t w = decoder.getWidth();
    uint16_t h = decoder.getHeight();
    if (w == 0 || h == 0) {
        decoder.close();
        finished = true;
        return;
    }

    GPU_TEXCOLOR texFmt = (decoder.getFormat() == 1) ? GPU_RGBA4 : GPU_RGB565;
    for (int i = 0; i < 2; i++) {
        C3D_TexInit(&tex[i], 512, 256, texFmt);
        C3D_TexSetFilter(&tex[i], GPU_LINEAR, GPU_LINEAR);
    }

    subtex.width  = w;
    subtex.height = h;
    subtex.left   = 0.0f;
    subtex.right  = (float)w / 512.0f;
    subtex.top    = 1.0f;
    subtex.bottom = 1.0f - (float)h / 256.0f;

    scaleX = 400.0f / (float)w;
    scaleY = 240.0f / (float)h;

    decoder.start();
}

InGameVideoPlayer::~InGameVideoPlayer() {
    decoder.close();
    for (int i = 0; i < 2; i++) {
        if (tex[i].data) {
            C3D_TexDelete(&tex[i]);
            tex[i].data = nullptr;
        }
    }
}

void InGameVideoPlayer::restartVideo() {
    decoder.close();
    displayedFrame = -1;
    songStartPosition = -1.0;
    firstFrameReady = false;
    finished = false;

    if (decoder.open(path, false)) {
        decoder.start();
    } else {
        finished = true;
    }
}

void InGameVideoPlayer::processGPUTransfer(uint16_t* frameBuf, int ringSlot) {
    if (!frameBuf) return;

    currentTex ^= 1;

    GX_TRANSFER_FORMAT transferFmt = (decoder.getFormat() == 1) ? GX_TRANSFER_FMT_RGBA4 : GX_TRANSFER_FMT_RGB565;

    // Direct GPU DMA transfer from 512-pitch decoded buffer — Zero CPU row-by-row memcpy!
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
    firstFrameReady = true;
}

void InGameVideoPlayer::update(float dt) {
    if (finished || isPaused) return;

    if (songStartPosition < 0.0) {
        songStartPosition = Conductor::songPosition;
    }

    double elapsed = (Conductor::songPosition - songStartPosition) / 1000.0;
    if (elapsed < 0.0) elapsed = 0.0;
    
    int fps = decoder.getFps();
    int targetFrame = (fps > 0) ? (int)(elapsed * (double)fps) : 0;
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
        if (loop) {
            restartVideo();
        } else {
            finished = true;
        }
    }
}

void InGameVideoPlayer::draw(float stageAlpha) {
    if (finished || !firstFrameReady || !tex[currentTex].data) return;

    float drawAlpha = alpha * stageAlpha;
    if (drawAlpha <= 0.0f) return;

    img.tex = &tex[currentTex];
    img.subtex = &subtex;

    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, drawAlpha);

    C2D_DrawImageAt(img, 0.0f, 0.0f, 0.5f, &tint, scaleX, scaleY);
}
