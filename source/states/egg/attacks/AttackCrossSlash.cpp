#include "AttackCrossSlash.hpp"
#include "../EggDuelState.hpp"
#include <algorithm>
#include <cmath>

static void getSwordPlacement(int quad, bool diag, float dist, float& outX, float& outY, float& outAngle) {
    int q = quad & 3;
    if (!diag) {
        static const float angles[4] = { 0.785398f, 2.356194f, -0.785398f, -2.356194f };
        static const float dx[4] = { -0.7071f,  0.7071f, -0.7071f,  0.7071f };
        static const float dy[4] = { -0.7071f, -0.7071f,  0.7071f,  0.7071f };
        outX = dx[q] * dist;
        outY = dy[q] * dist;
        outAngle = angles[q];
    } else {
        static const float angles[4] = { 1.570796f, 3.141593f, -1.570796f, 0.0f };
        static const float dx[4] = { 0.0f,  dist,  0.0f, -dist };
        static const float dy[4] = { -dist, 0.0f,  dist,  0.0f };
        outX = dx[q];
        outY = dy[q];
        outAngle = angles[q];
    }
}

void AttackCrossSlash::start(EggDuelState& duel) {
    round = 0;
    timer = 0.0f;
    slashTimer = 0.0f;
    isDiag = false;
    locked = false;
    slashing = false;
    hitCenterSparks = false;
    hitWallSparks = false;
    grazed = false;
    hasDecoy = false;
    decoyQuad = 0;

    swordQuad = rand() % 4;
    centerX = 200.0f;
    centerY = 120.0f;
    swordX = 200.0f;
    swordY = 120.0f;
    swordAngle = 0.0f;
}

void AttackCrossSlash::stop(EggDuelState& duel) {
    slashing = false;
    locked = false;
}

void AttackCrossSlash::update(EggDuelState& duel, float dt) {
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
        return;
    }

    timer += dt;
    float soulCX = duel.soulX + 8.0f;
    float soulCY = duel.soulY + 8.0f;

    const float dist = isDiag ? 92.0f : 105.0f;
    float offX = 0.0f, offY = 0.0f;
    getSwordPlacement(swordQuad, isDiag, dist, offX, offY, swordAngle);

    float decoyOffX = 0.0f, decoyOffY = 0.0f;
    if (hasDecoy) {
        getSwordPlacement(decoyQuad, isDiag, dist, decoyOffX, decoyOffY, decoyAngle);
    }

    const float TRACK_TIME = 0.60f;
    const float STRIKE_TIME = 0.78f;

    if (timer < TRACK_TIME) {
        locked = false;
        slashing = false;
        hitCenterSparks = false;
        hitWallSparks = false;

        centerX = soulCX;
        centerY = soulCY;
        swordX = centerX + offX;
        swordY = centerY + offY;
        if (hasDecoy) {
            decoyX = centerX + decoyOffX;
            decoyY = centerY + decoyOffY;
        }

        duel.enemyGlowAlpha = (timer / TRACK_TIME) * 0.5f;

    } else if (timer < STRIKE_TIME) {
        if (!locked) {
            locked = true;
            centerX = soulCX;
            centerY = soulCY;
            swordX = centerX + offX;
            swordY = centerY + offY;
            if (hasDecoy) {
                decoyX = centerX + decoyOffX;
                decoyY = centerY + decoyOffY;
            }
            duel.playLockSound();
        }

        duel.enemyOffsetX = 12.0f;
        duel.enemyScaleX = 0.82f;
        duel.enemyScaleY = 1.25f;
        duel.enemyColor = C2D_Color32(255, 255, 100, 255);

    } else {
        if (!slashing) {
            slashing = true;
            slashTimer = 0.0f;
            hitCenterSparks = false;
            hitWallSparks = false;

            duel.playCutSound();
            duel.screenShakeTimer = 0.16f;
            duel.screenShakeMag = 5.5f;
            duel.slashImpactFlash = 0.06f;

            duel.enemyOffsetX = -26.0f;
            duel.enemyScaleX = 1.40f;
            duel.enemyScaleY = 0.65f;
            duel.enemyColor = C2D_Color32(255, 50, 50, 255);

            EggDuelState::Bullet b;
            b.type = EggDuelState::BulletType::THRUST_SWORD;
            b.x = swordX;
            b.y = swordY;
            b.startX = swordX;
            b.startY = swordY;
            b.angle = swordAngle;
            b.vx = cosf(swordAngle) * 1350.0f;
            b.vy = sinf(swordAngle) * 1350.0f;
            b.length = 28.0f;
            b.width = 6.0f;
            b.damage = 26;
            b.maxLifetime = 1.0f;
            b.isFake = false;
            b.trailCount = 0;
            b.color = C2D_Color32(255, 255, 255, 255);
            b.glowColor = C2D_Color32(255, 220, 40, 220);
            duel.spawnBullet(b);

            if (hasDecoy) {
                EggDuelState::Bullet fb;
                fb.type = EggDuelState::BulletType::THRUST_SWORD;
                fb.x = decoyX;
                fb.y = decoyY;
                fb.startX = decoyX;
                fb.startY = decoyY;
                fb.angle = decoyAngle;
                fb.vx = cosf(decoyAngle) * 1350.0f;
                fb.vy = sinf(decoyAngle) * 1350.0f;
                fb.length = 28.0f;
                fb.width = 6.0f;
                fb.damage = 0;
                fb.maxLifetime = 1.0f;
                fb.isFake = true;
                fb.trailCount = 0;
                fb.color = C2D_Color32(255, 255, 255, 255);
                fb.glowColor = C2D_Color32(255, 220, 40, 220);
                duel.spawnBullet(fb);
            }

            const float hitThickness = 4.5f;
            bool hit = false;
            if (!isDiag) {
                float distH = std::abs(soulCY - centerY);
                float distV = std::abs(soulCX - centerX);
                if (distH <= (4.0f + hitThickness) || distV <= (4.0f + hitThickness)) {
                    hit = true;
                }
            } else {
                float dist1 = std::abs((soulCX - centerX) - (soulCY - centerY)) * 0.7071f;
                float dist2 = std::abs((soulCX - centerX) + (soulCY - centerY)) * 0.7071f;
                if (dist1 <= (4.0f + hitThickness) || dist2 <= (4.0f + hitThickness)) {
                    hit = true;
                }
            }

            if (hit && duel.iFrameTimer <= 0.0f && duel.curHp > 0) {
                duel.applyDamage(42, 20, 0.55f);
            } else if (!hit && !grazed && duel.curHp > 0) {
                bool isGraze = false;
                if (!isDiag) {
                    isGraze = duel.isPointInsideGrazeHeart(soulCX, centerY, soulCX, soulCY, duel.curSoulAngle) ||
                              duel.isPointInsideGrazeHeart(centerX, soulCY, soulCX, soulCY, duel.curSoulAngle);
                } else {
                    float u = soulCX - centerX;
                    float v = soulCY - centerY;
                    float p1x = centerX + (u + v) * 0.5f;
                    float p1y = centerY + (u + v) * 0.5f;
                    float p2x = centerX + (u - v) * 0.5f;
                    float p2y = centerY - (u - v) * 0.5f;
                    isGraze = duel.isPointInsideGrazeHeart(p1x, p1y, soulCX, soulCY, duel.curSoulAngle) ||
                              duel.isPointInsideGrazeHeart(p2x, p2y, soulCX, soulCY, duel.curSoulAngle);
                }
                if (isGraze) {
                    duel.triggerGraze(3.0f, false);
                    grazed = true;
                }
            }
        }

        if (slashing) {
            if (!hitCenterSparks && slashTimer >= 0.025f) {
                hitCenterSparks = true;
                duel.spawnSlashSparks(centerX, centerY, 0.0f, 0.0f, 16, 130.0f, 320.0f);
                duel.slashImpactFlash = 0.05f;
            }
            if (!hitWallSparks && slashTimer >= 0.052f) {
                hitWallSparks = true;
                const float cutLen = 110.0f;
                if (!isDiag) {
                    duel.spawnSlashSparks(centerX - cutLen, centerY,  1.0f,  0.0f, 6, 90.0f, 200.0f);
                    duel.spawnSlashSparks(centerX + cutLen, centerY, -1.0f,  0.0f, 6, 90.0f, 200.0f);
                    duel.spawnSlashSparks(centerX, centerY - cutLen,  0.0f,  1.0f, 6, 90.0f, 200.0f);
                    duel.spawnSlashSparks(centerX, centerY + cutLen,  0.0f, -1.0f, 6, 90.0f, 200.0f);
                } else {
                    float d = cutLen * 0.7071f;
                    duel.spawnSlashSparks(centerX - d, centerY - d,  0.7071f,  0.7071f, 6, 90.0f, 200.0f);
                    duel.spawnSlashSparks(centerX + d, centerY + d, -0.7071f, -0.7071f, 6, 90.0f, 200.0f);
                    duel.spawnSlashSparks(centerX - d, centerY + d,  0.7071f, -0.7071f, 6, 90.0f, 200.0f);
                    duel.spawnSlashSparks(centerX + d, centerY - d, -0.7071f,  0.7071f, 6, 90.0f, 200.0f);
                }
            }
        }

        slashTimer += dt;
        if (slashTimer >= 0.14f) {
            round++;
            grazed = false;
            timer = 0.0f;
            locked = false;
            slashing = false;
            hitCenterSparks = false;
            hitWallSparks = false;
            slashTimer = 0.0f;
            duel.screenShakeTimer = 0.0f;
            isDiag = (round % 2 == 1);
            swordQuad = rand() % 4;
            duel.playSwordAppearSound();

            if (round >= 4) {
                hasDecoy = true;
                if (!isDiag) {
                    int choices[2];
                    if (swordQuad == 0 || swordQuad == 3) {
                        choices[0] = 1; choices[1] = 2;
                    } else {
                        choices[0] = 0; choices[1] = 3;
                    }
                    decoyQuad = choices[rand() % 2];
                } else {
                    int choices[2] = { (swordQuad + 1) % 4, (swordQuad + 3) % 4 };
                    decoyQuad = choices[rand() % 2];
                }
            } else {
                hasDecoy = false;
            }
        }
    }
}

void AttackCrossSlash::draw(EggDuelState& duel, float shakeX, float shakeY) {
    float ccX = centerX + shakeX;
    float ccY = centerY + shakeY;

    if (slashing) {
        const float cutLen = 110.0f;
        const float sweepDur = 0.055f;
        const float totalDur = 0.16f;

        if (!isDiag) {
            duel.drawSweepingCut(ccX - cutLen, ccY, ccX + cutLen, ccY, 10.5f, slashTimer, sweepDur, totalDur);
            duel.drawSweepingCut(ccX, ccY - cutLen, ccX, ccY + cutLen, 10.5f, slashTimer, sweepDur, totalDur);
        } else {
            float d = cutLen * 0.7071f;
            duel.drawSweepingCut(ccX - d, ccY - d, ccX + d, ccY + d, 10.5f, slashTimer, sweepDur, totalDur);
            duel.drawSweepingCut(ccX - d, ccY + d, ccX + d, ccY - d, 10.5f, slashTimer, sweepDur, totalDur);
        }

        if (slashTimer >= 0.025f) {
            float flareProgress = std::min(1.0f, (slashTimer - 0.025f) / 0.12f);
            duel.drawIntersectionFlare(ccX, ccY, 28.0f, flareProgress);
        }
    } else {
        u32 warnColor;
        float lineWidth = 1.5f;

        if (locked) {
            int blink = (int)(timer * 38.0f) % 2;
            warnColor = (blink == 0) ? C2D_Color32(255, 255, 60, 255) : C2D_Color32(255, 255, 255, 230);
            lineWidth = 2.0f;
        } else {
            float trackProgress = std::min(1.0f, timer / 0.60f);
            u8 a = (u8)(70.0f + trackProgress * 155.0f);
            warnColor = C2D_Color32(255, 50, 50, a);
        }

        const float lineExtent = 100.0f;
        if (!isDiag) {
            C2D_DrawLine(ccX - lineExtent, ccY, warnColor, ccX + lineExtent, ccY, warnColor, lineWidth, 0.43f);
            C2D_DrawLine(ccX, ccY - lineExtent, warnColor, ccX, ccY + lineExtent, warnColor, lineWidth, 0.43f);
        } else {
            float d = lineExtent * 0.7071f;
            C2D_DrawLine(ccX - d, ccY - d, warnColor, ccX + d, ccY + d, warnColor, lineWidth, 0.43f);
            C2D_DrawLine(ccX - d, ccY + d, warnColor, ccX + d, ccY - d, warnColor, lineWidth, 0.43f);
        }

        if (duel.swordSheet && duel.swordImg.tex) {
            float rot = swordAngle - 1.570796f;
            float scaleY = 2.4f;
            float curScaleX = 2.4f;

            C2D_ImageTint tint;
            C2D_ImageTint* tintPtr = nullptr;
            if (locked) {
                if (((int)(duel.animTimer * 44.0f) % 2) == 0) {
                    C2D_PlainImageTint(&tint, C2D_Color32(255, 50, 50, 255), 1.0f);
                    tintPtr = &tint;
                }
            }
            C2D_DrawImageAtRotated(duel.swordImg, swordX + shakeX, swordY + shakeY, 0.46f, rot, tintPtr, curScaleX, scaleY);

            if (hasDecoy) {
                float fRot = decoyAngle - 1.570796f;
                C2D_ImageTint fTint;
                C2D_ImageTint* fTintPtr = nullptr;
                if (locked) {
                    if (((int)(duel.animTimer * 44.0f) % 2) == 0) {
                        C2D_PlainImageTint(&fTint, C2D_Color32(255, 50, 50, 255), 1.0f);
                        fTintPtr = &fTint;
                    }
                }
                C2D_DrawImageAtRotated(duel.swordImg, decoyX + shakeX, decoyY + shakeY, 0.46f, fRot, fTintPtr, curScaleX, scaleY);
            }
        }
    }
}
