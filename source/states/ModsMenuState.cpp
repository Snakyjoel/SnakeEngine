#include "ModsMenuState.hpp"
#include "MainMenuState.hpp"
#include "AssetConverterState.hpp"
#include "AdpcmEncoder.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <tremor/ivorbisfile.h>
#include <dirent.h>
#include <stdio.h>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "../backend/stb_image.h"

struct RawTexHeader {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t origW;
    uint16_t origH;
};

std::vector<ModMetadata>& ModsMenuState::getActiveList() {
    return ModHandler::get().getMods();
}

void ModsMenuState::init() {
    MusicPlayer::playMenuMusic();

    // Reset isolation
    ModHandler::get().currentModFolder = "";

    VCRFontFix();
    ModHandler::get().scanMods();
    reloadIcons();

    // --- Load buttons (unified spritesheet, 1 GPU texture = 1 draw call) ---
    {
        CachedSpritesheet* cs = SpritesheetCache::get().load("preload/images/menus/modMenu");
        if (cs) {
            auto getFrame = [&](const std::string& name) -> MenuButton {
                for (const auto& f : cs->frames) {
                    if (f.name == name) {
                        MenuButton btn;
                        btn.frame = f;
                        btn.hasValue = true;
                        return btn;
                    }
                }
                MenuButton btn;
                return btn;
            };
            btnConvertAssets = getFrame("convertAssets");
            btnArrowDown     = getFrame("arrowDown");
            btnArrowUp       = getFrame("arrowUp");
            btnOnOff         = getFrame("onoff");
            btnPlay          = getFrame("play");
            btnTop           = getFrame("top");
        }
    }

    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        sheetMenuBG = C2D_SpriteSheetLoad(bgPath.c_str());
        if (sheetMenuBG) imgMenuBG = C2D_SpriteSheetGetImage(sheetMenuBG, 0);
    }
    
    std::string bgbPath = "romfs:/shared/images/menuBGB.t3x";
    if (Paths::fileExists(bgbPath)) {
        sheetMenuBGB = C2D_SpriteSheetLoad(bgbPath.c_str());
        if (sheetMenuBGB) imgMenuBGB = C2D_SpriteSheetGetImage(sheetMenuBGB, 0);
    }
}

void ModsMenuState::reloadIcons() {
    for (auto sheet : iconSheets) C2D_SpriteSheetFree(sheet);
    iconSheets.clear();
    
    for (auto tex : manualTexes) {
        C3D_TexDelete(tex);
        delete tex;
    }
    manualTexes.clear();
    for (auto sub : manualSubtexes) delete sub;
    manualSubtexes.clear();
    
    icons.clear();

    auto& mods = getActiveList();
    for (const auto& mod : mods) {
        std::string basePath = std::string("sdmc:/SnakeEngine/") + mod.folder + "/pack";
        std::string rawPath = basePath + ".rawtex";
        std::string t3xPath = basePath + ".t3x";
        std::string pngPath = basePath + ".png";
        std::string fallbackPath = "romfs:/preload/images/menus/noIcon.t3x";
        std::string loadPngPath = "";

        C2D_Image icon = {nullptr, nullptr};

        if (Paths::fileExists(rawPath)) {
            FILE* f = fopen(rawPath.c_str(), "rb");
            if (f) {
                RawTexHeader header;
                if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                    C3D_Tex* tex = new C3D_Tex();
                    if (C3D_TexInit(tex, header.width, header.height, GPU_RGBA8)) {
                        C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
                        
                        size_t dataSize = (size_t)header.width * header.height * 4;
                        void* data = linearAlloc(dataSize);
                        if (data) {
                            fread(data, dataSize, 1, f);
                            C3D_TexUpload(tex, data);
                            C3D_TexFlush(tex);
                            linearFree(data);
                            
                            Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                            sub->width = header.origW; sub->height = header.origH;
                            sub->left = 0.0f; sub->top = 1.0f;
                            sub->right = (float)header.origW / header.width;
                            sub->bottom = 1.0f - ((float)header.origH / header.height);
                            
                            manualTexes.push_back(tex);
                            manualSubtexes.push_back(sub);
                            icon = {tex, sub};
                        } else {
                            delete tex;
                        }
                    } else {
                        delete tex;
                    }
                }
                fclose(f);
            }
        } else if (Paths::fileExists(t3xPath)) {
            C2D_SpriteSheet s = C2D_SpriteSheetLoad(t3xPath.c_str());
            if (s) {
                iconSheets.push_back(s);
                icon = C2D_SpriteSheetGetImage(s, 0);
            }
        } else if (Paths::fileExists(pngPath)) {
            loadPngPath = pngPath;
        } else if (Paths::fileExists(fallbackPath)) {
            loadPngPath = fallbackPath;
        }

        if (!loadPngPath.empty() && !icon.tex) {
            int w, h, c;
            unsigned char* data = stbi_load(loadPngPath.c_str(), &w, &h, &c, 4);
            if (data) {
                int pw = 1, ph = 1;
                while(pw < w) pw *= 2;
                while(ph < h) ph *= 2;

                C3D_Tex* tex = new C3D_Tex();
                if (C3D_TexInit(tex, pw, ph, GPU_RGBA8)) {
                    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
                    
                    uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                    if (swizzled) {
                        memset(swizzled, 0, pw * ph * 4);
                        
                        for(int y=0; y<h; y++) {
                            for(int x=0; x<w; x++) {
                                int src = (y*w+x)*4;
                                uint32_t px = (data[src]<<24)|(data[src+1]<<16)|(data[src+2]<<8)|data[src+3];
                                uint32_t i = (x & 7) | ((y & 7) << 8);
                                i = (i ^ (i << 2)) & 0x1313;
                                i = (i ^ (i << 1)) & 0x1515;
                                
                                uint32_t tx = x >> 3;
                                uint32_t ty = y >> 3;
                                uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                                uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                                
                                swizzled[tile_start + local_idx] = px;
                            }
                        }
                        C3D_TexUpload(tex, swizzled);
                        C3D_TexFlush(tex);
                        linearFree(swizzled);

                        Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                        sub->width = w; sub->height = h;
                        sub->left = 0.0f; sub->top = 1.0f;
                        sub->right = (float)w / pw; sub->bottom = 1.0f - ((float)h / ph);

                        manualTexes.push_back(tex);
                        manualSubtexes.push_back(sub);
                        icon = {tex, sub};
                    } else {
                        delete tex;
                    }
                } else {
                    delete tex;
                }
                stbi_image_free(data);
            }
        }
        
        icons.push_back(icon);
    }
}

void ModsMenuState::update(float dt) {
    gridOffset += dt * 32.0f;
    u32 kDown = hidKeysDown();
    auto& mods = getActiveList();

    if (currentState == STATE_CONVERTING) {
        processConversion();
        return;
    }



    if (currentState == STATE_CONVERT_MENU) {
        if (kDown & KEY_B) {
            currentState = STATE_IDLE;
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        }
        
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            subSelected--;
            if (subSelected < 0) subSelected = 4;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            subSelected++;
            if (subSelected > 4) subSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (kDown & KEY_A) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            if (subSelected != 3) { // 3 is Convert Videos (postponed)
                startConversion();
            }
        }

        if (kDown & KEY_TOUCH) {
            touchPosition touch; hidTouchRead(&touch);
            if (touch.px > 30 && touch.px < 290) {
                float approxIdx = (float)(touch.py - 120) / 50.0f + lerpSubSelected;
                int clickedIdx = (int)std::round(approxIdx);
                if (clickedIdx >= 0 && clickedIdx <= 4) {
                    subSelected = clickedIdx;
                    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                    if (subSelected != 3) {
                        startConversion();
                    }
                }
            }
        }

        lerpSubSelected += (subSelected - lerpSubSelected) * (1.0f - exp2f(-10.0f * dt));
        return;
    }



    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        curSelected--;
        if (curSelected < 0) curSelected = (int)mods.size() - 1;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected >= (int)mods.size()) curSelected = 0;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        ModHandler::get().saveConfig();
        switchState(new MainMenuState());
    }

    if ((kDown & KEY_A) || (kDown & KEY_X)) {
        if (!mods.empty() && curSelected >= 0 && curSelected < (int)mods.size()) {
            mods[curSelected].active = !mods[curSelected].active;
            ModHandler::get().saveConfig();
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        }
    }



    if (kDown & KEY_Y) {
        if (curSelected > 0) {
            ModHandler::get().reorderMod(curSelected, curSelected - 1);
            curSelected--;
            reloadIcons();
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
    }

    lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-10.0f * dt));

    // Touch Handling for Buttons
    if (kDown & KEY_TOUCH && !mods.empty()) {
        touchPosition touch;
        hidTouchRead(&touch);
        
        float tx = touch.px;
        float ty = touch.py;
        float cx = 160.0f; // Center of screen
        
        if (tx > cx - 81 && tx < cx + 81 && ty > 100 - 27 && ty < 100 + 27) {
            currentState = STATE_CONVERT_MENU;
            subSelected = 0;
            lerpSubSelected = 0.0f;
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            return;
        }
        
        auto& mod = mods[curSelected];

        // Bottom horizontal row: Y=200, height=58, width=58 (half = 29)
        if (ty > 200 - 29 && ty < 200 + 29) {
            if (tx > 60 - 29 && tx < 60 + 29) { // UP
                if (curSelected > 0) {
                    ModHandler::get().reorderMod(curSelected, curSelected - 1);
                    curSelected--;
                    reloadIcons();
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                }
            }
            else if (tx > 126 - 29 && tx < 126 + 29) { // DOWN
                if (curSelected < (int)mods.size() - 1) {
                    ModHandler::get().reorderMod(curSelected, curSelected + 1);
                    curSelected++;
                    reloadIcons();
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                }
            }
            else if (tx > 193 - 29 && tx < 193 + 29) { // TOP
                if (curSelected > 0) {
                    ModHandler::get().reorderMod(curSelected, 0);
                    curSelected = 0;
                    reloadIcons();
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                }
            }
            else if (tx > 260 - 29 && tx < 260 + 29) { // ON/OFF
                mod.active = !mod.active;
                ModHandler::get().saveConfig();
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            }
        }
    }

    // Smooth Color Lerp
    if (!mods.empty()) {
        const auto& mod = mods[curSelected];
        // Darken and desaturate the target color (30% of original intensity)
        u8 tr = (u8)(mod.color[0] * 0.75f);
        u8 tg = (u8)(mod.color[1] * 0.75f);
        u8 tb = (u8)(mod.color[2] * 0.75f);
        targetColor = C2D_Color32(tr, tg, tb, 255);
        
        auto lerpChannel = [&](u8 current, u8 target) -> u8 {
            float c = (float)current;
            float t = (float)target;
            c += (t - c) * (1.0f - exp2f(-5.0f * dt));
            return (u8)c;
        };

        u8 r = lerpChannel((currentColor >> 0) & 0xFF, (targetColor >> 0) & 0xFF);
        u8 g = lerpChannel((currentColor >> 8) & 0xFF, (targetColor >> 8) & 0xFF);
        u8 b = lerpChannel((currentColor >> 16) & 0xFF, (targetColor >> 16) & 0xFF);
        currentColor = C2D_Color32(r, g, b, 255);
    }
}

void ModsMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();
    auto& mods = getActiveList();

    auto darkened = [](u32 col, int brightness, int alpha) -> u32 {
        return C2D_Color32(
            (col & 0xFF) * brightness / 255,
            ((col >> 8) & 0xFF) * brightness / 255,
            ((col >> 16) & 0xFF) * brightness / 255,
            alpha
        );
    };
    auto drawGrid = [&](float w, float h, u32 col) {
        float gridSize = 32.0f;
        float gx = fmodf(gridOffset, gridSize);
        float gy = fmodf(gridOffset, gridSize);
        u32 darkCol = darkened(col, 40, 80);
        for (float x = -gridSize; x < w + gridSize; x += gridSize) {
            for (float y = -gridSize; y < h + gridSize; y += gridSize) {
                if ((int(x / gridSize) + int(y / gridSize)) % 2 == 0)
                    C2D_DrawRectSolid(x + gx, y + gy, 0.01f, gridSize, gridSize, darkCol);
            }
        }
    };

    C2D_SceneBegin(top);
    C2D_TargetClear(top, currentColor);
    drawGrid(400, 240, currentColor);

    // Draw menuBG decoration
    if (imgMenuBG.tex) drawCenteredBG(imgMenuBG, 400.0f, 240.0f, 0.1f);

    if (currentState == STATE_CONVERTING) {
        AddText("OPTIMIZING MOD ASSETS...", 200, 80, 0.7f, true, 2.0f, CWhite, 0.0f);
        AddText(conversionStatus, 200, 110, 0.45f, true, 1.5f, CWhite, 0.0f);
        
        float bx = 50, by = 140, bw = 300, bh = 20;
        C2D_DrawRectSolid(bx, by, 0.5f, bw, bh, C2D_Color32(60, 60, 60, 255));
        C2D_DrawRectSolid(bx, by, 0.51f, bw * conversionProgress, bh, CYellow);
        
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, currentColor);
        drawGrid(320, 240, currentColor);
        if (imgMenuBGB.tex) drawCenteredBG(imgMenuBGB, 320.0f, 240.0f, 0.1f);
        return;
    }

    if (currentState == STATE_CONVERT_MENU) {
        Alphabet::draw("ASSET CONVERTER", 200, 100, 1.2f, 1.0f, true);
    } else if (!mods.empty()) {
        float iconX = 45.0f;
        float centerY = 48.0f; 

        for (int i = -1; i <= 3; i++) {
            int idx = curSelected + i;
            if (idx < 0 || idx >= (int)mods.size()) continue;

            float dist = (idx - lerpSelected);
            float itemY = centerY + (dist * 65.0f);
            
            float targetScale = (idx == curSelected) ? 0.5f : 0.4f;
            float scale = targetScale;

            if (idx >= 0 && idx < (int)icons.size()) {
                if (icons[idx].tex != nullptr && icons[idx].subtex != nullptr) {
                    float iw = 150 * scale;
                    float ih = 150 * scale;
                    
                    if (idx == curSelected) {
                        C2D_DrawImageAt(icons[idx], iconX - (iw/2), itemY - (ih/2), 0.5f, NULL, scale, scale);
                    } else {
                        C2D_ImageTint tint;
                        C2D_PlainImageTint(&tint, C2D_Color32(0, 0, 0, 255), 0.5f); 
                        C2D_DrawImageAt(icons[idx], iconX - (iw/2), itemY - (ih/2), 0.5f, &tint, scale, scale);
                    }
                }
            }
        }

        // --- Info panel (right side) ---
        const auto& mod = mods[curSelected];
        float panelX = 100.0f;
        
        float boxW = 300.0f;
        float boxH = 210.0f;
        float bx = panelX - 10;
        float by = 10.5f;
        u32 boxColor = C2D_Color32(0, 0, 0, 160);
        
        C2D_DrawRectSolid(bx + 5, by, 0.44f, boxW - 10, boxH, boxColor);
        C2D_DrawRectSolid(bx, by + 5, 0.44f, boxW, boxH - 10, boxColor);
        C2D_DrawCircleSolid(bx + 5, by + 5, 0.44f, 5, boxColor);
        C2D_DrawCircleSolid(bx + boxW - 5, by + 5, 0.44f, 5, boxColor);
        C2D_DrawCircleSolid(bx + 5, by + boxH - 5, 0.44f, 5, boxColor);
        C2D_DrawCircleSolid(bx + boxW - 5, by + boxH - 5, 0.44f, 5, boxColor);

        AddText(mod.name, panelX, by + 10, 0.65f, false, 2.0f, CWhite, 280.0f);
        C2D_DrawRectSolid(panelX, by + 40, 0.45f, 280, 2, C2D_Color32(255, 255, 255, 100));
        
        std::string desc = mod.description;
        if (desc.empty()) desc = "No description available.";
        drawWrappedText(desc, panelX, by + 55, 0.4f, 280.0f, C2D_Color32(200, 200, 200, 255));
    } else {
        AddText("NO MODS DETECTED", 200, 120, 0.7f, true, 2.0f, CWhite, 0.0f);
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, currentColor);
    drawGrid(320, 240, currentColor);
    
    if (imgMenuBGB.tex) drawCenteredBG(imgMenuBGB, 320.0f, 240.0f, 0.1f);

    if (currentState == STATE_CONVERT_MENU) {
        std::vector<std::string> opts = {"Convert All", "Convert Images", "Convert Audio", "Convert Videos", "Custom"};
        for (size_t i = 0; i < opts.size(); i++) {
            float dist = ((float)i - lerpSubSelected);
            float itemY = 120.0f + (dist * 50.0f); // 50.0f spacing for larger text

            if (itemY < -30.0f || itemY > 270.0f) continue;
            
            float alpha = (i == 3) ? 0.3f : ((i == (size_t)subSelected) ? 1.0f : 0.6f);
            float scale = (i == (size_t)subSelected) ? 1.2f : 0.9f;
            Alphabet::draw(opts[i], 160, itemY, scale, alpha, true);
        }
    } else if (!mods.empty() && currentState == STATE_IDLE) {
        const auto& mod = mods[curSelected];
        u32 darkBox = C2D_Color32(0, 0, 0, 180);

        auto drawBtnBG = [&](float x, float y, float w, float h, u32 col) {
            C2D_DrawRectSolid(x - (w/2) + 4, y - (h/2), 0.5f, w - 8, h, col);
            C2D_DrawRectSolid(x - (w/2), y - (h/2) + 4, 0.5f, w, h - 8, col);
            C2D_DrawCircleSolid(x - (w/2) + 4, y - (h/2) + 4, 0.5f, 4, col);
            C2D_DrawCircleSolid(x + (w/2) - 4, y - (h/2) + 4, 0.5f, 4, col);
            C2D_DrawCircleSolid(x - (w/2) + 4, y + (h/2) - 4, 0.5f, 4, col);
            C2D_DrawCircleSolid(x + (w/2) - 4, y + (h/2) - 4, 0.5f, 4, col);
        };
        
        auto drawBtn = [&](const MenuButton& btn, float x, float y, float depth) {
            if (!btn.hasValue || !btn.frame.tex) return;
            drawFrameCentered(btn.frame, x, y, depth);
        };

        Alphabet::draw("MODS", 160, 10, 1.2f, 1.0f, true);

        // Convert button (center)
        drawBtn(btnConvertAssets, 160, 100, 0.51f);

        // Bottom horizontal row buttons
        drawBtnBG(60, 200, 58, 58, darkBox);
        drawBtn(btnArrowUp, 60, 200, 0.51f);

        drawBtnBG(126, 200, 58, 58, darkBox);
        drawBtn(btnArrowDown, 126, 200, 0.51f);

        drawBtnBG(193, 200, 58, 58, darkBox);
        drawBtn(btnTop, 193, 200, 0.51f);
        
        u32 statusColor = mod.active ? C2D_Color32(40, 180, 40, 200) : C2D_Color32(180, 40, 40, 200);
        drawBtnBG(260, 200, 58, 58, statusColor);
        drawBtn(btnOnOff, 260, 200, 0.51f);
    }
}

void ModsMenuState::exitState() {
    ModHandler::get().saveConfig();
    for (auto sheet : iconSheets) C2D_SpriteSheetFree(sheet);

    if (sheetMenuBG) C2D_SpriteSheetFree(sheetMenuBG);
    if (sheetMenuBGB) C2D_SpriteSheetFree(sheetMenuBGB);

    for (auto tex : manualTexes) {
        C3D_TexDelete(tex);
        delete tex;
    }
    manualTexes.clear();
    for (auto sub : manualSubtexes) delete sub;
    manualSubtexes.clear();

    C2D_TextBufDelete(vcrFontBuf);
}

void ModsMenuState::startConversion() {
    auto& mods = getActiveList();
    if (mods.empty() || curSelected < 0) return;
    
    std::string modDir = std::string("sdmc:/SnakeEngine/") + mods[curSelected].folder + "/";
    if (subSelected == 4) { // Custom
        switchState(new AssetConverterState(modDir));
        return;
    }

    conversionTargets.clear();

    if (subSelected == 0 || subSelected == 1) {
        std::vector<std::string> dirsToScan = {modDir};
        while (!dirsToScan.empty()) {
            std::string currentDir = dirsToScan.back();
            dirsToScan.pop_back();
            
            DIR* dir = opendir(currentDir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..") continue;
                    
                    std::string fullPath = currentDir + name;
                    bool isDir = (entry->d_type == DT_DIR);
                    if (entry->d_type == DT_UNKNOWN) {
                        struct stat st;
                        if (stat(fullPath.c_str(), &st) == 0) {
                            isDir = S_ISDIR(st.st_mode);
                        }
                    }

                    if (isDir) {
                        dirsToScan.push_back(fullPath + "/");
                    } else if (name.size() > 4 && name.substr(name.size() - 4) == ".png") {
                        std::string relPath = fullPath.substr(modDir.size());
                        conversionTargets.push_back(relPath);
                    }
                }
                closedir(dir);
            }
        }
    }

    if (subSelected == 0 || subSelected == 2) {
        std::vector<std::string> dirsToScan = {modDir};
        while (!dirsToScan.empty()) {
            std::string currentDir = dirsToScan.back();
            dirsToScan.pop_back();
            
            DIR* dir = opendir(currentDir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..") continue;
                    
                    std::string fullPath = currentDir + name;
                    bool isDir = (entry->d_type == DT_DIR);
                    if (entry->d_type == DT_UNKNOWN) {
                        struct stat st;
                        if (stat(fullPath.c_str(), &st) == 0) {
                            isDir = S_ISDIR(st.st_mode);
                        }
                    }

                    if (isDir) {
                        dirsToScan.push_back(fullPath + "/");
                    } else if (name.size() > 4 && name.substr(name.size() - 4) == ".ogg") {
                        std::string relPath = fullPath.substr(modDir.size());
                        conversionTargets.push_back(relPath);
                    }
                }
                closedir(dir);
            }
        }
    }

    if (conversionTargets.empty()) {
        currentState = STATE_IDLE;
        return;
    }

    currentState = STATE_CONVERTING;
    conversionProgress = 0;
    currentTargetIdx = 0;
    conversionStatus = "Initializing...";
}

void ModsMenuState::processConversion() {
    auto& mods = getActiveList();
    std::string modDir = std::string("sdmc:/SnakeEngine/") + mods[curSelected].folder + "/";
    
    if (isAudioPhase) {
        if (!convFIn || !convFOut || !convVfPtr) {
            isAudioPhase = false;
            currentTargetIdx++;
            return;
        }

        OggVorbis_File* vf = (OggVorbis_File*)convVfPtr;
        AdpcmEncoder::State* state = (AdpcmEncoder::State*)convStatePtr;
        
        const int CHUNK_SAMPLES = 8192;
        int16_t* pcmBuffer = (int16_t*)malloc(CHUNK_SAMPLES * convChannels * sizeof(int16_t));
        int16_t* monoBuffer = (int16_t*)malloc(CHUNK_SAMPLES * sizeof(int16_t));

        int bitstream;
        long read = ov_read(vf, (char*)pcmBuffer, CHUNK_SAMPLES * convChannels * sizeof(int16_t), &bitstream);
        
        if (read > 0) {
            int samplesRead = read / (convChannels * sizeof(int16_t));
            if (convChannels == 2) {
                for (int i = 0; i < samplesRead; i++) {
                    monoBuffer[i] = (int16_t)(((int32_t)pcmBuffer[i*2] + (int32_t)pcmBuffer[i*2+1]) / 2);
                }
            } else {
                memcpy(monoBuffer, pcmBuffer, samplesRead * sizeof(int16_t));
            }

            std::vector<uint8_t> adpcm = AdpcmEncoder::encodeIMA(monoBuffer, samplesRead, *state);
            convSamplesProcessed += samplesRead;
            if (convSamplesProcessed >= convTotalSamples) {
                AdpcmEncoder::flush(adpcm, *state);
            }
            fwrite(adpcm.data(), 1, adpcm.size(), convFOut);
            
            conversionProgress = (float)currentTargetIdx / conversionTargets.size();
            conversionProgress += ((float)convSamplesProcessed / convTotalSamples) * (1.0f / conversionTargets.size());
            conversionStatus = "Converting Audio: " + conversionTargets[currentTargetIdx] + " (" + std::to_string(convSamplesProcessed/1000) + "k / " + std::to_string(convTotalSamples/1000) + "k)";
        } else {
            fclose(convFOut);
            std::string oggPath = modDir + conversionTargets[currentTargetIdx];
            ov_clear(vf);
            free(convVfPtr);
            delete (AdpcmEncoder::State*)convStatePtr;

            remove(oggPath.c_str());

            convFIn = nullptr; convFOut = nullptr; convVfPtr = nullptr; convStatePtr = nullptr;
            isAudioPhase = false;
            currentTargetIdx++;
        }

        free(pcmBuffer);
        free(monoBuffer);
        return;
    }

    if (currentTargetIdx >= (int)conversionTargets.size()) {
        currentTargetIdx = 0;
        currentState = STATE_IDLE;
        reloadIcons();
        return;
    }

    std::string file = conversionTargets[currentTargetIdx];
    std::string fullPath = modDir + file;
    conversionStatus = "Checking: " + file;
    conversionProgress = (float)currentTargetIdx / conversionTargets.size();

    if (Paths::fileExists(fullPath)) {
        if (file.find(".png") != std::string::npos || file.find(".t3x") != std::string::npos) {
            std::string outPath = modDir + file.substr(0, file.find_last_of(".")) + ".rawtex";
            conversionStatus = "Converting Image: " + file;
            
            int w, h, c;
            unsigned char* data = nullptr;
            if (file.substr(file.find_last_of(".") + 1) != "t3x") {
                data = stbi_load(fullPath.c_str(), &w, &h, &c, 4);
            }

            if (data) {
                int pw = 1, ph = 1;
                while(pw < w) pw *= 2;
                while(ph < h) ph *= 2;

                FILE* f = fopen(outPath.c_str(), "wb");
                if (f) {
                    RawTexHeader header;
                    memcpy(header.magic, "RWTX", 4);
                    header.width = pw; header.height = ph;
                    header.origW = w; header.origH = h;
                    fwrite(&header, sizeof(RawTexHeader), 1, f);
                    uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                    memset(swizzled, 0, pw * ph * 4);
                    for(int y=0; y<h; y++) {
                        for(int x=0; x<w; x++) {
                            int src = (y*w+x)*4;
                            uint32_t px = (data[src]<<24)|(data[src+1]<<16)|(data[src+2]<<8)|data[src+3];
                            uint32_t i = (x & 7) | ((y & 7) << 8);
                            i = (i ^ (i << 2)) & 0x1313;
                            i = (i ^ (i << 1)) & 0x1515;
                            
                            uint32_t tx = x >> 3;
                            uint32_t ty = y >> 3;
                            uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                            uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                            
                            swizzled[tile_start + local_idx] = px;
                        }
                    }
                    fwrite(swizzled, pw * ph * 4, 1, f);
                    linearFree(swizzled);
                    fclose(f);
                    remove(fullPath.c_str());
                }
                stbi_image_free(data);
            }
            currentTargetIdx++;
        } else if (file.find(".ogg") != std::string::npos) {
            std::string outPath = modDir + file.substr(0, file.find_last_of(".")) + ".adp";
            conversionStatus = "Opening Audio: " + file;

            convFIn = fopen(fullPath.c_str(), "rb");
            if (convFIn) {
                OggVorbis_File* vf = (OggVorbis_File*)malloc(sizeof(OggVorbis_File));
                if (ov_open(convFIn, vf, NULL, 0) == 0) {
                    vorbis_info* vi = ov_info(vf, -1);
                    convChannels = vi->channels;
                    convTotalSamples = (uint32_t)ov_pcm_total(vf, -1);
                    convFOut = fopen(outPath.c_str(), "wb");
                    if (convFOut && convTotalSamples > 0) {
                        AdpcmEncoder::Header header;
                        memset(&header, 0, sizeof(header));
                        memcpy(header.magic, "SADP", 4);
                        header.sampleRate = vi->rate;
                        header.numSamples = convTotalSamples;
                        header.channels = 1;
                        fwrite(&header, sizeof(header), 1, convFOut);

                        convVfPtr = vf;
                        convStatePtr = new AdpcmEncoder::State();
                        convSamplesProcessed = 0;
                        isAudioPhase = true;
                        return; // Start incremental phase next frame
                    }
                }
                if (vf) { ov_clear(vf); free(vf); }
            }
            currentTargetIdx++;
        }
    } else {
        currentTargetIdx++;
    }
}


float ModsMenuState::drawWrappedText(const std::string& text, float x, float y, float scale, float wrapWidth, u32 color) {
    std::stringstream ss(text);
    std::string word;
    std::string line = "";
    float currentY = y;
    float lineHeight = 24.0f * scale;
    
    while (ss >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        
        C2D_Text gText;
        C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, testLine.c_str());
        
        float tw, th;
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
        
        if (tw > wrapWidth && !line.empty()) {
            AddText(line, x, currentY, scale, false, 0.0f, color, 0.0f);
            line = word;
            currentY += lineHeight;
        } else {
            line = testLine;
        }
    }
    if (!line.empty()) {
        AddText(line, x, currentY, scale, false, 0.0f, color, 0.0f);
    }
    currentY += lineHeight;
    return currentY;
}

