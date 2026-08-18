#pragma once
#include "../backend/MusicBeatState.hpp"
#include <citro2d.h>
#include <map>
#include <string>
#include <vector>

class CustomizeComboState : public MusicBeatState {
public:
    CustomizeComboState();
    ~CustomizeComboState();

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;

private:
    C2D_SpriteSheet bgSheet = nullptr;
    C2D_Image topBG;
    C2D_Image bottomBG;

    C2D_SpriteSheet ratingSheet = nullptr;
    C2D_Image ratingBaseImage;
    struct RatingInfo {
        Tex3DS_SubTexture sub;
        bool rotated = false;
        float frameWidth = 0.0f;
        float frameHeight = 0.0f;
        float autoScale = 1.0f;
    };
    std::map<std::string, RatingInfo> ratingSubtexs;

    C2D_SpriteSheet noteSheet = nullptr;
    C2D_Image noteBaseImage;
    std::vector<NoteSprite> noteSubtexs;

    bool dragging = false;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;

    float currentScale;
    float currentAlpha;
    float currentX;
    float currentY;

    float blinkTimer = 0.0f;
    float dpadTimer = 0.0f;
    bool isOutOfBounds = false;

    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
};
