//Tbh... i truly like the vanilla pause menu
#include "NormalPauseRenderer.hpp"
#include "PauseSubState.hpp"
#include "../states/PlayState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/Alphabet.hpp"
#include "../backend/Macros.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
#include <cmath>

extern C2D_Font globalVCRFont;

NormalPauseRenderer::NormalPauseRenderer(PauseSubState* parent)
    : subState(parent)
{
}

NormalPauseRenderer::~NormalPauseRenderer() {
    if (pauseTextBuf) {
        C2D_TextBufDelete(pauseTextBuf);
        pauseTextBuf = nullptr;
    }
    if (infoTextBuf) {
        C2D_TextBufDelete(infoTextBuf);
        infoTextBuf = nullptr;
    }
}

void NormalPauseRenderer::init() {
    infoTimer = 0.0f;
    if (!pauseTextBuf) {
        pauseTextBuf = C2D_TextBufNew(1024);
    }
    if (!infoTextBuf) {
        infoTextBuf = C2D_TextBufNew(256);
    }
    setupMenuTexts(subState->pauseMenuItems, "PAUSED");
    setupInfoTexts();
}

void NormalPauseRenderer::setupMenuTexts(const std::vector<std::string>& items, const std::string& title) {
    (void)title;
    if (!pauseTextBuf || !globalVCRFont) return;

    C2D_TextBufClear(pauseTextBuf);
    pauseOptionObjs.clear();
    optionWidths.clear();
    optionHeights.clear();

    pauseOptionObjs.resize(items.size());
    optionWidths.resize(items.size(), 0.0f);
    optionHeights.resize(items.size(), 0.0f);

    float fontScale = 0.50f;
    for (size_t i = 0; i < items.size(); i++) {
        C2D_TextFontParse(&pauseOptionObjs[i], globalVCRFont, pauseTextBuf, items[i].c_str());
        C2D_TextOptimize(&pauseOptionObjs[i]);
        C2D_TextGetDimensions(&pauseOptionObjs[i], fontScale, fontScale, &optionWidths[i], &optionHeights[i]);
    }
}

void NormalPauseRenderer::setupInfoTexts() {
    if (!infoTextBuf || !globalVCRFont) return;
    C2D_TextBufClear(infoTextBuf);

    std::string songName = "Test";
    std::string diffName = "Normal";
    int deaths = 0;
    if (PlayState::instance) {
        songName = PlayState::instance->curSong;
        diffName = PlayState::instance->currentDifficulty;
        if (diffName.empty()) diffName = "Normal";
        deaths = PlayState::blueBalls;
    }

    std::string rows[3] = {
        songName,
        "Difficulty: " + diffName,
        std::to_string(deaths) + " Blue Balls"
    };

    float fontScale = 0.55f;
    float lineSpacing = 8.0f;
    infoTotalHeight = 0.0f;
    maxInfoWidth = 0.0f;

    for (int i = 0; i < 3; i++) {
        C2D_TextFontParse(&infoTextObj[i], globalVCRFont, infoTextBuf, rows[i].c_str());
        C2D_TextOptimize(&infoTextObj[i]);
        C2D_TextGetDimensions(&infoTextObj[i], fontScale, fontScale, &infoWidths[i], &infoHeights[i]);
        if (infoWidths[i] > maxInfoWidth) maxInfoWidth = infoWidths[i];
        infoTotalHeight += infoHeights[i];
        if (i < 2) infoTotalHeight += lineSpacing;
    }
}

void NormalPauseRenderer::onMenuChanged(const std::vector<std::string>& items, const std::string& title) {
    setupMenuTexts(items, title);
}

void NormalPauseRenderer::update(float dt) {
    infoTimer += dt;
}

void NormalPauseRenderer::onCursorMove() {
    if (ClientPrefs::alphabetPause) {
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
    }
}

void NormalPauseRenderer::drawTop(C3D_RenderTarget* target, float screenW, float screenH) {
    (void)target;
    C2D_DrawRectSolid(0, 0, 0.99f, screenW, screenH, C2D_Color32(0, 0, 0, 150));

    const auto& items = subState->pauseMenuItems;
    int curSel = subState->pauseSelection;
    float lerpSel = subState->pauseLerpSelection;

    if (ClientPrefs::alphabetPause) {
        for (int i = 0; i < (int)items.size(); i++) {
            float targetY = (screenH / 2.0f) - (35.0f / 2.0f) + (i - lerpSel) * 60.0f;
            if (targetY < -70.0f || targetY > screenH + 40.0f) continue;

            bool sel = (i == curSel);
            float targetX = 10.0f + (i - lerpSel) * 10.0f;

            float scale = 1.5f;
            float alpha = sel ? 1.0f : 0.6f;
            Alphabet::draw(items[i], targetX, targetY, scale, alpha, false, 0xFFFFFFFF, 1.0f);
        }
    } else {
        for (int i = 0; i < (int)items.size() && i < (int)pauseOptionObjs.size(); i++) {
            float targetY = (screenH / 2.0f) - (35.0f / 2.0f) + (i - lerpSel) * 60.0f;
            if (targetY < -60.0f || targetY > screenH + 30.0f) continue;

            bool sel = (i == curSel);
            float targetX = 10.0f + (i - lerpSel) * 20.0f;

            float fScale = sel ? 0.85f : 0.6f;
            u32 color = sel ? CWhite : C2D_Color32(160, 160, 160, 200);

            DrawTextBorderFull(&pauseOptionObjs[i], targetX, targetY, 0.995f, fScale, fScale, 2.0f, CBlack);
            C2D_DrawText(&pauseOptionObjs[i], C2D_WithColor, targetX, targetY, 1.0f, fScale, fScale, color);
        }
    }
}

void NormalPauseRenderer::drawBottom(C3D_RenderTarget* target, float screenW, float screenH) {
    (void)target;
    C2D_DrawRectSolid(0, 0, 0.99f, screenW, screenH, C2D_Color32(0, 0, 0, 150));

    float fontScale = 0.55f;
    float lineSpacing = 8.0f;
    float startY = floorf((screenH - infoTotalHeight) / 2.0f);
    float currentY = startY;

    for (int i = 0; i < 3; i++) {
        float startDelay = i * 0.15f;
        float duration = 0.35f;
        float alpha = 0.0f;

        if (infoTimer > startDelay) {
            float progress = (infoTimer - startDelay) / duration;
            if (progress > 1.0f) progress = 1.0f;
            alpha = 1.0f - (1.0f - progress) * (1.0f - progress);
        }

        if (alpha > 0.005f) {
            u8 a = (u8)(alpha * 255.0f);
            float x = floorf((screenW - infoWidths[i]) / 2.0f);
            float y = floorf(currentY + (1.0f - alpha) * -5.0f);

            DrawTextBorderFull(&infoTextObj[i], x, y, 0.995f, fontScale, fontScale, 1.5f, C2D_Color32(0, 0, 0, a));
            C2D_DrawText(&infoTextObj[i], C2D_WithColor, x, y, 1.0f, fontScale, fontScale, C2D_Color32(255, 255, 255, a));
        }

        currentY += infoHeights[i] + lineSpacing;
    }
}

