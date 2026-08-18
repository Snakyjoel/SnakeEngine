#pragma once
#include <string>
#include <citro2d.h>

class ButtonPrompt {
public:
    std::string button;
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 0.55f;
    float alpha = 1.0f;
    u32 textColor = 0xFFFFFFFF;
    float depth = 0.95f;

    ButtonPrompt() = default;
    ButtonPrompt(const std::string& button, const std::string& text, float x = 0.0f, float y = 0.0f, float scale = 0.55f, float alpha = 1.0f, u32 textColor = 0xFFFFFFFF, float depth = 0.95f);

    void draw();
    void draw(float overrideX, float overrideY);
    void draw(float overrideX, float overrideY, float overrideAlpha);

    float getWidth() const;
    float getHeight() const;

    // Static utilities for easy one-line calls across any state
    static void init();
    static void drawPrompt(const std::string& button, const std::string& text, float x, float y, float scale = 0.55f, float alpha = 1.0f, u32 textColor = 0xFFFFFFFF, float depth = 0.95f);
    static void drawPrompt2(const std::string& btn1, const std::string& btn2, const std::string& text, float x, float y, float scale = 0.55f, float alpha = 1.0f, u32 textColor = 0xFFFFFFFF, float depth = 0.95f);
    static void drawButton(const std::string& button, float x, float y, float scale = 0.45f, float alpha = 1.0f, float depth = 0.95f);
    static float getPromptWidth(const std::string& button, const std::string& text, float scale = 0.55f);
    static float getPrompt2Width(const std::string& btn1, const std::string& btn2, const std::string& text, float scale = 0.55f);
    static float getPromptHeight(const std::string& button, const std::string& text, float scale = 0.55f);
    static float getButtonWidth(const std::string& button, float scale = 0.45f);
    static float getButtonHeight(const std::string& button, float scale = 0.45f);
    static std::string normalizeButtonName(const std::string& btn);
    static std::string getAnimatedDpad(float timer);
};
