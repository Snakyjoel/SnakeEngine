#pragma once
#include "backend/MusicBeatState.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>

class EggDialogueState : public MusicBeatState {
public:
    EggDialogueState();
    ~EggDialogueState() override;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    bool hasTransitions() const override { return false; }

private:
    enum class Branch {
        INTRO,
        CONFIRM_GIVE,
        GIVEN,
        REFUSED,
        REFUSE_DUEL,
        ACCEPT_DUEL
    };

    enum class ChoiceType {
        NONE,
        GIVE_OR_KEEP,
        CONFIRM_SURE,
        ACCEPT_OR_REFUSE
    };

    enum class SoundEffect {
        VOICE_BLEEP,
        CURSOR,
        CONFIRM,
        ADVANCE,
        WHITE_SWELL,
        COUNT
    };

    void initAudio();
    void freeAudio();
    void playSound(SoundEffect sfx);

    void setBranch(Branch branch);
    void nextLine();
    void confirmChoice(int index);

    Branch currentBranch = Branch::INTRO;
    ChoiceType activeChoice = ChoiceType::NONE;
    int choiceIndex = 0;

    std::vector<std::string> lines;
    int lineIndex = 0;
    std::string currentText;
    int charsShown = 0;
    float charTimer = 0.0f;
    bool lineComplete = false;
    int bleepCounter = 0;

    float blinkTimer = 0.0f;
    bool exiting = false;
    float exitTimer = 0.0f;
    bool toDuel = false;

    static constexpr int SFX_CHANNEL = 6;
    static constexpr int SFX_TOTAL = (int)SoundEffect::COUNT;
    int16_t* sfxBuffers[SFX_TOTAL] = { nullptr };
    uint32_t sfxLengths[SFX_TOTAL] = { 0 };
    ndspWaveBuf sfxWave;

    C2D_TextBuf topTextBuf = nullptr;
    C2D_TextBuf bottomTextBuf = nullptr;
    C2D_Text topText;
    C2D_Text promptText;
    C2D_Text choiceLeft;
    C2D_Text choiceRight;
};
