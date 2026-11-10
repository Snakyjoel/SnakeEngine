#ifndef PIXELPAUSERENDERER_HPP
#define PIXELPAUSERENDERER_HPP

#include "IPauseRenderer.hpp"
#include <vector>
#include <string>
#include <cstdint>

class PauseSubState;

class PixelPauseRenderer : public IPauseRenderer {
public:
    enum NesSfx {
        NES_SFX_OPEN = 0,
        NES_SFX_CURSOR,
        NES_SFX_SELECT,
        NES_SFX_CANCEL,
        NES_SFX_COUNT
    };

    static constexpr int NES_AUDIO_CHANNEL = 6;
    static void freeAudio();

    explicit PixelPauseRenderer(PauseSubState* parent);
    ~PixelPauseRenderer() override;

    void init() override;
    void onMenuChanged(const std::vector<std::string>& items, const std::string& title) override;
    void update(float dt) override;
    void drawTop(C3D_RenderTarget* target, float screenW, float screenH) override;
    void drawBottom(C3D_RenderTarget* target, float screenW, float screenH) override;

    void onCursorMove() override;
    void onSelect() override;
    void onCancel() override;

private:
    void setupMenuTexts(const std::vector<std::string>& items, const std::string& title);
    void setupInfoTexts();
    void playNesSound(NesSfx sound);
    static void initNesAudio();

    PauseSubState* subState;

    C2D_TextBuf pauseTextBuf = nullptr;
    C2D_Text pauseTitleObj;
    std::vector<C2D_Text> pauseOptionObjs;
    std::vector<float> optionWidths;
    std::vector<float> optionHeights;
    float titleWidth = 0.0f;
    float titleHeight = 0.0f;
    float maxOptionWidth = 0.0f;

    float nesBlinkTimer = 0.0f;
    float nesOpenTimer = 0.0f;
    float nesCursorY = 0.0f;

    C2D_TextBuf infoTextBuf = nullptr;
    C2D_Text infoTextObj[3];
    float infoWidths[3] = {0.0f, 0.0f, 0.0f};
    float infoHeights[3] = {0.0f, 0.0f, 0.0f};
    float infoTotalHeight = 0.0f;
    float maxInfoWidth = 0.0f;

    static int16_t* s_nesSfxBuffers[NES_SFX_COUNT];
    static uint32_t s_nesSfxSampleCounts[NES_SFX_COUNT];
    static ndspWaveBuf s_nesWaveBuf;
};

#endif

