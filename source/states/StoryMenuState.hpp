#pragma once
#include "../backend/MusicBeatState.hpp"
#include "../backend/WeekData.hpp"
#include "SparrowParser.hpp"
#include "../objects/AccuracyDisplay.hpp"
#include <vector>
#include <string>
#include <map>
#include <deque>
#include <unordered_map>
#include <unordered_set>

class StoryMenuState : public MusicBeatState {
public:
    struct AsyncLoadRequest {
        enum Type { WEEK_BANNER, BACKGROUND, DIFFICULTY } type;
        int         weekIndex    = -1;   // only for WEEK_BANNER
        std::string resolvedPath;        // pre-resolved on main thread
    };

    struct LoadedResult {
        AsyncLoadRequest::Type type;
        int         weekIndex = -1;
        std::string resolvedPath;
        void*       buffer    = nullptr;
        size_t      size      = 0;
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

    // Confirm animation
    bool  selectedWeek   = false;
    float confirmTimer   = 0.0f;
    float flickerTimer   = 0.0f;
    bool  flickerVisible = true;

    std::string pendingDiffSuffix;
    std::string pendingIntroVideo;
    bool        pendingIsMod    = false;
    std::string pendingModFolder;

    // ── Background loading thread ──────────────────────────────────────────
    Thread     loadThread    = nullptr;
    LightLock  loadLock;
    LightEvent loadEvent;
    volatile bool threadRunning = false;

    std::deque<AsyncLoadRequest> requestQueue;      // protected by loadLock
    std::deque<LoadedResult>     resultQueue;       // protected by loadLock
    std::unordered_set<int>      pendingWeekIndices;// indices in requestQueue

    // Sliding window cache: selectableWeeks index -> sheet
    std::unordered_map<int, C2D_SpriteSheet> weekSheets;

    // Single-slot assets (current selection)
    C2D_SpriteSheet activeBgSheet   = nullptr;
    std::string     activeBgName;
    C2D_SpriteSheet activeDiffSheet = nullptr;
    std::string     activeDiffName;
    bool            activeDiffPathExists = false;

    bool weekBannerFileExists(const std::string& weekName);

    float loadingAngle      = 0.0f;
    int   lastSelectedCheck = -1;
    int   lastDiffCheck     = -1;
    bool  isFirstDiffLoad   = true;
    // ──────────────────────────────────────────────────────────────────────

    // Touch
    bool touchActive = false;
    bool isDragging = false;
    float touchStartY = 0.0f;
    float lastTouchY = 0.0f;
    float touchStartLerp = 0.0f;
    float scrollVelocity = 0.0f;
    float touchHoldTime = 0.0f;

    C2D_SpriteSheet tracksSheet = nullptr;
    C2D_Image       tracksImg   = {};

    C2D_SpriteSheet uiSheet = nullptr;
    std::vector<Frame> uiFrames;
    Frame* arrowLeftFrame     = nullptr;
    Frame* arrowPushLeftFrame = nullptr;
    Frame* arrowRightFrame    = nullptr;
    Frame* arrowPushRightFrame= nullptr;
    Frame* lockFrame          = nullptr;

    AccuracyDisplay accuracyDisplay;

    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    float drawWrappedText(const std::string& text, float x, float y, float scale, float wrapWidth, u32 color);

    void updateText();
    void updateDifficulties();

    // Async loading
    static void threadMain(void* arg);
    void        triggerWindowLoad();
    void        triggerDiffLoad();

    // Getters: return cached sheet; callers show spinner if null
    C2D_Image getWeekImage(const std::string& name);
    C2D_Image getWeekBackgroundImage(const std::string& name);
    C2D_Image getDiffImage(const std::string& name);
};

