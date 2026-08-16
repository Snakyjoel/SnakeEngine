#pragma once
#include <string>
#include <vector>
#include "../backend/MusicBeatState.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../objects/CppAnimate.hpp"

struct MenuItem {
    std::string name;
    CppAnimate animate;
    float x, y;
    bool isDarkened = false;
};

class MainMenuState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    int curSelected = 0;
    std::vector<MenuItem> menuItems;

    C2D_SpriteSheet bgSheet = nullptr;
    C2D_SpriteSheet bottomBGSheet = nullptr;
    C2D_Image topBG;
    C2D_Image bottomBG;
    float lerpSelected = 0.0f;  // for top parallax effect

    // Bottom screen touch parallax
    float botParallaxX = 0.0f;
    float botParallaxY = 0.0f;
    bool botTouchActive = false;
    float botTouchStartX = 0.0f;
    float botTouchStartY = 0.0f;
    float botParallaxTargetX = 0.0f;
    float botParallaxTargetY = 0.0f;
    
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;

    bool isTransitioningToFreeplay = false;
    float transitionTimer = 0.0f;
    float introTimer = 0.0f;

public:
    static bool comingFromFreeplay;
};
