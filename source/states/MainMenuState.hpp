#pragma once
#include <string>
#include <vector>
#include "../backend/MusicBeatState.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../objects/CppAnimate.hpp"

class FreeplayState;

struct MenuItem {
    std::string name;
    CppAnimate animate;
    float x, y;
    bool isDarkened = false;
};

class MainMenuState : public MusicBeatState {
public:
    static std::string version;

    MainMenuState(bool startInFreeplay = false);
    ~MainMenuState() override;

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

    // Precomputed background layout
    float topBgScale = 0.95f;
    float topBgW = 0.0f;
    float topBgH = 0.0f;
    float topBgDrawX = 0.0f;
    float topBgMinY = 0.0f;

    float botBgScale = 1.10f;
    float botBgW = 0.0f;
    float botBgH = 0.0f;
    float botBgBaseX = 0.0f;
    float botBgBaseY = 0.0f;

    // Precomputed button prompt dimensions
    float cachedYBtnW = 0.0f;
    float cachedXBtnW = 0.0f;
    float cachedOptTextW = 0.0f;

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
    std::string versionDisplayStr;

    float introTimer = 0.0f;

    // Confirm animation
    bool selectedSomething = false;
    float confirmTimer = 0.0f;
    float flickerTimer = 0.0f;
    bool flickerVisible = true;
    float itemFadeTimers[6];
    float itemAlphas[6];

    // BG magenta
    bool bgMagentaActive = false;
    float bgMagentaTimer = 0.0f;
    float bgFlickerTimer = 0.0f;
    bool bgFlickerVisible = false;

    // Freeplay substate overlay
    bool startInFreeplay = false;
    bool isFreeplayActive = false;
    FreeplayState* freeplaySubState = nullptr;

public:
    static bool comingFromFreeplay;
};
