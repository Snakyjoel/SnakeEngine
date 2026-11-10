#include "ShaderManager.hpp"
#include <cmath>
#include <algorithm>

void ShaderManager::drawBloom(const std::string& camera, const RT& rt, C3D_RenderTarget* dest, float strength, float threshold, float radius, float x, float y, float scaleX, float scaleY) {
    if (strength <= 0.001f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    C2D_Flush();

    // Use dedicated bloomRT buffer (guaranteed never to alias with rt or dest)
    int helperHeight = isCameraExtended(camera) ? 512 : 256;
    if (!bloomRT.active || bloomRT.tex.height != helperHeight) {
        if (bloomRT.active) bloomRT.cleanup();
        bloomRT.init(512, helperHeight);
    }

    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;

    // 1/4 resolution downsample for minimal fillrate cost and smooth blur
    float downW = ceilf(w * 0.25f);
    float downH = ceilf(h * 0.25f);
    if (downW < 1.0f) downW = 1.0f;
    if (downH < 1.0f) downH = 1.0f;

    float scaleDownX = downW / w;
    float scaleDownY = downH / h;

    // Step 1: Render downsampled image into bloomRT
    C2D_SceneBegin(bloomRT.target);
    C2D_TargetClear(bloomRT.target, C2D_Color32(0, 0, 0, 0));
    C3D_TexSetFilter(&bloomRT.tex, GPU_LINEAR, GPU_LINEAR);

    C2D_DrawImageAt(rt.img, 0, 0, 0.0f, nullptr, scaleDownX, scaleDownY);
    C2D_Flush();

    // Step 2: Smooth-knee bright-pass extraction via multiplicative thresholding
    float thresh = (threshold > 0.0f) ? threshold : 0.45f;
    if (thresh > 0.05f) {
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                       GPU_DST_COLOR, GPU_ZERO,
                       GPU_DST_ALPHA, GPU_ZERO);
        C2D_DrawImageAt(rt.img, 0, 0, 0.0f, nullptr, scaleDownX, scaleDownY);
        C2D_Flush();

        if (thresh > 0.65f) {
            // Aggressive threshold for specular highlights
            C2D_DrawImageAt(rt.img, 0, 0, 0.0f, nullptr, scaleDownX, scaleDownY);
            C2D_Flush();
        }
    }

    // Step 3: Draw base scene to dest with standard blend
    C2D_SceneBegin(dest);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
    C2D_Flush();

    // Step 4: Additively composite the upscaled blurred bloom glow
    Tex3DS_SubTexture subtex = *bloomRT.img.subtex;
    float texWidth  = (float)bloomRT.tex.width;
    float texHeight = (float)bloomRT.tex.height;

    subtex.left   = 0.0f;
    subtex.right  = downW / texWidth;
    subtex.top    = 1.0f;
    subtex.bottom = 1.0f - (downH / texHeight);
    subtex.width  = (u16)downW;
    subtex.height = (u16)downH;

    C2D_Image bloomImg = bloomRT.img;
    bloomImg.subtex = &subtex;

    float scaleUpX = w / downW;
    float scaleUpY = h / downH;

    // Enable additive blend
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ONE,
                   GPU_ZERO, GPU_ONE);

    float rad = (radius > 0.0f) ? radius : 1.0f;
    float s = std::min(strength, 3.0f);

    if (rad <= 1.0f) {
        // Single bilinear tap: hardware linear filtering smoothly spreads the glow
        u8 c = (u8)(std::min(s * 255.0f, 255.0f));
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(c, c, c, 255), 1.0f);
        C2D_DrawImageAt(bloomImg, x, y, 0.0f, &tint, scaleX * scaleUpX, scaleY * scaleUpY);

        if (s > 1.0f) {
            u8 extraC = (u8)((s - 1.0f) * 255.0f);
            C2D_ImageTint extraTint;
            C2D_PlainImageTint(&extraTint, C2D_Color32(extraC, extraC, extraC, 255), 1.0f);
            C2D_DrawImageAt(bloomImg, x, y, 0.0f, &extraTint, scaleX * scaleUpX, scaleY * scaleUpY);
        }
    } else {
        // 4-tap diagonal spread for wider halos
        float r = (rad - 1.0f) * 1.5f;
        u8 tapC = (u8)(std::min(s * 0.25f * 255.0f, 255.0f));
        C2D_ImageTint tapTint;
        C2D_PlainImageTint(&tapTint, C2D_Color32(tapC, tapC, tapC, 255), 1.0f);

        C2D_DrawImageAt(bloomImg, x - r, y - r, 0.0f, &tapTint, scaleX * scaleUpX, scaleY * scaleUpY);
        C2D_DrawImageAt(bloomImg, x + r, y - r, 0.0f, &tapTint, scaleX * scaleUpX, scaleY * scaleUpY);
        C2D_DrawImageAt(bloomImg, x - r, y + r, 0.0f, &tapTint, scaleX * scaleUpX, scaleY * scaleUpY);
        C2D_DrawImageAt(bloomImg, x + r, y + r, 0.0f, &tapTint, scaleX * scaleUpX, scaleY * scaleUpY);
    }
    C2D_Flush();

    // Restore standard blend
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
}
