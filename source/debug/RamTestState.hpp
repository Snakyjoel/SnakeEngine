#pragma once
#include "../backend/MusicBeatState.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>

class RamTestState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    C2D_Font vcrFont    = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    C2D_SpriteSheet bgSheet       = nullptr;
    C2D_Image topBG;
    C2D_SpriteSheet bottomBGSheet = nullptr;
    C2D_Image bottomBG;

    struct MemBlock {
        void* ptr;
        size_t size;
        bool isLinear;
    };
    std::vector<MemBlock> allocatedBlocks;
    size_t totalAllocated = 0;

    void allocateRAM(size_t bytes);
    void freeAll();
};
