#include "PauseSubState.hpp"
#include "MemoryDebugState.hpp"
#include <malloc.h>
#include "../states/PlayState.hpp"
#include "../options/OptionsMenuState.hpp"
#include "../states/StoryMenuState.hpp"
#include "../states/FreeplayState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/Alphabet.hpp"
#include "../objects/InGameVideoPlayer.hpp"
#include "../backend/Macros.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
extern C2D_Font globalVCRFont;
extern C2D_Font globalPixelFont;

std::vector<std::string> PauseSubState::buildMainItems() {
    std::vector<std::string> mainItems = {"Resume", "Restart Song"};
    if (PlayState::instance && PlayState::instance->curSongDifficulties.size() >= 2) {
        mainItems.push_back("Change Difficulty");
    }
    if (ClientPrefs::debugInfo) {
        mainItems.push_back("Memory Debug");
    }
    mainItems.push_back("Options");
    mainItems.push_back("Exit to menu");
    return mainItems;
}

PauseSubState::PauseSubState() {
    pauseTextBuf = C2D_TextBufNew(1024);
    setupPauseMenu(buildMainItems(), "PAUSED");

    if (PlayState::instance && PlayState::instance->isPixelStage) {
        initNesAudio();
        playNesSound(NES_SFX_OPEN);
    }
}

PauseSubState::~PauseSubState() {
    freeNesAudio();
    if (pauseTextBuf) {
        C2D_TextBufDelete(pauseTextBuf);
        pauseTextBuf = nullptr;
    }
    if (memoryDebugState) {
        delete memoryDebugState;
        memoryDebugState = nullptr;
    }
}

void PauseSubState::setupPauseMenu(const std::vector<std::string>& items, const std::string& title) {
    bool isPixel = (PlayState::instance && PlayState::instance->isPixelStage);
    C2D_Font fontToUse = (isPixel && globalPixelFont) ? globalPixelFont : globalVCRFont;
    if (!fontToUse) fontToUse = globalVCRFont;
    if (!pauseTextBuf || !fontToUse) return;

    pauseMenuItems = items;
    C2D_TextBufClear(pauseTextBuf);

    std::string displayTitle = title;
    if (isPixel) {
        for (char& c : displayTitle) c = (char)toupper((unsigned char)c);
    }

    C2D_TextFontParse(&pauseTitleObj, fontToUse, pauseTextBuf, displayTitle.c_str());
    C2D_TextOptimize(&pauseTitleObj);

    float fontScale = 0.50f;
    C2D_TextGetDimensions(&pauseTitleObj, fontScale, fontScale, &titleWidth, &titleHeight);
    maxOptionWidth = titleWidth;

    for (size_t i = 0; i < pauseMenuItems.size() && i < 15; i++) {
        std::string itemText = pauseMenuItems[i];
        if (isPixel) {
            for (char& c : itemText) c = (char)toupper((unsigned char)c);
        }
        C2D_TextFontParse(&pauseOptionsObj[i], fontToUse, pauseTextBuf, itemText.c_str());
        C2D_TextOptimize(&pauseOptionsObj[i]);

        C2D_TextGetDimensions(&pauseOptionsObj[i], fontScale, fontScale, &optionWidths[i], &optionHeights[i]);
        if (optionWidths[i] > maxOptionWidth) {
            maxOptionWidth = optionWidths[i];
        }
    }
}

void PauseSubState::update(float dt) {
    if (memoryDebugState) {
        memoryDebugState->update(dt);
        if (!memoryDebugState->active) {
            delete memoryDebugState;
            memoryDebugState = nullptr;
        }
        return;
    }

    nesBlinkTimer += dt;
    if (nesOpenTimer < 0.32f) {
        nesOpenTimer += dt;
        if (nesOpenTimer > 0.32f) nesOpenTimer = 0.32f;
    }

    // Smooth NES cursor Y motion tween
    float targetCursorY = (float)pauseSelection;
    nesCursorY += (targetCursorY - nesCursorY) * (dt * 22.0f);

    u32 kDown = hidKeysDown();
    
    pauseLerpSelection += (pauseSelection - pauseLerpSelection) * (dt * 15.0f);
    bool isPixel = (PlayState::instance && PlayState::instance->isPixelStage);
    int maxSel = (int)pauseMenuItems.size() - 1;
    if (maxSel < 0) maxSel = 0;
    
    if (kDown & (KEY_DUP | KEY_CPAD_UP)) { 
        pauseSelection--; 
        if (pauseSelection < 0) pauseSelection = maxSel; 
        if (isPixel) playNesSound(NES_SFX_CURSOR);
        else if (ClientPrefs::alphabetPause) AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) { 
        pauseSelection++; 
        if (pauseSelection > maxSel) pauseSelection = 0; 
        if (isPixel) playNesSound(NES_SFX_CURSOR);
        else if (ClientPrefs::alphabetPause) AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    
    if (kDown & KEY_B) {
        if (isPixel) playNesSound(NES_SFX_CANCEL);
        if (pauseMenuState == PAUSE_DIFFICULTY) {
            pauseMenuState = PAUSE_MAIN;
            setupPauseMenu(buildMainItems(), "PAUSED");
            pauseSelection = 0;
            pauseLerpSelection = 0.0f;
            nesOpenTimer = 0.0f;
            if (isPixel) playNesSound(NES_SFX_OPEN);
        } else {
            PlayState::instance->paused = false;
            AudioEngine::resume();
            if (PlayState::instance->inGameVideo) PlayState::instance->inGameVideo->isPaused = false;
        }
        return;
    }
    
    if (kDown & KEY_A) {
        if (isPixel) playNesSound(NES_SFX_SELECT);
        if (pauseMenuState == PAUSE_MAIN) {
            std::string sel = pauseMenuItems[pauseSelection];
            if (sel == "Resume") {
                PlayState::instance->paused = false;
                AudioEngine::resume();
                if (PlayState::instance->inGameVideo) PlayState::instance->inGameVideo->isPaused = false;
            } else if (sel == "Restart Song") {
                if (PlayState::instance->isStoryMode) {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->weekData, PlayState::instance->curSongIdx, PlayState::instance->currentDifficulty));
                } else {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, PlayState::instance->currentDifficulty));
                }
            } else if (sel == "Change Difficulty") {
                pauseMenuState = PAUSE_DIFFICULTY;
                std::vector<std::string> diffItems = PlayState::instance->curSongDifficulties;
                diffItems.push_back("BACK");
                setupPauseMenu(diffItems, "CHANGE DIFFICULTY");
                pauseSelection = 0;
                pauseLerpSelection = 0.0f;
                nesOpenTimer = 0.0f;
                if (isPixel) playNesSound(NES_SFX_OPEN);
            } else if (sel == "Options") {
                OptionsMenuState::onPlayState = true;
                OptionsMenuState::isStoryMode = PlayState::instance->isStoryMode;
                OptionsMenuState::songName = PlayState::instance->curSong;
                OptionsMenuState::difficultyName = PlayState::instance->currentDifficulty;
                if (PlayState::instance->isStoryMode) {
                    OptionsMenuState::storyWeek = PlayState::instance->weekData;
                    OptionsMenuState::storySongIdx = PlayState::instance->curSongIdx;
                }
                MusicBeatState::switchState(new OptionsMenuState());
            } else if (sel == "Memory Debug") {
                memoryDebugState = new MemoryDebugState();
            } else if (sel == "Exit to menu") {
                if (PlayState::instance->isStoryMode) {
                    MusicBeatState::switchState(new StoryMenuState());
                } else {
                    MusicBeatState::switchState(new FreeplayState());
                }
            }
        } else if (pauseMenuState == PAUSE_DIFFICULTY) {
            std::string sel = pauseMenuItems[pauseSelection];
            if (sel == "BACK" || pauseSelection == (int)pauseMenuItems.size() - 1) {
                pauseMenuState = PAUSE_MAIN;
                setupPauseMenu(buildMainItems(), "PAUSED");
                pauseSelection = 0;
                pauseLerpSelection = 0.0f;
                nesOpenTimer = 0.0f;
                if (isPixel) playNesSound(NES_SFX_OPEN);
            } else {
                if (PlayState::instance->isStoryMode) {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->weekData, PlayState::instance->curSongIdx, sel));
                } else {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, sel));
                }
            }
        }
    }
}

void PauseSubState::draw() {
    if (memoryDebugState) {
        memoryDebugState->draw();
        return;
    }

    C2D_SceneBegin(PlayState::instance->top);
    float bw = (float)ScreenWidthTop;
    float bh = (float)ScreenHeight;
    
    if (PlayState::instance && PlayState::instance->isPixelStage) {
        // Dark translucent overlay on top screen
        C2D_DrawRectSolid(0, 0, 0.99f, bw, bh, C2D_Color32(0, 0, 0, 160));

        int itemCount = (int)pauseMenuItems.size();
        float fontScale = 0.50f;

        // 1. Calculate Title Box Dimensions using cached values
        float titleBoxW = floorf(titleWidth + 20.0f);
        float titleBoxH = floorf(titleHeight + 8.0f);

        // 2. Calculate Options Box Dimensions using cached values
        float selectorSquareSize = 8.0f;
        float selectorGap = 6.0f;
        float optionsBoxW = floorf(maxOptionWidth + selectorSquareSize + selectorGap + 20.0f);
        if (optionsBoxW < 140.0f) optionsBoxW = 140.0f;

        float itemLineH = floorf(titleHeight + 5.0f);
        float optionsBoxH = floorf(8.0f + (itemCount * itemLineH) + 8.0f);

        float gapBetweenBoxes = 8.0f;
        float totalH = titleBoxH + gapBetweenBoxes + optionsBoxH;

        float startY = floorf((bh - totalH) / 2.0f);
        float titleBoxY = startY;
        float titleBoxX = floorf((bw - titleBoxW) / 2.0f);

        float optionsBoxY = titleBoxY + titleBoxH + gapBetweenBoxes;
        float optionsBoxX = floorf((bw - optionsBoxW) / 2.0f);

        // 3. BackOut Stepped Entrance Animation (NES-style expanding vertically from center)
        float animDuration = 0.32f;
        float rawT = nesOpenTimer / animDuration;
        if (rawT > 1.0f) rawT = 1.0f;

        float t = rawT - 1.0f;
        float c1 = 1.70158f;
        float c3 = c1 + 1.0f;
        float easeBack = (rawT >= 1.0f) ? 1.0f : (1.0f + c3 * t * t * t + c1 * t * t);
        
        // Quantize progress to 16 discrete 8-bit NES frames for a retro stepped animation
        float stepProgress = (rawT >= 1.0f) ? 1.0f : (floorf(easeBack * 16.0f) / 16.0f);
        if (stepProgress < 0.05f) stepProgress = 0.05f;

        // Expand boxes vertically ONLY from center (width remains constant)
        float curTitleW = titleBoxW;
        float curTitleH = floorf(titleBoxH * stepProgress);
        float curTitleX = titleBoxX;
        float curTitleY = floorf(titleBoxY + (titleBoxH - curTitleH) / 2.0f);

        float curOptionsW = optionsBoxW;
        float curOptionsH = floorf(optionsBoxH * stepProgress);
        float curOptionsX = optionsBoxX;
        float curOptionsY = floorf(optionsBoxY + (optionsBoxH - curOptionsH) / 2.0f);

        float zDepth = 0.995f;

        // Draw Title Box: 2px Solid White Outer Border + Solid Black Inner Rectangle
        C2D_DrawRectSolid(curTitleX - 2.0f, curTitleY - 2.0f, zDepth, curTitleW + 4.0f, curTitleH + 4.0f, C2D_Color32(255, 255, 255, 255));
        C2D_DrawRectSolid(curTitleX, curTitleY, zDepth + 0.001f, curTitleW, curTitleH, C2D_Color32(0, 0, 0, 255));

        // Draw Options Box: 2px Solid White Outer Border + Solid Black Inner Rectangle
        C2D_DrawRectSolid(curOptionsX - 2.0f, curOptionsY - 2.0f, zDepth, curOptionsW + 4.0f, curOptionsH + 4.0f, C2D_Color32(255, 255, 255, 255));
        C2D_DrawRectSolid(curOptionsX, curOptionsY, zDepth + 0.001f, curOptionsW, curOptionsH, C2D_Color32(0, 0, 0, 255));

        // 4. Draw Items and Selector ONLY when entrance animation finishes (rawT >= 1.0f)
        if (rawT >= 1.0f) {
            // Draw Title Text (centered inside title box)
            float titleX = floorf(titleBoxX + (titleBoxW - titleWidth) / 2.0f);
            float titleY = floorf(titleBoxY + (titleBoxH - titleHeight) / 2.0f);
            C2D_DrawText(&pauseTitleObj, C2D_WithColor, titleX, titleY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));

            float startItemsY = floorf(optionsBoxY + 8.0f);
            float selectorX = floorf(optionsBoxX + 10.0f);
            float textX = floorf(optionsBoxX + 10.0f + selectorSquareSize + selectorGap);

            // Slow NES visibility blinking for curSelect (visibility toggles ON/OFF slowly every 0.60s)
            bool curSelectVisible = (fmodf(nesBlinkTimer, 0.60f) < 0.35f);

            for (int i = 0; i < itemCount && i < 15; i++) {
                bool sel = (i == pauseSelection);
                float itemY = floorf(startItemsY + (i * itemLineH));

                if (sel) {
                    if (curSelectVisible) {
                        // Solid White Square Selector (■)
                        float squareY = floorf(itemY + (titleHeight - selectorSquareSize) / 2.0f);
                        C2D_DrawRectSolid(selectorX, squareY, zDepth + 0.002f, selectorSquareSize, selectorSquareSize, C2D_Color32(255, 255, 255, 255));

                        // White Option Text
                        C2D_DrawText(&pauseOptionsObj[i], C2D_WithColor, textX, itemY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));
                    }
                } else {
                    // Unselected options are ALWAYS visible white text
                    C2D_DrawText(&pauseOptionsObj[i], C2D_WithColor, textX, itemY, zDepth + 0.002f, fontScale, fontScale, C2D_Color32(255, 255, 255, 255));
                }
            }
        }

        return;
    }

    C2D_DrawRectSolid(0, 0, 0.99f, bw, bh, C2D_Color32(0, 0, 0, 150));
    
    if (ClientPrefs::alphabetPause) {
        for (int i = 0; i < (int)pauseMenuItems.size(); i++) {
            bool sel = (i == pauseSelection);
            
            float targetY = (bh / 2.0f) - (35.0f / 2.0f) + (i - pauseLerpSelection) * 60.0f;
            float targetX = 10.0f + (i - pauseLerpSelection) * 10.0f;
            
            float scale = 1.5f;
            float alpha = sel ? 1.0f : 0.6f;
            Alphabet::draw(pauseMenuItems[i], targetX, targetY, scale, alpha, false, 0xFFFFFFFF, 1.0f);
        }
    } else {
        for (int i = 0; i < (int)pauseMenuItems.size() && i < 15; i++) {
            bool sel = (i == pauseSelection);
            
            float targetY = (bh / 2.0f) - (35.0f / 2.0f) + (i - pauseLerpSelection) * 60.0f;
            float targetX = 10.0f + (i - pauseLerpSelection) * 20.0f;
            
            float fScale = sel ? 0.85f : 0.6f;
            u32 color = sel ? CWhite : C2D_Color32(160, 160, 160, 200);
            
            DrawTextBorderFull(&pauseOptionsObj[i], targetX, targetY, 0.995f, fScale, fScale, 2.0f, CBlack);
            C2D_DrawText(&pauseOptionsObj[i], C2D_WithColor, targetX, targetY, 1.0f, fScale, fScale, color);
        }
    }
}

void PauseSubState::initNesAudio() {
    if (nesSfxBuffers[NES_SFX_OPEN] != nullptr) return;

    // 1. OPEN SOUND: 3-note ascending NES chord (C5 -> E5 -> G5) ~120ms
    {
        uint32_t samples = 5292;
        nesSfxSampleCounts[NES_SFX_OPEN] = samples;
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
            nesSfxBuffers[NES_SFX_OPEN] = buf;
        }
    }

    // 2. CURSOR SOUND: Quick NES blip frequency sweep (280Hz -> 140Hz) ~35ms
    {
        uint32_t samples = 1543;
        nesSfxSampleCounts[NES_SFX_CURSOR] = samples;
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
            nesSfxBuffers[NES_SFX_CURSOR] = buf;
        }
    }

    // 3. SELECT SOUND: Retro NES confirm arpeggio (D5 -> A5 -> D6) ~140ms
    {
        uint32_t samples = 6174;
        nesSfxSampleCounts[NES_SFX_SELECT] = samples;
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
            nesSfxBuffers[NES_SFX_SELECT] = buf;
        }
    }

    // 4. CANCEL SOUND: Retro NES back tone (A4 -> A3) ~100ms
    {
        uint32_t samples = 4410;
        nesSfxSampleCounts[NES_SFX_CANCEL] = samples;
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
            nesSfxBuffers[NES_SFX_CANCEL] = buf;
        }
    }
}

void PauseSubState::freeNesAudio() {
    ndspChnReset(5);
    for (int i = 0; i < NES_SFX_COUNT; i++) {
        if (nesSfxBuffers[i]) {
            linearFree(nesSfxBuffers[i]);
            nesSfxBuffers[i] = nullptr;
        }
        nesSfxSampleCounts[i] = 0;
    }
}

void PauseSubState::playNesSound(NesSfx sound) {
    if (!PlayState::instance || !PlayState::instance->isPixelStage) return;
    if (sound < 0 || sound >= NES_SFX_COUNT) return;
    if (!nesSfxBuffers[sound] || nesSfxSampleCounts[sound] == 0) return;

    ndspChnReset(5);
    ndspChnSetInterp(5, NDSP_INTERP_NONE);
    ndspChnSetRate(5, 44100);
    ndspChnSetFormat(5, NDSP_FORMAT_MONO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 0.8f;
    mix[1] = 0.8f;
    ndspChnSetMix(5, mix);

    memset(&nesWaveBuf, 0, sizeof(ndspWaveBuf));
    nesWaveBuf.data_vaddr = nesSfxBuffers[sound];
    nesWaveBuf.nsamples = nesSfxSampleCounts[sound];
    nesWaveBuf.looping = false;
    nesWaveBuf.status = NDSP_WBUF_FREE;

    ndspChnWaveBufAdd(5, &nesWaveBuf);
}
