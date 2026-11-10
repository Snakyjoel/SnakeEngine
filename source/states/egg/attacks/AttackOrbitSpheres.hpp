#pragma once
#include "EggAttack.hpp"
#include <citro2d.h>
#include <vector>

class AttackOrbitSpheres : public EggAttack {
public:
    void start(EggDuelState& duel) override;
    void update(EggDuelState& duel, float dt) override;
    void draw(EggDuelState& duel, float shakeX, float shakeY) override;
    void stop(EggDuelState& duel) override;

    float getArenaSize() const override { return 112.0f; }

    enum class Phase {
        TRACKING,
        LOCKED,
        COLLAPSE
    };

    static constexpr int SPHERE_COUNT = 6;
    static constexpr int MAX_TRAIL = 8;

    struct TrailNode {
        float x;
        float y;
        TrailNode(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
    };

    struct ExpandingSphere {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float radius = 7.0f;
        float lifetime = 0.0f;
        float maxLifetime = 1.4f;
        bool active = true;
        bool grazed = false;
        TrailNode trail[MAX_TRAIL];
        int trailCount = 0;
        float trailTimer = 0.0f;
    };

    struct OrbitWave {
        Phase phase = Phase::TRACKING;
        int waveRound = 0;
        float timer = 0.0f;
        float angle = 0.0f;
        float angleSpeed = 2.8f;
        float centerX = 200.0f;
        float centerY = 120.0f;
        float radius = 60.0f;
        bool active = true;
        TrailNode trails[SPHERE_COUNT][MAX_TRAIL];
        int trailCount = 0;
        float trailTimer = 0.0f;
    };

private:
    int round = 0;
    float timer = 0.0f;
    float waveSpawnTimer = 0.0f;
    float spikeEmergence = 0.0f;
    float spikeScrollTimer = 0.0f;

    std::vector<ExpandingSphere> expandingSpheres;
    std::vector<OrbitWave> waves;
};
