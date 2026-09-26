#pragma once
#include "../backend/MusicBeatState.hpp"
#include <citro2d.h>
#include <3ds.h>
#include <cmath>
#include <string>
#include <vector>

enum class DialogState {
    NONE,
    SIMPLE_TEXT, 
    BACK_INTRO,  
    BACK_CHOICE,
    BACK_RESULT_YES
};

class EggRoomState : public MusicBeatState {
public:
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    static constexpr int MAP_W = 21;
    static constexpr int MAP_H = 21;
    static constexpr int SCREEN_W = 400;
    static constexpr int SCREEN_H = 240;
    static constexpr float MOVE_SPEED = 3.5f;
    static constexpr float ROT_SPEED = 2.2f;

    static constexpr u32 VOID_COLOR       = C2D_Color32(0, 0, 0, 255);
    static constexpr u32 PATH_COLOR_BASE  = C2D_Color32(0x5A, 0x24, 0x66, 255);
    static constexpr u32 PATH_COLOR_GRID  = C2D_Color32(0x8A, 0x3E, 0x9B, 255);
    static constexpr u32 PATH_COLOR_EDGE  = C2D_Color32(0x3A, 0x12, 0x44, 255);

    static constexpr char MAP[MAP_H][MAP_W + 1] = {
        "                     ",
        "        PPPPP        ",
        "       PPPPPPP       ",
        "      PPPPPPPPP      ",
        "     PPPPPPPPPPP     ",
        "    PPPPPPTPPPPPP    ",
        "     PPPPPPPPPPP     ",
        "      PPPPPPPPP      ",
        "       PPPPPPP       ",
        "        PPPPP        ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PPP         ",
        "         PXP         ",
        "                     ",
    };

    float playerX;
    float playerY;
    float playerAngle;

    C2D_SpriteSheet treeSheet;
    C2D_Image treeImg;

    C2D_SpriteSheet noteSheet;
    C2D_SpriteSheet uiSheet;
    C2D_Image noteImg;
    C2D_Image uiImg;

    bool musicStarted;

    DialogState currentDialogState;
    std::string fullTextToPrint;
    float textTimer;
    int charsPrinted;
    bool eggReceived;
    int choiceIndex;
    C2D_TextBuf dialogTextBuf;
    C2D_Text dialogTextObj;

    std::vector<std::string> templateDialogLines;
    size_t currentLineIndex;

    float skyScrollTimer;
    u8 ramGarbage[512];
    Tex3DS_SubTexture glitchSubs[96];

    float crashTimer;
    bool startCrashTimer;

    void renderSkybox();
    void renderFloor3D();
    void renderTree3D();
    char getMapTile(int x, int y) const;
};