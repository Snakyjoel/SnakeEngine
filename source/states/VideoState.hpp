#pragma once

#include "MusicBeatState.hpp"
#include "../backend/codecs/SnakyDecoder.hpp"
#include <citro2d.h>
#include <citro3d.h>
#include <3ds.h>
#include <string>

class VideoState : public MusicBeatState {
public:
    VideoState(const std::string& videoPath, MusicBeatState* nextState, bool instantSkip = false);
    VideoState(const std::string& videoPath, MusicBeatState* nextState, bool unusedDebug, bool instantSkip);
    ~VideoState() override;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;

private:
    std::string path;
    MusicBeatState* m_nextState;
    SnakyDecoder decoder;

    // Graphics
    C3D_Tex tex[2];
    int currentTex = 0;
    Tex3DS_SubTexture subtex;
    C2D_Image img;

    int displayedFrame;
    float videoTimer;
    bool failedToLoad;
    volatile bool transitioning;

    bool m_instantSkip = false;

    // Hold to skip progress ring
    float skipProgress = 0.0f;
    float ringAlpha = 0.0f;

    void processGPUTransfer(uint16_t* frameBuf, int ringSlot);
};
