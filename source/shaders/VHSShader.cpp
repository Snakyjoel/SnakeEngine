#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <math.h>

void ShaderManager::drawVHS(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY) {
    float time = Conductor::songPosition / 1000.0f;
    C2D_Flush();
    
    float vhsOffset = (0.5f + sinf(time * 15.0f) * 0.5f) * 4.0f * strength;
    drawChromatic(rt, dest, vhsOffset, x, y, scaleX, scaleY);
    
    float rollY = fmodf(time * 80.0f, rt.img.subtex->height * scaleY);
    C2D_DrawRectSolid(x, y + rollY, 0.9f, rt.img.subtex->width * scaleX, 1.0f, C2D_Color32(255, 255, 255, 80 * strength));
    
    if (fmodf(time, 0.4f) < 0.05f) {
        float jitterY = fmodf(sinf(time * 43.12f) * 5000.0f, rt.img.subtex->height * scaleY);
        C2D_DrawRectSolid(x, y + jitterY, 0.9f, rt.img.subtex->width * scaleX, 3.0f, C2D_Color32(0, 0, 0, 100 * strength));
    }
    C2D_Flush();
}
