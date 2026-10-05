#include "RamTestState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../debug/DebugMenuState.hpp"
#include <malloc.h>
#include <string.h>

void RamTestState::init() {
    VCRFontFix();

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
}

void RamTestState::allocateRAM(size_t bytes) {
    void* ptr = malloc(bytes);
    bool isLinear = false;
    if (!ptr) {
        ptr = linearAlloc(bytes);
        isLinear = true;
    }
    memset(ptr, 0xAA, bytes);
    allocatedBlocks.push_back({ptr, bytes, isLinear});
    totalAllocated += bytes;
}

void RamTestState::freeAll() {
    for (size_t i = 0; i < allocatedBlocks.size(); i++) {
        if (allocatedBlocks[i].isLinear) {
            linearFree(allocatedBlocks[i].ptr);
        } else {
            free(allocatedBlocks[i].ptr);
        }
    }
    allocatedBlocks.clear();
    totalAllocated = 0;
}

void RamTestState::update(float dt) {
    u32 kDown = hidKeysDown();

    if (kDown & KEY_A) {
        allocateRAM(3 * 1024 * 1024);
    }
    if (kDown & KEY_X) {
        allocateRAM(5 * 1024 * 1024);
    }
    if (kDown & KEY_Y) {
        allocateRAM(10 * 1024 * 1024);
    }
    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        freeAll();
        switchState(new DebugMenuState());
    }
}

void RamTestState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    ClearTextBuf();

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(50, 50, 50, 255));

    if (bgSheet && topBG.tex) {
        drawCenteredBG(topBG, 400.0f, 240.0f, 0.1f);
    }

    extern u32 __ctru_heap_size;
    extern u32 __ctru_linear_heap_size;
    float heapTotal    = (float)__ctru_heap_size / (1024.0f * 1024.0f);
    float linearTotal  = (float)__ctru_linear_heap_size / (1024.0f * 1024.0f);
    struct mallinfo mi = mallinfo();
    float heapUsed     = (float)mi.uordblks / (1024.0f * 1024.0f);
    float linearFree   = (float)linearSpaceFree() / (1024.0f * 1024.0f);
    float linearUsed   = linearTotal - linearFree;
    float totalUsed    = heapUsed + linearUsed;
    float totalAvail   = heapTotal + linearTotal;

    AddTextCentered("RAM TEST", 200, 30, 0.7f, 1.5f, CWhite, 380.0f);

    char ramStr[128];
    sprintf(ramStr, "RAM: %.1f / %.1f MB", totalUsed, totalAvail);
    AddTextCentered(ramStr, 200, 80, 0.7f, 1.5f, CWhite, 380.0f);

    char detailStr[256];
    sprintf(detailStr, "STD: %.1f/%.1f | LIN: %.1f/%.1f", heapUsed, heapTotal, linearUsed, linearTotal);
    AddTextCentered(detailStr, 200, 120, 0.45f, 1.0f, CGray, 380.0f);

    char allocStr[128];
    sprintf(allocStr, "ALLOCATED: %.1f MB", (float)totalAllocated / (1024.0f * 1024.0f));
    AddTextCentered(allocStr, 200, 150, 0.5f, 1.0f, CWhite, 380.0f);

    AddTextCentered("A: +3 MB   X: +5 MB   Y: +10 MB", 200, 205, 0.45f, 1.0f, CWhite, 380.0f);
    AddText("B: Back", 200, 220, 0.4f, true, 1.0f, CGray, 380.0f);

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(40, 40, 40, 255));
    if (bottomBGSheet && bottomBG.tex) {
        drawCenteredBG(bottomBG, 320.0f, 240.0f, 0.1f);
    }
    C2D_Flush();
}

void RamTestState::exitState() {
    freeAll();

    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    if (vcrFontBuf) C2D_TextBufDelete(vcrFontBuf);
}
