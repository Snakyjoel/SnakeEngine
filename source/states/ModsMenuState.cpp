#include "ModsMenuState.hpp"
#include "MainMenuState.hpp"
#include "AssetConverterState.hpp"
#include "AdpcmEncoder.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../objects/ButtonPrompt.hpp"
#include "../backend/WeekData.hpp"
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

static void drawRotatedRect(float cx, float cy, float w, float h, float angleRad, u32 color, float depth) {
    float c = cosf(angleRad), s = sinf(angleRad);
    float hw = w * 0.5f, hh = h * 0.5f;

    auto rot = [&](float dx, float dy, float& ox, float& oy) {
        ox = cx + dx * c - dy * s;
        oy = cy + dx * s + dy * c;
    };

    float x1, y1, x2, y2, x3, y3, x4, y4;
    rot(-hw, -hh, x1, y1);
    rot( hw, -hh, x2, y2);
    rot( hw,  hh, x3, y3);
    rot(-hw,  hh, x4, y4);

    C2D_DrawTriangle(x1, y1, color, x2, y2, color, x3, y3, color, depth);
    C2D_DrawTriangle(x1, y1, color, x3, y3, color, x4, y4, color, depth);
}

static inline void setScissorBox(float x, float y, float w, float h) {
    C2D_Flush();
    float sLeft = y;
    float sRight = y + h;
    float sTop = x;
    float sBottom = x + w;

    if (sLeft < 0.0f) sLeft = 0.0f;
    if (sRight > 240.0f) sRight = 240.0f;
    if (sTop < 0.0f) sTop = 0.0f;
    if (sBottom > 400.0f) sBottom = 400.0f;

    u32 phys_left = (u32)(240.0f - sRight);
    u32 phys_right = (u32)(240.0f - sLeft);
    u32 phys_top = (u32)(400.0f - sBottom);
    u32 phys_bottom = (u32)(400.0f - sTop);

    C3D_SetScissor(GPU_SCISSOR_NORMAL, phys_left, phys_top, phys_right, phys_bottom);
}

static inline void setScissorBoxClipped(float x, float y, float w, float h, float cx, float cy, float cw, float ch) {
    float clipX = std::max(x, cx);
    float clipY = std::max(y, cy);
    float clipRight = std::min(x + w, cx + cw);
    float clipBottom = std::min(y + h, cy + ch);

    float finalW = std::max(0.0f, clipRight - clipX);
    float finalH = std::max(0.0f, clipBottom - clipY);

    setScissorBox(clipX, clipY, finalW, finalH);
}

static inline float getSpriteWidth(const Frame& f) {
    return f.rotated ? (float)f.h : (float)f.w;
}

static inline float getSpriteHeight(const Frame& f) {
    return f.rotated ? (float)f.w : (float)f.h;
}

static inline void drawSpriteCentered(const Frame& f, float cx, float cy, float depth = 0.51f, float scale = 1.0f, C2D_ImageTint* tint = nullptr) {
    if (!f.tex) return;
    float w = getSpriteWidth(f) * scale;
    float h = getSpriteHeight(f) * scale;
    drawFrameAt(f, cx - (w * 0.5f), cy - (h * 0.5f), depth, tint, scale, scale);
}

static inline void disableScissor() {
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

std::vector<ModMetadata>& ModsMenuState::getActiveList() {
    return ModHandler::get().getMods();
}

void ModsMenuState::init() {
    MusicPlayer::playMenuMusic();

    // Reset isolation
    ModHandler::get().currentModFolder = "";

    VCRFontFix();
    if (!customFont) customFont = C2D_FontLoad("romfs:/fonts/FunkinLingLong.bcfnt");
    vcrFont = customFont ? customFont : globalVCRFont;

    ModHandler::get().scanMods();
    reloadIcons();

    // --- Load UI assets from modMenu spritesheet ---
    modMenuSheet = SpritesheetCache::get().load("preload/images/mods/modMenu");
    if (modMenuSheet) {
        auto getFrame = [&](const std::string& name) -> Frame {
            for (const auto& f : modMenuSheet->frames) {
                if (f.name == name) return f;
            }
            return Frame();
        };
        bgFrame           = getFrame("bg");
        bgwiresFrame      = getFrame("bgwires");
        batteryFrame      = getFrame("battery");
        baseIconFrame     = getFrame("base-icon");
        fallbackIconFrame = getFrame("fallback-icon");
        convertFrame      = getFrame("convert");
        doneFrame         = getFrame("done");
        downFrame         = getFrame("down");
        upFrame           = getFrame("up");
        topFrame          = getFrame("top");
        topTextFrame      = getFrame("top-text");
        toggleFrame       = getFrame("toggle");
        wire4Frame        = getFrame("wire4");
        wire5Frame        = getFrame("wire5");
        wire6Frame        = getFrame("wire6");
        wire7Frame        = getFrame("wire7");
        wire8Frame        = getFrame("wire8");
        wireGFFrame       = getFrame("wireGF");

        btnArrowDown.frame     = downFrame;     btnArrowDown.hasValue     = (downFrame.tex != nullptr);
        btnArrowUp.frame       = upFrame;       btnArrowUp.hasValue       = (upFrame.tex != nullptr);
        btnOnOff.frame         = toggleFrame;   btnOnOff.hasValue         = (toggleFrame.tex != nullptr);
        btnTop.frame           = topFrame;      btnTop.hasValue           = (topFrame.tex != nullptr);
        btnConvertAssets.frame = convertFrame;  btnConvertAssets.hasValue = (convertFrame.tex != nullptr);
        btnDone.frame          = doneFrame;     btnDone.hasValue          = (doneFrame.tex != nullptr);
    }

    lastSelectedCheck = -1;
    updateIconWindow();
}

static void freeIconEntry(ModIconEntry& entry) {
    if (entry.sheet) {
        C2D_SpriteSheetFree(entry.sheet);
        entry.sheet = nullptr;
    }
    if (entry.manualTex) {
        C3D_TexDelete(entry.manualTex);
        delete entry.manualTex;
        entry.manualTex = nullptr;
    }
    if (entry.manualSub) {
        delete entry.manualSub;
        entry.manualSub = nullptr;
    }
    entry.image = {nullptr, nullptr};
}

void ModsMenuState::reloadIcons() {
    for (auto& pair : modIconCache) {
        freeIconEntry(pair.second);
    }
    modIconCache.clear();
}

void ModsMenuState::updateIconWindow() {
    auto& mods = getActiveList();
    if (mods.empty()) return;

    int totalMods = (int)mods.size();
    int minWindow = std::max(0, curSelected - 2);
    int maxWindow = std::min(totalMods - 1, curSelected + 5);

    // 1. Collect set of active mod folder names within window [curSelected - 2, curSelected + 5]
    std::unordered_set<std::string> activeFolders;
    for (int i = minWindow; i <= maxWindow; i++) {
        activeFolders.insert(mods[i].folder);
    }

    // 2. Evict and free any loaded icons outside of the sliding window
    for (auto it = modIconCache.begin(); it != modIconCache.end(); ) {
        if (!it->first.empty() && activeFolders.find(it->first) == activeFolders.end()) {
            freeIconEntry(it->second);
            it = modIconCache.erase(it);
        } else {
            ++it;
        }
    }

    // 3. Preload icons in priority order: curSelected, 5 ahead, 2 behind
    getModIcon(curSelected);
    for (int delta = 1; delta <= 5; delta++) {
        int idx = curSelected + delta;
        if (idx <= maxWindow) {
            getModIcon(idx);
        }
    }
    for (int delta = 1; delta <= 2; delta++) {
        int idx = curSelected - delta;
        if (idx >= minWindow) {
            getModIcon(idx);
        }
    }
}

C2D_Image ModsMenuState::getFallbackIcon() {
    if (fallbackIcon.tex != nullptr) return fallbackIcon;

    if (fallbackIconFrame.tex != nullptr) {
        fallbackIcon = { fallbackIconFrame.tex, &fallbackIconFrame.uv };
        return fallbackIcon;
    }

    std::string fallbackPath = "romfs:/preload/images/menus/noIcon.t3x";
    if (Paths::fileExists(fallbackPath)) {
        fallbackSheet = C2D_SpriteSheetLoad(fallbackPath.c_str());
        if (fallbackSheet) {
            fallbackIcon = C2D_SpriteSheetGetImage(fallbackSheet, 0);
        }
    }
    return fallbackIcon;
}

void ModsMenuState::enforceLRUCache(std::unordered_map<std::string, ModIconEntry>& cache, size_t maxSize) {
    if (cache.size() > maxSize) {
        auto oldest = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        freeIconEntry(oldest->second);
        cache.erase(oldest);
    }
}

C2D_Image ModsMenuState::getModIcon(int idx) {
    auto& mods = getActiveList();
    if (idx < 0 || idx >= (int)mods.size()) return getFallbackIcon();

    std::string folder = mods[idx].folder;
    if (modIconCache.count(folder)) {
        modIconCache[folder].lastAccessFrame = cacheFrameCount;
        return modIconCache[folder].image;
    }

    std::string basePath = std::string("sdmc:/SnakeEngine/") + folder + "/pack";
    std::string rawPath = basePath + ".rawtex";
    std::string t3xPath = basePath + ".t3x";
    std::string pngPath = basePath + ".png";

    std::string targetPath = "";
    bool isPng = false;
    bool isT3x = false;
    bool isRaw = false;

    if (Paths::fileExists(rawPath)) {
        targetPath = rawPath;
        isRaw = true;
    } else if (Paths::fileExists(t3xPath)) {
        targetPath = t3xPath;
        isT3x = true;
    } else if (Paths::fileExists(pngPath)) {
        targetPath = pngPath;
        isPng = true;
    }

    if (targetPath.empty()) {
        ModIconEntry fallbackEntry;
        fallbackEntry.image = getFallbackIcon();
        fallbackEntry.lastAccessFrame = cacheFrameCount;
        // Don't enforce LRU heavily on fallbacks since they share the same texture, but we add it to cache to avoid SD checks
        modIconCache[folder] = fallbackEntry;
        return fallbackEntry.image;
    }

    ModIconEntry entry;
    entry.lastAccessFrame = cacheFrameCount;

    if (isRaw) {
        FILE* f = fopen(targetPath.c_str(), "rb");
        if (f) {
            RawTexHeader header;
            if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                entry.manualTex = new C3D_Tex();
                if (C3D_TexInit(entry.manualTex, header.width, header.height, GPU_RGBA8)) {
                    C3D_TexSetFilter(entry.manualTex, GPU_LINEAR, GPU_LINEAR);
                    
                    size_t dataSize = (size_t)header.width * header.height * 4;
                    void* data = linearAlloc(dataSize);
                    if (data) {
                        fread(data, dataSize, 1, f);
                        C3D_TexUpload(entry.manualTex, data);
                        C3D_TexFlush(entry.manualTex);
                        linearFree(data);
                        
                        entry.manualSub = new Tex3DS_SubTexture();
                        entry.manualSub->width = header.origW; entry.manualSub->height = header.origH;
                        entry.manualSub->left = 0.0f; entry.manualSub->top = 1.0f;
                        entry.manualSub->right = (float)header.origW / header.width;
                        entry.manualSub->bottom = 1.0f - ((float)header.origH / header.height);
                        
                        entry.image = {entry.manualTex, entry.manualSub};
                    } else {
                        delete entry.manualTex;
                        entry.manualTex = nullptr;
                    }
                } else {
                    delete entry.manualTex;
                    entry.manualTex = nullptr;
                }
            }
            fclose(f);
        }
    } else if (isT3x) {
        entry.sheet = C2D_SpriteSheetLoad(targetPath.c_str());
        if (entry.sheet) {
            entry.image = C2D_SpriteSheetGetImage(entry.sheet, 0);
        }
    } else if (isPng) {
        int w, h, c;
        unsigned char* data = stbi_load(targetPath.c_str(), &w, &h, &c, 4);
        if (data) {
            int pw = 1, ph = 1;
            while(pw < w) pw *= 2;
            while(ph < h) ph *= 2;

            entry.manualTex = new C3D_Tex();
            if (C3D_TexInit(entry.manualTex, pw, ph, GPU_RGBA8)) {
                C3D_TexSetFilter(entry.manualTex, GPU_LINEAR, GPU_LINEAR);
                
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
                    C3D_TexUpload(entry.manualTex, swizzled);
                    C3D_TexFlush(entry.manualTex);
                    linearFree(swizzled);

                    entry.manualSub = new Tex3DS_SubTexture();
                    entry.manualSub->width = w; entry.manualSub->height = h;
                    entry.manualSub->left = 0.0f; entry.manualSub->top = 1.0f;
                    entry.manualSub->right = (float)w / pw; entry.manualSub->bottom = 1.0f - ((float)h / ph);

                    entry.image = {entry.manualTex, entry.manualSub};
                } else {
                    delete entry.manualTex;
                    entry.manualTex = nullptr;
                }
            } else {
                delete entry.manualTex;
                entry.manualTex = nullptr;
            }
            stbi_image_free(data);
        }
    }

    if (entry.image.tex != nullptr) {
        enforceLRUCache(modIconCache, 10); // Max 10 loaded icons in RAM at a time
        modIconCache[folder] = entry;
        return entry.image;
    }

    ModIconEntry fallbackEntry;
    fallbackEntry.image = getFallbackIcon();
    fallbackEntry.lastAccessFrame = cacheFrameCount;
    modIconCache[folder] = fallbackEntry;
    return fallbackEntry.image;
}

void ModsMenuState::update(float dt) {
    cacheFrameCount++;
    textScrollTime += dt;
    loadingAngle += dt * 3.14159f * 2.0f;
    if (btnAnimTimer > 0.0f) btnAnimTimer -= dt;
    gridOffset += dt * 32.0f;
    u32 kDown = hidKeysDown();
    auto& mods = getActiveList();
    int totalItems = (int)mods.size() + 1; // SD mods + 1 permanent Default Game Content item at end

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
            if (subSelected < 0) subSelected = 2;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            subSelected++;
            if (subSelected > 2) subSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (kDown & KEY_A) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            if (subSelected != 2) { // 2 is Convert Videos (postponed)
                startConversion();
            }
        }

        if (kDown & KEY_TOUCH) {
            touchPosition touch; hidTouchRead(&touch);
            if (touch.px > 30 && touch.px < 290) {
                float approxIdx = (float)(touch.py - 120) / 50.0f + lerpSubSelected;
                int clickedIdx = (int)std::round(approxIdx);
                if (clickedIdx >= 0 && clickedIdx <= 2) {
                    subSelected = clickedIdx;
                    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                    if (subSelected != 2) {
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
        if (curSelected < 0) curSelected = totalItems - 1;
        textScrollTime = 0.0f;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected >= totalItems) curSelected = 0;
        textScrollTime = 0.0f;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        ModHandler::get().saveConfig();
        WeekData::weeksLoaded.clear();
        WeekData::weeksList.clear();
        switchState(new MainMenuState());
    }

    // Toggle active state for SD mods (Default content item at mods.size() cannot be disabled)
    if ((kDown & KEY_A) || (kDown & KEY_X)) {
        if (curSelected >= 0 && curSelected < (int)mods.size()) {
            mods[curSelected].active = !mods[curSelected].active;
            ModHandler::get().saveConfig();
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            touchedBtnIdx = 5;
            btnAnimTimer = 0.15f;
        }
    }

    // Reorder (only for SD mods, cannot reorder default content item or move past mods.size() - 1)
    if (kDown & KEY_Y) {
        if (curSelected > 0 && curSelected < (int)mods.size()) {
            ModHandler::get().reorderMod(curSelected, curSelected - 1);
            curSelected--;
            reloadIcons();
            updateIconWindow();
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            touchedBtnIdx = 2;
            btnAnimTimer = 0.15f;
        }
    }

    if (curSelected != lastSelectedCheck) {
        lastSelectedCheck = curSelected;
        updateIconWindow();
    }

    lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-10.0f * dt));

    // Touch Handling for Buttons
    if (kDown & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);
        
        float tx = touch.px;
        float ty = touch.py;

        auto isTouched = [&](const Frame& f, float cx, float cy) -> bool {
            if (!f.tex) return false;
            float w = getSpriteWidth(f);
            float h = getSpriteHeight(f);
            return (tx >= cx - (w * 0.5f) && tx <= cx + (w * 0.5f) && ty >= cy - (h * 0.5f) && ty <= cy + (h * 0.5f));
        };
        
        // Convert Assets button -> Opens Conversion Submenu (STATE_CONVERT_MENU)
        if (isTouched(convertFrame, 160.0f, 40.0f)) {
            touchedBtnIdx = 0;
            btnAnimTimer = 0.15f;
            if (curSelected >= 0 && curSelected < (int)mods.size()) {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                currentState = STATE_CONVERT_MENU;
                subSelected = 0;
                lerpSubSelected = 0.0f;
                return;
            }
        }

        // Done button (done sprite at 160, 100) -> Returns to Main Menu
        if (isTouched(doneFrame, 160.0f, 100.0f)) {
            touchedBtnIdx = 1;
            btnAnimTimer = 0.15f;
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            ModHandler::get().saveConfig();
            switchState(new MainMenuState());
            return;
        }

        // Bottom row buttons (cy = 180) - disabled for Base Game item
        if (isTouched(upFrame, 50.0f, 180.0f)) { // UP
            touchedBtnIdx = 2;
            btnAnimTimer = 0.15f;
            if (curSelected > 0 && curSelected < (int)mods.size()) {
                ModHandler::get().reorderMod(curSelected, curSelected - 1);
                curSelected--;
                reloadIcons();
                updateIconWindow();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
        }
        else if (isTouched(downFrame, 123.0f, 180.0f)) { // DOWN
            touchedBtnIdx = 3;
            btnAnimTimer = 0.15f;
            if (curSelected < (int)mods.size() - 1) {
                ModHandler::get().reorderMod(curSelected, curSelected + 1);
                curSelected++;
                reloadIcons();
                updateIconWindow();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
        }
        else if (isTouched(topFrame, 196.0f, 180.0f)) { // TOP
            touchedBtnIdx = 4;
            btnAnimTimer = 0.15f;
            if (curSelected > 0 && curSelected < (int)mods.size()) {
                ModHandler::get().reorderMod(curSelected, 0);
                curSelected = 0;
                reloadIcons();
                updateIconWindow();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
        }
        else if (isTouched(toggleFrame, 270.0f, 180.0f)) { // ON/OFF toggle
            touchedBtnIdx = 5;
            btnAnimTimer = 0.15f;
            if (curSelected < (int)mods.size()) {
                mods[curSelected].active = !mods[curSelected].active;
                ModHandler::get().saveConfig();
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            }
        }
    }

    // Smooth Color Lerp
    u8 tr = 100, tg = 50, tb = 140;
    if (curSelected < (int)mods.size()) {
        const auto& mod = mods[curSelected];
        tr = (u8)(mod.color[0] * 0.75f);
        tg = (u8)(mod.color[1] * 0.75f);
        tb = (u8)(mod.color[2] * 0.75f);
    }
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

void ModsMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();
    auto& mods = getActiveList();
    int totalItems = (int)mods.size() + 1; // SD mods + 1 Default Content item at the end

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    if (bgFrame.tex) {
        drawFrameAt(bgFrame, 0.0f, 0.0f, 0.01f);
    }
    if (topTextFrame.tex) {
        drawFrameCentered(topTextFrame, 200.0f, 16.0f, 0.5f);
    }

    if (currentState == STATE_CONVERTING) {
        AddText("OPTIMIZING MOD ASSETS...", 200, 80, 0.7f, true, 0.0f, CWhite, 0.0f);
        AddText(conversionStatus, 200, 110, 0.45f, true, 0.0f, CWhite, 0.0f);
        
        float bx = 50, by = 140, bw = 300, bh = 20;
        C2D_DrawRectSolid(bx, by, 0.5f, bw, bh, C2D_Color32(60, 60, 60, 255));
        C2D_DrawRectSolid(bx, by, 0.51f, bw * conversionProgress, bh, CYellow);
        
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(0x46, 0x38, 0x4B, 255));
        return;
    }

    if (currentState == STATE_CONVERT_MENU) {
        Alphabet::draw("ASSET CONVERTER", 200, 100, 1.2f, 1.0f, true);
    } else if (totalItems > 0) {
        // --- 1. Container box color #222226 reduced in width by 1/3 to leave space on the right ---
        float containerX = 16.0f;
        float containerY = 36.0f;
        float containerW = 245.0f; // Reduced by 1/3 (leaving space on the right side of the screen)
        float containerH = 196.0f;
        u32 containerColor = C2D_Color32(0x22, 0x22, 0x26, 255);

        C2D_DrawRectSolid(containerX, containerY, 0.40f, containerW, containerH, containerColor);

        // --- 2. Visual scrollbar inside box of track color #2C2B31 and thumb color #524D63 ---
        float sbX = containerX + 6.0f;
        float sbY = containerY + 6.0f;
        float sbW = 5.0f;
        float sbH = containerH - 12.0f;
        u32 scrollTrackColor = C2D_Color32(0x2C, 0x2B, 0x31, 255);
        u32 scrollThumbColor = C2D_Color32(0x52, 0x4D, 0x63, 255);

        C2D_DrawRectSolid(sbX, sbY, 0.41f, sbW, sbH, scrollTrackColor);

        float thumbH = std::max(22.0f, sbH / (float)totalItems);
        float maxScroll = (totalItems > 1) ? (float)(totalItems - 1) : 1.0f;
        float scrollRatio = std::min(1.0f, std::max(0.0f, lerpSelected / maxScroll));
        float thumbY = sbY + scrollRatio * (sbH - thumbH);

        C2D_DrawRectSolid(sbX, thumbY, 0.42f, sbW, thumbH, scrollThumbColor);

        // Shift items slightly right (itemStartX = containerX + 17) to leave room for the scrollbar
        float itemStartX = containerX + 17.0f;
        float itemW = containerW - 23.0f; // Width fitting within the reduced box

        // Scissor mask: items are rendered ONLY inside the container box
        setScissorBox(containerX, containerY, containerW, containerH);

        float itemHeight = 48.0f; // Exact height of 48x48 icon
        float itemSpacing = 54.0f; // 48px height + 6px gap
        float startY = containerY + 6.0f; // 42.0f - matches scrollbar top sbY exactly!

        for (int i = 0; i < totalItems; i++) {
            float dist = ((float)i - lerpSelected);
            float itemY = startY + (dist * itemSpacing);

            // Viewport Culling
            if (itemY + itemHeight < containerY || itemY > containerY + containerH) continue;

            bool isSelected = (i == curSelected);
            bool isDefaultItem = (i == (int)mods.size());
            bool isActive = isDefaultItem ? true : mods[i].active;

            // Selected item: rectangle #59595C of height 48px extending across width
            if (isSelected) {
                u32 selBoxColor = C2D_Color32(0x59, 0x59, 0x5C, 255);
                C2D_DrawRectSolid(itemStartX, itemY, 0.44f, itemW, itemHeight, selBoxColor);
            } else if (!isActive) {
                // Darken inactive item background
                u32 inactiveBg = C2D_Color32(0x14, 0x14, 0x18, 180);
                C2D_DrawRectSolid(itemStartX, itemY, 0.44f, itemW, itemHeight, inactiveBg);
            }

            // Icon (strictly 48x48)
            float iconX = itemStartX;
            float iconY = itemY;

            C2D_ImageTint inactiveTint;
            C2D_PlainImageTint(&inactiveTint, C2D_Color32(70, 70, 70, 255), 0.75f);
            C2D_ImageTint* iconTintPtr = (!isActive) ? &inactiveTint : nullptr;

            if (isDefaultItem) {
                if (baseIconFrame.tex) {
                    float lw = frameLogicalW(baseIconFrame);
                    float lh = frameLogicalH(baseIconFrame);
                    float sx = (lw > 0.0f) ? (48.0f / lw) : 1.0f;
                    float sy = (lh > 0.0f) ? (48.0f / lh) : 1.0f;
                    drawFrameAt(baseIconFrame, iconX, iconY, 0.5f, iconTintPtr, sx, sy);
                }
            } else {
                bool isLoaded = (modIconCache.count(mods[i].folder) > 0);
                if (!isLoaded) {
                    drawRotatedRect(iconX + 24.0f, iconY + 24.0f, 20.0f, 20.0f, loadingAngle, CWhite, 0.5f);
                } else {
                    C2D_Image iconImg = getModIcon(i);
                    if (iconImg.tex != nullptr && iconImg.subtex != nullptr) {
                        float iw = (float)iconImg.subtex->width;
                        float ih = (float)iconImg.subtex->height;
                        float sx = (iw > 0.0f) ? (48.0f / iw) : 1.0f;
                        float sy = (ih > 0.0f) ? (48.0f / ih) : 1.0f;
                        C2D_DrawImageAt(iconImg, iconX, iconY, 0.5f, iconTintPtr, sx, sy);
                    }
                }
            }

            // Text (Name and Description)
            std::string name = isDefaultItem ? "base game" : mods[i].name;
            std::string desc = isDefaultItem ? "default game content" : mods[i].description;

            if (desc.empty()) desc = "No description available.";
            if (desc.length() > 40) {
                desc = desc.substr(0, 40) + "...";
            }

            u32 nameColor = isActive ? CWhite : C2D_Color32(130, 130, 130, 255);
            u32 descColor = isActive ? C2D_Color32(0xCD, 0xCD, 0xCE, 255) : C2D_Color32(0x7D, 0x7D, 0x7E, 255);

            float nameBoxX = itemStartX + 52.0f;
            float nameBoxY = itemY + 4.0f;
            float nameBoxW = itemW - 54.0f;

            // Measure unscaled title width
            C2D_Text tempNameText;
            C2D_TextFontParse(&tempNameText, vcrFont, vcrFontBuf, name.c_str());
            C2D_TextOptimize(&tempNameText);
            float tw = 0.0f, th = 0.0f;
            C2D_TextGetDimensions(&tempNameText, 0.52f, 0.52f, &tw, &th);

            // Scissor mask for the name box intersected with outer container bounds so text never overflows card or container
            setScissorBoxClipped(nameBoxX, nameBoxY - 2.0f, nameBoxW, 20.0f, containerX, containerY, containerW, containerH);

            if (isSelected && tw > nameBoxW) {
                float speed = 35.0f;
                float totalDist = tw + 40.0f;
                float scrollOffset = fmodf(textScrollTime * speed, totalDist);
                float drawX = nameBoxX - scrollOffset;

                AddText(name, drawX, nameBoxY, 0.52f, false, 0.0f, nameColor, 0.0f);
                AddText(name, drawX + totalDist, nameBoxY, 0.52f, false, 0.0f, nameColor, 0.0f);
            } else {
                AddText(name, nameBoxX, nameBoxY, 0.52f, false, 0.0f, nameColor, 0.0f);
            }

            // Restore outer container scissor box
            setScissorBox(containerX, containerY, containerW, containerH);

            // Render description text without black borders
            AddText(desc, nameBoxX, itemY + 26.0f, 0.38f, false, 0.0f, descColor, nameBoxW);
        }

        disableScissor();
    } else {
        AddText("NO MODS DETECTED", 200, 120, 0.7f, true, 0.0f, CWhite, 0.0f);
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(0x46, 0x38, 0x4B, 255));

    if (currentState == STATE_CONVERT_MENU) {
        std::vector<std::string> opts = {"Convert Images", "Convert Audio", "Convert Videos"};
        for (size_t i = 0; i < opts.size(); i++) {
            float dist = ((float)i - lerpSubSelected);
            float itemY = 120.0f + (dist * 50.0f);

            if (itemY < -30.0f || itemY > 270.0f) continue;
            
            float alpha = (i == 2) ? 0.3f : ((i == (size_t)subSelected) ? 1.0f : 0.6f);
            float scale = (i == (size_t)subSelected) ? 1.2f : 0.9f;
            Alphabet::draw(opts[i], 160, itemY, scale, alpha, true);
        }
    } else if (currentState == STATE_IDLE) {
        bool isBaseGame = (curSelected == (int)mods.size());

        C2D_ImageTint darkTint;
        C2D_PlainImageTint(&darkTint, C2D_Color32(60, 60, 60, 255), 0.8f);
        C2D_ImageTint* bottomBtnsTint = isBaseGame ? &darkTint : nullptr;

        auto drawAnimatedBtn = [&](const Frame& f, int btnId, float cx, float cy, C2D_ImageTint* baseTint) {
            if (!f.tex) return;

            if (touchedBtnIdx == btnId && btnAnimTimer > 0.0f) {
                float progress = 1.0f - (btnAnimTimer / 0.15f);
                if (progress < 0.5f) {
                    // Highlight phase: Bright white glow overlay, maintaining alpha and 1.0f scale
                    C2D_SetTintMode(C2D_TintMult);
                    C2D_ImageTint hTint;
                    C2D_PlainImageTint(&hTint, C2D_Color32(255, 255, 255, 255), 0.75f);
                    drawSpriteCentered(f, cx, cy, 0.51f, 1.0f, &hTint);
                } else {
                    // Invert/Press phase: Dark contrast overlay, maintaining alpha and 1.0f scale
                    C2D_SetTintMode(C2D_TintMult);
                    C2D_ImageTint iTint;
                    C2D_PlainImageTint(&iTint, C2D_Color32(30, 30, 30, 255), 0.75f);
                    drawSpriteCentered(f, cx, cy, 0.51f, 1.0f, &iTint);
                }
            } else {
                C2D_SetTintMode(C2D_TintMult);
                drawSpriteCentered(f, cx, cy, 0.51f, 1.0f, baseTint);
            }
        };

        // Render Top Action Buttons
        drawAnimatedBtn(convertFrame, 0, 160.0f, 40.0f, nullptr);
        drawAnimatedBtn(doneFrame, 1, 160.0f, 100.0f, nullptr);

        // Render 4 Bottom Control Buttons (darkened when Base Game is selected)
        drawAnimatedBtn(upFrame, 2, 50.0f, 180.0f, bottomBtnsTint);
        drawAnimatedBtn(downFrame, 3, 123.0f, 180.0f, bottomBtnsTint);
        drawAnimatedBtn(topFrame, 4, 196.0f, 180.0f, bottomBtnsTint);
        drawAnimatedBtn(toggleFrame, 5, 270.0f, 180.0f, bottomBtnsTint);
    }
}

void ModsMenuState::exitState() {
    ModHandler::get().saveConfig();
    reloadIcons();

    if (fallbackSheet) {
        C2D_SpriteSheetFree(fallbackSheet);
        fallbackSheet = nullptr;
    }
    fallbackIcon = {nullptr, nullptr};

    if (customFont) {
        C2D_FontFree(customFont);
        customFont = nullptr;
    }
    vcrFont = nullptr;
    if (vcrFontBuf) {
        C2D_TextBufDelete(vcrFontBuf);
        vcrFontBuf = nullptr;
    }
}

void ModsMenuState::startConversion() {
    auto& mods = getActiveList();
    if (mods.empty() || curSelected < 0) return;
    
    std::string modDir = std::string("sdmc:/SnakeEngine/") + mods[curSelected].folder + "/";
    if (subSelected == 0) { // 0 = Convert Images
        switchState(new AssetConverterState(modDir, false));
        return;
    } else if (subSelected == 1) { // 1 = Convert Audio
        switchState(new AssetConverterState(modDir, true));
        return;
    }
}

void ModsMenuState::processConversion() {
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

