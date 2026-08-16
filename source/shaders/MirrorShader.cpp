#include "ShaderManager.hpp"

void ShaderManager::drawMirror(const RT& rt, C3D_RenderTarget* dest, float type, float x, float y, float scaleX, float scaleY) {
    C2D_Flush();
    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;
    
    float origLeft   = rt.img.subtex->left;
    float origRight  = rt.img.subtex->right;
    float origTop    = rt.img.subtex->top;
    float origBottom = rt.img.subtex->bottom;
    float midU       = (origLeft + origRight) * 0.5f;
    float midV       = (origTop + origBottom) * 0.5f;
    
    if (type == 0.0f) {
        // Horizontal mirror: draw left half, then draw left half again with UVs flipped horizontally
        Tex3DS_SubTexture& leftSub  = tempSubtexs[0];
        Tex3DS_SubTexture& rightSub = tempSubtexs[1];
        leftSub  = *rt.img.subtex;
        rightSub = *rt.img.subtex;
        
        // Left panel: UV [origLeft .. midU]
        leftSub.width = (u16)(w / 2.0f);
        leftSub.left  = origLeft;
        leftSub.right = midU;
        
        // Right panel: UV [midU .. origLeft] (reversed = mirror of left)
        rightSub.width = (u16)(w / 2.0f);
        rightSub.left  = midU;
        rightSub.right = origLeft;
        
        C2D_Image leftImg  = rt.img; leftImg.subtex  = &leftSub;
        C2D_Image rightImg = rt.img; rightImg.subtex = &rightSub;
        
        float halfW = (w / 2.0f) * scaleX;
        C2D_DrawImageAt(leftImg,  x,          y, 0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        C2D_DrawImageAt(rightImg, x + halfW,  y, 0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        
    } else if (type == 1.0f) {
        // Vertical mirror: draw top half, then draw top half with UVs flipped vertically
        Tex3DS_SubTexture& topSub = tempSubtexs[0];
        Tex3DS_SubTexture& botSub = tempSubtexs[1];
        topSub = *rt.img.subtex;
        botSub = *rt.img.subtex;
        
        // Top panel: UV [origTop .. midV]
        topSub.height = (u16)(h / 2.0f);
        topSub.top    = origTop;
        topSub.bottom = midV;
        
        // Bottom panel: UV [midV .. origTop] (reversed = mirror of top)
        botSub.height = (u16)(h / 2.0f);
        botSub.top    = midV;
        botSub.bottom = origTop;
        
        C2D_Image topImg = rt.img; topImg.subtex = &topSub;
        C2D_Image botImg = rt.img; botImg.subtex = &botSub;
        
        float halfH = (h / 2.0f) * scaleY;
        C2D_DrawImageAt(topImg, x, y,          0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        C2D_DrawImageAt(botImg, x, y + halfH,  0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        
    } else {
        // Quad mirror: 4 quadrants, all show top-left mirrored
        Tex3DS_SubTexture& tlSub = tempSubtexs[0];
        Tex3DS_SubTexture& trSub = tempSubtexs[1];
        Tex3DS_SubTexture& blSub = tempSubtexs[2];
        Tex3DS_SubTexture& brSub = tempSubtexs[3];
        
        // TL quadrant: UV [origLeft..midU, origTop..midV]
        tlSub = *rt.img.subtex;
        tlSub.width = (u16)(w / 2.0f); tlSub.height = (u16)(h / 2.0f);
        tlSub.left = origLeft; tlSub.right = midU;
        tlSub.top  = origTop;  tlSub.bottom = midV;
        
        // TR quadrant: mirror TL horizontally (swap left/right)
        trSub = tlSub;
        trSub.left = midU; trSub.right = origLeft;
        
        // BL quadrant: mirror TL vertically (swap top/bottom)
        blSub = tlSub;
        blSub.top = midV; blSub.bottom = origTop;
        
        // BR quadrant: mirror TL both axes
        brSub = tlSub;
        brSub.left = midU; brSub.right = origLeft;
        brSub.top  = midV; brSub.bottom = origTop;
        
        C2D_Image tlImg = rt.img; tlImg.subtex = &tlSub;
        C2D_Image trImg = rt.img; trImg.subtex = &trSub;
        C2D_Image blImg = rt.img; blImg.subtex = &blSub;
        C2D_Image brImg = rt.img; brImg.subtex = &brSub;
        
        float halfW = (w / 2.0f) * scaleX;
        float halfH = (h / 2.0f) * scaleY;
        C2D_DrawImageAt(tlImg, x,          y,          0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        C2D_DrawImageAt(trImg, x + halfW,  y,          0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        C2D_DrawImageAt(blImg, x,          y + halfH,  0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
        C2D_DrawImageAt(brImg, x + halfW,  y + halfH,  0.0f, nullptr, scaleX, scaleY);
        C2D_Flush();
    }
}
