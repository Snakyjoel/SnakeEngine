#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <cmath>

void ShaderManager::drawWave(
    const RT& rt,
    C3D_RenderTarget* dest,
    float speed,
    float xStrength,
    float yStrength,
    float x,
    float y,
    float scaleX,
    float scaleY
) {
    if (xStrength <= 0.0f && yStrength <= 0.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    float time = Conductor::songPosition / 1000.0f;
    float stripH = 4.0f;
    int numStrips = (int)(rt.img.subtex->height / stripH);
    if (numStrips <= 0) return;

    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / numStrips;
    float textureWidth = (float)rt.img.subtex->width;

    float horizontalStretch = 1.0f;
    if (xStrength > 0.0f && textureWidth > 0.0f) {
        horizontalStretch = 1.0f + ((xStrength * 2.0f) / textureWidth);
    }

    float verticalStretch = 1.0f;
    if (yStrength > 0.0f && stripH > 0.0f) {
        verticalStretch = 1.0f + ((yStrength * 2.0f) / stripH);
    }

    float drawScaleX = scaleX * horizontalStretch;
    float drawScaleY = scaleY * verticalStretch;
    float extraWidth  = (textureWidth * (horizontalStretch - 1.0f) * scaleX) * 0.5f;
    float extraHeight = (stripH * (verticalStretch - 1.0f) * scaleY) * 0.5f;

    float xPhaseBase = time * speed;
    float xPhaseStep = stripH * 0.05f;
    float yPhaseBase = time * speed * 0.7f + 1.57f;
    float yPhaseStep = stripH * 0.08f;

    Tex3DS_SubTexture baseSub = *rt.img.subtex;
    baseSub.height = (u16)stripH;

    C2D_Image strip = rt.img;

    for (int i = 0; i < numStrips; i++) {
        float offsetY = i * stripH * scaleY;

        float xOffset = 0.0f;
        if (xStrength > 0.0f) {
            xOffset = sinf(xPhaseBase + (float)i * xPhaseStep) * xStrength;
        }

        float yOffset = 0.0f;
        if (yStrength > 0.0f) {
            yOffset = sinf(yPhaseBase + (float)i * yPhaseStep) * yStrength;
        }

        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = baseSub;
        tempSubtex.top = origTop + ((float)i * uvH);
        tempSubtex.bottom = origTop + ((float)(i + 1) * uvH);

        strip.subtex = &tempSubtex;

        float drawX = x + xOffset - extraWidth;
        float drawY = y + offsetY + yOffset - extraHeight;

        C2D_DrawImageAt(strip, drawX, drawY, 0.0f, nullptr, drawScaleX, drawScaleY);
    }
}
