#include "ShaderManager.hpp"

void ShaderManager::drawChromatic(const RT& rt, C3D_RenderTarget* dest, float offset, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    
    // Step 1: Draw Red channel normally to properly occlude background based on alpha
    C2D_SetTintMode(C2D_TintMult);
    C2D_ImageTint rTint; C2D_PlainImageTint(&rTint, C2D_Color32(255, 0, 0, 255), 1.0f);
    C2D_DrawImageAt(rt.img, x - offset, y, 0.0f, &rTint, scaleX, scaleY);
    C2D_Flush();
    
    // Step 2: Draw Green and Blue channels additively over the Red channel without altering alpha
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ZERO, GPU_ONE);
    
    C2D_ImageTint gTint; C2D_PlainImageTint(&gTint, C2D_Color32(0, 255, 0, 255), 1.0f);
    C2D_DrawImageAt(rt.img, x, y, 0.0f, &gTint, scaleX, scaleY);
    
    C2D_ImageTint bTint; C2D_PlainImageTint(&bTint, C2D_Color32(0, 0, 255, 255), 1.0f);
    C2D_DrawImageAt(rt.img, x + offset, y, 0.0f, &bTint, scaleX, scaleY);
    
    C2D_Flush();
    
    // Restore premultiplied blend
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
}
