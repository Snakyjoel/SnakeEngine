#include "MainMenuState.hpp"
#include "PlayState.hpp"
#include "StoryMenuState.hpp"
#include "FreeplayState.hpp"
#include "OptionsMenuState.hpp"
#include "ModsMenuState.hpp"
#include "CreditsState.hpp"
#include "TitleState.hpp"
#include "AchievementsMenuState.hpp"
#include "../objects/ButtonPrompt.hpp"
#include "../backend/ModHandler.hpp"
#include "WeekParser.hpp"
#include "../backend/AudioEngine.hpp"
#include <cmath>

bool MainMenuState::comingFromFreeplay = false;

void MainMenuState::init() {
    // Reset isolation
    ModHandler::get().currentModFolder = "";

    MusicPlayer::playMenuMusic();

    VCRFontFix();

    // Load backgrounds
    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
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

    auto setupItem = [&](const std::string& name, const std::string& prefix, float x, float y, bool isLoop, bool darkened = false) {
        MenuItem item;
        item.name = name;
        item.x = x;
        item.y = y;
        item.isDarkened = darkened;

        item.animate.loadSheet("preload/images/menus/menuOptions");
        item.animate.scaleX = 0.714f;
        item.animate.scaleY = 0.714f;
        item.animate.antialiasing = ClientPrefs::globalAntialiasing;
        item.animate.addAnim("idle", prefix + " idle", 24.0f, isLoop);
        
        bool selectedLoop = isLoop;
        if (name == "awards" || name == "options") {
            selectedLoop = false;
        }
        item.animate.addAnim("selected", prefix + " selected", 24.0f, selectedLoop);
        item.animate.play("idle");

        menuItems.push_back(item);
    };

    setupItem("story", "story_mode", 200, 50, true);
    setupItem("freeplay", "freeplay", 200, 100, true);
    setupItem("mods", "mods", 200, 150, true);
    setupItem("credits", "credits", 200, 200, true);
    setupItem("awards", "achievements", 60, 170, true, false);
    setupItem("options", "options", 260, 170, true);

    curSelected = 0;
    menuItems[curSelected].animate.play("selected");
    lerpSelected = 0.0f;
    botParallaxX = 0.0f;
    botParallaxY = 0.0f;
    botParallaxTargetX = 0.0f;
    botParallaxTargetY = 0.0f;
    botTouchActive = false;
    if (comingFromFreeplay) {
        introTimer = 0.0f;
        comingFromFreeplay = false;
    } else {
        introTimer = 1.0f;
    }
}

void MainMenuState::update(float dt) {
    if (introTimer < 1.0f) {
        introTimer += dt * 2.5f;
        if (introTimer > 1.0f) introTimer = 1.0f;
    }

    if (isTransitioningToFreeplay) {
        transitionTimer += dt;
        if (transitionTimer >= 0.35f) {
            MusicBeatState::skipTransition = true;
            switchState(new FreeplayState());
        }
        
        for (auto& item : menuItems) {
            item.animate.update(dt);
        }
        lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-10.0f * dt));
        botParallaxX += (0.0f - botParallaxX) * (1.0f - exp2f(-12.0f * dt));
        botParallaxY += (0.0f - botParallaxY) * (1.0f - exp2f(-12.0f * dt));
        return;
    }

    u32 kDown = hidKeysDown();
    touchPosition touch;
    hidTouchRead(&touch);

    int oldSelected = curSelected;
    
    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        curSelected--;
        if (curSelected < 0) curSelected = 3;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected > 3) curSelected = 0;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }

    if (keyJustPressed(KEY_Y)) {
        curSelected = 4;
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        switchState(new AchievementsMenuState());
    }
    if (keyJustPressed(KEY_X)) {
        curSelected = 5;
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        switchState(new OptionsMenuState());
    }

    if (keyJustPressed(KEY_TOUCH)) {
        if (touch.py > 120) {
            if (touch.px < 160) {
                curSelected = 4;
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                switchState(new AchievementsMenuState());
            } else {
                curSelected = 5;
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                switchState(new OptionsMenuState());
            }
        }
    }

    if (oldSelected != curSelected) {
        menuItems[oldSelected].animate.play("idle");
        menuItems[curSelected].animate.play("selected");
    }
 
    for (auto& item : menuItems) {
        item.animate.update(dt);
    }

    if (keyJustPressed(KEY_A | KEY_START)) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        if (curSelected == 0) switchState(new StoryMenuState());
        else if (curSelected == 1) {
            isTransitioningToFreeplay = true;
            transitionTimer = 0.0f;
        }
        else if (curSelected == 2) switchState(new ModsMenuState());
        else if (curSelected == 3) switchState(new CreditsState());
        else if (curSelected == 4) switchState(new AchievementsMenuState());
        else if (curSelected == 5) switchState(new OptionsMenuState());
    }

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        switchState(new TitleState());
    }

    lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-10.0f * dt));

    // Bottom screen touch parallax
    u32 kHeld = hidKeysHeld();
    u32 kUp = hidKeysUp();
    if (kDown & KEY_TOUCH) {
        botTouchActive = true;
        botTouchStartX = touch.px;
        botTouchStartY = touch.py;
    } else if (kHeld & KEY_TOUCH && botTouchActive) {
        touchPosition tp; hidTouchRead(&tp);
        // Max drag range: 12px offset each side
        float dx = (tp.px - botTouchStartX);
        float dy = (tp.py - botTouchStartY);
        if (dx >  12.0f) dx =  12.0f;
        if (dx < -12.0f) dx = -12.0f;
        if (dy >  10.0f) dy =  10.0f;
        if (dy < -10.0f) dy = -10.0f;
        botParallaxTargetX = dx;
        botParallaxTargetY = dy;
    } else if (kUp & KEY_TOUCH) {
        botTouchActive = false;
        botParallaxTargetX = 0.0f;
        botParallaxTargetY = 0.0f;
    } else if (!botTouchActive) {
        botParallaxTargetX = 0.0f;
        botParallaxTargetY = 0.0f;
    }
    botParallaxX += (botParallaxTargetX - botParallaxX) * (1.0f - exp2f(-12.0f * dt));
    botParallaxY += (botParallaxTargetY - botParallaxY) * (1.0f - exp2f(-12.0f * dt));
}

void MainMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    float alpha = isTransitioningToFreeplay ? std::max(0.0f, 1.0f - (transitionTimer / 0.35f)) : 1.0f;
    float drawAlpha = introTimer * alpha;

    ClearTextBuf();
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(253, 232, 113, 255)); // fde871

    if (bgSheet && topBG.tex) {
        // Calculate minimum scale to cover the 400x240 screen
        float minScaleX = 400.0f / topBG.subtex->width;
        float minScaleY = 240.0f / topBG.subtex->height;
        float minScale = std::max(minScaleX, minScaleY);
        
        // We use 0.95f as default or the minScale if it requires more
        float parallaxScale = std::max(0.95f, minScale);
        
        float bgW = topBG.subtex->width * parallaxScale;
        float bgH = topBG.subtex->height * parallaxScale;
        // Center horizontally
        float drawX = (400.0f - bgW) / 2.0f;
        
        // Y scroll: Map linearly between 0.0f (top of image) and 240.0f - bgH (bottom of image)
        float minY = 240.0f - bgH;  // bottom limit: image bottom at screen bottom
        float maxY = 0.0f;          // top limit: image top at screen top
        
        float t = 0.0f;
        if (!menuItems.empty() && menuItems.size() > 1) {
            t = lerpSelected / (menuItems.size() - 1.0f);
        }
        float drawY = t * minY;
        
        if (drawY < minY) drawY = minY;
        if (drawY > maxY) drawY = maxY;
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(220, 120, 39, (u8)(drawAlpha * 255.0f)), 1.0f);
        C2D_DrawImageAt(topBG, drawX, drawY, 0.1f, &tint, parallaxScale, parallaxScale);
    }

    for (int i = 0; i < 4; i++) {
        MenuItem& item = menuItems[i];
        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        if (item.isDarkened || drawAlpha < 1.0f) {
            float itemAlpha = drawAlpha;
            if (item.isDarkened) itemAlpha *= 0.3f;
            C2D_AlphaImageTint(&tint, itemAlpha); 
            tintPtr = &tint;
        }
        item.animate.drawCentered(item.x, item.y, 0.5f, 1.0f, 1.0f, tintPtr);
    }

    u32 textCol = C2D_Color32(255, 255, 255, (u8)(drawAlpha * 255.0f));
    AddText("Snake Engine v2.6.7", 8, 225, 0.38f, false, 1.5f, textCol, 0.0f);

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(253, 232, 113, 255)); // fde871

    if (bottomBGSheet && bottomBG.tex) {
        // Touch parallax: scale 1.10 so there's room to shift
        float bScale = 1.10f;
        float bW = bottomBG.subtex->width * bScale;
        float bH = bottomBG.subtex->height * bScale;
        float baseX = (320.0f - bW) / 2.0f;
        float baseY = (240.0f - bH) / 2.0f;
        float drawX = baseX + botParallaxX;
        float drawY = baseY + botParallaxY;
        // Clamp so edges never show
        if (drawX < 320.0f - bW) drawX = 320.0f - bW;
        if (drawX > 0.0f) drawX = 0.0f;
        if (drawY < 240.0f - bH) drawY = 240.0f - bH;
        if (drawY > 0.0f) drawY = 0.0f;
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(220, 120, 39, (u8)(drawAlpha * 255.0f)), 1.0f);
        C2D_DrawImageAt(bottomBG, drawX, drawY, 0.1f, &tint, bScale, bScale);
    }

    for (int i = 4; i < 6; i++) {
        MenuItem& item = menuItems[i];
        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        if (item.isDarkened || drawAlpha < 1.0f) {
            float itemAlpha = drawAlpha;
            if (item.isDarkened) itemAlpha *= 0.3f;
            C2D_AlphaImageTint(&tint, itemAlpha); 
            tintPtr = &tint;
        }
        item.animate.drawCentered(item.x, item.y, 0.5f, 1.0f, 1.0f, tintPtr);
    }

    ButtonPrompt::drawPrompt("y", "Achievements", 8.0f, 210.0f, 0.60f, drawAlpha);
    float xPromptWidth = ButtonPrompt::getPromptWidth("x", "Options", 0.60f);
    ButtonPrompt::drawPrompt("x", "Options", 320.0f - 8.0f - xPromptWidth, 210.0f, 0.60f, drawAlpha, 0xFFFFFFFF, 0.95f, 1.0f, true);
}

void MainMenuState::exitState() {
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    C2D_TextBufDelete(vcrFontBuf);
}
