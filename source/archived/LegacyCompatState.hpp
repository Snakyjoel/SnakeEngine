#pragma once
#include "../backend/MusicBeatState.hpp"
#include <citro2d.h>
#include <string>

class LegacyCompatState : public MusicBeatState {
public:
    explicit LegacyCompatState(const std::string& modName = "");
    void init() override;
    void update(float dt) override {}
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    std::string _mod;

    C2D_TextBuf msgBuf    = nullptr;
    C2D_Text    txtStop;
    C2D_Text    txtModLine;
    C2D_Text    txtLine1;
    C2D_Text    txtLine2;
    C2D_Text    txtLine3;
    C2D_Text    txtLine4;
    C2D_Text    txtLine5;
    C2D_Text    txtLine6;
    C2D_Text    txtSig;
};
