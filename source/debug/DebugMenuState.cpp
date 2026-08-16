#include "../debug/DebugMenuState.hpp"
#include "TitleState.hpp"
#include "MusicPlayerState.hpp"
#include "SnakyPlayerState.hpp"
#include "ImageViewerState.hpp"
#include "EggRoomState.hpp"
#include "ResultState.hpp"
#include "../backend/AudioEngine.hpp"
#include <math.h>
#include <sys/stat.h>
#include "../objects/Alphabet.hpp"

static void drawBG(C2D_Image img, bool valid, float w, float h) {
    if (valid) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(100, 100, 100, 255), 1.0f);
        drawCenteredBG(img, w, h, 0.1f, &tint);
    }
}

void DebugMenuState::init() {
    VCRFontFix();

    // Load shared backgrounds
    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        bgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
        if (bgSheet) {
            topBG = C2D_SpriteSheetGetImage(bgSheet, 0);
            if (topBG.tex) C3D_TexSetFilter(topBG.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }
    std::string bgbPath = "romfs:/shared/images/menuBGB.t3x";
    if (Paths::fileExists(bgbPath)) {
        bottomBGSheet = C2D_SpriteSheetLoad(bgbPath.c_str());
        if (bottomBGSheet) {
            bottomBG = C2D_SpriteSheetGetImage(bottomBGSheet, 0);
            if (bottomBG.tex) C3D_TexSetFilter(bottomBG.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }

    // Load Alphabet sheet
    SpritesheetCache::get().load("shared/images/Alphabet");
    
    // Stop any playing music so audio player tests are clean
    MusicPlayer::stop();

    menuItems.clear();
    menuItems.push_back({"Music Player", "File Explorer & Music Player", 0});
    menuItems.push_back({"Snaky Player", "Test Snaky video playback", 2});
    menuItems.push_back({"Image Viewer", "View PNG and T3X images", 3});
    if (!ClientPrefs::eggInteractionOccurred) {
        menuItems.push_back({"Egg Room", "???", 4});
    }
    menuItems.push_back({"Result: PERFECT", "Test Result: PERFECT rank", 5});
    menuItems.push_back({"Result: EXCELLENT", "Test Result: EXCELLENT rank", 6});
    menuItems.push_back({"Result: GOOD", "Test Result: GOOD rank", 7});
    menuItems.push_back({"Result: GREAT", "Test Result: GREAT rank", 8});
    menuItems.push_back({"Result: LOSS", "Test Result: LOSS rank", 9});
}

void DebugMenuState::update(float dt) {
    gridOffset = fmodf(gridOffset + dt * 25.0f, 80.0f);
    lerpSelected += ((float)curSelected - lerpSelected) * dt * 10.0f;
    u32 kDown = hidKeysDown();

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        switchState(new TitleState());
        return;
    }
    
    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        curSelected--;
        if (curSelected < 0) curSelected = (int)menuItems.size() - 1;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected >= (int)menuItems.size()) curSelected = 0;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    
    if (kDown & (KEY_A | KEY_START)) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        switch (menuItems[curSelected].id) {
            case 0: switchState(new MusicPlayerState("sdmc:/")); break;
            case 2: switchState(new SnakyPlayerState("")); break;
            case 3: switchState(new ImageViewerState("")); break;
            case 4: switchState(new EggRoomState()); break;
            case 5: switchState(new ResultState(false, true, "Test Song", "Normal", 100, 100, 100, 0, 0, 0, 0, 1000000)); break;
            case 6: switchState(new ResultState(false, true, "Test Song", "Normal", 100, 80, 85, 10, 1, 0, 0, 900000)); break;
            case 7: switchState(new ResultState(false, true, "Test Song", "Normal", 100, 50, 50, 15, 0, 1, 0, 700000)); break;
            case 8: switchState(new ResultState(false, true, "Test Song", "Normal", 100, 80, 75, 10, 1, 0, 0, 850000)); break;
            case 9: switchState(new ResultState(false, true, "Test Song", "Normal", 100, 10, 10, 10, 5, 5, 20, 200000)); break;
        }
    }
}

void DebugMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    ClearTextBuf();

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(50, 50, 50, 255));
    drawBG(topBG, bgSheet != nullptr, 400.0f, 240.0f);

    Alphabet::draw("Debug Menu", 200.0f, 40.0f, 1.5f, 1.0f, true, C2D_Color32(255, 0, 0, 255));

    if (curSelected < (int)menuItems.size())
        AddTextCentered(menuItems[curSelected].desc.c_str(), 200, 140, 0.38f, 1.5f, CWhite, 370.0f);

    AddText("B: Back to Title", 200, 220, 0.30f, true, 1.5f, CGray, 380.0f);

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(40, 40, 40, 255));
    drawBG(bottomBG, bottomBGSheet != nullptr, 320.0f, 240.0f);

    for (int i = 0; i < (int)menuItems.size(); i++) {
        bool sel = (i == curSelected);
        float targetY = 110.0f + (i - lerpSelected) * 35.0f;
        float targetX = 160.0f;

        if (targetY < -30.0f || targetY > 270.0f) continue;

        std::string text = menuItems[i].name;
        float scale = 1.2f;
        u32 col = 0xFFFFFFFF;
        Alphabet::draw(text, targetX, targetY, scale, sel ? 1.0f : 0.5f, true, col);
    }
}

void DebugMenuState::exitState() {
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    C2D_TextBufDelete(vcrFontBuf);
}

void DebugMenuState::dumpFile(const std::string& path) {
    if (path.find("romfs:/") != 0) return; // Only dump romfs files
    
    // Create dumps directory
    mkdir("sdmc:/SnakeEngine/dumps", 0777);
    
    std::string outPath = "sdmc:/SnakeEngine/dumps/" + path.substr(7);
    
    // Create subdirectories if needed
    size_t pos = 24; // after sdmc:/SnakeEngine/dumps/
    while ((pos = outPath.find('/', pos)) != std::string::npos) {
        std::string dir = outPath.substr(0, pos);
        mkdir(dir.c_str(), 0777);
        pos++;
    }
    
    FILE* in = fopen(path.c_str(), "rb");
    if (!in) return;
    
    FILE* out = fopen(outPath.c_str(), "wb");
    if (!out) {
        fclose(in);
        return;
    }
    
    char buffer[8192];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, read, out);
    }
    
    fclose(in);
    fclose(out);
    
    // Play a success sound
    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
}
