#include "Stage.hpp"
#include "../states/PlayState.hpp"
#include <jansson.h>
#include <cstdio>
#include <cmath>

static float getJsonFloat(json_t* obj, const char* key, float defaultValue) {
    json_t* val = json_object_get(obj, key);
    if (json_is_number(val)) return (float)json_number_value(val);
    return defaultValue;
}

static bool getJsonBool(json_t* obj, const char* key, bool defaultValue) {
    json_t* val = json_object_get(obj, key);
    if (json_is_boolean(val)) return json_boolean_value(val);
    return defaultValue;
}

Stage::Stage(const std::string& path) {
    loadFromJson(path);
}

Stage::~Stage() {
    // Textures are owned by SpritesheetCache — do NOT free them here.
    sprites.clear();
}

void Stage::loadFromJson(const std::string& path) {
    json_t* root = nullptr;
    json_error_t error;

    if (!Paths::fileExists(path)) {
        loadFromJson(Paths::stageJson("StageTest"));
        return;
    }

    root = json_load_file(path.c_str(), 0, &error);
    if (!root) return;

    defaultZoom = getJsonFloat(root, "defaultZoom", defaultZoom);
    cameraSpeed = getJsonFloat(root, "camera_speed", cameraSpeed);

    // Coord parsing helper
    auto parseCoords = [&](const char* key, float& rx, float& ry) -> bool {
        json_t* arr = json_object_get(root, key);
        if (json_is_array(arr) && json_array_size(arr) >= 2) {
            rx = (float)json_number_value(json_array_get(arr, 0));
            ry = (float)json_number_value(json_array_get(arr, 1));
            return true;
        }
        return false;
    };

    parseCoords("boyfriend", bfX, bfY);
    parseCoords("opponent", dadX, dadY);
    parseCoords("girlfriend", gfX, gfY);

    if (!parseCoords("camera_boyfriend", bfCamX, bfCamY)) parseCoords("boyfriend_camera", bfCamX, bfCamY);
    if (!parseCoords("camera_opponent", dadCamX, dadCamY)) parseCoords("opponent_camera", dadCamX, dadCamY);
    if (!parseCoords("camera_girlfriend", gfCamX, gfCamY)) parseCoords("girlfriend_camera", gfCamX, gfCamY);

    hideGirlfriend = getJsonBool(root, "hide_girlfriend", hideGirlfriend);
    hideOpponent = getJsonBool(root, "hide_opponent", hideOpponent);
    isPixelStage = getJsonBool(root, "isPixelStage", isPixelStage);

    json_t* jSprites = json_object_get(root, "sprites");
    if (json_is_array(jSprites)) {
        size_t index;
        json_t* val;
        json_array_foreach(jSprites, index, val) {
            StageSprite s;
            json_t* jImg = json_object_get(val, "image");
            if (json_is_string(jImg)) {
                s.name = json_string_value(jImg);
            }
            s.x = getJsonFloat(val, "x", 0.0f);
            s.y = getJsonFloat(val, "y", 0.0f);
            s.scrollX = getJsonFloat(val, "scrollX", 1.0f);
            s.scrollY = getJsonFloat(val, "scrollY", 1.0f);
            s.scale = getJsonFloat(val, "scale", 1.0f);
            s.scaleX = getJsonFloat(val, "scaleX", s.scale);
            s.scaleY = getJsonFloat(val, "scaleY", s.scale);
            s.front = getJsonBool(val, "front", false);

            json_t* jAlpha = json_object_get(val, "alpha");
            if (json_is_number(jAlpha)) {
                s.alpha = (float)json_number_value(jAlpha);
            } else if (json_is_boolean(jAlpha)) {
                s.alpha = json_boolean_value(jAlpha) ? 1.0f : 0.0f;
            } else {
                s.alpha = 1.0f;
            }

            // Load texture using SpritesheetCache so identical images share one copy in RAM
            std::string imgPath = "stages/" + s.name;
            auto* cs = SpritesheetCache::get().load(imgPath);
            if (cs && !cs->frames.empty()) {
                s.sheet = cs->sheet;
                s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
                if (s.img.tex) {
                    C3D_TexSetFilter(s.img.tex, GPU_NEAREST, GPU_NEAREST);
                    sprites.push_back(s);
                }
            }
        }
    }

    json_decref(root);
}

void Stage::draw(float camX, float camY, float camZoom, bool frontLayer, float shakeX, float shakeY) {
    constexpr float screenScale = 240.0f / 720.0f;
    float baseDepth = frontLayer ? 0.48f : 0.10f; 

    int renderedCount = 0;
    for (auto& s : sprites) {
        if (s.front != frontLayer) continue;
        if (!s.visible || s.alpha <= 0.0f) continue;

        // Parallax math relative to center
        float totalDepth3D = s.depth3D + (PlayState::instance ? PlayState::instance->camGame3DDepth : 0.0f);
        float offset3D = get3DOffset(totalDepth3D);
        float drawX = ((s.x - (camX * s.scrollX)) * camZoom * screenScale) + (ScreenWidthTop * 0.5f) + shakeX + offset3D;
        float drawY = ((s.y - (camY * s.scrollY)) * camZoom * screenScale) + (ScreenHeight * 0.5f) + shakeY;
        
        float drawScaleX = s.scaleX * screenScale * camZoom;
        float drawScaleY = s.scaleY * screenScale * camZoom;

        float frameW = 0.0f;
        float frameH = 0.0f;
        C2D_Image img = s.img;
        bool frameRotated = false;

        if (s.animated && s.currentAnim && !s.currentAnim->indices.empty()) {
            int frameIdx = s.currentAnim->indices[(int)s.curFrame];
            const std::vector<Frame>& useFrames = s.isExternalAnim ? s.externalFrames : s.frames;
            if (frameIdx >= 0 && frameIdx < (int)useFrames.size()) {
                const Frame& curFrame = useFrames[frameIdx];
                img.subtex = &curFrame.uv;
                img.tex = curFrame.tex;
                frameRotated = curFrame.rotated;
                frameW = curFrame.frameW;
                frameH = curFrame.frameH;

                // Apply offsets
                drawX -= (curFrame.frameX + s.currentAnim->offsetX) * drawScaleX;
                drawY -= (curFrame.frameY + s.currentAnim->offsetY) * drawScaleY;
            }
        } else {
            if (img.subtex) {
                frameW = img.subtex->width;
                frameH = img.subtex->height;
            }
        }

        // Scales from origin (center of frame)
        if (!PlayState::instance || !PlayState::instance->legacyPositioning) {
            float originX = frameW * 0.5f;
            float originY = frameH * 0.5f;
            drawX += originX * (1.0f - s.scaleX) * screenScale * camZoom;
            drawY += originY * (1.0f - s.scaleY) * screenScale * camZoom;
        }
        
        static Tex3DS_SubTexture defaultSubtex;
        if (img.subtex == nullptr) {
            defaultSubtex.width = img.tex ? img.tex->width : 0;
            defaultSubtex.height = img.tex ? img.tex->height : 0;
            defaultSubtex.left = 0.0f;
            defaultSubtex.top = 0.0f;
            defaultSubtex.right = 1.0f;
            defaultSubtex.bottom = 1.0f;
            img.subtex = &defaultSubtex;
        }

        float drawDepth = baseDepth + ((float)renderedCount * 0.002f);

        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        if (s.alpha < 1.0f) {
            C2D_AlphaImageTint(&tint, s.alpha);
            tintPtr = &tint;
        }

        if (frameRotated) {
            // Sprite stored 90° CW in atlas: compensate with -90° (CCW) rotation.
            constexpr float angleRad = -1.57079632679f;
            float cx = drawX + img.subtex->width  * drawScaleX * 0.5f;
            float cy = drawY + img.subtex->height * drawScaleY * 0.5f;
            C2D_DrawImageAtRotated(img, cx, cy, drawDepth, angleRad, tintPtr, drawScaleX, drawScaleY);
        } else {
            C2D_DrawImageAt(img, drawX, drawY, drawDepth, tintPtr, drawScaleX, drawScaleY);
        }
        renderedCount++;
    }
}
