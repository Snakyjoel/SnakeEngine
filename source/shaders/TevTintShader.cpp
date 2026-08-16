#include "ShaderManager.hpp"

void ShaderManager::drawTevTint(const RT& rt, C3D_RenderTarget* dest, C2D_TintMode mode, u32 color, float blend, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    C2D_SetTintMode(mode);
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, blend);
    C2D_DrawImageAt(rt.img, x, y, 0.0f, &tint, scaleX, scaleY);
    C2D_Flush();
    C2D_SetTintMode(C2D_TintMult);
}

void ShaderManager::drawSaturation(const RT& rt, C3D_RenderTarget* dest, float saturation, float x, float y, float scaleX, float scaleY) {
    if (saturation <= 1.0f) {
        drawTevTint(rt, dest, C2D_TintLuma, C2D_Color32(255, 255, 255, 255), 1.0f - saturation, x, y, scaleX, scaleY);
    } else {
        // Draw normal image first
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        
        // Additive blend the image over itself to intensify colors
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ZERO, GPU_ONE);
        C2D_SetTintMode(C2D_TintMult);
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, (saturation - 1.0f) * 0.35f); 
        C2D_DrawImageAt(rt.img, x, y, 0.0f, &tint, scaleX, scaleY);
        C2D_Flush();
        
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    }
}

void ShaderManager::drawColorDepth(const RT& rt, C3D_RenderTarget* dest, float levels, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    drawTevTint(rt, dest, C2D_TintLuma, C2D_Color32((u8)levels, 255, 255, 255), 1.0f, x, y, scaleX, scaleY);
    C2D_Flush();
}
