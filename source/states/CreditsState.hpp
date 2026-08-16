#pragma once
#include <string>
#include <vector>
#include "../backend/MusicBeatState.hpp"
#include "../backend/ModHandler.hpp"
#include "SparrowParser.hpp"
#include <citro2d.h>

struct CreditEntry {
    bool isTitle = false;
    std::string text1; // For title, or name for entry
    std::string text2; // Description for entry
};

struct CreditsGroup {
    std::string name;
    std::string iconFrame; // from sheet or empty for mod
    bool isMod = false;
    std::string modFolder;
    std::string musicPath;
    std::vector<CreditEntry> entries;
    
    // Mod icon loading
    C2D_Image modIcon = {nullptr, nullptr};
    C3D_Tex* manualTex = nullptr;
    Tex3DS_SubTexture* manualSub = nullptr;
    C2D_SpriteSheet modSheet = nullptr;
    bool iconLoaded = false;
};

class CreditsState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    void drawScrollText(const std::string& text, float x, float y, float scale, bool centered, float border, u32 color, float wrapWidth = 0.0f);
    void loadModIcon(CreditsGroup& group);
    void freeModIcon(CreditsGroup& group);
    void updateIconCache();
    void parseCreditsFile(CreditsGroup& group, const std::string& filePath);

    int curSelected = 0;
    float scrollPercent = 0.0f;
    std::vector<CreditsGroup> groups;
    
    // Spritesheet for built-in icons
    C2D_SpriteSheet iconSheet = nullptr;
    std::vector<Frame> iconFrames;
    
    // Font details
    C2D_Font vcrFont;
    C2D_TextBuf vcrFontBuf;

    C2D_SpriteSheet bgSheet = nullptr;
    C2D_SpriteSheet bottomBGSheet = nullptr;
    C2D_Image topBG;
    C2D_Image bottomBG;

    // Menu state
    enum SubState {
        STATE_SELECTING,
        STATE_SCROLLING
    };
    SubState subState = STATE_SELECTING;

    // Scrolling credits state
    float scrollY = 240.0f;
    float scrollSpeed = 45.0f; // pixels per second
    float totalScrollHeight = 0.0f;
    bool musicPlaying = false;
};
