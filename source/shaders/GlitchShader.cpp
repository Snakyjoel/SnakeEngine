#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <math.h>

void ShaderManager::drawGlitchSkew(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float userStripH, float x, float y, float scaleX, float scaleY) {
    float time = Conductor::songPosition / 100.0f; // Fast time
    
    float stripH = (userStripH > 0.0f) ? userStripH : 16.0f; // Default 16px
    int numStrips = rt.img.subtex->height / stripH;
    
    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / numStrips;
    
    for (int i = 0; i < numStrips; i++) {
        float offsetY = i * stripH;
        // Pseudo-random glitch offset
        float randomVal = fmod(sinf(time * 12.3f + i * 8.4f) * 43758.5453f, 1.0f);
        float waveOffset = (randomVal > 0.95f) ? (sinf(time * speed) * strength * randomVal) : 0.0f;
        
        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = *rt.img.subtex;
        tempSubtex.top = origTop + (i * uvH);
        tempSubtex.bottom = origTop + ((i + 1) * uvH);
        tempSubtex.height = stripH;
        
        C2D_Image strip = rt.img;
        strip.subtex = &tempSubtex;
        
        C2D_DrawImageAt(strip, x + waveOffset, y + offsetY, 0.0f, nullptr, scaleX, scaleY);
    }
}
