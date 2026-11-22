#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <3ds.h>
#include <cmath>
#include <algorithm>

void ShaderManager::drawCRT(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY) {
    if (!rt.active || !rt.img.subtex) return;

    if (strength <= 0.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    float time = (Conductor::songPosition > 0.0f) ? (Conductor::songPosition / 1000.0f) : ((float)(osGetTime() % 1000000) / 1000.0f);

    float totalW = rt.img.subtex->width * scaleX;
    float totalH = rt.img.subtex->height * scaleY;
    float cx = totalW * 0.5f;
    float cy = totalH * 0.5f;

    // Greenish phosphor tint & subtle analog flicker matching tvcrt.frag
    float flicker = 1.0f - (sinf(time * 60.0f) * 0.015f * strength);
    u8 r = (u8)std::min(255.0f, std::max(0.0f, 240.0f * flicker));
    u8 g = (u8)std::min(255.0f, std::max(0.0f, 255.0f * flicker));
    u8 b = (u8)std::min(255.0f, std::max(0.0f, 240.0f * flicker));
    C2D_ImageTint crtTint;
    C2D_PlainImageTint(&crtTint, C2D_Color32(r, g, b, 255), 1.0f);
    C2D_SetTintMode(C2D_TintMult);

    // 1. Horizontal barrel distortion matching tvcrt curve function
    float stripH = 2.0f;
    int numStrips = (int)(rt.img.subtex->height / stripH);
    if (numStrips <= 0) return;

    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / (float)numStrips;

    Tex3DS_SubTexture baseSub = *rt.img.subtex;
    baseSub.height = (u16)stripH;

    C2D_Image strip = rt.img;

    for (int i = 0; i < numStrips; i++) {
        float drawY = (float)i * stripH * scaleY;
        float ny = (cy > 0.0f) ? ((drawY - cy) / cy) : 0.0f;
        
        // Exact horizontal curvature formula from tvcrt.frag: uv.x * (1 + (ny*1.1/5.0)^2)
        float curveXFactor = 1.0f + (0.0484f * ny * ny * strength);
        float scaleVal = (1.0f / curveXFactor);

        float drawW = totalW * scaleVal;
        float drawX = x + cx - (drawW * 0.5f);

        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = baseSub;
        tempSubtex.top = origTop + ((float)i * uvH);
        tempSubtex.bottom = origTop + ((float)(i + 1) * uvH);

        strip.subtex = &tempSubtex;
        C2D_DrawImageAt(strip, drawX, y + drawY, 0.0f, &crtTint, scaleX * scaleVal, scaleY);
    }

    C2D_Flush();
    C2D_SetTintMode(C2D_TintMult);

    // 2. Animated rolling scanlines
    u8 scanAlpha = (u8)(75.0f * strength);
    if (scanAlpha > 0) {
        float scroll = fmodf(time * 18.0f, 4.0f);
        u32 scanColor = C2D_Color32(0, 0, 0, scanAlpha);
        for (float sy = -4.0f + scroll; sy < totalH + 4.0f; sy += 4.0f) {
            if (sy + 2.0f <= 0.0f || sy >= totalH) continue;
            C2D_DrawRectSolid(x, y + sy, 0.0f, totalW, 2.0f, scanColor);
        }
    }

    // 3. Curved top and bottom CRT screen border mask matching tvcrt.frag vertical curvature: uv.y * (1 + (nx*1.1/4.0)^2)
    int maskSlices = 40;
    float sliceW = totalW / (float)maskSlices;
    u32 blackBorder = C2D_Color32(0, 0, 0, 255);

    for (int col = 0; col < maskSlices; col++) {
        float colX = (float)col * sliceW;
        float nx = (cx > 0.0f) ? (((colX + sliceW * 0.5f) - cx) / cx) : 0.0f;

        float curveYFactor = 1.0f + (0.075625f * nx * nx * strength);
        float visibleHalfH = cy / curveYFactor;
        float topCurveH = cy - visibleHalfH;

        if (topCurveH > 0.5f) {
            C2D_DrawRectSolid(x + colX, y, 0.0f, sliceW + 0.5f, topCurveH, blackBorder);
            C2D_DrawRectSolid(x + colX, y + totalH - topCurveH, 0.0f, sliceW + 0.5f, topCurveH + 0.5f, blackBorder);
        }
    }

    // 4. CRT tube corner vignette
    u8 vigAlpha = (u8)(110.0f * strength);
    if (vigAlpha > 0) {
        float borderW = 36.0f * strength;
        float borderH = 28.0f * strength;
        u32 cDark = C2D_Color32(0, 0, 0, vigAlpha);
        u32 cClear = C2D_Color32(0, 0, 0, 0);

        C2D_DrawRectangle(x, y, 0.0f, totalW, borderH, cDark, cDark, cClear, cClear);
        C2D_DrawRectangle(x, y + totalH - borderH, 0.0f, totalW, borderH, cClear, cClear, cDark, cDark);
        C2D_DrawRectangle(x, y, 0.0f, borderW, totalH, cDark, cClear, cDark, cClear);
        C2D_DrawRectangle(x + totalW - borderW, y, 0.0f, borderW, totalH, cClear, cDark, cClear, cDark);
    }
}
