#include <3ds.h>

// FLASH
static float _timer[2] = {0};
static float _duration[2] = {1};
static u32 _color[2] = {0};
static u64 _lastTick[2] = {0};
static bool _active[2] = {false};

void flash(int screen, float duration, u32 color)
{
    int i = (screen == topScreen) ? 0 : 1;

    _timer[i] = duration;
    _duration[i] = duration;
    _color[i] = color;
    _lastTick[i] = svcGetSystemTick();
    _active[i] = true;
}

void drawFlash(int screen)
{
    for (int i = 0; i < 2; i++)
    {
        if (screen != topScreen && i == 0) continue;
        if (screen != bottomScreen && i == 1) continue;

        if (!_active[i]) continue;

        u64 now = svcGetSystemTick();
        float dt = (float)(now - _lastTick[i]) / SYSCLOCK_ARM11;
        _lastTick[i] = now;

        _timer[i] -= dt;
        if (_timer[i] < 0.0f) _timer[i] = 0.0f;

        float alpha = _timer[i] / _duration[i];
        u8 a = (u8)(alpha * 255.0f);

        u32 c = _color[i];
        u8 r = c & 0xFF;
        u8 g = (c >> 8) & 0xFF;
        u8 b = (c >> 16) & 0xFF;

        u32 final = C2D_Color32(r, g, b, a);

        float w = (i == 0) ? ScreenWidthTop : ScreenWidthBot;

        C2D_DrawRectSolid(0, 0, 0.95f, w, ScreenHeight, final);

        if (_timer[i] <= 0.0f)
            _active[i] = false;
    }
}