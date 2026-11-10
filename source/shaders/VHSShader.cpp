#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <cmath>

void ShaderManager::drawVHS(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY) {
    if (strength <= 0.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    float time = Conductor::songPosition / 1000.0f;
    float vhsOffset = (0.5f + sinf(time * 15.0f) * 0.5f) * 4.0f * strength;
    drawChromatic(rt, dest, vhsOffset, x, y, scaleX, scaleY);

    float rollY = fmodf(time * 80.0f, rt.img.subtex->height * scaleY);
    C2D_DrawRectSolid(x, y + rollY, 0.9f, rt.img.subtex->width * scaleX, 1.0f, C2D_Color32(255, 255, 255, (u8)(80.0f * strength)));

    if (fmodf(time, 0.4f) < 0.05f) {
        float jitterY = fmodf(sinf(time * 43.12f) * 5000.0f, rt.img.subtex->height * scaleY);
        C2D_DrawRectSolid(x, y + jitterY, 0.9f, rt.img.subtex->width * scaleX, 3.0f, C2D_Color32(0, 0, 0, (u8)(100.0f * strength)));
    }
    C2D_Flush();
}
