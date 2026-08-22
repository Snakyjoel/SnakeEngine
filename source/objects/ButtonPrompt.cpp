#include "ButtonPrompt.hpp"
#include "Alphabet.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
#include <algorithm>
#include <cstring>
#include <strings.h>
#include <cmath>

ButtonPrompt::ButtonPrompt(const std::string& button, const std::string& text, float x, float y, float scale, float alpha, u32 textColor, float depth)
    : button(button), text(text), x(x), y(y), scale(scale), alpha(alpha), textColor(textColor), depth(depth) {}

void ButtonPrompt::init() {
    if (!ClientPrefs::buttonPrompts) return;
    SpritesheetCache::get().load("shared/images/buttons");
    SpritesheetCache::get().load("shared/images/Alphabet");
}

std::string ButtonPrompt::normalizeButtonName(const std::string& btn) {
    std::string lower = btn;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "a" || lower == "accept" || lower == "confirm") return "a";
    if (lower == "b" || lower == "back" || lower == "cancel") return "b";
    if (lower == "x") return "x";
    if (lower == "y") return "y";
    if (lower == "start") return "start";
    if (lower == "select") return "select";
    if (lower == "home") return "home";
    if (lower == "stick") return "stick";
    if (lower == "l") return "l";
    if (lower == "r") return "r";
    if (lower == "dup" || lower == "up" || lower == "d-up") return "dUp";
    if (lower == "ddown" || lower == "down" || lower == "d-down") return "dDown";
    if (lower == "dleft" || lower == "left" || lower == "d-left") return "dLeft";
    if (lower == "dright" || lower == "right" || lower == "d-right") return "dRight";

    return btn;
}

static const Frame* findButtonFrame(CachedSpritesheet* sheet, const std::string& btn) {
    if (!sheet) return nullptr;

    std::string norm = ButtonPrompt::normalizeButtonName(btn);
    for (const auto& f : sheet->frames) {
        if (f.name == norm || f.name == btn) return &f;
    }
    for (const auto& f : sheet->frames) {
        if (strcasecmp(f.name.c_str(), btn.c_str()) == 0 || strcasecmp(f.name.c_str(), norm.c_str()) == 0) {
            return &f;
        }
    }
    return nullptr;
}

void ButtonPrompt::drawButton(const std::string& button, float x, float y, float scale, float alpha, float depth) {
    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    if (!sheet) return;

    const Frame* f = findButtonFrame(sheet, button);
    if (!f || !f->tex) return;

    C2D_Image img;
    img.tex = f->tex;
    img.subtex = &f->uv;

    C2D_ImageTint tint;
    C2D_ImageTint* tintPtr = nullptr;
    if (alpha < 1.0f) {
        C2D_AlphaImageTint(&tint, std::max(0.0f, std::min(1.0f, alpha)));
        tintPtr = &tint;
    }

    float lx = x - f->frameX * scale;
    float ly = y - f->frameY * scale;

    if (f->rotated) {
        float angleRad = -(3.14159265f / 2.0f);
        float cx = lx + f->uv.height * scale / 2.0f;
        float cy = ly + f->uv.width  * scale / 2.0f;
        C2D_DrawImageAtRotated(img, cx, cy, depth, angleRad, tintPtr, scale, scale);
    } else {
        C2D_DrawImageAt(img, lx, ly, depth, tintPtr, scale, scale);
    }
}

float ButtonPrompt::getButtonWidth(const std::string& button, float scale) {
    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    if (!sheet) return 0.0f;

    const Frame* f = findButtonFrame(sheet, button);
    if (!f) return 0.0f;

    float w = (f->frameW > 0) ? (float)f->frameW : (f->rotated ? (float)f->h : (float)f->w);
    return w * scale;
}

float ButtonPrompt::getButtonHeight(const std::string& button, float scale) {
    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    if (!sheet) return 0.0f;

    const Frame* f = findButtonFrame(sheet, button);
    if (!f) return 0.0f;

    float h = (f->frameH > 0) ? (float)f->frameH : (f->rotated ? (float)f->w : (float)f->h);
    return h * scale;
}

void ButtonPrompt::drawPrompt(const std::string& button, const std::string& text, float x, float y, float scale, float alpha, u32 textColor, float depth, float textScaleMultiplier, bool buttonOnRight) {
    if (!ClientPrefs::buttonPrompts) return;
    float btnScale = scale * 0.75f;
    float btnW = getButtonWidth(button, btnScale);
    float btnH = getButtonHeight(button, btnScale);

    float textScale = scale * 1.25f * textScaleMultiplier;
    float fontScreenScale = (240.0f / 720.0f) * 1.25f * textScaleMultiplier;
    float textVisualH = 50.0f * textScale * fontScreenScale;
    float promptH = std::max(btnH, textVisualH);

    float spacing = 6.0f * (scale / 0.55f);

    if (buttonOnRight) {
        float textX = x;
        float textY = y + (promptH - textVisualH) * 0.5f + (7.0f * scale * textScaleMultiplier);
        Alphabet::draw(text, textX, textY, textScale, alpha, false, textColor, depth);

        float textW = Alphabet::getTextWidth(text, textScale);
        float btnX = x + textW + spacing;
        float btnY = y + (promptH - btnH) * 0.5f;
        drawButton(button, btnX, btnY, btnScale, alpha, depth);
    } else {
        float btnY = y + (promptH - btnH) * 0.5f;
        drawButton(button, x, btnY, btnScale, alpha, depth);

        float textX = x + btnW + spacing;
        float textY = y + (promptH - textVisualH) * 0.5f + (7.0f * scale * textScaleMultiplier);
        Alphabet::draw(text, textX, textY, textScale, alpha, false, textColor, depth);
    }
}

void ButtonPrompt::drawPrompt2(const std::string& btn1, const std::string& btn2, const std::string& text, float x, float y, float scale, float alpha, u32 textColor, float depth, float textScaleMultiplier) {
    if (!ClientPrefs::buttonPrompts) return;
    float btnScale = scale * 0.75f;
    float btn1W = getButtonWidth(btn1, btnScale);
    float btn1H = getButtonHeight(btn1, btnScale);
    float btn2W = getButtonWidth(btn2, btnScale);
    float btn2H = getButtonHeight(btn2, btnScale);
    float maxBtnH = std::max(btn1H, btn2H);

    float textScale = scale * 1.25f * textScaleMultiplier;
    float fontScreenScale = (240.0f / 720.0f) * 1.25f * textScaleMultiplier;
    float textVisualH = 50.0f * textScale * fontScreenScale;
    float promptH = std::max(maxBtnH, textVisualH);

    float btn1Y = y + (promptH - btn1H) * 0.5f;
    drawButton(btn1, x, btn1Y, btnScale, alpha, depth);

    float btnGap = 4.0f * (scale / 0.55f);
    float btn2X = x + btn1W + btnGap;
    float btn2Y = y + (promptH - btn2H) * 0.5f;
    drawButton(btn2, btn2X, btn2Y, btnScale, alpha, depth);

    float textGap = 6.0f * (scale / 0.55f);
    float textX = btn2X + btn2W + textGap;
    float textY = y + (promptH - textVisualH) * 0.5f + (7.0f * scale * textScaleMultiplier);

    Alphabet::draw(text, textX, textY, textScale, alpha, false, textColor, depth);
}

float ButtonPrompt::getPromptWidth(const std::string& button, const std::string& text, float scale, float textScaleMultiplier) {
    float btnScale = scale * 0.75f;
    float btnW = getButtonWidth(button, btnScale);
    float spacing = 6.0f * (scale / 0.55f);
    float textScale = scale * 1.25f * textScaleMultiplier;
    float textW = Alphabet::getTextWidth(text, textScale);
    return btnW + spacing + textW;
}

float ButtonPrompt::getPrompt2Width(const std::string& btn1, const std::string& btn2, const std::string& text, float scale, float textScaleMultiplier) {
    float btnScale = scale * 0.75f;
    float btn1W = getButtonWidth(btn1, btnScale);
    float btn2W = getButtonWidth(btn2, btnScale);
    float btnGap = 4.0f * (scale / 0.55f);
    float textGap = 6.0f * (scale / 0.55f);
    float textScale = scale * 1.25f * textScaleMultiplier;
    float textW = Alphabet::getTextWidth(text, textScale);
    return btn1W + btnGap + btn2W + textGap + textW;
}

float ButtonPrompt::getPromptHeight(const std::string& button, const std::string& text, float scale, float textScaleMultiplier) {
    float btnScale = scale * 0.75f;
    float btnH = getButtonHeight(button, btnScale);
    float textScale = scale * 1.25f * textScaleMultiplier;
    float fontScreenScale = (240.0f / 720.0f) * 1.25f * textScaleMultiplier;
    float textVisualH = 50.0f * textScale * fontScreenScale;
    return std::max(btnH, textVisualH);
}

void ButtonPrompt::draw() {
    drawPrompt(button, text, x, y, scale, alpha, textColor, depth);
}

void ButtonPrompt::draw(float overrideX, float overrideY) {
    drawPrompt(button, text, overrideX, overrideY, scale, alpha, textColor, depth);
}

void ButtonPrompt::draw(float overrideX, float overrideY, float overrideAlpha) {
    drawPrompt(button, text, overrideX, overrideY, scale, overrideAlpha, textColor, depth);
}

float ButtonPrompt::getWidth() const {
    return getPromptWidth(button, text, scale);
}

float ButtonPrompt::getHeight() const {
    return getPromptHeight(button, text, scale);
}

std::string ButtonPrompt::getAnimatedDpad(float timer) {
    static const char* dpadCycle[] = {"dUp", "dLeft", "dDown", "dRight"};
    int step = ((int)std::floor(timer / 2.0f)) % 4;
    if (step < 0) step = (step % 4 + 4) % 4;
    return dpadCycle[step];
}
