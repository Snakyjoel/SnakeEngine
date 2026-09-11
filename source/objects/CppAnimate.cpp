#include "CppAnimate.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

CppAnimate::~CppAnimate() {
    if (smSheet) C2D_SpriteSheetFree(smSheet);
}

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


// HERE I COME SAN FRANCISCO
const Frame* CppAnimate::currentFrame() const {
    if (!sheet) return nullptr;
    const AnimData* animData = getCurAnimData();
    if (!animData || animData->frameIndices.empty()) return nullptr;
    int frameIdx = animData->frameIndices[curFrameIdx];
    if (frameIdx < 0 || frameIdx >= (int)sheet->frames.size()) return nullptr;
    return &sheet->frames[frameIdx];
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
    if (isSpritemapMode) {
        return smAnims.count(name) > 0 || smData.symbols.count(name) > 0;
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// Spritemap (Adobe Animate) mode implementation
// ─────────────────────────────────────────────────────────────────────────────

bool CppAnimate::loadSpritemap(const std::string& t3xPath,
                                const std::string& atlasJsonPath,
                                const std::string& animJsonPath) {
    if (!SpritemapParser::parse(atlasJsonPath, animJsonPath, smData)) {
        return false;
    }

    smSheet = C2D_SpriteSheetLoad(t3xPath.c_str());
    if (!smSheet) {
        return false;
    }

    C2D_Image mainImg = C2D_SpriteSheetGetImage(smSheet, 0);
    smTex = mainImg.tex;
    smSubtex = mainImg.subtex;
    if (!smTex) { 
        C2D_SpriteSheetFree(smSheet); smSheet = nullptr; smSubtex = nullptr; return false; 
    }

    // Build a subtexture covering the entire texture
    smFullSub.width  = (u16)smTex->width;
    smFullSub.height = (u16)smTex->height;
    smFullSub.left   = 0.0f;
    smFullSub.top    = 0.0f;
    smFullSub.right  = 1.0f;
    smFullSub.bottom = 1.0f;

    isSpritemapMode = true;

    // Auto-scale: fit the canvas into the 3DS top screen (400 x 240)
    float scX = 400.f / smData.canvasW;
    float scY = 240.f / smData.canvasH;
    spritemapScale = (scX < scY) ? scX : scY;

    return true;
}

void CppAnimate::addSpritemapAnim(const std::string& name,
                                   const std::string& symbolName,
                                   const std::vector<int>& indices,
                                   float fps, bool loop) {
    SmAnimData d;
    d.symbolName = symbolName;
    d.indices = indices;

    bool foundAnim = false;
    for (const auto& a : smData.animations) {
        if (a.name == symbolName) {
            d.symbolName = a.symbolName; // use the root symbol
            if (d.indices.empty()) {
                for (int i = 0; i < a.duration; i++) {
                    d.indices.push_back(a.startFrame + i);
                }
            } else {
                for (size_t i = 0; i < d.indices.size(); i++) {
                    d.indices[i] = a.startFrame + d.indices[i];
                }
            }
            foundAnim = true;
            break;
        }
    }
    if (!foundAnim && d.indices.empty()) {
        auto sit = smData.symbols.find(symbolName);
        int dur = (sit != smData.symbols.end()) ? sit->second.duration() : 1;
        for (int i = 0; i < dur; i++) {
            d.indices.push_back(i);
        }
    }

    d.fps  = (fps > 0.f) ? fps : (float)smData.frameRate;
    d.loop = loop;
    smAnims[name] = d;
}

std::vector<std::string> CppAnimate::getSpritemapAnimNames() const {
    std::vector<std::string> names;
    for (const auto& anim : smData.animations)
        names.push_back(anim.name);
    return names;
}

// play() and update() and draw() — patch existing Sparrow implementations
// to dispatch to spritemap mode first.

void CppAnimate::play(const std::string& name, bool forceRestart) {
    if (isSpritemapMode) {
        auto it = smAnims.find(name);
        std::string symName;
        float fps = (float)smData.frameRate;
        bool loop = true;
        int duration = 1;
        if (it != smAnims.end()) {
            symName    = it->second.symbolName;
            fps        = it->second.fps;
            loop       = it->second.loop;
            duration   = (int)it->second.indices.size();
        } else {
            // direct symbol name
            if (smData.symbols.find(name) == smData.symbols.end()) return;
            symName = name;
            auto sit = smData.symbols.find(symName);
            duration = (sit != smData.symbols.end()) ? sit->second.duration() : 1;
        }
        if (curAnim == name && !forceRestart && !animFinished) return;
        curAnim        = name;
        smFPS          = fps;
        smLoop         = loop;
        animFinished   = false;
        paused         = false;
        smFrameTimer   = 0.f;
        smLogicalFrame = 0;
        smTotalFrames  = duration;
        return;
    }
    // Original Sparrow path
    if (!hasAnim(name)) return;
    if (curAnim == name && !forceRestart && !animFinished) return;
    curAnim      = name;
    animFinished = false;
    paused       = false;
    curFrameIdx  = 0;
    frameTimer   = 0.0f;
}

void CppAnimate::setLoop(const std::string& name, bool loop) {
    if (smAnims.count(name)) smAnims[name].loop = loop;
    if (anims.count(name)) anims[name].loop = loop;
    if (curAnim == name) smLoop = loop;
}

void CppAnimate::update(float dt) {
    if (isSpritemapMode) {
        if (paused || animFinished || smTotalFrames <= 0) return;
        float frameDur = (smFPS > 0.f) ? (1.f / smFPS) : (1.f / 24.f);
        smFrameTimer += dt;
        while (smFrameTimer >= frameDur) {
            smFrameTimer -= frameDur;
            smLogicalFrame++;
            if (smLogicalFrame >= smTotalFrames) {
                if (smLoop) {
                    smLogicalFrame = 0;
                } else {
                    smLogicalFrame = smTotalFrames - 1;
                    animFinished   = true;
                    if (onAnimFinished) onAnimFinished(curAnim);
                    break;
                }
            }
        }
        return;
    }
    // Original XML path
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

void CppAnimate::draw(float x, float y, float depth, float sx, float sy, C2D_ImageTint* tint) {
    if (!visible) return;

    if (isSpritemapMode) {
        if (!smTex) return;
        std::string symName;
        auto it = smAnims.find(curAnim);
        if (it != smAnims.end()) {
            symName = it->second.symbolName;
        } else {
            if (smData.symbols.find(curAnim) == smData.symbols.end()) return;
            symName = curAnim;
        }
        // Root matrix: canvas-center → (x, y), with overall scale
        float finalScaleX = spritemapScale * scaleX * sx;
        float finalScaleY = spritemapScale * scaleY * sy;
        float canvasCX   = smData.canvasW * 0.5f;
        float canvasCY   = smData.canvasH * 0.5f;
        float rad = angle * (3.14159265f / 180.f);
        float cosA = cosf(rad);
        float sinA = sinf(rad);

        float flipScaleX = finalScaleX * (flipX ? -1.f : 1.f);
        float flipScaleY = finalScaleY * (flipY ? -1.f : 1.f);

        AffineMatrix rootMX;
        rootMX.a  = flipScaleX * cosA;
        rootMX.b  = flipScaleX * sinA;
        rootMX.c  = flipScaleY * (-sinA);
        rootMX.d  = flipScaleY * cosA;
        rootMX.tx = -canvasCX * rootMX.a - canvasCY * rootMX.c + x;
        rootMX.ty = -canvasCX * rootMX.b - canvasCY * rootMX.d + y;
        int drawFrame = smLogicalFrame;
        if (it != smAnims.end() && smLogicalFrame >= 0 && smLogicalFrame < (int)it->second.indices.size()) {
            drawFrame = it->second.indices[smLogicalFrame];
        }
        drawSymbol(symName, drawFrame, rootMX, depth, 0);
        return;
    }

    // Original XML path
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
    if (alpha < 1.0f && !tint) { C2D_AlphaImageTint(&alphaTint, alpha); usedTint = &alphaTint; }
    C3D_TexSetFilter(f->tex, antialiasing ? GPU_LINEAR : GPU_NEAREST, antialiasing ? GPU_LINEAR : GPU_NEAREST);
    drawFrameAt(*f, finalX, finalY, depth, usedTint, finalSX, finalSY);
}

// ── Affine matrix helpers ─────────────────────────────────────────────────────

AffineMatrix CppAnimate::compose(const AffineMatrix& p, const AffineMatrix& c) {
    AffineMatrix r;
    r.a  = c.a * p.a  + c.b * p.c;
    r.b  = c.a * p.b  + c.b * p.d;
    r.c  = c.c * p.a  + c.d * p.c;
    r.d  = c.c * p.b  + c.d * p.d;
    r.tx = c.tx * p.a + c.ty * p.c + p.tx;
    r.ty = c.tx * p.b + c.ty * p.d + p.ty;
    return r;
}

AffineMatrix CppAnimate::applyPivot(const AffineMatrix& mx, const Pivot& trp) {
    return mx;
}

// ── Recursive symbol draw ─────────────────────────────────────────────────────

void CppAnimate::drawSymbol(const std::string& symbolName,
                             int logicalFrame,
                             const AffineMatrix& parentMX,
                             float depth,
                             int recursionDepth) const {
    if (recursionDepth > 8) return;
    auto sit = smData.symbols.find(symbolName);
    if (sit == smData.symbols.end()) return;
    const SpritemapSymbol& sym = sit->second;
    int totalDur = sym.duration();
    if (totalDur <= 0) return;
    int frame = logicalFrame % totalDur;

    // Layers: last in array = bottom-most (drawn first)
    for (int li = (int)sym.layers.size() - 1; li >= 0; li--) {
        const SpritemapKeyframe* kf = sym.getKeyframe(li, frame);
        if (!kf) continue;
        for (const auto& elem : kf->elements) {
            if (elem.isAtlasSprite) {
                AffineMatrix composed = compose(parentMX, elem.asi.mx);
                drawPiece(elem.asi.pieceName, composed, depth);
            } else {
                const SymbolInstance& si = elem.si;
                auto childIt = smData.symbols.find(si.symbolName);
                if (childIt == smData.symbols.end()) continue;
                int childTotal = childIt->second.duration();
                if (childTotal <= 0) continue;

                int childFrame;
                if (si.loopType == "SF") {
                    childFrame = si.firstFrame;
                } else if (si.loopType == "PO") {
                    childFrame = si.firstFrame + logicalFrame;
                    if (childFrame >= childTotal) childFrame = childTotal - 1;
                } else {
                    childFrame = (si.firstFrame + logicalFrame) % childTotal;
                }

                AffineMatrix childMX  = applyPivot(si.mx, si.trp);
                AffineMatrix composed = compose(parentMX, childMX);
                drawSymbol(si.symbolName, childFrame, composed, depth, recursionDepth + 1);
            }
        }
    }
}

// ── Draw one atlas piece with an affine matrix ────────────────────────────────

void CppAnimate::drawPiece(const std::string& pieceName,
                            const AffineMatrix& mx,
                            float depth) const {
    if (!smTex) return;
    auto pit = smData.atlas.find(pieceName);
    if (pit == smData.atlas.end()) return;
    const SpritemapPiece& piece = pit->second;

    static Tex3DS_SubTexture defaultSubtex;
    defaultSubtex.width = (u16)smTex->width;
    defaultSubtex.height = (u16)smTex->height;
    defaultSubtex.left = 0.0f;
    defaultSubtex.top = 0.0f;
    defaultSubtex.right = 1.0f;
    defaultSubtex.bottom = 1.0f;

    const Tex3DS_SubTexture* sub = smSubtex ? smSubtex : &defaultSubtex;

    float rw = sub->right - sub->left;
    float rh = sub->bottom - sub->top;
    float refW = (float)sub->width;
    float refH = (float)sub->height;

    Tex3DS_SubTexture uv;
    uv.width  = (u16)piece.w;
    uv.height = (u16)piece.h;
    uv.left   = sub->left + ((float)piece.x * rw / refW);
    uv.top    = sub->top  + ((float)piece.y * rh / refH);
    uv.right  = sub->left + ((float)(piece.x + piece.w) * rw / refW);
    uv.bottom = sub->top  + ((float)(piece.y + piece.h) * rh / refH);

    float A = mx.a;
    float B = mx.b;
    float C = mx.c;
    float D = mx.d;

    if (piece.rotated) {
        A = -mx.c;
        B = -mx.d;
        C = mx.a;
        D = mx.b;
    }

    float scaleYf = sqrtf(C * C + D * D) * spritemapTextureScale;
    float angle   = atan2f(-C, D);
    float cosA    = cosf(angle);
    float sinA    = sinf(angle);
    float rawScaleX = (A * cosA + B * sinA) * spritemapTextureScale;
    float scaleXf = rawScaleX;

    // Bypass Citro2D negative scale bugs by swapping texture coordinates
    if (scaleXf < 0.0f) {
        scaleXf = -scaleXf;
        std::swap(uv.left, uv.right);
    }
    if (scaleYf < 0.0f) {
        scaleYf = -scaleYf;
        std::swap(uv.top, uv.bottom);
    }

    C2D_Image img = { smTex, &uv };

    // Local unrotated dimensions
    float localW = (piece.rotated ? (float)piece.h : (float)piece.w) * spritemapTextureScale;
    float localH = (piece.rotated ? (float)piece.w : (float)piece.h) * spritemapTextureScale;

    float cx = mx.a * (localW * 0.5f) + mx.c * (localH * 0.5f) + mx.tx;
    float cy = mx.b * (localW * 0.5f) + mx.d * (localH * 0.5f) + mx.ty;

    GPU_TEXTURE_FILTER_PARAM filt = antialiasing ? GPU_LINEAR : GPU_NEAREST;
    C3D_TexSetFilter(smTex, filt, filt);

    C2D_ImageTint alphaTint;
    const C2D_ImageTint* usedTint = nullptr;
    if (alpha < 1.0f) { C2D_AlphaImageTint(&alphaTint, alpha); usedTint = &alphaTint; }

    float shearY = 0.0f;
    float divisor = rawScaleX;
    if (divisor != 0.0f) {
        shearY = (-A * sinA + B * cosA) / divisor;
    }

    C3D_Mtx originalMtx;
    bool hasShear = (fabsf(shearY) > 0.001f);
    if (hasShear) {
        C2D_ViewSave(&originalMtx);
        C2D_ViewTranslate(cx, cy);
        C2D_ViewRotate(angle);
        C2D_ViewShear(0.0f, shearY);
        C2D_ViewRotate(-angle);
        C2D_ViewTranslate(-cx, -cy);
    }

    C2D_DrawImageAtRotated(img, cx, cy, depth, angle,
                           usedTint, scaleXf, scaleYf);

    if (hasShear) {
        C2D_ViewRestore(&originalMtx);
    }
}
