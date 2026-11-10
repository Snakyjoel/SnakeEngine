// I know, I could have used pre-rendered sprites and audio, but I decided to make things difficult for myself.
#include "PixelPauseRenderer.hpp"
#include "PauseSubState.hpp"
#include "../states/PlayState.hpp"
#include <cmath>
#include <cstring>
#include <cctype>

extern C2D_Font globalVCRFont;
extern C2D_Font globalPixelFont;

int16_t* PixelPauseRenderer::s_nesSfxBuffers[PixelPauseRenderer::NES_SFX_COUNT] = {nullptr, nullptr, nullptr, nullptr};
uint32_t PixelPauseRenderer::s_nesSfxSampleCounts[PixelPauseRenderer::NES_SFX_COUNT] = {0, 0, 0, 0};
ndspWaveBuf PixelPauseRenderer::s_nesWaveBuf;

PixelPauseRenderer::PixelPauseRenderer(PauseSubState* parent)
    : subState(parent)
{
}

PixelPauseRenderer::~PixelPauseRenderer() {
    ndspChnSetPaused(NES_AUDIO_CHANNEL, true);
    ndspChnReset(NES_AUDIO_CHANNEL);
    if (pauseTextBuf) {
        C2D_TextBufDelete(pauseTextBuf);
        pauseTextBuf = nullptr;
    }
    if (infoTextBuf) {
        C2D_TextBufDelete(infoTextBuf);
        infoTextBuf = nullptr;
    }
}

void PixelPauseRenderer::init() {
    nesBlinkTimer = 0.0f;
    nesOpenTimer = 0.0f;
    nesCursorY = 0.0f;

    if (!pauseTextBuf) {
        pauseTextBuf = C2D_TextBufNew(1024);
    }
    if (!infoTextBuf) {
        infoTextBuf = C2D_TextBufNew(256);
    }

    initNesAudio();
    setupMenuTexts(subState->pauseMenuItems, "PAUSED");
    setupInfoTexts();
    playNesSound(NES_SFX_OPEN);
}

void PixelPauseRenderer::setupMenuTexts(const std::vector<std::string>& items, const std::string& title) {
    C2D_Font fontToUse = globalPixelFont ? globalPixelFont : globalVCRFont;
    if (!pauseTextBuf || !fontToUse) return;

    C2D_TextBufClear(pauseTextBuf);
    pauseOptionObjs.clear();
    optionWidths.clear();
    optionHeights.clear();

    std::string displayTitle = title;
    for (char& c : displayTitle) c = (char)toupper((unsigned char)c);

    C2D_TextFontParse(&pauseTitleObj, fontToUse, pauseTextBuf, displayTitle.c_str());
    C2D_TextOptimize(&pauseTitleObj);

    float fontScale = 0.50f;
    C2D_TextGetDimensions(&pauseTitleObj, fontScale, fontScale, &titleWidth, &titleHeight);
    maxOptionWidth = titleWidth;

    pauseOptionObjs.resize(items.size());
    optionWidths.resize(items.size(), 0.0f);
    optionHeights.resize(items.size(), 0.0f);

    for (size_t i = 0; i < items.size(); i++) {
        std::string itemText = items[i];
        for (char& c : itemText) c = (char)toupper((unsigned char)c);

        C2D_TextFontParse(&pauseOptionObjs[i], fontToUse, pauseTextBuf, itemText.c_str());
        C2D_TextOptimize(&pauseOptionObjs[i]);
        C2D_TextGetDimensions(&pauseOptionObjs[i], fontScale, fontScale, &optionWidths[i], &optionHeights[i]);
        if (optionWidths[i] > maxOptionWidth) {
            maxOptionWidth = optionWidths[i];
        }
    }
}

void PixelPauseRenderer::setupInfoTexts() {
    C2D_Font fontToUse = globalPixelFont ? globalPixelFont : globalVCRFont;
    if (!infoTextBuf || !fontToUse) return;
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

    std::string row0 = songName;
    std::string row1 = "Difficulty: " + diffName;
    std::string row2 = std::to_string(deaths) + " Blue Balls";

    for (char& c : row0) c = (char)toupper((unsigned char)c);
    for (char& c : row1) c = (char)toupper((unsigned char)c);
    for (char& c : row2) c = (char)toupper((unsigned char)c);

    std::string rows[3] = {row0, row1, row2};
    float fontScale = 0.50f;
    float lineSpacing = 6.0f;
    infoTotalHeight = 0.0f;
    maxInfoWidth = 0.0f;

    for (int i = 0; i < 3; i++) {
        C2D_TextFontParse(&infoTextObj[i], fontToUse, infoTextBuf, rows[i].c_str());
        C2D_TextOptimize(&infoTextObj[i]);
        C2D_TextGetDimensions(&infoTextObj[i], fontScale, fontScale, &infoWidths[i], &infoHeights[i]);
        if (infoWidths[i] > maxInfoWidth) maxInfoWidth = infoWidths[i];
        infoTotalHeight += infoHeights[i];
        if (i < 2) infoTotalHeight += lineSpacing;
    }
}

void PixelPauseRenderer::onMenuChanged(const std::vector<std::string>& items, const std::string& title) {
    nesOpenTimer = 0.0f;
    setupMenuTexts(items, title);
    playNesSound(NES_SFX_OPEN);
}

void PixelPauseRenderer::update(float dt) {
    nesBlinkTimer += dt;
    if (nesOpenTimer < 0.32f) {
        nesOpenTimer += dt;
        if (nesOpenTimer > 0.32f) nesOpenTimer = 0.32f;
    }

    float targetCursorY = (float)subState->pauseSelection;
    nesCursorY += (targetCursorY - nesCursorY) * (dt * 22.0f);
}

void PixelPauseRenderer::onCursorMove() {
    playNesSound(NES_SFX_CURSOR);
}

void PixelPauseRenderer::onSelect() {
    playNesSound(NES_SFX_SELECT);
}

void PixelPauseRenderer::onCancel() {
    playNesSound(NES_SFX_CANCEL);
}

void PixelPauseRenderer::drawTop(C3D_RenderTarget* target, float screenW, float screenH) {
    (void)target;
    C2D_DrawRectSolid(0, 0, 0.99f, screenW, screenH, C2D_Color32(0, 0, 0, 160));

    int itemCount = (int)subState->pauseMenuItems.size();
    float fontScale = 0.50f;

    float titleBoxW = floorf(titleWidth + 20.0f);
    float titleBoxH = floorf(titleHeight + 8.0f);

    float selectorSquareSize = 8.0f;
    float selectorGap = 6.0f;
    float optionsBoxW = floorf(maxOptionWidth + selectorSquareSize + selectorGap + 20.0f);
    if (optionsBoxW < 140.0f) optionsBoxW = 140.0f;

    float itemLineH = floorf(titleHeight + 5.0f);
    float optionsBoxH = floorf(8.0f + (itemCount * itemLineH) + 8.0f);

    float gapBetweenBoxes = 8.0f;
    float totalH = titleBoxH + gapBetweenBoxes + optionsBoxH;

    float startY = floorf((screenH - totalH) / 2.0f);
    float titleBoxY = startY;
    float titleBoxX = floorf((screenW - titleBoxW) / 2.0f);

    float optionsBoxY = titleBoxY + titleBoxH + gapBetweenBoxes;
    float optionsBoxX = floorf((screenW - optionsBoxW) / 2.0f);

    float animDuration = 0.32f;
    float rawT = nesOpenTimer / animDuration;
    if (rawT > 1.0f) rawT = 1.0f;

    float t = rawT - 1.0f;
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    float easeBack = (rawT >= 1.0f) ? 1.0f : (1.0f + c3 * t * t * t + c1 * t * t);

    float stepProgress = (rawT >= 1.0f) ? 1.0f : (floorf(easeBack * 16.0f) / 16.0f);
    if (stepProgress < 0.05f) stepProgress = 0.05f;

    float curTitleW = titleBoxW;
    float curTitleH = floorf(titleBoxH * stepProgress);
    float curTitleX = titleBoxX;
    float curTitleY = floorf(titleBoxY + (titleBoxH - curTitleH) / 2.0f);

    float curOptionsW = optionsBoxW;
    float curOptionsH = floorf(optionsBoxH * stepProgress);
    float curOptionsX = optionsBoxX;
    float curOptionsY = floorf(optionsBoxY + (optionsBoxH - curOptionsH) / 2.0f);

    float zDepth = 0.995f;

    C2D_DrawRectSolid(curTitleX - 2.0f, curTitleY - 2.0f, zDepth, curTitleW + 4.0f, curTitleH + 4.0f, C2D_Color32(255, 255, 255, 255));
    C2D_DrawRectSolid(curTitleX, curTitleY, zDepth + 0.001f, curTitleW, curTitleH, C2D_Color32(0, 0, 0, 255));

    C2D_DrawRectSolid(curOptionsX - 2.0f, curOptionsY - 2.0f, zDepth, curOptionsW + 4.0f, curOptionsH + 4.0f, C2D_Color32(255, 255, 255, 255));
    C2D_DrawRectSolid(curOptionsX, curOptionsY, zDepth + 0.001f, curOptionsW, curOptionsH, C2D_Color32(0, 0, 0, 255));

    if (rawT >= 1.0f) {
        float titleX = floorf(titleBoxX + (titleBoxW - titleWidth) / 2.0f);
        float titleY = floorf(titleBoxY + (titleBoxH - titleHeight) / 2.0f);
        C2D_DrawText(&pauseTitleObj, C2D_WithColor, titleX, titleY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));

        float startItemsY = floorf(optionsBoxY + 8.0f);
        float selectorX = floorf(optionsBoxX + 10.0f);
        float textX = floorf(optionsBoxX + 10.0f + selectorSquareSize + selectorGap);

        bool curSelectVisible = (fmodf(nesBlinkTimer, 0.60f) < 0.35f);

        for (int i = 0; i < itemCount && i < (int)pauseOptionObjs.size(); i++) {
            bool sel = (i == subState->pauseSelection);
            float itemY = floorf(startItemsY + (i * itemLineH));

            if (sel) {
                if (curSelectVisible) {
                    float squareY = floorf(itemY + (titleHeight - selectorSquareSize) / 2.0f);
                    C2D_DrawRectSolid(selectorX, squareY, zDepth + 0.002f, selectorSquareSize, selectorSquareSize, C2D_Color32(255, 255, 255, 255));
                    C2D_DrawText(&pauseOptionObjs[i], C2D_WithColor, textX, itemY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));
                }
            } else {
                C2D_DrawText(&pauseOptionObjs[i], C2D_WithColor, textX, itemY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));
            }
        }
    }
}

void PixelPauseRenderer::drawBottom(C3D_RenderTarget* target, float screenW, float screenH) {
    (void)target;
    C2D_DrawRectSolid(0, 0, 0.99f, screenW, screenH, C2D_Color32(0, 0, 0, 160));

    float fontScale = 0.50f;
    float lineSpacing = 6.0f;

    float boxW = floorf(maxInfoWidth + 24.0f);
    if (boxW < 140.0f) boxW = 140.0f;
    float boxH = floorf(infoTotalHeight + 16.0f);

    float animDuration = 0.32f;
    float rawT = nesOpenTimer / animDuration;
    if (rawT > 1.0f) rawT = 1.0f;

    float t = rawT - 1.0f;
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    float easeBack = (rawT >= 1.0f) ? 1.0f : (1.0f + c3 * t * t * t + c1 * t * t);

    float stepProgress = (rawT >= 1.0f) ? 1.0f : (floorf(easeBack * 16.0f) / 16.0f);
    if (stepProgress < 0.05f) stepProgress = 0.05f;

    float curBoxW = boxW;
    float curBoxH = floorf(boxH * stepProgress);
    float curBoxX = floorf((screenW - curBoxW) / 2.0f);
    float curBoxY = floorf((screenH - curBoxH) / 2.0f);

    float zDepth = 0.995f;
    C2D_DrawRectSolid(curBoxX - 2.0f, curBoxY - 2.0f, zDepth, curBoxW + 4.0f, curBoxH + 4.0f, C2D_Color32(255, 255, 255, 255));
    C2D_DrawRectSolid(curBoxX, curBoxY, zDepth + 0.001f, curBoxW, curBoxH, C2D_Color32(0, 0, 0, 255));

    if (rawT >= 1.0f) {
        float startY = floorf((screenH - infoTotalHeight) / 2.0f);
        float currentY = startY;

        for (int i = 0; i < 3; i++) {
            float x = floorf((screenW - infoWidths[i]) / 2.0f);
            C2D_DrawText(&infoTextObj[i], C2D_WithColor, x, currentY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));
            currentY += infoHeights[i] + lineSpacing;
        }
    }
}

void PixelPauseRenderer::initNesAudio() {
    if (s_nesSfxBuffers[NES_SFX_OPEN] != nullptr) return;

    // 1. OPEN SOUND
    {
        uint32_t samples = 5292;
        s_nesSfxSampleCounts[NES_SFX_OPEN] = samples;
        int16_t* buf = (int16_t*)linearMemAlign(samples * sizeof(int16_t), 0x80);
        if (buf) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < samples; i++) {
                float timeSec = (float)i / 44100.0f;
                float freq = 523.25f; // C5
                float duty = 0.25f;
                if (timeSec >= 0.08f) {
                    freq = 783.99f; // G5
                    duty = 0.50f;
                } else if (timeSec >= 0.04f) {
                    freq = 659.25f; // E5
                    duty = 0.25f;
                }
                phase += freq / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                float env = 1.0f - ((float)i / (float)samples);
                int16_t val = (phase < duty) ? (int16_t)(20000.0f * env) : (int16_t)(-20000.0f * env);
                buf[i] = val;
            }
            DSP_FlushDataCache(buf, samples * sizeof(int16_t));
            s_nesSfxBuffers[NES_SFX_OPEN] = buf;
        }
    }

    // 2. CURSOR SOUND
    {
        uint32_t samples = 1543;
        s_nesSfxSampleCounts[NES_SFX_CURSOR] = samples;
        int16_t* buf = (int16_t*)linearMemAlign(samples * sizeof(int16_t), 0x80);
        if (buf) {
            float currentPhase = 0.0f;
            for (uint32_t i = 0; i < samples; i++) {
                float progress = (float)i / (float)samples;
                float freq = 280.0f - (140.0f * progress);
                currentPhase += freq / 44100.0f;
                if (currentPhase >= 1.0f) currentPhase -= floorf(currentPhase);
                float env = (1.0f - progress) * (1.0f - progress);
                int16_t val = (currentPhase < 0.50f) ? (int16_t)(18000.0f * env) : (int16_t)(-18000.0f * env);
                buf[i] = val;
            }
            DSP_FlushDataCache(buf, samples * sizeof(int16_t));
            s_nesSfxBuffers[NES_SFX_CURSOR] = buf;
        }
    }

    // 3. SELECT SOUND
    {
        uint32_t samples = 6174;
        s_nesSfxSampleCounts[NES_SFX_SELECT] = samples;
        int16_t* buf = (int16_t*)linearMemAlign(samples * sizeof(int16_t), 0x80);
        if (buf) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < samples; i++) {
                float timeSec = (float)i / 44100.0f;
                float freq = 587.33f; // D5
                float duty = 0.25f;
                if (timeSec >= 0.07f) {
                    freq = 1174.66f; // D6
                    duty = 0.50f;
                } else if (timeSec >= 0.035f) {
                    freq = 880.00f; // A5
                    duty = 0.25f;
                }
                phase += freq / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                float env = 1.0f - ((float)i / (float)samples);
                int16_t val = (phase < duty) ? (int16_t)(22000.0f * env) : (int16_t)(-22000.0f * env);
                buf[i] = val;
            }
            DSP_FlushDataCache(buf, samples * sizeof(int16_t));
            s_nesSfxBuffers[NES_SFX_SELECT] = buf;
        }
    }

    // 4. CANCEL SOUND
    {
        uint32_t samples = 4410;
        s_nesSfxSampleCounts[NES_SFX_CANCEL] = samples;
        int16_t* buf = (int16_t*)linearMemAlign(samples * sizeof(int16_t), 0x80);
        if (buf) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < samples; i++) {
                float timeSec = (float)i / 44100.0f;
                float freq = (timeSec < 0.04f) ? 440.00f : 220.00f;
                float duty = 0.125f;
                phase += freq / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                float env = 1.0f - ((float)i / (float)samples);
                int16_t val = (phase < duty) ? (int16_t)(18000.0f * env) : (int16_t)(-18000.0f * env);
                buf[i] = val;
            }
            DSP_FlushDataCache(buf, samples * sizeof(int16_t));
            s_nesSfxBuffers[NES_SFX_CANCEL] = buf;
        }
    }
}

void PixelPauseRenderer::freeAudio() {
    ndspChnSetPaused(NES_AUDIO_CHANNEL, true);
    ndspChnReset(NES_AUDIO_CHANNEL);
    for (int i = 0; i < NES_SFX_COUNT; i++) {
        if (s_nesSfxBuffers[i]) {
            linearFree(s_nesSfxBuffers[i]);
            s_nesSfxBuffers[i] = nullptr;
        }
        s_nesSfxSampleCounts[i] = 0;
    }
}

void PixelPauseRenderer::playNesSound(NesSfx sound) {
    if (!PlayState::instance) return;
    if (sound < 0 || sound >= NES_SFX_COUNT) return;
    if (!s_nesSfxBuffers[sound] || s_nesSfxSampleCounts[sound] == 0) return;

    ndspChnSetPaused(NES_AUDIO_CHANNEL, true);
    ndspChnReset(NES_AUDIO_CHANNEL);
    ndspChnSetInterp(NES_AUDIO_CHANNEL, NDSP_INTERP_NONE);
    ndspChnSetRate(NES_AUDIO_CHANNEL, 44100.0f);
    ndspChnSetFormat(NES_AUDIO_CHANNEL, NDSP_FORMAT_MONO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 0.8f;
    mix[1] = 0.8f;
    ndspChnSetMix(NES_AUDIO_CHANNEL, mix);

    memset(&s_nesWaveBuf, 0, sizeof(ndspWaveBuf));
    s_nesWaveBuf.data_vaddr = s_nesSfxBuffers[sound];
    s_nesWaveBuf.nsamples = s_nesSfxSampleCounts[sound];
    s_nesWaveBuf.looping = false;
    s_nesWaveBuf.status = NDSP_WBUF_FREE;

    ndspChnWaveBufAdd(NES_AUDIO_CHANNEL, &s_nesWaveBuf);
}
