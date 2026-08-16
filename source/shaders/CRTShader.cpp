#include "ShaderManager.hpp"
#include <math.h>

void ShaderManager::drawCRT(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    
    // Spherical barrel distortion (True curvature on both X and Y axes)
    float stripH = 2.0f; // 2px height for smooth curve
    int numStrips = rt.img.subtex->height / stripH;
    
    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / numStrips;
    
    float centerY = (rt.img.subtex->height * scaleY) / 2.0f;
    float centerX = (rt.img.subtex->width * scaleX) / 2.0f;
    
    for (int i = 0; i < numStrips; i++) {
        float origDrawY = i * stripH * scaleY;
        
        // Normalized distance from center Y [-1.0 to 1.0]
        float distY = (origDrawY - centerY) / centerY; 
        
        // Curve equation: narrow the width and height at the top and bottom
        float curveAmount = 0.03f * strength; // 3% curvature
        float scaleVal = 1.0f - (distY * distY * curveAmount);
        
        float drawW = rt.img.subtex->width * scaleX * scaleVal;
        float drawX = x + centerX - (drawW / 2.0f);
        
        // Spherical vertical distortion: shift Y position towards the center
        float drawY = centerY + distY * scaleVal * centerY;
        
        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = *rt.img.subtex;
        tempSubtex.top = origTop + (i * uvH);
        tempSubtex.bottom = origTop + ((i + 1) * uvH);
        tempSubtex.height = stripH; // CRITICAL: Fixes vertical stretching!
        
        C2D_Image strip = rt.img;
        strip.subtex = &tempSubtex;
        
        C2D_DrawImageAt(strip, drawX, y + drawY, 0.0f, nullptr, scaleX * scaleVal, scaleY * scaleVal);
    }
    C2D_Flush();
    
    // Scanlines
    for (int i = 0; i < rt.img.subtex->height; i += 4) {
        C2D_DrawRectSolid(x, y + i, 0.0f, rt.img.subtex->width * scaleX, 2.0f, C2D_Color32(0, 0, 0, 80 * strength));
    }
}
