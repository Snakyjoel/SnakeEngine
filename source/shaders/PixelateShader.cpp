#include "ShaderManager.hpp"
#include <math.h>

void ShaderManager::drawPixelate(const RT& rt, C3D_RenderTarget* dest, float pixelSize, float x, float y, float scaleX, float scaleY) {
    if (pixelSize <= 1.0f) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }
    
    C2D_Flush();
    
    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;
    float downW = ceilf(w / pixelSize);
    float downH = ceilf(h / pixelSize);
    if (downW < 1.0f) downW = 1.0f;
    if (downH < 1.0f) downH = 1.0f;
    
    C2D_SceneBegin(helperRT.target);
    C2D_TargetClear(helperRT.target, C2D_Color32(0, 0, 0, 0));
    
    C3D_TexSetFilter(&helperRT.tex, GPU_NEAREST, GPU_NEAREST);
    
    float scaleDownX = downW / w;
    float scaleDownY = downH / h;
    C2D_DrawImageAt(rt.img, 0, 0, 0.0f, nullptr, scaleDownX, scaleDownY);
    C2D_Flush();
    
    C2D_SceneBegin(dest);
    
    Tex3DS_SubTexture subtex = *helperRT.img.subtex;
    float texWidth = helperRT.tex.width;
    float texHeight = helperRT.tex.height;
    
    subtex.left = 0.0f;
    subtex.right = downW / texWidth;
    subtex.top = 1.0f;
    subtex.bottom = 1.0f - (downH / texHeight);
    subtex.width = (u16)downW;
    subtex.height = (u16)downH;
    
    C2D_Image pixelatedImg = helperRT.img;
    pixelatedImg.subtex = &subtex;
    
    float scaleUpX = w / downW;
    float scaleUpY = h / downH;
    C2D_DrawImageAt(pixelatedImg, x, y, 0.0f, nullptr, scaleX * scaleUpX, scaleY * scaleUpY);
    C2D_Flush();
    
    C3D_TexSetFilter(&helperRT.tex, GPU_LINEAR, GPU_NEAREST);
}
