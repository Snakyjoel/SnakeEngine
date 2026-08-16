#pragma once
#include <string>
#include <vector>
#include "../backend/MusicBeatState.hpp"
#include <citro2d.h>
#include "EggRoomState.hpp"
#include "ResultState.hpp"

class DebugMenuState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;
    
    static void dumpFile(const std::string& path);
    
private:
    int curSelected = 0;
    float lerpSelected = 0.0f;
    float gridOffset = 0.0f;

    // Background images
    C2D_SpriteSheet bgSheet       = nullptr;
    C2D_SpriteSheet bottomBGSheet = nullptr;
    C2D_Image topBG;
    C2D_Image bottomBG;

    struct DebugMenuItem {
        std::string name;
        std::string desc;
        int id;
    };
    std::vector<DebugMenuItem> menuItems;

    C2D_Font   vcrFont    = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
};
