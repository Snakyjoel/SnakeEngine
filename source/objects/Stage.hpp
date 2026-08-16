#pragma once
#include <string>
#include <vector>
#include <citro2d.h>
#include "../backend/SpritesheetCache.hpp"
#include <map>

struct StageSprite {
    std::string name;
    C2D_SpriteSheet sheet = nullptr;
    C2D_Image img = {nullptr, nullptr};
    float x = 0.0f;
    float y = 0.0f;
    float scrollX = 1.0f;
    float scrollY = 1.0f;
    float scale = 1.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    bool front = false;
    float alpha = 1.0f;
    bool visible = true;
    float angle = 0.0f;
    bool flipX = false;
    bool flipY = false;
    std::string camera = "camGame";
    void* vramData = nullptr;
    bool isGraphic = false;
    u32 graphicColor = 0xFFFFFFFF;
    float graphicWidth = 0.0f;
    float graphicHeight = 0.0f;
    bool antialiasing = true;
    int zOrder = 0;

    // Animation support
    bool animated = false;
    std::vector<Frame> frames;
    std::map<std::string, Animation> animations;
    Animation* currentAnim = nullptr;
    float curFrame = 0;
    float frameTimer = 0;
    bool animFinished = false;
    
    // External Anim Support
    bool isExternalAnim = false;
    std::vector<Frame> externalFrames;
    Animation externalAnimData;

    void update(float dt) {
        if (!animated || !currentAnim || currentAnim->indices.empty() || animFinished) return;
        frameTimer += dt * currentAnim->fps;
        while (frameTimer >= 1.0f && !animFinished) {
            frameTimer -= 1.0f;
            curFrame++;
            if (curFrame >= currentAnim->indices.size()) {
                if (currentAnim->loop) {
                    curFrame = 0;
                } else {
                    curFrame = currentAnim->indices.size() - 1;
                    animFinished = true;
                    frameTimer = 0.0f;
                }
            }
        }
    }

    void playAnim(const std::string& animName, bool force = false) {
        if (!force && currentAnim && currentAnim->name == animName && !animFinished) return;
        if (animations.count(animName)) {
            isExternalAnim = false;
            currentAnim = &animations[animName];
            curFrame = 0; frameTimer = 0; animFinished = false;
        }
    }

    void playAnimFES(const std::string& path, const std::string& animName, int fps, bool loop, float offsetX, float offsetY) {
        auto* cs = SpritesheetCache::get().load(path);
        if (!cs) return;

        isExternalAnim = true;
        externalFrames = cs->frames;

        externalAnimData.name = "FES_" + animName;
        externalAnimData.prefix = animName;
        externalAnimData.fps = fps;
        externalAnimData.loop = loop;
        externalAnimData.offsetX = offsetX;
        externalAnimData.offsetY = offsetY;
        externalAnimData.indices.clear();

        for (int i = 0; i < (int)externalFrames.size(); i++) {
            if (externalFrames[i].name.find(animName) == 0) {
                externalAnimData.indices.push_back(i);
            }
        }

        if (externalAnimData.indices.empty()) {
            printf("\x1b[17;1HFES ERROR: Anim '%s' not found in external XML!\x1b[K\n", animName.c_str());
        } else {
            currentAnim = &externalAnimData;
            curFrame = 0; frameTimer = 0; animFinished = false;
        }
    }
};

class Stage {
public:
    Stage(const std::string& path);
    ~Stage();

    void loadFromJson(const std::string& path);
    void draw(float camX, float camY, float camZoom, bool frontLayer, float shakeX = 0, float shakeY = 0);



    float defaultZoom = 1.05f;
    float cameraSpeed = 1.0f;

    float bfX = 770.0f;
    float bfY = 100.0f;
    float dadX = 100.0f;
    float dadY = 100.0f;
    float gfX = 400.0f;
    float gfY = 130.0f;

    // Stage-specific camera offsets
    float bfCamX = 0.0f;
    float bfCamY = 0.0f;
    float dadCamX = 0.0f;
    float dadCamY = 0.0f;
    float gfCamX = 0.0f;
    float gfCamY = 0.0f;

    // Visibility flags
    bool hideGirlfriend = false;
    bool hideOpponent = false;

    std::vector<StageSprite> sprites;
};
