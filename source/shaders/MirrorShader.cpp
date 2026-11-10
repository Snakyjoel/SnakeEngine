#include "ShaderManager.hpp"

void ShaderManager::drawMirror(const RT& rt, C3D_RenderTarget* dest, float type, float x, float y, float scaleX, float scaleY) {
    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;

    float origLeft   = rt.img.subtex->left;
    float origRight  = rt.img.subtex->right;
    float origTop    = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float midU       = (origLeft + origRight) * 0.5f;
    float midV       = (origTop + origBottom) * 0.5f;

    float halfW = (w * 0.5f) * scaleX;
    float halfH = (h * 0.5f) * scaleY;

    int mode = (int)type;

    if (mode == 0) {
        // Horizontal mirror: left half mirrored onto right side
        Tex3DS_SubTexture& sub = tempSubtexs[0];
        sub.width  = (u16)(w * 0.5f);
        sub.height = (u16)h;
        sub.left   = origLeft;
        sub.right  = midU;
        sub.top    = origTop;
        sub.bottom = origBottom;

        C2D_Image img = rt.img;
        img.subtex = &sub;

        // Normal left half [0 .. halfW]
        C2D_DrawImageAt(img, x, y, 0.0f, nullptr, scaleX, scaleY);
        // Flipped left half [halfW .. w]
        C2D_DrawImageAt(img, x + halfW, y, 0.0f, nullptr, -scaleX, scaleY);

    } else if (mode == 1) {
        // Vertical mirror: top half mirrored onto bottom side
        Tex3DS_SubTexture& sub = tempSubtexs[0];
        sub.width  = (u16)w;
        sub.height = (u16)(h * 0.5f);
        sub.left   = origLeft;
        sub.right  = origRight;
        sub.top    = origTop;
        sub.bottom = midV;

        C2D_Image img = rt.img;
        img.subtex = &sub;

        // Normal top half [0 .. halfH]
        C2D_DrawImageAt(img, x, y, 0.0f, nullptr, scaleX, scaleY);
        // Flipped top half [halfH .. h]
        C2D_DrawImageAt(img, x, y + halfH, 0.0f, nullptr, scaleX, -scaleY);

    } else {
        // Quad mirror: top-left quadrant mirrored to all 4 quadrants
        Tex3DS_SubTexture& sub = tempSubtexs[0];
        sub.width  = (u16)(w * 0.5f);
        sub.height = (u16)(h * 0.5f);
        sub.left   = origLeft;
        sub.right  = midU;
        sub.top    = origTop;
        sub.bottom = midV;

        C2D_Image img = rt.img;
        img.subtex = &sub;

        // 1. Top-Left (normal)
        C2D_DrawImageAt(img, x, y, 0.0f, nullptr, scaleX, scaleY);
        // 2. Top-Right (horizontal flip)
        C2D_DrawImageAt(img, x + halfW, y, 0.0f, nullptr, -scaleX, scaleY);
        // 3. Bottom-Left (vertical flip)
        C2D_DrawImageAt(img, x, y + halfH, 0.0f, nullptr, scaleX, -scaleY);
        // 4. Bottom-Right (both flipped)
        C2D_DrawImageAt(img, x + halfW, y + halfH, 0.0f, nullptr, -scaleX, -scaleY);
    }

    C2D_Flush();
}
