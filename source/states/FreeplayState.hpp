#pragma once
#include "../backend/MusicBeatState.hpp"
#include "../backend/WeekData.hpp"
#include "SparrowParser.hpp"
#include "../objects/CppAnimate.hpp"
#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>

struct SpriteCacheEntry {
    C2D_SpriteSheet sheet;
    u64 lastAccessFrame;
};

struct FreeplaySong {
    std::string name;
    std::string week;
    SongInfo info;
};

class FreeplayState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    int curSelected = 0;
    int curDifficulty = 1;
    static int savedDifficulty;
    static std::string savedSongName;
    static std::string savedCategory;
    float lerpSelected = 0;
    std::vector<FreeplaySong> songs;
    std::vector<std::string> curWeekDiffs;
    void updateDifficulties();

    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    
    // Background color lerping
    float curColor[3] = {100, 100, 100};
    float targetColor[3] = {100, 100, 100};
    
    // Background sprite
    C2D_SpriteSheet bfBgSheet = nullptr;
    C2D_Image getBfBackgroundImage();
    
    // UI and difficulty sprites
    std::unordered_map<std::string, SpriteCacheEntry> diffCache;
    
    C2D_Image getDifficultyImage(const std::string& name);

    // Icon Cache for Freeplay Songs list
    std::unordered_map<std::string, SpriteCacheEntry> iconCache;
    C2D_Image getIconImage(const std::string& name);
    
    // Song capsule sprite with scrolling text
    C2D_SpriteSheet capsuleSheet = nullptr;
    C2D_Image getCapsuleImage();
    
    // Text scrolling parameters
    std::string scrollingText;
    int lastScrolledSongIndex = -1;
    float scrollTime = 0.0f;
    float scrollOffset = 0.0f;
    
    // Background scroll parameters
    float bgScrollFast = 0.0f;
    float bgScrollMedium = 0.0f;
    float bgScrollSlow = 0.0f;
    
    // Touch scrolling/tapping parameters
    bool touchActive = false;
    bool isDragging = false;
    float touchStartY = 0.0f;
    float touchStartLerp = 0.0f;
    float lastTouchY = 0.0f;
    float lastTouchX = 0.0f;
    float scrollVelocity = 0.0f;
    float touchHoldTime = 0.0f;

    // Capsule dimensions (from asset specs)
    const float CAPSULE_TEXT_START_X = 21.0f;
    const float CAPSULE_TEXT_START_Y = 7.0f;
    const float CAPSULE_TEXT_WIDTH = 140.0f;  // 142 - 21
    const float CAPSULE_TEXT_HEIGHT = 13.0f;  // 20 - 7
    const float SCROLL_SPEED = 60.0f;  // pixels per second
    const float SCROLL_PAUSE_TIME = 1.5f;  // seconds to pause at end

    // Highscore and digital numbers UI
    C2D_SpriteSheet highscoreSheet = nullptr;
    std::vector<Frame> highscoreFrames;
    float highscoreAnimTime = 0.0f;

    C2D_SpriteSheet numbersSheet = nullptr;
    std::vector<Frame> numberFrames[10];
    float numbersAnimTime = 0.0f;

    float lerpScore = 0.0f;
    int targetScore = 0;

    // Album Art
    std::unordered_map<std::string, SpriteCacheEntry> albumCache;
    std::unordered_map<std::string, SpriteCacheEntry> albumTextCache;
    C2D_SpriteSheet menuBgSheet = nullptr;
    std::string currentAlbumName;
    C2D_Image getAlbumImage(const std::string& name);
    C2D_Image getAlbumTextImage(const std::string& name);
    std::string getAlbumNameForSelected();

    // Selected icon bounce animation
    float iconBounceY = 0.0f;
    float iconBounceVelocity = 0.0f;

    // LRU Cache parameters
    u64 cacheFrameCount = 0;
    float timeSinceSelectionChange = 0.0f;
    float loadingAngle = 0.0f;

    // Back-and-forth capsule text scrolling state
    int scrollState = 0;
    float scrollStateTime = 0.0f;

    // Album rotation animation state
    float albumRotation = -5.0f;
    float albumTextFlashTime = 0.0f;

    // Cleared accuracy display details
    C2D_SpriteSheet clearedSheet = nullptr;
    std::vector<Frame> clearedNumberFrames[10];
    Frame clearedBoxFrame;
    float lerpAccuracy = 0.0f;
    float targetAccuracy = 0.0f;

    // Difficulty selection arrow and animation offset details
    C2D_SpriteSheet arrowSheet = nullptr;
    float leftArrowVisibleTime = 0.0f;
    float rightArrowVisibleTime = 0.0f;
    float diffOffsetX = 0.0f;

    // Intro transition
    float introTimer = 0.0f;

    // BF DJ & Scrolling Text
    CppAnimate bfdjAnimate;
    float textScrollTime = 0.0f;

    bool isExiting = false;
    float exitTimer = 0.0f;
    // Easter egg parameters
    float iconEggTime = -1.0f;
    float iconEggY = 0.0f;
    float iconRotation = 0.0f;
    
    // Extended easter egg physics
    bool iconEggActive = false;
    float iconEggXOffset = 0.0f;
    float iconEggYOffset = 0.0f;
    float iconEggVelY = 0.0f;
    float iconEggVelX = 0.0f;
    float iconEggScaleX = 1.0f;
    float iconEggScaleY = 1.0f;
    float iconEggRotVel = 0.0f;
    int iconEggCombo = 0;
    static int iconEggBest;
    float iconEggTextBounce = 0.0f;
    float iconEggLossTimer = 0.0f;
    int iconEggLastCombo = 0;

    // Category / Letter organizer additions
    std::vector<FreeplaySong> allSongs;
    std::vector<std::string> categories;
    int curCategoryIdx = 0;
    float categoryBounceY = 0.0f;
    std::set<std::string> favorites;
    std::map<std::string, float> heartBounceAnim;

    C2D_SpriteSheet letterStuffSheet = nullptr;
    std::vector<Frame> letterStuffFrames;

    void rebuildCategories();
    void applyCategoryFilter(bool keepSelection = false);
    bool hasSongsInCategory(const std::string& catName);
    void loadFavorites();
    void saveFavorites();
    Frame getLetterFrame(const std::string& categoryName);
    Frame getRatingFrame(const std::string& rating);
    u32 getRatingColor(const std::string& rating, u8 alpha);
    Frame getArrowFrame();
    void drawCategoryOrganizer(float topIntroY, float exitAlpha, float ostX);
};
