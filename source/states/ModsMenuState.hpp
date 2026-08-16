#pragma once
#include "../backend/MusicBeatState.hpp"
#include "../backend/ModHandler.hpp"
#include <vector>
#include <citro2d.h>

#include "../objects/Alphabet.hpp"


struct MenuButton {
    Frame frame = {};
    bool hasValue = false;
};

class ModsMenuState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    enum MenuState {
        STATE_IDLE,
        STATE_CONVERT_MENU,
        STATE_CONVERTING
    };
    MenuState currentState = STATE_IDLE;
    std::vector<ModMetadata>& getActiveList();

    int curSelected = 0;
    int subSelected = 0;
    std::vector<C2D_Image> icons;
    std::vector<C2D_SpriteSheet> iconSheets;
    
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    
    std::vector<C3D_Tex*> manualTexes;
    std::vector<Tex3DS_SubTexture*> manualSubtexes;

    MenuButton btnArrowUp, btnArrowDown, btnTop, btnPlay, btnOnOff, btnConvertAssets;
    C2D_Image imgMenuBG, imgMenuBGB;
    C2D_SpriteSheet sheetMenuBG = nullptr, sheetMenuBGB = nullptr;

    float gridOffset = 0;
    float lerpSelected = 0;
    float lerpSubSelected = 0.0f;
    u32 targetColor = C2D_Color32(20, 20, 25, 255);
    u32 currentColor = C2D_Color32(20, 20, 25, 255);
    float conversionProgress = 0;
    std::string conversionStatus = "";
    std::vector<std::string> conversionTargets;
    int currentTargetIdx = 0;

    // Incremental Audio Conversion
    FILE* convFIn = nullptr;
    FILE* convFOut = nullptr;
    void* convVfPtr = nullptr; // OggVorbis_File* (void* to avoid include here)
    uint32_t convSamplesProcessed = 0;
    uint32_t convTotalSamples = 0;
    int convChannels = 0;
    void* convStatePtr = nullptr; // AdpcmEncoder::State*
    bool isAudioPhase = false;
    
    void reloadIcons();
    void startConversion();
    void processConversion();
    float drawWrappedText(const std::string& text, float x, float y, float scale, float wrapWidth, u32 color);
};
