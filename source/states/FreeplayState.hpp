#pragma once
#include <3ds.h>
#include "../backend/MusicBeatState.hpp"
#include "../backend/WeekData.hpp"
#include "SparrowParser.hpp"
#include "../objects/CppAnimate.hpp"
#include "../objects/AccuracyDisplay.hpp"
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
    FreeplayState(bool isSubState = false);
    ~FreeplayState() override;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;
    
    static std::string currentChar;

    void openSubState(bool playIntro = true);

    bool isSubState = false;
    bool hasFinishedExit = false;
    bool isExiting = false;
    bool hasExited = false;
    float exitTimer = 0.0f;

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

    // Cached week display name lines and difficulty strings
    struct CachedWeekLines {
        std::vector<std::string> lines;
        float scale = 0.8f;
        float startY = 120.0f;
        float lineHeight = 16.0f;
    };
    CachedWeekLines cachedWeekLines;
    void updateSelectedWeekText();

    std::string cachedCurrentDifficultyStr;
    std::string cachedCurDiffLower;
    std::string cachedUpperDiffStr;
    std::string cachedAlbumName;
    float cachedFavPromptWidth = 0.0f;
    
    // Background and unified Freeplay BF sprite sheet
    C2D_SpriteSheet freeplayBFSheet = nullptr;
    Frame bfBgFrame;
    Frame capsuleFrame;
    Frame arrowFrame;
    C2D_Image getBfBackgroundImage();
    
    // UI and difficulty sprites
    C2D_SpriteSheet freeplayDiffSheet = nullptr;
    std::vector<Frame> freeplayDiffFrames;
    Frame getDifficultyFrame(const std::string& name);
    bool hasBaseDiff(const std::string& name);
    C2D_Image getDifficultyImage(const std::string& name);

    // Icon Cache for Freeplay Songs list
    C2D_SpriteSheet freeplayIconsSheet = nullptr;
    std::vector<Frame> freeplayIconsFrames;
    Frame getIconFrame(const std::string& name);
    bool hasBaseIcon(const std::string& name);
    C2D_Image getIconImage(const std::string& name);
    
    // Song capsule sprite with scrolling text
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

    std::vector<Frame> numberFrames[10];
    float numbersAnimTime = 0.0f;
    int lastDigit[7] = {-1, -1, -1, -1, -1, -1, -1};
    float digitAnimTimer[7] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    float lerpScore = 0.0f;
    int targetScore = 0;

    // Album Art
    C2D_SpriteSheet menuBgSheet = nullptr;
    C2D_SpriteSheet freeplayAlbumsSheet = nullptr;
    std::vector<Frame> freeplayAlbumsFrames;
    std::string currentAlbumName;
    Frame getAlbumFrame(const std::string& name);
    Frame getAlbumTextFrame(const std::string& name);
    bool hasBaseAlbum(const std::string& name);
    C2D_Image getAlbumImage(const std::string& name);
    C2D_Image getAlbumTextImage(const std::string& name);
    std::string getAlbumNameForSelected(int songIdx = -1);

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

    // Cleared accuracy display
    AccuracyDisplay accuracyDisplay;

    // Difficulty selection arrow and animation offset details
    float leftArrowVisibleTime = 0.0f;
    float rightArrowVisibleTime = 0.0f;
    float diffOffsetX = 0.0f;

    // Intro transition
    float introTimer = 0.0f;

    // BF DJ & Scrolling Text
    CppAnimate bfdjAnimate;
    float textScrollTime = 0.0f;

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

    std::vector<Frame> letterStuffFrames;
    Frame starFrame;
    std::vector<Frame> fireLoopFrames;
    float starWaveTimer = 0.0f;

    struct StarSlot {
        bool visible = false;
        int flameTier = 0; // 0: none, 1: blue, 2: red
        float flameTimer = 0.0f;
        bool isExtinguishing = false;
        int extinguishTier = 0;
        float extinguishTimer = 0.0f;
        float spawnScale = 1.0f;
    };
    StarSlot starSlots[10];
    int displayStars = 0;
    int targetStars = 0;
    float starChainTimer = 0.0f;
    void updateStarChain(float dt);

    Frame getFireFrame(float timer);
    int getStarsForSong(const FreeplaySong& fs, const std::string& diffName = "", int diffIdx = -1);

    // LRU Asset Cache Structures
    struct CacheEntry {
        C2D_SpriteSheet sheet = nullptr;
        u64 lastAccessFrame = 0;
        bool isCharIcon = false;
    };

    std::unordered_map<std::string, CacheEntry> iconCache;
    std::unordered_map<std::string, CacheEntry> albumCache;
    std::unordered_map<std::string, CacheEntry> albumTextCache;
    std::unordered_map<std::string, CacheEntry> diffCache;

    static constexpr size_t MAX_ICON_CACHE = 12;
    static constexpr size_t MAX_ALBUM_CACHE = 6;
    static constexpr size_t MAX_ALBUM_TEXT_CACHE = 6;
    static constexpr size_t MAX_DIFF_CACHE = 4;

    // Background loading structures and thread state
    struct AsyncLoadRequest {
        enum class AssetType { DIFFICULTY, ICON, ALBUM, ALBUM_TEXT } type;
        std::string key;
        std::string songName;
        std::string week;
        std::string iconName;
        std::string albumName;
        std::string difficultyName;
        int songIndex = -1;
        int priority = 0;
        
        // Resolved by background thread
        std::string resolvedPath;
        bool iconIsChar = false;
    };

    struct LoadedRawData {
        void* buffer = nullptr;
        size_t size = 0;
        AsyncLoadRequest req;
    };

    Thread loadThread = nullptr;
    LightLock loadLock;
    LightEvent loadEvent;
    volatile bool threadRunning = false;

    std::vector<AsyncLoadRequest> pendingRequests;
    std::vector<LoadedRawData> completedDataQueue;

    int lastSelectedCheck = -1;
    int lastDifficultyCheck = -1;

    void triggerAsyncLoad();
    void queueAssetLoad(AsyncLoadRequest::AssetType type, const std::string& key, const AsyncLoadRequest& baseReq, int priority);
    void prunePendingRequests();
    LoadedRawData loadRawFile(AsyncLoadRequest& req);
    static void threadMain(void* arg);
    void evictLruCacheIfNeeded();
    void clearAssetCache();

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
