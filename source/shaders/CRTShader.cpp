#include "ShaderManager.hpp"
#include <cmath>

void ShaderManager::drawCRT(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY) {
    if (strength <= 0.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    // Spherical barrel distortion (True curvature on both X and Y axes)
    float stripH = 2.0f; // 2px height for smooth curve
    int numStrips = (int)(rt.img.subtex->height / stripH);
    if (numStrips <= 0) return;

    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / (float)numStrips;

    float centerY = (rt.img.subtex->height * scaleY) * 0.5f;
    float centerX = (rt.img.subtex->width * scaleX) * 0.5f;
    float invCenterY = (centerY > 0.0f) ? (1.0f / centerY) : 1.0f;
    float texW_scaleX = rt.img.subtex->width * scaleX;
    float curveAmount = 0.03f * strength;

    Tex3DS_SubTexture baseSub = *rt.img.subtex;
    baseSub.height = (u16)stripH;

    C2D_Image strip = rt.img;

    for (int i = 0; i < numStrips; i++) {
        float origDrawY = (float)i * stripH * scaleY;
        float distY = (origDrawY - centerY) * invCenterY;
        float scaleVal = 1.0f - (distY * distY * curveAmount);

        float drawW = texW_scaleX * scaleVal;
        float drawX = x + centerX - (drawW * 0.5f);
        float drawY = centerY + distY * scaleVal * centerY;

        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = baseSub;
        tempSubtex.top = origTop + ((float)i * uvH);
        tempSubtex.bottom = origTop + ((float)(i + 1) * uvH);

        strip.subtex = &tempSubtex;
        C2D_DrawImageAt(strip, drawX, y + drawY, 0.0f, nullptr, scaleX * scaleVal, scaleY * scaleVal);
    }

    // Scanlines
    u8 scanAlpha = (u8)(80.0f * strength);
    if (scanAlpha > 0) {
        u32 scanColor = C2D_Color32(0, 0, 0, scanAlpha);
        float scanW = texW_scaleX;
        int imgH = rt.img.subtex->height;
        for (int i = 0; i < imgH; i += 4) {
            C2D_DrawRectSolid(x, y + (float)i, 0.0f, scanW, 2.0f, scanColor);
        }
    }
}
