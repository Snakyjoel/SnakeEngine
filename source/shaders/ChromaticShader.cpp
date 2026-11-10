#include "ShaderManager.hpp"
#include <cmath>

void ShaderManager::drawChromatic(const RT& rt, C3D_RenderTarget* dest, float offset, float x, float y, float scaleX, float scaleY) {
    if (std::abs(offset) < 0.001f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    static bool s_tintsInit = false;
    static C2D_ImageTint s_rTint, s_gTint, s_bTint;
    if (!s_tintsInit) {
        C2D_PlainImageTint(&s_rTint, C2D_Color32(255, 0, 0, 255), 1.0f);
        C2D_PlainImageTint(&s_gTint, C2D_Color32(0, 255, 0, 255), 1.0f);
        C2D_PlainImageTint(&s_bTint, C2D_Color32(0, 0, 255, 255), 1.0f);
        s_tintsInit = true;
    }

    // Step 1: Draw Red channel normally to properly occlude background based on alpha
    C2D_SetTintMode(C2D_TintMult);
    C2D_DrawImageAt(rt.img, x - offset, y, 0.0f, &s_rTint, scaleX, scaleY);
    C2D_Flush();

    // Step 2: Draw Green and Blue channels additively over the Red channel without altering alpha
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ZERO, GPU_ONE);
    C2D_DrawImageAt(rt.img, x, y, 0.0f, &s_gTint, scaleX, scaleY);
    C2D_DrawImageAt(rt.img, x + offset, y, 0.0f, &s_bTint, scaleX, scaleY);
    C2D_Flush();

    // Restore premultiplied blend
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
}
