#include "EggRoomState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/MusicBeatState.hpp"
#include "../states/MainMenuState.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

constexpr char EggRoomState::MAP[MAP_H][MAP_W + 1];

static std::string wrapText(const std::string& text, size_t maxChars = 24) {
    std::string result = "";
    std::string currentLine = "";
    std::string word = "";
    for (char c : text) {
        if (c == '\n') {
            result += currentLine + word + "\n";
            currentLine = "";
            word = "";
        } else if (c == ' ') {
            if (currentLine.length() + word.length() > maxChars) {
                result += currentLine + "\n";
                currentLine = word + " ";
            } else {
                currentLine += word + " ";
            }
            word = "";
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        if (currentLine.length() + word.length() > maxChars) {
            result += currentLine + "\n" + word;
        } else {
            result += currentLine + word;
        }
    } else {
        result += currentLine;
    }
    return result;
}

void EggRoomState::init() {
    MusicPlayer::stop();
    MusicPlayer::play("romfs:/preload/music/man.ogg", 0.7f);
    musicStarted = true;

    treeSheet = C2D_SpriteSheetLoad("romfs:/preload/images/tree.t3x");
    if (treeSheet) {
        treeImg = C2D_SpriteSheetGetImage(treeSheet, 0);
        if (treeImg.tex) {
            C3D_TexSetFilter(treeImg.tex, GPU_NEAREST, GPU_NEAREST);
        }
    }

    noteSheet = C2D_SpriteSheetLoad("romfs:/shared/images/NOTE_assets.t3x");
    if (noteSheet) {
        noteImg = C2D_SpriteSheetGetImage(noteSheet, 0);
    }

    uiSheet = C2D_SpriteSheetLoad("romfs:/preload/images/campaign_menu_UI_assets.t3x");
    if (uiSheet) {
        uiImg = C2D_SpriteSheetGetImage(uiSheet, 0);
    }

    playerX = 10.5f;
    playerY = 19.5f;
    playerAngle = -1.57079632679f; 

    dialogTextBuf = C2D_TextBufNew(512);
    currentDialogState = DialogState::NONE;
    eggReceived = false;
    charsPrinted = 0;
    textTimer = 0.0f;
    choiceIndex = 0;
    skyScrollTimer = 0.0f;
    crashTimer = 0.0f;
    startCrashTimer = false;

    u8* heapPtr = (u8*)malloc(512);
    if (heapPtr) {
        for (int i = 0; i < 512; i++) {
            ramGarbage[i] = heapPtr[i];
        }
        free(heapPtr);
    } else {
        for (int i = 0; i < 512; i++) {
            ramGarbage[i] = (u8)(i * 37);
        }
    }

    int horizonY = 130;
    for (int i = 0; i < 96; i++) {
        int idx = (i * 4) % 508;
        glitchSubs[i].width = (u16)(40 + (ramGarbage[idx + 2] % 140));
        glitchSubs[i].height = (u16)(10 + (ramGarbage[idx + 1] % 30));
        glitchSubs[i].left   = (float)(ramGarbage[idx] % 100) / 100.0f;
        glitchSubs[i].top    = (float)(ramGarbage[idx + 1] % 100) / 100.0f;
        glitchSubs[i].right  = glitchSubs[i].left + (float)(ramGarbage[idx + 2] % 50) / 100.0f;
        glitchSubs[i].bottom = glitchSubs[i].top + (float)(ramGarbage[idx + 3] % 50) / 100.0f;
    }

    templateDialogLines = {
        "* (Well, there's a man here.)",
        "* (He doesn't seem surprised to see you.)",
        "* (You aren't surprised to see him, either.)",
        "* (That surprises you.)",
        "* (You don't know who he is.)",
        "* (You know you've forgotten him.)",
        "* (You can't remember forgetting.)",
        "* (The man waits.)",
        "* (He has the patience of someone who has waited before.)",
        "* (And will again.)",
        "* (You search your memory.)",
        "* (The harder you search...)",
        "* (...the more familiar he becomes.)",
        "* (Not his face.)",
        "* (The feeling.)",
        "* (As though this meeting had already happened.)",
        "* (Or had been waiting to happen.)",
        "* (The man smiles.)",
        "* \"There you are.\"",
        "* (You don't know whether he was looking for you.)",
        "* (Or whether you were looking for him.)",
        "* \"It's been a while.\"",
        "* (It hasn't.)",
        "* (The man doesn't seem concerned.)",
        "* (He reaches into his coat.)",
        "* (He offers you something.)"
    };
}

char EggRoomState::getMapTile(int x, int y) const {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return ' ';
    return MAP[y][x];
}

void EggRoomState::update(float dt) {
    u32 kHeld = hidKeysHeld();
    u32 kDown = hidKeysDown();

    skyScrollTimer += dt * 4.0f;

    if (startCrashTimer) {
        crashTimer += dt;
        if (crashTimer >= 30.0f) {
            *((volatile u32*)nullptr) = 0;
        }
    }

    if (currentDialogState != DialogState::NONE) {
        textTimer += dt;
        if (textTimer >= 0.03f) {
            textTimer = 0.0f;
            if (charsPrinted < (int)fullTextToPrint.size()) {
                charsPrinted++;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.35f);
            }
        }

        bool textFinished = (charsPrinted >= (int)fullTextToPrint.size());
        
        if (currentDialogState == DialogState::BACK_CHOICE && textFinished) {
            if (kDown & (KEY_UP | KEY_DOWN)) {
                choiceIndex = (choiceIndex == 0) ? 1 : 0;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.6f);
            }
        }

        if (kDown & KEY_A) {
            if (textFinished) {
                switch (currentDialogState) {
                    case DialogState::SIMPLE_TEXT:
                        currentDialogState = DialogState::NONE;
                        break;
                    case DialogState::BACK_INTRO:
                        currentLineIndex++;
                        if (currentLineIndex < templateDialogLines.size()) {
                            fullTextToPrint = wrapText(templateDialogLines[currentLineIndex]);
                            charsPrinted = 0;
                        } else {
                            currentDialogState = DialogState::BACK_CHOICE;
                            fullTextToPrint = wrapText("* (Will you accept it?)");
                            charsPrinted = 0;
                            choiceIndex = 0;
                        }
                        break;
                    case DialogState::BACK_CHOICE:
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                        if (choiceIndex == 0) {
                            currentDialogState = DialogState::BACK_RESULT_YES;
                            fullTextToPrint = wrapText("* (You received an Egg.)");
                            charsPrinted = 0;
                            eggReceived = true;
                        } else {
                            currentDialogState = DialogState::NONE;
                            startCrashTimer = true;
                            crashTimer = 0.0f;
                            ClientPrefs::acceptedEgg = false;
                            ClientPrefs::eggInteractionOccurred = true;
                            ClientPrefs::saveSettings();
                        }
                        break;
                    case DialogState::BACK_RESULT_YES:
                        currentDialogState = DialogState::NONE;
                        startCrashTimer = true;
                        crashTimer = 0.0f;
                        ClientPrefs::acceptedEgg = true;
                        ClientPrefs::eggInteractionOccurred = true;
                        ClientPrefs::saveSettings();
                        break;
                    default:
                        currentDialogState = DialogState::NONE;
                        break;
                }
            } else {
                charsPrinted = (int)fullTextToPrint.size();
            }
        }
        return; 
    }
    
    if (kDown & KEY_A) {
        float treeX = 10.5f;
        float treeY = 5.5f;
        float dist = sqrtf((playerX - treeX)*(playerX - treeX) + (playerY - treeY)*(playerY - treeY));
        if (dist < 2.0f) {
            bool isBehind = (playerY < treeY);
            if (isBehind) {
                if (!eggReceived) {
                    currentDialogState = DialogState::BACK_INTRO;
                    currentLineIndex = 0;
                    fullTextToPrint = wrapText(templateDialogLines[0]);
                } else {
                    currentDialogState = DialogState::SIMPLE_TEXT;
                    fullTextToPrint = wrapText("* (Well, there is no man here.)");
                }
            } else {
                if (!eggReceived) {
                    currentDialogState = DialogState::SIMPLE_TEXT;
                    fullTextToPrint = wrapText("* (He is behind the tree)");
                } else {
                    currentDialogState = DialogState::SIMPLE_TEXT;
                    fullTextToPrint = wrapText("* (It's just a tree.)");
                }
            }
            charsPrinted = 0;
            textTimer = 0.0f;
            return;
        }
    }

    float moveSpeed = MOVE_SPEED * dt;
    float rotSpeed  = ROT_SPEED * dt;

    float dirX = cosf(playerAngle);
    float dirY = sinf(playerAngle);

    float nextX = playerX;
    float nextY = playerY;
    bool moved = false;

    if (kHeld & KEY_DUP) {
        nextX += dirX * moveSpeed;
        nextY += dirY * moveSpeed;
        moved = true;
    }
    if (kHeld & KEY_DDOWN) {
        nextX -= dirX * moveSpeed;
        nextY -= dirY * moveSpeed;
        moved = true;
    }

    if (kHeld & KEY_DLEFT) {
        playerAngle -= rotSpeed;
    }
    if (kHeld & KEY_DRIGHT) {
        playerAngle += rotSpeed;
    }

    if (kHeld & KEY_L) {
        float strafeX = sinf(playerAngle);
        float strafeY = -cosf(playerAngle);
        nextX += strafeX * moveSpeed;
        nextY += strafeY * moveSpeed;
        moved = true;
    }
    if (kHeld & KEY_R) {
        float strafeX = -sinf(playerAngle);
        float strafeY = cosf(playerAngle);
        nextX += strafeX * moveSpeed;
        nextY += strafeY * moveSpeed;
        moved = true;
    }

    if (moved) {
        if (getMapTile((int)nextX, (int)playerY) != 'T') {
            playerX = nextX;
        }
        if (getMapTile((int)playerX, (int)nextY) != 'T') {
            playerY = nextY;
        }
    }
}

void EggRoomState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_TargetClear(top, VOID_COLOR);
    C2D_SceneBegin(top);

    renderSkybox();
    renderFloor3D();
    renderTree3D();

    C2D_TargetClear(bottom, VOID_COLOR);
    C2D_SceneBegin(bottom);

    if (currentDialogState != DialogState::NONE && globalVCRFont != nullptr) {
        C2D_DrawRectSolid(10, 150, 0.0f, 300, 80, C2D_Color32(255, 255, 255, 255));
        C2D_DrawRectSolid(12, 152, 0.0f, 296, 76, VOID_COLOR);

        C2D_TextBufClear(dialogTextBuf);
        std::string currentStr = fullTextToPrint.substr(0, charsPrinted);
        C2D_TextFontParse(&dialogTextObj, globalVCRFont, dialogTextBuf, currentStr.c_str());
        C2D_TextOptimize(&dialogTextObj);
        C2D_DrawText(&dialogTextObj, C2D_WithColor, 20.0f, 160.0f, 0.5f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));

        if (currentDialogState == DialogState::BACK_CHOICE && charsPrinted >= (int)fullTextToPrint.size()) {
            C2D_Text yesTxt, noTxt;
            C2D_TextFontParse(&yesTxt, globalVCRFont, dialogTextBuf, "YES");
            C2D_TextFontParse(&noTxt, globalVCRFont, dialogTextBuf, "NO");
            C2D_TextOptimize(&yesTxt);
            C2D_TextOptimize(&noTxt);

            u32 colorYes = (choiceIndex == 0) ? C2D_Color32(255, 255, 0, 255) : C2D_Color32(150, 150, 150, 255);
            u32 colorNo  = (choiceIndex == 1) ? C2D_Color32(255, 255, 0, 255) : C2D_Color32(150, 150, 150, 255);

            C2D_DrawText(&yesTxt, C2D_WithColor, 250.0f, 170.0f, 0.5f, 0.45f, 0.45f, colorYes);
            C2D_DrawText(&noTxt, C2D_WithColor, 250.0f, 190.0f, 0.5f, 0.45f, 0.45f, colorNo);
        }
    }
}

void EggRoomState::renderSkybox() {
    int horizonY = 130;
    float scrollX = (playerAngle * 382.0f) + skyScrollTimer;

    int numStrips = 96;
    for (int i = 0; i < numStrips; i++) {
        int idx = (i * 4) % 508;
        
        float stripH = (float)glitchSubs[i].height;
        float stripW = (float)glitchSubs[i].width;

        float sx = 0.0f;
        float sy = 0.0f;
        int dirType = i % 4;
        if (dirType == 0) {
            sx = scrollX * 0.7f;
        } else if (dirType == 1) {
            sx = -scrollX * 0.7f;
        } else if (dirType == 2) {
            sy = skyScrollTimer * 4.0f;
            sx = playerAngle * 382.0f;
        } else {
            sy = -skyScrollTimer * 4.0f;
            sx = playerAngle * 382.0f;
        }

        float startX = fmodf((float)(ramGarbage[idx + 3] * 3) - sx, (float)(SCREEN_W + stripW)) - stripW;
        if (startX < -stripW) startX += (SCREEN_W + stripW);

        float startY = fmodf((float)(ramGarbage[idx] * 2) - sy, (float)(horizonY + stripH)) - stripH;
        if (startY < -stripH) startY += (horizonY + stripH);

        C2D_Image glitchImg;
        if ((i % 3 == 0) && treeImg.tex) {
            glitchImg.tex = treeImg.tex;
        } else if ((i % 3 == 1) && noteImg.tex) {
            glitchImg.tex = noteImg.tex;
        } else if (uiImg.tex) {
            glitchImg.tex = uiImg.tex;
        } else if (treeImg.tex) {
            glitchImg.tex = treeImg.tex;
        } else {
            continue;
        }
        
        glitchImg.subtex = &glitchSubs[i];

        u8 r = ramGarbage[idx];
        u8 g = ramGarbage[idx + 1];
        u8 b = ramGarbage[idx + 2];
        
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 25), 0.7f);

        C2D_DrawImageAt(glitchImg, startX, startY, 0.0f, &tint, 1.0f, 1.0f);

        if (startX + stripW > SCREEN_W) {
            C2D_DrawImageAt(glitchImg, startX - SCREEN_W, startY, 0.0f, &tint, 1.0f, 1.0f);
        }
    }
}

void EggRoomState::renderFloor3D() {
    float dirX = cosf(playerAngle);
    float dirY = sinf(playerAngle);

    float fovScale = 0.66f;
    float planeX = -sinf(playerAngle) * fovScale;
    float planeY =  cosf(playerAngle) * fovScale;

    int horizonY = 130; 
    const float camH = 150.0f; 

    const int Y_STEP = 1;
    const int COL_STEP = 2;

    for (int y = horizonY + 1; y < SCREEN_H; y += Y_STEP) {
        float p = (float)(y - horizonY);
        float posZ = camH / p;

        float rayDir0X = dirX - planeX;
        float rayDir0Y = dirY - planeY;
        float rayDir1X = dirX + planeX;
        float rayDir1Y = dirY + planeY;

        float floorStepX = posZ * (rayDir1X - rayDir0X) / (float)SCREEN_W;
        float floorStepY = posZ * (rayDir1Y - rayDir0Y) / (float)SCREEN_W;

        float floorX = playerX + posZ * rayDir0X;
        float floorY = playerY + posZ * rayDir0Y;

        int startX = -1;

        float fogFactor = 1.0f - (posZ / 15.0f);
        if (fogFactor < 0.0f) fogFactor = 0.0f;

        u8 r = (u8)((float)0x5A * fogFactor);
        u8 g = (u8)((float)0x24 * fogFactor);
        u8 b = (u8)((float)0x66 * fogFactor);
        u32 pathColor = C2D_Color32(r, g, b, 255);

        for (int x = 0; x < SCREEN_W; x += COL_STEP) {
            int mapCellX = (int)floorf(floorX);
            int mapCellY = (int)floorf(floorY);

            bool isPathTile = false;
            if (mapCellX >= 0 && mapCellX < MAP_W && mapCellY >= 0 && mapCellY < MAP_H) {
                char tile = getMapTile(mapCellX, mapCellY);
                if (tile == 'P' || tile == 'T' || tile == 'X') {
                    isPathTile = true;
                }
            }

            if (isPathTile) {
                if (startX == -1) startX = x;
            } else {
                if (startX != -1) {
                    C2D_DrawRectSolid((float)startX, (float)y, 0.0f, (float)(x - startX), (float)Y_STEP, pathColor);
                    startX = -1;
                }
            }

            floorX += floorStepX * COL_STEP;
            floorY += floorStepY * COL_STEP;
        }

        if (startX != -1) {
            C2D_DrawRectSolid((float)startX, (float)y, 0.0f, (float)(SCREEN_W - startX), (float)Y_STEP, pathColor);
        }
    }
}

void EggRoomState::renderTree3D() {
    if (!treeImg.tex) return;

    float treeX = 10.5f;
    float treeY = 5.5f;

    float dx = treeX - playerX;
    float dy = treeY - playerY;

    float dirX = cosf(playerAngle);
    float dirY = sinf(playerAngle);

    float fovScale = 0.66f;
    float planeX = -sinf(playerAngle) * fovScale;
    float planeY =  cosf(playerAngle) * fovScale;

    float invDet = 1.0f / (planeX * dirY - dirX * planeY);
    float transformX = invDet * (dirY * dx - dirX * dy);
    float transformY = invDet * (-planeY * dx + planeX * dy);

    if (transformY <= 0.05f) return;

    int spriteScreenX = (int)((float)(SCREEN_W / 2) * (1.0f + transformX / transformY));

    int horizonY = 130;
    const float camH = 150.0f;

    float worldTreeHeight = 10.0f; 

    float dist = sqrtf(dx*dx + dy*dy);
    int spriteHeight = (int)((camH / dist) * worldTreeHeight);
    
    float playerRelAngle = atan2f(dy, dx);
    float treeNormalAngle = -1.57079632679f; 
    float diffAngle = playerRelAngle - treeNormalAngle;
    float apparentWidthScale = fabs(cosf(diffAngle));

    int spriteWidth  = (int)(spriteHeight * apparentWidthScale);

    if (spriteWidth <= 0 || spriteHeight <= 0) return;

    int floorBaseY = horizonY + (int)(camH / transformY);
    int spriteScreenY = floorBaseY - spriteHeight;

    if (spriteScreenX + spriteWidth < -300 || spriteScreenX - spriteWidth > SCREEN_W + 300) return;

    float origW = 64.0f;
    float origH = 64.0f;
    if (treeImg.subtex && treeImg.subtex->width > 0) {
        origW = (float)treeImg.subtex->width;
        origH = (float)treeImg.subtex->height;
    }

    float scaleX = (float)spriteWidth / origW;
    float scaleY = (float)spriteHeight / origH;

    float drawX = (float)(spriteScreenX - spriteWidth / 2);
    float drawY = (float)spriteScreenY;

    float shadowScaleY = scaleY * 0.22f;
    float shadowHeight = spriteHeight * 0.22f;
    float shadowDrawY = (float)floorBaseY - shadowHeight;

    C2D_ImageTint shadowTint;
    float shadowFog = 1.0f - (dist / 15.0f);
    if (shadowFog < 0.0f) shadowFog = 0.0f;
    u8 shadowAlpha = (u8)(120 * shadowFog);

    C2D_PlainImageTint(&shadowTint, C2D_Color32(0, 0, 0, shadowAlpha), 1.0f);

    C2D_DrawImageAt(treeImg, drawX, shadowDrawY, 0.0f, &shadowTint, scaleX, shadowScaleY);

    float fogFactor = 1.0f - (dist / 15.0f);
    if (fogFactor < 0.0f) fogFactor = 0.0f;
    
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(0, 0, 0, 255), 1.0f - fogFactor);

    C2D_DrawImageAt(treeImg, drawX, drawY, 0.0f, &tint, scaleX, scaleY);
}

void EggRoomState::exitState() {
    if (musicStarted) {
        MusicPlayer::stop();
        musicStarted = false;
    }
    if (treeSheet) {
        C2D_SpriteSheetFree(treeSheet);
        treeSheet = nullptr;
        treeImg.tex = nullptr;
        treeImg.subtex = nullptr;
    }
    if (noteSheet) {
        C2D_SpriteSheetFree(noteSheet);
        noteSheet = nullptr;
        noteImg.tex = nullptr;
        noteImg.subtex = nullptr;
    }
    if (uiSheet) {
        C2D_SpriteSheetFree(uiSheet);
        uiSheet = nullptr;
        uiImg.tex = nullptr;
        uiImg.subtex = nullptr;
    }
    if (dialogTextBuf) {
        C2D_TextBufDelete(dialogTextBuf);
        dialogTextBuf = nullptr;
    }
}