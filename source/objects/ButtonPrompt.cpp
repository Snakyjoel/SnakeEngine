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

    bool outline = false;
    // Check if ends with " outline" or "outline"
    if (lower.length() > 8 && lower.substr(lower.length() - 8) == " outline") {
        outline = true;
        lower = lower.substr(0, lower.length() - 8);
    } else if (lower.length() > 7 && lower.substr(lower.length() - 7) == "outline") {
        outline = true;
        lower = lower.substr(0, lower.length() - 7);
    }

    std::string base = lower;
    if (lower == "a" || lower == "accept" || lower == "confirm") base = "a";
    else if (lower == "b" || lower == "back" || lower == "cancel") base = "b";
    else if (lower == "x") base = "x";
    else if (lower == "y") base = "y";
    else if (lower == "start") base = "start";
    else if (lower == "select") base = "select";
    else if (lower == "home") base = "home";
    else if (lower == "stick" || lower == "pad") base = "pad";
    else if (lower == "l") base = "l";
    else if (lower == "r") base = "r";
    else if (lower == "dup" || lower == "up" || lower == "d-up" || lower == "d pad" || lower == "dpad" ||
             lower == "ddown" || lower == "down" || lower == "d-down" ||
             lower == "dleft" || lower == "left" || lower == "d-left" ||
             lower == "dright" || lower == "right" || lower == "d-right") {
        base = "d pad";
    }

    if (outline) {
        return base + " outline";
    }
    return base;
}

static float getButtonRotationAngle(const std::string& btn) {
    std::string lower = btn;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    // Remove " outline" if present for rotation check
    if (lower.length() > 8 && lower.substr(lower.length() - 8) == " outline") {
        lower = lower.substr(0, lower.length() - 8);
    } else if (lower.length() > 7 && lower.substr(lower.length() - 7) == "outline") {
        lower = lower.substr(0, lower.length() - 7);
    }

    const float PI = 3.14159265f;
    // Base d-pad sprite
    if (lower == "dright" || lower == "right" || lower == "d-right") {
        return PI / 2.0f; // 90° clockwise
    }
    if (lower == "ddown" || lower == "down" || lower == "d-down") {
        return PI;        // 180°
    }
    if (lower == "dleft" || lower == "left" || lower == "d-left") {
        return -PI / 2.0f; // 270° (or -90°)
    }
    return 0.0f;
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

static float calcFrameWidth(const Frame* f, float scale, float rotAngle = 0.0f) {
    if (!f) return 0.0f;
    bool isRot90 = (std::abs(std::sin(rotAngle)) > 0.7f);
    float baseW = (f->frameW > 0) ? (float)f->frameW : (f->rotated ? (float)f->h : (float)f->w);
    float baseH = (f->frameH > 0) ? (float)f->frameH : (f->rotated ? (float)f->w : (float)f->h);
    float w = isRot90 ? baseH : baseW;
    return w * scale;
}

static float calcFrameHeight(const Frame* f, float scale, float rotAngle = 0.0f) {
    if (!f) return 0.0f;
    bool isRot90 = (std::abs(std::sin(rotAngle)) > 0.7f);
    float baseW = (f->frameW > 0) ? (float)f->frameW : (f->rotated ? (float)f->h : (float)f->w);
    float baseH = (f->frameH > 0) ? (float)f->frameH : (f->rotated ? (float)f->w : (float)f->h);
    float h = isRot90 ? baseW : baseH;
    return h * scale;
}

static void renderButtonFrame(const Frame* f, float x, float y, float scale, float alpha, float depth, float extraAngle = 0.0f) {
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

    float totalAngle = extraAngle;
    if (f->rotated) {
        totalAngle -= (3.14159265f / 2.0f);
    }

    if (std::abs(totalAngle) > 0.001f) {
        float cx = lx + (f->rotated ? f->uv.height : f->uv.width)  * scale / 2.0f;
        float cy = ly + (f->rotated ? f->uv.width  : f->uv.height) * scale / 2.0f;
        C2D_DrawImageAtRotated(img, cx, cy, depth, totalAngle, tintPtr, scale, scale);
    } else {
        C2D_DrawImageAt(img, lx, ly, depth, tintPtr, scale, scale);
    }
}

void ButtonPrompt::drawButton(const std::string& button, float x, float y, float scale, float alpha, float depth) {
    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    if (!sheet) return;
    const Frame* f = findButtonFrame(sheet, button);
    float rot = getButtonRotationAngle(button);
    renderButtonFrame(f, x, y, scale, alpha, depth, rot);
}

float ButtonPrompt::getButtonWidth(const std::string& button, float scale) {
    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    if (!sheet) return 0.0f;
    const Frame* f = findButtonFrame(sheet, button);
    float rot = getButtonRotationAngle(button);
    return calcFrameWidth(f, scale, rot);
}

float ButtonPrompt::getButtonHeight(const std::string& button, float scale) {
    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    if (!sheet) return 0.0f;
    const Frame* f = findButtonFrame(sheet, button);
    float rot = getButtonRotationAngle(button);
    return calcFrameHeight(f, scale, rot);
}

void ButtonPrompt::drawPrompt(const std::string& button, const std::string& text, float x, float y, float scale, float alpha, u32 textColor, float depth, float textScaleMultiplier, bool buttonOnRight) {
    if (!ClientPrefs::buttonPrompts) return;

    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    const Frame* f = findButtonFrame(sheet, button);
    float rot = getButtonRotationAngle(button);

    float btnScale = scale * 0.75f;
    float btnW = calcFrameWidth(f, btnScale, rot);
    float btnH = calcFrameHeight(f, btnScale, rot);

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
        renderButtonFrame(f, btnX, btnY, btnScale, alpha, depth, rot);
    } else {
        float btnY = y + (promptH - btnH) * 0.5f;
        renderButtonFrame(f, x, btnY, btnScale, alpha, depth, rot);

        float textX = x + btnW + spacing;
        float textY = y + (promptH - textVisualH) * 0.5f + (7.0f * scale * textScaleMultiplier);
        Alphabet::draw(text, textX, textY, textScale, alpha, false, textColor, depth);
    }
}

void ButtonPrompt::drawPrompt2(const std::string& btn1, const std::string& btn2, const std::string& text, float x, float y, float scale, float alpha, u32 textColor, float depth, float textScaleMultiplier) {
    if (!ClientPrefs::buttonPrompts) return;

    CachedSpritesheet* sheet = SpritesheetCache::get().load("shared/images/buttons");
    const Frame* f1 = findButtonFrame(sheet, btn1);
    const Frame* f2 = findButtonFrame(sheet, btn2);
    float rot1 = getButtonRotationAngle(btn1);
    float rot2 = getButtonRotationAngle(btn2);

    float btnScale = scale * 0.75f;
    float btn1W = calcFrameWidth(f1, btnScale, rot1);
    float btn1H = calcFrameHeight(f1, btnScale, rot1);
    float btn2W = calcFrameWidth(f2, btnScale, rot2);
    float btn2H = calcFrameHeight(f2, btnScale, rot2);
    float maxBtnH = std::max(btn1H, btn2H);

    float textScale = scale * 1.25f * textScaleMultiplier;
    float fontScreenScale = (240.0f / 720.0f) * 1.25f * textScaleMultiplier;
    float textVisualH = 50.0f * textScale * fontScreenScale;
    float promptH = std::max(maxBtnH, textVisualH);

    float btn1Y = y + (promptH - btn1H) * 0.5f;
    renderButtonFrame(f1, x, btn1Y, btnScale, alpha, depth, rot1);

    float btnGap = 4.0f * (scale / 0.55f);
    float btn2X = x + btn1W + btnGap;
    float btn2Y = y + (promptH - btn2H) * 0.5f;
    renderButtonFrame(f2, btn2X, btn2Y, btnScale, alpha, depth, rot2);

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
