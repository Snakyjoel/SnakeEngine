#pragma once
#include "../backend/MusicBeatState.hpp"
#include <citro2d.h>
#include <vector>
#include <string>

class SnakyPlayerState : public MusicBeatState {
public:
    SnakyPlayerState(const std::string& folder = "") : modFolder(folder) {}
    
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    std::string modFolder;
    std::vector<std::string> snakyFiles;
    int curSelected = 0;
    
    C2D_TextBuf textBuf;

    void scanFiles();
    void playSelected();
};
