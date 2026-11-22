#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <3ds.h>
#include <cmath>
#include <algorithm>

void ShaderManager::drawDrugs(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float colorSpeed, float x, float y, float scaleX, float scaleY) {
    if (!rt.active || !rt.img.subtex) return;

    float time = (Conductor::songPosition > 0.0f) ? (Conductor::songPosition / 1000.0f) : ((float)(osGetTime() % 1000000) / 1000.0f);
    float cs = (colorSpeed > 0.0f) ? colorSpeed : 2.0f;
    u8 r = (u8)(sinf(time * cs)           * 127.5f + 127.5f);
    u8 g = (u8)(sinf(time * cs + 2.0944f) * 127.5f + 127.5f);
    u8 b = (u8)(sinf(time * cs + 4.1888f) * 127.5f + 127.5f);

    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 255), 1.0f);

    C2D_SceneBegin(dest);
    C2D_SetTintMode(C2D_TintMult);

    if (strength <= 0.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, &tint, scaleX, scaleY);
        C2D_Flush();
        return;
    }

    float stripH = 2.0f;
    int numStrips = (int)(rt.img.subtex->height / stripH);
    if (numStrips <= 0) return;

    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float origLeft = rt.img.subtex->left;
    float origRight = rt.img.subtex->right;
    float uvH = (origBottom - origTop) / (float)numStrips;
    float textureWidth = (float)rt.img.subtex->width;
    float totalW = textureWidth * scaleX;
    float totalH = rt.img.subtex->height * scaleY;

    float texFullW = (float)rt.tex.width;
    float onePixelU = (texFullW > 0.0f) ? (1.0f / texFullW) : (1.0f / 512.0f);

    float xStrength = strength;
    float yStrength = strength * 0.6f;

    float xPhaseBase = time * speed;
    float xPhaseStep = stripH * 0.045f;
    float yPhaseBase = time * speed * 0.85f + 1.57f;
    float yPhaseStep = stripH * 0.065f;

    auto getYOffset = [&](int index) -> float {
        if (yStrength <= 0.0f) return 0.0f;
        float w1 = sinf(yPhaseBase + (float)index * yPhaseStep);
        float w2 = sinf(yPhaseBase * 1.5f + (float)index * yPhaseStep * 1.8f + 1.2f) * 0.3f;
        return (w1 + w2) * yStrength;
    };

    auto getXOffset = [&](int index) -> float {
        if (xStrength <= 0.0f) return 0.0f;
        float w1 = sinf(xPhaseBase + (float)index * xPhaseStep);
        float w2 = sinf(xPhaseBase * 1.7f + (float)index * xPhaseStep * 2.2f + 0.8f) * 0.35f;
        return (w1 + w2) * xStrength;
    };

    C2D_Image strip = rt.img;
    C2D_Image leftClampImg = rt.img;
    C2D_Image rightClampImg = rt.img;

    for (int i = 0; i < numStrips; i++) {
        float baseDrawY = (float)i * stripH * scaleY;
        float curY = y + baseDrawY + getYOffset(i);

        float nextBaseY = (float)(i + 1) * stripH * scaleY;
        float nextY = (i + 1 < numStrips) ? (y + nextBaseY + getYOffset(i + 1)) : (y + totalH + getYOffset(i));

        if (i == 0 && curY > y) {
            curY = y;
        }

        float actualH = (nextY > curY) ? (nextY - curY + 0.6f) : (stripH * scaleY + 0.6f);
        if (i == numStrips - 1 && curY + actualH < y + totalH) {
            actualH = (y + totalH) - curY;
        }
        float drawScaleY = actualH / stripH;

        float xOffset = getXOffset(i);
        float foldCompression = 1.0f;
        if (xStrength > 0.0f) {
            foldCompression = 1.0f - (cosf(xPhaseBase + (float)i * xPhaseStep) * 0.02f * std::min(xStrength / 10.0f, 1.2f));
        }

        float drawW = totalW * foldCompression;
        float drawX = x + xOffset + (totalW - drawW) * 0.5f;

        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 160];
        tempSubtex = *rt.img.subtex;
        tempSubtex.height = (u16)stripH;
        tempSubtex.top = origTop + ((float)i * uvH);
        tempSubtex.bottom = origTop + ((float)(i + 1) * uvH);

        strip.subtex = &tempSubtex;
        C2D_DrawImageAt(strip, drawX, curY, 0.0f, &tint, scaleX * foldCompression, drawScaleY);

        // Pixel stretch clamp on left side
        float leftGap = drawX - x;
        if (leftGap > 0.5f) {
            Tex3DS_SubTexture& leftSub = tempSubtexs[160 + (i % 160)];
            leftSub = tempSubtex;
            leftSub.width = 1;
            leftSub.left = origLeft;
            leftSub.right = origLeft + onePixelU;
            leftClampImg.subtex = &leftSub;
            C2D_DrawImageAt(leftClampImg, x, curY, 0.0f, &tint, leftGap, drawScaleY);
        }

        // Pixel stretch clamp on right side
        float rightGap = (x + totalW) - (drawX + drawW);
        if (rightGap > 0.5f) {
            Tex3DS_SubTexture& rightSub = tempSubtexs[320 + (i % 160)];
            rightSub = tempSubtex;
            rightSub.width = 1;
            rightSub.left = origRight - onePixelU;
            rightSub.right = origRight;
            rightClampImg.subtex = &rightSub;
            C2D_DrawImageAt(rightClampImg, drawX + drawW, curY, 0.0f, &tint, rightGap, drawScaleY);
        }
    }

    C2D_Flush();
}
