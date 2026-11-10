#pragma once
#include <citro2d.h>

class EggDuelState;

enum class SoulGravity {
    DOWN,
    UP,
    LEFT,
    RIGHT
};

static inline void setScissorBox(float x, float y, float w, float h) {
    C2D_Flush();
    float sLeft = y;
    float sRight = y + h;
    float sTop = x;
    float sBottom = x + w;

    if (sLeft < 0.0f) sLeft = 0.0f;
    if (sRight > 240.0f) sRight = 240.0f;
    if (sTop < 0.0f) sTop = 0.0f;
    if (sBottom > 400.0f) sBottom = 400.0f;

    u32 phys_left = (u32)(240.0f - sRight);
    u32 phys_right = (u32)(240.0f - sLeft);
    u32 phys_top = (u32)(400.0f - sBottom);
    u32 phys_bottom = (u32)(400.0f - sTop);

    C3D_SetScissor(GPU_SCISSOR_NORMAL, phys_left, phys_top, phys_right, phys_bottom);
}

static inline void disableScissor() {
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

static inline void drawCircle(float cx, float cy, float radius, u32 color, float depth = 0.45f) {
    const int SIDES = 8;
    const float stepAng = 6.2831853f / (float)SIDES;
    for (int s = 0; s < SIDES; s++) {
        float a1 = (float)s * stepAng;
        float a2 = (float)(s + 1) * stepAng;
        C2D_DrawTriangle(cx, cy, color,
                         cx + cosf(a1) * radius, cy + sinf(a1) * radius, color,
                         cx + cosf(a2) * radius, cy + sinf(a2) * radius, color,
                         depth);
    }
}

class EggAttack {
public:
    virtual ~EggAttack() = default;

    virtual void start(EggDuelState& duel) = 0;
    virtual void update(EggDuelState& duel, float dt) = 0;
    virtual void draw(EggDuelState& duel, float shakeX, float shakeY) = 0;
    virtual void stop(EggDuelState& duel) {}

    virtual float getArenaSize() const { return 152.0f; }
    virtual bool isSoulBlue() const { return false; }
};
