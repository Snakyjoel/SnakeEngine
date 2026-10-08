#pragma once
#include <string>
#include <vector>
#include <citro2d.h>
#include "SparrowParser.hpp"
#include "../backend/Paths.hpp"
#include "../backend/Macros.hpp"

class AccuracyDisplay {
public:
    AccuracyDisplay();
    ~AccuracyDisplay();

    // Loads the freeplayAssetsBF sprite sheet and parses cleared box & cleared number frames
    // Also loads letterStuff for rating letter display
    void load();

    // Sets target accuracy (0.0f to 100.0f)
    // If snap is true, immediately sets lerpAccuracy = targetAccuracy
    void setAccuracy(float accuracy, bool snap = false);

    // Sets the rating letter to display to the left of the accuracy box ("GP","P","E","G","g","L","")
    void setRating(const std::string& rating);

    // Updates accuracy lerp interpolation towards targetAccuracy
    void update(float dt);

    // Draws the accuracy box, numbers and rating letter at (x, y)
    // The rating letter is drawn to the LEFT of (x,y) automatically
    void draw(float x, float y, float depth = 0.5f, float alpha = 1.0f, float scale = 0.65f);

    float getTargetAccuracy() const { return targetAccuracy; }
    float getLerpAccuracy() const { return lerpAccuracy; }

private:
    Frame getRatingFrame() const;
    u32   getRatingColor(u8 alpha) const;

    C2D_SpriteSheet freeplayBFSheet = nullptr;
    Frame clearedBoxFrame;
    std::vector<Frame> clearedNumberFrames[10];
    std::vector<Frame> letterStuffFrames;

    std::string currentRating;
    float targetAccuracy = 0.0f;
    float lerpAccuracy = 0.0f;
    bool isLoaded = false;
};
