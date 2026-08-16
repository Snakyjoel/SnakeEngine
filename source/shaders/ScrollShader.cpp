#include "ShaderManager.hpp"
#include <math.h>

void ShaderManager::drawScroll(const std::string& camera, const RT& rt, C3D_RenderTarget* dest, float speedX, float speedY, float zoom, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    
    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;
    
    // Retrieve accumulated scroll positions (continuous, doesn't teleport on variable speed changes)
    float scrollX = fmodf(scrollAccumulators[camera].first, w);
    float scrollY = fmodf(scrollAccumulators[camera].second, h);
    
    if (scrollX < 0.0f) scrollX += w;
    if (scrollY < 0.0f) scrollY += h;
    
    float splitX = w - scrollX;
    float splitY = h - scrollY;
    
    float zoomVal = (zoom > 0.0f) ? zoom : 1.0f;
    
    float centerX = (w * scaleX) / 2.0f;
    float centerY = (h * scaleY) / 2.0f;
    float zoomOffsetX = centerX * (1.0f - zoomVal);
    float zoomOffsetY = centerY * (1.0f - zoomVal);
    
    // Draw helper lambda
    auto drawPart = [&](float texX0, float texY0, float partW, float partH, float destX, float destY, int subtexIdx) {
        if (partW <= 0.0f || partH <= 0.0f) return;
        
        Tex3DS_SubTexture& sub = tempSubtexs[subtexIdx];
        sub.width = (u16)partW;
        sub.height = (u16)partH;
        
        // Map pixel range [texX0 .. texX0 + partW] to UV left/right
        sub.left = (rt.img.subtex->left * rt.tex.width + texX0) / (float)rt.tex.width;
        sub.right = (rt.img.subtex->left * rt.tex.width + texX0 + partW) / (float)rt.tex.width;
        
        // Map pixel range [texY0 .. texY0 + partH] to UV top/bottom (note: Y axis is inverted in UV)
        // 1.0f in UV is Y=0. bottom of texture (e.g. 0.0625) is Y=240.
        // So UV = 1.0f - Y / rt.tex.height
        float texY_top = rt.tex.height * (1.0f - rt.img.subtex->top) + texY0;
        sub.top = 1.0f - (texY_top / (float)rt.tex.height);
        sub.bottom = 1.0f - ((texY_top + partH) / (float)rt.tex.height);
        
        C2D_Image img = rt.img;
        img.subtex = &sub;
        
        C2D_DrawImageAt(img, destX, destY, 0.0f, nullptr, scaleX * zoomVal, scaleY * zoomVal);
    };
    
    // Step 1: Top-Left quadrant (texture [splitX .. w], [splitY .. h])
    drawPart(splitX, splitY, scrollX, scrollY, 
             x + zoomOffsetX, 
             y + zoomOffsetY, 0);
             
    // Step 2: Top-Right quadrant (texture [0 .. splitX], [splitY .. h])
    drawPart(0.0f, splitY, splitX, scrollY, 
             x + zoomOffsetX + scrollX * scaleX * zoomVal, 
             y + zoomOffsetY, 1);
             
    // Step 3: Bottom-Left quadrant (texture [splitX .. w], [0 .. splitY])
    drawPart(splitX, 0.0f, scrollX, splitY, 
             x + zoomOffsetX, 
             y + zoomOffsetY + scrollY * scaleY * zoomVal, 2);
             
    // Step 4: Bottom-Right quadrant (texture [0 .. splitX], [0 .. splitY])
    drawPart(0.0f, 0.0f, splitX, splitY, 
             x + zoomOffsetX + scrollX * scaleX * zoomVal, 
             y + zoomOffsetY + scrollY * scaleY * zoomVal, 3);
             
    C2D_Flush();
}
