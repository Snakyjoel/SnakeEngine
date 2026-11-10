#pragma once
#include "../backend/MusicBeatState.hpp"
#include <cstdint>

// PreloadCacheState: initial boot state responsible for hardware calibration
// verification and asset cache initialization before entering the title screen.
class PreloadCacheState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override {}

private:
    bool        hwCheckDone          = false;
    float       bootTimer            = 0.0f;
    bool        legacyCompatRequired = false;
    std::string cacheFlushTarget;
    std::string detectedModName;

    bool checkHwCalibration();
    void writeHwCalibration();
    void enterLegacyMode();
};
