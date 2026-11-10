#pragma once
#include "../backend/MusicBeatState.hpp"
#include <vector>
#include <string>

class CharacterSelectState : public MusicBeatState {
public:
    CharacterSelectState(bool playedVideo = false);
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;
private:
    bool playedVideo = false;
    int curSelected = 0;
    std::vector<std::string> characters = {"bf", "pico"};
};
