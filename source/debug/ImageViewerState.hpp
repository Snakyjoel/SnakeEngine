#pragma once
#include "../backend/MusicBeatState.hpp"
#include <citro2d.h>
#include <vector>
#include <string>

class ImageViewerState : public MusicBeatState {
public:
    ImageViewerState(const std::string& folder = "") : modFolder(folder) {}
    
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    std::string modFolder;
    std::vector<std::string> imageFiles;
    int curSelected = 0;
    
    C2D_TextBuf textBuf;
    
    // Viewer properties
    bool isViewing = false;
    C3D_Tex* viewTex = nullptr;
    Tex3DS_SubTexture* viewSubtex = nullptr;
    C2D_Image viewImg;
    C2D_SpriteSheet viewSheet = nullptr;
    
    // Panning
    float panX = 0.0f;
    float panY = 0.0f;
    float zoomLevel = 1.0f;

    void scanFiles();
    void openImage();
    void closeImage();
};
