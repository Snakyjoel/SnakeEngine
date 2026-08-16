#pragma once
#include "../backend/MusicBeatState.hpp"
#include "../backend/WeekData.hpp"
#include "SparrowParser.hpp"
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

class StoryMenuState : public MusicBeatState {
public:
    struct StoryCacheEntry {
        C2D_SpriteSheet sheet;
        u64 lastAccessFrame;
    };

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    int curSelected = 0;
    int curDifficulty = 0;
    float lerpSelected = 0;
    std::vector<std::string> selectableWeeks;
    std::vector<std::string> curWeekDiffs;

    std::unordered_map<std::string, StoryCacheEntry> weekCache;
    std::unordered_map<std::string, StoryCacheEntry> diffCache;
    
    // Max items in cache before pruning
    const size_t MAX_CACHE = 15;
    u64 cacheFrameCount = 0;
    
    // Touch variables
    bool touchActive = false;
    bool isDragging = false;
    float touchStartY = 0.0f;
    float lastTouchY = 0.0f;
    float touchStartLerp = 0.0f;
    float scrollVelocity = 0.0f;
    float touchHoldTime = 0.0f;

    C2D_SpriteSheet backgroundSheet = nullptr;
    std::string currentBackground;

    C2D_SpriteSheet uiSheet = nullptr;
    std::vector<Frame> uiFrames;
    Frame* arrowLeftFrame = nullptr;
    Frame* arrowPushLeftFrame = nullptr;
    Frame* arrowRightFrame = nullptr;
    Frame* arrowPushRightFrame = nullptr;
    Frame* lockFrame = nullptr;
    
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    float drawWrappedText(const std::string& text, float x, float y, float scale, float wrapWidth, u32 color);

    void updateText();
    void updateDifficulties();
    C2D_Image getWeekImage(const std::string& name);
    C2D_Image getWeekBackgroundImage(const std::string& name);
    C2D_Image getDiffImage(const std::string& name);
};
