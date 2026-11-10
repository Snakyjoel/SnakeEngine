#pragma once
#include "EggAttack.hpp"
#include <citro2d.h>

class AttackBlueSlam : public EggAttack {
public:
    void start(EggDuelState& duel) override;
    void update(EggDuelState& duel, float dt) override;
    void draw(EggDuelState& duel, float shakeX, float shakeY) override;
    void stop(EggDuelState& duel) override;

    float getArenaSize() const override { return 152.0f; }
    bool isSoulBlue() const override { return true; }

    using SlamWall = SoulGravity;

    enum class Phase {
        TELEGRAPH,
        FLING,
        SLAMMED,
        STRIKE,
        COOLDOWN
    };

private:
    Phase phase = Phase::TELEGRAPH;
    SlamWall wall = SlamWall::DOWN;
    SlamWall prevWall = SlamWall::DOWN;
    SlamWall prevPrevWall = SlamWall::DOWN;

    int round = 0;
    float timer = 0.0f;
    float phaseTimer = 0.0f;

    float spikeEmergence = 0.0f;
    bool spikesActive = false;

    float swordX = 0.0f;
    float swordY = 0.0f;
    float swordTargetAngle = 0.0f;
    bool swordLocked = false;
    bool swordFired = false;

    SlamWall pickNextWall(SlamWall prev, SlamWall prevPrev, float soulX, float soulY);
};
