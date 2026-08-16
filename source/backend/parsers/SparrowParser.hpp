#pragma once
#include <string>
#include <vector>
#include <citro2d.h>

struct Frame {
    std::string name;
    int x, y, w, h;
    int frameX, frameY, frameW, frameH;
    int index;
    int sheetIdx = 0;
    bool rotated; // true = frame rotated 90° CW in the atlas (Tex-Packer-EX)
    Tex3DS_SubTexture uv;
    C3D_Tex* tex; // Pointer to the texture this frame belongs to
};

struct Animation {
    std::string name;
    std::string prefix; // prefix (name in XML)
    std::vector<int> indices;
    int fps;
    bool loop;
    float offsetX;
    float offsetY;
};

class SparrowParser {
public:
    static void parseXml(const std::string& xmlPath, std::vector<Frame>& outFrames, int* outAtlasW = nullptr, int* outAtlasH = nullptr, bool* outSplit = nullptr, int* outCols = nullptr, int* outRows = nullptr);
};
