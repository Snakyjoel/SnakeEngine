#pragma once
#include "../backend/MusicBeatState.hpp"
#include <string>
#include <vector>
#include <citro2d.h>

class OutdatedState : public MusicBeatState {
public:
    OutdatedState(int compResult, const std::string& onlineVer, const std::string& source = "gamebanana");
    ~OutdatedState() override;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;

private:
    int comparison;
    std::string latestVersion;
    std::string updateSource;
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    C2D_TextBuf textBuf = nullptr;
    float gridScrollTime;
    struct CachedSpritesheet* bgLogoSheet;
    struct CachedSpritesheet* hifellaSheet;
    C2D_Text titleText;
    C2D_Text bodyText[12];
    std::vector<std::string> bodyLines;
    int bodyTextLines;
    float fadeAlpha;
    bool transitioning;
};
