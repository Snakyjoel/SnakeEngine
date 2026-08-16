#pragma once
#include "../backend/MusicBeatState.hpp"
#include "Achievements.hpp"
#include "SparrowParser.hpp"
#include "../objects/Alphabet.hpp"
#include "../objects/CppAnimate.hpp"
#include <citro2d.h>
#include <vector>

class AchievementsMenuState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    void changeSelection(int change = 0);
    std::string wrapText(const std::string& text, float scale, float maxWidth);

    int curSelected = 0;
    float lerpSelected = 0.0f;
    
    std::vector<AchievementInfo> options;
    std::vector<int> achievementIndex;

    C2D_SpriteSheet bgSheet = nullptr;
    C2D_SpriteSheet bottomBGSheet = nullptr;
    C2D_Image topBG;
    C2D_Image bottomBG;

    CppAnimate iconAnimate;
    
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
};
