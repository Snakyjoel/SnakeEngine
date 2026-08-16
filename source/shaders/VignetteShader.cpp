#include "ShaderManager.hpp"

static void drawGradientRect(float x, float y, float w, float h, u32 c1, u32 c2, u32 c3, u32 c4) {
    C2D_DrawTriangle(x, y, c1, x + w, y, c2, x, y + h, c3, 0.9f);
    C2D_DrawTriangle(x + w, y, c2, x + w, y + h, c4, x, y + h, c3, 0.9f);
}

void ShaderManager::drawVignette(const RT& rt, C3D_RenderTarget* dest, float opacity, float size, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
    
    float w = rt.img.subtex->width * scaleX;
    float h = rt.img.subtex->height * scaleY;
    float thick = (size > 0.0f) ? size : 40.0f;
    
    u32 black = C2D_Color32(0, 0, 0, (u8)(opacity * 255.0f));
    u32 trans = C2D_Color32(0, 0, 0, 0);
    
    drawGradientRect(x, y, w, thick, black, black, trans, trans);
    drawGradientRect(x, y + h - thick, w, thick, trans, trans, black, black);
    drawGradientRect(x, y, thick, h, black, trans, black, trans);
    drawGradientRect(x + w - thick, y, thick, h, trans, black, trans, black);
    
    C2D_Flush();
}
