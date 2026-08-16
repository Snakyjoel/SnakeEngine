#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../backend/parsers/SparrowParser.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../backend/Macros.hpp"

// CppAnimate - a knock off of FlxAnimate lmao
// Uses SpritesheetCache to load the sheet + XML automatically.
// Not intended for gameplay (yet)
class CppAnimate {
public:
    CppAnimate() = default;
    ~CppAnimate() = default;

    // Load spritesheet + XML via SpritesheetCache.
    // Path format is the same as SpritesheetCache::load() (e.g. "preload/images/gfDanceTitle")
    void loadSheet(const std::string& path);

    // Add an animation by frame name prefix.
    // All frames whose name starts with `prefix` will be collected in order.
    // If `indices` is non-empty, only those positions within the matched frames are used.
    void addAnim(const std::string& name, const std::string& prefix,
                 float fps = 24.0f, bool loop = true,
                 float offX = 0.0f, float offY = 0.0f,
                 const std::vector<int>& indices = {});

    // Playback
    void play(const std::string& name, bool forceRestart = false);
    void stop();
    void pause();
    void resume();

    // Update frame timer. Call every frame from your state's update().
    void update(float dt);

    // Draw the current frame at (x, y).
    // x/y is the top-left corner taking frameX/frameY offsets into account.
    void draw(float x, float y, float depth,
              float sx = 1.0f, float sy = 1.0f,
              C2D_ImageTint* tint = nullptr);

    // Draw centered at (cx, cy).
    void drawCentered(float cx, float cy, float depth,
                      float sx = 1.0f, float sy = 1.0f,
                      C2D_ImageTint* tint = nullptr);

    // Fired at the end of a non-looping animation.
    std::function<void(const std::string&)> onAnimFinished;

    bool hasAnim(const std::string& name) const;

    // Logical size of the current frame (respects frameW/frameH if set)
    float width()  const;
    float height() const;

    // State
    std::string curAnim    = "";
    bool animFinished      = false;
    bool visible           = true;
    float alpha            = 1.0f;
    bool flipX             = false;
    bool flipY             = false;
    float scaleX           = 1.0f;
    float scaleY           = 1.0f;
    bool antialiasing      = true;

    // Offset applied on top of the animation's own offset
    float extraOffsetX     = 0.0f;
    float extraOffsetY     = 0.0f;

    // Set this to true to ignore frame offsets (frameX/frameY) from the XML
    bool ignoreFrameOffsets = false;

private:
    CachedSpritesheet* sheet = nullptr;

    // Built animation data: indices into sheet->frames
    struct AnimData {
        std::string prefix;
        std::vector<int> frameIndices; // indices into sheet->frames
        float fps;
        bool loop;
        float offX, offY;
    };

    std::map<std::string, AnimData> anims;
    const AnimData* getCurAnimData() const;
    int curFrameIdx        = 0;  // index into curAnimData->frameIndices
    float frameTimer       = 0.0f;
    bool paused            = false;

    const Frame* currentFrame() const;
};
