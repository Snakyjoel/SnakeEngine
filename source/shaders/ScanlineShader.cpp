#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <math.h>

static void drawGradientRect(float x, float y, float w, float h, u32 c1, u32 c2, u32 c3, u32 c4) {
    C2D_DrawTriangle(x, y, c1, x + w, y, c2, x, y + h, c3, 0.9f);
    C2D_DrawTriangle(x + w, y, c2, x + w, y + h, c4, x, y + h, c3, 0.9f);
}

void ShaderManager::drawScanlineRoll(const RT& rt, C3D_RenderTarget* dest, float speed, float opacity, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
    
    float w = rt.img.subtex->width * scaleX;
    float h = rt.img.subtex->height * scaleY;
    float time = Conductor::songPosition / 1000.0f;
    
    float rollSpeed = (speed > 0.0f) ? speed : 100.0f;
    float yPos = fmodf(time * rollSpeed, h);
    
    float barH = 30.0f * scaleY;
    u32 black = C2D_Color32(0, 0, 0, (u8)(opacity * 160.0f));
    u32 trans = C2D_Color32(0, 0, 0, 0);
    
    drawGradientRect(x, y + yPos - barH / 2.0f, w, barH / 2.0f, trans, trans, black, black);
    drawGradientRect(x, y + yPos, w, barH / 2.0f, black, black, trans, trans);
    
    C2D_Flush();
}
