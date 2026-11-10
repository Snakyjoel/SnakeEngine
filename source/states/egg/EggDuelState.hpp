#pragma once
#include "backend/MusicBeatState.hpp"
#include "backend/parsers/SparrowParser.hpp"
#include "attacks/EggAttack.hpp"
#include <citro2d.h>
#include <string>
#include <vector>
#include <memory>

enum class DuelPhase {
    ACTION_SELECT,
    BOX_TRANSITION_IN,
    ENEMY_ATTACK,
    BOX_TRANSITION_OUT
};

class EggDuelState : public MusicBeatState {
    friend class EggAttack;
    friend class AttackCrossSlash;
    friend class AttackBlueSlam;
    friend class AttackOrbitSpheres;

public:
    EggDuelState();
    ~EggDuelState() override;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    bool hasTransitions() const override { return false; }

    std::vector<std::unique_ptr<EggAttack>> attackList;
    void initAttacks();

private:
    static constexpr float FADE_DURATION = 0.6f;
    static constexpr float BOX_TRANS_DURATION = 0.35f;

    static constexpr float ARENA_SIZE = 152.0f;
    static constexpr float ARENA_SIZE_ATTACK2 = 112.0f;
    static constexpr float ATTACK2_SPIKE_SCALE = 1.4f;
    static constexpr float ATTACK2_SPIKE_SCROLL_SPD = 45.0f;
    static constexpr float ARENA_CENTER_X = 200.0f;
    static constexpr float ARENA_CENTER_Y = 120.0f;
    static constexpr float ARENA_TARGET_X = (400.0f - ARENA_SIZE) * 0.5f;
    static constexpr float ARENA_TARGET_Y = (240.0f - ARENA_SIZE) * 0.5f;

    static constexpr float DIALOG_BOX_X = 0.0f;
    static constexpr float DIALOG_BOX_Y = 146.0f;
    static constexpr float DIALOG_BOX_W = 320.0f;
    static constexpr float DIALOG_BOX_H = 94.0f;

    static constexpr int ACTION_COUNT = 5;
    static constexpr int TOTAL_ATTACKS = 3;

    DuelPhase duelPhase = DuelPhase::ACTION_SELECT;
    float fadeTimer = 0.0f;
    float animTimer = 0.0f;
    float scrollDownTimer = 0.0f;
    float scrollUpTimer = 0.0f;

    int curHp = 160;
    int maxHp = 160;
    int curKarma = 0;
    float karmaDrainTimer = 0.0f;
    int curAction = 0;
    int curAttack = 0;
    bool isDefending = false;

    float curArenaSize = 0.0f;
    float curArenaAngle = 0.0f;
    float boxTransitionTimer = 0.0f;

    struct BoxTrailNode {
        float size = 0.0f;
        float angle = 0.0f;
        BoxTrailNode(float s = 0.0f, float a = 0.0f) : size(s), angle(a) {}
    };
    static constexpr int MAX_BOX_TRAILS = 5;
    BoxTrailNode boxTrails[MAX_BOX_TRAILS];
    int boxTrailCount = 0;

    float enemyOffsetX = 0.0f;
    float enemyOffsetY = 0.0f;
    float enemyScaleX = 1.0f;
    float enemyScaleY = 1.0f;
    float enemyTilt = 0.0f;
    float enemyGlowAlpha = 0.0f;
    u32 enemyColor = C2D_Color32(255, 255, 255, 255);

    float attackTimer = 0.0f;
    float attackPatternTimer = 0.0f;
    float waveTimer = 0.0f;
    int waveCount = 0;

    float soulX = 85.0f;
    float soulY = 112.0f;
    float curSoulAngle = 0.0f;
    float targetSoulAngle = 0.0f;
    float soulExitStartX = 192.0f;
    float soulExitStartY = 112.0f;

    bool soulIsBlue = false;
    SoulGravity soulGravity = SoulGravity::DOWN;
    bool soulFlinging = false;
    float soulVelX = 0.0f;
    float soulVelY = 0.0f;
    bool soulOnGround = false;

    float iFrameTimer = 0.0f;
    float screenShakeTimer = 0.0f;
    float screenShakeMag = 0.0f;

    struct SoulTrailNode {
        float x = 0.0f;
        float y = 0.0f;
        SoulTrailNode(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
    };
    static constexpr int MAX_SOUL_TRAILS = 4;
    SoulTrailNode soulTrails[MAX_SOUL_TRAILS];
    int soulTrailCount = 0;

    C2D_SpriteSheet soulSheet = nullptr;
    C2D_Image soulImg;
    C2D_SpriteSheet swordSheet = nullptr;
    C2D_Image swordImg;
    Tex3DS_SubTexture swordBladeSubtex;
    C2D_Image swordBladeImg;
    Tex3DS_SubTexture spikeSubtex;
    C2D_Image spikeImg;
    C2D_SpriteSheet spikeSheet = nullptr;

    enum class BulletType {
        CROSS_SLASH,
        THRUST_SWORD,
        STAR_ORB
    };

    struct BulletTrailNode {
        float x;
        float y;
        BulletTrailNode(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
    };
    static constexpr int MAX_BULLET_TRAIL = 8;

    struct Bullet {
        BulletType type = BulletType::STAR_ORB;
        float x = 0.0f;
        float y = 0.0f;
        float startX = 0.0f;
        float startY = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float angle = 0.0f;
        float radius = 4.0f;
        float length = 24.0f;
        float width = 6.0f;
        int damage = 24;
        float timer = 0.0f;
        float maxLifetime = 7.0f;
        bool active = true;
        bool isFake = false;
        bool grazed = false;
        bool wallHit = false;
        BulletTrailNode trail[MAX_BULLET_TRAIL];
        int trailCount = 0;
        u32 color = C2D_Color32(255, 255, 255, 255);
        u32 glowColor = C2D_Color32(255, 255, 255, 120);
    };

    std::vector<Bullet> bullets;

    struct SlashSpark {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float size = 0.0f;
        float timer = 0.0f;
        float maxLife = 0.24f;
        bool active = true;
    };

    struct SpatialCut {
        float x1 = 0.0f, y1 = 0.0f;
        float x2 = 0.0f, y2 = 0.0f;
        float timer = 0.0f;
        float maxDuration = 0.24f;
        float maxThickness = 9.0f;
        bool active = true;
    };

    struct CrossFlare {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 28.0f;
        float timer = 0.0f;
        float maxDuration = 0.16f;
        bool active = true;
    };

    std::vector<SlashSpark> slashSparks;
    std::vector<SpatialCut> spatialCuts;
    std::vector<CrossFlare> crossFlares;
    float slashImpactFlash = 0.0f;

    void updateAttackPatterns(float dt);
    void updateBullets(float dt);
    void drawBullets();
    void spawnBullet(const Bullet& b);
    void applyDamage(int directDmg, int karmaDmg, float iFrameDur = 0.45f);
    void startEnemyAttack();

    void drawRotatedBox(float cx, float cy, float size, float angleRad, u32 borderColor, u32 fillColor, float depth, float borderThickness);
    void spawnSlashSparks(float cx, float cy, float dirX, float dirY, int count, float speedMin, float speedMax);
    void spawnSpatialCut(float x1, float y1, float x2, float y2, float duration, float thickness);
    void spawnCrossFlare(float cx, float cy, float radius, float duration);
    void updateSlashEffects(float dt);
    void drawSlashEffects();
    void drawCurvedSlashIncision(float cx, float cy, float dirX, float dirY, float length, float maxThickness, float progress);
    void drawIntersectionFlare(float cx, float cy, float radius, float progress);
    void drawTravelingSlashWake(float x1, float y1, float x2, float y2, float maxThickness, float alphaRatio);
    void drawSweepingCut(float startX, float startY, float endX, float endY, float maxThickness, float timer, float sweepDuration, float totalDuration);

    C2D_SpriteSheet skyboxSheet = nullptr;
    C2D_Image skyboxImg;
    static constexpr int SKYBOX_COLS = 20;
    static constexpr int SKYBOX_ROWS = 12;
    static constexpr int SKYBOX_CELLS = SKYBOX_COLS * SKYBOX_ROWS;
    Tex3DS_SubTexture skyboxSubtexs[SKYBOX_CELLS];

    C2D_SpriteSheet optionsSheet = nullptr;
    std::vector<Frame> optionFrames;
    Frame* frameAttack = nullptr;
    Frame* frameAct = nullptr;
    Frame* frameItem = nullptr;
    Frame* frameDefense = nullptr;

    C2D_TextBuf textBuf = nullptr;
    C2D_Text nameText;
    C2D_Text hpNumText;
    C2D_Text hpLabelText;
    C2D_Text krLabelText;
    C2D_Text actionLabels[5];
    C2D_Text awaitingText;
    C2D_Text dialogueText;

    int16_t* jumpQuickBuffer = nullptr;
    uint32_t jumpQuickSampleCount = 0;
    ndspWaveBuf jumpQuickWaveBuf;

    int16_t* cut1Buffer = nullptr;
    uint32_t cut1SampleCount = 0;
    ndspWaveBuf cut1WaveBuf;

    int16_t* cut2Buffer = nullptr;
    uint32_t cut2SampleCount = 0;
    ndspWaveBuf cut2WaveBuf;

    void initAudio();
    void freeAudio();
    void playCursorSound();
    void playHurtSound() {}
    void playSlashSound() {}
    void playLockSound() {}
    void playSwordAppearSound();
    void playCutSound();

    float curTP = 0.0f;
    float targetTP = 0.0f;
    float drainTP = 0.0f;
    float drainDelayTimer = 0.0f;
    float grazePulseTimer = 0.0f;

    C2D_SpriteSheet grazeSheet = nullptr;
    C2D_Image grazeImg;

    C2D_SpriteSheet tpSheet = nullptr;
    C2D_Image tpImg;
    Tex3DS_SubTexture tpLeftSubtex;
    Tex3DS_SubTexture tpMidSubtex;
    Tex3DS_SubTexture tpRightSubtex;
    C2D_Image tpLeftImg;
    C2D_Image tpMidImg;
    C2D_Image tpRightImg;
    bool tpLoaded = false;
    C2D_Text tpLabelText;
    C2D_Text tpPercentText;

    bool isPointInsideGrazeHeart(float worldX, float worldY, float soulCX, float soulCY, float soulAngle);
    void triggerGraze(float tpGain, bool isContinuous);
    void updateTP(float dt);
    void drawTPBar(float x, float y, float w);
};
