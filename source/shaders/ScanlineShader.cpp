#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <cmath>

static inline void drawGradientRect(float x, float y, float w, float h, u32 c1, u32 c2, u32 c3, u32 c4) {
    C2D_DrawTriangle(x, y, c1, x + w, y, c2, x, y + h, c3, 0.9f);
    C2D_DrawTriangle(x + w, y, c2, x + w, y + h, c4, x, y + h, c3, 0.9f);
}

void ShaderManager::drawScanlineRoll(const RT& rt, C3D_RenderTarget* dest, float speed, float opacity, float x, float y, float scaleX, float scaleY) {
    C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
    if (opacity <= 0.001f) return;

    float w = rt.img.subtex->width * scaleX;
    float h = rt.img.subtex->height * scaleY;
    float time = Conductor::songPosition / 1000.0f;

    float rollSpeed = (speed > 0.0f) ? speed : 100.0f;
    float yPos = fmodf(time * rollSpeed, h);

    float halfBarH = 15.0f * scaleY;
    u8 alphaVal = (u8)(opacity * 160.0f);
    if (alphaVal == 0) return;

    u32 black = C2D_Color32(0, 0, 0, alphaVal);
    u32 trans = C2D_Color32(0, 0, 0, 0);

    drawGradientRect(x, y + yPos - halfBarH, w, halfBarH, trans, trans, black, black);
    drawGradientRect(x, y + yPos,            w, halfBarH, black, black, trans, trans);
    C2D_Flush();
}
