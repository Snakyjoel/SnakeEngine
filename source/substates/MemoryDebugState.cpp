#include "MemoryDebugState.hpp"
#include <malloc.h>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../states/PlayState.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../shaders/ShaderManager.hpp"
#include "../backend/LuaManager.hpp"
#include "../objects/Character.hpp"
#include "../objects/Stage.hpp"
#include "../objects/InGameVideoPlayer.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/Macros.hpp"

static u32 getTexSize(C3D_Tex* tex) {
    if (!tex) return 0;
    u32 w = tex->width;
    u32 h = tex->height;
    u32 bpp = 4;
    switch (tex->fmt) {
        case GPU_RGBA8: bpp = 4; break;
        case GPU_RGB8: bpp = 3; break;
        case GPU_RGBA5551:
        case GPU_RGB565:
        case GPU_RGBA4:
        case GPU_LA8: bpp = 2; break;
        case GPU_A8:
        case GPU_L8:
        case GPU_ETC1A4: bpp = 1; break;
        case GPU_L4:
        case GPU_A4:
        case GPU_ETC1: return (w * h) / 2;
        default: bpp = 4; break;
    }
    return w * h * bpp;
}

static bool isVRAM(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >= 0x1F000000 && addr < 0x1F600000);
}

static std::string formatBytes(u32 bytes) {
    if (bytes == 0) return "0B";
    if (bytes < 1024) return std::to_string(bytes) + "B";
    char buf[32];
    if (bytes < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1fKB", (float)bytes / 1024.0f);
    } else {
        snprintf(buf, sizeof(buf), "%.2fMB", (float)bytes / (1024.0f * 1024.0f));
    }
    return std::string(buf);
}

static std::string makeAsciiBar(float ratio, int totalBars = 8) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    int filled = (int)roundf(ratio * (float)totalBars);
    if (filled > totalBars) filled = totalBars;
    std::string s = "[";
    for (int i = 0; i < filled; i++) s += "#";
    for (int i = filled; i < totalBars; i++) s += "-";
    s += "]";
    return s;
}

MemoryDebugState::MemoryDebugState() {
    dynamicTextBuf = C2D_TextBufNew(2048);
    active = true;
    sortMode = SortMode::DEFAULT;
    memorySelection = 0;
    memoryScrollLerp = 0.0f;
    gatherAllMemoryItems();
}

MemoryDebugState::~MemoryDebugState() {
    if (dynamicTextBuf) {
        C2D_TextBufDelete(dynamicTextBuf);
        dynamicTextBuf = nullptr;
    }
}

void MemoryDebugState::gatherAllMemoryItems() {
    rawItems.clear();
    std::set<void*> addedTexData;

    auto addTex = [&](const std::string& name, C3D_Tex* tex, const std::string& cat = "Texture") {
        if (!tex) return;
        MemoryItem item;
        item.name = name;
        item.category = cat;
        if (tex->data && addedTexData.count(tex->data) > 0) {
            item.size = 0;
            item.name += " (Shared)";
        } else {
            item.size = getTexSize(tex);
            if (tex->data) addedTexData.insert(tex->data);
        }
        item.isVram = isVRAM(tex->data);
        rawItems.push_back(item);
    };

    auto addSheet = [&](const std::string& name, C2D_SpriteSheet sheet, const std::string& cat = "Texture") {
        if (!sheet) return;
        C2D_Image img = C2D_SpriteSheetGetImage(sheet, 0);
        addTex(name, img.tex, cat);
    };

    // Spritesheet Cache
    const auto& cache = SpritesheetCache::get().getCache();
    for (const auto& pair : cache) {
        if (pair.second.sheet) addSheet(pair.first, pair.second.sheet, "Cache");
        for (size_t i = 0; i < pair.second.subSheets.size(); i++) {
            addSheet(pair.first + "_[" + std::to_string(i) + "]", pair.second.subSheets[i], "Cache");
        }
    }

    if (PlayState::instance) {
        // Characters
        if (PlayState::instance->bf) addSheet("BF Character", PlayState::instance->bf->sheet, "Character");
        if (PlayState::instance->dad) addSheet("Dad Character", PlayState::instance->dad->sheet, "Character");
        if (PlayState::instance->gf) addSheet("GF Character", PlayState::instance->gf->sheet, "Character");
        if (PlayState::instance->deadBF) addSheet("deadBF Character", PlayState::instance->deadBF->sheet, "Character");

        // Icons
        if (PlayState::instance->iconBf.sheet) addSheet("Icon: " + PlayState::instance->iconBf.iconName, PlayState::instance->iconBf.sheet, "Icon");
        else if (PlayState::instance->iconBf.tex.data) addTex("Icon: " + PlayState::instance->iconBf.iconName, &PlayState::instance->iconBf.tex, "Icon");

        if (PlayState::instance->iconDad.sheet) addSheet("Icon: " + PlayState::instance->iconDad.iconName, PlayState::instance->iconDad.sheet, "Icon");
        else if (PlayState::instance->iconDad.tex.data) addTex("Icon: " + PlayState::instance->iconDad.iconName, &PlayState::instance->iconDad.tex, "Icon");

        // Notes & Ratings
        addSheet("noteSheet", PlayState::instance->noteSheet, "Gameplay");
        if (PlayState::instance->fastNoteSheet) {
            addSheet("fastNoteSheet", PlayState::instance->fastNoteSheet, "Gameplay");
        }
        addSheet("ratingSheet", PlayState::instance->ratingSheet, "Gameplay");
        if (PlayState::instance->countdownSheet) {
            addSheet("countdownSheet", PlayState::instance->countdownSheet, "Gameplay");
        }

        for (const auto& pair : PlayState::instance->customNoteSheets) {
            if (pair.second) addSheet("CustomSheet: " + pair.first, pair.second, "Gameplay");
        }
        for (const auto& pair : PlayState::instance->customNoteImages) {
            if (pair.second.tex) addTex("CustomNote: " + pair.first, pair.second.tex, "Gameplay");
        }

        // Cached Icons
        for (const auto& pair : PlayState::instance->healthIconCache) {
            if (pair.second.sheet) addSheet("IconCache: " + pair.first, pair.second.sheet, "Icon");
            else if (pair.second.tex.data) addTex("IconCache: " + pair.first, (C3D_Tex*)&pair.second.tex, "Icon");
        }

        // Stage Sprites
        if (PlayState::instance->currentStage) {
            for (size_t i = 0; i < PlayState::instance->currentStage->sprites.size(); i++) {
                const auto& s = PlayState::instance->currentStage->sprites[i];
                if (s.sheet) {
                    addSheet("StageSprite[" + std::to_string(i) + "]", s.sheet, "Stage");
                }
            }
        }
        if (PlayState::instance->lazyBGSheet) {
            addSheet("lazyBGSheet", PlayState::instance->lazyBGSheet, "Stage");
        }
        if (PlayState::instance->lazyRawTex) {
            addTex("lazyRawTex", PlayState::instance->lazyRawTex, "Stage");
        }

        // Lua
        for (const auto& pair : PlayState::instance->luaTextureCache) {
            if (pair.second.sheet) addSheet("LuaCache: " + pair.first, pair.second.sheet, "Lua");
            else if (pair.second.tex) addTex("LuaCache: " + pair.first, pair.second.tex, "Lua");
        }

        for (const auto& s : PlayState::instance->luaSprites) {
            if (s.sheet) {
                addSheet("Lua: " + s.name, s.sheet, "Lua");
            } else if (s.img.tex) {
                addTex("Lua: " + s.name, s.img.tex, "Lua");
            } else if (s.isGraphic) {
                MemoryItem item;
                item.name = "LuaGraphic: " + s.name;
                item.size = 0;
                item.isVram = false;
                item.category = "Lua";
                rawItems.push_back(item);
            }
        }

        for (const auto& t : PlayState::instance->luaTexts) {
            if (t.buf) {
                MemoryItem item;
                item.name = "Text: " + t.tag;
                item.size = 512;
                item.isVram = false;
                item.category = "Text";
                rawItems.push_back(item);
            }
        }

        // Chart Notes Vector
        if (!PlayState::instance->songNotes.empty()) {
            MemoryItem item;
            item.name = "Chart Notes (" + std::to_string(PlayState::instance->songNotes.size()) + ")";
            item.size = PlayState::instance->songNotes.size() * sizeof(Note);
            item.isVram = false;
            item.category = "Gameplay";
            rawItems.push_back(item);
        }
    }

    // Lua VM State Memory
    if (LuaManager::get().isOpen()) {
        size_t luaMem = LuaManager::get().getMemoryUsed();
        if (luaMem > 0) {
            MemoryItem item;
            item.name = "Lua VM Heap";
            item.size = (u32)luaMem;
            item.isVram = false;
            item.category = "Lua";
            rawItems.push_back(item);
        }
    }

    // Shaders
    auto targets = ShaderManager::get().getActiveTargets();
    for (const auto& t : targets) {
        addTex("Shader: " + t.first, t.second, "Shader");
    }

    // Audio
    const auto& sndCache = AudioEngine::getSoundCache();
    for (const auto& pair : sndCache) {
        if (pair.second.buffer) {
            MemoryItem item;
            item.name = "Audio: " + pair.first;
            item.size = pair.second.samplesPerChannel * pair.second.channels * 2;
            item.isVram = false;
            item.category = "Audio";
            rawItems.push_back(item);
        }
    }

    if (AudioEngine::isLoaded) {
        MemoryItem item;
        item.name = "Audio: Song Inst";
        item.size = 4 * 2048 * 2 * 2;
        item.isVram = false;
        item.category = "Audio";
        rawItems.push_back(item);

        if (AudioEngine::hasVocals) {
            MemoryItem itemV;
            itemV.name = "Audio: Song Voc";
            itemV.size = 4 * 2048 * 2 * 2;
            itemV.isVram = false;
            itemV.category = "Audio";
            rawItems.push_back(itemV);
        }
    }

    // Text Buffers
    auto addTextBuf = [&](const std::string& name, C2D_TextBuf buf, size_t sizeVal) {
        if (!buf) return;
        MemoryItem item;
        item.name = name;
        item.size = sizeVal * 24;
        item.isVram = false;
        item.category = "Text";
        rawItems.push_back(item);
    };

    if (PlayState::instance) {
        addTextBuf("TextBuf: vcrFont", PlayState::instance->vcrFontBuf, 2048);
        addTextBuf("TextBuf: botplay", PlayState::instance->botplayTextBuf, 32);
        addTextBuf("TextBuf: timeText", PlayState::instance->timeTextBuf, 32);
        addTextBuf("TextBuf: lyrics", PlayState::instance->lyricsTextBuf, 2048);
        addTextBuf("TextBuf: debug", PlayState::instance->debugTextBuf, 2048);
    }

    // In-Game Video Player
    if (PlayState::instance && PlayState::instance->inGameVideo) {
        auto* vid = PlayState::instance->inGameVideo;
        for (int i = 0; i < 2; i++) {
            const C3D_Tex* t = vid->getTex(i);
            if (t && t->data) {
                addTex("Video: Tex[" + std::to_string(i) + "]", (C3D_Tex*)t, "Video");
            }
        }
        size_t ringMem = vid->getDecoder().getLinearMemoryUsed();
        if (ringMem > 0) {
            MemoryItem item;
            item.name = "Video: RingBuffer";
            item.size = (u32)ringMem;
            item.isVram = false;
            item.category = "Video";
            rawItems.push_back(item);
        }
        size_t heapMem = vid->getDecoder().getHeapMemoryUsed();
        if (heapMem > 0) {
            MemoryItem item;
            item.name = "Video: StreamBuf";
            item.size = (u32)heapMem;
            item.isVram = false;
            item.category = "Video";
            rawItems.push_back(item);
        }
    }

    applySort();
}

void MemoryDebugState::applySort() {
    memoryItems = rawItems;

    switch (sortMode) {
        case SortMode::SIZE_DESC:
            std::stable_sort(memoryItems.begin(), memoryItems.end(), [](const MemoryItem& a, const MemoryItem& b) {
                return a.size > b.size;
            });
            break;
        case SortMode::NAME_ASC:
            std::stable_sort(memoryItems.begin(), memoryItems.end(), [](const MemoryItem& a, const MemoryItem& b) {
                return a.name < b.name;
            });
            break;
        case SortMode::POOL:
            std::stable_sort(memoryItems.begin(), memoryItems.end(), [](const MemoryItem& a, const MemoryItem& b) {
                if (a.isVram != b.isVram) return a.isVram > b.isVram; // VRAM first
                return a.size > b.size;
            });
            break;
        case SortMode::DEFAULT:
        default:
            break;
    }

    rebuildFormattedCache();
    if (memorySelection >= (int)memoryItems.size()) {
        memorySelection = memoryItems.empty() ? 0 : (int)memoryItems.size() - 1;
    }
}

void MemoryDebugState::rebuildFormattedCache() {
    formattedItems.clear();
    formattedItems.reserve(memoryItems.size());

    for (const auto& item : memoryItems) {
        FormattedItem fi;
        std::string nameStr = item.name;
        size_t pos = nameStr.find_last_of("/\\");
        if (pos != std::string::npos) nameStr = nameStr.substr(pos + 1);
        if (nameStr.length() > 20) nameStr = nameStr.substr(0, 18) + "..";
        fi.displayName = nameStr;
        fi.sizeStr = formatBytes(item.size);
        fi.poolStr = item.isVram ? "GFX" : "LIN";
        formattedItems.push_back(fi);
    }
}

void MemoryDebugState::update(float dt) {
    u32 kDown = hidKeysDown();

    memoryScrollLerp += (memorySelection - memoryScrollLerp) * (dt * 18.0f);

    int count = (int)memoryItems.size();
    if (count > 0) {
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            memorySelection = (memorySelection > 0) ? (memorySelection - 1) : (count - 1);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            memorySelection = (memorySelection + 1 < count) ? (memorySelection + 1) : 0;
        }

        // Fast Page Jump: 6 items
        if (kDown & (KEY_DLEFT | KEY_L)) {
            memorySelection = std::max(0, memorySelection - 6);
        }
        if (kDown & (KEY_DRIGHT | KEY_R)) {
            memorySelection = std::min(count - 1, memorySelection + 6);
        }
    }

    if (kDown & KEY_X) {
        gatherAllMemoryItems();
    }

    if (kDown & KEY_Y) {
        int nextSort = ((int)sortMode + 1) % 4;
        sortMode = (SortMode)nextSort;
        applySort();
    }

    if (kDown & (KEY_B | KEY_A | KEY_START)) {
        active = false;
    }
}

void MemoryDebugState::draw() {
    if (g_isRightEye || !PlayState::instance || !PlayState::instance->bottom) return;

    C2D_SceneBegin(PlayState::instance->bottom);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

    C2D_TargetClear(PlayState::instance->bottom, C2D_Color32(0, 0, 0, 255));
    if (!dynamicTextBuf) {
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
        return;
    }

    C2D_TextBufClear(dynamicTextBuf);

    int count = (int)memoryItems.size();
    float lineH = 14.0f;
    float startListY = 58.0f;

    for (int i = 0; i < count; i++) {
        float y = startListY + ((float)i - memoryScrollLerp) * lineH;
        if (y < 42.0f || y > 220.0f) continue;

        bool isSelected = (i == memorySelection);
        const auto& fi = formattedItems[i];
        std::string rowName = (isSelected ? "> " : "  ") + fi.displayName;

        u32 color = isSelected ? CWhite : C2D_Color32(160, 160, 160, 255);
        if (memoryItems[i].size > 2 * 1024 * 1024) {
            color = isSelected ? C2D_Color32(255, 100, 100, 255) : C2D_Color32(210, 80, 80, 255);
        }

        C2D_Text nameObj, sizeObj, poolObj;
        C2D_TextFontParse(&nameObj, nullptr, dynamicTextBuf, rowName.c_str());
        C2D_TextFontParse(&sizeObj, nullptr, dynamicTextBuf, fi.sizeStr.c_str());
        C2D_TextFontParse(&poolObj, nullptr, dynamicTextBuf, fi.poolStr.c_str());
        C2D_TextOptimize(&nameObj);
        C2D_TextOptimize(&sizeObj);
        C2D_TextOptimize(&poolObj);

        float scale = 0.38f;
        C2D_DrawText(&nameObj, C2D_WithColor, 6.0f, y, 0.50f, scale, scale, color);
        C2D_DrawText(&sizeObj, C2D_WithColor, 200.0f, y, 0.50f, scale, scale, color);
        C2D_DrawText(&poolObj, C2D_WithColor, 270.0f, y, 0.50f, scale, scale, color);
    }

    C2D_DrawRectSolid(0.0f, 0.0f, 0.60f, 320.0f, 57.0f, C2D_Color32(0, 0, 0, 255));

    C2D_DrawRectSolid(0.0f, 217.0f, 0.60f, 320.0f, 23.0f, C2D_Color32(0, 0, 0, 255));

    C2D_Text titleText;
    C2D_TextFontParse(&titleText, nullptr, dynamicTextBuf, "=== MEMORY DEBUG ===");
    C2D_TextOptimize(&titleText);
    C2D_DrawText(&titleText, C2D_WithColor, 6.0f, 3.0f, 0.70f, 0.42f, 0.42f, CWhite);

    // Hardware telemetry
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
    float vramUsed     = (float)(6 * 1024 * 1024 - vramSpaceFree()) / (1024.0f * 1024.0f);
    float vramTotal    = 6.0f;

    float linRatio  = (linearTotal > 0.001f) ? (linearUsed / linearTotal) : 0.0f;
    float vramRatio = (vramTotal > 0.001f)   ? (vramUsed / vramTotal)     : 0.0f;

    float cpuPercent = C3D_GetProcessingTime() * 6.0f;
    if (cpuPercent > 100.0f) cpuPercent = 100.0f;
    if (cpuPercent < 0.0f)   cpuPercent = 0.0f;
    int curFps = (int)PlayState::instance->curFPS;

    // Line 1: RAM & VRAM with ASCII bars
    char line1Buf[128];
    snprintf(line1Buf, sizeof(line1Buf), "RAM %s %.1f/%.1fM  GFX %s %.1f/6.0M",
             makeAsciiBar(linRatio, 6).c_str(), linearUsed, linearTotal,
             makeAsciiBar(vramRatio, 6).c_str(), vramUsed);
    C2D_Text line1Text;
    C2D_TextFontParse(&line1Text, nullptr, dynamicTextBuf, line1Buf);
    C2D_TextOptimize(&line1Text);
    C2D_DrawText(&line1Text, C2D_WithColor, 6.0f, 16.0f, 0.70f, 0.36f, 0.36f, CWhite);

    // Line 2: FPS, CPU, Total
    char line2Buf[128];
    snprintf(line2Buf, sizeof(line2Buf), "FPS:%d CPU:%d%% HEAP:%.1fM TOT:%.1f/%.1fM",
             curFps, (int)cpuPercent, heapUsed, totalUsed, totalAvail);
    C2D_Text line2Text;
    C2D_TextFontParse(&line2Text, nullptr, dynamicTextBuf, line2Buf);
    C2D_TextOptimize(&line2Text);
    C2D_DrawText(&line2Text, C2D_WithColor, 6.0f, 27.0f, 0.70f, 0.36f, 0.36f, C2D_Color32(180, 180, 180, 255));

    // Top Divider Line
    C2D_DrawRectSolid(4.0f, 40.0f, 0.70f, 312.0f, 1.0f, C2D_Color32(100, 100, 100, 255));

    // Column Headers
    const char* sortLabel = (sortMode == SortMode::SIZE_DESC) ? "Size" :
                            (sortMode == SortMode::NAME_ASC)   ? "Name" :
                            (sortMode == SortMode::POOL)       ? "Pool" : "Def";

    char colBuf[64];
    snprintf(colBuf, sizeof(colBuf), "OBJECT (%s)", sortLabel);
    C2D_Text colName, colSize, colPool;
    C2D_TextFontParse(&colName, nullptr, dynamicTextBuf, colBuf);
    C2D_TextFontParse(&colSize, nullptr, dynamicTextBuf, "SIZE");
    C2D_TextFontParse(&colPool, nullptr, dynamicTextBuf, "POOL");
    C2D_TextOptimize(&colName);
    C2D_TextOptimize(&colSize);
    C2D_TextOptimize(&colPool);

    C2D_DrawText(&colName, C2D_WithColor, 6.0f, 43.0f, 0.70f, 0.36f, 0.36f, C2D_Color32(220, 220, 220, 255));
    C2D_DrawText(&colSize, C2D_WithColor, 200.0f, 43.0f, 0.70f, 0.36f, 0.36f, C2D_Color32(220, 220, 220, 255));
    C2D_DrawText(&colPool, C2D_WithColor, 270.0f, 43.0f, 0.70f, 0.36f, 0.36f, C2D_Color32(220, 220, 220, 255));

    // Footer Section
    C2D_DrawRectSolid(4.0f, 217.0f, 0.70f, 312.0f, 1.0f, C2D_Color32(100, 100, 100, 255));

    // Footer Controls & Progress
    char footerBuf[128];
    snprintf(footerBuf, sizeof(footerBuf), "[B] Exit  [X] Reload  [Y] Sort  (%d/%d)", 
             count > 0 ? (memorySelection + 1) : 0, count);
    C2D_Text footerText;
    C2D_TextFontParse(&footerText, nullptr, dynamicTextBuf, footerBuf);
    C2D_TextOptimize(&footerText);
    C2D_DrawText(&footerText, C2D_WithColor, 6.0f, 221.0f, 0.70f, 0.36f, 0.36f, CWhite);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
}
