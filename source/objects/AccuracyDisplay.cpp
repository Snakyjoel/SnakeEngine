#include "AccuracyDisplay.hpp"
#include <cmath>
#include <algorithm>

AccuracyDisplay::AccuracyDisplay() {
    clearedBoxFrame.tex = nullptr;
    for (int i = 0; i < 10; i++) {
        clearedNumberFrames[i].clear();
    }
}

AccuracyDisplay::~AccuracyDisplay() {
    if (freeplayBFSheet) {
        C2D_SpriteSheetFree(freeplayBFSheet);
        freeplayBFSheet = nullptr;
    }
    // letterStuffSheet is managed by SpritesheetCache, not freed here
}

void AccuracyDisplay::load() {
    if (isLoaded) return;

    std::string bfAssetsImgPath = Paths::image("freeplay/freeplayAssetsBF");
    std::string bfAssetsXmlPath = Paths::xml("freeplay/freeplayAssetsBF");
    freeplayBFSheet = C2D_SpriteSheetLoad(bfAssetsImgPath.c_str());

    for (int i = 0; i < 10; i++) {
        clearedNumberFrames[i].clear();
    }
    clearedBoxFrame.tex = nullptr;
    letterStuffFrames.clear();

    if (freeplayBFSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(freeplayBFSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::vector<Frame> tempFrames;
        SparrowParser::parseXml(bfAssetsXmlPath, tempFrames);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : tempFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left   = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top    = mainImg.subtex->top  + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right  = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top  + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);

            std::string nl = f.name;
            for (char& c : nl) c = (char)tolower((unsigned char)c);

            if (nl.find("clearbox") != std::string::npos) {
                clearedBoxFrame = f;
            } else {
                for (int i = 0; i < 10; i++) {
                    std::string numExact = std::to_string(i);
                    std::string numZeros = std::to_string(i) + "0000";
                    if (nl == numExact || nl == numZeros) {
                        clearedNumberFrames[i].push_back(f);
                        break;
                    }
                }
            }

            // Rating letters (same logic as FreeplayState)
            if (nl.find("instance") != std::string::npos || nl.find("seperator") != std::string::npos) {
                letterStuffFrames.push_back(f);
            }
        }
    }
    isLoaded = true;
}

void AccuracyDisplay::setAccuracy(float accuracy, bool snap) {
    targetAccuracy = accuracy;
    if (snap) {
        lerpAccuracy = targetAccuracy;
    }
}

void AccuracyDisplay::setRating(const std::string& rating) {
    currentRating = rating;
}

void AccuracyDisplay::update(float dt) {
    if (std::abs(targetAccuracy - lerpAccuracy) < 0.05f) {
        lerpAccuracy = targetAccuracy;
    } else {
        lerpAccuracy += (targetAccuracy - lerpAccuracy) * (1.0f - exp2f(-12.0f * dt));
    }
}

Frame AccuracyDisplay::getRatingFrame() const {
    if (currentRating.empty() || letterStuffFrames.empty()) return Frame();
    std::string prefix = currentRating;
    if (prefix == "P" || prefix == "GP") prefix = "p";
    else if (prefix == "E") prefix = "e";
    else if (prefix == "G" || prefix == "g") prefix = "g";
    else if (prefix == "L") prefix = "l";

    for (const auto& f : letterStuffFrames) {
        std::string nl = f.name;
        for (char& c : nl) c = (char)tolower((unsigned char)c);
        if (nl.find(prefix + " ") == 0 || nl == prefix) {
            return f;
        }
    }
    return Frame();
}

u32 AccuracyDisplay::getRatingColor(u8 alpha) const {
    if (currentRating == "GP") return C2D_Color32(0xFF, 0xD7, 0x00, alpha);
    if (currentRating == "P")  return C2D_Color32(0xFF, 0xA8, 0xFF, alpha);
    if (currentRating == "E")  return C2D_Color32(0xFF, 0xFF, 0xB9, alpha);
    if (currentRating == "G")  return C2D_Color32(0xEF, 0xFD, 0xFF, alpha);
    if (currentRating == "g")  return C2D_Color32(0xF3, 0xA3, 0x80, alpha);
    if (currentRating == "L")  return C2D_Color32(0x6B, 0x8C, 0xFB, alpha);
    return C2D_Color32(255, 255, 255, alpha);
}

void AccuracyDisplay::draw(float x, float y, float depth, float alpha, float scale) {
    if (!clearedBoxFrame.tex) return;

    u8 alphaU8 = (u8)(alpha * 255.0f);
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
    drawFrameAt(clearedBoxFrame, x, y, depth, &tint, scale, scale);

    // Draw accuracy digits (right-to-left, using Sparrow frameW for proper "1" spacing)
    int accVal = (int)std::round(lerpAccuracy);
    if (accVal < 0) accVal = 0;
    if (accVal > 100) accVal = 100;
    std::string accStr = std::to_string(accVal);

    float padding = 1.0f * scale;
    float currentX = x + (65.0f * scale);
    float currentY = y + (18.0f * scale);

    for (int idx = (int)accStr.length() - 1; idx >= 0; idx--) {
        int digit = accStr[idx] - '0';
        if (digit >= 0 && digit <= 9 && !clearedNumberFrames[digit].empty()) {
            const Frame& f = clearedNumberFrames[digit][0];
            float logicW = (f.frameW > 0 ? (float)f.frameW : (float)f.w) * scale;
            currentX -= logicW;
            float drawX = currentX + (-f.frameX * scale);
            float drawY = currentY + (-f.frameY * scale);
            drawFrameAt(f, drawX, drawY, depth + 0.1f, &tint, scale, scale);
            currentX -= padding;
        }
    }

    // Draw rating letter to the LEFT of the accuracy box
    if (!currentRating.empty()) {
        Frame rFrame = getRatingFrame();
        if (rFrame.tex) {
            // Same scale factor as FreeplayState: 0.8 / 1.77083
            float rScale = (0.8f / 1.77083f) * scale / 0.65f;
            // Logical sizes account for rotated / trimmed Sparrow frames
            float rW = frameLogicalW(rFrame) * rScale;
            float boxH = frameLogicalH(clearedBoxFrame) * scale;
            float centerX = x - (4.0f * scale) - (rW * 0.5f);
            float centerY = y + (boxH * 0.5f);
            C2D_ImageTint rTint;
            C2D_PlainImageTint(&rTint, getRatingColor(alphaU8), 1.0f);
            drawFrameCentered(rFrame, centerX, centerY, depth + 0.05f, &rTint, rScale, rScale);
        }
    }
}
