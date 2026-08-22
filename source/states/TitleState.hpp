#pragma once
#include <string>
#include <vector>
#include "../backend/MusicBeatState.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../objects/CppAnimate.hpp"

class TitleState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;
    void beatHit(int beat) override;

private:
    float timer = 0.0f;
    int curBeat = -1;
    float exitProgress = 0.0f;
    float ringAlpha = 0.0f;

    // Intro text sequence
    bool skippedIntro = false;
    bool showNewgrounds = false;
    std::vector<std::string> introLines;
    std::string wackyText1;
    std::string wackyText2;

    // CppAnimate Sprites
    CppAnimate gf;
    CppAnimate logo;
    CppAnimate ng;
    CppAnimate titleEnter;
    CppAnimate titleEnter2;

    bool gfDanceLeftActive = true;

    // Logo beat zoom
    float logoScale = 1.0f;

    // Transitions and Flash
    bool transitioning = false;
    float switchTimer = 0.0f;
    float updateWaitTimer = 0.0f;

    //Timers for videos
    float promoTimer = 0.0f;
    bool promoPending = false;
    bool promoFadingOut = false;
    float promoFadeTime = 0.0f;
    std::string promoChosenVideo = "";

    // Helper functions
    void createCoolText(const std::vector<std::string>& textArray);
    void addMoreText(const std::string& text);
    void deleteCoolText();
    void skipIntro();
    
    std::vector<u32> konamiInput;
    const std::vector<u32> konamiTarget = {
        KEY_DUP, KEY_DUP, KEY_DDOWN, KEY_DDOWN,
        KEY_DLEFT, KEY_DRIGHT, KEY_DLEFT, KEY_DRIGHT,
        KEY_B, KEY_A
    };
};
