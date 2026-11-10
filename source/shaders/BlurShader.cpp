#include "ShaderManager.hpp"

void ShaderManager::drawBlur(const RT& rt, C3D_RenderTarget* dest, float radius, float x, float y, float scaleX, float scaleY) {
    if (radius <= 0.01f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    static bool s_tintsInit = false;
    static C2D_ImageTint s_tint2;
    static C2D_ImageTint s_tint3;
    static C2D_ImageTint s_tint4;
    if (!s_tintsInit) {
        C2D_AlphaImageTint(&s_tint2, 0.50f);
        C2D_AlphaImageTint(&s_tint3, 0.33f);
        C2D_AlphaImageTint(&s_tint4, 0.25f);
        s_tintsInit = true;
    }

    // Accumulated fractional alpha blending for 4-tap box blur
    C2D_DrawImageAt(rt.img, x + radius, y, 0.0f, nullptr, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x - radius, y, 0.0f, &s_tint2, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x, y + radius, 0.0f, &s_tint3, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x, y - radius, 0.0f, &s_tint4, scaleX, scaleY);
    C2D_Flush();
}
