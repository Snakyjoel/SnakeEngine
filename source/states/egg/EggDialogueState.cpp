#include "EggDialogueState.hpp"
#include "EggDuelState.hpp"
#include "TitleState.hpp"
#include "backend/savedata/ClientPrefs.hpp"
#include <cmath>
#include <algorithm>

extern C2D_Font globalVCRFont;
extern C2D_Font globalPixelFont;

static int16_t* allocAudioBuffer(uint32_t samples) {
    int16_t* buf = (int16_t*)linearMemAlign(samples * sizeof(int16_t), 0x80);
    return buf;
}

EggDialogueState::EggDialogueState() {}

EggDialogueState::~EggDialogueState() {
    freeAudio();
    if (topTextBuf) {
        C2D_TextBufDelete(topTextBuf);
        topTextBuf = nullptr;
    }
    if (bottomTextBuf) {
        C2D_TextBufDelete(bottomTextBuf);
        bottomTextBuf = nullptr;
    }
}

void EggDialogueState::initAudio() {
    // 0: Voice blip (short 165Hz square pulse)
    {
        uint32_t len = 1100;
        sfxLengths[(int)SoundEffect::VOICE_BLEEP] = len;
        int16_t* b = allocAudioBuffer(len);
        if (b) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < len; i++) {
                float env = 1.0f - ((float)i / (float)len);
                phase += 165.0f / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                b[i] = (phase < 0.25f) ? (int16_t)(14000.0f * env) : (int16_t)(-14000.0f * env);
            }
            DSP_FlushDataCache(b, len * sizeof(int16_t));
            sfxBuffers[(int)SoundEffect::VOICE_BLEEP] = b;
        }
    }

    // 1: Menu cursor move (downward chirp)
    {
        uint32_t len = 1500;
        sfxLengths[(int)SoundEffect::CURSOR] = len;
        int16_t* b = allocAudioBuffer(len);
        if (b) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < len; i++) {
                float prog = (float)i / (float)len;
                float env = (1.0f - prog) * (1.0f - prog);
                phase += (280.0f - 140.0f * prog) / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                b[i] = (phase < 0.50f) ? (int16_t)(16000.0f * env) : (int16_t)(-16000.0f * env);
            }
            DSP_FlushDataCache(b, len * sizeof(int16_t));
            sfxBuffers[(int)SoundEffect::CURSOR] = b;
        }
    }

    // 2: Confirm selection (two-tone chime: C5 -> G5)
    {
        uint32_t len = 4200;
        sfxLengths[(int)SoundEffect::CONFIRM] = len;
        int16_t* b = allocAudioBuffer(len);
        if (b) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < len; i++) {
                float t = (float)i / 44100.0f;
                float freq = (t < 0.045f) ? 523.25f : 783.99f;
                float env = 1.0f - ((float)i / (float)len);
                phase += freq / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                b[i] = (phase < 0.25f) ? (int16_t)(18000.0f * env) : (int16_t)(-18000.0f * env);
            }
            DSP_FlushDataCache(b, len * sizeof(int16_t));
            sfxBuffers[(int)SoundEffect::CONFIRM] = b;
        }
    }

    // 3: Advance line chirp
    {
        uint32_t len = 900;
        sfxLengths[(int)SoundEffect::ADVANCE] = len;
        int16_t* b = allocAudioBuffer(len);
        if (b) {
            float phase = 0.0f;
            for (uint32_t i = 0; i < len; i++) {
                float prog = (float)i / (float)len;
                float env = 1.0f - prog;
                phase += (440.0f - 220.0f * prog) / 44100.0f;
                if (phase >= 1.0f) phase -= floorf(phase);
                b[i] = (phase < 0.50f) ? (int16_t)(14000.0f * env) : (int16_t)(-14000.0f * env);
            }
            DSP_FlushDataCache(b, len * sizeof(int16_t));
            sfxBuffers[(int)SoundEffect::ADVANCE] = b;
        }
    }

    // 4: White screen swell for battle transition (~2.5s crescendo)
    {
        uint32_t len = 110250;
        sfxLengths[(int)SoundEffect::WHITE_SWELL] = len;
        int16_t* b = allocAudioBuffer(len);
        if (b) {
            float p1 = 0.0f, p2 = 0.0f, p3 = 0.0f;
            for (uint32_t i = 0; i < len; i++) {
                float prog = (float)i / (float)len;
                float freq = 120.0f * powf(4.333f, prog);

                p1 += freq / 44100.0f;
                p2 += (freq * 1.501f) / 44100.0f;
                p3 += (freq * 2.0f) / 44100.0f;
                if (p1 >= 1.0f) p1 -= floorf(p1);
                if (p2 >= 1.0f) p2 -= floorf(p2);
                if (p3 >= 1.0f) p3 -= floorf(p3);

                float mixed = (sinf(p1 * 6.2831853f) +
                               sinf(p2 * 6.2831853f) * 0.45f +
                               sinf(p3 * 6.2831853f) * 0.35f) / 1.8f;
                float env = powf(prog, 1.6f);
                b[i] = (int16_t)(mixed * env * 27000.0f);
            }
            DSP_FlushDataCache(b, len * sizeof(int16_t));
            sfxBuffers[(int)SoundEffect::WHITE_SWELL] = b;
        }
    }
}

void EggDialogueState::freeAudio() {
    ndspChnSetPaused(SFX_CHANNEL, true);
    ndspChnReset(SFX_CHANNEL);
    svcSleepThread(16000000LL);

    for (int i = 0; i < SFX_TOTAL; i++) {
        if (sfxBuffers[i]) {
            linearFree(sfxBuffers[i]);
            sfxBuffers[i] = nullptr;
        }
        sfxLengths[i] = 0;
    }
}

void EggDialogueState::playSound(SoundEffect sfx) {
    int idx = (int)sfx;
    if (idx < 0 || idx >= SFX_TOTAL || !sfxBuffers[idx] || sfxLengths[idx] == 0) return;

    ndspChnSetPaused(SFX_CHANNEL, true);
    ndspChnReset(SFX_CHANNEL);
    ndspChnSetInterp(SFX_CHANNEL, NDSP_INTERP_NONE);
    ndspChnSetRate(SFX_CHANNEL, 44100);
    ndspChnSetFormat(SFX_CHANNEL, NDSP_FORMAT_MONO_PCM16);

    float mix[12] = { 0 };
    mix[0] = 0.75f;
    mix[1] = 0.75f;
    ndspChnSetMix(SFX_CHANNEL, mix);

    memset(&sfxWave, 0, sizeof(ndspWaveBuf));
    sfxWave.data_vaddr = sfxBuffers[idx];
    sfxWave.nsamples = sfxLengths[idx];
    sfxWave.looping = false;
    sfxWave.status = NDSP_WBUF_FREE;

    ndspChnWaveBufAdd(SFX_CHANNEL, &sfxWave);
}

void EggDialogueState::init() {
    MusicBeatState::transPhase = TransitionPhase::NONE;
    MusicBeatState::skipTransition = true;

    initAudio();

    topTextBuf = C2D_TextBufNew(1024);
    bottomTextBuf = C2D_TextBufNew(512);

    setBranch(Branch::INTRO);
}

void EggDialogueState::setBranch(Branch branch) {
    currentBranch = branch;
    lines.clear();
    lineIndex = 0;
    activeChoice = ChoiceType::NONE;
    choiceIndex = 0;
    bleepCounter = 0;

    switch (branch) {
        case Branch::INTRO:
            lines = {
                "...",
                "... ...",
                "Oh.",
                "Hey there.",
                "You've got that egg,\ndon't you?",
                "The one from\nbehind the tree.",
                "Mind handing it back?",
                "It's... kind of mine,\nyou see."
            };
            break;

        case Branch::CONFIRM_GIVE:
            lines = {
                "Wait, really?",
                "Just like that?",
                "Are you sure?\nNo takesies-backsies."
            };
            break;

        case Branch::GIVEN:
            lines = {
                "Wow. Thanks.",
                "(You handed over the Egg.)",
                "(It feels like nothing\nchanged, yet somehow...\neverything did.)",
                "Take care now."
            };
            break;

        case Branch::REFUSED:
            lines = {
                "...",
                "Oh.",
                "I see.",
                "Holding onto it\npretty tight, huh?",
                "But let's be honest\nfor a second...",
                "It's not like you actually\ncare about a plain old egg.",
                "You just figured that\nif you held onto it\nlong enough...",
                "...something was\nbound to happen.",
                "Weren't you?",
                "Heh.",
                "You have that look.",
                "The look of someone\nalways expecting a little\nsomething extra.",
                "Well...",
                "Far be it from me\nto disappoint someone\nso curious.",
                "Tell you what.",
                "How about a little match?",
                "A duel.",
                "If you can actually beat me,\nyou'll get to see what's\non the other side.",
                "Sound fair?"
            };
            break;

        case Branch::REFUSE_DUEL:
            lines = {
                "Aw. Really?",
                "After carrying it with you\nall this way?",
                "Well, I can't force you.",
                "In that case, I'll just\nbe taking this back now.",
                "(Before you could react,\nthe egg gently slipped\nright out of your hands.)",
                "(You don't have\nthe Egg anymore.)",
                "A shame.\nYou were this close to finding\nwhat you were looking for.",
                "Take care, kiddo."
            };
            break;

        case Branch::ACCEPT_DUEL:
            lines = {
                "Heh. That's the spirit.",
                "Let's see if you're as good\nat keeping up as you are\nat looking for trouble.",
                "Don't blink."
            };
            break;
    }

    if (!lines.empty()) {
        currentText = lines[0];
        charsShown = 0;
        charTimer = 0.0f;
        lineComplete = false;
    }
}

void EggDialogueState::nextLine() {
    playSound(SoundEffect::ADVANCE);

    lineIndex++;
    if (lineIndex < (int)lines.size()) {
        currentText = lines[lineIndex];
        charsShown = 0;
        charTimer = 0.0f;
        lineComplete = false;
        bleepCounter = 0;
    } else {
        switch (currentBranch) {
            case Branch::INTRO:
                activeChoice = ChoiceType::GIVE_OR_KEEP;
                choiceIndex = 0;
                break;

            case Branch::CONFIRM_GIVE:
                activeChoice = ChoiceType::CONFIRM_SURE;
                choiceIndex = 0;
                break;

            case Branch::GIVEN:
                ClientPrefs::acceptedEgg = false;
                ClientPrefs::saveSettings();
                exiting = true;
                exitTimer = 0.0f;
                toDuel = false;
                break;

            case Branch::REFUSED:
                activeChoice = ChoiceType::ACCEPT_OR_REFUSE;
                choiceIndex = 0;
                break;

            case Branch::REFUSE_DUEL:
                ClientPrefs::acceptedEgg = false;
                ClientPrefs::eggInteractionOccurred = true;
                ClientPrefs::saveSettings();
                exiting = true;
                exitTimer = 0.0f;
                toDuel = false;
                break;

            case Branch::ACCEPT_DUEL:
                ClientPrefs::inEggDuel = true;
                ClientPrefs::saveSettings();
                exiting = true;
                exitTimer = 0.0f;
                toDuel = true;
                playSound(SoundEffect::WHITE_SWELL);
                break;
        }
    }
}

void EggDialogueState::confirmChoice(int index) {
    playSound(SoundEffect::CONFIRM);

    if (activeChoice == ChoiceType::GIVE_OR_KEEP) {
        activeChoice = ChoiceType::NONE;
        setBranch(index == 0 ? Branch::CONFIRM_GIVE : Branch::REFUSED);
    } else if (activeChoice == ChoiceType::CONFIRM_SURE) {
        activeChoice = ChoiceType::NONE;
        setBranch(index == 0 ? Branch::GIVEN : Branch::REFUSED);
    } else if (activeChoice == ChoiceType::ACCEPT_OR_REFUSE) {
        activeChoice = ChoiceType::NONE;
        setBranch(index == 0 ? Branch::ACCEPT_DUEL : Branch::REFUSE_DUEL);
    }
}

void EggDialogueState::update(float dt) {
    if (exiting) {
        exitTimer += dt;
        float limit = toDuel ? 2.5f : 0.8f;
        if (exitTimer >= limit) {
            MusicBeatState::skipTransition = true;
            if (toDuel) {
                switchState(new EggDuelState());
            } else {
                switchState(new TitleState());
            }
        }
        return;
    }

    u32 kDown = hidKeysDown();
    touchPosition touch;
    hidTouchRead(&touch);

    blinkTimer += dt * 3.5f;

    if (!lineComplete) {
        charTimer += dt;
        const float charInterval = 0.035f;
        while (charTimer >= charInterval && charsShown < (int)currentText.size()) {
            charTimer -= charInterval;
            char c = currentText[charsShown++];

            if (c != ' ' && c != '\n' && c != '.' && c != ',' &&
                c != '?' && c != '!' && c != '(' && c != ')' && c != '-') {
                if (++bleepCounter >= 2) {
                    playSound(SoundEffect::VOICE_BLEEP);
                    bleepCounter = 0;
                }
            }
        }
        if (charsShown >= (int)currentText.size()) {
            charsShown = (int)currentText.size();
            lineComplete = true;
        }

        if ((kDown & (KEY_A | KEY_START)) || (kDown & KEY_TOUCH)) {
            charsShown = (int)currentText.size();
            lineComplete = true;
        }
    } else if (activeChoice == ChoiceType::NONE) {
        if ((kDown & (KEY_A | KEY_START)) || (kDown & KEY_TOUCH)) {
            nextLine();
        }
    } else {
        int prev = choiceIndex;
        if (kDown & (KEY_DLEFT | KEY_DUP | KEY_CPAD_LEFT | KEY_CPAD_UP)) {
            choiceIndex = 0;
        } else if (kDown & (KEY_DRIGHT | KEY_DDOWN | KEY_CPAD_RIGHT | KEY_CPAD_DOWN)) {
            choiceIndex = 1;
        }

        if (choiceIndex != prev) {
            playSound(SoundEffect::CURSOR);
        }

        if (kDown & (KEY_A | KEY_START)) {
            confirmChoice(choiceIndex);
            return;
        }

        if (kDown & KEY_TOUCH) {
            if (touch.py >= 70 && touch.py <= 160) {
                if (touch.px >= 10 && touch.px <= 150) {
                    choiceIndex = 0;
                    confirmChoice(0);
                    return;
                } else if (touch.px >= 170 && touch.px <= 310) {
                    choiceIndex = 1;
                    confirmChoice(1);
                    return;
                }
            }
        }
    }
}

void EggDialogueState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_Font font = globalPixelFont ? globalPixelFont : globalVCRFont;

    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);

    if (font && topTextBuf) {
        float scale = 0.50f;

        // Measure line height to keep text vertically stable while typing
        C2D_TextBufClear(topTextBuf);
        C2D_Text measure;
        C2D_TextFontParse(&measure, font, topTextBuf, currentText.c_str());
        float fullW = 0.0f, fullH = 0.0f;
        C2D_TextGetDimensions(&measure, scale, scale, &fullW, &fullH);

        if (fullW > 360.0f) scale *= (360.0f / fullW);

        float textY = std::max(25.0f, std::round((240.0f - fullH) * 0.5f));

        C2D_TextBufClear(topTextBuf);
        std::string sub = currentText.substr(0, charsShown);
        C2D_TextFontParse(&topText, font, topTextBuf, sub.c_str());
        C2D_TextOptimize(&topText);
        C2D_DrawText(&topText, C2D_WithColor | C2D_AlignCenter, 200.0f, textY, 0.5f, scale, scale, C2D_Color32(255, 255, 255, 255));

        if (lineComplete && activeChoice == ChoiceType::NONE && !exiting) {
            if (((int)blinkTimer % 2) == 0) {
                C2D_TextFontParse(&promptText, font, topTextBuf, "v");
                C2D_TextOptimize(&promptText);
                C2D_DrawText(&promptText, C2D_WithColor | C2D_AlignCenter, 200.0f, 212.0f, 0.5f, 0.50f, 0.50f, C2D_Color32(255, 255, 0, 255));
            }
        }
    }

    if (exiting) {
        float limit = toDuel ? 2.5f : 0.8f;
        u8 a = (u8)(std::min(1.0f, exitTimer / limit) * 255.0f);
        u32 col = toDuel ? C2D_Color32(255, 255, 255, a) : C2D_Color32(0, 0, 0, a);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 400.0f, 240.0f, col);
    }

    if (!g_isRightEye && bottom) {
        C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bottom);

        if (activeChoice != ChoiceType::NONE && font && bottomTextBuf) {
            C2D_TextBufClear(bottomTextBuf);

            std::string leftLabel = (activeChoice == ChoiceType::ACCEPT_OR_REFUSE) ? "[ ACCEPT ]" : "[ YES ]";
            std::string rightLabel = (activeChoice == ChoiceType::ACCEPT_OR_REFUSE) ? "[ REFUSE ]" : "[ NO ]";

            std::string fullLeft = (choiceIndex == 0 ? "> " : "  ") + leftLabel + (choiceIndex == 0 ? " <" : "  ");
            std::string fullRight = (choiceIndex == 1 ? "> " : "  ") + rightLabel + (choiceIndex == 1 ? " <" : "  ");

            u32 colL = (choiceIndex == 0) ? C2D_Color32(255, 255, 0, 255) : C2D_Color32(160, 160, 160, 255);
            u32 colR = (choiceIndex == 1) ? C2D_Color32(255, 255, 0, 255) : C2D_Color32(160, 160, 160, 255);

            C2D_TextFontParse(&choiceLeft, font, bottomTextBuf, fullLeft.c_str());
            C2D_TextOptimize(&choiceLeft);
            C2D_DrawText(&choiceLeft, C2D_WithColor | C2D_AlignCenter, 80.0f, 110.0f, 0.5f, 0.50f, 0.50f, colL);

            C2D_TextFontParse(&choiceRight, font, bottomTextBuf, fullRight.c_str());
            C2D_TextOptimize(&choiceRight);
            C2D_DrawText(&choiceRight, C2D_WithColor | C2D_AlignCenter, 240.0f, 110.0f, 0.5f, 0.50f, 0.50f, colR);
        }

        if (exiting) {
            float limit = toDuel ? 2.5f : 0.8f;
            u8 a = (u8)(std::min(1.0f, exitTimer / limit) * 255.0f);
            u32 col = toDuel ? C2D_Color32(255, 255, 255, a) : C2D_Color32(0, 0, 0, a);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 320.0f, 240.0f, col);
        }
    }
}
