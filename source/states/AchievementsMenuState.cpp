#include "AchievementsMenuState.hpp"
#include "MainMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "SparrowParser.hpp"
#include <cmath>
#include <sstream>

void AchievementsMenuState::init() {
    VCRFontFix();
    
    std::string bgPath = "romfs:/shared/images/menuBGBlue.t3x";
    if (!Paths::fileExists(bgPath)) bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        bgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
        if (bgSheet) {
            topBG = C2D_SpriteSheetGetImage(bgSheet, 0);
            if (topBG.tex) C3D_TexSetFilter(topBG.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }
    
    std::string bgbPath = "romfs:/shared/images/menuBGB.t3x";
    if (Paths::fileExists(bgbPath)) {
        bottomBGSheet = C2D_SpriteSheetLoad(bgbPath.c_str());
        if (bottomBGSheet) {
            bottomBG = C2D_SpriteSheetGetImage(bottomBGSheet, 0);
            if (bottomBG.tex) C3D_TexSetFilter(bottomBG.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }

    iconAnimate.loadSheet("preload/images/menus/achievements");

    SpritesheetCache::get().load("shared/images/Alphabet");

    Achievements::loadAchievements();

    for (size_t i = 0; i < Achievements::achievementsStuff.size(); i++) {
        if (!Achievements::achievementsStuff[i].hidden || Achievements::isAchievementUnlocked(Achievements::achievementsStuff[i].saveTag)) {
            options.push_back(Achievements::achievementsStuff[i]);
            achievementIndex.push_back(i);
        }
    }

    iconAnimate.addAnim("lockedachievement", "lockedachievement", 24.0f, true);
    for (const auto& opt : options) {
        iconAnimate.addAnim(opt.saveTag, opt.saveTag, 24.0f, true);
    }

    curSelected = 0;
    lerpSelected = 0.0f;
    changeSelection(0);
}

void AchievementsMenuState::update(float dt) {
    u32 kDown = hidKeysDown();

    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        changeSelection(-1);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        changeSelection(1);
    }

    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        switchState(new MainMenuState());
    }

    iconAnimate.update(dt);
    lerpSelected += ((float)curSelected - lerpSelected) * (1.0f - exp2f(-10.0f * dt));
}

void AchievementsMenuState::changeSelection(int change) {
    curSelected += change;
    if (curSelected < 0) curSelected = options.size() - 1;
    if (curSelected >= (int)options.size()) curSelected = 0;
    
    if (change != 0) {
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
    }
}

void AchievementsMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(146, 113, 253, 255));

    if (bgSheet && topBG.tex) {
        float minScaleX = 400.0f / topBG.subtex->width;
        float minScaleY = 240.0f / topBG.subtex->height;
        float scale = std::max(0.95f, std::max(minScaleX, minScaleY));
        float bgW = topBG.subtex->width * scale;
        float bgH = topBG.subtex->height * scale;
        float drawX = (400.0f - bgW) / 2.0f;
        float drawY = -((lerpSelected / std::max(1.0f, (float)(options.size() - 1))) * (bgH - 240.0f));
        if (drawY > 0) drawY = 0;
        if (drawY < 240.0f - bgH) drawY = 240.0f - bgH;
        C2D_ImageTint tintBG;
        C2D_PlainImageTint(&tintBG, C2D_Color32(39, 71, 220, 255), 1.0f);
        C2D_DrawImageAt(topBG, drawX, drawY, 0.1f, &tintBG, scale, scale);
    }

    for (int i = 0; i < (int)options.size(); i++) {
        bool isSelected = (i == curSelected);
        float itemAlpha = isSelected ? 1.0f : 0.6f;
        float targetY = 90.0f + (i - lerpSelected) * 90.0f;
        
        if (targetY < -50.0f || targetY > 290.0f) continue;

        std::string achieveName = options[i].saveTag;
        bool unlocked = Achievements::isAchievementUnlocked(achieveName);
        std::string textToDraw = unlocked ? options[i].name : "?";

        float textHeight = 70.0f * 1.1f * (240.0f / 720.0f);
        CachedSpritesheet* alphabetSheet = SpritesheetCache::get().load("shared/images/Alphabet");
        if (alphabetSheet) {
            for (const auto& f : alphabetSheet->frames) {
                if (f.name == "A0000") {
                    textHeight = frameLogicalH(f) * 1.1f * (240.0f / 720.0f);
                    break;
                }
            }
        }

        float centerY = targetY + 25.0f;
        float textY = centerY - textHeight / 2.0f;

        u32 color = C2D_Color32(255, 255, 255, (u8)(itemAlpha * 255.0f));
        Alphabet::draw(textToDraw, 110.0f, textY, 1.1f, itemAlpha, false, color);
        
        std::string frameName = unlocked ? achieveName : "lockedachievement";
        iconAnimate.play(frameName);
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, itemAlpha);
        iconAnimate.drawCentered(60.0f, centerY, 0.5f, 0.8f, 0.8f, &tint);
    }

    Alphabet::draw("ACHIEVEMENTS", 200.0f, 20.0f, 1.2f, 1.0f, true, CWhite);

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(123, 92, 224, 255));

    if (bottomBGSheet && bottomBG.tex) {
        C2D_ImageTint tintBG;
        C2D_PlainImageTint(&tintBG, C2D_Color32(39, 71, 220, 255), 1.0f);
        drawCenteredBG(bottomBG, 320.0f, 240.0f, 0.1f, &tintBG);
    }
    
    C2D_DrawRectSolid(0, 0, 0.2f, 320, 240, C2D_Color32(0, 0, 0, 100));

    if (curSelected >= 0 && curSelected < (int)options.size()) {
        std::string desc = options[curSelected].description;
        std::string wrapped = wrapText(desc, 0.7f, 300.0f);
        AddTextCentered(wrapped, 160.0f, 100.0f, 0.7f, 2.0f, CWhite, 0.0f);
    }
}

std::string AchievementsMenuState::wrapText(const std::string& text, float scale, float maxWidth) {
    if (text.empty()) return "";
    std::string result = "";
    std::string line = "";
    std::string word = "";
    std::stringstream ss(text);
    while (ss >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        C2D_Text tempText;
        C2D_TextFontParse(&tempText, vcrFont, vcrFontBuf, testLine.c_str());
        float w = tempText.width * scale;
        if (w > maxWidth && !line.empty()) {
            result += line + "\n";
            line = word;
        } else {
            line = testLine;
        }
    }
    result += line;
    return result;
}

void AchievementsMenuState::exitState() {
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    C2D_TextBufDelete(vcrFontBuf);
}
