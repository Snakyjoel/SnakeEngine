// Reworked because it looked like bullshit in Thorns
#include "ShaderManager.hpp"
#include "backend/Conductor.hpp"
#include <math.h>

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
    float time = Conductor::songPosition / 1000.0f;

    // Smaller strips make the wave look smoother,
    // while still keeping the amount of draw calls reasonable.
    float stripH = 4.0f;

    int numStrips =
        (int)(rt.img.subtex->height / stripH);

    float origTop =
        rt.img.subtex->top;

    float origBottom =
        rt.img.subtex->bottom;

    float uvH =
        (origBottom - origTop) / numStrips;

    float textureWidth =
        (float)rt.img.subtex->width;

    // Stretch the strips horizontally so moving them around
    // doesn't expose the empty area at the sides.
    float horizontalStretch = 1.0f;

    if (xStrength > 0.0f && textureWidth > 0.0f) {
        horizontalStretch =
            1.0f +
            ((xStrength * 2.0f) / textureWidth);
    }

    // The strips can move up or down independently.
    // Making them taller prevents gaps from appearing between them.
    float verticalStretch = 1.0f;

    if (yStrength > 0.0f && stripH > 0.0f) {
        verticalStretch =
            1.0f +
            ((yStrength * 2.0f) / stripH);
    }

    // Make the strip wider and taller to hide the gaps
    // created by the wave movement.
    float drawScaleX = scaleX * horizontalStretch;
    float drawScaleY = scaleY * verticalStretch;

    // Keep the extra size centered around the original strip.
    float extraWidth  = (textureWidth * (horizontalStretch - 1.0f) * scaleX) * 0.5f;
    float extraHeight = (stripH * (verticalStretch - 1.0f) * scaleY) * 0.5f;

    for (int i = 0; i < numStrips; i++) {

        // Original vertical position of this strip.
        float offsetY =
            i * stripH * scaleY;

        // Move each strip sideways to create the horizontal wave.
        float xOffset = 0.0f;

        if (xStrength > 0.0f) {
            xOffset =
                sinf(
                    time * speed +
                    (i * stripH * 0.05f)
                ) * xStrength;
        }

        // Give the strips a slightly different vertical position
        // to create the second part of the wave.
        float yOffset = 0.0f;

        if (yStrength > 0.0f) {
            yOffset =
                sinf(
                    time * speed * 0.7f +
                    (i * stripH * 0.08f + 1.57f)
                ) * yStrength;
        }

        // Use a temporary subtexture for this particular strip.
        Tex3DS_SubTexture& tempSubtex =
            tempSubtexs[i % 512];

        tempSubtex =
            *rt.img.subtex;

        tempSubtex.top =
            origTop +
            (i * uvH);

        tempSubtex.bottom =
            origTop +
            ((i + 1) * uvH);

        tempSubtex.height =
            (u16)stripH;

        C2D_Image strip =
            rt.img;

        strip.subtex =
            &tempSubtex;

        // Apply the wave offsets and extra padding centering.
        float drawX =
            x + xOffset - extraWidth;

        float drawY =
            y + offsetY + yOffset - extraHeight;

        C2D_DrawImageAt(
            strip,
            drawX,
            drawY,
            0.0f,
            nullptr,
            drawScaleX,
            drawScaleY
        );
    }
}