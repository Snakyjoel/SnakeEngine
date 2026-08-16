#include "ShaderManager.hpp"

void ShaderManager::drawBlur(const RT& rt, C3D_RenderTarget* dest, float radius, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    
    // To achieve a true box blur without additive blending overriding issues,
    // Use accumulated fractional alpha blending.
    
    // Step 1: 100% opacity
    C2D_DrawImageAt(rt.img, x + radius, y, 0.0f, nullptr, scaleX, scaleY);
    
    // Step 2: 50% opacity (averages 1 and 2 equally)
    C2D_ImageTint tint2; C2D_AlphaImageTint(&tint2, 0.5f);
    C2D_DrawImageAt(rt.img, x - radius, y, 0.0f, &tint2, scaleX, scaleY);
    
    // Step 3: 33% opacity (averages 1, 2, and 3 equally)
    C2D_ImageTint tint3; C2D_AlphaImageTint(&tint3, 0.33f);
    C2D_DrawImageAt(rt.img, x, y + radius, 0.0f, &tint3, scaleX, scaleY);
    
    // Step 4: 25% opacity (averages all 4 equally)
    C2D_ImageTint tint4; C2D_AlphaImageTint(&tint4, 0.25f);
    C2D_DrawImageAt(rt.img, x, y - radius, 0.0f, &tint4, scaleX, scaleY);
    
    C2D_Flush();
}
