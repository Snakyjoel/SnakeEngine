#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <math.h>

void ShaderManager::drawDrugs(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float colorSpeed, float x, float y, float scaleX, float scaleY) {
    float time = Conductor::songPosition / 1000.0f;
    
    // Step 1: Render 2D wave into helperRT
    C2D_SceneBegin(helperRT.target);
    C2D_TargetClear(helperRT.target, C2D_Color32(0, 0, 0, 255));
    
    float stripH   = 4.0f;
    int numStrips  = (int)(rt.img.subtex->height / stripH);
    
    float origTop    = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH        = (origBottom - origTop) / numStrips;
    
    for (int i = 0; i < numStrips; i++) {
        float baseY = i * stripH * scaleY;
        
        // X wave: horizontal sinusoidal displacement
        float xOffset = sinf(time * speed        + i * stripH * 0.06f) * strength;
        // Y wave: vertical displacement (90° phase shifted, different frequency)
        float yOffset = sinf(time * speed * 0.8f + i * stripH * 0.09f + 1.5708f) * (strength * 0.6f);
        
        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex        = *rt.img.subtex;
        tempSubtex.top    = origTop + (i       * uvH);
        tempSubtex.bottom = origTop + ((i + 1) * uvH);
        tempSubtex.height = (u16)stripH;
        
        C2D_Image strip = rt.img;
        strip.subtex    = &tempSubtex;
        
        C2D_DrawImageAt(strip, xOffset, baseY + yOffset, 0.0f, nullptr, scaleX, scaleY);
    }
    C2D_Flush();
    
    // Step 2: Draw helperRT onto dest with rainbow colour tint ---
    C2D_SceneBegin(dest);
    
    float cs = (colorSpeed > 0.0f) ? colorSpeed : 2.0f;
    u8 r = (u8)(sinf(time * cs)               * 127.5f + 127.5f);
    u8 g = (u8)(sinf(time * cs + 2.0944f)     * 127.5f + 127.5f); // 2 pi/3 offset
    u8 b = (u8)(sinf(time * cs + 4.1888f)     * 127.5f + 127.5f); // 4 pi/3 offset
    
    C2D_SetTintMode(C2D_TintMult);
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 255), 1.0f);
    C2D_DrawImageAt(helperRT.img, x, y, 0.0f, &tint, scaleX, scaleY);
    C2D_Flush();
    C2D_SetTintMode(C2D_TintMult); // Restore (default mode)
}
