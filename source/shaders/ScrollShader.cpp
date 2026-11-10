#include "ShaderManager.hpp"
#include <cmath>

void ShaderManager::drawScroll(const std::string& camera, const RT& rt, C3D_RenderTarget* dest, float speedX, float speedY, float zoom, float x, float y, float scaleX, float scaleY) {
    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;

    // Retrieve accumulated scroll positions (continuous, doesn't teleport on variable speed changes)
    float scrollX = fmodf(scrollAccumulators[camera].first, w);
    float scrollY = fmodf(scrollAccumulators[camera].second, h);

    if (scrollX < 0.0f) scrollX += w;
    if (scrollY < 0.0f) scrollY += h;

    float zoomVal = (zoom > 0.0f) ? zoom : 1.0f;

    if (scrollX < 0.001f && scrollY < 0.001f && std::abs(zoomVal - 1.0f) < 0.001f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }

    float splitX = w - scrollX;
    float splitY = h - scrollY;

    float centerX = (w * scaleX) * 0.5f;
    float centerY = (h * scaleY) * 0.5f;
    float zoomOffsetX = centerX * (1.0f - zoomVal);
    float zoomOffsetY = centerY * (1.0f - zoomVal);

    float invTexW = 1.0f / (float)rt.tex.width;
    float invTexH = 1.0f / (float)rt.tex.height;
    float baseLeftPx = rt.img.subtex->left * (float)rt.tex.width;
    float baseTopPx = (float)rt.tex.height * (1.0f - rt.img.subtex->top);

    auto drawPart = [&](float texX0, float texY0, float partW, float partH, float destX, float destY, int subtexIdx) {
        if (partW <= 0.0f || partH <= 0.0f) return;

        Tex3DS_SubTexture& sub = tempSubtexs[subtexIdx];
        sub.width = (u16)partW;
        sub.height = (u16)partH;

        sub.left = (baseLeftPx + texX0) * invTexW;
        sub.right = (baseLeftPx + texX0 + partW) * invTexW;

        float texY_top = baseTopPx + texY0;
        sub.top = 1.0f - (texY_top * invTexH);
        sub.bottom = 1.0f - ((texY_top + partH) * invTexH);

        C2D_Image img = rt.img;
        img.subtex = &sub;

        C2D_DrawImageAt(img, destX, destY, 0.0f, nullptr, scaleX * zoomVal, scaleY * zoomVal);
    };

    drawPart(splitX, splitY, scrollX, scrollY, x + zoomOffsetX, y + zoomOffsetY, 0);
    drawPart(0.0f, splitY, splitX, scrollY, x + zoomOffsetX + scrollX * scaleX * zoomVal, y + zoomOffsetY, 1);
    drawPart(splitX, 0.0f, scrollX, splitY, x + zoomOffsetX, y + zoomOffsetY + scrollY * scaleY * zoomVal, 2);
    drawPart(0.0f, 0.0f, splitX, splitY, x + zoomOffsetX + scrollX * scaleX * zoomVal, y + zoomOffsetY + scrollY * scaleY * zoomVal, 3);

    C2D_Flush();
}
