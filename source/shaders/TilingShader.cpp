#include "ShaderManager.hpp"
#include <math.h>
#include <cmath>

void ShaderManager::drawTiling(const RT& rt, C3D_RenderTarget* dest, float count, float x, float y, float scaleX, float scaleY) {
    int N = (int)std::round(count);
    if (N <= 1) {
        C2D_DrawImageAt(rt.img, x, y, 0.0f, nullptr, scaleX, scaleY);
        return;
    }
    
    C2D_Flush();
    
    int cols = (int)ceilf(sqrtf((float)N));
    int rows = (int)ceilf((float)N / (float)cols);
    
    float w = rt.img.subtex->width;
    float h = rt.img.subtex->height;
    
    float cellW = w / cols;
    float cellH = h / rows;
    
    float drawCellW = cellW * scaleX;
    float drawCellH = cellH * scaleY;
    
    int drawn = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (drawn >= N) break;
            
            float cellX = x + c * drawCellW;
            float cellY = y + r * drawCellH;
            
            float cellScaleX = scaleX / cols;
            float cellScaleY = scaleY / rows;
            
            C2D_DrawImageAt(rt.img, cellX, cellY, 0.0f, nullptr, cellScaleX, cellScaleY);
            drawn++;
        }
    }
    C2D_Flush();
}
