#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include "../states/PlayState.hpp"
#include <string.h>
#include <math.h>
#include <algorithm>

ShaderManager::ShaderManager() {
}

ShaderManager::~ShaderManager() {
    cleanup();
}

void ShaderManager::init() {
    int h_game = (extendedCameras.count("camGame") > 0) ? 512 : 256;
    int h_hud  = (extendedCameras.count("camHUD") > 0) ? 512 : 256;
    int h_oth  = (extendedCameras.count("camOther") > 0) ? 512 : 256;
    int h_bot  = (extendedCameras.count("camBottom") > 0) ? 512 : 256;

    if (!targets["camGame"].active) targets["camGame"].init(512, h_game);
    if (!targets["camHUD"].active) targets["camHUD"].init(512, h_hud);
    if (!targets["camOther"].active) targets["camOther"].init(512, h_oth);
    if (!targets["camBottom"].active) targets["camBottom"].init(512, h_bot);

    bool anyExtended = !extendedCameras.empty();
    int helperHeight = anyExtended ? 512 : 256;
    if (helperRT.active && helperRT.tex.height != helperHeight) {
        helperRT.cleanup();
    }
    if (helperRT2.active && helperRT2.tex.height != helperHeight) {
        helperRT2.cleanup();
    }
    if (!helperRT.active)  helperRT.init(512, helperHeight);
    if (!helperRT2.active) helperRT2.init(512, helperHeight);
}

void ShaderManager::cleanup() {
    for (auto& pair : targets) {
        pair.second.cleanup();
    }
    targets.clear();
    helperRT.cleanup();
    helperRT2.cleanup();
    activeShaders.clear();
    extendedCameras.clear();
}

void ShaderManager::setCameraExtended(const std::string& camera, bool extended) {
    if (extended) {
        if (extendedCameras.count(camera) == 0) {
            extendedCameras.insert(camera);
            auto& rt = targets[camera];
            if (rt.active) {
                rt.cleanup();
                rt.init(512, 512);
            }
            if (helperRT.active && helperRT.tex.height != 512) {
                helperRT.cleanup();
                helperRT.init(512, 512);
            }
            if (helperRT2.active && helperRT2.tex.height != 512) {
                helperRT2.cleanup();
                helperRT2.init(512, 512);
            }
        }
    } else {
        if (extendedCameras.count(camera) > 0) {
            extendedCameras.erase(camera);
            auto& rt = targets[camera];
            if (rt.active) {
                rt.cleanup();
                rt.init(512, 256);
            }
        }
    }
}

bool ShaderManager::isCameraExtended(const std::string& camera) const {
    return extendedCameras.count(camera) > 0;
}

void ShaderManager::RT::init(int w, int h) {
    if (active) return;
    
    if (!C3D_TexInitVRAM(&tex, w, h, GPU_RGBA8)) {
        if (!C3D_TexInit(&tex, w, h, GPU_RGBA8)) {
            printf("\x1b[16;1HERROR: Failed to alloc Shader RT %dx%d\x1b[K\n", w, h);
            return;
        }
    }
    C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_NEAREST);
    C3D_TexSetWrap(&tex, GPU_REPEAT, GPU_REPEAT);

    target = C3D_RenderTargetCreateFromTex(&tex, GPU_TEXFACE_2D, 0, -1);
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x00000000, 0); // Clear with transparent
    
    img.tex = &tex;
    img.subtex = new Tex3DS_SubTexture();
    origSubtex = const_cast<Tex3DS_SubTexture*>(img.subtex);
    origSubtex->width = (w == 512) ? 400 : w; 
    origSubtex->height = (h == 512) ? 480 : 240;
    origSubtex->left = 0.0f;
    origSubtex->top = 1.0f;
    origSubtex->right = origSubtex->width / (float)w;
    origSubtex->bottom = 1.0f - (origSubtex->height / (float)h);
    
    active = true;
}

void ShaderManager::RT::cleanup() {
    if (active) {
        if (target) C3D_RenderTargetDelete(target);
        C3D_TexDelete(&tex);
        if (origSubtex) delete origSubtex;
        img.subtex = nullptr;
        origSubtex = nullptr;
        active = false;
        target = nullptr;
    }
}

void ShaderManager::setCameraShader(const std::string& camera, const std::string& shaderName, float v1, float v2, float v3) {
    ShaderParams params;
    params.name   = shaderName;
    params.value1 = v1;
    params.value2 = v2;
    params.value3 = v3;
    
    auto& stack = activeShaders[camera];
    // If this shader name already exists in the stack, update its params in-place
    for (auto& p : stack) {
        if (p.name == shaderName) {
            p = params;
            return;
        }
    }
    // Otherwise append to the stack
    stack.push_back(params);
}

void ShaderManager::clearAllShaders() {
    activeShaders.clear();
}

void ShaderManager::setShaderFloat(const std::string& camera, int index, float value) {
    auto it = activeShaders.find(camera);
    if (it != activeShaders.end() && !it->second.empty()) {
        // Modifies the FIRST shader in stack (backward compat)
        auto& p = it->second[0];
        if (index == 1) p.value1 = value;
        else if (index == 2) p.value2 = value;
        else if (index == 3) p.value3 = value;
    }
}

void ShaderManager::setShaderParam(const std::string& camera, const std::string& shaderName, int index, float value) {
    auto it = activeShaders.find(camera);
    if (it == activeShaders.end()) return;
    for (auto& p : it->second) {
        if (p.name == shaderName) {
            if (index == 1) p.value1 = value;
            else if (index == 2) p.value2 = value;
            else if (index == 3) p.value3 = value;
            return;
        }
    }
}

void ShaderManager::removeCameraShader(const std::string& camera) {
    activeShaders.erase(camera);
}

void ShaderManager::removeCameraShaders(const std::string& camera, const std::vector<std::string>& names) {
    auto it = activeShaders.find(camera);
    if (it == activeShaders.end()) return;
    auto& stack = it->second;
    stack.erase(
        std::remove_if(stack.begin(), stack.end(), [&](const ShaderParams& p) {
            for (const auto& n : names) {
                if (p.name == n) return true;
            }
            return false;
        }),
        stack.end()
    );
    // If stack is now empty, remove the camera entry entirely
    if (stack.empty()) activeShaders.erase(it);
}

bool ShaderManager::beginCamera(const std::string& camera, C3D_RenderTarget* fallbackTarget) {
    auto it = activeShaders.find(camera);
    bool hasShaders = (it != activeShaders.end() && !it->second.empty());
    bool isExtended = isCameraExtended(camera);

    bool hasTransforms = false;
    if (PlayState::instance) {
        if (camera == "camGame") {
            hasTransforms = (PlayState::instance->camX_offset != 0.0f ||
                             PlayState::instance->camY_offset != 0.0f ||
                             PlayState::instance->camScaleX != 1.0f ||
                             PlayState::instance->camScaleY != 1.0f ||
                             PlayState::instance->camAngle != 0.0f ||
                             PlayState::instance->camAlpha != 1.0f ||
                             !PlayState::instance->camVisible ||
                             PlayState::instance->camFlipX ||
                             PlayState::instance->camFlipY);
        } else if (camera == "camHUD") {
            hasTransforms = (PlayState::instance->hudX_offset != 0.0f ||
                             PlayState::instance->hudY_offset != 0.0f ||
                             PlayState::instance->hudScaleX != 1.0f ||
                             PlayState::instance->hudScaleY != 1.0f ||
                             PlayState::instance->hudAngle != 0.0f ||
                             PlayState::instance->hudAlpha != 1.0f ||
                             !PlayState::instance->hudVisible ||
                             PlayState::instance->hudFlipX ||
                             PlayState::instance->hudFlipY);
        } else if (camera == "camOther") {
            hasTransforms = (PlayState::instance->otherX_offset != 0.0f ||
                             PlayState::instance->otherY_offset != 0.0f ||
                             PlayState::instance->otherScaleX != 1.0f ||
                             PlayState::instance->otherScaleY != 1.0f ||
                             PlayState::instance->otherAngle != 0.0f ||
                             PlayState::instance->otherAlpha != 1.0f ||
                             !PlayState::instance->otherVisible ||
                             PlayState::instance->otherFlipX ||
                             PlayState::instance->otherFlipY);
        }
    }

    auto applyCameraTransforms = [&](const std::string& cam) {
        C2D_ViewReset();
        float centerX = 400.0f / 2.0f;
        float centerY = 240.0f / 2.0f;
        if (cam == "camBottom") {
            centerX = 320.0f / 2.0f;
        }
        float tx = 0.0f, ty = 0.0f, tz = 1.0f, tsx = 1.0f, tsy = 1.0f, tang = 0.0f;
        bool fx = false, fy = false;
        if (PlayState::instance) {
            if (cam == "camGame") {
                tx = PlayState::instance->camX_offset;
                ty = PlayState::instance->camY_offset;
                tsx = PlayState::instance->camScaleX;
                tsy = PlayState::instance->camScaleY;
                tang = PlayState::instance->camAngle;
                fx = PlayState::instance->camFlipX;
                fy = PlayState::instance->camFlipY;
            } else if (cam == "camHUD") {
                tx = PlayState::instance->hudX_offset;
                ty = PlayState::instance->hudY_offset;
                tsx = PlayState::instance->hudScaleX;
                tsy = PlayState::instance->hudScaleY;
                tang = PlayState::instance->hudAngle;
                fx = PlayState::instance->hudFlipX;
                fy = PlayState::instance->hudFlipY;
            } else if (cam == "camOther") {
                tx = PlayState::instance->otherX_offset;
                ty = PlayState::instance->otherY_offset;
                tsx = PlayState::instance->otherScaleX;
                tsy = PlayState::instance->otherScaleY;
                tang = PlayState::instance->otherAngle;
                fx = PlayState::instance->otherFlipX;
                fy = PlayState::instance->otherFlipY;
            }
        }
        C2D_ViewTranslate(centerX, centerY);
        C2D_ViewRotateDegrees(tang);
        C2D_ViewScale(tsx * tz * (fx ? -1.0f : 1.0f), tsy * tz * (fy ? -1.0f : 1.0f));
        C2D_ViewTranslate(-centerX + tx, -centerY + ty);
    };

    if (hasShaders || isExtended || hasTransforms) {
        auto& rt = targets[camera];
        if (rt.active && rt.target) {
            C2D_SceneBegin(rt.target);
            C2D_TargetClear(rt.target, C2D_Color32(0, 0, 0, 0));
            // Correct alpha accumulation for offscreen targets (alpha source uses GPU_ONE)
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
            applyCameraTransforms(camera);
            return true;
        }
    }
    C2D_SceneBegin(fallbackTarget);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    applyCameraTransforms(camera);
    return false;
}

void ShaderManager::update(float dt) {
    // Accumulate scroll positions frame-by-frame
    for (auto& pair : activeShaders) {
        const std::string& cam = pair.first;
        for (auto& p : pair.second) {
            if (p.name == "scroll" || p.name == "infinite_scroll" || p.name == "infinite scroll") {
                scrollAccumulators[cam].first  += p.value1 * dt;
                scrollAccumulators[cam].second += p.value2 * dt;
            }
        }
    }
}

void ShaderManager::presentCamera(const std::string& camName, C3D_RenderTarget* dest, bool isBottomPart) {
    auto it = activeShaders.find(camName);
    bool hasShaders = (it != activeShaders.end() && !it->second.empty());
    bool isExtended = isCameraExtended(camName);
    
    // Get camera transformations
    float x = 0.0f;
    float y = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float angle = 0.0f;
    float alpha = 1.0f;
    bool visible = true;
    bool flipX = false;
    bool flipY = false;

    if (PlayState::instance) {
        if (camName == "camGame") {
            x = PlayState::instance->camX_offset;
            y = PlayState::instance->camY_offset;
            scaleX = PlayState::instance->camScaleX;
            scaleY = PlayState::instance->camScaleY;
            angle = PlayState::instance->camAngle;
            alpha = PlayState::instance->camAlpha;
            visible = PlayState::instance->camVisible;
            flipX = PlayState::instance->camFlipX;
            flipY = PlayState::instance->camFlipY;
        } else if (camName == "camHUD") {
            x = PlayState::instance->hudX_offset;
            y = PlayState::instance->hudY_offset;
            scaleX = PlayState::instance->hudScaleX;
            scaleY = PlayState::instance->hudScaleY;
            angle = PlayState::instance->hudAngle;
            alpha = PlayState::instance->hudAlpha;
            visible = PlayState::instance->hudVisible;
            flipX = PlayState::instance->hudFlipX;
            flipY = PlayState::instance->hudFlipY;
        } else if (camName == "camOther") {
            x = PlayState::instance->otherX_offset;
            y = PlayState::instance->otherY_offset;
            scaleX = PlayState::instance->otherScaleX;
            scaleY = PlayState::instance->otherScaleY;
            angle = PlayState::instance->otherAngle;
            alpha = PlayState::instance->otherAlpha;
            visible = PlayState::instance->otherVisible;
            flipX = PlayState::instance->otherFlipX;
            flipY = PlayState::instance->otherFlipY;
        }
    }

    bool hasTransforms = (x != 0.0f || y != 0.0f || scaleX != 1.0f || scaleY != 1.0f || angle != 0.0f || alpha != 1.0f || !visible || flipX || flipY);

    if (!visible) return;
    if (!hasShaders && !isExtended && !hasTransforms) return;
    
    const RT& cameraRT = targets[camName];
    if (!cameraRT.active) return;
    
    Tex3DS_SubTexture tempSub;
    if (isExtended) {
        if (isBottomPart) {
            tempSub.width = 320;
            tempSub.height = 240;
            tempSub.left = 40.0f / 512.0f;
            tempSub.top = 1.0f - (272.0f / 512.0f);
            tempSub.right = 360.0f / 512.0f;
            tempSub.bottom = 1.0f - (512.0f / 512.0f);
        } else {
            tempSub.width = 400;
            tempSub.height = 240;
            tempSub.left = 0.0f;
            tempSub.top = 1.0f;
            tempSub.right = 400.0f / 512.0f;
            tempSub.bottom = 1.0f - (240.0f / 512.0f);
        }
        const_cast<RT&>(cameraRT).img.subtex = &tempSub;
        if (helperRT.active) const_cast<RT&>(helperRT).img.subtex = &tempSub;
        if (helperRT2.active) const_cast<RT&>(helperRT2).img.subtex = &tempSub;
    }
    
    const RT* finalSrc = &cameraRT;
    if (hasShaders) {
        const ShaderStack& stack = it->second;
        if (stack.size() == 1) {
            C2D_SceneBegin(helperRT.target);
            C2D_TargetClear(helperRT.target, C2D_Color32(0, 0, 0, 0));
            C2D_ViewReset();
            drawShaderEffect(camName, cameraRT, helperRT.target, stack[0], 0, 0, 1.0f, 1.0f);
            C2D_Flush();
            finalSrc = &helperRT;
        } else {
            const RT* currentSrc = &cameraRT;
            for (size_t i = 0; i < stack.size(); i++) {
                RT& passDestRT = (i % 2 == 0) ? helperRT2 : helperRT;
                C2D_SceneBegin(passDestRT.target);
                C2D_TargetClear(passDestRT.target, C2D_Color32(0, 0, 0, 0));
                C2D_ViewReset();
                drawShaderEffect(camName, *currentSrc, passDestRT.target, stack[i], 0, 0, 1.0f, 1.0f);
                C2D_Flush();
                currentSrc = &passDestRT;
            }
            finalSrc = (stack.size() % 2 == 0) ? &helperRT2 : &helperRT;
        }
    }

    C2D_SceneBegin(dest);
    
    C2D_ImageTint alphaTint;
    C2D_ImageTint* tintPtr = nullptr;
    if (alpha < 1.0f) {
        C2D_PlainImageTint(&alphaTint, C2D_Color32(255, 255, 255, (u8)(alpha * 255)), 1.0f);
        tintPtr = &alphaTint;
    }

    float screenWidth = isBottomPart ? 320.0f : 400.0f;
    float screenHeight = 240.0f;
    
    float drawX = screenWidth / 2.0f;
    float drawY = screenHeight / 2.0f;
    float drawAngle = 0.0f;
    float drawScaleX = 1.0f;
    float drawScaleY = 1.0f;

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    
    C2D_ViewReset();
    C2D_DrawImageAtRotated(finalSrc->img, drawX, drawY, 0.0f, drawAngle, tintPtr, drawScaleX, drawScaleY);
    
    C2D_Flush();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    if (isExtended) {
        const_cast<RT&>(cameraRT).img.subtex = cameraRT.origSubtex;
        if (helperRT.active) const_cast<RT&>(helperRT).img.subtex = helperRT.origSubtex;
        if (helperRT2.active) const_cast<RT&>(helperRT2).img.subtex = helperRT2.origSubtex;
    }
}

void ShaderManager::endCamera(const std::string& camera, C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);

    presentCamera(camera, top, false);
    if (isCameraExtended(camera) && bottom) {
        presentCamera(camera, bottom, true);
    }

    // Restore depth test for normal rendering
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C2D_Flush();
    C2D_SceneBegin(top); // Return focus to top screen
    C2D_ViewReset();
}

void ShaderManager::drawShaderEffect(const std::string& camera, const RT& rt, C3D_RenderTarget* dest, const ShaderParams& params, float x, float y, float scaleX, float scaleY) {
    if (!rt.active) return;
    
    C2D_Flush();
    // RT has premultiplied RGB — GPU_ONE composites correctly, GPU_SRC_ALPHA would double-premultiply.
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    
    if (params.name == "grayscale") {
        drawTevTint(rt, dest, C2D_TintLuma, C2D_Color32(255, 255, 255, 255), params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "invert") {
        drawTevTint(rt, dest, C2D_TintOneMinusAdd, C2D_Color32(0, 0, 0, 255), params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "sepia") {
        drawTevTint(rt, dest, C2D_TintLuma, C2D_Color32(255, 230, 180, 255), params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "gameboy") {
        drawTevTint(rt, dest, C2D_TintLuma, C2D_Color32(155, 188, 15, 255), params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "virtualboy") {
        drawTevTint(rt, dest, C2D_TintLuma, C2D_Color32(255, 0, 0, 255), params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "saturation") {
        drawSaturation(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "chromatic") {
        drawChromatic(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "wave") {
        drawWave(rt, dest, params.value1, params.value2, params.value3, x, y, scaleX, scaleY);
    } else if (params.name == "glitch" || params.name == "skew") {
        drawGlitchSkew(rt, dest, params.value1, params.value2, params.value3, x, y, scaleX, scaleY);
    } else if (params.name == "crt") {
        drawCRT(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "blur") {
        drawBlur(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "pixelate") {
        drawPixelate(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "tile" || params.name == "tiling" || params.name == "kaleidoscope") {
        drawTiling(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "tint") {
        drawTevTint(rt, dest, C2D_TintMult, C2D_Color32((u8)params.value1, (u8)params.value2, (u8)params.value3, 255), 1.0f, x, y, scaleX, scaleY);
    } else if (params.name == "vignette") {
        drawVignette(rt, dest, params.value1, params.value2, x, y, scaleX, scaleY);
    } else if (params.name == "mirror") {
        drawMirror(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "scanline_roll" || params.name == "scanline roll" || params.name == "scanlineroll") {
        drawScanlineRoll(rt, dest, params.value1, params.value2, x, y, scaleX, scaleY);
    } else if (params.name == "vhs") {
        drawVHS(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "color_depth" || params.name == "color depth" || params.name == "colordepth") {
        drawColorDepth(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else if (params.name == "scroll" || params.name == "infinite_scroll" || params.name == "infinite scroll" || params.name == "infinitescroll") {
        drawScroll(camera, rt, dest, params.value1, params.value2, params.value3, x, y, scaleX, scaleY);
    } else if (params.name == "drugs") {
        drawDrugs(rt, dest, params.value1, params.value2, params.value3, x, y, scaleX, scaleY);
    } else if (params.name == "bw" || params.name == "black_white" || params.name == "blackwhite" || params.name == "black white") {
        drawBW(rt, dest, params.value1, x, y, scaleX, scaleY);
    } else {
        // Unknown shader name: just draw the source image unmodified
        C2D_Flush();
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
    }
    
    C2D_Flush();
    // Restore default blend for direct-to-screen draws.
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}
