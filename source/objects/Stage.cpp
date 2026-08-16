#include "Stage.hpp"
#include "../states/PlayState.hpp"
#include <jansson.h>
#include <stdio.h>

Stage::Stage(const std::string& path) {
    loadFromJson(path);
}

Stage::~Stage() {
    // Textures owned by SpritesheetCache — do NOT free them here.
    sprites.clear();
}

void Stage::loadFromJson(const std::string& path) {
    json_t *root;
    json_error_t error;

    if (!Paths::fileExists(path)) {
        loadFromJson(Paths::stageJson("StageTest"));
        return;
    }

    root = json_load_file(path.c_str(), 0, &error);
    if (!root) {
        return;
    }


    json_t *jZoom = json_object_get(root, "defaultZoom");
    if (json_is_number(jZoom)) defaultZoom = (float)json_number_value(jZoom);

    json_t *jSpeed = json_object_get(root, "camera_speed");
    if (json_is_number(jSpeed)) cameraSpeed = (float)json_number_value(jSpeed);

    // Coord parsing macro
    auto parseCoords = [&](const char* key, float& rx, float& ry) -> bool {
        json_t *arr = json_object_get(root, key);
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

    if (!parseCoords("camera_boyfriend", bfCamX, bfCamY))  parseCoords("boyfriend_camera",  bfCamX, bfCamY);
    if (!parseCoords("camera_opponent",  dadCamX, dadCamY)) parseCoords("opponent_camera",   dadCamX, dadCamY);
    if (!parseCoords("camera_girlfriend",gfCamX, gfCamY))  parseCoords("girlfriend_camera", gfCamX, gfCamY);

    json_t *jHideGF = json_object_get(root, "hide_girlfriend");
    if (json_is_boolean(jHideGF)) hideGirlfriend = json_boolean_value(jHideGF);

    json_t *jHideOpp = json_object_get(root, "hide_opponent");
    if (json_is_boolean(jHideOpp)) hideOpponent = json_boolean_value(jHideOpp);

    json_t *jSprites = json_object_get(root, "sprites");
    if (json_is_array(jSprites)) {
        size_t index; json_t *val;
        json_array_foreach(jSprites, index, val) {
            StageSprite s;
            s.name = json_string_value(json_object_get(val, "image"));
            s.x = json_is_number(json_object_get(val, "x")) ? (float)json_number_value(json_object_get(val, "x")) : 0.0f;
            s.y = json_is_number(json_object_get(val, "y")) ? (float)json_number_value(json_object_get(val, "y")) : 0.0f;
            s.scrollX = json_is_number(json_object_get(val, "scrollX")) ? (float)json_number_value(json_object_get(val, "scrollX")) : 1.0f;
            s.scrollY = json_is_number(json_object_get(val, "scrollY")) ? (float)json_number_value(json_object_get(val, "scrollY")) : 1.0f;
            s.scale = json_is_number(json_object_get(val, "scale")) ? (float)json_number_value(json_object_get(val, "scale")) : 1.0f;
            s.scaleX = json_is_number(json_object_get(val, "scaleX")) ? (float)json_number_value(json_object_get(val, "scaleX")) : s.scale;
            s.scaleY = json_is_number(json_object_get(val, "scaleY")) ? (float)json_number_value(json_object_get(val, "scaleY")) : s.scale;
            
            json_t *jFront = json_object_get(val, "front");
            s.front = json_is_boolean(jFront) ? json_boolean_value(jFront) : false;

            json_t *jAlpha = json_object_get(val, "alpha");
            s.alpha = json_is_boolean(jAlpha) ? json_boolean_value(jAlpha) : true;

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
    float screenScale = 240.0f / 720.0f;
    float baseDepth = frontLayer ? 0.48f : 0.10f; 

    int renderedCount = 0;
    for (auto& s : sprites) {
        if (s.front != frontLayer) continue;
        if (!s.visible || s.alpha <= 0.0f) continue;

        // Parallax math relative to center
        float drawX = ((s.x - (camX * s.scrollX)) * camZoom * screenScale) + (ScreenWidthTop / 2.0f) + shakeX;
        float drawY = ((s.y - (camY * s.scrollY)) * camZoom * screenScale) + (ScreenHeight / 2.0f) + shakeY;
        
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

        // Scales from the origin (center of the frame)
        if (!PlayState::instance || !PlayState::instance->legacyPositioning) {
            float originX = frameW / 2.0f;
            float originY = frameH / 2.0f;
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

        float drawDepth = baseDepth + (renderedCount * 0.002f);

        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        if (s.alpha < 1.0f) {
            C2D_AlphaImageTint(&tint, s.alpha);
            tintPtr = &tint;
        }

        if (frameRotated) {
            // Sprite stored 90° CW in atlas: compensate with -90° (CCW) rotation.
            float angleRad = -(3.14159265f / 2.0f);
            float cx = drawX + img.subtex->width  * drawScaleX / 2.0f;
            float cy = drawY + img.subtex->height * drawScaleY / 2.0f;
            C2D_DrawImageAtRotated(img, cx, cy, drawDepth, angleRad, tintPtr, drawScaleX, drawScaleY);
        } else {
            C2D_DrawImageAt(img, drawX, drawY, drawDepth, tintPtr, drawScaleX, drawScaleY);
        }
        renderedCount++;
    }
}
