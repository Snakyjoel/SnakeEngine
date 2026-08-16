#include "ShaderManager.hpp"

// Each pass multiplies the framebuffer by the grayscale image, exponentially
// pushing midtones to black while keeping near-white pixels white.
// value1 = number of multiply passes (default 8; higher = harsher threshold)
void ShaderManager::drawBW(const RT& rt, C3D_RenderTarget* dest, float passes, float x, float y, float scaleX, float scaleY) {
    int numPasses = (passes > 0.0f) ? (int)passes : 8;
    if (numPasses < 1) numPasses = 1;
    if (numPasses > 24) numPasses = 24; // Cap to avoid excessive draw calls
    
    // Step 1: Draw grayscale source image into helperRT (normal blend)
    C2D_SceneBegin(helperRT.target);
    C2D_TargetClear(helperRT.target, C2D_Color32(0, 0, 0, 255));
    
    C2D_SetTintMode(C2D_TintLuma);
    C2D_ImageTint grayTint;
    C2D_PlainImageTint(&grayTint, C2D_Color32(255, 255, 255, 255), 1.0f);
    C2D_DrawImageAt(rt.img, 0, 0, 0.0f, &grayTint, 1.0f, 1.0f);
    C2D_Flush();
    C2D_SetTintMode(C2D_TintMult);
    
    // Step 2: Repeatedly multiply helperRT by the grayscale source ---
    // Blend equation: result = src * dst  (src=grayscale image, dst=helperRT)
    // GPU blend: result = src * GPU_DST_COLOR + dst * GPU_ZERO
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_DST_COLOR, GPU_ZERO,
                   GPU_DST_ALPHA, GPU_ZERO);
    
    C2D_SetTintMode(C2D_TintLuma);
    for (int i = 0; i < numPasses; i++) {
        C2D_DrawImageAt(rt.img, 0, 0, 0.0f, &grayTint, 1.0f, 1.0f);
        C2D_Flush();
    }
    C2D_SetTintMode(C2D_TintMult);
    
    // Restore premultiplied blend
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    
    // Step 3: Present helperRT to dest ---
    C2D_SceneBegin(dest);
    C2D_DrawImageAt(helperRT.img, x, y, 0.0f, nullptr, scaleX, scaleY);
    C2D_Flush();
}
