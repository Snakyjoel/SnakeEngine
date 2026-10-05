#include "OutdatedState.hpp"
#include "MainMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../backend/Macros.hpp"
#include <sstream>

OutdatedState::OutdatedState(int compResult, const std::string& onlineVer, const std::string& source) {
    comparison = compResult;
    latestVersion = onlineVer;
    updateSource = source;
    bodyTextLines = 0;
    fadeAlpha = 1.0f;
    transitioning = false;
    textBuf = nullptr;
    vcrFont = nullptr;
    vcrFontBuf = nullptr;
    gridScrollTime = 0.0f;
    bgLogoSheet = nullptr;
    hifellaSheet = nullptr;
}

OutdatedState::~OutdatedState() {
    if (textBuf) {
        C2D_TextBufDelete(textBuf);
        textBuf = nullptr;
    }
    if (vcrFontBuf) {
        C2D_TextBufDelete(vcrFontBuf);
        vcrFontBuf = nullptr;
    }
}

void OutdatedState::init() {
    VCRFontFix();
    textBuf = C2D_TextBufNew(1024);
    
    if (comparison < 0) {
        if (updateSource == "gamejolt") {
            bgLogoSheet = SpritesheetCache::get().load("preload/images/gamejolt");
        } else {
            bgLogoSheet = SpritesheetCache::get().load("preload/images/gamebanana");
        }
    } else {
        hifellaSheet = SpritesheetCache::get().load("preload/images/hifella");
    }

    std::string titleStr = (comparison < 0) ? "OUTDATED VERSION!" : "HOLY MOLY THANKS FOR TESTING!";
    C2D_TextFontParse(&titleText, vcrFont, textBuf, titleStr.c_str());
    C2D_TextOptimize(&titleText);

    float dummyH = 0.0f;
    C2D_TextGetDimensions(&titleText, 0.6f, 0.6f, &titleWidth, &dummyH);

    if (comparison < 0) {
        std::string siteName = (updateSource == "gamejolt") ? "GameJolt" : "GameBanana";
        bodyLines = {
            "Sup bro, looks like you're running",
            "an outdated version of Snake Engine (v" + MainMenuState::version + ").",
            "",
            "Please update to v" + latestVersion + "!",
            "Go to " + siteName + " to download it.",
            "",
            "Press A or START to proceed anyway.",
            "",
            "Thank you for using the Engine!"
        };
    } else {
        bodyLines = {
            "Beep Beep Turulititu",
            "Start diggin",
            "at your butt,",
            "Twin.",
            "What???",
            "WHAAAAAAAAAAAT?!?!?!?",
            "",
            "Press A or START to proceed anyway."
        };
    }

    bodyTextLines = (int)bodyLines.size();
    for (int i = 0; i < bodyTextLines && i < 12; i++) {
        C2D_TextFontParse(&bodyText[i], vcrFont, textBuf, bodyLines[i].c_str());
        C2D_TextOptimize(&bodyText[i]);
        C2D_TextGetDimensions(&bodyText[i], 0.4f, 0.4f, &bodyWidths[i], &dummyH);
    }
}

void OutdatedState::update(float dt) {
    gridScrollTime += dt * 30.0f;

    if (transitioning) {
        fadeAlpha -= dt * 2.0f;
        if (fadeAlpha <= 0.0f) {
            fadeAlpha = 0.0f;
            switchState(new MainMenuState());
        }
        return;
    }

    u32 kDown = hidKeysDown();
    if (kDown & (KEY_A | KEY_START | KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        transitioning = true;
    }
}

void OutdatedState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    float alphaVal = fadeAlpha;

    beginScreen(top);
    if (comparison < 0) {
        C2D_DrawRectSolid(0, 0, 0.0f, 400, 240, C2D_Color32(0, 0, 25, 255));
        
        float gridSize = 32.0f;
        float gx = fmodf(gridScrollTime, gridSize);
        float gy = fmodf(gridScrollTime, gridSize);
        u32 gridCol = C2D_Color32(0, 0, 70, 255);
        float grid3DOffset = get3DOffset(-10.0f);

        for (float x = -gridSize; x < 400 + gridSize; x += gridSize) {
            for (float y = -gridSize; y < 240 + gridSize; y += gridSize) {
                if ((int(x / gridSize) + int(y / gridSize)) % 2 == 0) {
                    C2D_DrawRectSolid(x + gx + grid3DOffset, y + gy, 0.01f, gridSize, gridSize, gridCol);
                }
            }
        }
    } else {
        if (hifellaSheet && !hifellaSheet->frames.empty()) {
            Frame& f = hifellaSheet->frames[0];
            float sx = 400.0f / frameLogicalW(f);
            float sy = 240.0f / frameLogicalH(f);
            drawFrameAt(f, 0.0f, 0.0f, 0.05f, nullptr, sx, sy);
        } else {
            C2D_DrawRectSolid(0, 0, 0.0f, 400, 240, C2D_Color32(20, 40, 20, 255));
        }
    }

    u32 textCol = CWhite;
    u32 yellowCol = CYellow;
    float text3DOffset = get3DOffset(10.0f);
    
    // Draw Title (Both border and text share identical titleX including 3D offset)
    float titleX = 200.0f - (titleWidth / 2.0f) + text3DOffset;
    u32 titleCol = (comparison < 0) ? C2D_Color32(255, 0, 0, 255) : C2D_Color32(100, 255, 100, 255);
    DrawTextBorderFull(&titleText, titleX, 20.0f, 0.5f, 0.6f, 0.6f, 2.0f, CBlack);
    C2D_DrawText(&titleText, C2D_WithColor, titleX, 20.0f, 0.5f, 0.6f, 0.6f, titleCol);

    // Draw Body Lines (Both border and text share identical lineX including 3D offset)
    float startY = 60.0f;
    for (int i = 0; i < bodyTextLines && i < 12; i++) {
        u32 col = textCol;
        if (bodyLines[i].find("v" + latestVersion) != std::string::npos || bodyLines[i].find("v" + MainMenuState::version) != std::string::npos) {
            col = yellowCol;
        }
        float lineX = 200.0f - (bodyWidths[i] / 2.0f) + text3DOffset;
        DrawTextBorderFull(&bodyText[i], lineX, startY, 0.5f, 0.4f, 0.4f, 1.5f, CBlack);
        C2D_DrawText(&bodyText[i], C2D_WithColor, lineX, startY, 0.5f, 0.4f, 0.4f, col);
        startY += 18.0f;
    }

    // Top screen transition fade
    if (alphaVal < 1.0f) {
        C2D_DrawRectSolid(0, 0, 0.95f, 400, 240, C2D_Color32(0, 0, 0, (u8)((1.0f - alphaVal) * 255.0f)));
    }

    beginScreen(bottom);
    if (comparison < 0) {
        C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, CBlack);
        if (bgLogoSheet && !bgLogoSheet->frames.empty()) {
            Frame& f = bgLogoSheet->frames[0];
            float scale = 0.5f;
            float drawX = 160.0f - ((frameLogicalW(f) * scale) / 2.0f);
            float drawY = 120.0f - ((frameLogicalH(f) * scale) / 2.0f);
            drawFrameAt(f, drawX, drawY, 0.5f, nullptr, scale, scale);
        }
    } else {
        if (hifellaSheet && !hifellaSheet->frames.empty()) {
            Frame& f = hifellaSheet->frames[0];
            float sx = 320.0f / frameLogicalW(f);
            float sy = 240.0f / frameLogicalH(f);
            drawFrameAt(f, 0.0f, 0.0f, 0.05f, nullptr, sx, sy);
        } else {
            C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, CBlack);
        }
    }

    // Bottom screen transition fade
    if (alphaVal < 1.0f) {
        C2D_DrawRectSolid(0, 0, 0.95f, 320, 240, C2D_Color32(0, 0, 0, (u8)((1.0f - alphaVal) * 255.0f)));
    }
}
