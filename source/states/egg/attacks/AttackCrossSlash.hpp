#pragma once
#include "EggAttack.hpp"
#include <citro2d.h>

class AttackCrossSlash : public EggAttack {
public:
    void start(EggDuelState& duel) override;
    void update(EggDuelState& duel, float dt) override;
    void draw(EggDuelState& duel, float shakeX, float shakeY) override;
    void stop(EggDuelState& duel) override;

    float getArenaSize() const override { return 152.0f; }

private:
    int round = 0;
    float timer = 0.0f;
    float slashTimer = 0.0f;
    bool isDiag = false;
    bool locked = false;
    bool slashing = false;
    bool hitCenterSparks = false;
    bool hitWallSparks = false;
    bool grazed = false;

    int swordQuad = 0;
    float centerX = 200.0f;
    float centerY = 120.0f;
    float swordX = 200.0f;
    float swordY = 120.0f;
    float swordAngle = 0.0f;

    bool hasDecoy = false;
    int decoyQuad = 0;
    float decoyX = 200.0f;
    float decoyY = 120.0f;
    float decoyAngle = 0.0f;
};
