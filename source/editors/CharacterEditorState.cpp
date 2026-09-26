#include "CharacterEditorState.hpp"
#include "../backend/Paths.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/ModHandler.hpp"
#include "../shaders/ShaderManager.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
#include "../debug/DebugMenuState.hpp"
#include <citro2d.h>
#include <dirent.h>
#include <algorithm>
#include <cstdio>

static std::string getIndicesString(const std::vector<int>& indices, bool pcMode) {
    if (indices.empty()) return "NONE";
    std::string s = "[";
    const size_t maxLen = pcMode ? 10 : 26;
    for (size_t i = 0; i < indices.size(); i++) {
        s += std::to_string(indices[i]);
        if (i < indices.size() - 1) {
            s += ",";
            if (s.length() > maxLen) {
                s += "...";
                break;
            }
        }
    }
    s += "]";
    return s;
}

static std::string fmtFloat(float val, int decimals = 2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, val);
    return buf;
}

static std::string resolveModFolderForCharacter(const std::string& name) {
    std::string customPath = "sdmc:/SnakeEngine/characters/" + name + ".json";
    if (Paths::fileExists(customPath)) return "";

    for (const auto& mod : ModHandler::get().getMods()) {
        if (!mod.active) continue;
        std::string path = ModHandler::getWorkingBase() + mod.folder + "/characters/" + name + ".json";
        if (Paths::fileExists(path)) return mod.folder;
    }
    return "";
}

static inline void setScissorBox(float x, float y, float w, float h, float screenW = 320.0f) {
    C2D_Flush();
    float sLeft = y;
    float sRight = y + h;
    float sTop = x;
    float sBottom = x + w;

    if (sLeft < 0.0f) sLeft = 0.0f;
    if (sRight > 240.0f) sRight = 240.0f;
    if (sTop < 0.0f) sTop = 0.0f;
    if (sBottom > screenW) sBottom = screenW;

    u32 phys_left = (u32)(240.0f - std::min(240.0f, std::max(0.0f, sRight)));
    u32 phys_right = (u32)(240.0f - std::min(240.0f, std::max(0.0f, sLeft)));
    u32 phys_top = (u32)(screenW - std::min(screenW, std::max(0.0f, sBottom)));
    u32 phys_bottom = (u32)(screenW - std::min(screenW, std::max(0.0f, sTop)));

    if (phys_left < phys_right && phys_top < phys_bottom) {
        C3D_SetScissor(GPU_SCISSOR_NORMAL, phys_left, phys_top, phys_right, phys_bottom);
    }
}

static inline void disableScissor() {
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}


struct UIElement {
    int id;              // -1 for accordion headers, 0..29 for controls
    int headerIndex;
    float x, y, w, h;
    std::string label;
};

static std::vector<UIElement> getPCLayout(const std::vector<CharacterEditorState::UIWindow>& windows, float scale = 1.0f) {
    std::vector<UIElement> elements;
    
    for (int winIdx = 0; winIdx < 6; winIdx++) {
        const CharacterEditorState::UIWindow* winPtr = nullptr;
        for (const auto& w : windows) {
            if (w.id == winIdx) {
                winPtr = &w;
                break;
            }
        }
        if (!winPtr) continue;
        const auto& win = *winPtr;
        if (!win.expanded) continue;

        float titleH = 12.0f * scale;
        elements.push_back({ -1, win.id, win.x, win.y, win.w, titleH, win.title });
        
        float contentY = win.y + titleH;
        float contentH = win.h - titleH;
        float pad = 4.0f * scale;
        float itemH = 10.0f * scale;

        if (win.id == 0) { // Settings (Char list)
            float listH = std::max(20.0f * scale, contentH - 46.0f * scale);
            listH = std::max(20.0f * scale, std::floor(listH / itemH) * itemH);
            
            // Character Listbox (id = 0)
            elements.push_back({ 0, -1, win.x + pad, contentY + pad, win.w - pad * 2, listH, "CharList" });
            // Playable Checkbox (id = 1)
            elements.push_back({ 1, -1, win.x + pad, contentY + pad * 2 + listH, win.w - pad * 2, itemH, "Playable" });
            // Reload Button (id = 2)
            elements.push_back({ 2, -1, win.x + pad, contentY + pad * 2 + listH + itemH + 2.0f * scale, (win.w - pad * 3) * 0.5f, 14.0f * scale, "Reload" });
            // Save Button (id = 3)
            elements.push_back({ 3, -1, win.x + win.w * 0.5f + pad * 0.5f, contentY + pad * 2 + listH + itemH + 2.0f * scale, (win.w - pad * 3) * 0.5f, 14.0f * scale, "Save" });
        }
        else if (win.id == 1) { // Ghost
            elements.push_back({ 4, -1, win.x + pad, contentY + pad, win.w - pad * 2, 14.0f * scale, "Make Ghost" });
            elements.push_back({ 5, -1, win.x + pad, contentY + pad * 2 + 14.0f * scale, win.w - pad * 2, itemH, "Show Ghost" });
            elements.push_back({ 6, -1, win.x + pad, contentY + pad * 2 + 14.0f * scale + itemH + 3.0f * scale, win.w - pad * 2, itemH, "Highlight" });
            elements.push_back({ 7, -1, win.x + pad, contentY + pad * 2 + 14.0f * scale + (itemH + 3.0f * scale) * 2, win.w - pad * 2, 16.0f * scale, "Alpha" });
        }
        else if (win.id == 2) { // Character Settings
            std::string charLabels[] = {
                "Sing Length", "Scale", "Flip X", "Antialiasing",
                "Pos X", "Pos Y", "Cam Off X", "Cam Off Y"
            };
            float step = (contentH - pad * 2) / 8.0f;
            if (step < itemH) step = itemH;
            for (int i = 0; i < 8; i++) {
                elements.push_back({ 8 + i, -1, win.x + pad, contentY + pad + i * step, win.w - pad * 2, itemH, charLabels[i] });
            }
        }
        else if (win.id == 3) { // Animations
            float listH = std::max(20.0f * scale, contentH - 64.0f * scale);
            listH = std::max(20.0f * scale, std::floor(listH / itemH) * itemH);

            elements.push_back({ 16, -1, win.x + pad, contentY + pad, win.w - pad * 2, listH, "AnimList" });
            elements.push_back({ 17, -1, win.x + pad, contentY + pad * 2 + listH, win.w - pad * 2, itemH, "FPS" });
            elements.push_back({ 18, -1, win.x + pad, contentY + pad * 2 + listH + itemH + 2.0f * scale, win.w - pad * 2, itemH, "Loop" });
            elements.push_back({ 19, -1, win.x + pad, contentY + pad * 2 + listH + (itemH + 2.0f * scale) * 2, win.w - pad * 2, itemH, "Offset X" });
            elements.push_back({ 20, -1, win.x + pad, contentY + pad * 2 + listH + (itemH + 2.0f * scale) * 3, win.w - pad * 2, itemH, "Offset Y" });
        }
        else if (win.id == 4) { // Camera
            elements.push_back({ 21, -1, win.x + pad, contentY + pad, win.w - pad * 2, itemH, "Enable" });
            elements.push_back({ 22, -1, win.x + pad, contentY + pad * 2 + itemH, win.w - pad * 2, itemH, "Zoom" });
        }
        else if (win.id == 5) { // Reference
            float listH = 38.0f * scale;
            elements.push_back({ 23, -1, win.x + pad, contentY + pad, win.w - pad * 2, listH, "RefCharList" });
            elements.push_back({ 29, -1, win.x + pad, contentY + pad * 2 + listH, win.w - pad * 2, listH, "RefAnimList" });
            
            float nextY = contentY + pad * 3 + listH * 2;
            elements.push_back({ 24, -1, win.x + pad, nextY, win.w - pad * 2, itemH, "Show Ref" });
            elements.push_back({ 26, -1, win.x + pad, nextY + itemH + 2.0f * scale, win.w - pad * 2, itemH, "Ref Alpha" });
            elements.push_back({ 27, -1, win.x + pad, nextY + (itemH + 2.0f * scale) * 2, win.w - pad * 2, itemH, "Ref Pos X" });
            elements.push_back({ 28, -1, win.x + pad, nextY + (itemH + 2.0f * scale) * 3, win.w - pad * 2, itemH, "Ref Pos Y" });
        }
    }
    return elements;
}

#include "../backend/ModHandler.hpp"

void CharacterEditorState::drawText(const std::string& text, float x, float y, float scale, bool centered, u32 color, float depth, float maxWidth, bool rightAlign, bool centerY) {
    if (!vcrFont || !textBuf || text.empty()) return;
    C2D_Text gText;
    C2D_TextFontParse(&gText, vcrFont, textBuf, text.c_str());
    C2D_TextOptimize(&gText);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
    float actualScale = scale;
    if (maxWidth > 0.0f && tw > maxWidth) {
        actualScale *= (maxWidth / tw);
        C2D_TextGetDimensions(&gText, actualScale, actualScale, &tw, &th);
    }
    float dx = x;
    if (rightAlign) {
        dx = x - tw;
    } else if (centered) {
        dx = x - tw * 0.5f;
    }
    float dy = (centerY || centered) ? (y - th * 0.5f) : y;
    dx = std::round(dx);
    dy = std::round(dy);
    C2D_DrawText(&gText, C2D_WithColor, dx, dy, depth, actualScale, actualScale, color);
}

void CharacterEditorState::drawStyledButton(float x, float y, float w, float h, const std::string& title, const std::string& subtitle, u32 topCol, u32 botCol, u32 borderCol) {
    C2D_DrawRectangle(x, y, 0.5f, w, h, topCol, topCol, botCol, botCol);
    C2D_DrawLine(x, y, borderCol, x + w, y, borderCol, 1.0f, 0.52f);
    C2D_DrawLine(x, y, borderCol, x, y + h, borderCol, 1.0f, 0.52f);
    C2D_DrawLine(x + w, y, borderCol, x + w, y + h, borderCol, 1.0f, 0.52f);
    C2D_DrawLine(x, y + h, borderCol, x + w, y + h, borderCol, 1.0f, 0.52f);

    if (subtitle.empty()) {
        drawText(title, x + w * 0.5f, y + h * 0.5f, 0.36f, true, CWhite, 0.85f, w - 8.0f);
    } else {
        drawText(title, x + w * 0.5f, y + h * 0.33f, 0.34f, true, CWhite, 0.85f, w - 8.0f);
        drawText(subtitle, x + w * 0.5f, y + h * 0.72f, 0.28f, true, C2D_Color32(0, 210, 255, 255), 0.85f, w - 8.0f);
    }
}

void CharacterEditorState::renderCheckbox(float x, float y, bool checked, bool selected, float depth) {
    u32 boxBg = selected ? C2D_Color32(0, 110, 200, 255) : C2D_Color32(30, 32, 42, 255);
    u32 boxBorder = selected ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(80, 80, 100, 255);
    float boxW = 12.0f * (pcMode ? pcUiScale : 1.0f);
    float boxH = 12.0f * (pcMode ? pcUiScale : 1.0f);

    C2D_DrawRectSolid(x, y, depth, boxW, boxH, boxBg);
    C2D_DrawLine(x, y, boxBorder, x + boxW, y, boxBorder, 1.0f, depth + 0.001f);
    C2D_DrawLine(x, y, boxBorder, x, y + boxH, boxBorder, 1.0f, depth + 0.001f);
    C2D_DrawLine(x + boxW, y, boxBorder, x + boxW, y + boxH, boxBorder, 1.0f, depth + 0.001f);
    C2D_DrawLine(x, y + boxH, boxBorder, x + boxW, y + boxH, boxBorder, 1.0f, depth + 0.001f);

    if (checked) {
        float innerPad = 2.5f * (pcMode ? pcUiScale : 1.0f);
        u32 checkColor = C2D_Color32(0, 230, 150, 255);
        C2D_DrawRectSolid(x + innerPad, y + innerPad, depth + 0.002f, boxW - innerPad * 2.0f, boxH - innerPad * 2.0f, checkColor);
    }
}

void CharacterEditorState::renderSlider(float x, float y, float w, float valPct, bool selected, float depth) {
    float barH = 4.0f * (pcMode ? pcUiScale : 1.0f);
    u32 bgCol = C2D_Color32(25, 27, 36, 255);
    u32 fillCol = selected ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(0, 140, 210, 255);

    C2D_DrawRectSolid(x, y, depth, w, barH, bgCol);
    if (valPct > 0.0f) {
        C2D_DrawRectSolid(x, y, depth + 0.001f, w * std::min(1.0f, std::max(0.0f, valPct)), barH, fillCol);
    }

    float handleW = 6.0f * (pcMode ? pcUiScale : 1.0f);
    float handleH = 10.0f * (pcMode ? pcUiScale : 1.0f);
    float handleX = x + (w - handleW) * std::min(1.0f, std::max(0.0f, valPct));
    float handleY = y + (barH - handleH) * 0.5f;

    u32 handleCol = selected ? CWhite : C2D_Color32(200, 200, 210, 255);
    C2D_DrawRectSolid(handleX, handleY, depth + 0.002f, handleW, handleH, handleCol);
}

void CharacterEditorState::renderButton(float x, float y, float w, float h, const std::string& label, bool selected, float depth) {
    u32 topCol = selected ? C2D_Color32(0, 110, 200, 255) : C2D_Color32(35, 38, 50, 255);
    u32 botCol = selected ? C2D_Color32(0, 75, 150, 255) : C2D_Color32(22, 24, 32, 255);
    u32 borderCol = selected ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(70, 72, 88, 255);

    C2D_DrawRectangle(x, y, depth, w, h, topCol, topCol, botCol, botCol);
    C2D_DrawLine(x, y, borderCol, x + w, y, borderCol, 1.0f, depth + 0.001f);
    C2D_DrawLine(x, y, borderCol, x, y + h, borderCol, 1.0f, depth + 0.001f);
    C2D_DrawLine(x + w, y, borderCol, x + w, y + h, borderCol, 1.0f, depth + 0.001f);
    C2D_DrawLine(x, y + h, borderCol, x + w, y + h, borderCol, 1.0f, depth + 0.001f);

    float ts = pcMode ? 0.26f * pcUiScale : 0.35f;
    drawText(label, x + w * 0.5f, y + h * 0.5f, ts, true, CWhite, depth + 0.002f, w - 4.0f, false, true);
}

void CharacterEditorState::renderStepper(float itemX, float itemY, float itemW, const std::string& label, const std::string& valStr, bool selected, float depth) {
    float h = pcMode ? (10.0f * pcUiScale) : 20.0f;
    if (selected) renderLeftAccent(itemX, itemY, h, depth);

    float ts = pcMode ? (0.24f * pcUiScale) : 0.32f;
    float padLeft = pcMode ? (4.0f * pcUiScale) : 10.0f;
    drawText(label, itemX + padLeft, itemY + h * 0.5f, ts, false, CWhite, depth + 0.001f, itemW * 0.45f, false, true);

    float btnW = pcMode ? (12.0f * pcUiScale) : 20.0f;
    float btnH = pcMode ? (8.0f * pcUiScale) : 16.0f;
    float btnY = itemY + (h - btnH) * 0.5f;

    float minusX = itemX + itemW - (pcMode ? (38.0f * pcUiScale) : 75.0f);
    float plusX = itemX + itemW - (pcMode ? (14.0f * pcUiScale) : 22.0f);
    float midX = (minusX + btnW + plusX) * 0.5f;

    u32 btnBg = selected ? C2D_Color32(0, 110, 200, 255) : C2D_Color32(32, 35, 48, 255);
    u32 btnBorder = selected ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(75, 80, 100, 255);

    // Minus Button [-]
    C2D_DrawRectSolid(minusX, btnY, depth + 0.001f, btnW, btnH, btnBg);
    C2D_DrawLine(minusX, btnY, btnBorder, minusX + btnW, btnY, btnBorder, 1.0f, depth + 0.002f);
    C2D_DrawLine(minusX, btnY, btnBorder, minusX, btnY + btnH, btnBorder, 1.0f, depth + 0.002f);
    C2D_DrawLine(minusX + btnW, btnY, btnBorder, minusX + btnW, btnY + btnH, btnBorder, 1.0f, depth + 0.002f);
    C2D_DrawLine(minusX, btnY + btnH, btnBorder, minusX + btnW, btnY + btnH, btnBorder, 1.0f, depth + 0.002f);
    drawText("-", minusX + btnW * 0.5f, btnY + btnH * 0.5f, ts, true, CWhite, depth + 0.003f, 0.0f, false, true);

    // Value text in middle
    drawText(valStr, midX, itemY + h * 0.5f, ts, true, selected ? C2D_Color32(0, 230, 255, 255) : CWhite, depth + 0.002f, std::max(5.0f, plusX - (minusX + btnW) - 2.0f), false, true);

    // Plus Button [+]
    C2D_DrawRectSolid(plusX, btnY, depth + 0.001f, btnW, btnH, btnBg);
    C2D_DrawLine(plusX, btnY, btnBorder, plusX + btnW, btnY, btnBorder, 1.0f, depth + 0.002f);
    C2D_DrawLine(plusX, btnY, btnBorder, plusX, btnY + btnH, btnBorder, 1.0f, depth + 0.002f);
    C2D_DrawLine(plusX + btnW, btnY, btnBorder, plusX + btnW, btnY + btnH, btnBorder, 1.0f, depth + 0.002f);
    C2D_DrawLine(plusX, btnY + btnH, btnBorder, plusX + btnW, btnY + btnH, btnBorder, 1.0f, depth + 0.002f);
    drawText("+", plusX + btnW * 0.5f, btnY + btnH * 0.5f, ts, true, CWhite, depth + 0.003f, 0.0f, false, true);
}

void CharacterEditorState::renderArrowsVal(float itemX, float itemY, float itemW, const std::string& label, const std::string& valStr, bool selected, float depth) {
    renderStepper(itemX, itemY, itemW, label, valStr, selected, depth);
}

void CharacterEditorState::renderListBox(float bx, float by, float bw, float bh, const std::vector<std::string>& items, int selectedIdx, float scrollY, bool activeSelected, float depth) {
    C2D_DrawRectSolid(bx, by, depth, bw, bh, C2D_Color32(18, 20, 28, 240));
    u32 borderCol = activeSelected ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(60, 62, 75, 255);
    C2D_DrawLine(bx, by, borderCol, bx + bw, by, borderCol, 1.0f, depth + 0.001f);
    C2D_DrawLine(bx, by, borderCol, bx, by + bh, borderCol, 1.0f, depth + 0.001f);
    C2D_DrawLine(bx + bw, by, borderCol, bx + bw, by + bh, borderCol, 1.0f, depth + 0.001f);
    C2D_DrawLine(bx, by + bh, borderCol, bx + bw, by + bh, borderCol, 1.0f, depth + 0.001f);

    float itemH = pcMode ? 10.0f * pcUiScale : 18.0f;
    float ts = pcMode ? 0.24f * pcUiScale : 0.32f;

    setScissorBox(bx + 1.0f, by + 1.0f, bw - 2.0f, bh - 2.0f);

    for (size_t i = 0; i < items.size(); i++) {
        float itemY = by + i * itemH - scrollY;
        if (itemY + itemH < by || itemY > by + bh) continue;

        bool isCur = ((int)i == selectedIdx);
        if (isCur) {
            u32 itemBg = activeSelected ? C2D_Color32(0, 110, 200, 255) : C2D_Color32(40, 44, 60, 255);
            C2D_DrawRectSolid(bx + 1.0f, itemY, depth + 0.002f, bw - 2.0f, itemH, itemBg);
        }

        std::string txt = items[i];
        float maxW = bw - 14.0f * (pcMode ? pcUiScale : 1.0f);
        drawText(txt, bx + 6.0f * (pcMode ? pcUiScale : 1.0f), itemY + itemH * 0.5f, ts, false, isCur ? CWhite : C2D_Color32(180, 185, 200, 255), depth + 0.003f, maxW, false, true);
    }

    disableScissor();
}

void CharacterEditorState::renderLeftAccent(float itemX, float itemY, float h, float depth) {
    float barW = pcMode ? 2.0f * pcUiScale : 3.0f;
    C2D_DrawRectSolid(itemX, itemY, depth + 0.005f, barW, h, C2D_Color32(0, 210, 255, 255));
}


CharacterEditorState::CharacterEditorState() {
    auto scanDir = [this](const std::string& path) {
        DIR* dir = opendir(Paths::resolve(path).c_str());
        if (!dir) return;
        struct dirent* dp;
        while ((dp = readdir(dir)) != nullptr) {
            std::string file = dp->d_name;
            if (file.find(".json") != std::string::npos) {
                std::string charName = file.substr(0, file.find_last_of('.'));
                if (std::find(characterList.begin(), characterList.end(), charName) == characterList.end()) {
                    characterList.push_back(charName);
                }
            }
        }
        closedir(dir);
    };

    scanDir("romfs:/preload/characters");
    scanDir("romfs:/shared/characters");
    scanDir("sdmc:/SnakeEngine/characters");

    for (const auto& mod : ModHandler::get().getMods()) {
        if (mod.active) {
            scanDir(ModHandler::getWorkingBase() + mod.folder + "/characters");
        }
    }

    if (characterList.empty()) characterList.push_back("bf");

    windows = {
        { "Settings", 4.0f, 15.0f, 152.0f, 100.0f, true, 0 },
        { "Ghost", 4.0f, 118.0f, 152.0f, 80.0f, false, 1 },
        { "Character", 160.0f, 15.0f, 156.0f, 124.0f, true, 2 },
        { "Animations", 160.0f, 121.0f, 156.0f, 114.0f, false, 3 },
        { "Camera", 160.0f, 60.0f, 156.0f, 55.0f, false, 4 },
        { "Reference", 4.0f, 15.0f, 152.0f, 175.0f, false, 5 }
    };
}

void CharacterEditorState::loadRefCharacter(const std::string& name) {
    if (refObj) {
        delete refObj;
        refObj = nullptr;
    }

    std::string oldModFolder = ModHandler::get().currentModFolder;
    ModHandler::get().currentModFolder = resolveModFolderForCharacter(name);

    refObj = new Character();
    refObj->loadFromPsychJson(Paths::characterJson(name));
    refObj->alpha = refAlpha;
    refObj->dance();

    ModHandler::get().currentModFolder = oldModFolder;

    auto it = std::find(characterList.begin(), characterList.end(), name);
    if (it != characterList.end()) {
        curRefCharIndex = std::distance(characterList.begin(), it);
    }

    refCharScrollY = 0.0f;
    updateRefAnimList();
}

void CharacterEditorState::updateRefAnimList() {
    refAnimList.clear();
    if (refObj) {
        for (const auto& kv : refObj->animations) {
            refAnimList.push_back(kv.first);
        }
    }
    curRefAnimIndex = 0;
    refAnimSliderIndex = 0;
    refAnimScrollY = 0.0f;
    playCurRefAnim();
}

void CharacterEditorState::playCurRefAnim() {
    if (refObj && !refAnimList.empty() && curRefAnimIndex < (int)refAnimList.size()) {
        refObj->playAnim(refAnimList[curRefAnimIndex], true);
    }
}

void CharacterEditorState::loadCharacter(const std::string& name) {
    if (charObj) delete charObj;
    if (ghostObj) delete ghostObj;

    currentCharacter = name;
    ModHandler::get().currentModFolder = resolveModFolderForCharacter(name);
    
    charObj = new Character();
    charObj->loadFromPsychJson(Paths::characterJson(name));
    charObj->isPlayer = true; // Player character faces right
    charObj->dance();

    ghostObj = new Character();
    ghostObj->loadFromPsychJson(Paths::characterJson(name));
    ghostObj->isPlayer = charObj->isPlayer;
    ghostObj->alpha = ghostAlpha;
    ghostObj->isHighlighted = ghostHighlight;
    if (charObj) {
        ghostObj->x = charObj->x;
        ghostObj->y = charObj->y;
        ghostObj->baseX = ghostObj->x;
        ghostObj->baseY = ghostObj->y;
    }
    ghostObj->animFinished = true; // freeze ghost initially
    ghostObj->dance();
    ghostObj->animFinished = true; // freeze ghost initially

    // Find and sync character index
    auto it = std::find(characterList.begin(), characterList.end(), name);
    if (it != characterList.end()) {
        curCharIndex = std::distance(characterList.begin(), it);
    }

    charScrollY = 0.0f;
    animScrollY = 0.0f;
    animSliderIndex = 0;
    updateAnimList();
}

void CharacterEditorState::updateAnimList() {
    animList.clear();
    for (const auto& kv : charObj->animations) {
        animList.push_back(kv.first);
    }
    curAnimIndex = 0;
    animSliderIndex = 0;
    animScrollY = 0.0f;
    playCurAnim();
}

void CharacterEditorState::playCurAnim() {
    if (animList.empty()) return;
    charObj->playAnim(animList[curAnimIndex], true);
}

void CharacterEditorState::update(float dt) {
    MusicBeatState::update(dt);

    if (saveMessageTimer > 0.0f) {
        saveMessageTimer -= dt;
    }

    if (currentStage) {
        for (auto& s : currentStage->sprites) {
            s.update(dt);
        }
    }

    if (charObj) charObj->update(dt);
    if (ghostObj) ghostObj->update(dt);
    if (refObj) refObj->update(dt);

    if (keyJustPressed(KEY_X)) {
        pcMode = !pcMode;
        uiExpanded = true;
    }

    // Camera panning & zooming
    if (hidKeysHeld() & KEY_CPAD_UP) camY -= 200 * dt;
    if (hidKeysHeld() & KEY_CPAD_DOWN) camY += 200 * dt;
    if (hidKeysHeld() & KEY_CPAD_LEFT) camX -= 200 * dt;
    if (hidKeysHeld() & KEY_CPAD_RIGHT) camX += 200 * dt;
    if (hidKeysHeld() & KEY_L) camZoom += 1.0f * dt;
    if (hidKeysHeld() & KEY_R) camZoom -= 1.0f * dt;
    if (camZoom < 0.1f) camZoom = 0.1f;


    if (keyJustPressed(KEY_SELECT) || keyJustPressed(KEY_B)) {
        exitState();
        return;
    }


    // Populate visibleOptionIds for PC Mode accordion navigation
    std::vector<int> visibleOptionIds;
    if (pcMode && uiExpanded) {
        auto layout = getPCLayout(windows, pcUiScale);
        for (const auto& elem : layout) {
            if (elem.id != -1) {
                visibleOptionIds.push_back(elem.id);
            }
        }
        if (curSelected >= (int)visibleOptionIds.size()) {
            curSelected = visibleOptionIds.empty() ? 0 : (int)visibleOptionIds.size() - 1;
        }
    }

    // Calculate tab maximum options for DPAD navigation
    int maxOpts = 4;
    if (!pcMode) {
        if (currentTab == 2) maxOpts = 8;
        else if (currentTab == 3) maxOpts = 5;
        else if (currentTab == 4) maxOpts = 2;
        else if (currentTab == 5) maxOpts = 7;
    } else {
        maxOpts = visibleOptionIds.size();
    }

    if (maxOpts > 0) {
        if (keyJustPressed(KEY_DUP)) {
            curSelected--;
            if (curSelected < 0) curSelected = maxOpts - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.3f);
        }
        if (keyJustPressed(KEY_DDOWN)) {
            curSelected++;
            if (curSelected >= maxOpts) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.3f);
        }
    }

    bool pressLeft = keyJustPressed(KEY_DLEFT);
    bool pressRight = keyJustPressed(KEY_DRIGHT);
    
    // Y button held: fast adjust
    float change = 1.0f;
    if (hidKeysHeld() & KEY_Y) change = 10.0f;

    // Simulate pressRight with KEY_A on action rows
    if (keyJustPressed(KEY_A)) {
        if (!pcMode) {
            if (currentTab == 0 && (curSelected == 1 || curSelected == 2 || curSelected == 3)) {
                pressRight = true;
            } else if (currentTab == 1 && (curSelected == 0 || curSelected == 1 || curSelected == 2)) {
                pressRight = true;
            } else if (currentTab == 2 && (curSelected == 2 || curSelected == 3)) {
                pressRight = true;
            } else if (currentTab == 3 && curSelected == 2) {
                pressRight = true;
            } else if (currentTab == 4 && curSelected == 0) {
                pressRight = true;
            } else if (currentTab == 5 && (curSelected == 1 || curSelected == 2)) {
                pressRight = true;
            }
        } else if (!visibleOptionIds.empty()) {
            int optId = visibleOptionIds[curSelected];
            if (optId == 1 || optId == 2 || optId == 3 || optId == 4 || optId == 5 || optId == 6 || optId == 10 || optId == 11 || optId == 18 || optId == 21 || optId == 24) {
                pressRight = true;
            }
        }
    }

    // Process Dpad / Button Changes
    if (pressLeft || pressRight) {
        if (!pcMode) {
            if (currentTab == 0) { // Settings
                if (curSelected == 0) { // Character list selection via D-pad
                    if (pressLeft) {
                        curCharIndex--;
                        if (curCharIndex < 0) curCharIndex = characterList.size() - 1;
                    } else {
                        curCharIndex++;
                        if (curCharIndex >= (int)characterList.size()) curCharIndex = 0;
                    }
                    loadCharacter(characterList[curCharIndex]);
                } else if (curSelected == 1) { // Playable
                    charObj->isPlayer = !charObj->isPlayer;
                    ghostObj->isPlayer = charObj->isPlayer;
                    centerCameraOnTarget();
                    playCurAnim();
                } else if (curSelected == 2) { // Reload
                    loadCharacter(currentCharacter);
                } else if (curSelected == 3) { // Save
                    saveCharacter();
                }
            }
            else if (currentTab == 1) { // Ghost
                if (curSelected == 0) { // Make Ghost from Current
                    ghostObj->isPlayer = charObj->isPlayer;
                    ghostObj->flipX = charObj->flipX;
                    ghostObj->charScale = charObj->charScale;
                    ghostObj->charScaleX = charObj->charScale;
                    ghostObj->charScaleY = charObj->charScale;
                    if (ghostObj->isSpritemap) {
                        ghostObj->spritemapAnim.scaleX = charObj->charScale;
                        ghostObj->spritemapAnim.scaleY = charObj->charScale;
                    }
                    ghostObj->animations = charObj->animations;
                    ghostObj->playAnim(charObj->curAnim, true);
                    ghostObj->curFrame = charObj->curFrame;
                    ghostObj->frameTimer = charObj->frameTimer;
                    ghostObj->animFinished = charObj->animFinished;
                    ghostObj->spritemapAnim.smLogicalFrame = charObj->spritemapAnim.smLogicalFrame;
                    ghostObj->spritemapAnim.animFinished = charObj->spritemapAnim.animFinished;
                } else if (curSelected == 1) { // Show Ghost
                    showGhost = !showGhost;
                } else if (curSelected == 2) { // Highlight Ghost
                    ghostHighlight = !ghostHighlight;
                    ghostObj->isHighlighted = ghostHighlight;
                } else if (curSelected == 3) { // Ghost Alpha Slider
                    ghostAlpha += pressLeft ? -0.1f : 0.1f;
                    if (ghostAlpha < 0.0f) ghostAlpha = 0.0f;
                    if (ghostAlpha > 1.0f) ghostAlpha = 1.0f;
                    ghostObj->alpha = ghostAlpha;
                }
            }
            else if (currentTab == 2) { // Character Config
                if (curSelected == 0) { // Sing Anim Length
                    charObj->singDuration += pressLeft ? -0.5f : 0.5f;
                    if (charObj->singDuration < 0.1f) charObj->singDuration = 0.1f;
                    ghostObj->singDuration = charObj->singDuration;
                } else if (curSelected == 1) { // Scale
                    charObj->charScale += pressLeft ? -change * 0.1f : change * 0.1f;
                    if (charObj->charScale < 0.1f) charObj->charScale = 0.1f;
                    charObj->charScaleX = charObj->charScale;
                    charObj->charScaleY = charObj->charScale;
                    if (charObj->isSpritemap) {
                        charObj->spritemapAnim.scaleX = charObj->charScale;
                        charObj->spritemapAnim.scaleY = charObj->charScale;
                    }
                    ghostObj->charScale = charObj->charScale;
                    ghostObj->charScaleX = charObj->charScale;
                    ghostObj->charScaleY = charObj->charScale;
                    if (ghostObj->isSpritemap) {
                        ghostObj->spritemapAnim.scaleX = charObj->charScale;
                        ghostObj->spritemapAnim.scaleY = charObj->charScale;
                    }
                    playCurAnim();
                } else if (curSelected == 2) { // Flip X
                    charObj->flipX = !charObj->flipX;
                    ghostObj->flipX = charObj->flipX;
                    playCurAnim();
                } else if (curSelected == 3) { // Antialiasing
                    charObj->noAntialiasing = !charObj->noAntialiasing;
                    ghostObj->noAntialiasing = charObj->noAntialiasing;
                    charObj->setAntialiasing(!charObj->noAntialiasing);
                    ghostObj->setAntialiasing(!ghostObj->noAntialiasing);
                } else if (curSelected == 4) { // Pos X
                    charObj->x += pressLeft ? -change : change;
                    charObj->baseX = charObj->x;
                    ghostObj->x = charObj->x;
                    ghostObj->baseX = charObj->x;
                } else if (curSelected == 5) { // Pos Y
                    charObj->y += pressLeft ? -change : change;
                    charObj->baseY = charObj->y;
                    ghostObj->y = charObj->y;
                    ghostObj->baseY = charObj->y;
                } else if (curSelected == 6) { // Cam Offset X
                    charObj->camOffsetX += pressLeft ? -change * 5.0f : change * 5.0f;
                    ghostObj->camOffsetX = charObj->camOffsetX;
                } else if (curSelected == 7) { // Cam Offset Y
                    charObj->camOffsetY += pressLeft ? -change * 5.0f : change * 5.0f;
                    ghostObj->camOffsetY = charObj->camOffsetY;
                }
            }
            else if (currentTab == 3) { // Animations List selection via D-pad
                if (!animList.empty()) {
                    std::string curAnimName = animList[animSliderIndex];
                    if (curSelected == 0) { // Select Anim
                        if (pressLeft) {
                            animSliderIndex--;
                            if (animSliderIndex < 0) animSliderIndex = animList.size() - 1;
                        } else {
                            animSliderIndex++;
                            if (animSliderIndex >= (int)animList.size()) animSliderIndex = 0;
                        }
                        curAnimIndex = animSliderIndex;
                        playCurAnim();
                    } else if (curSelected == 1) { // FPS
                        charObj->animations[curAnimName].fps += pressLeft ? -1 : 1;
                        if (charObj->animations[curAnimName].fps < 1) charObj->animations[curAnimName].fps = 1;
                        ghostObj->animations[curAnimName].fps = charObj->animations[curAnimName].fps;
                        playCurAnim();
                    } else if (curSelected == 2) { // Loop
                        charObj->setAnimLoop(curAnimName, !charObj->animations[curAnimName].loop);
                        ghostObj->setAnimLoop(curAnimName, charObj->animations[curAnimName].loop);
                        playCurAnim();
                    } else if (curSelected == 3) { // Offset X
                        charObj->animations[curAnimName].offsetX += pressLeft ? -change : change;
                        ghostObj->animations[curAnimName].offsetX = charObj->animations[curAnimName].offsetX;
                        playCurAnim();
                    } else if (curSelected == 4) { // Offset Y
                        charObj->animations[curAnimName].offsetY += pressLeft ? -change : change;
                        ghostObj->animations[curAnimName].offsetY = charObj->animations[curAnimName].offsetY;
                        playCurAnim();
                    }
                }
            }
            else if (currentTab == 4) { // Camera
                if (curSelected == 0) { // Enable Frame
                    showCamBounds = !showCamBounds;
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                } else if (curSelected == 1) { // Cam Zoom
                    simCamZoom += pressLeft ? -0.05f : 0.05f;
                    if (simCamZoom < 0.1f) simCamZoom = 0.1f;
                    if (simCamZoom > 3.0f) simCamZoom = 3.0f;
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                }
            }
            else if (currentTab == 5) { // Reference
                if (curSelected == 0) { // Ref Char Listbox
                    curRefCharIndex += pressLeft ? -1 : 1;
                    if (curRefCharIndex < 0) curRefCharIndex = characterList.size() - 1;
                    if (curRefCharIndex >= (int)characterList.size()) curRefCharIndex = 0;
                    loadRefCharacter(characterList[curRefCharIndex]);
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                } else if (curSelected == 1) { // Ref Anim Listbox
                    if (!refAnimList.empty()) {
                        refAnimSliderIndex += pressLeft ? -1 : 1;
                        if (refAnimSliderIndex < 0) refAnimSliderIndex = refAnimList.size() - 1;
                        if (refAnimSliderIndex >= (int)refAnimList.size()) refAnimSliderIndex = 0;
                        curRefAnimIndex = refAnimSliderIndex;
                        playCurRefAnim();
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                } else if (curSelected == 2) { // Show Ref
                    showRefChar = !showRefChar;
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                } else if (curSelected == 3) { // Ref Alpha
                    refAlpha += pressLeft ? -0.1f : 0.1f;
                    if (refAlpha < 0.0f) refAlpha = 0.0f;
                    if (refAlpha > 1.0f) refAlpha = 1.0f;
                    if (refObj) refObj->alpha = refAlpha;
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                } else if (curSelected == 4) { // Ref Pos X
                    if (refObj) {
                        refObj->x += pressLeft ? -change : change;
                        refObj->baseX = refObj->x;
                    }
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                } else if (curSelected == 5) { // Ref Pos Y
                    if (refObj) {
                        refObj->y += pressLeft ? -change : change;
                        refObj->baseY = refObj->y;
                    }
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                }
            }
        } else if (!visibleOptionIds.empty()) {
            // PC Mode Accordion-based value adjustments
            int optId = visibleOptionIds[curSelected];
            if (optId == 0) { // Character list
                if (pressLeft) {
                    curCharIndex--;
                    if (curCharIndex < 0) curCharIndex = characterList.size() - 1;
                } else {
                    curCharIndex++;
                    if (curCharIndex >= (int)characterList.size()) curCharIndex = 0;
                }
                loadCharacter(characterList[curCharIndex]);
            } else if (optId == 1) { // Playable
                charObj->isPlayer = !charObj->isPlayer;
                ghostObj->isPlayer = charObj->isPlayer;
                centerCameraOnTarget();
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 2) { // Reload
                loadCharacter(currentCharacter);
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
            } else if (optId == 3) { // Save
                saveCharacter();
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
            } else if (optId == 4) { // Make Ghost
                ghostObj->isPlayer = charObj->isPlayer;
                ghostObj->flipX = charObj->flipX;
                ghostObj->charScale = charObj->charScale;
                ghostObj->charScaleX = charObj->charScale;
                ghostObj->charScaleY = charObj->charScale;
                if (ghostObj->isSpritemap) {
                    ghostObj->spritemapAnim.scaleX = charObj->charScale;
                    ghostObj->spritemapAnim.scaleY = charObj->charScale;
                }
                ghostObj->animations = charObj->animations;
                ghostObj->playAnim(charObj->curAnim, true);
                ghostObj->curFrame = charObj->curFrame;
                ghostObj->frameTimer = charObj->frameTimer;
                ghostObj->animFinished = charObj->animFinished;
                ghostObj->spritemapAnim.smLogicalFrame = charObj->spritemapAnim.smLogicalFrame;
                ghostObj->spritemapAnim.animFinished = charObj->spritemapAnim.animFinished;
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
            } else if (optId == 5) { // Show Ghost
                showGhost = !showGhost;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 6) { // Highlight Ghost
                ghostHighlight = !ghostHighlight;
                ghostObj->isHighlighted = ghostHighlight;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 7) { // Ghost Alpha Slider
                ghostAlpha += pressLeft ? -0.05f : 0.05f;
                if (ghostAlpha < 0.0f) ghostAlpha = 0.0f;
                if (ghostAlpha > 1.0f) ghostAlpha = 1.0f;
                ghostObj->alpha = ghostAlpha;
            } else if (optId == 8) { // Sing Anim Length
                charObj->singDuration += pressLeft ? -0.5f : 0.5f;
                if (charObj->singDuration < 0.1f) charObj->singDuration = 0.1f;
                ghostObj->singDuration = charObj->singDuration;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 9) { // Scale
                charObj->charScale += pressLeft ? -change * 0.05f : change * 0.05f;
                if (charObj->charScale < 0.1f) charObj->charScale = 0.1f;
                charObj->charScaleX = charObj->charScale;
                charObj->charScaleY = charObj->charScale;
                if (charObj->isSpritemap) {
                    charObj->spritemapAnim.scaleX = charObj->charScale;
                    charObj->spritemapAnim.scaleY = charObj->charScale;
                }
                ghostObj->charScale = charObj->charScale;
                ghostObj->charScaleX = charObj->charScale;
                ghostObj->charScaleY = charObj->charScale;
                if (ghostObj->isSpritemap) {
                    ghostObj->spritemapAnim.scaleX = charObj->charScale;
                    ghostObj->spritemapAnim.scaleY = charObj->charScale;
                }
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 10) { // Flip X
                charObj->flipX = !charObj->flipX;
                ghostObj->flipX = charObj->flipX;
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 11) { // Antialiasing
                charObj->noAntialiasing = !charObj->noAntialiasing;
                ghostObj->noAntialiasing = charObj->noAntialiasing;
                charObj->setAntialiasing(!charObj->noAntialiasing);
                ghostObj->setAntialiasing(!ghostObj->noAntialiasing);
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 12) { // Pos X
                charObj->x += pressLeft ? -change : change;
                charObj->baseX = charObj->x;
                ghostObj->x = charObj->x;
                ghostObj->baseX = charObj->x;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 13) { // Pos Y
                charObj->y += pressLeft ? -change : change;
                charObj->baseY = charObj->y;
                ghostObj->y = charObj->y;
                ghostObj->baseY = charObj->y;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 14) { // Cam Offset X
                charObj->camOffsetX += pressLeft ? -change * 5.0f : change * 5.0f;
                ghostObj->camOffsetX = charObj->camOffsetX;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 15) { // Cam Offset Y
                charObj->camOffsetY += pressLeft ? -change * 5.0f : change * 5.0f;
                ghostObj->camOffsetY = charObj->camOffsetY;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 16) { // Animation selection list
                if (!animList.empty()) {
                    if (pressLeft) {
                        animSliderIndex--;
                        if (animSliderIndex < 0) animSliderIndex = animList.size() - 1;
                    } else {
                        animSliderIndex++;
                        if (animSliderIndex >= (int)animList.size()) animSliderIndex = 0;
                    }
                    curAnimIndex = animSliderIndex;
                    playCurAnim();
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                }
            } else if (optId == 17 && !animList.empty()) { // FPS
                std::string curAnimName = animList[animSliderIndex];
                charObj->animations[curAnimName].fps += pressLeft ? -1 : 1;
                if (charObj->animations[curAnimName].fps < 1) charObj->animations[curAnimName].fps = 1;
                ghostObj->animations[curAnimName].fps = charObj->animations[curAnimName].fps;
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 18 && !animList.empty()) { // Loop
                std::string curAnimName = animList[animSliderIndex];
                charObj->setAnimLoop(curAnimName, !charObj->animations[curAnimName].loop);
                ghostObj->setAnimLoop(curAnimName, charObj->animations[curAnimName].loop);
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 19 && !animList.empty()) { // Offset X
                std::string curAnimName = animList[animSliderIndex];
                charObj->animations[curAnimName].offsetX += pressLeft ? -change : change;
                ghostObj->animations[curAnimName].offsetX = charObj->animations[curAnimName].offsetX;
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 20 && !animList.empty()) { // Offset Y
                std::string curAnimName = animList[animSliderIndex];
                charObj->animations[curAnimName].offsetY += pressLeft ? -change : change;
                ghostObj->animations[curAnimName].offsetY = charObj->animations[curAnimName].offsetY;
                playCurAnim();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 21) { // Enable Frame Checkbox
                showCamBounds = !showCamBounds;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 22) { // Cam Zoom Stepper
                simCamZoom += pressLeft ? -0.05f : 0.05f;
                if (simCamZoom < 0.1f) simCamZoom = 0.1f;
                if (simCamZoom > 3.0f) simCamZoom = 3.0f;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 23) { // Ref Char listbox
                curRefCharIndex += pressLeft ? -1 : 1;
                if (curRefCharIndex < 0) curRefCharIndex = characterList.size() - 1;
                if (curRefCharIndex >= (int)characterList.size()) curRefCharIndex = 0;
                loadRefCharacter(characterList[curRefCharIndex]);
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 29) { // Ref Anim listbox
                if (!refAnimList.empty()) {
                    refAnimSliderIndex += pressLeft ? -1 : 1;
                    if (refAnimSliderIndex < 0) refAnimSliderIndex = refAnimList.size() - 1;
                    if (refAnimSliderIndex >= (int)refAnimList.size()) refAnimSliderIndex = 0;
                    curRefAnimIndex = refAnimSliderIndex;
                    playCurRefAnim();
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                }
            } else if (optId == 24) { // Show Ref Checkbox
                showRefChar = !showRefChar;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 26) { // Ref Alpha Stepper
                refAlpha += pressLeft ? -0.05f : 0.05f;
                if (refAlpha < 0.0f) refAlpha = 0.0f;
                if (refAlpha > 1.0f) refAlpha = 1.0f;
                if (refObj) refObj->alpha = refAlpha;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 27 && refObj) { // Ref Pos X
                refObj->x += pressLeft ? -change : change;
                refObj->baseX = refObj->x;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            } else if (optId == 28 && refObj) { // Ref Pos Y
                refObj->y += pressLeft ? -change : change;
                refObj->baseY = refObj->y;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            }
        }
    }

    // Touch Screen Inputs
    touchPosition touch;
    bool touchHeld = (hidKeysHeld() & KEY_TOUCH);
    if (touchHeld) {
        hidTouchRead(&touch);
    }
    bool touchDown = keyJustPressed(KEY_TOUCH);
    bool touchUp = (!touchHeld && touchHeldLastFrame);

    if (touchDown) {
        touchStartPx = touch.px;
        touchStartPy = touch.py;
        touchMoved = false;
    }

    if (touchHeld) {
        float mdx = touch.px - touchStartPx;
        float mdy = touch.py - touchStartPy;
        if (mdx * mdx + mdy * mdy > 100.0f) {
            touchMoved = true;
        }
    }

    // Top header & Bottom taskbar buttons check in PC mode
    if (touchDown && pcMode) {
        float headerH = 14.0f * pcUiScale;
        float footerH = 14.0f * pcUiScale;
        float stW = 112.0f * pcUiScale;
        float stX = 320.0f - stW - 3.0f;
        float btnBoxW = 16.0f * pcUiScale;

        if (touch.px >= 0 && touch.px <= btnBoxW + 2.0f && touch.py >= 0 && touch.py <= headerH) {
            showAnimOverlay = !showAnimOverlay;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            touchHeld = false;
            touchDown = false;
            touchUp = false;
        } else if (touch.px >= btnBoxW + 3.0f && touch.px <= btnBoxW * 2.0f + 5.0f && touch.py >= 0 && touch.py <= headerH) {
            dragTargetMode = (dragTargetMode == 0) ? 1 : 0;
            centerCameraOnTarget();
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            touchHeld = false;
            touchDown = false;
            touchUp = false;
        } else if (touch.py >= 0 && touch.py <= headerH && touch.px >= stX - 5.0f) {
            float oldScale = pcUiScale;
            if (touch.px >= stX && touch.px <= stX + 22.0f * pcUiScale) { // Decrement [-]
                pcUiScale -= 0.10f;
                if (pcUiScale < 0.60f) pcUiScale = 0.60f;
            } else if (touch.px >= stX + stW - 22.0f * pcUiScale && touch.px <= 320.0f) { // Increment [+]
                pcUiScale += 0.10f;
                if (pcUiScale > 1.50f) pcUiScale = 1.50f;
            } else { // Reset center
                pcUiScale = 1.00f;
            }
            if (pcUiScale != oldScale) {
                float ratio = pcUiScale / oldScale;
                for (auto& win : windows) {
                    win.w *= ratio;
                    win.h *= ratio;
                    if (win.w < 60.0f * pcUiScale) win.w = 60.0f * pcUiScale;
                    if (win.w > 300.0f) win.w = 300.0f;
                    if (win.h < 50.0f * pcUiScale) win.h = 50.0f * pcUiScale;
                    if (win.h > 210.0f) win.h = 210.0f;
                }
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
            }
            touchHeld = false;
            touchDown = false;
            touchUp = false;
        }
        else if (touch.py >= 240.0f - footerH && touch.py <= 240.0f) {
            int clickedWinId = -1;
            float tX0 = 3.0f;
            float tW0 = 24.0f * pcUiScale;
            float tW1 = 30.0f * pcUiScale;
            float tW2 = 26.0f * pcUiScale;
            float tW3 = 28.0f * pcUiScale;
            float tW4 = 25.0f * pcUiScale;
            float tW5 = 24.0f * pcUiScale;

            if (touch.px >= tX0 && touch.px <= tX0 + tW0) clickedWinId = 0;       // Set
            else if (touch.px >= tX0 + tW0 + 2.0f && touch.px <= tX0 + tW0 + tW1 + 2.0f) clickedWinId = 1;  // Ghost
            else if (touch.px >= tX0 + tW0 + tW1 + 4.0f && touch.px <= tX0 + tW0 + tW1 + tW2 + 4.0f) clickedWinId = 2; // Char
            else if (touch.px >= tX0 + tW0 + tW1 + tW2 + 6.0f && touch.px <= tX0 + tW0 + tW1 + tW2 + tW3 + 6.0f) clickedWinId = 3; // Anim
            else if (touch.px >= tX0 + tW0 + tW1 + tW2 + tW3 + 8.0f && touch.px <= tX0 + tW0 + tW1 + tW2 + tW3 + tW4 + 8.0f) clickedWinId = 4; // Cam
            else if (touch.px >= tX0 + tW0 + tW1 + tW2 + tW3 + tW4 + 10.0f && touch.px <= tX0 + tW0 + tW1 + tW2 + tW3 + tW4 + tW5 + 10.0f) clickedWinId = 5; // Ref

            if (clickedWinId != -1) {
                for (size_t i = 0; i < windows.size(); i++) {
                    if (windows[i].id == clickedWinId) {
                        windows[i].expanded = !windows[i].expanded;
                        if (windows[i].expanded) {
                            UIWindow temp = windows[i];
                            windows.erase(windows.begin() + i);
                            windows.push_back(temp);
                        }
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        break;
                    }
                }
                touchHeld = false;
                touchDown = false;
                touchUp = false;
            }
        }
    }

    if (touchHeld) {
        if (pcMode) {
            if (resizingWindowId != -1) {
                // Resize window with minimum and maximum boundaries per window content
                for (auto& win : windows) {
                    if (win.id == resizingWindowId) {
                        float newW = touch.px - win.x;
                        float newH = touch.py - win.y;

                        float minW = 120.0f * pcUiScale;
                        float minH = 80.0f * pcUiScale;
                        if (win.id == 0) { // Character list
                            minW = 130.0f * pcUiScale;
                            minH = 85.0f * pcUiScale;
                        } else if (win.id == 1) { // Ghost
                            minW = 120.0f * pcUiScale;
                            minH = 75.0f * pcUiScale;
                        } else if (win.id == 2) { // Settings
                            minW = 130.0f * pcUiScale;
                            minH = 105.0f * pcUiScale;
                        } else if (win.id == 3) { // Animations
                            minW = 130.0f * pcUiScale;
                            minH = 90.0f * pcUiScale;
                        } else if (win.id == 5) { // Reference
                            minW = 130.0f * pcUiScale;
                            minH = 100.0f * pcUiScale;
                        }

                        if (newW < minW) newW = minW;
                        if (newW > 280.0f) newW = 280.0f;
                        if (newH < minH) newH = minH;
                        if (newH > 220.0f) newH = 220.0f;

                        win.w = newW;
                        win.h = newH;
                        break;
                    }
                }
            } else if (draggedWindowId != -1) {
                // Drag the window
                for (auto& win : windows) {
                    if (win.id == draggedWindowId) {
                        win.x = touch.px - dragWindowOffsetX;
                        win.y = touch.py - dragWindowOffsetY;
                        
                        // Clamp window to screen bounds so it doesn't get completely lost
                        if (win.x < -win.w + 20.0f) win.x = -win.w + 20.0f;
                        if (win.x > 320.0f - 20.0f) win.x = 320.0f - 20.0f;
                        if (win.y < 0.0f) win.y = 0.0f;
                        if (win.y > 240.0f - 12.0f) win.y = 240.0f - 12.0f;
                        break;
                    }
                }
            } else if (isDraggingCharList) {
                float dy = touch.py - dragStartY;
                charScrollY = dragStartScroll - dy;
                float itemH = 10.0f * pcUiScale;
                float boxH = 54.0f;
                float maxScroll = (characterList.size() * itemH) - boxH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (charScrollY < 0.0f) charScrollY = 0.0f;
                if (charScrollY > maxScroll) charScrollY = maxScroll;
            } else if (isDraggingAnimList) {
                float dy = touch.py - dragStartY;
                animScrollY = dragStartScroll - dy;
                float itemH = 10.0f * pcUiScale;
                float boxH = 54.0f;
                float maxScroll = (animList.size() * itemH) - boxH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (animScrollY < 0.0f) animScrollY = 0.0f;
                if (animScrollY > maxScroll) animScrollY = maxScroll;
            } else if (isDraggingRefCharList) {
                float dy = touch.py - dragStartY;
                refCharScrollY = dragStartScroll - dy;
                float itemH = 10.0f * pcUiScale;
                float boxH = 38.0f * pcUiScale;
                float maxScroll = (characterList.size() * itemH) - boxH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (refCharScrollY < 0.0f) refCharScrollY = 0.0f;
                if (refCharScrollY > maxScroll) refCharScrollY = maxScroll;
            } else if (isDraggingRefAnimList) {
                float dy = touch.py - dragStartY;
                refAnimScrollY = dragStartScroll - dy;
                float itemH = 10.0f * pcUiScale;
                float boxH = 38.0f * pcUiScale;
                float maxScroll = (refAnimList.size() * itemH) - boxH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (refAnimScrollY < 0.0f) refAnimScrollY = 0.0f;
                if (refAnimScrollY > maxScroll) refAnimScrollY = maxScroll;
            } else {
                // Continuous Slider Dragging
                bool touchingAnyWindow = false;
                if (uiExpanded) {
                    auto layout = getPCLayout(windows, pcUiScale);
                    for (const auto& elem : layout) {
                        if (elem.id == 7) { // Ghost Alpha Slider
                            float sliderX = elem.x + 4.0f;
                            float sliderW = elem.w - 8.0f;
                            if (touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                                touchingAnyWindow = true;
                                float pct = (touch.px - sliderX) / sliderW;
                                if (pct < 0.0f) pct = 0.0f;
                                if (pct > 1.0f) pct = 1.0f;
                                ghostAlpha = pct;
                                ghostObj->alpha = ghostAlpha;
                            }
                        } else if (elem.id == 26) { // Ref Alpha Slider
                            float sliderX = elem.x + 4.0f;
                            float sliderW = elem.w - 8.0f;
                            if (touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                                touchingAnyWindow = true;
                                float pct = (touch.px - sliderX) / sliderW;
                                if (pct < 0.0f) pct = 0.0f;
                                if (pct > 1.0f) pct = 1.0f;
                                refAlpha = pct;
                                if (refObj) refObj->alpha = refAlpha;
                            }
                        } else {
                            if (touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                                touchingAnyWindow = true;
                            }
                        }
                    }
                    for (const auto& win : windows) {
                        if (!win.expanded) continue;
                        if (touch.px >= win.x && touch.px <= win.x + win.w && touch.py >= win.y && touch.py <= win.y + win.h) {
                            touchingAnyWindow = true;
                        }
                    }
                }

                if (!touchingAnyWindow) {
                    if (lastTouchX != -1) {
                        float dx = touch.px - lastTouchX;
                        float dy = touch.py - lastTouchY;
                        
                        if (hidKeysHeld() & KEY_Y) {
                            camX -= dx / camZoom;
                            camY -= dy / camZoom;
                        } else if (dragTargetMode == 1 && refObj) {
                            float posScale = (240.0f / 720.0f) * camZoom;
                            if (posScale > 0.001f) {
                                float dOffX = dx / posScale;
                                float dOffY = dy / posScale;
                                refObj->x += dOffX;
                                refObj->y += dOffY;
                                refObj->baseX = refObj->x;
                                refObj->baseY = refObj->y;
                            }
                        } else if (charObj && !animList.empty()) {
                            std::string curAnimName = animList[curAnimIndex];
                            float scaleFactor = charObj->charScale * (240.0f / 720.0f) * camZoom;
                            if (scaleFactor > 0.01f) {
                                float dOffX = dx / scaleFactor;
                                float dOffY = dy / scaleFactor;
                                
                                bool shouldFlip = (charObj->isPlayer != charObj->flipX);
                                if (shouldFlip) {
                                    charObj->animations[curAnimName].offsetX += dOffX;
                                } else {
                                    charObj->animations[curAnimName].offsetX -= dOffX;
                                }
                                charObj->animations[curAnimName].offsetY -= dOffY;
                                
                                if (curAnimIndex == curGhostAnimIndex) {
                                    ghostObj->animations[curAnimName].offsetX = charObj->animations[curAnimName].offsetX;
                                    ghostObj->animations[curAnimName].offsetY = charObj->animations[curAnimName].offsetY;
                                }
                                playCurAnim();
                            }
                        }
                    }
                    lastTouchX = touch.px;
                    lastTouchY = touch.py;
                } else {
                    lastTouchX = -1;
                    lastTouchY = -1;
                }
            }
        } 
        else if (uiExpanded || !pcMode) {
            lastTouchX = -1;
            lastTouchY = -1;

            float boxX = 10.0f;
            float boxY = 28.0f;
            float boxW = 300.0f;
            float boxH = 82.0f;
            float itemH = 16.0f;

            if (touchDown) {
                // 1. Tab Headers (Normal Mode Only)
                if (touch.py >= 0 && touch.py <= 24) {
                    float tabW = 53.33f;
                    int newTab = (int)(touch.px / tabW);
                    if (newTab >= 0 && newTab <= 5) {
                        currentTab = newTab;
                        curSelected = 0;
                        isDraggingCharList = false;
                        isDraggingAnimList = false;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                }
                // 2. Drag Start for Lists (Normal Mode Only)
                else if (currentTab == 0 && touch.px >= boxX && touch.px <= boxX + boxW && touch.py >= boxY && touch.py <= boxY + boxH) {
                    dragStartY = touch.py;
                    dragStartScroll = charScrollY;
                    isDraggingCharList = true;
                    curSelected = 0;
                }
                else if (currentTab == 3 && touch.px >= boxX && touch.px <= boxX + boxW && touch.py >= boxY && touch.py <= boxY + boxH) {
                    dragStartY = touch.py;
                    dragStartScroll = animScrollY;
                    isDraggingAnimList = true;
                    curSelected = 0;
                }
                else if (currentTab == 5 && touch.px >= boxX && touch.px <= boxX + boxW && touch.py >= 27.0f && touch.py <= 75.0f) {
                    dragStartY = touch.py;
                    dragStartScroll = refCharScrollY;
                    isDraggingRefCharList = true;
                    curSelected = 0;
                }
                else if (currentTab == 5 && touch.px >= boxX && touch.px <= boxX + boxW && touch.py >= 78.0f && touch.py <= 126.0f) {
                    dragStartY = touch.py;
                    dragStartScroll = refAnimScrollY;
                    isDraggingRefAnimList = true;
                    curSelected = 1;
                }
            }

            if (currentTab == 1) {
                float sliderX = 15.0f;
                float sliderW = 290.0f;
                float sliderY = 136.0f;
                if (touch.py >= sliderY - 10.0f && touch.py <= sliderY + 24.0f) {
                    curSelected = 3;
                    float pct = (touch.px - sliderX) / sliderW;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 1.0f) pct = 1.0f;
                    ghostAlpha = pct;
                    ghostObj->alpha = ghostAlpha;
                }
            } else if (currentTab == 5) {
                float sliderX = 15.0f;
                float sliderW = 290.0f;
                float sliderY = 166.0f;
                if (touch.py >= sliderY - 10.0f && touch.py <= sliderY + 24.0f) {
                    curSelected = 3;
                    float pct = (touch.px - sliderX) / sliderW;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 1.0f) pct = 1.0f;
                    refAlpha = pct;
                    if (refObj) refObj->alpha = refAlpha;
                }
            }

            if (isDraggingCharList) {
                float dy = touch.py - dragStartY;
                charScrollY = dragStartScroll - dy;
                float maxScroll = (characterList.size() * itemH) - boxH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (charScrollY < 0.0f) charScrollY = 0.0f;
                if (charScrollY > maxScroll) charScrollY = maxScroll;
            }
            else if (isDraggingAnimList) {
                float dy = touch.py - dragStartY;
                animScrollY = dragStartScroll - dy;
                float maxScroll = (animList.size() * itemH) - boxH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (animScrollY < 0.0f) animScrollY = 0.0f;
                if (animScrollY > maxScroll) animScrollY = maxScroll;
            }
            else if (isDraggingRefCharList) {
                float dy = touch.py - dragStartY;
                refCharScrollY = dragStartScroll - dy;
                float maxScroll = (characterList.size() * itemH) - 48.0f;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (refCharScrollY < 0.0f) refCharScrollY = 0.0f;
                if (refCharScrollY > maxScroll) refCharScrollY = maxScroll;
            }
            else if (isDraggingRefAnimList) {
                float dy = touch.py - dragStartY;
                refAnimScrollY = dragStartScroll - dy;
                float maxScroll = (refAnimList.size() * itemH) - 48.0f;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                if (refAnimScrollY < 0.0f) refAnimScrollY = 0.0f;
                if (refAnimScrollY > maxScroll) refAnimScrollY = maxScroll;
            }
        }
    } else {
        lastTouchX = -1;
        lastTouchY = -1;
    }

    // Touch Down Processing for PC Mode (Focus, Resize, and Drag start)
    if (touchDown && pcMode && uiExpanded) {
        for (int i = (int)windows.size() - 1; i >= 0; i--) {
            auto& win = windows[i];
            if (!win.expanded) continue; // Minimized windows do not draw or intercept touches on main canvas

            if (touch.px >= win.x && touch.px <= win.x + win.w && touch.py >= win.y && touch.py <= win.y + win.h) {
                // Focus: Bring window to front
                if (i < (int)windows.size() - 1) {
                    UIWindow temp = win;
                    windows.erase(windows.begin() + i);
                    windows.push_back(temp);
                }
                auto& focusedWin = windows.back();

                // Check resize handle (bottom-right 14x14)
                if (touch.px >= focusedWin.x + focusedWin.w - 14.0f && touch.py >= focusedWin.y + focusedWin.h - 14.0f) {
                    resizingWindowId = focusedWin.id;
                }
                else if (touch.py <= focusedWin.y + 12.0f * pcUiScale) {
                    // Clicked title bar
                    if (touch.px >= focusedWin.x + focusedWin.w - 14.0f * pcUiScale) {
                        focusedWin.expanded = false; // Minimize to bottom taskbar!
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    } else {
                        // Start dragging window
                        draggedWindowId = focusedWin.id;
                        dragWindowOffsetX = touch.px - focusedWin.x;
                        dragWindowOffsetY = touch.py - focusedWin.y;
                    }
                } else {
                    // Clicked inside body - check listboxes start drag
                    auto layout = getPCLayout(windows, pcUiScale);
                    for (const auto& elem : layout) {
                        if (elem.id == 0 && touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                            dragStartY = touch.py;
                            dragStartScroll = charScrollY;
                            isDraggingCharList = true;
                            auto it = std::find(visibleOptionIds.begin(), visibleOptionIds.end(), elem.id);
                            if (it != visibleOptionIds.end()) curSelected = std::distance(visibleOptionIds.begin(), it);
                        } else if (elem.id == 16 && touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                            dragStartY = touch.py;
                            dragStartScroll = animScrollY;
                            isDraggingAnimList = true;
                            auto it = std::find(visibleOptionIds.begin(), visibleOptionIds.end(), elem.id);
                            if (it != visibleOptionIds.end()) curSelected = std::distance(visibleOptionIds.begin(), it);
                        } else if (elem.id == 23 && touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                            dragStartY = touch.py;
                            dragStartScroll = refCharScrollY;
                            isDraggingRefCharList = true;
                            auto it = std::find(visibleOptionIds.begin(), visibleOptionIds.end(), elem.id);
                            if (it != visibleOptionIds.end()) curSelected = std::distance(visibleOptionIds.begin(), it);
                        } else if (elem.id == 29 && touch.px >= elem.x && touch.px <= elem.x + elem.w && touch.py >= elem.y && touch.py <= elem.y + elem.h) {
                            dragStartY = touch.py;
                            dragStartScroll = refAnimScrollY;
                            isDraggingRefAnimList = true;
                            auto it = std::find(visibleOptionIds.begin(), visibleOptionIds.end(), elem.id);
                            if (it != visibleOptionIds.end()) curSelected = std::distance(visibleOptionIds.begin(), it);
                        }
                    }
                }
                break;
            }
        }
    }

    // Touch Up: Gesture tap detection
    if (touchUp) {
        draggedWindowId = -1;
        resizingWindowId = -1;
        isDraggingCharList = false;
        isDraggingAnimList = false;
        isDraggingRefCharList = false;
        isDraggingRefAnimList = false;

        // Execute tap action if touch didn't drag significantly
        if (!touchMoved && touchStartPy >= 0.0f && touchStartPy < 240.0f && (!pcMode || uiExpanded)) {
            if (pcMode) {
                auto layout = getPCLayout(windows, pcUiScale);
                for (const auto& elem : layout) {
                    if (touchStartPx >= elem.x && touchStartPx <= elem.x + elem.w && touchStartPy >= elem.y && touchStartPy <= elem.y + elem.h) {
                        if (elem.id != -1) {
                            // Tapped a control element
                            auto it = std::find(visibleOptionIds.begin(), visibleOptionIds.end(), elem.id);
                            if (it != visibleOptionIds.end()) {
                                curSelected = std::distance(visibleOptionIds.begin(), it);
                            }
                            
                            // Process action for this control
                            if (elem.id == 0) { // Character listbox
                                int tapped = (int)((touchStartPy - elem.y + charScrollY) / (10.0f * pcUiScale));
                                if (tapped >= 0 && tapped < (int)characterList.size()) {
                                    curCharIndex = tapped;
                                    loadCharacter(characterList[curCharIndex]);
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            } else if (elem.id == 1) { // Playable Checkbox
                                charObj->isPlayer = !charObj->isPlayer;
                                ghostObj->isPlayer = charObj->isPlayer;
                                centerCameraOnTarget();
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 2) { // Reload Button
                                loadCharacter(currentCharacter);
                                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
                            } else if (elem.id == 3) { // Save Button
                                saveCharacter();
                                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
                            } else if (elem.id == 4) { // Make Ghost Button
                                ghostObj->isPlayer = charObj->isPlayer;
                                ghostObj->flipX = charObj->flipX;
                                ghostObj->charScale = charObj->charScale;
                                ghostObj->charScaleX = charObj->charScale;
                                ghostObj->charScaleY = charObj->charScale;
                                if (ghostObj->isSpritemap) {
                                    ghostObj->spritemapAnim.scaleX = charObj->charScale;
                                    ghostObj->spritemapAnim.scaleY = charObj->charScale;
                                }
                                ghostObj->animations = charObj->animations;
                                ghostObj->playAnim(charObj->curAnim, true);
                                ghostObj->curFrame = charObj->curFrame;
                                ghostObj->frameTimer = charObj->frameTimer;
                                ghostObj->animFinished = charObj->animFinished;
                                ghostObj->spritemapAnim.smLogicalFrame = charObj->spritemapAnim.smLogicalFrame;
                                ghostObj->spritemapAnim.animFinished = charObj->spritemapAnim.animFinished;
                                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
                            } else if (elem.id == 5) { // Show Ghost Checkbox
                                showGhost = !showGhost;
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 6) { // Highlight Ghost Checkbox
                                ghostHighlight = !ghostHighlight;
                                ghostObj->isHighlighted = ghostHighlight;
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 10) { // Flip X Checkbox
                                charObj->flipX = !charObj->flipX;
                                ghostObj->flipX = charObj->flipX;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 11) { // Antialiasing Checkbox
                                charObj->noAntialiasing = !charObj->noAntialiasing;
                                ghostObj->noAntialiasing = charObj->noAntialiasing;
                                charObj->setAntialiasing(!charObj->noAntialiasing);
                                ghostObj->setAntialiasing(!ghostObj->noAntialiasing);
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 16) { // Animations listbox
                                int tapped = (int)((touchStartPy - elem.y + animScrollY) / (10.0f * pcUiScale));
                                if (tapped >= 0 && tapped < (int)animList.size()) {
                                    animSliderIndex = tapped;
                                    curAnimIndex = animSliderIndex;
                                    playCurAnim();
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            } else if (elem.id == 21) { // Enable Cam Frame Checkbox
                                showCamBounds = !showCamBounds;
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 23) { // Ref Char listbox tap
                                int tapped = (int)((touchStartPy - elem.y + refCharScrollY) / (10.0f * pcUiScale));
                                if (tapped >= 0 && tapped < (int)characterList.size()) {
                                    curRefCharIndex = tapped;
                                    loadRefCharacter(characterList[curRefCharIndex]);
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            } else if (elem.id == 29) { // Ref Anim listbox tap
                                int tapped = (int)((touchStartPy - elem.y + refAnimScrollY) / (10.0f * pcUiScale));
                                if (tapped >= 0 && tapped < (int)refAnimList.size()) {
                                    refAnimSliderIndex = tapped;
                                    curRefAnimIndex = refAnimSliderIndex;
                                    playCurRefAnim();
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            } else if (elem.id == 24) { // Show Ref Checkbox
                                showRefChar = !showRefChar;
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 26) { // Ref Alpha Slider
                                float sliderX = elem.x + 4.0f;
                                float sliderW = elem.w - 8.0f;
                                float pct = (touchStartPx - sliderX) / sliderW;
                                if (pct < 0.0f) pct = 0.0f;
                                if (pct > 1.0f) pct = 1.0f;
                                refAlpha = pct;
                                if (refObj) refObj->alpha = refAlpha;
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (elem.id == 18) { // Loop Checkbox
                                if (!animList.empty()) {
                                    std::string curAnimName = animList[animSliderIndex];
                                    charObj->setAnimLoop(curAnimName, !charObj->animations[curAnimName].loop);
                                    ghostObj->setAnimLoop(curAnimName, charObj->animations[curAnimName].loop);
                                    playCurAnim();
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            } else {
                                // Arrows adjusted elements
                                float arrowL = elem.x + elem.w - 32.0f * pcUiScale;
                                float arrowR = elem.x + elem.w - 12.0f * pcUiScale;
                                bool isLeft = (touchStartPx >= arrowL - 4.0f * pcUiScale && touchStartPx <= arrowL + 14.0f * pcUiScale);
                                bool isRight = (touchStartPx >= arrowR - 4.0f * pcUiScale && touchStartPx <= arrowR + 14.0f * pcUiScale);
                                if (isLeft || isRight) {
                                    float sign = isLeft ? -1.0f : 1.0f;
                                    if (elem.id == 8) { // Sing Length
                                        charObj->singDuration += sign * 0.5f;
                                        if (charObj->singDuration < 0.1f) charObj->singDuration = 0.1f;
                                        ghostObj->singDuration = charObj->singDuration;
                                    } else if (elem.id == 9) { // Scale
                                        charObj->charScale += sign * change * 0.1f;
                                        if (charObj->charScale < 0.1f) charObj->charScale = 0.1f;
                                        charObj->charScaleX = charObj->charScale;
                                        charObj->charScaleY = charObj->charScale;
                                        if (charObj->isSpritemap) {
                                            charObj->spritemapAnim.scaleX = charObj->charScale;
                                            charObj->spritemapAnim.scaleY = charObj->charScale;
                                        }
                                        ghostObj->charScale = charObj->charScale;
                                        ghostObj->charScaleX = charObj->charScale;
                                        ghostObj->charScaleY = charObj->charScale;
                                        if (ghostObj->isSpritemap) {
                                            ghostObj->spritemapAnim.scaleX = charObj->charScale;
                                            ghostObj->spritemapAnim.scaleY = charObj->charScale;
                                        }
                                        playCurAnim();
                                    } else if (elem.id == 12) { // Pos X
                                        charObj->x += sign * change;
                                        charObj->baseX = charObj->x;
                                        ghostObj->x = charObj->x;
                                        ghostObj->baseX = charObj->x;
                                    } else if (elem.id == 13) { // Pos Y
                                        charObj->y += sign * change;
                                        charObj->baseY = charObj->y;
                                        ghostObj->y = charObj->y;
                                        ghostObj->baseY = charObj->y;
                                    } else if (elem.id == 14) { // Cam Off X
                                        charObj->camOffsetX += sign * change * 5.0f;
                                        ghostObj->camOffsetX = charObj->camOffsetX;
                                    } else if (elem.id == 15) { // Cam Off Y
                                        charObj->camOffsetY += sign * change * 5.0f;
                                        ghostObj->camOffsetY = charObj->camOffsetY;
                                    } else if (elem.id == 17 && !animList.empty()) { // FPS
                                        std::string curAnimName = animList[animSliderIndex];
                                        charObj->animations[curAnimName].fps += (int)sign;
                                        if (charObj->animations[curAnimName].fps < 1) charObj->animations[curAnimName].fps = 1;
                                        ghostObj->animations[curAnimName].fps = charObj->animations[curAnimName].fps;
                                        playCurAnim();
                                    } else if (elem.id == 19 && !animList.empty()) { // Offset X
                                        std::string curAnimName = animList[animSliderIndex];
                                        charObj->animations[curAnimName].offsetX += sign * change;
                                        ghostObj->animations[curAnimName].offsetX = charObj->animations[curAnimName].offsetX;
                                        playCurAnim();
                                    } else if (elem.id == 20 && !animList.empty()) { // Offset Y
                                        std::string curAnimName = animList[animSliderIndex];
                                        charObj->animations[curAnimName].offsetY += sign * change;
                                        ghostObj->animations[curAnimName].offsetY = charObj->animations[curAnimName].offsetY;
                                        playCurAnim();
                                    } else if (elem.id == 22) { // Cam Zoom Stepper
                                        simCamZoom += sign * 0.05f;
                                        if (simCamZoom < 0.1f) simCamZoom = 0.1f;
                                        if (simCamZoom > 3.0f) simCamZoom = 3.0f;
                                    } else if (elem.id == 27 && refObj) { // Ref Pos X
                                        refObj->x += sign * change;
                                        refObj->baseX = refObj->x;
                                    } else if (elem.id == 28 && refObj) { // Ref Pos Y
                                        refObj->y += sign * change;
                                        refObj->baseY = refObj->y;
                                    }
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            }
                        }
                        break;
                    }
                }
            } else {
                // Original 3DS Tab tap detection logic (Normal Mode) - Improved for touch sensitivity
                if (currentTab == 0) { // Settings
                    // Tapped Character List Box
                    float boxX = 10.0f, boxY = 25.0f, boxW = 300.0f, boxH = 88.0f, itemH = 16.0f;
                    if (touchStartPx >= boxX && touchStartPx <= boxX + boxW && touchStartPy >= boxY && touchStartPy <= boxY + boxH) {
                        int tapped = (int)((touchStartPy - boxY + charScrollY) / itemH);
                        if (tapped >= 0 && tapped < (int)characterList.size()) {
                            curCharIndex = tapped;
                            loadCharacter(characterList[curCharIndex]);
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                    // Playable Checkbox (forgiving row height tap)
                    float checkY = 118.0f;
                    if (touchStartPy >= checkY - 5.0f && touchStartPy <= checkY + 20.0f) {
                        curSelected = 1;
                        charObj->isPlayer = !charObj->isPlayer;
                        ghostObj->isPlayer = charObj->isPlayer;
                        centerCameraOnTarget();
                        playCurAnim();
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                    // Reload Button
                    float btnY = 145.0f;
                    float btnH = 24.0f;
                    if (touchStartPy >= btnY - 2.0f && touchStartPy <= btnY + btnH + 2.0f && touchStartPx >= 10 && touchStartPx <= 310) {
                        curSelected = 2;
                        loadCharacter(currentCharacter);
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
                    }
                    // Save Button
                    float saveY = 175.0f;
                    float saveH = 24.0f;
                    if (touchStartPy >= saveY - 2.0f && touchStartPy <= saveY + saveH + 2.0f && touchStartPx >= 10 && touchStartPx <= 310) {
                        curSelected = 3;
                        saveCharacter();
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
                    }
                }
                else if (currentTab == 1) { // Ghost
                    // Make Ghost Button
                    float btnY = 30.0f;
                    float btnH = 26.0f;
                    if (touchStartPy >= btnY - 2.0f && touchStartPy <= btnY + btnH + 2.0f && touchStartPx >= 10 && touchStartPx <= 310) {
                        curSelected = 0;
                        ghostObj->isPlayer = charObj->isPlayer;
                        ghostObj->flipX = charObj->flipX;
                        ghostObj->charScale = charObj->charScale;
                        ghostObj->charScaleX = charObj->charScale;
                        ghostObj->charScaleY = charObj->charScale;
                        if (ghostObj->isSpritemap) {
                            ghostObj->spritemapAnim.scaleX = charObj->charScale;
                            ghostObj->spritemapAnim.scaleY = charObj->charScale;
                        }
                        ghostObj->animations = charObj->animations;
                        ghostObj->playAnim(charObj->curAnim, true);
                        ghostObj->curFrame = charObj->curFrame;
                        ghostObj->frameTimer = charObj->frameTimer;
                        ghostObj->animFinished = charObj->animFinished;
                        ghostObj->spritemapAnim.smLogicalFrame = charObj->spritemapAnim.smLogicalFrame;
                        ghostObj->spritemapAnim.animFinished = charObj->spritemapAnim.animFinished;
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.5f);
                    }
                    // Show Ghost Checkbox (forgiving row height)
                    float checkShowY = 64.0f;
                    if (touchStartPy >= checkShowY - 5.0f && touchStartPy <= checkShowY + 20.0f) {
                        curSelected = 1;
                        showGhost = !showGhost;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                    // Highlight Ghost Checkbox (forgiving row height)
                    float checkHighY = 88.0f;
                    if (touchStartPy >= checkHighY - 5.0f && touchStartPy <= checkHighY + 20.0f) {
                        curSelected = 2;
                        ghostHighlight = !ghostHighlight;
                        ghostObj->isHighlighted = ghostHighlight;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                }
                else if (currentTab == 2) { // Character
                    for (int i = 0; i < 8; i++) {
                        float itemY = 28.0f + i * 24.0f;
                        float itemH = 20.0f;
                        if (touchStartPy >= itemY - 2.0f && touchStartPy <= itemY + itemH + 2.0f) {
                            curSelected = i;
                            if (i == 2) { // Flip X
                                charObj->flipX = !charObj->flipX;
                                ghostObj->flipX = charObj->flipX;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (i == 3) { // Antialiasing
                                charObj->noAntialiasing = !charObj->noAntialiasing;
                                ghostObj->noAntialiasing = charObj->noAntialiasing;
                                charObj->setAntialiasing(!charObj->noAntialiasing);
                                ghostObj->setAntialiasing(!ghostObj->noAntialiasing);
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else {
                                float arrowL = 200.0f;
                                float arrowR = 280.0f;
                                if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) { // Left arrow
                                    if (i == 0) {
                                        charObj->singDuration -= 0.5f;
                                        if (charObj->singDuration < 0.1f) charObj->singDuration = 0.1f;
                                        ghostObj->singDuration = charObj->singDuration;
                                    } else if (i == 1) {
                                        charObj->charScale -= 0.1f;
                                        if (charObj->charScale < 0.1f) charObj->charScale = 0.1f;
                                        charObj->charScaleX = charObj->charScale;
                                        charObj->charScaleY = charObj->charScale;
                                        if (charObj->isSpritemap) {
                                            charObj->spritemapAnim.scaleX = charObj->charScale;
                                            charObj->spritemapAnim.scaleY = charObj->charScale;
                                        }
                                        ghostObj->charScale = charObj->charScale;
                                        ghostObj->charScaleX = charObj->charScale;
                                        ghostObj->charScaleY = charObj->charScale;
                                        if (ghostObj->isSpritemap) {
                                            ghostObj->spritemapAnim.scaleX = charObj->charScale;
                                            ghostObj->spritemapAnim.scaleY = charObj->charScale;
                                        }
                                        playCurAnim();
                                    } else if (i == 4) {
                                        charObj->x -= change;
                                        charObj->baseX = charObj->x;
                                        ghostObj->x = charObj->x;
                                        ghostObj->baseX = charObj->x;
                                    } else if (i == 5) {
                                        charObj->y -= change;
                                        charObj->baseY = charObj->y;
                                        ghostObj->y = charObj->y;
                                        ghostObj->baseY = charObj->y;
                                    } else if (i == 6) {
                                        charObj->camOffsetX -= change * 5.0f;
                                        ghostObj->camOffsetX = charObj->camOffsetX;
                                    } else if (i == 7) {
                                        charObj->camOffsetY -= change * 5.0f;
                                        ghostObj->camOffsetY = charObj->camOffsetY;
                                    }
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                } else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) { // Right arrow
                                    if (i == 0) {
                                        charObj->singDuration += 0.5f;
                                        ghostObj->singDuration = charObj->singDuration;
                                    } else if (i == 1) {
                                        charObj->charScale += 0.1f;
                                        charObj->charScaleX = charObj->charScale;
                                        charObj->charScaleY = charObj->charScale;
                                        if (charObj->isSpritemap) {
                                            charObj->spritemapAnim.scaleX = charObj->charScale;
                                            charObj->spritemapAnim.scaleY = charObj->charScale;
                                        }
                                        ghostObj->charScale = charObj->charScale;
                                        ghostObj->charScaleX = charObj->charScale;
                                        ghostObj->charScaleY = charObj->charScale;
                                        if (ghostObj->isSpritemap) {
                                            ghostObj->spritemapAnim.scaleX = charObj->charScale;
                                            ghostObj->spritemapAnim.scaleY = charObj->charScale;
                                        }
                                        playCurAnim();
                                    } else if (i == 4) {
                                        charObj->x += change;
                                        charObj->baseX = charObj->x;
                                        ghostObj->x = charObj->x;
                                        ghostObj->baseX = charObj->x;
                                    } else if (i == 5) {
                                        charObj->y += change;
                                        charObj->baseY = charObj->y;
                                        ghostObj->y = charObj->y;
                                        ghostObj->baseY = charObj->y;
                                    } else if (i == 6) {
                                        charObj->camOffsetX += change * 5.0f;
                                        ghostObj->camOffsetX = charObj->camOffsetX;
                                    } else if (i == 7) {
                                        charObj->camOffsetY += change * 5.0f;
                                        ghostObj->camOffsetY = charObj->camOffsetY;
                                    }
                                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                                }
                            }
                            break;
                        }
                    }
                }
                else if (currentTab == 3) { // Animations
                    // Tapped Animations List Box
                    float boxX = 10.0f, boxY = 25.0f, boxW = 300.0f, boxH = 88.0f, itemH = 16.0f;
                    if (touchStartPx >= boxX && touchStartPx <= boxX + boxW && touchStartPy >= boxY && touchStartPy <= boxY + boxH) {
                        int tapped = (int)((touchStartPy - boxY + animScrollY) / itemH);
                        if (tapped >= 0 && tapped < (int)animList.size()) {
                            animSliderIndex = tapped;
                            curAnimIndex = animSliderIndex;
                            playCurAnim();
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                    
                    if (!animList.empty()) {
                        std::string curAnimName = animList[animSliderIndex];
                        float arrowL = 200.0f;
                        float arrowR = 280.0f;

                        // FPS
                        float fpsY = 122.0f;
                        if (touchStartPy >= fpsY - 4.0f && touchStartPy <= fpsY + 18.0f) {
                            curSelected = 1;
                            if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) {
                                charObj->animations[curAnimName].fps--;
                                if (charObj->animations[curAnimName].fps < 1) charObj->animations[curAnimName].fps = 1;
                                ghostObj->animations[curAnimName].fps = charObj->animations[curAnimName].fps;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) {
                                charObj->animations[curAnimName].fps++;
                                ghostObj->animations[curAnimName].fps = charObj->animations[curAnimName].fps;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            }
                        }
                        // Loop (forgiving row height)
                        else if (touchStartPy >= 142.0f && touchStartPy <= 164.0f) {
                            curSelected = 2;
                            charObj->setAnimLoop(curAnimName, !charObj->animations[curAnimName].loop);
                            ghostObj->setAnimLoop(curAnimName, charObj->animations[curAnimName].loop);
                            playCurAnim();
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                        // Offset X
                        float offXY = 166.0f;
                        if (touchStartPy >= offXY - 4.0f && touchStartPy <= offXY + 18.0f) {
                            curSelected = 3;
                            if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) {
                                charObj->animations[curAnimName].offsetX -= change;
                                ghostObj->animations[curAnimName].offsetX = charObj->animations[curAnimName].offsetX;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) {
                                charObj->animations[curAnimName].offsetX += change;
                                ghostObj->animations[curAnimName].offsetX = charObj->animations[curAnimName].offsetX;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            }
                        }
                        // Offset Y
                        float offYY = 188.0f;
                        if (touchStartPy >= offYY - 4.0f && touchStartPy <= offYY + 18.0f) {
                            curSelected = 4;
                            if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) {
                                charObj->animations[curAnimName].offsetY -= change;
                                ghostObj->animations[curAnimName].offsetY = charObj->animations[curAnimName].offsetY;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            } else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) {
                                charObj->animations[curAnimName].offsetY += change;
                                ghostObj->animations[curAnimName].offsetY = charObj->animations[curAnimName].offsetY;
                                playCurAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            }
                        }
                }
                else if (currentTab == 4) { // Camera
                    float checkY = 40.0f;
                    if (touchStartPy >= checkY - 5.0f && touchStartPy <= checkY + 24.0f) {
                        curSelected = 0;
                        showCamBounds = !showCamBounds;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                    float zoomY = 80.0f;
                    if (touchStartPy >= zoomY - 5.0f && touchStartPy <= zoomY + 24.0f) {
                        curSelected = 1;
                        float arrowL = 200.0f;
                        float arrowR = 280.0f;
                        if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) {
                            simCamZoom -= 0.05f;
                            if (simCamZoom < 0.1f) simCamZoom = 0.1f;
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        } else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) {
                            simCamZoom += 0.05f;
                            if (simCamZoom > 3.0f) simCamZoom = 3.0f;
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                }
                else if (currentTab == 5) { // Reference
                    float box1X = 10.0f, box1Y = 27.0f, box1W = 300.0f, box1H = 48.0f;
                    float box2X = 10.0f, box2Y = 78.0f, box2W = 300.0f, box2H = 48.0f;
                    float checkShowY = 130.0f;
                    float alphaY = 155.0f;
                    float posX_Y = 180.0f;
                    float posY_Y = 205.0f;
                    float arrowL = 200.0f;
                    float arrowR = 280.0f;

                    if (touchStartPx >= box1X && touchStartPx <= box1X + box1W && touchStartPy >= box1Y && touchStartPy <= box1Y + box1H) {
                        curSelected = 0;
                        int tapped = (int)((touchStartPy - box1Y + refCharScrollY) / 16.0f);
                        if (tapped >= 0 && tapped < (int)characterList.size()) {
                            curRefCharIndex = tapped;
                            loadRefCharacter(characterList[curRefCharIndex]);
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                    else if (touchStartPx >= box2X && touchStartPx <= box2X + box2W && touchStartPy >= box2Y && touchStartPy <= box2Y + box2H) {
                        curSelected = 1;
                        if (!refAnimList.empty()) {
                            int tapped = (int)((touchStartPy - box2Y + refAnimScrollY) / 16.0f);
                            if (tapped >= 0 && tapped < (int)refAnimList.size()) {
                                refAnimSliderIndex = tapped;
                                curRefAnimIndex = refAnimSliderIndex;
                                playCurRefAnim();
                                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                            }
                        }
                    }
                    else if (touchStartPy >= checkShowY - 4.0f && touchStartPy <= checkShowY + 18.0f) {
                        curSelected = 2;
                        showRefChar = !showRefChar;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                    }
                    else if (touchStartPy >= alphaY - 4.0f && touchStartPy <= alphaY + 28.0f) {
                        curSelected = 3;
                        float sliderX = 15.0f;
                        float sliderW = 290.0f;
                        if (touchStartPx >= sliderX - 5.0f && touchStartPx <= sliderX + sliderW + 5.0f) {
                            float pct = (touchStartPx - sliderX) / sliderW;
                            if (pct < 0.0f) pct = 0.0f;
                            if (pct > 1.0f) pct = 1.0f;
                            refAlpha = pct;
                            if (refObj) refObj->alpha = refAlpha;
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                    else if (touchStartPy >= posX_Y - 4.0f && touchStartPy <= posX_Y + 18.0f) {
                        curSelected = 4;
                        if (refObj) {
                            if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) refObj->x -= change;
                            else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) refObj->x += change;
                            refObj->baseX = refObj->x;
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                    else if (touchStartPy >= posY_Y - 4.0f && touchStartPy <= posY_Y + 18.0f) {
                        curSelected = 5;
                        if (refObj) {
                            if (touchStartPx >= arrowL - 10.0f && touchStartPx <= arrowL + 25.0f) refObj->y -= change;
                            else if (touchStartPx >= arrowR - 10.0f && touchStartPx <= arrowR + 25.0f) refObj->y += change;
                            refObj->baseY = refObj->y;
                            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.4f);
                        }
                    }
                }
            }
        }
    }
    }

    touchHeldLastFrame = touchHeld;
}

void CharacterEditorState::centerCameraOnTarget() {
    if (!currentStage) return;

    float bfStageX = currentStage->bfX;
    float bfStageY = currentStage->bfY;
    float dadStageX = currentStage->dadX;
    float dadStageY = currentStage->dadY;

    float mainStageX = (charObj && charObj->isPlayer) ? bfStageX : dadStageX;
    float mainStageY = (charObj && charObj->isPlayer) ? bfStageY : dadStageY;
    float refStageX = (charObj && charObj->isPlayer) ? dadStageX : bfStageX;
    float refStageY = (charObj && charObj->isPlayer) ? dadStageY : bfStageY;

    if (dragTargetMode == 1 && refObj) {
        bool isP = refObj->isPlayer;
        float stageCamX = isP ? currentStage->bfCamX : currentStage->dadCamX;
        float stageCamY = isP ? currentStage->bfCamY : currentStage->dadCamY;
        float refWorldX = refStageX + refObj->x;
        float refWorldY = refStageY + refObj->y;
        camX = refWorldX + 150.0f + refObj->camOffsetX + stageCamX;
        camY = refWorldY + 150.0f + refObj->camOffsetY + stageCamY;
    } else if (charObj) {
        bool isP = charObj->isPlayer;
        float stageCamX = isP ? currentStage->bfCamX : currentStage->dadCamX;
        float stageCamY = isP ? currentStage->bfCamY : currentStage->dadCamY;
        float mainWorldX = mainStageX + charObj->x;
        float mainWorldY = mainStageY + charObj->y;
        camX = mainWorldX + 150.0f + charObj->camOffsetX + stageCamX;
        camY = mainWorldY + 150.0f + charObj->camOffsetY + stageCamY;
    } else {
        camX = (mainStageX + refStageX) * 0.5f;
        camY = (mainStageY + refStageY) * 0.5f - 100.0f;
    }
    camZoom = currentStage->defaultZoom;
}

void CharacterEditorState::saveCharacter() {
    if (charObj && !currentCharacter.empty()) {
        float oldX = charObj->x;
        float oldY = charObj->y;
        float oldBaseX = charObj->baseX;
        float oldBaseY = charObj->baseY;

        if (currentStage) {
            float activeStageX = charObj->isPlayer ? currentStage->bfX : currentStage->dadX;
            float activeStageY = charObj->isPlayer ? currentStage->bfY : currentStage->dadY;
            charObj->x -= activeStageX;
            charObj->y -= activeStageY;
            charObj->baseX = charObj->x;
            charObj->baseY = charObj->y;
        }

        std::string path = "sdmc:/SnakeEngine/characters/" + currentCharacter + ".json";
        charObj->saveToPsychJson(path);

        charObj->x = oldX;
        charObj->y = oldY;
        charObj->baseX = oldBaseX;
        charObj->baseY = oldBaseY;

        saveMessageTimer = 3.0f;
    }
}

CharacterEditorState::~CharacterEditorState() {
    if (charObj) delete charObj;
    if (ghostObj) delete ghostObj;
    if (refObj) delete refObj;
    if (currentStage) delete currentStage;
    if (textBuf) C2D_TextBufDelete(textBuf);
}

void CharacterEditorState::init() {
    MusicBeatState::init();
    vcrFont = globalVCRFont;
    if (!vcrFont) {
        vcrFont = C2D_FontLoad("romfs:/fonts/vcr.bcfnt");
    }
    if (!textBuf) {
        textBuf = C2D_TextBufNew(8192);
    }
    currentStage = new Stage(Paths::stageJson("stage"));

    loadCharacter(currentCharacter);
    std::string defaultRef = "dad";
    if (std::find(characterList.begin(), characterList.end(), "dad") == characterList.end()) {
        if (characterList.size() > 1) defaultRef = characterList[1];
        else defaultRef = currentCharacter;
    }
    loadRefCharacter(defaultRef);

    centerCameraOnTarget();
}

void CharacterEditorState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    if (textBuf) C2D_TextBufClear(textBuf);

    // Sanitize animation selection bounds to prevent crashes
    if (!animList.empty()) {
        if (curAnimIndex < 0) curAnimIndex = 0;
        if (curAnimIndex >= (int)animList.size()) curAnimIndex = (int)animList.size() - 1;
        if (animSliderIndex < 0) animSliderIndex = 0;
        if (animSliderIndex >= (int)animList.size()) animSliderIndex = (int)animList.size() - 1;
    }

    // 1. TOP SCREEN (400x240): Canvas & Character Rendering
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(18, 18, 24, 255));

    // Top Header Bar
    C2D_DrawRectSolid(0, 0, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
    C2D_DrawRectSolid(0, 21, 0.81f, 400, 1, C2D_Color32(0, 210, 255, 255));
    drawText("CHARACTER EDITOR - " + currentCharacter, 12, 4, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.88f);

    std::string zoomHUD = "Zoom: " + std::to_string(camZoom).substr(0,4) + "x | Cam: (" + std::to_string((int)camX) + "," + std::to_string((int)camY) + ")";
    drawText(zoomHUD, 388, 4, 0.34f, false, C2D_Color32(190, 200, 215, 220), 0.88f, 0.0f, true);

    // Canvas Center Crosshair (subtle lines)
    C2D_DrawLine(200, 22, C2D_Color32(255, 0, 0, 65), 200, 240, C2D_Color32(255, 0, 0, 65), 1.0f, 0.0f);
    C2D_DrawLine(0, 120, C2D_Color32(255, 0, 0, 65), 400, 120, C2D_Color32(255, 0, 0, 65), 1.0f, 0.0f);

    float screenScale = 240.0f / 720.0f;

    auto* cs = SpritesheetCache::get().load("stages/stage");
    if (cs && !cs->frames.empty()) {
        const Frame& f = cs->frames[0];
        C2D_Image img = { f.tex, &f.uv };
        float scale = 3.5f;
        float drawScaleX = scale * screenScale * camZoom;
        float drawScaleY = scale * screenScale * camZoom;

        float drawX = ((24.0f - camX) * camZoom * screenScale) + 200.0f;
        float drawY = ((-124.0f - camY) * camZoom * screenScale) + 120.0f;

        float originX = (f.frameW > 0 ? (float)f.frameW : (float)f.w) / 2.0f;
        float originY = (f.frameH > 0 ? (float)f.frameH : (float)f.h) / 2.0f;
        if (originX <= 0.0f) originX = 512.0f;
        if (originY <= 0.0f) originY = 280.0f;

        drawX += originX * (1.0f - scale) * screenScale * camZoom;
        drawY += originY * (1.0f - scale) * screenScale * camZoom;

        C2D_DrawImageAt(img, drawX, drawY, 0.10f, nullptr, drawScaleX, drawScaleY);
    } else if (currentStage) {
        currentStage->draw(camX, camY, camZoom, false);
    }

    float bfStageX = currentStage ? currentStage->bfX : 0.0f;
    float bfStageY = currentStage ? currentStage->bfY : 0.0f;
    float dadStageX = currentStage ? currentStage->dadX : 0.0f;
    float dadStageY = currentStage ? currentStage->dadY : 0.0f;

    float mainStageX = (charObj && charObj->isPlayer) ? bfStageX : dadStageX;
    float mainStageY = (charObj && charObj->isPlayer) ? bfStageY : dadStageY;
    float refStageX = (refObj && refObj->isPlayer) ? bfStageX : dadStageX;
    float refStageY = (refObj && refObj->isPlayer) ? bfStageY : dadStageY;

    if (showGhost && ghostObj) {
        ghostObj->draw(mainStageX, mainStageY, 0.5f, camZoom, camX, camY);
    }
    if (showRefChar && refObj) {
        refObj->draw(refStageX, refStageY, 0.55f, camZoom, camX, camY);
    }
    if (charObj) {
        charObj->draw(mainStageX, mainStageY, 0.6f, camZoom, camX, camY);
    }



    // Camera Follow Target Scope (green circular target)
    Character* targetChar = (dragTargetMode == 1 && refObj) ? refObj : charObj;
    if (targetChar) {
        bool targetIsPlayer = (dragTargetMode == 1 && refObj) ? refObj->isPlayer : (charObj ? charObj->isPlayer : true);
        float rawTargetStageX = targetIsPlayer ? (currentStage ? currentStage->bfX : 0.0f) : (currentStage ? currentStage->dadX : 0.0f);
        float rawTargetStageY = targetIsPlayer ? (currentStage ? currentStage->bfY : 0.0f) : (currentStage ? currentStage->dadY : 0.0f);
        float targetStageCamX = targetIsPlayer ? (currentStage ? currentStage->bfCamX : 0.0f) : (currentStage ? currentStage->dadCamX : 0.0f);
        float targetStageCamY = targetIsPlayer ? (currentStage ? currentStage->bfCamY : 0.0f) : (currentStage ? currentStage->dadCamY : 0.0f);

        float targetWorldX = rawTargetStageX + targetChar->x + 150.0f + targetChar->camOffsetX + targetStageCamX;
        float targetWorldY = rawTargetStageY + targetChar->y + 150.0f + targetChar->camOffsetY + targetStageCamY;
        float screenFollowX = ((targetWorldX - camX) * screenScale * camZoom) + 200.0f;
        float screenFollowY = ((targetWorldY - camY) * screenScale * camZoom) + 120.0f;

        float armLenScreen = 24.0f * screenScale * camZoom;
        float dotRadScreen = 5.0f * screenScale * camZoom;
        if (dotRadScreen < 1.0f) dotRadScreen = 1.0f;
        if (armLenScreen < 2.0f) armLenScreen = 2.0f;

        C2D_DrawCircleSolid(screenFollowX, screenFollowY, 0.35f, dotRadScreen, C2D_Color32(0, 255, 0, 255));
        C2D_DrawLine(screenFollowX - armLenScreen, screenFollowY, C2D_Color32(0, 255, 0, 200), screenFollowX + armLenScreen, screenFollowY, C2D_Color32(0, 255, 0, 200), 1.0f, 0.35f);
        C2D_DrawLine(screenFollowX, screenFollowY - armLenScreen, C2D_Color32(0, 255, 0, 200), screenFollowX, screenFollowY + armLenScreen, C2D_Color32(0, 255, 0, 200), 1.0f, 0.35f);

        // Simulated Camera Viewport Box (Golden Yellow)
        if (showCamBounds) {
            float rectW = (1280.0f / simCamZoom) * screenScale * camZoom;
            float rectH = (720.0f / simCamZoom) * screenScale * camZoom;
            float rectX = screenFollowX - rectW * 0.5f;
            float rectY = screenFollowY - rectH * 0.5f;
            u32 camBoxCol = C2D_Color32(255, 215, 0, 220);

            C2D_DrawLine(rectX, rectY, camBoxCol, rectX + rectW, rectY, camBoxCol, 1.0f, 0.36f);
            C2D_DrawLine(rectX, rectY, camBoxCol, rectX, rectY + rectH, camBoxCol, 1.0f, 0.36f);
            C2D_DrawLine(rectX + rectW, rectY, camBoxCol, rectX + rectW, rectY + rectH, camBoxCol, 1.0f, 0.36f);
            C2D_DrawLine(rectX, rectY + rectH, camBoxCol, rectX + rectW, rectY + rectH, camBoxCol, 1.0f, 0.36f);

            char camZoomStr[32];
            snprintf(camZoomStr, sizeof(camZoomStr), "CAM FRAME (%.2fx)", simCamZoom);
            drawText(camZoomStr, rectX + 4.0f, rectY + 2.0f, 0.26f, false, camBoxCol, 0.37f);
        }
    }

    // 2. BOTTOM SCREEN (320x240): Tab UI or PC Mode Split
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(18, 18, 22, 255));


    if (pcMode) {
        float previewX = 0.0f;
        float previewW = 320.0f;
        float previewCenterX = 160.0f;

        C2D_DrawRectSolid(previewX, 0.0f, 0.01f, previewW, 240.0f, C2D_Color32(22, 24, 32, 255));

        for (float gx = 0.0f; gx <= 320.0f; gx += 20.0f) {
            C2D_DrawLine(gx, 14.0f, C2D_Color32(70, 70, 82, 45), gx, 226.0f, C2D_Color32(70, 70, 82, 45), 1.0f, 0.02f);
        }
        for (float gy = 34.0f; gy <= 226.0f; gy += 20.0f) {
            C2D_DrawLine(0.0f, gy, C2D_Color32(70, 70, 82, 45), 320.0f, gy, C2D_Color32(70, 70, 82, 45), 1.0f, 0.02f);
        }

        C2D_DrawLine(previewCenterX, 14.0f, C2D_Color32(255, 0, 0, 55), previewCenterX, 226.0f, C2D_Color32(255, 0, 0, 55), 1.0f, 0.03f);
        C2D_DrawLine(0.0f, 120.0f, C2D_Color32(255, 0, 0, 55), 320.0f, 120.0f, C2D_Color32(255, 0, 0, 55), 1.0f, 0.03f);

        float pcStageX = -40.0f / (screenScale * camZoom);
        float bfStageX = (currentStage ? currentStage->bfX : 0.0f) + pcStageX;
        float bfStageY = currentStage ? currentStage->bfY : 0.0f;
        float dadStageX = (currentStage ? currentStage->dadX : 0.0f) + pcStageX;
        float dadStageY = currentStage ? currentStage->dadY : 0.0f;

        float mainStageX = (charObj && charObj->isPlayer) ? bfStageX : dadStageX;
        float mainStageY = (charObj && charObj->isPlayer) ? bfStageY : dadStageY;
        float refStageX = (refObj && refObj->isPlayer) ? bfStageX : dadStageX;
        float refStageY = (refObj && refObj->isPlayer) ? bfStageY : dadStageY;

        auto* cs = SpritesheetCache::get().load("stages/stage");
        if (cs && !cs->frames.empty()) {
            const Frame& f = cs->frames[0];
            C2D_Image img = { f.tex, &f.uv };
            float scale = 3.5f;
            float drawScaleX = scale * screenScale * camZoom;
            float drawScaleY = scale * screenScale * camZoom;

            float drawX = ((24.0f - camX) * camZoom * screenScale) + 160.0f;
            float drawY = ((-124.0f - camY) * camZoom * screenScale) + 120.0f;

            float originX = (f.frameW > 0 ? (float)f.frameW : (float)f.w) / 2.0f;
            float originY = (f.frameH > 0 ? (float)f.frameH : (float)f.h) / 2.0f;
            if (originX <= 0.0f) originX = 512.0f;
            if (originY <= 0.0f) originY = 280.0f;

            drawX += originX * (1.0f - scale) * screenScale * camZoom;
            drawY += originY * (1.0f - scale) * screenScale * camZoom;

            C2D_DrawImageAt(img, drawX, drawY, 0.10f, nullptr, drawScaleX, drawScaleY);
        } else if (currentStage) {
            currentStage->draw(camX, camY, camZoom, false, -40.0f, 0.0f);
        }

        if (showGhost && ghostObj) {
            ghostObj->draw(mainStageX, mainStageY, 0.3f, camZoom, camX, camY);
        }
        if (showRefChar && refObj) {
            refObj->draw(refStageX, refStageY, 0.35f, camZoom, camX, camY);
        }
        if (charObj) {
            charObj->draw(mainStageX, mainStageY, 0.4f, camZoom, camX, camY);
        }



        Character* targetChar = (dragTargetMode == 1 && refObj) ? refObj : charObj;
        if (targetChar) {
            bool targetIsPlayer = (dragTargetMode == 1 && refObj) ? refObj->isPlayer : (charObj ? charObj->isPlayer : true);
            float rawTargetStageX = targetIsPlayer ? (currentStage ? currentStage->bfX : 0.0f) : (currentStage ? currentStage->dadX : 0.0f);
            float rawTargetStageY = targetIsPlayer ? (currentStage ? currentStage->bfY : 0.0f) : (currentStage ? currentStage->dadY : 0.0f);
            float targetStageCamX = targetIsPlayer ? (currentStage ? currentStage->bfCamX : 0.0f) : (currentStage ? currentStage->dadCamX : 0.0f);
            float targetStageCamY = targetIsPlayer ? (currentStage ? currentStage->bfCamY : 0.0f) : (currentStage ? currentStage->dadCamY : 0.0f);

            float targetWorldX = rawTargetStageX + targetChar->x + 150.0f + targetChar->camOffsetX + targetStageCamX;
            float targetWorldY = rawTargetStageY + targetChar->y + 150.0f + targetChar->camOffsetY + targetStageCamY;
            float pcFollowX = ((targetWorldX - camX) * screenScale * camZoom) + previewCenterX;
            float pcFollowY = ((targetWorldY - camY) * screenScale * camZoom) + 120.0f;

            float armLenScreen = 24.0f * screenScale * camZoom;
            float dotRadScreen = 5.0f * screenScale * camZoom;
            if (dotRadScreen < 1.0f) dotRadScreen = 1.0f;
            if (armLenScreen < 2.0f) armLenScreen = 2.0f;

            C2D_DrawCircleSolid(pcFollowX, pcFollowY, 0.35f, dotRadScreen, C2D_Color32(0, 255, 0, 255));
            C2D_DrawLine(pcFollowX - armLenScreen, pcFollowY, C2D_Color32(0, 255, 0, 200), pcFollowX + armLenScreen, pcFollowY, C2D_Color32(0, 255, 0, 200), 1.0f, 0.35f);
            C2D_DrawLine(pcFollowX, pcFollowY - armLenScreen, C2D_Color32(0, 255, 0, 200), pcFollowX, pcFollowY + armLenScreen, C2D_Color32(0, 255, 0, 200), 1.0f, 0.35f);

            if (showCamBounds) {
                float rectW = (1280.0f / simCamZoom) * screenScale * camZoom;
                float rectH = (720.0f / simCamZoom) * screenScale * camZoom;
                float rectX = pcFollowX - rectW * 0.5f;
                float rectY = pcFollowY - rectH * 0.5f;
                u32 camBoxCol = C2D_Color32(255, 215, 0, 220);

                C2D_DrawLine(rectX, rectY, camBoxCol, rectX + rectW, rectY, camBoxCol, 1.0f, 0.36f);
                C2D_DrawLine(rectX, rectY, camBoxCol, rectX, rectY + rectH, camBoxCol, 1.0f, 0.36f);
                C2D_DrawLine(rectX + rectW, rectY, camBoxCol, rectX + rectW, rectY + rectH, camBoxCol, 1.0f, 0.36f);
                C2D_DrawLine(rectX, rectY + rectH, camBoxCol, rectX + rectW, rectY + rectH, camBoxCol, 1.0f, 0.36f);
            }
        }

        float headerH = 14.0f * pcUiScale;
        float footerH = 14.0f * pcUiScale;

        if (showAnimOverlay && charObj && !animList.empty()) {
            float ovX = 4.0f;
            float ovY = headerH + 3.0f;
            float ovScale = 0.26f * pcUiScale;
            float ovLineH = 10.5f * pcUiScale;
            int maxVisible = (int)(((240.0f - footerH) - ovY) / ovLineH);
            if (maxVisible < 1) maxVisible = 1;
            int count = (int)animList.size();
            int showCount = count < maxVisible ? count : maxVisible;

            u32 outlineCol = C2D_Color32(0, 0, 0, 255);

            for (int ai = 0; ai < showCount; ai++) {
                const std::string& aName = animList[ai];
                float ox = charObj->animations[aName].offsetX;
                float oy = charObj->animations[aName].offsetY;

                std::string displayName = (aName.length() > 9) ? (aName.substr(0, 8) + "~") : aName;
                char animLine[40];
                snprintf(animLine, sizeof(animLine), "%s: [%d,%d]", displayName.c_str(), (int)ox, (int)oy);

                u32 animLineCol = (ai == curAnimIndex)
                    ? C2D_Color32(0, 210, 255, 255)
                    : C2D_Color32(230, 230, 240, 255);

                float ly = ovY + ai * ovLineH;
                // Black 4-way outline
                drawText(animLine, ovX - 1.0f, ly, ovScale, false, outlineCol, 0.44f);
                drawText(animLine, ovX + 1.0f, ly, ovScale, false, outlineCol, 0.44f);
                drawText(animLine, ovX, ly - 1.0f, ovScale, false, outlineCol, 0.44f);
                drawText(animLine, ovX, ly + 1.0f, ovScale, false, outlineCol, 0.44f);
                // Main colored text
                drawText(animLine, ovX, ly, ovScale, false, animLineCol, 0.45f);
            }
        }

        // Preview Header bar
        C2D_DrawRectSolid(0.0f, 0.0f, 0.62f, 320.0f, headerH, C2D_Color32(10, 10, 16, 230));
        C2D_DrawRectSolid(0.0f, headerH - 1.0f, 0.63f, 320.0f, 1.0f, C2D_Color32(0, 210, 255, 200));
        {
            std::string headerStr = currentCharacter;
            if (!animList.empty() && curAnimIndex < (int)animList.size()) {
                std::string animPart = animList[curAnimIndex];
                if (animPart.length() > 12) animPart = animPart.substr(0, 11) + "~";
                headerStr += "  |  " + animPart;
            }
            drawText(headerStr, 160.0f, headerH * 0.5f, 0.28f * pcUiScale, true, C2D_Color32(220, 225, 235, 255), 0.65f, 240.0f, false, true);
        }

        // Toggle Animation List Overlay button
        float btnBoxW = 16.0f * pcUiScale;
        float btnBoxH = 12.0f * pcUiScale;
        C2D_DrawRectSolid(3.0f, 1.0f, 0.64f, btnBoxW, btnBoxH, C2D_Color32(30, 30, 45, 255));
        drawText(showAnimOverlay ? "<-" : "->", 3.0f + btnBoxW * 0.5f, 1.0f + btnBoxH * 0.5f, 0.28f * pcUiScale, true, C2D_Color32(0, 210, 255, 255), 0.65f, 0.0f, false, true);

        // Toggle Drag Target (Player P / Reference R) button
        float btn2X = 3.0f + btnBoxW + 3.0f;
        u32 prBg = (dragTargetMode == 1) ? C2D_Color32(60, 40, 20, 255) : C2D_Color32(30, 30, 45, 255);
        u32 prTxtCol = (dragTargetMode == 1) ? C2D_Color32(255, 170, 0, 255) : C2D_Color32(0, 210, 255, 255);
        C2D_DrawRectSolid(btn2X, 1.0f, 0.64f, btnBoxW, btnBoxH, prBg);
        drawText(dragTargetMode == 1 ? "R" : "P", btn2X + btnBoxW * 0.5f, 1.0f + btnBoxH * 0.5f, 0.28f * pcUiScale, true, prTxtCol, 0.65f, 0.0f, false, true);

        // UI Scale Stepper control [-] SCALE: 1.0x [+]
        float stW = 112.0f * pcUiScale;
        float stX = 320.0f - stW - 3.0f;
        C2D_DrawRectSolid(stX, 1.0f, 0.64f, stW, btnBoxH, C2D_Color32(30, 30, 45, 255));
        C2D_DrawRectSolid(stX + 1.0f, 2.0f, 0.65f, 16.0f * pcUiScale, btnBoxH - 2.0f, C2D_Color32(45, 50, 70, 255));
        drawText("-", stX + 8.0f * pcUiScale, 1.0f + btnBoxH * 0.5f, 0.28f * pcUiScale, true, C2D_Color32(0, 210, 255, 255), 0.66f, 0.0f, false, true);
        
        char scaleStr[24];
        snprintf(scaleStr, sizeof(scaleStr), "SCALE:%.2fx", pcUiScale);
        drawText(scaleStr, stX + stW * 0.5f, 1.0f + btnBoxH * 0.5f, 0.24f * pcUiScale, true, CWhite, 0.66f, 0.0f, false, true);

        C2D_DrawRectSolid(stX + stW - 16.0f * pcUiScale, 2.0f, 0.65f, 15.0f * pcUiScale, btnBoxH - 2.0f, C2D_Color32(45, 50, 70, 255));
        drawText("+", stX + stW - 8.0f * pcUiScale, 1.0f + btnBoxH * 0.5f, 0.28f * pcUiScale, true, C2D_Color32(0, 210, 255, 255), 0.66f, 0.0f, false, true);

        // Preview Footer bar & PC Mode Taskbar
        float footerY = 240.0f - footerH;
        C2D_DrawRectSolid(0.0f, footerY, 0.62f, 320.0f, footerH, C2D_Color32(10, 10, 16, 230));
        C2D_DrawRectSolid(0.0f, footerY, 0.63f, 320.0f, 1.0f, C2D_Color32(0, 210, 255, 200));

        if (pcMode && uiExpanded) {
            // Windows Taskbar items on left side
            struct TaskbarItem { int id; std::string name; float x, w; };
            float tX0 = 3.0f;
            float tW0 = 24.0f * pcUiScale;
            float tW1 = 30.0f * pcUiScale;
            float tW2 = 26.0f * pcUiScale;
            float tW3 = 28.0f * pcUiScale;
            float tW4 = 25.0f * pcUiScale;
            float tW5 = 24.0f * pcUiScale;
            TaskbarItem taskbarItems[] = {
                { 0, "Set", tX0, tW0 },
                { 1, "Ghost", tX0 + tW0 + 2.0f, tW1 },
                { 2, "Char", tX0 + tW0 + tW1 + 4.0f, tW2 },
                { 3, "Anim", tX0 + tW0 + tW1 + tW2 + 6.0f, tW3 },
                { 4, "Cam", tX0 + tW0 + tW1 + tW2 + tW3 + 8.0f, tW4 },
                { 5, "Ref", tX0 + tW0 + tW1 + tW2 + tW3 + tW4 + 10.0f, tW5 }
            };

            for (int i = 0; i < 6; i++) {
                const auto& item = taskbarItems[i];
                const UIWindow* winPtr = nullptr;
                for (const auto& w : windows) {
                    if (w.id == item.id) { winPtr = &w; break; }
                }
                if (!winPtr) continue;

                bool isMinimized = !winPtr->expanded;
                u32 btnBg = isMinimized ? C2D_Color32(35, 45, 65, 255) : C2D_Color32(20, 22, 28, 200);
                u32 txtCol = isMinimized ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(140, 145, 160, 255);
                u32 borderCol = isMinimized ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(50, 50, 65, 255);

                C2D_DrawRectSolid(item.x, footerY + 1.0f, 0.64f, item.w, footerH - 2.0f, btnBg);
                C2D_DrawLine(item.x, footerY + 1.0f, borderCol, item.x + item.w, footerY + 1.0f, borderCol, 1.0f, 0.65f);
                C2D_DrawLine(item.x, footerY + 1.0f, borderCol, item.x, footerY + footerH - 1.0f, borderCol, 1.0f, 0.65f);
                C2D_DrawLine(item.x + item.w, footerY + 1.0f, borderCol, item.x + item.w, footerY + footerH - 1.0f, borderCol, 1.0f, 0.65f);
                C2D_DrawLine(item.x, footerY + footerH - 1.0f, borderCol, item.x + item.w, footerY + footerH - 1.0f, borderCol, 1.0f, 0.65f);

                drawText(item.name, item.x + item.w * 0.5f, footerY + footerH * 0.5f, 0.25f * pcUiScale, true, txtCol, 0.66f, 0.0f, false, true);
            }
        }

        // Camera / Zoom info string right-aligned on bottom bar
        {
            char footerStr[48];
            int curFr = charObj ? charObj->curFrame : 0;
            int totalFr = 0;
            if (charObj && !animList.empty() && curAnimIndex < (int)animList.size()) {
                totalFr = (int)charObj->animations[animList[curAnimIndex]].indices.size();
            }
            if (totalFr > 0) {
                if (curFr < 0) curFr = 0;
                if (curFr >= totalFr) curFr = totalFr - 1;
                snprintf(footerStr, sizeof(footerStr), "Fr:%d/%d  Z:%.2fx", curFr + 1, totalFr, camZoom);
            } else {
                snprintf(footerStr, sizeof(footerStr), "Z:%.2fx  Cam:(%d,%d)", camZoom, (int)camX, (int)camY);
            }
            drawText(footerStr, 316.0f, footerY + footerH * 0.5f, 0.25f * pcUiScale, false, C2D_Color32(190, 200, 215, 220), 0.65f, 0.0f, true, true);
        }
    }

    if (pcMode) {
        if (uiExpanded) {
            auto layout = getPCLayout(windows, pcUiScale);
            std::vector<int> visibleOptionIds;
            for (const auto& elem : layout) {
                if (elem.id != -1) {
                    visibleOptionIds.push_back(elem.id);
                }
            }

            for (size_t winIdx = 0; winIdx < windows.size(); winIdx++) {
                const auto& win = windows[winIdx];
                if (!win.expanded) continue;

                float winH = win.h;
                bool isFocused = (winIdx == windows.size() - 1);
                float winZ = 0.40f + winIdx * 0.05f;

                C2D_DrawRectSolid(win.x + 1.5f, win.y + 1.5f, winZ, win.w, winH, C2D_Color32(0, 0, 0, 75));
                C2D_DrawRectSolid(win.x, win.y, winZ + 0.005f, win.w, winH, C2D_Color32(22, 24, 32, 245));
                
                float titleH = 12.0f * pcUiScale;
                u32 titleTopCol = isFocused ? C2D_Color32(35, 45, 60, 255) : C2D_Color32(25, 27, 34, 255);
                u32 titleBotCol = isFocused ? C2D_Color32(20, 30, 45, 255) : C2D_Color32(18, 19, 24, 255);
                C2D_DrawRectangle(win.x, win.y, winZ + 0.010f, win.w, titleH, titleTopCol, titleTopCol, titleBotCol, titleBotCol);

                u32 borderCol = isFocused ? C2D_Color32(0, 210, 255, 255) : C2D_Color32(65, 65, 80, 255);
                C2D_DrawLine(win.x, win.y + titleH - 1.0f, borderCol, win.x + win.w, win.y + titleH - 1.0f, borderCol, 1.0f, winZ + 0.012f);
                C2D_DrawLine(win.x, win.y, borderCol, win.x + win.w, win.y, borderCol, 1.0f, winZ + 0.012f);
                C2D_DrawLine(win.x, win.y, borderCol, win.x, win.y + winH, borderCol, 1.0f, winZ + 0.012f);
                C2D_DrawLine(win.x + win.w, win.y, borderCol, win.x + win.w, win.y + winH, borderCol, 1.0f, winZ + 0.012f);
                C2D_DrawLine(win.x, win.y + winH, borderCol, win.x + win.w, win.y + winH, borderCol, 1.0f, winZ + 0.012f);
                
                float winTitleTs = 0.26f * pcUiScale;
                drawText(win.title, win.x + 6.0f * pcUiScale, win.y + titleH * 0.5f, winTitleTs, false, isFocused ? CWhite : C2D_Color32(180, 180, 195, 255), winZ + 0.015f, 0.0f, false, true);
                drawText(win.expanded ? "[-]" : "[+]", win.x + win.w - 14.0f * pcUiScale, win.y + titleH * 0.5f, winTitleTs, false, C2D_Color32(0, 210, 255, 255), winZ + 0.015f, 0.0f, false, true);

                if (win.expanded) {
                    C2D_DrawLine(win.x + win.w - 8.0f, win.y + win.h - 2.0f, borderCol, win.x + win.w - 2.0f, win.y + win.h - 8.0f, borderCol, 1.0f, winZ + 0.02f);
                    C2D_DrawLine(win.x + win.w - 5.0f, win.y + win.h - 2.0f, borderCol, win.x + win.w - 2.0f, win.y + win.h - 5.0f, borderCol, 1.0f, winZ + 0.02f);

                    for (const auto& elem : layout) {
                        bool belongs = false;
                        if (win.id == 0 && (elem.id >= 0 && elem.id <= 3)) belongs = true;
                        else if (win.id == 1 && (elem.id >= 4 && elem.id <= 7)) belongs = true;
                        else if (win.id == 2 && (elem.id >= 8 && elem.id <= 15)) belongs = true;
                        else if (win.id == 3 && (elem.id >= 16 && elem.id <= 20)) belongs = true;
                        else if (win.id == 4 && (elem.id >= 21 && elem.id <= 22)) belongs = true;
                        else if (win.id == 5 && (elem.id >= 23 && elem.id <= 29)) belongs = true;

                        if (belongs) {
                            bool isSelected = (!visibleOptionIds.empty() && curSelected < (int)visibleOptionIds.size() && visibleOptionIds[curSelected] == elem.id);

                            if (elem.id == 0) {
                                renderListBox(elem.x, elem.y, elem.w, elem.h, characterList, curCharIndex, charScrollY, isSelected, winZ + 0.025f);
                            } else if (elem.id == 1) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Playable", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, charObj->isPlayer, isSelected, winZ + 0.025f);
                            } else if (elem.id == 2) {
                                renderButton(elem.x, elem.y, elem.w, elem.h, "RELOAD", isSelected, winZ + 0.025f);
                            } else if (elem.id == 3) {
                                renderButton(elem.x, elem.y, elem.w, elem.h, "SAVE", isSelected, winZ + 0.025f);
                            } else if (elem.id == 4) {
                                renderButton(elem.x, elem.y, elem.w, elem.h, "MAKE GHOST", isSelected, winZ + 0.025f);
                            } else if (elem.id == 5) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Show Ghost", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, showGhost, isSelected, winZ + 0.025f);
                            } else if (elem.id == 6) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Highlight", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, ghostHighlight, isSelected, winZ + 0.025f);
                            } else if (elem.id == 7) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Alpha: " + fmtFloat(ghostAlpha, 2), elem.x + 4.0f * pcUiScale, elem.y + 4.0f * pcUiScale, 0.24f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderSlider(elem.x + 4.0f * pcUiScale, elem.y + 9.0f * pcUiScale, elem.w - 8.0f * pcUiScale, ghostAlpha, isSelected, winZ + 0.025f);
                            } else if (elem.id == 10) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Flip X", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, charObj->flipX, isSelected, winZ + 0.025f);
                            } else if (elem.id == 11) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Antialiasing", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, !charObj->noAntialiasing, isSelected, winZ + 0.025f);
                            } else if (elem.id == 16) {
                                if (animList.empty()) {
                                    drawText("No Anims Loaded", elem.x + 4.0f * pcUiScale, elem.y + 12.0f * pcUiScale, 0.24f * pcUiScale, false, C2D_Color32(255, 100, 100, 255), winZ + 0.025f);
                                } else {
                                    renderListBox(elem.x, elem.y, elem.w, elem.h, animList, animSliderIndex, animScrollY, isSelected, winZ + 0.025f);
                                }
                            } else if (elem.id == 18) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Loop", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                bool loopVal = (!animList.empty() && animSliderIndex < (int)animList.size()) ? charObj->animations[animList[animSliderIndex]].loop : false;
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, loopVal, isSelected, winZ + 0.025f);
                            } else if (elem.id == 21) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Enable Frame", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, showCamBounds, isSelected, winZ + 0.025f);
                            } else if (elem.id == 23) {
                                renderListBox(elem.x, elem.y, elem.w, elem.h, characterList, curRefCharIndex, refCharScrollY, isSelected, winZ + 0.025f);
                            } else if (elem.id == 29) {
                                if (refAnimList.empty()) {
                                    drawText("No Ref Anims", elem.x + 4.0f * pcUiScale, elem.y + 8.0f * pcUiScale, 0.24f * pcUiScale, false, C2D_Color32(255, 100, 100, 255), winZ + 0.025f);
                                } else {
                                    renderListBox(elem.x, elem.y, elem.w, elem.h, refAnimList, refAnimSliderIndex, refAnimScrollY, isSelected, winZ + 0.025f);
                                }
                            } else if (elem.id == 24) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Show Ref", elem.x + 4.0f * pcUiScale, elem.y + elem.h * 0.5f, 0.26f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderCheckbox(elem.x + elem.w - 12.0f * pcUiScale, elem.y + (elem.h - 7.0f * pcUiScale) * 0.5f, showRefChar, isSelected, winZ + 0.025f);
                            } else if (elem.id == 26) {
                                if (isSelected) renderLeftAccent(elem.x, elem.y, elem.h, winZ + 0.025f);
                                drawText("Ref Alpha: " + fmtFloat(refAlpha, 2), elem.x + 4.0f * pcUiScale, elem.y + 4.0f * pcUiScale, 0.24f * pcUiScale, false, CWhite, winZ + 0.025f, 0.0f, false, true);
                                renderSlider(elem.x + 4.0f * pcUiScale, elem.y + 9.0f * pcUiScale, elem.w - 8.0f * pcUiScale, refAlpha, isSelected, winZ + 0.025f);
                            } else {
                                std::string valStr = "";
                                if (elem.id == 8) valStr = fmtFloat(charObj->singDuration, 2);
                                else if (elem.id == 9) valStr = fmtFloat(charObj->charScale, 2);
                                else if (elem.id == 12) valStr = std::to_string((int)charObj->x);
                                else if (elem.id == 13) valStr = std::to_string((int)charObj->y);
                                else if (elem.id == 14) valStr = std::to_string((int)charObj->camOffsetX);
                                else if (elem.id == 15) valStr = std::to_string((int)charObj->camOffsetY);
                                else if (elem.id == 17) valStr = (animList.empty() || animSliderIndex >= (int)animList.size()) ? "0" : std::to_string(charObj->animations[animList[animSliderIndex]].fps);
                                else if (elem.id == 19) valStr = (animList.empty() || animSliderIndex >= (int)animList.size()) ? "0" : std::to_string((int)charObj->animations[animList[animSliderIndex]].offsetX);
                                else if (elem.id == 20) valStr = (animList.empty() || animSliderIndex >= (int)animList.size()) ? "0" : std::to_string((int)charObj->animations[animList[animSliderIndex]].offsetY);
                                else if (elem.id == 22) valStr = fmtFloat(simCamZoom, 2) + "x";
                                else if (elem.id == 27) valStr = std::to_string((int)(refObj ? refObj->x : 0));
                                else if (elem.id == 28) valStr = std::to_string((int)(refObj ? refObj->y : 0));

                                renderArrowsVal(elem.x, elem.y, elem.w, elem.label, valStr, isSelected, winZ + 0.025f);
                            }
                        }
                    }
                }
            }
        }
    } else {
        float tabW = 53.33f;
        std::string tabLabelsNormal[] = { "SET", "GHOST", "CHAR", "ANIM", "CAM", "REF" };

        C2D_DrawRectSolid(0, 0, 0.35f, 320, 24, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 23, 0.36f, 320, 1, C2D_Color32(0, 210, 255, 255));

        for (int i = 0; i < 6; i++) {
            bool isActive = (i == currentTab);
            u32 tabBg = isActive ? C2D_Color32(0, 110, 200, 255) : C2D_Color32(22, 24, 32, 255);

            C2D_DrawRectSolid(i * tabW, 0.0f, 0.40f, tabW, 23.0f, tabBg);
            C2D_DrawLine(i * tabW, 0.0f, C2D_Color32(50, 55, 75, 255), i * tabW, 23.0f, C2D_Color32(50, 55, 75, 255), 1.0f, 0.42f);

            if (isActive) {
                C2D_DrawRectSolid(i * tabW + 1, 21.0f, 0.45f, tabW - 2, 2.0f, C2D_Color32(0, 210, 255, 255));
            }

            drawText(tabLabelsNormal[i], i * tabW + tabW * 0.5f, 11.5f, 0.30f, true, isActive ? CWhite : C2D_Color32(170, 180, 200, 255), 0.50f, 0.0f, false, true);
        }

        if (currentTab == 0) {
            float boxX = 10.0f;
            float boxY = 28.0f;
            float boxW = 300.0f;
            float boxH = 82.0f;
            renderListBox(boxX, boxY, boxW, boxH, characterList, curCharIndex, charScrollY, curSelected == 0);
            
            float playY = 118.0f;
            drawText("Playable Character", 12.0f, playY + 10.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderCheckbox(320.0f - 25.0f, playY + 3.0f, charObj->isPlayer, curSelected == 1);
            
            float btnY1 = 145.0f;
            float btnY2 = 175.0f;
            float btnH = 24.0f;
            renderButton(10.0f, btnY1, 300.0f, btnH, "RELOAD CHARACTER", curSelected == 2);
            renderButton(10.0f, btnY2, 300.0f, btnH, "SAVE CHARACTER", curSelected == 3);
        }
        else if (currentTab == 1) {
            float btnY = 30.0f;
            float btnH = 26.0f;
            renderButton(10.0f, btnY, 300.0f, btnH, "MAKE GHOST FROM CURRENT", curSelected == 0);
            
            float checkShowY = 64.0f;
            float checkHighY = 88.0f;
            drawText("Show Ghost", 12.0f, checkShowY + 10.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderCheckbox(320.0f - 25.0f, checkShowY + 3.0f, showGhost, curSelected == 1);
            
            drawText("Highlight Ghost", 12.0f, checkHighY + 10.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderCheckbox(320.0f - 25.0f, checkHighY + 3.0f, ghostHighlight, curSelected == 2);
            
            float alphaY = 114.0f;
            float sliderX = 15.0f;
            float sliderY = 136.0f;
            float sliderW = 290.0f;
            drawText("Ghost Alpha: " + fmtFloat(ghostAlpha, 2), 12.0f, alphaY + 6.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderSlider(sliderX, sliderY, sliderW, ghostAlpha, curSelected == 3);
        }
        else if (currentTab == 2) {
            for (int i = 0; i < 8; i++) {
                float itemY = 28.0f + i * 24.0f;
                bool selected = (curSelected == i);
                if (i == 0) {
                    renderArrowsVal(0.0f, itemY, 320.0f, "Sing Length", fmtFloat(charObj->singDuration, 2), selected);
                } else if (i == 1) {
                    renderArrowsVal(0.0f, itemY, 320.0f, "Scale", fmtFloat(charObj->charScale, 2), selected);
                } else if (i == 2) {
                    drawText("Flip X", 12.0f, itemY + 12.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
                    renderCheckbox(320.0f - 25.0f, itemY + 5.0f, charObj->flipX, selected);
                } else if (i == 3) {
                    drawText("Antialiasing", 12.0f, itemY + 12.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
                    renderCheckbox(320.0f - 25.0f, itemY + 5.0f, !charObj->noAntialiasing, selected);
                } else if (i == 4) {
                    renderArrowsVal(0.0f, itemY, 320.0f, "Pos X", std::to_string((int)charObj->x), selected);
                } else if (i == 5) {
                    renderArrowsVal(0.0f, itemY, 320.0f, "Pos Y", std::to_string((int)charObj->y), selected);
                } else if (i == 6) {
                    renderArrowsVal(0.0f, itemY, 320.0f, "Cam Off X", std::to_string((int)charObj->camOffsetX), selected);
                } else if (i == 7) {
                    renderArrowsVal(0.0f, itemY, 320.0f, "Cam Off Y", std::to_string((int)charObj->camOffsetY), selected);
                }
            }
        }
        else if (currentTab == 3) {
            if (animList.empty()) {
                drawText("No Animations Loaded", 160.0f, 60.0f, 0.45f, true, C2D_Color32(255, 100, 100, 255), 0.5f, 0.0f, false, true);
            } else {
                float boxX = 10.0f;
                float boxY = 28.0f;
                float boxW = 300.0f;
                float boxH = 70.0f;
                renderListBox(boxX, boxY, boxW, boxH, animList, animSliderIndex, animScrollY, curSelected == 0);
                
                std::string curAnimName = (animSliderIndex < (int)animList.size()) ? animList[animSliderIndex] : "";
                if (!curAnimName.empty()) {
                    float symY = 102.0f;
                    float fpsY = 122.0f;
                    float loopY = 144.0f;
                    float offXY = 166.0f;
                    float offYY = 188.0f;
                    float indY = 210.0f;

                    std::string prefix = charObj->animations[curAnimName].prefix;
                    if (prefix.length() > 32) prefix = prefix.substr(0, 30) + "...";
                    drawText("Symbol: " + prefix, 12.0f, symY + 6.0f, 0.30f, false, C2D_Color32(200, 200, 220, 255), 0.5f, 0.0f, false, true);

                    renderArrowsVal(0.0f, fpsY, 320.0f, "FPS", std::to_string(charObj->animations[curAnimName].fps), curSelected == 1);
                    drawText("Loop", 12.0f, loopY + 10.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
                    renderCheckbox(320.0f - 25.0f, loopY + 3.0f, charObj->animations[curAnimName].loop, curSelected == 2);
                    renderArrowsVal(0.0f, offXY, 320.0f, "Offset X", std::to_string((int)charObj->animations[curAnimName].offsetX), curSelected == 3);
                    renderArrowsVal(0.0f, offYY, 320.0f, "Offset Y", std::to_string((int)charObj->animations[curAnimName].offsetY), curSelected == 4);

                    drawText("Indices: " + getIndicesString(charObj->animations[curAnimName].indices, pcMode), 12.0f, indY + 6.0f, 0.28f, false, C2D_Color32(180, 180, 200, 255), 0.5f, 0.0f, false, true);
                }
            }
        }
        else if (currentTab == 4) {
            float checkShowY = 40.0f;
            drawText("Enable Camera Frame", 12.0f, checkShowY + 10.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderCheckbox(320.0f - 25.0f, checkShowY + 3.0f, showCamBounds, curSelected == 0);

            float zoomY = 80.0f;
            renderArrowsVal(0.0f, zoomY, 320.0f, "Camera Zoom", fmtFloat(simCamZoom, 2) + "x", curSelected == 1);
        }
        else if (currentTab == 5) {
            float box1X = 10.0f, box1Y = 27.0f, box1W = 300.0f, box1H = 48.0f;
            renderListBox(box1X, box1Y, box1W, box1H, characterList, curRefCharIndex, refCharScrollY, curSelected == 0);

            float box2X = 10.0f, box2Y = 78.0f, box2W = 300.0f, box2H = 48.0f;
            if (refAnimList.empty()) {
                drawText("No Ref Anims Loaded", 160.0f, 98.0f, 0.35f, true, C2D_Color32(255, 100, 100, 255), 0.5f, 0.0f, false, true);
            } else {
                renderListBox(box2X, box2Y, box2W, box2H, refAnimList, refAnimSliderIndex, refAnimScrollY, curSelected == 1);
            }

            float checkShowY = 126.0f;
            float alphaY = 148.0f;
            float sliderX = 15.0f;
            float sliderY = 166.0f;
            float sliderW = 290.0f;
            float posX_Y = 184.0f;
            float posY_Y = 206.0f;

            drawText("Show Reference", 12.0f, checkShowY + 10.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderCheckbox(320.0f - 25.0f, checkShowY + 3.0f, showRefChar, curSelected == 2);

            drawText("Ref Alpha: " + fmtFloat(refAlpha, 2), 12.0f, alphaY + 6.0f, 0.35f, false, CWhite, 0.5f, 0.0f, false, true);
            renderSlider(sliderX, sliderY, sliderW, refAlpha, curSelected == 3);

            renderArrowsVal(0.0f, posX_Y, 320.0f, "Ref Pos X", std::to_string((int)(refObj ? refObj->x : 0)), curSelected == 4);
            renderArrowsVal(0.0f, posY_Y, 320.0f, "Ref Pos Y", std::to_string((int)(refObj ? refObj->y : 0)), curSelected == 5);
        }
    }

    if (saveMessageTimer > 0.0f) {
        float alpha = 1.0f;
        if (saveMessageTimer < 0.5f) {
            alpha = saveMessageTimer / 0.5f;
        }
        u32 bannerColor = C2D_Color32(16, 45, 30, (u8)(230 * alpha));
        u32 borderColor = C2D_Color32(30, 120, 70, (u8)(255 * alpha));
        u32 textColor = C2D_Color32(80, 240, 120, (u8)(255 * alpha));

        float bannerW = 220.0f;
        float bannerX = (320.0f - bannerW) * 0.5f;
        float bannerY = 100.0f;
        float bannerH = 40.0f;

        C2D_DrawRectSolid(bannerX, bannerY, 0.95f, bannerW, bannerH, bannerColor);
        C2D_DrawLine(bannerX, bannerY, borderColor, bannerX + bannerW, bannerY, borderColor, 1.0f, 0.96f);
        C2D_DrawLine(bannerX, bannerY, borderColor, bannerX, bannerY + bannerH, borderColor, 1.0f, 0.96f);
        C2D_DrawLine(bannerX + bannerW, bannerY, borderColor, bannerX + bannerW, bannerY + bannerH, borderColor, 1.0f, 0.96f);
        C2D_DrawLine(bannerX, bannerY + bannerH, borderColor, bannerX + bannerW, bannerY + bannerH, borderColor, 1.0f, 0.96f);
        
        drawText("CHARACTER SAVED!", bannerX + bannerW * 0.5f, bannerY + bannerH * 0.5f, 0.45f, true, textColor, 0.98f);
    }
}

void CharacterEditorState::exitState() {
    ModHandler::get().currentModFolder = "";
    switchState(new DebugMenuState());
}
