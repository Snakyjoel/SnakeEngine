#include "CppAnimate.hpp"
#include <algorithm>
#include <cmath>

void CppAnimate::loadSheet(const std::string& path) {
    sheet = SpritesheetCache::get().load(path);
}

void CppAnimate::addAnim(const std::string& name, const std::string& prefix,
                          float fps, bool loop,
                          float offX, float offY,
                          const std::vector<int>& indices) {
    if (!sheet) return;

    AnimData anim;
    anim.prefix = prefix;
    anim.fps    = fps;
    anim.loop   = loop;
    anim.offX   = offX;
    anim.offY   = offY;

    // Collect all frames matching the prefix, in order
    std::vector<int> matched;
    for (int i = 0; i < (int)sheet->frames.size(); i++) {
        if (sheet->frames[i].name.find(prefix) == 0) {
            matched.push_back(i);
        }
    }

    if (indices.empty()) {
        anim.frameIndices = matched;
    } else {
        for (int idx : indices) {
            if (idx >= 0 && idx < (int)matched.size()) {
                anim.frameIndices.push_back(matched[idx]);
            }
        }
    }

    anims[name] = anim;
}

void CppAnimate::play(const std::string& name, bool forceRestart) {
    if (!hasAnim(name)) return;

    if (curAnim == name && !forceRestart && !animFinished) return;

    curAnim      = name;
    animFinished = false;
    paused       = false;

    curFrameIdx  = 0;
    frameTimer   = 0.0f;
}

const CppAnimate::AnimData* CppAnimate::getCurAnimData() const {
    if (curAnim.empty()) return nullptr;
    auto it = anims.find(curAnim);
    if (it != anims.end()) return &it->second;
    return nullptr;
}

void CppAnimate::stop() {
    paused = true;
}

void CppAnimate::pause() {
    paused = true;
}

void CppAnimate::resume() {
    paused = false;
}

void CppAnimate::update(float dt) {
    const AnimData* animData = getCurAnimData();
    if (!animData || paused || animFinished) return;
    if (animData->frameIndices.empty()) return;

    float frameDuration = (animData->fps > 0.0f) ? (1.0f / animData->fps) : (1.0f / 24.0f);
    frameTimer += dt;

    while (frameTimer >= frameDuration) {
        frameTimer -= frameDuration;
        curFrameIdx++;

        if (curFrameIdx >= (int)animData->frameIndices.size()) {
            if (animData->loop) {
                curFrameIdx = 0;
            } else {
                curFrameIdx  = (int)animData->frameIndices.size() - 1;
                animFinished = true;
                if (onAnimFinished) onAnimFinished(curAnim);
                break;
            }
        }
    }
}
// JARONA!
const Frame* CppAnimate::currentFrame() const {
    if (!sheet) return nullptr;
    const AnimData* animData = getCurAnimData();
    if (!animData || animData->frameIndices.empty()) return nullptr;
    int frameIdx = animData->frameIndices[curFrameIdx];
    if (frameIdx < 0 || frameIdx >= (int)sheet->frames.size()) return nullptr;
    return &sheet->frames[frameIdx];
}

void CppAnimate::draw(float x, float y, float depth, float sx, float sy, C2D_ImageTint* tint) {
    if (!visible) return;
    const Frame* f = currentFrame();
    if (!f || !f->tex) return;

    const AnimData* animData = getCurAnimData();
    float finalX = x + (animData ? animData->offX : 0.0f) + extraOffsetX;
    float finalY = y + (animData ? animData->offY : 0.0f) + extraOffsetY;

    float finalSX = sx * scaleX * (flipX ? -1.0f : 1.0f);
    float finalSY = sy * scaleY * (flipY ? -1.0f : 1.0f);

    if (ignoreFrameOffsets) {
        finalX += (float)f->frameX * finalSX;
        finalY += (float)f->frameY * finalSY;
    }

    C2D_ImageTint alphaTint;
    C2D_ImageTint* usedTint = tint;
    if (alpha < 1.0f && !tint) {
        C2D_AlphaImageTint(&alphaTint, alpha);
        usedTint = &alphaTint;
    }

    if (antialiasing) {
        C3D_TexSetFilter(f->tex, GPU_LINEAR, GPU_LINEAR);
    } else {
        C3D_TexSetFilter(f->tex, GPU_NEAREST, GPU_NEAREST);
    }

    drawFrameAt(*f, finalX, finalY, depth, usedTint, finalSX, finalSY);
}

void CppAnimate::drawCentered(float cx, float cy, float depth, float sx, float sy, C2D_ImageTint* tint) {
    if (!visible) return;
    const Frame* f = currentFrame();
    if (!f || !f->tex) return;

    float finalSX = sx * scaleX;
    float finalSY = sy * scaleY;

    const AnimData* animData = getCurAnimData();
    float offX = animData ? animData->offX : 0.0f;
    float offY = animData ? animData->offY : 0.0f;

    float w = ignoreFrameOffsets ? (f->rotated ? (float)f->h : (float)f->w) : frameLogicalW(*f);
    float h = ignoreFrameOffsets ? (f->rotated ? (float)f->w : (float)f->h) : frameLogicalH(*f);

    float drawX = cx - w * finalSX * 0.5f + offX + extraOffsetX;
    float drawY = cy - h * finalSY * 0.5f + offY + extraOffsetY;

    draw(drawX, drawY, depth, sx, sy, tint);
}

bool CppAnimate::hasAnim(const std::string& name) const {
    return anims.count(name) > 0;
}

float CppAnimate::width() const {
    const Frame* f = currentFrame();
    if (!f) return 0.0f;
    float w = ignoreFrameOffsets ? (f->rotated ? (float)f->h : (float)f->w) : frameLogicalW(*f);
    return w * scaleX;
}

float CppAnimate::height() const {
    const Frame* f = currentFrame();
    if (!f) return 0.0f;
    float h = ignoreFrameOffsets ? (f->rotated ? (float)f->w : (float)f->h) : frameLogicalH(*f);
    return h * scaleY;
}
