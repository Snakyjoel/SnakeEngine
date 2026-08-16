#pragma once

#include <citro2d.h>
#include <citro3d.h>
#include <3ds.h>
#include <string>
#include <vector>
#include <stdio.h>

class InGameVideoPlayer {
public:
    InGameVideoPlayer(const std::string& videoPath, bool inFrontOfHUD = true, bool loop = false);
    ~InGameVideoPlayer();

    void update(float dt);
    void draw(float stageAlpha = 1.0f);

    bool inFrontOfHUD;
    bool loop;
    bool isPaused;
    bool finished;
    float alpha;

private:
    std::string path;
    FILE* file;

    // Header info
    uint16_t width, height;
    uint8_t fps, format;
    uint32_t totalFrames;
    bool isZlib;

    // Graphics
    C3D_Tex tex[2];
    int currentTex = 0;
    Tex3DS_SubTexture subtex;
    C2D_Image img;

    // Lightweight Ring Buffer (6 frames)
    static constexpr int RING_SIZE = 6;
    uint16_t* decodeBuf[RING_SIZE];
    int ringFrameIds[RING_SIZE]; // actual video frame number stored in each ring slot
    volatile int writeIdx = 0;
    volatile int readIdx = 0;
    volatile int ringCount = 0;
    volatile int lockedReadIdx = -1;

    uint16_t* linearBuffer[2];
    int currentLinearBuffer = 0;

    int currentFrame;
    int displayedFrame;
    float videoTimer;
    double songStartPosition; // songPosition when video started, for sync
    int lastTransferReadIdx;  // index being transferred to GPU
    bool gpuTransferPending;  // async transfer in flight
    volatile int targetFrameCached;
    volatile bool skipToNextKey; // true = decoder should skip delta frames to catch up

    std::vector<uint8_t> compressedBuf;
    std::vector<uint8_t> uncompressedBuf;
    bool firstFrameReady; // Guard against drawing uninitialized VRAM

    // Threading
    Thread decodeThread;
    LightEvent eventDecodeRequest;
    LightLock decodeLock;
    volatile bool threadRunning;
    volatile bool videoEnded;

    void readHeader();
    void decodeChunk();
    void restartVideo();

    static void threadMain(void* arg);
};
