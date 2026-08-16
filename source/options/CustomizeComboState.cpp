#include "CustomizeComboState.hpp"
#include "OptionsMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/Alphabet.hpp"
#include <fstream>
#include <algorithm>

static void parseNoteXml(const std::string& xmlPath, C3D_Tex* tex, C2D_Image baseImg, std::vector<NoteSprite>& subs) {
    subs.clear();
    subs.resize(24);
    for (auto& s : subs) {
        s.tex = tex;
    }

    std::ifstream f(xmlPath);
    if (!f.is_open()) return;
    float nw = baseImg.subtex->right  - baseImg.subtex->left;
    float nh = baseImg.subtex->bottom - baseImg.subtex->top;
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("<SubTexture") == std::string::npos) continue;
        auto getValString = [&](const std::string& key) {
            size_t p = line.find(" " + key);
            if (p == std::string::npos) return std::string("");
            p = line.find("=", p + key.size() + 1);
            if (p == std::string::npos) return std::string("");
            p = line.find("\"", p + 1);
            if (p == std::string::npos) return std::string("");
            size_t e = line.find("\"", p + 1);
            if (e == std::string::npos) return std::string("");
            return line.substr(p + 1, e - p - 1);
        };
        auto getValFloat = [&](const std::string& key) {
            std::string val = getValString(key);
            return val.empty() ? 0.0f : (float)atof(val.c_str());
        };

        std::string name = getValString("name");
        float x = getValFloat("x");
        float y = getValFloat("y");
        float w = getValFloat("width");
        float h = getValFloat("height");
        bool rotated = getValString("rotated") == "true";
        float frameX = getValFloat("frameX");
        float frameY = getValFloat("frameY");
        float frameWidth = getValFloat("frameWidth");
        float frameHeight = getValFloat("frameHeight");

        int lane = -1;
        int slot = -1;

        if (name.find("arrowLEFT") != std::string::npos) { lane = 0; slot = 1; }
        else if (name.find("arrowDOWN") != std::string::npos) { lane = 1; slot = 1; }
        else if (name.find("arrowUP") != std::string::npos) { lane = 2; slot = 1; }
        else if (name.find("arrowRIGHT") != std::string::npos) { lane = 3; slot = 1; }
        
        else if (name.find("purple") != std::string::npos && name.find("hold") == std::string::npos && name.find("press") == std::string::npos && name.find("confirm") == std::string::npos) { lane = 0; slot = 2; }
        else if (name.find("blue") != std::string::npos && name.find("hold") == std::string::npos && name.find("press") == std::string::npos && name.find("confirm") == std::string::npos) { lane = 1; slot = 2; }
        else if (name.find("green") != std::string::npos && name.find("hold") == std::string::npos && name.find("press") == std::string::npos && name.find("confirm") == std::string::npos) { lane = 2; slot = 2; }
        else if (name.find("red") != std::string::npos && name.find("hold") == std::string::npos && name.find("press") == std::string::npos && name.find("confirm") == std::string::npos) { lane = 3; slot = 2; }

        else if (name.find("left press") != std::string::npos) { lane = 0; slot = 3; }
        else if (name.find("down press") != std::string::npos) { lane = 1; slot = 3; }
        else if (name.find("green press") != std::string::npos || name.find("up press") != std::string::npos) { lane = 2; slot = 3; }
        else if (name.find("right press") != std::string::npos) { lane = 3; slot = 3; }

        else if (name.find("left confirm") != std::string::npos) { lane = 0; slot = 0; }
        else if (name.find("down confirm") != std::string::npos) { lane = 1; slot = 0; }
        else if (name.find("up confirm") != std::string::npos) { lane = 2; slot = 0; }
        else if (name.find("right confirm") != std::string::npos) { lane = 3; slot = 0; }

        else if (name.find("purple hold piece") != std::string::npos) { lane = 0; slot = 4; }
        else if (name.find("blue hold piece") != std::string::npos) { lane = 1; slot = 4; }
        else if (name.find("green hold piece") != std::string::npos) { lane = 2; slot = 4; }
        else if (name.find("red hold piece") != std::string::npos) { lane = 3; slot = 4; }

        else if (name.find("purple end hold") != std::string::npos || name.find("pruple hold end") != std::string::npos) { lane = 0; slot = 5; }
        else if (name.find("blue hold end") != std::string::npos) { lane = 1; slot = 5; }
        else if (name.find("green hold end") != std::string::npos) { lane = 2; slot = 5; }
        else if (name.find("red hold end") != std::string::npos) { lane = 3; slot = 5; }

        if (lane != -1 && slot != -1) {
            int destIdx = lane * 6 + slot;
            NoteSprite& ns = subs[destIdx];
            ns.tex = tex;
            ns.rotated = rotated;
            ns.w = w;
            ns.h = h;
            ns.frameX = frameX;
            ns.frameY = frameY;
            ns.frameWidth = frameWidth ? frameWidth : w;
            ns.frameHeight = frameHeight ? frameHeight : h;

            float pw = rotated ? h : w;
            float ph = rotated ? w : h;
            ns.sub.width = (u16)pw;
            ns.sub.height = (u16)ph;
            ns.sub.left = baseImg.subtex->left + (x * nw / baseImg.subtex->width);
            ns.sub.top = baseImg.subtex->top + (y * nh / baseImg.subtex->height);
            ns.sub.right = baseImg.subtex->left + ((x + pw) * nw / baseImg.subtex->width);
            ns.sub.bottom = baseImg.subtex->top + ((y + ph) * nh / baseImg.subtex->height);
        }
    }
}

CustomizeComboState::CustomizeComboState() {}

CustomizeComboState::~CustomizeComboState() {
    if (vcrFontBuf) C2D_TextBufDelete(vcrFontBuf);
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (ratingSheet) C2D_SpriteSheetFree(ratingSheet);
    if (noteSheet) C2D_SpriteSheetFree(noteSheet);
}

void CustomizeComboState::init() {
    VCRFontFix();
    
    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        bgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
        if (bgSheet) {
            topBG = C2D_SpriteSheetGetImage(bgSheet, 0);
            if (topBG.tex) C3D_TexSetFilter(topBG.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
            bottomBG = topBG; // Use same for bottom
        }
    }

    ratingSheet = C2D_SpriteSheetLoad(Paths::image("ratingSkins/rating", "shared").c_str());
    if (ratingSheet) {
        ratingBaseImage = C2D_SpriteSheetGetImage(ratingSheet, 0);
        if (ratingBaseImage.tex) C3D_TexSetFilter(ratingBaseImage.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        if (ratingBaseImage.subtex != nullptr) {
            std::ifstream file(Paths::xml("ratingSkins/rating", "shared").c_str());
            std::string line;
            float rw = ratingBaseImage.subtex->right - ratingBaseImage.subtex->left;
            float rh = ratingBaseImage.subtex->bottom - ratingBaseImage.subtex->top;
        
            while (std::getline(file, line)) {
                if (line.find("<SubTexture") != std::string::npos) {
                    auto getVal = [&](const std::string& key) {
                        size_t pos = line.find(key + "=\"");
                        if (pos == std::string::npos) return std::string("0");
                        pos += key.length() + 2;
                        size_t end = line.find("\"", pos);
                        return line.substr(pos, end - pos);
                    };
                    
                    std::string nameInfo = getVal("name");
                    std::string id = "shit";
                    if (nameInfo.find("sick") != std::string::npos) id = "sick";
                    else if (nameInfo.find("good") != std::string::npos) id = "good";
                    else if (nameInfo.find("bad") != std::string::npos) id = "bad";
                    
                    Tex3DS_SubTexture sub = {};
                    float x = atof(getVal("x").c_str());
                    float y = atof(getVal("y").c_str());
                    float w = atof(getVal("width").c_str());
                    float h = atof(getVal("height").c_str());
                    bool rotated = (getVal("rotated") == "true");
                    float frameWidth = atof(getVal("frameWidth").c_str());
                    float frameHeight = atof(getVal("frameHeight").c_str());
                    
                    float pw = w, ph = h;
                    if (rotated) {
                        bool looksLikeOriginal = (fabsf(w - frameHeight) < 1.0f && fabsf(h - frameWidth) < 1.0f);
                        if (looksLikeOriginal) {
                            pw = h;
                            ph = w;
                        }
                    }
                    sub.width = (u16)pw;
                    sub.height = (u16)ph;
                    if (ratingBaseImage.subtex) {
                        sub.left = ratingBaseImage.subtex->left + (x * rw / (float)ratingBaseImage.subtex->width);
                        sub.top = ratingBaseImage.subtex->top + (y * rh / (float)ratingBaseImage.subtex->height);
                        sub.right = ratingBaseImage.subtex->left + ((x + pw) * rw / (float)ratingBaseImage.subtex->width);
                        sub.bottom = ratingBaseImage.subtex->top + ((y + ph) * rh / (float)ratingBaseImage.subtex->height);
                    }
                    
                    RatingInfo info;
                    info.sub = sub;
                    info.rotated = rotated;
                    info.frameWidth = frameWidth;
                    info.frameHeight = frameHeight;
                    ratingSubtexs[id] = info;
                }
            }
            
            constexpr float REFERENCE_SICK_WIDTH = 160.0f;
            if (ratingSubtexs.count("sick")) {
                float skinW = ratingSubtexs["sick"].frameWidth;
                float autoScale = skinW < REFERENCE_SICK_WIDTH ? REFERENCE_SICK_WIDTH / skinW : 1.0f;
                for (auto& kv : ratingSubtexs) {
                    kv.second.autoScale = autoScale;
                }
            }
        }
    }
    
    std::string notePath = Paths::image("NOTE_assets", "shared");
    noteSheet = C2D_SpriteSheetLoad(notePath.c_str());
    if (noteSheet) {
        noteBaseImage = C2D_SpriteSheetGetImage(noteSheet, 0);
        if (noteBaseImage.tex) C3D_TexSetFilter(noteBaseImage.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        if (noteBaseImage.subtex != nullptr) {
            parseNoteXml("romfs:/shared/images/NOTE_assets.xml", noteBaseImage.tex, noteBaseImage, noteSubtexs);
        }
    }

    currentScale = ClientPrefs::comboScale;
    currentAlpha = ClientPrefs::comboAlpha;
    currentX = (400.0f - 50.0f) + ClientPrefs::comboOffsetX;
    currentY = 35.0f + ClientPrefs::comboOffsetY;
}

void CustomizeComboState::update(float dt) {
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    
    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        ClientPrefs::comboOffsetX = currentX - (400.0f - 50.0f);
        ClientPrefs::comboOffsetY = currentY - 35.0f;
        ClientPrefs::comboScale = currentScale;
        ClientPrefs::comboAlpha = currentAlpha;
        ClientPrefs::saveSettings();
        switchState(new OptionsMenuState());
        return;
    }

    // Touch logic for dragging in the bottom screen representation
    touchPosition touch;
    hidTouchRead(&touch);
    
    // Bottom screen preview rect: 300x180 centered
    float previewW = 300.0f;
    float previewH = 180.0f;
    float previewX = (320.0f - previewW) / 2.0f;
    float previewY = (240.0f - previewH) / 2.0f;
    float mapScale = previewW / 400.0f; // 300/400 = 0.75
    
    if (kDown & KEY_TOUCH) {
        float mappedTouchX = (touch.px - previewX) / mapScale;
        float mappedTouchY = (touch.py - previewY) / mapScale;
        
        float sw = ratingSubtexs["sick"].sub.width * 0.4f * currentScale * ratingSubtexs["sick"].autoScale;
        float sh = ratingSubtexs["sick"].sub.height * 0.4f * currentScale * ratingSubtexs["sick"].autoScale;
        
        float hitX = currentX - sw / 2.0f;
        float hitY = currentY - sh / 2.0f;
        
        if (mappedTouchX >= hitX && mappedTouchX <= hitX + sw &&
            mappedTouchY >= hitY && mappedTouchY <= hitY + sh) {
            dragging = true;
            dragOffsetX = currentX - mappedTouchX;
            dragOffsetY = currentY - mappedTouchY;
        } else {
            // Click outside to immediately move
            dragging = true;
            dragOffsetX = 0;
            dragOffsetY = 0;
            currentX = mappedTouchX;
            currentY = mappedTouchY;
        }
    }
    
    if (kHeld & KEY_TOUCH) {
        if (dragging) {
            float mappedTouchX = (touch.px - previewX) / mapScale;
            float mappedTouchY = (touch.py - previewY) / mapScale;
            currentX = mappedTouchX + dragOffsetX;
            currentY = mappedTouchY + dragOffsetY;
        }
    } else {
        dragging = false;
    }

    // D-Pad for precise movement
    if (kHeld & (KEY_DLEFT | KEY_CPAD_LEFT))  currentX -= 150.0f * dt;
    if (kHeld & (KEY_DRIGHT | KEY_CPAD_RIGHT)) currentX += 150.0f * dt;
    if (kHeld & (KEY_DUP | KEY_CPAD_UP))    currentY -= 150.0f * dt;
    if (kHeld & (KEY_DDOWN | KEY_CPAD_DOWN))  currentY += 150.0f * dt;

    // L/R for scale
    if (kHeld & KEY_L) currentScale = std::max(0.1f, currentScale - 1.0f * dt);
    if (kHeld & KEY_R) currentScale = std::min(5.0f, currentScale + 1.0f * dt);
    
    // X/Y for alpha
    if (kHeld & KEY_Y) currentAlpha = std::max(0.1f, currentAlpha - 1.0f * dt);
    if (kHeld & KEY_X) currentAlpha = std::min(1.0f, currentAlpha + 1.0f * dt);

    // Reset with SELECT
    if (kDown & KEY_SELECT) {
        currentX = (400.0f - 50.0f);
        currentY = 35.0f;
        currentScale = 1.0f;
        currentAlpha = 1.0f;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    
    // Bounds checking
    float comboW = 0.0f;
    float comboH = 0.0f;
    if (ratingSubtexs.count("sick")) {
        comboW = ratingSubtexs["sick"].sub.width * 0.4f * currentScale * ratingSubtexs["sick"].autoScale;
        comboH = ratingSubtexs["sick"].sub.height * 0.4f * currentScale * ratingSubtexs["sick"].autoScale;
    }
    
    float comboLeft = currentX - comboW / 2.0f;
    float comboRight = currentX + comboW / 2.0f;
    float comboTop = currentY - comboH / 2.0f;
    float comboBottom = currentY + comboH / 2.0f;
    
    isOutOfBounds = (comboLeft < 0 || comboRight > 400 || comboTop < 0 || comboBottom > 240);
    
    blinkTimer += dt;
    if (blinkTimer > 1.0f) blinkTimer -= 1.0f;
}

void CustomizeComboState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    ClearTextBuf();

    // -- TOP SCREEN --
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(146, 113, 253, 255));
    if (bgSheet) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(39, 71, 220, 255), 1.0f);
        drawCenteredBG(topBG, 400.0f, 240.0f, 0.1f, &tint);
    }

    // HUD Preview
    float receptorY = ClientPrefs::downscroll ? 240.0f - 40.0f - 20.0f : 20.0f;
    float spacing = 38.0f;
    float playerX = 400.0f / 2.0f + 25.0f;
    float noteScale = 0.6f;

    C2D_ImageTint noteTint;
    C2D_AlphaImageTint(&noteTint, 0.7f);

    // Opponent Strums
    if (ClientPrefs::opponentStrums) {
        for (int i = 0; i < 4; i++) {
            float lx = 16.0f + i * spacing;
            if (ClientPrefs::middleScroll) {
                if (i == 0) lx = 16.0f;
                else if (i == 1) lx = 16.0f + spacing;
                else if (i == 2) lx = 400.0f - 16.0f - 2 * spacing;
                else lx = 400.0f - 16.0f - spacing;
            }
            if (noteSheet && noteSubtexs.size() > 0) {
                int spriteIdx = i * 6 + 1;
                if (spriteIdx < (int)noteSubtexs.size()) {
                    NoteSprite ns = noteSubtexs[spriteIdx];
                    float laneCenterX = lx + (spacing / 2.0f);
                    float laneCenterY = receptorY + (spacing / 2.0f);
                    float origW = ns.frameWidth ? ns.frameWidth : ns.w;
                    float origH = ns.frameHeight ? ns.frameHeight : ns.h;
                    float expectedCenterX = laneCenterX + (ns.w / 2.0f - origW / 2.0f - ns.frameX) * noteScale;
                    float expectedCenterY = laneCenterY + (ns.h / 2.0f - origH / 2.0f - ns.frameY) * noteScale;
                    float drawX, drawY;
                    if (ns.rotated) {
                        drawX = expectedCenterX - ns.h * noteScale * 0.5f;
                        drawY = expectedCenterY - ns.w * noteScale * 0.5f;
                    } else {
                        drawX = expectedCenterX - ns.w * noteScale * 0.5f;
                        drawY = expectedCenterY - ns.h * noteScale * 0.5f;
                    }
                    renderNoteSprite(ns, drawX, drawY, 0.15f, &noteTint, noteScale, noteScale);
                }
            } else {
                C2D_DrawRectSolid(lx, receptorY, 0.15f, spacing-4, spacing-4, C2D_Color32(100,100,100,100)); 
            }
        }
    }
    // Player Strums
    for (int i = 0; i < 4; i++) {
        float lx = playerX + i * spacing;
        if (ClientPrefs::middleScroll) {
            lx = (400.0f / 2.0f - (2 * spacing)) + i * spacing;
        }
        if (noteSheet && noteSubtexs.size() > 0) {
            int spriteIdx = i * 6 + 1;
            if (spriteIdx < (int)noteSubtexs.size()) {
                NoteSprite ns = noteSubtexs[spriteIdx];
                float laneCenterX = lx + (spacing / 2.0f);
                float laneCenterY = receptorY + (spacing / 2.0f);
                float origW = ns.frameWidth ? ns.frameWidth : ns.w;
                float origH = ns.frameHeight ? ns.frameHeight : ns.h;
                float expectedCenterX = laneCenterX + (ns.w / 2.0f - origW / 2.0f - ns.frameX) * noteScale;
                float expectedCenterY = laneCenterY + (ns.h / 2.0f - origH / 2.0f - ns.frameY) * noteScale;
                float drawX, drawY;
                if (ns.rotated) {
                    drawX = expectedCenterX - ns.h * noteScale * 0.5f;
                    drawY = expectedCenterY - ns.w * noteScale * 0.5f;
                } else {
                    drawX = expectedCenterX - ns.w * noteScale * 0.5f;
                    drawY = expectedCenterY - ns.h * noteScale * 0.5f;
                }
                renderNoteSprite(ns, drawX, drawY, 0.15f, &noteTint, noteScale, noteScale);
            }
        } else {
            C2D_DrawRectSolid(lx, receptorY, 0.15f, spacing-4, spacing-4, C2D_Color32(100,100,100,150)); 
        }
    }
    // Time Bar
    if (ClientPrefs::timeBarType != 3) {
        float timeBarW = 150.0f;
        float timeBarH = 5.0f;
        float timeBarX = (400.0f - timeBarW) / 2.0f;
        float timeBarY = ClientPrefs::downscroll ? 240.0f - 15.0f : 10.0f;
        C2D_DrawRectSolid(timeBarX, timeBarY, 0.15f, timeBarW, timeBarH, C2D_Color32(0,0,0,150));
    }
    // Health Bar
    if (ClientPrefs::healthBar) {
        float healthBarW = 200.0f;
        float healthBarH = 5.0f;
        float healthBarX = (400.0f - healthBarW) / 2.0f;
        float healthBarY = ClientPrefs::downscroll ? 20.0f : 240.0f - 20.0f;
        C2D_DrawRectSolid(healthBarX, healthBarY, 0.15f, healthBarW, healthBarH, C2D_Color32(255,0,0,150));
        C2D_DrawRectSolid(healthBarX + healthBarW/2.0f, healthBarY, 0.16f, healthBarW/2.0f, healthBarH, C2D_Color32(0,255,0,150));
    }
    // ------------------------

    // Draw the combo on top screen
    if (ratingSheet && ratingSubtexs.count("sick")) {
        RatingInfo& ri = ratingSubtexs["sick"];

        C2D_ImageTint tint;
        float displayAlpha = currentAlpha;
        if (isOutOfBounds) {
            displayAlpha = currentAlpha * (0.3f + 0.7f * (0.5f + 0.5f * sinf(blinkTimer * 2 * M_PI)));
        }
        C2D_AlphaImageTint(&tint, displayAlpha);
        
        float scale = 0.4f * currentScale;
        float baseW = ri.sub.width;
        float baseH = ri.sub.height;
        float drawX, drawY;
        if (ri.rotated) {
            drawX = currentX - (baseH * scale / 2.0f);
            drawY = currentY - (baseW * scale / 2.0f);
        } else {
            drawX = currentX - (baseW * scale / 2.0f);
            drawY = currentY - (baseH * scale / 2.0f);
        }
        
        renderRatingSprite(ratingBaseImage.tex, &ri.sub, ri.rotated, ri.frameWidth, ri.frameHeight, drawX, drawY, 0.95f, &tint, scale * ri.autoScale);
    }
    
    float comboW = ratingSubtexs.count("sick") ? ratingSubtexs["sick"].frameWidth * 0.4f * currentScale : 150.0f;
    float comboH = ratingSubtexs.count("sick") ? ratingSubtexs["sick"].frameHeight * 0.4f * currentScale : 150.0f;
    float comboLeft = currentX - comboW / 2.0f;
    float comboRight = currentX + comboW / 2.0f;
    float comboTop = currentY - comboH / 2.0f;
    float comboBottom = currentY + comboH / 2.0f;
    
    float tw = Alphabet::getTextWidth("PREVIEW", 0.7f);
    float pX = 400.0f - tw - 10.0f;
    float defaultPy = ClientPrefs::downscroll ? 10.0f : 240.0f - 35.0f;
    float altPy = ClientPrefs::downscroll ? 240.0f - 35.0f : 10.0f;
    bool overlapPreview = (comboRight > pX && comboLeft < pX + tw && 
                           comboBottom > defaultPy && comboTop < defaultPy + 25.0f);
    float actualPy = overlapPreview ? altPy : defaultPy;

    float cX = 10.0f;
    float defaultCy = ClientPrefs::downscroll ? 240.0f - 35.0f : 10.0f;
    float altCy = ClientPrefs::downscroll ? 10.0f : 240.0f - 35.0f;
    bool overlapCoords = (comboRight > cX && comboLeft < cX + 120.0f && 
                          comboBottom > defaultCy && comboTop < defaultCy + 40.0f);
    float actualCy = overlapCoords ? altCy : defaultCy;

    float previewPulse = 0.5f + 0.5f * sinf(blinkTimer * M_PI);
    float previewAlpha = 0.4f + 0.6f * previewPulse; // pulse between 0.4 and 1.0
    Alphabet::draw("PREVIEW", pX, actualPy, 0.7f, previewAlpha, false);
    
    char coordStr[64];
    sprintf(coordStr, "Combo XY: [%d, %d]", (int)currentX, (int)currentY);
            
    C2D_Text coordObj;
    C2D_TextFontParse(&coordObj, vcrFont, vcrFontBuf, coordStr);
    C2D_TextOptimize(&coordObj);
    
    DrawTextBorderCardinal(&coordObj, cX, actualCy, 0.96f, 0.4f, 0.4f, 1.5f, C2D_Color32(0,0,0,255));
    C2D_DrawText(&coordObj, C2D_WithColor, cX, actualCy, 0.97f, 0.4f, 0.4f, CWhite);

    if (isOutOfBounds) {
        AddTextCentered("Combo is out of screen bounds", 200, 220, 0.4f, 1.0f, CRed, 400.0f);
    }


    // -- BOTTOM SCREEN --
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(146, 113, 253, 255));
    if (bgSheet) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(39, 71, 220, 255), 1.0f);
        drawCenteredBG(bottomBG, 320.0f, 240.0f, 0.1f, &tint);
    }

    float previewW = 300.0f;
    float previewH = 180.0f;
    float previewX = (320.0f - previewW) / 2.0f;
    float previewY = (240.0f - previewH) / 2.0f;
    float mapScale = previewW / 400.0f; 

    // Draw preview box
    C2D_DrawRectSolid(previewX, previewY, 0.2f, previewW, previewH, C2D_Color32(0, 0, 0, 150));
    
    // Draw grid lines inside preview box
    for (float x = previewX; x < previewX + previewW; x += 15.0f * mapScale) {
        C2D_DrawRectSolid(x, previewY, 0.21f, 1.0f, previewH, C2D_Color32(255, 255, 255, 30));
    }
    for (float y = previewY; y < previewY + previewH; y += 15.0f * mapScale) {
        C2D_DrawRectSolid(previewX, y, 0.21f, previewW, 1.0f, C2D_Color32(255, 255, 255, 30));
    }

    // Draw box border
    C2D_DrawRectSolid(previewX - 2, previewY - 2, 0.22f, previewW + 4, 2, CWhite);
    C2D_DrawRectSolid(previewX - 2, previewY + previewH, 0.22f, previewW + 4, 2, CWhite);
    C2D_DrawRectSolid(previewX - 2, previewY, 0.22f, 2, previewH, CWhite);
    C2D_DrawRectSolid(previewX + previewW, previewY, 0.22f, 2, previewH, CWhite);

    // Draw combo inside preview box
    if (ratingSheet && ratingSubtexs.count("sick")) {
        RatingInfo& ri = ratingSubtexs["sick"];

        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, currentAlpha);
        
        float scale = 0.4f * currentScale * mapScale;
        float baseW = ri.sub.width;
        float baseH = ri.sub.height;
        float drawX, drawY;
        if (ri.rotated) {
            drawX = previewX + (currentX - (baseH * 0.4f * currentScale / 2.0f)) * mapScale;
            drawY = previewY + (currentY - (baseW * 0.4f * currentScale / 2.0f)) * mapScale;
        } else {
            drawX = previewX + (currentX - (baseW * 0.4f * currentScale / 2.0f)) * mapScale;
            drawY = previewY + (currentY - (baseH * 0.4f * currentScale / 2.0f)) * mapScale;
        }
        
        renderRatingSprite(ratingBaseImage.tex, &ri.sub, ri.rotated, ri.frameWidth, ri.frameHeight, drawX, drawY, 0.95f, &tint, scale * ri.autoScale);
    }

    // UI text
    AddTextCentered("Touch/D-Pad: Move | L/R: Scale", 160, 15, 0.4f, 1.0f, CWhite, 320.0f);
    AddTextCentered("X/Y: Alpha | SELECT: Reset | B: Save", 160, 225, 0.4f, 1.0f, CWhite, 320.0f);
}
