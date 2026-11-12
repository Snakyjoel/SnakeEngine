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
#include "../objects/Alphabet.hpp"
#include "../backend/ModHandler.hpp"
#include "WeekParser.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <cmath>

bool MainMenuState::comingFromFreeplay = false;
std::string MainMenuState::version = "3.7.9";

MainMenuState::MainMenuState(bool startInFreeplay)
    : startInFreeplay(startInFreeplay), freeplaySubState(nullptr)
{
}

MainMenuState::~MainMenuState() {
    if (freeplaySubState) {
        delete freeplaySubState;
        freeplaySubState = nullptr;
    }
}

void MainMenuState::init() {
    // Reset isolation
    ModHandler::get().currentModFolder = "";

    versionDisplayStr = "v" + version;
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

    // Precompute top background layout
    if (bgSheet && topBG.tex && topBG.subtex) {
        float minScaleX = 400.0f / (float)topBG.subtex->width;
        float minScaleY = 240.0f / (float)topBG.subtex->height;
        topBgScale = fmaxf(0.95f, fmaxf(minScaleX, minScaleY));
        topBgW = topBG.subtex->width * topBgScale;
        topBgH = topBG.subtex->height * topBgScale;
        topBgDrawX = (400.0f - topBgW) / 2.0f;
        topBgMinY = 240.0f - topBgH;
    }

    // Precompute bottom background layout
    if (bottomBGSheet && bottomBG.tex && bottomBG.subtex) {
        botBgScale = 1.10f;
        botBgW = bottomBG.subtex->width * botBgScale;
        botBgH = bottomBG.subtex->height * botBgScale;
        botBgBaseX = (320.0f - botBgW) / 2.0f;
        botBgBaseY = (240.0f - botBgH) / 2.0f;
    }

    // Precompute button prompt dimensions
    cachedYBtnW = ButtonPrompt::getButtonWidth("y", 0.50f);
    cachedXBtnW = ButtonPrompt::getButtonWidth("x", 0.50f);
    cachedOptTextW = Alphabet::getTextWidth("Options", 0.85f);

    menuItems.clear();
    menuItems.reserve(6);

    auto setupItem = [&](const std::string& name, const std::string& prefix, float x, float y, bool isLoop, bool darkened = false) {
        MenuItem item;
        item.name = name;
        item.x = x;
        item.y = y;
        item.isDarkened = darkened;

        item.animate.loadSheet("preload/images/menus/menuOptions");
        if (name == "awards" || name == "options") {
            item.animate.scaleX = 0.85f;
            item.animate.scaleY = 0.85f;
        } else {
            item.animate.scaleX = 0.714f;
            item.animate.scaleY = 0.714f;
        }
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
    setupItem("awards", "achievements", 60, 60, true, false);
    setupItem("options", "options", 260, 175, true);

    curSelected = startInFreeplay ? 1 : 0;
    menuItems[curSelected].animate.play("selected");
    lerpSelected = (float)curSelected;
    botParallaxX = 0.0f;
    botParallaxY = 0.0f;
    botParallaxTargetX = 0.0f;
    botParallaxTargetY = 0.0f;
    botTouchActive = false;
    introTimer = 1.0f;

    // Init confirm animation state
    selectedSomething = false;
    confirmTimer = 0.0f;
    flickerTimer = 0.0f;
    flickerVisible = true;
    bgMagentaActive = false;
    bgMagentaTimer = 0.0f;
    bgFlickerTimer = 0.0f;
    bgFlickerVisible = false;
    for (int i = 0; i < 6; i++) {
        itemAlphas[i] = startInFreeplay ? (i == curSelected ? 1.0f : 0.0f) : 1.0f;
        itemFadeTimers[i] = 0.0f;
    }

    if (startInFreeplay) {
        freeplaySubState = new FreeplayState(true);
        freeplaySubState->init();
        isFreeplayActive = true;
        freeplaySubState->openSubState(false);
    } else {
        freeplaySubState = nullptr;
        isFreeplayActive = false;
    }
}

void MainMenuState::update(float dt) {
    if (freeplaySubState && isFreeplayActive) {
        freeplaySubState->update(dt);
        if (freeplaySubState->hasFinishedExit) {
            isFreeplayActive = false;

            selectedSomething = false;
            confirmTimer = 0.0f;
            flickerTimer = 0.0f;
            flickerVisible = true;
            bgMagentaActive = false;
            bgFlickerVisible = false;
            for (int i = 0; i < 6; i++) {
                itemAlphas[i] = 1.0f;
                itemFadeTimers[i] = 0.0f;
            }
            if (curSelected >= 0 && curSelected < (int)menuItems.size()) {
                menuItems[curSelected].animate.play("selected");
            }
            MusicPlayer::playMenuMusic();
        } else if (freeplaySubState->isExiting) {
            // As Freeplay slides out, smoothly fade the other menu items back in
            float t = freeplaySubState->exitTimer / 0.35f;
            if (t > 1.0f) t = 1.0f;
            for (int i = 0; i < 6; i++) {
                if (i != curSelected) {
                    itemAlphas[i] = t;
                }
            }
        }
        return;
    }

    if (introTimer < 1.0f) {
        introTimer += dt * 2.5f;
        if (introTimer > 1.0f) introTimer = 1.0f;
    }

    // Confirm animation tick
    if (selectedSomething) {
        confirmTimer += dt;

        // Flicker
        flickerTimer += dt;
        if (flickerTimer >= 0.06f) {
            flickerTimer -= 0.06f;
            flickerVisible = !flickerVisible;
        }

        // Fade-out items
        for (int i = 0; i < 6; i++) {
            if (i == curSelected) continue;
            itemFadeTimers[i] += dt;
            float t = itemFadeTimers[i] / 0.50f;
            if (t > 1.0f) t = 1.0f;
            itemAlphas[i] = (1.0f - t) * (1.0f - t);
        }

        // Flicker BG magenta
        if (ClientPrefs::flashing && bgMagentaActive) {
            bgMagentaTimer += dt;
            bgFlickerTimer += dt;
            if (bgFlickerTimer >= 0.15f) {
                bgFlickerTimer -= 0.15f;
                bgFlickerVisible = !bgFlickerVisible;
            }
            if (bgMagentaTimer >= 1.1f) {
                bgMagentaActive = false;
                bgFlickerVisible = false;
            }
        }

        if (curSelected == 1 && confirmTimer >= 0.70f) {
            if (!freeplaySubState) {
                freeplaySubState = new FreeplayState(true);
                freeplaySubState->init();
            }
            freeplaySubState->openSubState(true);
            isFreeplayActive = true;
            selectedSomething = false;
            confirmTimer = 0.0f;
            return;
        }

        if (confirmTimer >= 1.0f) {
            if (curSelected == 0) switchState(new StoryMenuState());
            else if (curSelected == 2) switchState(new ModsMenuState());
            else if (curSelected == 3) switchState(new CreditsState());
            else if (curSelected == 4) switchState(new AchievementsMenuState());
            else if (curSelected == 5) switchState(new OptionsMenuState());
            return;
        }

        for (auto& item : menuItems) item.animate.update(dt);
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
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected > 3) curSelected = 0;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
    }

    auto confirmOption = [&](int index) {
        if (oldSelected >= 0 && oldSelected < (int)menuItems.size() && oldSelected != index) {
            menuItems[oldSelected].animate.play("idle");
        }
        curSelected = index;
        if (curSelected >= 0 && curSelected < (int)menuItems.size()) {
            menuItems[curSelected].animate.play("selected");
        }
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.adp", 0.7f);
        selectedSomething = true;
        confirmTimer = 0.0f;
        flickerTimer = 0.0f;
        flickerVisible = true;
        for (int i = 0; i < 6; i++) {
            itemFadeTimers[i] = 0.0f;
            if (i != curSelected) itemAlphas[i] = 1.0f;
        }
        bgMagentaActive = true;
        bgMagentaTimer = 0.0f;
        bgFlickerTimer = 0.0f;
        bgFlickerVisible = false;
    };

    if (keyJustPressed(KEY_Y)) {
        confirmOption(4);
        return;
    }
    if (keyJustPressed(KEY_X)) {
        confirmOption(5);
        return;
    }

    if (keyJustPressed(KEY_TOUCH)) {
        if (touch.px >= 10.0f && touch.px <= 240.0f && touch.py >= 30.0f && touch.py <= 90.0f) {
            confirmOption(4);
            return;
        } else if (touch.px >= 80.0f && touch.px <= 310.0f && touch.py >= 145.0f && touch.py <= 205.0f) {
            confirmOption(5);
            return;
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
        confirmOption(curSelected);
        return;
    }

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.adp", 0.7f);
        switchState(new TitleState());
        return;
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
    if (freeplaySubState && isFreeplayActive && !freeplaySubState->isExiting) {
        freeplaySubState->draw(top, bottom);
        return;
    }

    C2D_SetTintMode(C2D_TintMult);
    float drawAlpha = introTimer;
    float topAlpha = drawAlpha;
    if (isFreeplayActive && freeplaySubState && freeplaySubState->isExiting) {
        topAlpha *= std::max(0.0f, std::min(freeplaySubState->exitTimer / 0.35f, 1.0f));
    }

    bool magentaActive = selectedSomething &&
        (!ClientPrefs::flashing || (ClientPrefs::flashing && bgFlickerVisible));

    u32 bgClearCol = magentaActive ? C2D_Color32(253, 113, 155, 255)
                                   : C2D_Color32(253, 232, 113, 255);
    u8  bgTintR    = magentaActive ? 220 : 220;
    u8  bgTintG    = magentaActive ?  40 : 120;
    u8  bgTintB    = magentaActive ? 167 :  39;

    ClearTextBuf();
    C2D_SceneBegin(top);
    C2D_TargetClear(top, bgClearCol);

    if (bgSheet && topBG.tex) {
        float t = 0.0f;
        if (!menuItems.empty() && menuItems.size() > 1) {
            t = lerpSelected / (menuItems.size() - 1.0f);
        }
        float drawY = t * topBgMinY;
        if (drawY < topBgMinY) drawY = topBgMinY;
        if (drawY > 0.0f) drawY = 0.0f;

        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(bgTintR, bgTintG, bgTintB, (u8)(topAlpha * 255.0f)), 1.0f);
        C2D_DrawImageAt(topBG, topBgDrawX + get3DOffset(-10.0f), drawY, 0.1f, &tint, topBgScale, topBgScale);
    }

    for (int i = 0; i < 4; i++) {
        MenuItem& item = menuItems[i];
        float myAlpha = itemAlphas[i];
        if (selectedSomething && i == curSelected) myAlpha *= (flickerVisible ? 1.0f : 0.0f);
        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        float finalAlpha = topAlpha * myAlpha;
        if (item.isDarkened || finalAlpha < 1.0f) {
            float itemAlpha = finalAlpha;
            if (item.isDarkened) itemAlpha *= 0.3f;
            C2D_AlphaImageTint(&tint, itemAlpha);
            tintPtr = &tint;
        }
        item.animate.drawCentered(item.x + get3DOffset(10.0f), item.y, 0.5f, 1.0f, 1.0f, tintPtr);
    }

    u32 textCol = C2D_Color32(255, 255, 255, (u8)(topAlpha * 255.0f));
    AddText(versionDisplayStr, 8 + get3DOffset(10.0f), 225, 0.38f, false, 1.5f, textCol, 0.0f);

    if (!g_isRightEye && bottom) {
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, bgClearCol);

        if (bottomBGSheet && bottomBG.tex) {
            float drawX = botBgBaseX + botParallaxX;
            float drawY = botBgBaseY + botParallaxY;
            // Clamp so edges never show
            if (drawX < 320.0f - botBgW) drawX = 320.0f - botBgW;
            if (drawX > 0.0f) drawX = 0.0f;
            if (drawY < 240.0f - botBgH) drawY = 240.0f - botBgH;
            if (drawY > 0.0f) drawY = 0.0f;
            C2D_ImageTint tint;
            float botBgAlpha = drawAlpha;
            if (selectedSomething && curSelected == 1) {
                botBgAlpha *= std::max(0.0f, 1.0f - (confirmTimer / 0.70f));
            } else if (isFreeplayActive) {
                if (freeplaySubState && freeplaySubState->isExiting) {
                    botBgAlpha *= std::max(0.0f, std::min(freeplaySubState->exitTimer / 0.35f, 1.0f));
                } else {
                    botBgAlpha = 0.0f;
                }
            }
            if (botBgAlpha > 0.001f) {
                C2D_PlainImageTint(&tint, C2D_Color32(bgTintR, bgTintG, bgTintB, (u8)(botBgAlpha * 255.0f)), 1.0f);
                C2D_DrawImageAt(bottomBG, drawX, drawY, 0.1f, &tint, botBgScale, botBgScale);
            }
        }

        float botPromptsAlpha = drawAlpha;
        if (selectedSomething && curSelected == 1) {
            botPromptsAlpha *= std::max(0.0f, 1.0f - (confirmTimer / 0.70f));
        } else if (isFreeplayActive) {
            if (freeplaySubState && freeplaySubState->isExiting) {
                botPromptsAlpha *= std::max(0.0f, std::min(freeplaySubState->exitTimer / 0.35f, 1.0f));
            } else {
                botPromptsAlpha = 0.0f;
            }
        }

        if (botPromptsAlpha > 0.001f) {
            for (int i = 4; i < 6; i++) {
                MenuItem& item = menuItems[i];
                float myAlpha = itemAlphas[i];
                if (selectedSomething && i == curSelected) myAlpha *= (flickerVisible ? 1.0f : 0.0f);
                C2D_ImageTint tint;
                C2D_ImageTint* tintPtr = nullptr;
                float finalAlpha = botPromptsAlpha * myAlpha;
                if (item.isDarkened || finalAlpha < 1.0f) {
                    float itemAlpha = finalAlpha;
                    if (item.isDarkened) itemAlpha *= 0.3f;
                    C2D_AlphaImageTint(&tint, itemAlpha);
                    tintPtr = &tint;
                }
                item.animate.drawCentered(item.x, item.y, 0.5f, 1.0f, 1.0f, tintPtr);
            }

            if (ClientPrefs::buttonPrompts) {
                float textScale = 0.85f;
                float btnScale = 0.50f;
                float slideY = 15.0f * (1.0f - botPromptsAlpha);

                // Achievements
                MenuItem& awardsItem = menuItems[4];
                float awardsAlpha = botPromptsAlpha * itemAlphas[4];
                if (selectedSomething && curSelected == 4) awardsAlpha *= (flickerVisible ? 1.0f : 0.0f);

                if (awardsAlpha > 0.001f) {
                    // Button sprite [Y]
                    float yBtnX = awardsItem.x - (cachedYBtnW * 0.5f);
                    float yBtnY = awardsItem.y + 25.0f + slideY;
                    ButtonPrompt::drawButton("y", yBtnX, yBtnY, btnScale, awardsAlpha, 0.6f);

                    // Alphabet text "Achievements"
                    float textX = awardsItem.x + 48.0f;
                    float textY = awardsItem.y - 8.0f + slideY;
                    Alphabet::draw("Achievements", textX, textY, textScale, awardsAlpha, false, 0xFFFFFFFF, 0.95f);
                }

                // Options
                MenuItem& optionsItem = menuItems[5];
                float optionsAlpha = botPromptsAlpha * itemAlphas[5];
                if (selectedSomething && curSelected == 5) optionsAlpha *= (flickerVisible ? 1.0f : 0.0f);

                if (optionsAlpha > 0.001f) {
                    // Button sprite [X]
                    float xBtnX = optionsItem.x - (cachedXBtnW * 0.5f);
                    float xBtnY = optionsItem.y + 25.0f + slideY;
                    ButtonPrompt::drawButton("x", xBtnX, xBtnY, btnScale, optionsAlpha, 0.6f);

                    // Alphabet text "Options"
                    float optTextX = optionsItem.x - 48.0f - cachedOptTextW;
                    float optTextY = optionsItem.y - 8.0f + slideY;
                    Alphabet::draw("Options", optTextX, optTextY, textScale, optionsAlpha, false, 0xFFFFFFFF, 0.95f);
                }
            }
        }
    }

    if (freeplaySubState && isFreeplayActive) {
        freeplaySubState->draw(top, bottom);
    }
}

void MainMenuState::exitState() {
    if (freeplaySubState) {
        delete freeplaySubState;
        freeplaySubState = nullptr;
    }
    if (bgSheet) {
        C2D_SpriteSheetFree(bgSheet);
        bgSheet = nullptr;
    }
    if (bottomBGSheet) {
        C2D_SpriteSheetFree(bottomBGSheet);
        bottomBGSheet = nullptr;
    }
    C2D_TextBufDelete(vcrFontBuf);
    vcrFontBuf = nullptr;
    SpritesheetCache::get().clear();
}
