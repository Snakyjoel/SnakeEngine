#pragma once
#include "../backend/MusicBeatState.hpp"
#include "../objects/Character.hpp"
#include "../objects/Note.hpp"
#include "../objects/Stage.hpp"
#include "SongParser.hpp"
#include "../backend/WeekData.hpp"
#include "../backend/LuaManager.hpp"

#include <vector>
#include <map>
#include <citro2d.h>

// Stores a 128x64 health-icon spritesheet split into normal/losing halves
struct HealthIconData {
    C2D_SpriteSheet sheet = nullptr;  // kept alive so tex/UV stay valid
    C3D_Tex tex;
    Tex3DS_SubTexture normalSub; // left  half (0..63)  — normal
    Tex3DS_SubTexture losingSub; // right half (64..127) — losing
    bool loaded = false;
    void* vramData = nullptr;
    std::string iconName = "";
};

struct DebugLog {
    std::string text;
    float timer;
};

struct LuaTween {
    std::string tag;
    std::string targetTag; // Sprite tag
    std::string prop;
    float startValue;
    float endValue;
    float duration;
    float timer;
    std::string ease;
    bool finished = false;
};

struct LuaText {
    std::string tag;
    std::string text;
    float x = 0;
    float y = 0;
    float size = 1.0f;
    u32 color = 0xFFFFFFFF;
    float width = 0;
    std::string camera = "hud";
    float alpha = 1.0f;
    bool active = false;
    bool front = false;
    bool visible = true;

    C2D_TextBuf buf = nullptr;
    C2D_Text c2dObj;
    bool dirty = true;

    std::string alignment = "left";
    float borderSize = 0.0f;
    u32 borderColor = 0xFF000000;
};

struct LuaTimer {
    std::string tag;
    float duration;
    int loops;
    int loopsLeft;
    float timer = 0.0f;
};

struct SharedLuaTexture {
    C2D_SpriteSheet sheet = nullptr;
    C3D_Tex* tex = nullptr;
    const Tex3DS_SubTexture* subtex = nullptr;
    void* vramData = nullptr;
    int refCount = 0;
    std::string originalPath = "";
};

class InGameVideoPlayer;

class PlayState : public MusicBeatState {
    friend class LuaManager;
    friend class PauseSubState;
public:
    static PlayState* instance; // Quick global access for Lua logic
    InGameVideoPlayer* inGameVideo = nullptr;
    void startVideo(const std::string& name, bool inFrontOfHUD = true, bool loop = false);
    void stopVideo();

    // Lua Script Management Containers
    std::map<std::string, SharedLuaTexture> luaTextureCache;
    std::map<int, SharedLuaTexture> namedTextureRegistry;
    int nextTextureHandle = 1;
    bool luaManualTextureMode = false;
    std::vector<StageSprite> luaSprites;
    std::map<std::string, size_t> luaSpriteIndices; 
    std::vector<LuaTween> activeTweens;
    std::vector<LuaTimer> activeTimers;
    std::vector<LuaText> luaTexts;
    std::map<std::string, size_t> luaTextIndices;
    int currentZOrder = 0;

    float camX = 0.0f;
    float camY = 0.0f;

    PlayState(const std::string& songName = "test", const std::string& difficulty = "");
    PlayState(const WeekData& week, int songIdx, const std::string& difficulty = "");

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;
    void stepHit(int step) override;
    void beatHit(int beat) override;



    // === PUBLIC for Lua (getProperty / setProperty) ===
    std::vector<Note> songNotes;
    float playerX;
    float spacing;
    float customPlayerStrumX[4];
    float customPlayerStrumY[4];
    float customPlayerStrumAngle[4];
    float customPlayerStrumAlpha[4];
    float customPlayerStrumDirection[4];

    float customOpponentStrumX[4];
    float customOpponentStrumY[4];
    float customOpponentStrumAngle[4];
    float customOpponentStrumAlpha[4];
    float customOpponentStrumDirection[4];

    float getLaneY(int lane, bool isPlayer);
    float getLaneAngle(int lane, bool isPlayer);
    float getLaneAlpha(int lane, bool isPlayer);
    float getLaneDirection(int lane, bool isPlayer);

    void triggerEvent(const Event& event);
    bool showGrid = true;
    bool scoreTxtVisible = true;
    bool timeTxtVisible = true;
    bool timeBarVisible = true;
    u32 scoreTxtColor = 0xFFFFFFFF;  // RGBA, default white
    u32 timeTxtColor  = 0xFFFFFFFF;
    u32 timeBarColor  = 0xFFFFFFFF;

    float playerUnderlayAlpha = -1.0f; // -1.0f means use ClientPrefs
    float opponentUnderlayAlpha = -1.0f; // -1.0f means use ClientPrefs
    bool playerUnderlayVisible = true;
    bool opponentUnderlayVisible = true;
    u32 playerUnderlayColor = 0xFF000000; // RGBA, default black
    u32 opponentUnderlayColor = 0xFF000000; // RGBA, default black

    int score = 0;
    int misses = 0;
    int hits = 0;
    int combo = 0;
    int maxCombo = 0;
    
    int sicks = 0;
    int goods = 0;
    int bads = 0;
    int shits = 0;
    
    std::vector<int> keysUsed;
    bool bfWentIdle = false;
    float currentHoldTime[4] = {0, 0, 0, 0};
    float maxHoldTime = 0.0f;
    float health = 1.0f;
    double songLength = 0;
    float camZoom = 1.0f;
    float hudZoom = 1.0f;
    float camAlpha = 1.0f;
    float hudAlpha = 1.0f;
    float targetZoom = 1.0f;

    std::vector<DebugLog> debugLogs;
    void addDebugMessage(const std::string& text);
    void updateLuaText(LuaText& t);

    float camShakeIntensity = 0.0f;
    float camShakeTimer = 0.0f;
    float hudShakeIntensity = 0.0f;

    std::string curSong;
    std::string currentDifficulty;
    Character* bf = nullptr;
    Character* dad = nullptr;
    Character* gf = nullptr;

    float hudShakeTimer = 0.0f;
    float otherShakeIntensity = 0.0f;
    float otherShakeTimer = 0.0f;

    float csX = 0, csY = 0, hsX = 0, hsY = 0, osX = 0, osY = 0;

    float camFollowX = 0.0f;
    float camFollowY = 0.0f;
    bool autoIconPosition = true;
    bool camFollowLocked = false;
    // legacyPositioning: Si es true, usa el origin en top-left (viejo SnakeEngine). 
    // Si es false, usa center pivot (HaxeFlixel).
    bool legacyPositioning = false;
    float lockedCamX = 0.0f;
    float lockedCamY = 0.0f;
    float iconP1X = 0.0f;
    float iconP1Y = 0.0f;
    float iconP2X = 0.0f;
    float iconP2Y = 0.0f;
    float iconP1Alpha = 1.0f;
    float iconP2Alpha = 1.0f;
    float iconP1Scale = 1.0f;
    float iconP2Scale = 1.0f;
    bool iconP1Visible = true;
    bool iconP2Visible = true;
    float camAngle = 0.0f;
    float hudAngle = 0.0f;

    // Custom Strum Properties
    float customPlayerStrumScaleX[4];
    float customPlayerStrumScaleY[4];
    u32 customPlayerStrumColor[4];
    bool customPlayerStrumVisible[4];
    bool customPlayerStrumFlipX[4];
    bool customPlayerStrumFlipY[4];
    bool customPlayerStrumAntialiasing[4];

    float customOpponentStrumScaleX[4];
    float customOpponentStrumScaleY[4];
    u32 customOpponentStrumColor[4];
    bool customOpponentStrumVisible[4];
    bool customOpponentStrumFlipX[4];
    bool customOpponentStrumFlipY[4];
    bool customOpponentStrumAntialiasing[4];

    // UI Object Properties
    // Healthbar
    float healthBarX = -9999.0f;
    float healthBarY = -9999.0f;
    float healthBarScaleX = 1.0f;
    float healthBarScaleY = 1.0f;
    float healthBarAlpha = 1.0f;
    u32 healthBarColor = 0xFFFFFFFF;
    float healthBarAngle = 0.0f;
    bool healthBarVisible = true;
    bool healthBarFlipX = false;
    bool healthBarFlipY = false;
    bool healthBarAntialiasing = true;

    // HealthbarBg
    float healthBarBGX = -9999.0f;
    float healthBarBGY = -9999.0f;
    float healthBarBGScaleX = 1.0f;
    float healthBarBGScaleY = 1.0f;
    float healthBarBGAlpha = 1.0f;
    u32 healthBarBGColor = 0xFFFFFFFF;
    float healthBarBGAngle = 0.0f;
    bool healthBarBGVisible = true;
    bool healthBarBGFlipX = false;
    bool healthBarBGFlipY = false;
    bool healthBarBGAntialiasing = true;

    // Timebar
    float timeBarX_val = -9999.0f;
    float timeBarY_val = -9999.0f;
    float timeBarScaleX = 1.0f;
    float timeBarScaleY = 1.0f;
    float timeBarAlpha = 1.0f;
    float timeBarAngle = 0.0f;
    bool timeBarFlipX = false;
    bool timeBarFlipY = false;
    bool timeBarAntialiasing = true;

    // TimebarBg
    float timeBarBGX = -9999.0f;
    float timeBarBGY = -9999.0f;
    float timeBarBGScaleX = 1.0f;
    float timeBarBGScaleY = 1.0f;
    float timeBarBGAlpha = 1.0f;
    u32 timeBarBGColor = 0xFFFFFFFF;
    float timeBarBGAngle = 0.0f;
    bool timeBarBGVisible = true;
    bool timeBarBGFlipX = false;
    bool timeBarBGFlipY = false;
    bool timeBarBGAntialiasing = true;

    // IconP1 (mostly exists, let's add missing ones)
    float iconP1ScaleX = 1.0f;
    float iconP1ScaleY = 1.0f;
    u32 iconP1Color = 0xFFFFFFFF;
    float iconP1Angle = 0.0f;
    bool iconP1FlipX = false;
    bool iconP1FlipY = false;
    bool iconP1Antialiasing = true;

    // IconP2 (mostly exists, let's add missing ones)
    float iconP2ScaleX = 1.0f;
    float iconP2ScaleY = 1.0f;
    u32 iconP2Color = 0xFFFFFFFF;
    float iconP2Angle = 0.0f;
    bool iconP2FlipX = false;
    bool iconP2FlipY = false;
    bool iconP2Antialiasing = true;

    // ScoreTxt
    float scoreTxtX = -9999.0f;
    float scoreTxtY = -9999.0f;
    float scoreTxtScaleX = 1.0f;
    float scoreTxtScaleY = 1.0f;
    float scoreTxtAlpha = 1.0f;
    float scoreTxtAngle = 0.0f;
    bool scoreTxtFlipX = false;
    bool scoreTxtFlipY = false;
    bool scoreTxtAntialiasing = true;

    // TimeTxt
    float timeTxtX = -9999.0f;
    float timeTxtY = -9999.0f;
    float timeTxtScaleX = 1.0f;
    float timeTxtScaleY = 1.0f;
    float timeTxtAlpha = 1.0f;
    float timeTxtAngle = 0.0f;
    bool timeTxtFlipX = false;
    bool timeTxtFlipY = false;
    bool timeTxtAntialiasing = true;

    // Countdown
    float countdownX = -9999.0f;
    float countdownY = -9999.0f;
    float countdownScaleX = 1.0f;
    float countdownScaleY = 1.0f;
    float countdownAngle = 0.0f;
    bool countdownVisible = true;
    bool countdownFlipX = false;
    bool countdownFlipY = false;
    bool countdownAntialiasing = true;
    u32 countdownColor = 0xFFFFFFFF;

    // Cameras
    float camX_offset = 0.0f;
    float camY_offset = 0.0f;
    float camScaleX = 1.0f;
    float camScaleY = 1.0f;
    bool camVisible = true;
    bool camFlipX = false;
    bool camFlipY = false;

    float hudX_offset = 0.0f;
    float hudY_offset = 0.0f;
    float hudScaleX = 1.0f;
    float hudScaleY = 1.0f;
    bool hudVisible = true;
    bool hudFlipX = false;
    bool hudFlipY = false;

    float otherX_offset = 0.0f;
    float otherY_offset = 0.0f;
    float otherZoom = 1.0f;
    float otherScaleX = 1.0f;
    float otherScaleY = 1.0f;
    float otherAngle = 0.0f;
    float otherAlpha = 1.0f;
    bool otherVisible = true;
    bool otherFlipX = false;
    bool otherFlipY = false;



    void drawLuaSpritesForCamera(const std::string& camera, bool front, float shakeX, float shakeY);
    float getLaneX(int lane, bool isPlayer);

    float receptorY;
    float noteScale;
    float p3DS;

private:
    WeekData weekData;
    int curSongIdx = 0;
    bool isStoryMode = false;
    bool endedSong = false;
    bool gameOver = false;
    Character* deadBF = nullptr;
    bool deathSoundPlayed = false;
    bool deathLoopPlaying = false;
    bool deathConfirmActive = false;
    float deathConfirmTimer = 0.0f;
    float gameOverTimer = 0.0f;
    float deathCamFollowX = 0.0f;
    float deathCamFollowY = 0.0f;
    bool deathCamFollowStarted = false;
    bool deferredClearUnused = false; // true = call clearUnused at start of next update (GPU-safe)

    // Deferred character swap — used when async load hasn't finished yet at event time
    struct PendingCharSwap {
        std::string charType; // "dad", "bf", "gf"
        std::string charName; // character to swap to
    };
    std::vector<PendingCharSwap> pendingSwaps;
    
    // HUD & Stats
    float gridOffset = 0.0f;
    float totalNotesHit = 0.0f;
    float totalNoteScore = 0.0f;
    float accuracy = 0.0f;
    std::string ratingName = "?";
    float curFPS = 0.0f;
    float fpsUpdateTimer = 0.0f;
    int lastStep = -1;

    // Cache Score
    C2D_Text scoreTextObj;
    bool scoreTextNeedsUpdate = true;
    int cachedScore = -1;
    int cachedMisses = -1;
    int cachedCombo = -1;



    void handleInput(float dt);
    void endSong();
    void applyCharacterSwap(const std::string& charType, const std::string& charName, Character* newChar);
    
    void updateCamera(float dt);
    void updateNotesLogic(float dt);
    void drawHUD(float shakeX = 0, float shakeY = 0);
    void drawNotes(float shakeX = 0, float shakeY = 0);
    void drawRating(float shakeX = 0, float shakeY = 0);
    void moveChar(int lane, Character* c);
    void focusCamera(bool isPlayer);
    void drawHitboxOverlay();
    void drawLuaTextsForCamera(const std::string& camera, bool front, float shakeX, float shakeY);
    std::string wrapString(const std::string& text, float scale, float maxWidth);


    C3D_RenderTarget* top;
    C3D_RenderTarget* bottom;

    C2D_SpriteSheet noteSheet;
    C2D_SpriteSheet fastNoteSheet = nullptr;  // NoteSheetFast
    C2D_Image       fastNoteBaseImg;
    std::vector<NoteSprite> fastNoteSubtexs; // [0]=note, [1]=tail, [2]=noteholdend
    SongData songData;
    int curSection = -1;
    size_t nextNoteIndex = 0;
    size_t nextEventIndex = 0;
    
    // Custom Notes
    std::map<std::string, C2D_SpriteSheet> customNoteSheets;
    std::map<std::string, C2D_Image> customNoteImages;

    // Arrow skin scale factor: adjusts noteScale so custom-skin notes match default visual size
    float arrowSkinScaleFactor = 1.0f;
    float fastNoteSkinScaleFactor = 1.0f;
    
    // Lyrics Event Variables
    C2D_TextBuf lyricsTextBuf;
    C2D_Text lyricsTextObj;
    std::string currentLyrics;
    u32 currentLyricsColor;
    float currentLyricsSize;
    
    Stage* currentStage;
    
    bool paused = false;
    class PauseSubState* pauseSubState = nullptr;
    std::vector<std::string> curSongDifficulties;
    
    C2D_TextBuf debugTextBuf = nullptr;
    
    bool keyPressed[4];
    bool keyHeld[4];
    bool keyHitRecently[4];
    float receptorTimer[8];
    int receptorActiveSlot[8];
    float receptorAnimTime[8];
    
    float pOffsetX, pOffsetY;


    float eOffsetX, eOffsetY;
    
    bool startTimerActive;
    int countdownTick;

    // Countdown Visuals
    C2D_SpriteSheet countdownSheet = nullptr;
    std::map<std::string, Tex3DS_SubTexture> countdownSubtexs;
    bool countdownActive;
    float countdownAlpha;
    float countdownScale;
    float countdownYOffset;
    float countdownTimer;
    std::string currentCountdownFrame;
    
    void tickCountdown();

    bool musicStarted;
    int lastBeat;
    
    bool camZooming = true;
    float scoreZoom = 1.0f;

    float vocalMuteTimer = 0.0f;
    float iconBump = 1.0f; // Temporary scale for icon bounce effect

    // Rating Popup variables
    C2D_SpriteSheet ratingSheet = nullptr;
    C2D_Image ratingBaseImage;
    std::map<std::string, Tex3DS_SubTexture> ratingSubtexs;
    std::vector<NoteSprite> noteSubtexs;
    std::vector<std::vector<NoteSprite>> noteSubtexFrames;
    bool ratingActive = false;
    float ratingX = 0.0f;
    float ratingY = 0.0f;
    float ratingVelY = 0.0f;
    float ratingAccelY = 550.0f;
    float ratingScale = 0.7f;
    float ratingAlpha = 1.0f;
    float ratingTimer = 0.0f;
    std::string currentRatingStr;
    
    C2D_Image lazyBG;
    C2D_SpriteSheet lazyBGSheet = nullptr;
    C3D_Tex* lazyRawTex = nullptr;
    Tex3DS_SubTexture* lazyRawSub = nullptr;
    bool lazyIsRaw = false;


    C2D_TextBuf vcrFontBuf;

    
    HealthIconData iconBf;
    HealthIconData iconDad;
    std::map<std::string, HealthIconData> healthIconCache;

    C2D_Font vcrFont;

    void loadHealthIcon(HealthIconData& icon, const std::string& name);
    void drawText(C2D_Text* textObj, float x, float y, float scale, bool centered = true, float maxWidth = 0.0f);
};

