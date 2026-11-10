#include "AttackOrbitSpheres.hpp"
#include "../EggDuelState.hpp"
#include <algorithm>
#include <cmath>

static constexpr float SPIKE_HAZARD_D = 8.5f;
static constexpr float ORBIT_SPIN_SPEED = 3.25f;
static constexpr float BLINK_PERIOD = 0.30f;
static constexpr float LOCKED_DUR = 0.15f;
static constexpr float COLLAPSE_DUR = 0.15f;
static constexpr float EXPAND_SPEED = 520.0f;
static constexpr float WAVE_INTERVAL = (BLINK_PERIOD * 3.0f) + LOCKED_DUR + COLLAPSE_DUR - 0.10f; // 1.10s
static constexpr float TRAIL_INTERVAL = 0.025f;
static constexpr float WAVE_ANGLE_OFFSET = 35.0f * (3.14159265f / 180.0f);
static constexpr int MAX_ROUNDS = 5;

void AttackOrbitSpheres::start(EggDuelState& duel) {
    round = 0;
    timer = 0.0f;
    waveSpawnTimer = 0.0f;
    spikeEmergence = 0.0f;
    spikeScrollTimer = 0.0f;
    expandingSpheres.clear();
    waves.clear();
}

void AttackOrbitSpheres::stop(EggDuelState& duel) {
    spikeEmergence = 0.0f;
    spikeScrollTimer = 0.0f;
    expandingSpheres.clear();
    waves.clear();
}

void AttackOrbitSpheres::update(EggDuelState& duel, float dt) {
    float soulCX = duel.soulX + 8.0f;
    float soulCY = duel.soulY + 8.0f;
    const float arenaSize = duel.ARENA_SIZE_ATTACK2;

    // Emerge perimeter spikes
    if (spikeEmergence < 1.0f) {
        float prev = spikeEmergence;
        spikeEmergence = std::min(1.0f, spikeEmergence + dt * 3.5f);
        if (prev <= 0.0f && spikeEmergence > 0.0f) {
            duel.playSwordAppearSound();
        }
    }
    spikeScrollTimer += dt;

    // Check perimeter spike collision & graze
    if (spikeEmergence > 0.10f) {
        float boxX = duel.ARENA_CENTER_X - (arenaSize * 0.5f);
        float boxY = duel.ARENA_CENTER_Y - (arenaSize * 0.5f);
        float innerLeft = boxX + 2.0f;
        float innerTop = boxY + 2.0f;
        float innerRight = innerLeft + (arenaSize - 4.0f);
        float innerBottom = innerTop + (arenaSize - 4.0f);
        float curHazard = SPIKE_HAZARD_D * spikeEmergence;

        bool hitSpike = (duel.soulX < innerLeft + curHazard) ||
                        ((duel.soulX + 16.0f) > innerRight - curHazard) ||
                        (duel.soulY < innerTop + curHazard) ||
                        ((duel.soulY + 16.0f) > innerBottom - curHazard);

        if (hitSpike && duel.curHp > 0 && duel.iFrameTimer <= 0.0f) {
            duel.applyDamage(26, 16, 0.40f);
        }

        if (spikeEmergence > 0.5f && duel.curHp > 0 && !hitSpike) {
            bool grazed = duel.isPointInsideGrazeHeart(innerLeft + curHazard, soulCY, soulCX, soulCY, duel.curSoulAngle) ||
                          duel.isPointInsideGrazeHeart(innerRight - curHazard, soulCY, soulCX, soulCY, duel.curSoulAngle) ||
                          duel.isPointInsideGrazeHeart(soulCX, innerTop + curHazard, soulCX, soulCY, duel.curSoulAngle) ||
                          duel.isPointInsideGrazeHeart(soulCX, innerBottom - curHazard, soulCX, soulCY, duel.curSoulAngle);
            if (grazed) {
                duel.triggerGraze(8.0f * dt, true);
            }
        }
    }

    // Update expanding spheres
    for (size_t i = 0; i < expandingSpheres.size(); i++) {
        auto& sp = expandingSpheres[i];
        if (!sp.active) continue;

        sp.trailTimer += dt;
        if (sp.trailTimer >= TRAIL_INTERVAL) {
            sp.trailTimer = 0.0f;
            for (int t = MAX_TRAIL - 1; t > 0; t--) {
                sp.trail[t] = sp.trail[t - 1];
            }
            sp.trail[0] = { sp.x, sp.y };
            if (sp.trailCount < MAX_TRAIL) sp.trailCount++;
        }

        sp.x += sp.vx * dt;
        sp.y += sp.vy * dt;
        sp.lifetime += dt;

        if (sp.lifetime >= sp.maxLifetime || sp.x < -80.0f || sp.x > 480.0f || sp.y < -80.0f || sp.y > 320.0f) {
            sp.active = false;
            continue;
        }

        if (duel.curHp > 0 && duel.iFrameTimer <= 0.0f) {
            float dx = soulCX - sp.x;
            float dy = soulCY - sp.y;
            if ((dx * dx + dy * dy) <= (11.0f * 11.0f)) {
                duel.applyDamage(30, 16, 0.40f);
            }
        }

        if (!sp.grazed && duel.curHp > 0) {
            float dx = soulCX - sp.x;
            float dy = soulCY - sp.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 0.001f) {
                float cx = sp.x + (dx / dist) * sp.radius;
                float cy = sp.y + (dy / dist) * sp.radius;
                if (duel.isPointInsideGrazeHeart(cx, cy, soulCX, soulCY, duel.curSoulAngle)) {
                    duel.triggerGraze(2.5f, false);
                    sp.grazed = true;
                }
            }
        }
    }

    expandingSpheres.erase(
        std::remove_if(expandingSpheres.begin(), expandingSpheres.end(),
            [](const ExpandingSphere& s) { return !s.active; }),
        expandingSpheres.end()
    );

    // Spawn new waves
    timer += dt;
    waveSpawnTimer += dt;
    if (round < MAX_ROUNDS) {
        if (round == 0 || waveSpawnTimer >= WAVE_INTERVAL) {
            waveSpawnTimer = 0.0f;
            OrbitWave w;
            w.phase = Phase::TRACKING;
            w.waveRound = round;
            w.timer = 0.0f;
            w.angle = (float)round * WAVE_ANGLE_OFFSET;
            w.angleSpeed = ORBIT_SPIN_SPEED;
            w.centerX = soulCX;
            w.centerY = soulCY;
            w.radius = 60.0f;
            w.active = true;
            w.trailCount = 0;
            w.trailTimer = 0.0f;
            waves.push_back(w);
            round++;
        }
    } else if (waves.empty() && expandingSpheres.empty()) {
        duel.attackTimer = 0.0f;
        duel.duelPhase = DuelPhase::BOX_TRANSITION_OUT;
        duel.screenShakeTimer = 0.0f;
        duel.screenShakeMag = 0.0f;
        duel.boxTransitionTimer = 0.0f;
        duel.boxTrailCount = 0;
        duel.soulTrailCount = 0;
        duel.soulExitStartX = duel.soulX;
        duel.soulExitStartY = duel.soulY;
        spikeEmergence = 0.0f;
        spikeScrollTimer = 0.0f;
        return;
    }

    // Update active waves
    for (size_t wi = 0; wi < waves.size(); wi++) {
        auto& w = waves[wi];
        if (!w.active) continue;

        w.timer += dt;

        if (w.phase == Phase::TRACKING) {
            w.angle += w.angleSpeed * dt;
            w.centerX = soulCX;
            w.centerY = soulCY;
            w.radius = 60.0f;

            float cycle = fmodf(w.timer, BLINK_PERIOD) / BLINK_PERIOD;
            duel.enemyGlowAlpha = (cycle >= 0.50f) ? 0.4f : 0.0f;
            duel.enemyColor = C2D_Color32(255, 255, 255, 255);

            if (w.timer >= (BLINK_PERIOD * 3.0f)) {
                w.phase = Phase::LOCKED;
                w.timer = 0.0f;
                w.centerX = soulCX;
                w.centerY = soulCY;
                w.angle = (float)w.waveRound * WAVE_ANGLE_OFFSET;
                duel.playSwordAppearSound();
            }
        } else if (w.phase == Phase::LOCKED) {
            w.radius = 60.0f;
            if (w.timer >= LOCKED_DUR) {
                w.phase = Phase::COLLAPSE;
                w.timer = 0.0f;
                duel.enemyScaleX = 1.25f;
                duel.enemyScaleY = 1.25f;
            }
        } else if (w.phase == Phase::COLLAPSE) {
            float p = std::min(1.0f, w.timer / COLLAPSE_DUR);
            w.radius = 60.0f * (1.0f - p);

            if (duel.curHp > 0 && duel.iFrameTimer <= 0.0f) {
                bool hit = false;
                for (int i = 0; i < SPHERE_COUNT; i++) {
                    float th = w.angle + (float)i * (6.2831853f / (float)SPHERE_COUNT);
                    float sx = w.centerX + cosf(th) * w.radius;
                    float sy = w.centerY + sinf(th) * w.radius;
                    float dx = soulCX - sx, dy = soulCY - sy;
                    if ((dx * dx + dy * dy) <= (11.0f * 11.0f)) {
                        hit = true;
                        break;
                    }
                }
                if (!hit && fabsf(w.radius) <= 10.0f) {
                    float dx = soulCX - w.centerX, dy = soulCY - w.centerY;
                    if ((dx * dx + dy * dy) <= (14.0f * 14.0f)) hit = true;
                }

                if (hit) {
                    duel.applyDamage(34, 18, 0.45f);
                } else if (duel.curHp > 0) {
                    bool nearSpheres = false;
                    for (int i = 0; i < SPHERE_COUNT; i++) {
                        float th = w.angle + (float)i * (6.2831853f / (float)SPHERE_COUNT);
                        float sx = w.centerX + cosf(th) * w.radius;
                        float sy = w.centerY + sinf(th) * w.radius;
                        float dx = soulCX - sx, dy = soulCY - sy;
                        if (sqrtf(dx * dx + dy * dy) <= 24.0f) {
                            nearSpheres = true;
                            break;
                        }
                    }
                    if (nearSpheres) duel.triggerGraze(4.0f * dt, true);
                }
            }

            if (w.timer >= COLLAPSE_DUR) {
                duel.playCutSound();
                duel.screenShakeTimer = 0.12f;
                duel.screenShakeMag = 4.0f;
                duel.slashImpactFlash = 0.05f;
                duel.spawnCrossFlare(w.centerX, w.centerY, 30.0f, 0.18f);
                duel.spawnSlashSparks(w.centerX, w.centerY, 0.0f, 0.0f, 10, 100.0f, 260.0f);

                for (int i = 0; i < SPHERE_COUNT; i++) {
                    float th = w.angle + (float)i * (6.2831853f / (float)SPHERE_COUNT);
                    ExpandingSphere sp;
                    sp.x = w.centerX;
                    sp.y = w.centerY;
                    sp.vx = -cosf(th) * EXPAND_SPEED;
                    sp.vy = -sinf(th) * EXPAND_SPEED;
                    sp.radius = 7.0f;
                    sp.lifetime = 0.0f;
                    sp.maxLifetime = 1.4f;
                    sp.active = true;
                    sp.trailCount = w.trailCount;
                    for (int t = 0; t < MAX_TRAIL; t++) {
                        sp.trail[t] = w.trails[i][t];
                    }
                    expandingSpheres.push_back(sp);
                }
                w.active = false;
            }
        }

        if (w.active) {
            w.trailTimer += dt;
            if (w.trailTimer >= TRAIL_INTERVAL) {
                w.trailTimer = 0.0f;
                for (int i = 0; i < SPHERE_COUNT; i++) {
                    float th = w.angle + (float)i * (6.2831853f / (float)SPHERE_COUNT);
                    float sx = w.centerX + cosf(th) * w.radius;
                    float sy = w.centerY + sinf(th) * w.radius;
                    for (int t = MAX_TRAIL - 1; t > 0; t--) {
                        w.trails[i][t] = w.trails[i][t - 1];
                    }
                    w.trails[i][0] = { sx, sy };
                }
                if (w.trailCount < MAX_TRAIL) w.trailCount++;
            }
        }
    }

    waves.erase(
        std::remove_if(waves.begin(), waves.end(),
            [](const OrbitWave& w) { return !w.active; }),
        waves.end()
    );
}

void AttackOrbitSpheres::draw(EggDuelState& duel, float shakeX, float shakeY) {
    if (spikeEmergence > 0.005f && duel.spikeImg.tex) {
        float boxX = duel.ARENA_CENTER_X - (duel.ARENA_SIZE_ATTACK2 * 0.5f);
        float boxY = duel.ARENA_CENTER_Y - (duel.ARENA_SIZE_ATTACK2 * 0.5f);
        float innerLeft = boxX + 2.0f + shakeX;
        float innerTop = boxY + 2.0f + shakeY;
        float innerW = duel.ARENA_SIZE_ATTACK2 - 4.0f;
        float innerH = duel.ARENA_SIZE_ATTACK2 - 4.0f;
        float innerRight = innerLeft + innerW;
        float innerBottom = innerTop + innerH;

        setScissorBox(innerLeft, innerTop, innerW, innerH);

        const float scale = duel.ATTACK2_SPIKE_SCALE;
        const float spikeWidth = 8.0f * scale;
        const float spikeHeight = 7.0f * scale;
        const float halfH = spikeHeight * 0.5f;
        const float emergedOffset = spikeHeight * spikeEmergence;
        const float scrollSpeed = duel.ATTACK2_SPIKE_SCROLL_SPD;
        float scrollOffset = fmodf(spikeScrollTimer * scrollSpeed, spikeWidth);

        int countX = (int)(innerW / spikeWidth) + 3;
        int countY = (int)(innerH / spikeWidth) + 3;

        // Top wall
        for (int i = -1; i < countX; i++) {
            float px = innerLeft + (float)i * spikeWidth - scrollOffset + (spikeWidth * 0.5f);
            float py = innerTop - halfH + emergedOffset;
            C2D_DrawImageAtRotated(duel.spikeImg, px, py, 0.44f, 3.14159265f, nullptr, scale, scale);
        }
        // Bottom wall
        for (int i = -1; i < countX; i++) {
            float px = innerLeft + (float)i * spikeWidth + scrollOffset + (spikeWidth * 0.5f);
            float py = innerBottom + halfH - emergedOffset;
            C2D_DrawImageAtRotated(duel.spikeImg, px, py, 0.44f, 0.0f, nullptr, scale, scale);
        }
        // Left wall
        for (int i = -1; i < countY; i++) {
            float px = innerLeft - halfH + emergedOffset;
            float py = innerTop + (float)i * spikeWidth + scrollOffset + (spikeWidth * 0.5f);
            C2D_DrawImageAtRotated(duel.spikeImg, px, py, 0.44f, 1.570796f, nullptr, scale, scale);
        }
        // Right wall
        for (int i = -1; i < countY; i++) {
            float px = innerRight + halfH - emergedOffset;
            float py = innerTop + (float)i * spikeWidth - scrollOffset + (spikeWidth * 0.5f);
            C2D_DrawImageAtRotated(duel.spikeImg, px, py, 0.44f, -1.570796f, nullptr, scale, scale);
        }

        disableScissor();
    }

    // Expanding spheres and their fading motion trails
    for (size_t i = 0; i < expandingSpheres.size(); i++) {
        const auto& sp = expandingSpheres[i];
        if (!sp.active) continue;

        for (int t = sp.trailCount - 1; t >= 0; t--) {
            float fadeRatio = 1.0f - ((float)(t + 1) / (float)(sp.trailCount + 1));
            u8 trailAlpha = (u8)(fadeRatio * 150.0f);
            if (trailAlpha < 8) continue;

            float r = sp.radius * (0.60f + 0.40f * fadeRatio);
            drawCircle(sp.trail[t].x + shakeX, sp.trail[t].y + shakeY, r, C2D_Color32(255, 255, 255, trailAlpha), 0.45f);
        }

        drawCircle(sp.x + shakeX, sp.y + shakeY, sp.radius, C2D_Color32(255, 255, 255, 255), 0.46f);
    }

    // Orbiting waves and trails
    for (size_t wi = 0; wi < waves.size(); wi++) {
        const auto& w = waves[wi];
        if (!w.active) continue;

        u8 alpha = 102;
        if (w.phase == Phase::TRACKING) {
            float cycle = fmodf(w.timer, BLINK_PERIOD) / BLINK_PERIOD;
            alpha = (cycle >= 0.50f) ? 255 : 102;
        } else {
            alpha = 255;
        }

        u32 sphereColor = C2D_Color32(255, 255, 255, alpha);

        for (int i = 0; i < SPHERE_COUNT; i++) {
            for (int t = w.trailCount - 1; t >= 0; t--) {
                float fadeRatio = 1.0f - ((float)(t + 1) / (float)(w.trailCount + 1));
                u8 trailAlpha = (u8)(fadeRatio * (float)alpha * 0.55f);
                if (trailAlpha < 6) continue;

                float r = 7.0f * (0.60f + 0.40f * fadeRatio);
                drawCircle(w.trails[i][t].x + shakeX, w.trails[i][t].y + shakeY, r, C2D_Color32(255, 255, 255, trailAlpha), 0.45f);
            }

            float th = w.angle + (float)i * (6.2831853f / (float)SPHERE_COUNT);
            float sx = w.centerX + cosf(th) * w.radius + shakeX;
            float sy = w.centerY + sinf(th) * w.radius + shakeY;
            drawCircle(sx, sy, 7.0f, sphereColor, 0.46f);
        }
    }
}
