#ifndef PAUSESUBSTATE_HPP
#define PAUSESUBSTATE_HPP

#include <vector>
#include <string>
#include <citro2d.h>

class PauseSubState {
public:
    PauseSubState();
    ~PauseSubState();

    void update(float dt);
    void draw();

    // Menu selections
    int pauseSelection = 0;
    float pauseLerpSelection = 0.0f;

    enum PauseMenuState {
        PAUSE_MAIN,
        PAUSE_DIFFICULTY
    };
    PauseMenuState pauseMenuState = PAUSE_MAIN;

    std::vector<std::string> pauseMenuItems;
    class MemoryDebugState* memoryDebugState = nullptr;
    
    enum NesSfx {
        NES_SFX_OPEN = 0,
        NES_SFX_CURSOR,
        NES_SFX_SELECT,
        NES_SFX_CANCEL,
        NES_SFX_COUNT
    };

private:
    std::vector<std::string> buildMainItems();
    void setupPauseMenu(const std::vector<std::string>& items, const std::string& title = "PAUSED");
    void initNesAudio();
    void freeNesAudio();
    void playNesSound(NesSfx sound);

    C2D_TextBuf pauseTextBuf = nullptr;
    C2D_Text pauseTitleObj;
    C2D_Text pauseOptionsObj[15];
    float optionWidths[15] = {0};
    float optionHeights[15] = {0};
    float titleWidth = 0.0f;
    float titleHeight = 0.0f;
    float maxOptionWidth = 0.0f;

    float nesBlinkTimer = 0.0f;
    float nesOpenTimer = 0.0f;
    float nesCursorY = 0.0f;

    int16_t* nesSfxBuffers[NES_SFX_COUNT] = {nullptr, nullptr, nullptr, nullptr};
    uint32_t nesSfxSampleCounts[NES_SFX_COUNT] = {0, 0, 0, 0};
    ndspWaveBuf nesWaveBuf;
};

#endif
