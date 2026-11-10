#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <cmath>

void ShaderManager::drawDrugs(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float colorSpeed, float x, float y, float scaleX, float scaleY) {
    float time = Conductor::songPosition / 1000.0f;
    float cs = (colorSpeed > 0.0f) ? colorSpeed : 2.0f;
    u8 r = (u8)(sinf(time * cs)           * 127.5f + 127.5f);
    u8 g = (u8)(sinf(time * cs + 2.0944f) * 127.5f + 127.5f);
    u8 b = (u8)(sinf(time * cs + 4.1888f) * 127.5f + 127.5f);

    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 255), 1.0f);

    // If strength is 0, skip wave slicing and helperRT pass completely
    if (strength <= 0.0f) {
        C2D_SetTintMode(C2D_TintMult);
        C2D_DrawImageAt(rt.img, x, y, 0.0f, &tint, scaleX, scaleY);
        return;
    }

    // Step 1: Render 2D wave into helperRT
    C2D_SceneBegin(helperRT.target);
    C2D_TargetClear(helperRT.target, C2D_Color32(0, 0, 0, 255));

    float stripH = 4.0f;
    int numStrips = (int)(rt.img.subtex->height / stripH);
    if (numStrips <= 0) return;

    float origTop = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float uvH = (origBottom - origTop) / (float)numStrips;

    float xPhaseBase = time * speed;
    float xPhaseStep = stripH * 0.06f;
    float yPhaseBase = time * speed * 0.8f + 1.5708f;
    float yPhaseStep = stripH * 0.09f;
    float yStrength = strength * 0.6f;

    Tex3DS_SubTexture baseSub = *rt.img.subtex;
    baseSub.height = (u16)stripH;

    C2D_Image strip = rt.img;

    for (int i = 0; i < numStrips; i++) {
        float baseY = (float)i * stripH * scaleY;
        float xOffset = sinf(xPhaseBase + (float)i * xPhaseStep) * strength;
        float yOffset = sinf(yPhaseBase + (float)i * yPhaseStep) * yStrength;

        Tex3DS_SubTexture& tempSubtex = tempSubtexs[i % 512];
        tempSubtex = baseSub;
        tempSubtex.top = origTop + ((float)i * uvH);
        tempSubtex.bottom = origTop + ((float)(i + 1) * uvH);

        strip.subtex = &tempSubtex;
        C2D_DrawImageAt(strip, xOffset, baseY + yOffset, 0.0f, nullptr, scaleX, scaleY);
    }
    C2D_Flush();

    // Step 2: Draw helperRT onto dest with rainbow tint
    C2D_SceneBegin(dest);
    C2D_SetTintMode(C2D_TintMult);
    C2D_DrawImageAt(helperRT.img, x, y, 0.0f, &tint, scaleX, scaleY);
    C2D_Flush();
}
