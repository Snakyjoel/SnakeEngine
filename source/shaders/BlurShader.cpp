#include "ShaderManager.hpp"

void ShaderManager::drawBlur(const RT& rt, C3D_RenderTarget* dest, float radius, float x, float y, float scaleX, float scaleY) {
    if (!rt.active || !rt.img.subtex) return;

    if (radius <= 0.01f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    static bool s_tintsInit = false;
    static C2D_ImageTint s_tint2;
    static C2D_ImageTint s_tint3;
    static C2D_ImageTint s_tint4;
    if (!s_tintsInit) {
        // Pre-scaled RGB for GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA blend
        C2D_PlainImageTint(&s_tint2, C2D_Color32(128, 128, 128, 128), 1.0f);
        C2D_PlainImageTint(&s_tint3, C2D_Color32(85, 85, 85, 85), 1.0f);
        C2D_PlainImageTint(&s_tint4, C2D_Color32(64, 64, 64, 64), 1.0f);
        s_tintsInit = true;
    }

    C2D_DrawImageAt(rt.img, x + radius, y, 0.0f, nullptr, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x - radius, y, 0.0f, &s_tint2, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x, y + radius, 0.0f, &s_tint3, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x, y - radius, 0.0f, &s_tint4, scaleX, scaleY);
    C2D_Flush();
}
