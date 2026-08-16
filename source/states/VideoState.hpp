#pragma once

#include "MusicBeatState.hpp"
#include <citro2d.h>
#include <citro3d.h>
#include <3ds.h>
#include <string>
#include <vector>
#include <stdio.h>

class VideoState : public MusicBeatState {
public:
    VideoState(const std::string& videoPath, MusicBeatState* nextState, bool debugUI = false);
    ~VideoState() override;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;

private:
    std::string path;
    MusicBeatState* m_nextState;
    FILE* file;

    // Header info
    uint16_t width, height;
    uint8_t fps, format;
    uint32_t totalFrames;
    uint16_t audioRate;
    uint8_t channels, audioFormat;
    bool isZlib;

    // Graphics
    C3D_Tex tex[2];
    int currentTex = 0;
    Tex3DS_SubTexture subtex;
    C2D_Image img;
    
    // Ring Buffer
    static constexpr int RING_SIZE = 15;
    uint16_t* decodeBuf[RING_SIZE];
    volatile int writeIdx = 0;
    volatile int readIdx = 0;
    volatile int ringCount = 0;
    volatile int lockedReadIdx = -1;
    
    uint16_t* linearBuffer[2]; // padded GPU upload buffer (512 x 256)
    int currentLinearBuffer = 0;

    int currentFrame;
    int displayedFrame;
    float videoTimer;
    
    std::vector<uint8_t> compressedBuf;
    std::vector<uint8_t> uncompressedBuf;
    bool needsGPUTransfer;
    
    // Debug UI
    bool isDebug;
    bool isPaused;
    C2D_TextBuf textBuf;
    
    // Threading
    Thread decodeThread;
    LightEvent eventDecodeRequest;
    LightEvent eventDecodeDone;
    LightLock decodeLock;
    volatile bool threadRunning;
    volatile bool frameReady;
    
    // Audio
    static constexpr int BUF_COUNT = 15;
    ndspWaveBuf waveBuf[BUF_COUNT];
    int16_t* audioBufferData;
    int fillIdx;
    bool hasAudio;
    bool failedToLoad;
    bool videoEnded;
    volatile bool transitioning;
    uint64_t audioSamplesPlayed;
    
    // Hold to skip progress ring
    float skipProgress = 0.0f;
    float ringAlpha = 0.0f;

    void readHeader();
    void decodeChunk();
    float getAudioMasterTime();
    void updateAudioTracking();
    
    static void threadMain(void* arg);
};
