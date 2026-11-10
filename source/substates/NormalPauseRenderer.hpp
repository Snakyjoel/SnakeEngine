#ifndef NORMALPAUSERENDERER_HPP
#define NORMALPAUSERENDERER_HPP

#include "IPauseRenderer.hpp"
#include <vector>
#include <string>

class PauseSubState;

class NormalPauseRenderer : public IPauseRenderer {
public:
    explicit NormalPauseRenderer(PauseSubState* parent);
    ~NormalPauseRenderer() override;

    void init() override;
    void onMenuChanged(const std::vector<std::string>& items, const std::string& title) override;
    void update(float dt) override;
    void drawTop(C3D_RenderTarget* target, float screenW, float screenH) override;
    void drawBottom(C3D_RenderTarget* target, float screenW, float screenH) override;

    void onCursorMove() override;

private:
    void setupMenuTexts(const std::vector<std::string>& items, const std::string& title);
    void setupInfoTexts();

    PauseSubState* subState;

    C2D_TextBuf pauseTextBuf = nullptr;
    std::vector<C2D_Text> pauseOptionObjs;
    std::vector<float> optionWidths;
    std::vector<float> optionHeights;

    C2D_TextBuf infoTextBuf = nullptr;
    C2D_Text infoTextObj[3];
    float infoWidths[3] = {0.0f, 0.0f, 0.0f};
    float infoHeights[3] = {0.0f, 0.0f, 0.0f};
    float infoTotalHeight = 0.0f;
    float maxInfoWidth = 0.0f;
    float infoTimer = 0.0f;
};

#endif

