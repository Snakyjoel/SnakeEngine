#include "SparrowParser.hpp"
#include <fstream>
#include <sstream>
#include <cmath>

static int safeStoi(const std::string& s) {
    if (s.empty()) return 0;
    char* end;
    double val = std::strtod(s.c_str(), &end);
    return (end == s.c_str()) ? 0 : (int)std::round(val);
}

static std::vector<int> splitStringInt(const std::string& s, char delim) {
    std::vector<int> res;
    if (s.empty()) return res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            char* end;
            double val = std::strtod(item.c_str(), &end);
            if (end != item.c_str()) {
                res.push_back((int)std::round(val));
            }
        }
    }
    return res;
}

void SparrowParser::parseXml(const std::string& xmlPath, std::vector<Frame>& outFrames, int* outAtlasW, int* outAtlasH, bool* outSplit, int* outCols, int* outRows) {
    std::ifstream file(xmlPath);
    if (!file.is_open()) return;

    std::string line;
    outFrames.clear();
    if (outAtlasW) *outAtlasW = 0;
    if (outAtlasH) *outAtlasH = 0;
    if (outSplit) *outSplit = false;
    if (outCols) *outCols = 1;
    if (outRows) *outRows = 1;
    
    bool isSplit = false;
    int atlasW = 0;
    int atlasH = 0;
    std::vector<int> splitX;
    std::vector<int> splitY;
    int cols = 1;
    int rows = 1;

    while (std::getline(file, line)) {
        if (line.find("<TextureAtlas") != std::string::npos) {
            auto getVal = [&](const std::string& key) {
                size_t pos = line.find(key + "=\"");
                if (pos == std::string::npos) return std::string("");
                pos += key.length() + 2;
                size_t end = line.find("\"", pos);
                if (end == std::string::npos) return std::string("");
                return line.substr(pos, end - pos);
            };
            atlasW = safeStoi(getVal("width"));
            atlasH = safeStoi(getVal("height"));
            if (outAtlasW) *outAtlasW = atlasW;
            if (outAtlasH) *outAtlasH = atlasH;
            
            std::string splitAttr = getVal("split");
            if (splitAttr == "true" || splitAttr == "1") {
                isSplit = true;
                if (outSplit) *outSplit = true;
                
                std::string sx = getVal("splitX");
                std::string sy = getVal("splitY");
                splitX = splitStringInt(sx, ',');
                splitY = splitStringInt(sy, ',');
                
                splitX.push_back(atlasW);
                splitY.push_back(atlasH);
                cols = (int)splitX.size();
                rows = (int)splitY.size();
                if (outCols) *outCols = cols;
                if (outRows) *outRows = rows;
            }
        } else if (line.find("<SubTexture") != std::string::npos) {
            Frame f;
            auto getVal = [&](const std::string& key) {
                size_t pos = line.find(key + "=\"");
                if (pos == std::string::npos) return std::string("");
                pos += key.length() + 2;
                size_t end = line.find("\"", pos);
                if (end == std::string::npos) return std::string("");
                return line.substr(pos, end - pos);
            };
            
            f.name = getVal("name");
            f.x = safeStoi(getVal("x"));
            f.y = safeStoi(getVal("y"));
            f.w = safeStoi(getVal("width"));
            f.h = safeStoi(getVal("height"));
            f.frameX = safeStoi(getVal("frameX"));
            f.frameY = safeStoi(getVal("frameY"));
            f.frameW = safeStoi(getVal("frameWidth"));
            f.frameH = safeStoi(getVal("frameHeight"));

            if (isSplit) {
                int col = 0;
                for (size_t i = 0; i < splitX.size(); ++i) {
                    if (f.x < splitX[i]) {
                        col = (int)i;
                        break;
                    }
                }
                
                int row = 0;
                for (size_t i = 0; i < splitY.size(); ++i) {
                    if (f.y < splitY[i]) {
                        row = (int)i;
                        break;
                    }
                }
                
                f.sheetIdx = col + row * cols;
                f.x = f.x - (col > 0 ? splitX[col - 1] : 0);
                f.y = f.y - (row > 0 ? splitY[row - 1] : 0);
            } else {
                f.sheetIdx = 0;
            }

            std::string rotVal = getVal("rotated");
            f.rotated = (rotVal == "true" || rotVal == "1");

            if (f.frameW == 0) f.frameW = f.rotated ? f.h : f.w;
            if (f.frameH == 0) f.frameH = f.rotated ? f.w : f.h;
            
            f.index = (int)outFrames.size();
            outFrames.push_back(f);
        }
    }
}
