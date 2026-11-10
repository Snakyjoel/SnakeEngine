#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <cmath>

void ShaderManager::drawGlitchSkew(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float userStripH, float x, float y, float scaleX, float scaleY) {
    if (strength <= 0.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    float time = Conductor::songPosition / 100.0f; // Fast time
    float stripH = (userStripH > 0.0f) ? userStripH : 16.0f; // Default 16px
    int numStrips = (int)(rt.img.subtex->height / stripH);
    if (numStrips <= 0) return;

    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / (float)numStrips;

    float sinSpeed = sinf(time * speed) * strength;
    float baseRandSeed = time * 12.3f;

    Tex3DS_SubTexture baseSub = *rt.img.subtex;
    baseSub.height = (u16)stripH;

    C2D_Image strip = rt.img;

    for (int i = 0; i < numStrips; i++) {
        float offsetY = (float)i * stripH;
        float randomVal = fmodf(sinf(baseRandSeed + (float)i * 8.4f) * 43758.5453f, 1.0f);
        float waveOffset = (randomVal > 0.95f) ? (sinSpeed * randomVal) : 0.0f;

        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = baseSub;
        tempSubtex.top = origTop + ((float)i * uvH);
        tempSubtex.bottom = origTop + ((float)(i + 1) * uvH);

        strip.subtex = &tempSubtex;
        C2D_DrawImageAt(strip, x + waveOffset, y + offsetY, 0.0f, nullptr, scaleX, scaleY);
    }
}
