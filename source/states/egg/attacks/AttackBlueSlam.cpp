#include "AttackBlueSlam.hpp"
#include "../EggDuelState.hpp"
#include <algorithm>
#include <cmath>

struct WallParams {
    float soulAngle;
    float enemyOffX, enemyOffY;
    float enemyScaleX, enemyScaleY;
    float enemyTilt;
    float flingVx, flingVy;
    float swordAngle;
};

static const WallParams s_walls[4] = {
    // DOWN
    { 0.0f,         0.0f,  16.0f, 0.85f, 1.25f,  0.20f,    0.0f,  850.0f,  1.570796f },
    // UP
    { 3.14159265f,  0.0f, -16.0f, 0.85f, 1.25f, -0.20f,    0.0f, -850.0f, -1.570796f },
    // LEFT
    { 1.570796f,  -18.0f,   0.0f, 1.25f, 0.85f, -0.25f, -850.0f,    0.0f,  3.14159265f },
    // RIGHT
    { -1.570796f,  18.0f,   0.0f, 1.25f, 0.85f,  0.25f,  850.0f,    0.0f,  0.0f }
};

void AttackBlueSlam::start(EggDuelState& duel) {
    round = 0;
    phase = Phase::TELEGRAPH;
    wall = SlamWall::DOWN;
    prevWall = SlamWall::DOWN;
    prevPrevWall = SlamWall::DOWN;
    timer = 0.0f;
    phaseTimer = 0.0f;
    spikeEmergence = 0.0f;
    spikesActive = false;
    swordLocked = false;
    swordFired = false;

    duel.soulIsBlue = true;
    duel.soulGravity = SlamWall::DOWN;
    duel.soulFlinging = false;
}

void AttackBlueSlam::stop(EggDuelState& duel) {
    spikeEmergence = 0.0f;
    spikesActive = false;
    duel.soulIsBlue = false;
    duel.soulFlinging = false;
}

void AttackBlueSlam::update(EggDuelState& duel, float dt) {
    if (round >= 8) {
        duel.attackTimer = 0.0f;
        duel.duelPhase = DuelPhase::BOX_TRANSITION_OUT;
        duel.screenShakeTimer = 0.0f;
        duel.screenShakeMag = 0.0f;
        duel.boxTransitionTimer = 0.0f;
        duel.boxTrailCount = 0;
        duel.soulTrailCount = 0;
        duel.soulExitStartX = duel.soulX;
        duel.soulExitStartY = duel.soulY;
        duel.soulIsBlue = false;
        return;
    }

    timer += dt;
    phaseTimer += dt;

    float soulCX = duel.soulX + 8.0f;
    float soulCY = duel.soulY + 8.0f;

    const float wallPadding = 2.0f;
    const float soulSize = 16.0f;
    const float minX = duel.ARENA_TARGET_X + wallPadding;
    const float maxX = duel.ARENA_TARGET_X + duel.ARENA_SIZE - wallPadding - soulSize;
    const float minY = duel.ARENA_TARGET_Y + wallPadding;
    const float maxY = duel.ARENA_TARGET_Y + duel.ARENA_SIZE - wallPadding - soulSize;

    const int wallIdx = (int)wall & 3;
    const WallParams& wp = s_walls[wallIdx];

    switch (phase) {
        case Phase::TELEGRAPH: {
            duel.soulIsBlue = true;
            duel.soulGravity = wall;
            duel.soulFlinging = false;
            spikesActive = false;
            spikeEmergence = 0.0f;
            swordFired = false;

            duel.targetSoulAngle = wp.soulAngle;
            duel.enemyOffsetX = wp.enemyOffX;
            duel.enemyOffsetY = wp.enemyOffY;
            duel.enemyScaleX = wp.enemyScaleX;
            duel.enemyScaleY = wp.enemyScaleY;
            duel.enemyTilt = wp.enemyTilt;
            duel.enemyColor = C2D_Color32(0, 0, 255, 255);
            duel.enemyGlowAlpha = 0.7f;

            if (phaseTimer >= 0.08f) {
                phase = Phase::FLING;
                phaseTimer = 0.0f;
                duel.soulFlinging = true;
                duel.playSlashSound();

                duel.soulVelX = wp.flingVx;
                duel.soulVelY = wp.flingVy;
            }
            break;
        }

        case Phase::FLING: {
            bool hitWall = false;
            if (wall == SlamWall::DOWN  && duel.soulY >= maxY) { duel.soulY = maxY; hitWall = true; }
            if (wall == SlamWall::UP    && duel.soulY <= minY) { duel.soulY = minY; hitWall = true; }
            if (wall == SlamWall::LEFT  && duel.soulX <= minX) { duel.soulX = minX; hitWall = true; }
            if (wall == SlamWall::RIGHT && duel.soulX >= maxX) { duel.soulX = maxX; hitWall = true; }

            if (hitWall || phaseTimer >= 0.22f) {
                phase = Phase::SLAMMED;
                phaseTimer = 0.0f;
                duel.soulFlinging = false;
                duel.soulOnGround = true;
                duel.soulVelX = 0.0f;
                duel.soulVelY = 0.0f;
                duel.playHurtSound();
                duel.screenShakeTimer = 0.16f;
                duel.screenShakeMag = 5.0f;

                if (wall == SlamWall::DOWN) {
                    swordX = soulCX;
                    swordY = duel.ARENA_TARGET_Y + 12.0f;
                } else if (wall == SlamWall::UP) {
                    swordX = soulCX;
                    swordY = duel.ARENA_TARGET_Y + duel.ARENA_SIZE - 12.0f;
                } else if (wall == SlamWall::LEFT) {
                    swordX = duel.ARENA_TARGET_X + duel.ARENA_SIZE - 12.0f;
                    swordY = soulCY;
                } else {
                    swordX = duel.ARENA_TARGET_X + 12.0f;
                    swordY = soulCY;
                }
                swordTargetAngle = wp.swordAngle;
                swordLocked = false;
                swordFired = false;
                duel.playSwordAppearSound();
            }
            break;
        }

        case Phase::SLAMMED: {
            const float TRACK_DUR = 0.12f;
            const float TOTAL_SLAM_DUR = 0.32f;

            if (phaseTimer < TRACK_DUR) {
                if (wall == SlamWall::DOWN || wall == SlamWall::UP) {
                    swordX = soulCX;
                } else {
                    swordY = soulCY;
                }
                swordLocked = false;
            } else {
                if (!swordLocked) {
                    swordLocked = true;
                    duel.playLockSound();
                }
            }

            if (phaseTimer >= TOTAL_SLAM_DUR) {
                phase = Phase::STRIKE;
                phaseTimer = 0.0f;
                spikesActive = true;
                duel.playCutSound();
                duel.screenShakeTimer = 0.16f;
                duel.screenShakeMag = 5.0f;
                duel.slashImpactFlash = 0.06f;

                if (!swordFired) {
                    swordFired = true;
                    EggDuelState::Bullet b;
                    b.type = EggDuelState::BulletType::THRUST_SWORD;
                    b.x = swordX;
                    b.y = swordY;
                    b.startX = swordX;
                    b.startY = swordY;
                    b.angle = swordTargetAngle;
                    b.vx = cosf(swordTargetAngle) * 1400.0f;
                    b.vy = sinf(swordTargetAngle) * 1400.0f;
                    b.length = 28.0f;
                    b.width = 6.0f;
                    b.damage = 20;
                    b.maxLifetime = 0.60f;
                    b.isFake = false;
                    b.trailCount = 0;
                    b.color = C2D_Color32(255, 255, 255, 255);
                    b.glowColor = C2D_Color32(255, 220, 40, 220);
                    duel.spawnBullet(b);
                }
            }
            break;
        }

        case Phase::STRIKE: {
            const float EMERGE_DUR = 0.04f;
            const float HOLD_DUR   = 0.24f;
            const float RETRACT_DUR = 0.14f;
            const float TOTAL_STRIKE = EMERGE_DUR + HOLD_DUR + RETRACT_DUR;

            if (phaseTimer < EMERGE_DUR) {
                spikeEmergence = phaseTimer / EMERGE_DUR;
            } else if (phaseTimer < (EMERGE_DUR + HOLD_DUR)) {
                spikeEmergence = 1.0f;
            } else {
                float p = (phaseTimer - (EMERGE_DUR + HOLD_DUR)) / RETRACT_DUR;
                spikeEmergence = std::max(0.0f, 1.0f - (p * p));
            }

            const float hazardDepth = 32.0f * spikeEmergence;
            if (spikesActive && duel.iFrameTimer <= 0.0f && duel.curHp > 0) {
                bool hit = false;
                if (wall == SlamWall::DOWN  && (duel.soulY + 16.0f) >= (maxY + 16.0f - hazardDepth)) hit = true;
                if (wall == SlamWall::UP    && duel.soulY <= (minY + hazardDepth)) hit = true;
                if (wall == SlamWall::LEFT  && duel.soulX <= (minX + hazardDepth)) hit = true;
                if (wall == SlamWall::RIGHT && (duel.soulX + 16.0f) >= (maxX + 16.0f - hazardDepth)) hit = true;

                if (hit) {
                    duel.applyDamage(38, 24, 0.45f);
                }
            }

            if (spikesActive && spikeEmergence > 0.4f && duel.curHp > 0) {
                float spkX = soulCX, spkY = soulCY;
                if (wall == SlamWall::DOWN)  spkY = (maxY + 16.0f) - hazardDepth;
                if (wall == SlamWall::UP)    spkY = minY + hazardDepth;
                if (wall == SlamWall::LEFT)  spkX = minX + hazardDepth;
                if (wall == SlamWall::RIGHT) spkX = (maxX + 16.0f) - hazardDepth;

                if (duel.isPointInsideGrazeHeart(spkX, spkY, soulCX, soulCY, duel.curSoulAngle)) {
                    duel.triggerGraze(8.0f * dt, true);
                }
            }

            if (phaseTimer >= TOTAL_STRIKE) {
                phase = Phase::COOLDOWN;
                phaseTimer = 0.0f;
                spikesActive = false;
                spikeEmergence = 0.0f;
            }
            break;
        }

        case Phase::COOLDOWN: {
            if (phaseTimer >= 0.04f) {
                round++;
                prevPrevWall = prevWall;
                prevWall = wall;
                wall = pickNextWall(prevWall, prevPrevWall, duel.soulX, duel.soulY);
                duel.soulGravity = wall;
                phase = Phase::TELEGRAPH;
                phaseTimer = 0.0f;
            }
            break;
        }
    }
}

void AttackBlueSlam::draw(EggDuelState& duel, float shakeX, float shakeY) {
    const float innerLeft = duel.ARENA_TARGET_X + shakeX + 2.0f;
    const float innerTop  = duel.ARENA_TARGET_Y + shakeY + 2.0f;
    const float innerW    = duel.ARENA_SIZE - 4.0f;
    const float innerH    = duel.ARENA_SIZE - 4.0f;

    // Minimum jump height indicator line (32px clearance)
    if (phase == Phase::SLAMMED || phase == Phase::TELEGRAPH) {
        int blink = (int)(duel.animTimer * 20.0f) % 2;
        u32 indCol = C2D_Color32(255, 35, 35, blink ? 240 : 70);
        const float safeMargin = 32.0f;

        if (wall == SlamWall::DOWN) {
            float lineY = (innerTop + innerH) - safeMargin;
            C2D_DrawLine(innerLeft, lineY, indCol, innerLeft + innerW, lineY, indCol, 1.5f, 0.43f);
        } else if (wall == SlamWall::UP) {
            float lineY = innerTop + safeMargin;
            C2D_DrawLine(innerLeft, lineY, indCol, innerLeft + innerW, lineY, indCol, 1.5f, 0.43f);
        } else if (wall == SlamWall::LEFT) {
            float lineX = innerLeft + safeMargin;
            C2D_DrawLine(lineX, innerTop, indCol, lineX, innerTop + innerH, indCol, 1.5f, 0.43f);
        } else {
            float lineX = (innerLeft + innerW) - safeMargin;
            C2D_DrawLine(lineX, innerTop, indCol, lineX, innerTop + innerH, indCol, 1.5f, 0.43f);
        }
    }

    // Active wall spikes
    if (spikeEmergence > 0.005f && duel.spikeImg.tex) {
        setScissorBox(innerLeft, innerTop, innerW, innerH);

        const float scale = 1.8f;
        const float spikeWidth = 8.0f * scale;
        const float spikeHeight = 7.0f * scale;
        const float halfH = spikeHeight * 0.5f;
        const float emergedOffset = spikeHeight * spikeEmergence;

        const int count = (int)((wall == SlamWall::DOWN || wall == SlamWall::UP ? innerW : innerH) / spikeWidth) + 1;
        const float spacing = (wall == SlamWall::DOWN || wall == SlamWall::UP) ? (innerW / (float)count) : (innerH / (float)count);

        for (int i = 0; i < count; i++) {
            float px = 0.0f, py = 0.0f, rot = 0.0f;
            if (wall == SlamWall::DOWN) {
                px = innerLeft + (float)i * spacing + (spacing * 0.5f);
                py = (innerTop + innerH) + halfH - emergedOffset;
                rot = 0.0f;
            } else if (wall == SlamWall::UP) {
                px = innerLeft + (float)i * spacing + (spacing * 0.5f);
                py = innerTop - halfH + emergedOffset;
                rot = 3.14159265f;
            } else if (wall == SlamWall::LEFT) {
                px = innerLeft - halfH + emergedOffset;
                py = innerTop + (float)i * spacing + (spacing * 0.5f);
                rot = 1.570796f;
            } else {
                px = (innerLeft + innerW) + halfH - emergedOffset;
                py = innerTop + (float)i * spacing + (spacing * 0.5f);
                rot = -1.570796f;
            }
            C2D_DrawImageAtRotated(duel.spikeImg, px, py, 0.44f, rot, nullptr, scale, scale);
        }

        disableScissor();
    }

    // Anti-jump sword hazard
    if ((phase == Phase::SLAMMED || (phase == Phase::STRIKE && phaseTimer < 0.14f)) && !swordFired) {
        float swordAlpha = 1.0f;
        if (phase == Phase::SLAMMED) {
            swordAlpha = std::min(1.0f, phaseTimer / 0.08f);
        }

        float sx = swordX + shakeX;
        float sy = swordY + shakeY;

        float farEndX = sx + cosf(swordTargetAngle) * 180.0f;
        float farEndY = sy + sinf(swordTargetAngle) * 180.0f;

        u32 warnColor;
        if (swordLocked) {
            int blink = (int)(duel.animTimer * 38.0f) % 2;
            warnColor = blink ? C2D_Color32(255, 255, 60, 255) : C2D_Color32(255, 255, 255, 230);
        } else {
            u8 lineAlpha = (u8)(swordAlpha * 180.0f);
            warnColor = C2D_Color32(255, 70, 70, lineAlpha);
        }

        u32 swordLineHead = warnColor;
        u32 swordLineFade = (swordLineHead & 0x00FFFFFF);
        C2D_DrawLine(sx, sy, swordLineHead, farEndX, farEndY, swordLineFade, swordLocked ? 2.2f : 1.2f, 0.42f);

        if (duel.swordSheet && duel.swordImg.tex) {
            float rot = swordTargetAngle - 1.570796f;
            C2D_ImageTint tint;
            C2D_ImageTint* tintPtr = nullptr;
            if (swordLocked && ((int)(duel.animTimer * 44.0f) % 2 == 0)) {
                C2D_PlainImageTint(&tint, C2D_Color32(255, 50, 50, 255), 1.0f);
                tintPtr = &tint;
            }
            C2D_DrawImageAtRotated(duel.swordImg, sx, sy, 0.46f, rot, tintPtr, 2.4f, 2.4f);
        }
    }
}

AttackBlueSlam::SlamWall AttackBlueSlam::pickNextWall(SlamWall prev, SlamWall prevPrev, float soulX, float soulY) {
    const float wallPadding = 2.0f;
    const float soulSize = 16.0f;
    const float minX = 192.0f - 60.0f + wallPadding;
    const float maxX = 192.0f + 60.0f - wallPadding - soulSize;
    const float minY = 112.0f - 60.0f + wallPadding;
    const float maxY = 112.0f + 60.0f - wallPadding - soulSize;

    float distDown = std::max(10.0f, maxY - soulY);
    float distUp = std::max(10.0f, soulY - minY);
    float distLeft = std::max(10.0f, soulX - minX);
    float distRight = std::max(10.0f, maxX - soulX);

    struct Candidate {
        SlamWall wall;
        float weight;
    };
    Candidate candidates[4];
    int count = 0;

    auto addCandidate = [&](SlamWall w, float dist) {
        if (w == prev) return;
        float wgt = dist;
        if (w == prevPrev) wgt *= 0.35f;
        candidates[count++] = { w, wgt };
    };

    addCandidate(SlamWall::DOWN, distDown);
    addCandidate(SlamWall::UP, distUp);
    addCandidate(SlamWall::LEFT, distLeft);
    addCandidate(SlamWall::RIGHT, distRight);

    if (count == 0) return SlamWall::DOWN;

    float totalWeight = 0.0f;
    for (int i = 0; i < count; i++) totalWeight += candidates[i].weight;

    float r = ((float)rand() / (float)RAND_MAX) * totalWeight;
    float accum = 0.0f;
    for (int i = 0; i < count; i++) {
        accum += candidates[i].weight;
        if (r <= accum) return candidates[i].wall;
    }
    return candidates[count - 1].wall;
}
