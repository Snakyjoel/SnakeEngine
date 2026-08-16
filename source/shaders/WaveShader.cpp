#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <math.h>

void ShaderManager::drawWave(const RT& rt, C3D_RenderTarget* dest, float speed, float xStrength, float yStrength, float x, float y, float scaleX, float scaleY) {
    float time = Conductor::songPosition / 1000.0f;
    
    // Fixed 4px strip height for good quality/performance balance
    float stripH   = 4.0f;
    int numStrips  = (int)(rt.img.subtex->height / stripH);
    
    float origTop    = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH        = (origBottom - origTop) / numStrips;
    
    for (int i = 0; i < numStrips; i++) {
        float offsetY = i * stripH * scaleY;
        
        // Horizontal (X) wave: based on vertical position
        float xOffset = (xStrength > 0.0f) ? sinf(time * speed + (i * stripH * 0.05f)) * xStrength : 0.0f;
        // Vertical (Y) wave: based on horizontal "column" position — approximated using strip index with phase offset
        float yOffset = (yStrength > 0.0f) ? sinf(time * speed * 0.7f + (i * stripH * 0.08f + 1.57f)) * yStrength : 0.0f;
        
        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex        = *rt.img.subtex;
        tempSubtex.top    = origTop + (i * uvH);
        tempSubtex.bottom = origTop + ((i + 1) * uvH);
        tempSubtex.height = (u16)stripH;
        
        C2D_Image strip = rt.img;
        strip.subtex    = &tempSubtex;
        
        C2D_DrawImageAt(strip, x + xOffset, y + offsetY + yOffset, 0.0f, nullptr, scaleX, scaleY);
    }
}
