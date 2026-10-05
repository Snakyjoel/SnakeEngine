#pragma once

#include "../backend/codecs/SnakyDecoder.hpp"
#include <citro2d.h>
#include <citro3d.h>
#include <3ds.h>
#include <string>

class InGameVideoPlayer {
public:
    InGameVideoPlayer(const std::string& videoPath, bool inFrontOfHUD = true, bool loop = false);
    ~InGameVideoPlayer();

    void update(float dt);
    void draw(float stageAlpha = 1.0f);
    void restartVideo();

    bool inFrontOfHUD = true;
    bool loop = false;
    bool isPaused = false;
    bool finished = false;
    float alpha = 1.0f;

private:
    std::string path;
    SnakyDecoder decoder;

    // Graphics
    C3D_Tex tex[2];
    int currentTex = 0;
    Tex3DS_SubTexture subtex;
    C2D_Image img;
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    int displayedFrame = -1;
    double songStartPosition = -1.0;
    bool firstFrameReady = false;

    void processGPUTransfer(uint16_t* frameBuf, int ringSlot);
};
