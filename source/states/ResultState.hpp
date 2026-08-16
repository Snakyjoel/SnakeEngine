#pragma once
#include "../backend/MusicBeatState.hpp"
#include "Achievements.hpp"
#include "SparrowParser.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <string>
#include <citro2d.h>

class ResultState : public MusicBeatState {
public:
    ResultState(bool isStoryMode, bool fromDebug, const std::string& name, const std::string& difficulty,
                int totalNotes, int maxCombo, int sicks, int goods, int bads, int shits, int misses, long long score);

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    bool isStoryMode;
    bool fromDebug = false;
    std::string name;
    std::string difficulty;

    int totalNotes;
    int maxCombo;
    int sicks;
    int goods;
    int bads;
    int shits;
    int misses;
    long long score;

    float percentage;
    std::string rank;
    std::string finalRating;

    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;

    C2D_SpriteSheet achievementsSheet = nullptr;
    std::vector<Frame> achievementsFrames;
    C2D_Image achievementsImage;

    CachedSpritesheet* resultsSheet = nullptr;
    std::vector<int> resultsAnimFrames;
    int soundSystemFrameIdx = -1;
    int resultsCurFrame = 0;
    float resultsFrameTime = 0.0f;
    const float resultsFps = 12.0f;
    const float tallyFps = 12.0f;
    const float categoriesFps = 16.0f;

    std::vector<int> tallyAnimFrames;
    int tallyCurFrame = 0;
    float tallyFrameTime = 0.0f;
    bool tallyStarted = false;
    bool tallyAnimDone = false;

    CachedSpritesheet* categoriesSheet = nullptr;
    std::vector<int> categoriesAnimFrames;
    int categoriesCurFrame = 0;
    float categoriesFrameTime = 0.0f;
    bool categoriesStarted = false;
    bool categoriesAnimDone = false;

    CachedSpritesheet* textSheet = nullptr;
    std::map<char, Frame> numSmallFrames;

    struct NumberStat {
        int targetValue;
        int currentDisplay;
        u32 color;
        float x;       // base X (= kStatStartX + xOffset)
        float xOffset; // individual horizontal nudge per stat row
        float y;
        float scale;
        bool active;
        bool complete;
    };
    std::vector<NumberStat> numberStats;
    int currentStatIndex = 0;
    bool statsStarted = false;

    // Score display using NUMBER DIGITAL sprites from rankAssets2
    // Each digit 0-9 has 5 animation frames (played once on change, hold last)
    std::vector<Frame> scoreDigitFrames[10]; // multi-frame per digit 0-9
    Frame goneFrame;                          // "GONE0001" placeholder frame (1 frame)
    Frame disabledFrame;                      // "DISABLED0001" placeholder frame

    // Per digit-slot animation state (10 slots for 10 digits)
    int   slotAnimFrame[10]  = {};   // current animation frame index per slot
    float slotAnimTimer[10]  = {};   // elapsed time for current slot anim frame
    int   slotPrevDigit[10]  = {};   // previous digit value to detect changes

    bool  holyScoreMode  = false; // true when score > 9999999999
    float holyScoreTimer = 0.0f;  // drives rainbow colour cycling

    // Score count-up state
    long long scoreDisplayValue = 0; // current value shown (counts from 0 to score)
    float scoreCountTimer = 0.0f;    // time elapsed since stats started (for count-up)
    static constexpr float kScoreCountDuration = 1.2f; // seconds to count up to final score

    float timer = 0.0f;
    float statsTimer = 0.0f; // time elapsed since stats started

    // Music playback state based on rank
    std::string musicIntroPath;
    std::string musicLoopPath;
    bool musicIntroPlaying = false;
    bool musicLoopPlaying  = false;
    bool musicStarted      = false;
    double musicIntroDurationMs = 0.0; // duration of the intro track in ms
    float  musicIntroElapsed    = 0.0f; // time elapsed since intro started (seconds)

    // Percentage count-up animation
    // Counts from 0 to (finalPct-1) over pctAnimDuration, pauses, then snaps to finalPct
    static constexpr float kPctPauseDuration    = 0.5f;  // pause at penultimate before final snap
    static constexpr float kPctFallbackDuration = 3.5f;  // used when there is no intro track
    float pctAnimTimer    = 0.0f;   // elapsed since pct anim started
    float pctAnimDuration = 3.5f;   // total time for 0 -> (finalPct-1), set from intro length
    float pctDisplayValue = 0.0f;   // current animated value shown in draw()
    int   pctLastInt      = -1;     // last integer shown, to detect when to play scroll sound
    bool  pctAnimActive   = false;  // true once statsStarted
    bool  pctPausePhase   = false;  // true while waiting at (finalPct-1)
    float pctPauseTimer   = 0.0f;   // elapsed time in pause phase
    bool  pctAnimDone     = false;  // true after final snap to finalPct

    // Background scrolling text, flash, and fade variables
    Frame bgScrollFrame;
    Frame bgTextVerticalFrame;
    Frame clearPercentFrame;
    Frame pctNumberFrames[10];
    float bgScrollOffset   = 0.0f;
    float bgFadeAlpha      = 0.0f;
    float flashAlpha       = 0.0f;
    bool  flashTriggered   = false;

    // Percentage clearPercentText and custom numbers animation variables
    float pctFadeAlpha    = 1.0f;
    float pctBlinkTimer   = 0.0f;
    bool  pctBlinkState   = false;

    static constexpr float kPctNumOffsetX = -115.0f;  // custom X offset for numbers
    static constexpr float kPctNumOffsetY = 20.0f;  // custom Y offset for numbers

    // Achievements custom slide-in and scroll animation variables
    float achEnterTimer  = 0.0f;
    float achScrollOffset = 0.0f;

    struct RankAnim {
        CachedSpritesheet* sheet = nullptr;
        std::vector<Frame> introFrames;
        std::vector<Frame> loopFrames;
        bool introPlaying = false;
        float introTimer = 0.0f;
        float introDuration = 0.0f;
        int curFrame = 0;
        float frameTimer = 0.0f;
        bool started = false;
        float scale = 0.85f;
        float offsetY = 0.0f;
        float fps = 12.0f;
        float alpha = 0.0f;
    };
    RankAnim rankAnim;

    CachedSpritesheet* gfSheet = nullptr;
    std::vector<int> gfFrames;
    bool gfOutPlaying = false;
    float gfOutX = -200.0f;
    float gfOutTargetX = 60.0f;
    float gfOutDuration = 1.2f;
    float gfOutTimer = 0.0f;
    int gfOutFrame = 0;
    float gfOutFrameTimer = 0.0f;
    float gfOffsetY = 0.0f;
    float gfScale = 0.25f;
    float gfFps = 12.0f;

    CachedSpritesheet* heartsSheet = nullptr;
    std::vector<int> heartsFrames;
    float heartsTimer = 0.0f;
    int heartsFrame = 0;
    float heartsFrameTimer = 0.0f;
    float heartsScale = 1.0f;
    float heartsOffsetX = 0.0f;
    float heartsOffsetY = 0.0f;
    float heartsFps = 12.0f;

    CachedSpritesheet* goodGfSheet = nullptr;
    std::vector<Frame> goodGfIntroFrames;
    std::vector<Frame> goodGfLoopFrames;
    bool goodGfIntroPlaying = false;
    float goodGfAlpha = 0.0f;
    float goodGfFps = 12.0f;
    float goodGfOffsetX = 0.0f;
    float goodGfOffsetY = 0.0f;
    float goodGfScale = 0.25f;
    int goodGfCurFrame = 0;
    float goodGfFrameTimer = 0.0f;

    float lossOffsetY = 0.0f;
    float lossBaseH = 0.0f;
};

