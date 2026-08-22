#include "AchievementsMenuState.hpp"
#include "MainMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "SparrowParser.hpp"
#include "../objects/ButtonPrompt.hpp"
#include <cmath>
#include <sstream>

void AchievementsMenuState::init() {
    
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

    iconAnimate.loadSheet("preload/images/menus/achievementsAssets");

    SpritesheetCache::get().load("shared/images/Alphabet");

    Achievements::loadAchievements();

    for (size_t i = 0; i < Achievements::achievementsStuff.size(); i++) {
        options.push_back(Achievements::achievementsStuff[i]);
        achievementIndex.push_back(i);
    }

    iconAnimate.addAnim("lockedachievement", "lock", 24.0f, true);
    iconAnimate.addAnim("lock", "lock", 24.0f, true);
    iconAnimate.addAnim("template", "template", 24.0f, true);
    for (const auto& opt : options) {
        iconAnimate.addAnim(opt.saveTag, opt.saveTag, 24.0f, true);
    }

    curSelected = 0;
    lerpSelected = 0.0f;
    changeSelection(0);
    textScrollTime = 0.0f;

    quanticoFont = C2D_FontLoad("romfs:/fonts/Quantico-Bold.bcfnt");
    quanticoFontBuf = C2D_TextBufNew(4096);
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
    textScrollTime += dt;
}

void AchievementsMenuState::changeSelection(int change) {
    curSelected += change;
    if (curSelected < 0) curSelected = options.size() - 1;
    if (curSelected >= (int)options.size()) curSelected = 0;
    
    if (change != 0) {
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
        textScrollTime = 0.0f;
    }
}

void AchievementsMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    if (quanticoFontBuf) {
        C2D_TextBufClear(quanticoFontBuf);
    }

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
        
        std::string achieveName = options[i].saveTag;
        bool unlocked = Achievements::isAchievementUnlocked(achieveName);
        bool isSecret = options[i].hidden;
        bool isNotAvailable = options[i].notAvailable;

        float itemAlpha = isSelected ? 1.0f : 0.6f;
        if (isNotAvailable && !unlocked) {
            itemAlpha *= 0.5f; // 50% transparency for not available
        }

        float targetY = 90.0f + (i - lerpSelected) * 90.0f;
        if (targetY < -50.0f || targetY > 290.0f) continue;

        std::string textToDraw;
        if (unlocked) {
            textToDraw = options[i].name;
        } else if (isSecret) {
            textToDraw = "Secret Medal";
        } else {
            textToDraw = "?";
        }

        float tempX = 20.0f;
        float tempY = targetY + 9.0f;

        u8 r = 255, g = 255, b = 255;
        if (isNotAvailable && !unlocked) {
            r = 100; g = 100; b = 100;
        }
        u32 color = C2D_Color32(r, g, b, (u8)(itemAlpha * 255.0f));

        // Draw background template
        iconAnimate.play("template");
        C2D_ImageTint tempTint;
        if (isNotAvailable && !unlocked) {
            C2D_PlainImageTint(&tempTint, C2D_Color32(100, 100, 100, (u8)(itemAlpha * 255.0f)), 1.0f);
        } else {
            C2D_AlphaImageTint(&tempTint, itemAlpha);
        }
        iconAnimate.drawCentered(tempX + 125.0f, tempY + 36.0f, 0.45f, 1.0f, 1.0f, &tempTint);

        // Draw text on top of template (name) inside the capsule zone (X: 71, Y: 35, W: 168, H: 24)
        std::string nameUpper = textToDraw;
        std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);

        float tw, th;
        C2D_Text tempText;
        C2D_TextFontParse(&tempText, quanticoFont, quanticoFontBuf, nameUpper.c_str());
        C2D_TextGetDimensions(&tempText, 0.9f, 0.9f, &tw, &th);
        
        float textBoxX = tempX + 71.0f;
        float textBoxY = tempY + 35.0f;
        float textBoxW = 168.0f;
        float textBoxH = 24.0f;
        float textBoxXEnd = textBoxX + textBoxW;
        float textY = tempY + 47.0f - th / 2.0f;

        C2D_Flush();

        float sLeft = textBoxY;
        float sRight = textBoxY + textBoxH;
        float sTop = textBoxX;
        float sBottom = textBoxXEnd;

        if (sLeft < 0.0f) sLeft = 0.0f;
        if (sRight > 240.0f) sRight = 240.0f;
        if (sTop < 0.0f) sTop = 0.0f;
        if (sBottom > 400.0f) sBottom = 400.0f;

        u32 phys_left = (u32)(240.0f - sRight);
        u32 phys_right = (u32)(240.0f - sLeft);
        u32 phys_top = (u32)(400.0f - sBottom);
        u32 phys_bottom = (u32)(400.0f - sTop);
        
        C3D_SetScissor(GPU_SCISSOR_NORMAL, phys_left, phys_top, phys_right, phys_bottom);

        float textW = tw;
        if (isSelected && textW > 168.0f) {
            float speed = 35.0f;
            float totalDist = textW + 40.0f;
            float scrollOffset = fmodf(textScrollTime * speed, totalDist);
            float drawX = textBoxX - scrollOffset;
            drawQuanticoText(nameUpper, drawX, textY, 0.9f, false, color, 0.48f, 1.5f);
            drawQuanticoText(nameUpper, drawX + totalDist, textY, 0.9f, false, color, 0.48f, 1.5f);
        } else {
            drawQuanticoText(nameUpper, textBoxX, textY, 0.9f, false, color, 0.48f, 1.5f);
        }

        C2D_Flush();
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
        
        std::string frameName;
        float iconAlpha = itemAlpha;
        if (unlocked) {
            frameName = achieveName;
        } else if (isSecret) {
            frameName = "lock";
        } else {
            frameName = achieveName;
            iconAlpha = itemAlpha * 0.4f;
        }

        iconAnimate.play(frameName);
        C2D_ImageTint tint;
        if (isNotAvailable && !unlocked) {
            C2D_PlainImageTint(&tint, C2D_Color32(100, 100, 100, (u8)(iconAlpha * 255.0f)), 1.0f);
        } else {
            C2D_AlphaImageTint(&tint, iconAlpha);
        }
        // Draw icon centered inside the 60x60 box (at X: tempX + 35, Y: tempY + 35) with scale 0.6f
        iconAnimate.drawCentered(tempX + 35.0f, tempY + 35.0f, 0.5f, 0.6f, 0.6f, &tint);
    }

    drawQuanticoText("ACHIEVEMENTS", 200.0f, 20.0f, 0.8f, true, CWhite, 0.95f, 2.0f);

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(123, 92, 224, 255));

    if (bottomBGSheet && bottomBG.tex) {
        C2D_ImageTint tintBG;
        C2D_PlainImageTint(&tintBG, C2D_Color32(39, 71, 220, 255), 1.0f);
        drawCenteredBG(bottomBG, 320.0f, 240.0f, 0.1f, &tintBG);
    }
    
    C2D_DrawRectSolid(0, 0, 0.2f, 320, 240, C2D_Color32(0, 0, 0, 100));

    if (curSelected >= 0 && curSelected < (int)options.size()) {
        std::string desc;
        bool unlocked = Achievements::isAchievementUnlocked(options[curSelected].saveTag);
        bool isSecret = options[curSelected].hidden;
        
        if (unlocked) {
            desc = options[curSelected].description;
        } else if (isSecret) {
            desc = "Unlock to view details.";
        } else {
            desc = options[curSelected].description;
        }

        std::string wrapped = wrapText(desc, 0.45f, 280.0f);
        
        std::vector<std::string> lines;
        std::stringstream ss(wrapped);
        std::string line;
        while (std::getline(ss, line, '\n')) {
            lines.push_back(line);
        }
        
        float lineH = 18.0f;
        float totalH = lines.size() * lineH;
        float startY = 120.0f - totalH / 2.0f;
        
        for (size_t l = 0; l < lines.size(); l++) {
            drawQuanticoText(lines[l], 160.0f, startY + l * lineH, 0.45f, true, CWhite, 0.5f, 1.0f);
        }
    }
    ButtonPrompt::drawPrompt("b", "Back", 8.0f, 205.0f, 0.70f, 1.0f);
}

std::string AchievementsMenuState::wrapText(const std::string& text, float scale, float maxWidth) {
    if (text.empty()) return "";
    std::string result = "";
    std::string line = "";
    std::string word = "";
    std::stringstream ss(text);
    while (ss >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        float w = getQuanticoTextWidth(testLine, scale);
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

void AchievementsMenuState::drawQuanticoText(const std::string& textStr, float x, float y, float scale, bool centered, u32 color, float depth, float border, float maxWidth) {
    if (!quanticoFont || !quanticoFontBuf) return;
    
    C2D_Text gText;
    C2D_TextFontParse(&gText, quanticoFont, quanticoFontBuf, textStr.c_str());
    C2D_TextOptimize(&gText);
    
    float tw, th;
    C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
    float actualScale = scale;
    if (maxWidth > 0.0f && tw > maxWidth) {
        actualScale *= maxWidth / tw;
        C2D_TextGetDimensions(&gText, actualScale, actualScale, &tw, &th);
    }
    
    float dx = centered ? x - (tw / 2.0f) : x;
    float dy = y;
    dx = std::round(dx); dy = std::round(dy);
    
    if (border > 0.0f) {
        u8 a = (color >> 24) & 0xFF;
        u32 borderColor = C2D_Color32(0, 0, 0, a);
        DrawTextBorderFull(&gText, dx, dy, depth - 0.01f, actualScale, actualScale, border, borderColor);
    }
    
    C2D_DrawText(&gText, C2D_WithColor, dx, dy, depth, actualScale, actualScale, color);
}

float AchievementsMenuState::getQuanticoTextWidth(const std::string& textStr, float scale) {
    if (!quanticoFont || !quanticoFontBuf) return 0.0f;
    C2D_Text gText;
    C2D_TextFontParse(&gText, quanticoFont, quanticoFontBuf, textStr.c_str());
    float tw, th;
    C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
    return tw;
}

void AchievementsMenuState::exitState() {
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    if (quanticoFontBuf) {
        C2D_TextBufDelete(quanticoFontBuf);
        quanticoFontBuf = nullptr;
    }
    if (quanticoFont) {
        C2D_FontFree(quanticoFont);
        quanticoFont = nullptr;
    }
}
