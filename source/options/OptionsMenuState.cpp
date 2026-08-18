#include "OptionsMenuState.hpp"
#include "MainMenuState.hpp"
#include "PlayState.hpp"
#include "../backend/AudioEngine.hpp"
#include <math.h>
#include <sstream>
#include <fstream>
#include "../objects/Alphabet.hpp"
#include "../objects/ButtonPrompt.hpp"
#include "CustomizeComboState.hpp"
#include "Highscores.hpp"
#include "Achievements.hpp"
#include <cmath>

bool OptionsMenuState::onPlayState = false;
bool OptionsMenuState::isStoryMode = false;
std::string OptionsMenuState::songName = "";
std::string OptionsMenuState::difficultyName = "";
WeekData OptionsMenuState::storyWeek = WeekData();
int OptionsMenuState::storySongIdx = 0;

static void drawBG(C2D_Image img, bool valid, float w, float h) {
    if (valid) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(39, 71, 220, 255), 1.0f);
        drawCenteredBG(img, w, h, 0.1f, &tint);
    }
}

static void drawCheckbox(float x, float y, const OptionsMenuState::CheckboxState& cb, float alpha) {
    CachedSpritesheet* cbSheet = SpritesheetCache::get().load("shared/images/checkboxanim");
    if (!cbSheet) return;

    std::string frameName = "checkbox0000";
    float customOffsetX = 0.0f;
    float customOffsetY = 0.0f;

    if (cb.currentAnim == "checked") {
        frameName = "checkbox finish0000";
        customOffsetX = 3.0f;
        customOffsetY = 12.0f;
    } else if (cb.currentAnim == "unchecked") {
        frameName = "checkbox0000";
        customOffsetX = 0.0f;
        customOffsetY = 2.0f;
    } else if (cb.currentAnim == "checking") {
        int idx = (int)(cb.animTime * 24.0f);
        if (idx >= 10) {
            frameName = "checkbox finish0000";
            customOffsetX = 3.0f;
            customOffsetY = 12.0f;
        } else {
            frameName = "checkbox anim000" + std::to_string(idx);
            customOffsetX = 34.0f;
            customOffsetY = 25.0f;
        }
    } else if (cb.currentAnim == "unchecking") {
        int idx = (int)(cb.animTime * 24.0f);
        if (idx >= 8) {
            frameName = "checkbox0000";
            customOffsetX = 0.0f;
            customOffsetY = 2.0f;
        } else {
            frameName = "checkbox anim reverse000" + std::to_string(idx);
            customOffsetX = 25.0f;
            customOffsetY = 28.0f;
        }
    }

    const Frame* foundFrame = nullptr;
    for (const auto& f : cbSheet->frames) {
        if (f.name == frameName) {
            foundFrame = &f;
            break;
        }
    }

    if (foundFrame) {
        C2D_Image img;
        img.tex = foundFrame->tex;
        img.subtex = &foundFrame->uv;

        float screenScale = 240.0f / 720.0f;
        float drawScale = 1.3f * screenScale;

        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        if (alpha < 1.0f) {
            C2D_AlphaImageTint(&tint, alpha);
            tintPtr = &tint;
        }

        float lx = x - (foundFrame->frameX + customOffsetX) * drawScale;
        float ly = y - (foundFrame->frameY + customOffsetY) * drawScale;

        if (foundFrame->rotated) {
            float angleRad = -(3.14159265f / 2.0f);
            float cx = lx + foundFrame->uv.height * drawScale / 2.0f;
            float cy = ly + foundFrame->uv.width  * drawScale / 2.0f;
            C2D_DrawImageAtRotated(img, cx, cy, 0.95f, angleRad, tintPtr, drawScale, drawScale);
        } else {
            drawImageScaledTinted(img, lx, ly, 0.95f, drawScale, drawScale, tintPtr);
        }
    }
}

static void hsvToRgb(float h, float s, float v, unsigned char& r, unsigned char& g, unsigned char& b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rr, gg, bb;
    if      (h < 60)  { rr=c;  gg=x;  bb=0; }
    else if (h < 120) { rr=x;  gg=c;  bb=0; }
    else if (h < 180) { rr=0;  gg=c;  bb=x; }
    else if (h < 240) { rr=0;  gg=x;  bb=c; }
    else if (h < 300) { rr=x;  gg=0;  bb=c; }
    else              { rr=c;  gg=0;  bb=x; }
    r = (unsigned char)((rr + m) * 255);
    g = (unsigned char)((gg + m) * 255);
    b = (unsigned char)((bb + m) * 255);
}

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

        int group = -1;
        if (name.find("purple") != std::string::npos) group = 0;
        else if (name.find("blue") != std::string::npos) group = 1;
        else if (name.find("green") != std::string::npos) group = 2;
        else if (name.find("red") != std::string::npos) group = 3;

        if (group != -1) {
            int subType = -1;
            if (name.find("confirm") != std::string::npos) subType = 0;
            else if (name.find("press") != std::string::npos) subType = 3;
            else if (name.find("hold end") != std::string::npos || name.find("HoldEnd") != std::string::npos) subType = 5;
            else if (name.find("hold piece") != std::string::npos || name.find("HoldPiece") != std::string::npos) subType = 4;
            else if (name.find("arrow") != std::string::npos) subType = 2;
            else if (name.find("receptor") != std::string::npos) subType = 1;

            if (subType != -1) {
                int slot = group * 6 + subType;
                NoteSprite& ns = subs[slot];
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
}

static void parseNoteFastXml(const std::string& xmlPath, C3D_Tex* tex, C2D_Image baseImg, std::vector<NoteSprite>& subs) {
    subs.clear();
    subs.resize(3);
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

        int slot = -1;
        if (name.find("Note0") != std::string::npos) { slot = 0; }
        else if (name.find("Tail0") != std::string::npos) { slot = 1; }
        else if (name.find("NoteHoldEnd0") != std::string::npos) { slot = 2; }

        if (slot != -1) {
            NoteSprite& ns = subs[slot];
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

void OptionsMenuState::init() {
    if (!MusicPlayer::isPlaying()) {
        MusicPlayer::playMenuMusic();
    }
    VCRFontFix();

    // Initialize OptionManager schemas and values
    OptionManager::get().init();
    OptionManager::get().refreshModSchemas();

    // Load shared backgrounds
    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        bgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
        if (bgSheet) {
            topBG = C2D_SpriteSheetGetImage(bgSheet, 0);
            if (topBG.tex) C3D_TexSetFilter(topBG.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        }
    }
    std::string bgbPath = "romfs:/shared/images/menuBGB.t3x";
    if (Paths::fileExists(bgbPath)) {
        bottomBGSheet = C2D_SpriteSheetLoad(bgbPath.c_str());
        if (bottomBGSheet) {
            bottomBG = C2D_SpriteSheetGetImage(bottomBGSheet, 0);
            if (bottomBG.tex) C3D_TexSetFilter(bottomBG.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        }
    }

    // Load note sprites for the Note Colors screen
    noteSheetNormal = C2D_SpriteSheetLoad("romfs:/shared/images/NOTE_assets.t3x");
    if (noteSheetNormal) {
        baseNoteImgNormal = C2D_SpriteSheetGetImage(noteSheetNormal, 0);
        if (baseNoteImgNormal.tex) C3D_TexSetFilter(baseNoteImgNormal.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        parseNoteXml("romfs:/shared/images/NOTE_assets.xml", baseNoteImgNormal.tex, baseNoteImgNormal, noteSubsNormal);
    }
    noteSheetFast = C2D_SpriteSheetLoad("romfs:/shared/images/noteSkins/NoteSheetFast.t3x");
    if (noteSheetFast) {
        baseNoteImgFast = C2D_SpriteSheetGetImage(noteSheetFast, 0);
        if (baseNoteImgFast.tex) C3D_TexSetFilter(baseNoteImgFast.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        parseNoteFastXml("romfs:/shared/images/noteSkins/NoteSheetFast.xml", baseNoteImgFast.tex, baseNoteImgFast, noteSubsFast);
    }

    // Load Button Prompt & Alphabet sheets
    ButtonPrompt::init();

    menuState = STATE_MAIN;
    curSelected = 0;
    lerpSelected = 0.0f;
    lerpColorNoteSelected = (float)colorNoteSelected;
}

std::string OptionsMenuState::getKeyName(unsigned int key) {
    switch (key) {
        case KEY_A:      return "A";
        case KEY_B:      return "B";
        case KEY_X:      return "X";
        case KEY_Y:      return "Y";
        case KEY_DLEFT:  return "D-Left";
        case KEY_DRIGHT: return "D-Right";
        case KEY_DUP:    return "D-Up";
        case KEY_DDOWN:  return "D-Down";
        default:         return "None";
    }
}

void OptionsMenuState::drawNoteSprite(int noteData, float x, float y, float scale, bool fast) {
    static const unsigned char FAST_COLORS[4][3] = {
        {0xC2, 0x4B, 0x99}, // Left  - purple
        {0x00, 0xFF, 0xFF}, // Down  - cyan
        {0x12, 0xFA, 0x05}, // Up    - green
        {0xF9, 0x39, 0x3F}, // Right - red
    };

    if (fast) {
        if (!noteSheetFast || noteSubsFast.empty()) return;

        unsigned char r, g, b;
        if (ClientPrefs::noteColorsEnabled) {
            r = ClientPrefs::noteColors[noteData][0];
            g = ClientPrefs::noteColors[noteData][1];
            b = ClientPrefs::noteColors[noteData][2];
        } else {
            r = FAST_COLORS[noteData][0];
            g = FAST_COLORS[noteData][1];
            b = FAST_COLORS[noteData][2];
        }
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 255), 1.0f);

        static const float ANGLES[4] = {
            -M_PI / 2.0f,
             M_PI,
             0.0f,
             M_PI / 2.0f
        };

        NoteSprite noteSprite = noteSubsFast[0];
        float origW = noteSprite.frameWidth ? noteSprite.frameWidth : noteSprite.w;
        float origH = noteSprite.frameHeight ? noteSprite.frameHeight : noteSprite.h;
        float expectedCenterX = x + (origW * scale / 2.0f);
        float expectedCenterY = y + (origH * scale / 2.0f);
        float noteDrawX, noteDrawY;
        if (noteSprite.rotated) {
            noteDrawX = expectedCenterX - noteSprite.h * scale * 0.5f;
            noteDrawY = expectedCenterY - noteSprite.w * scale * 0.5f;
        } else {
            noteDrawX = expectedCenterX - noteSprite.w * scale * 0.5f;
            noteDrawY = expectedCenterY - noteSprite.h * scale * 0.5f;
        }

        if (noteSubsFast.size() >= 2) {
            NoteSprite tailSprite = noteSubsFast[1];
            float tailW = tailSprite.w * scale;
            float tailX = expectedCenterX - (tailW / 2.0f);
            float tailY = expectedCenterY;
            float tailScaleY = 2.5f * scale;
            C2D_SetTintMode(C2D_TintMult);
            renderNoteSprite(tailSprite, tailX, tailY, 0.49f, &tint, scale, tailScaleY);

            if (noteSubsFast.size() >= 3) {
                NoteSprite endSprite = noteSubsFast[2];
                float endW = endSprite.w * scale;
                float endX = expectedCenterX - (endW / 2.0f);
                float endY = expectedCenterY + (tailSprite.h * tailScaleY) + 0.25f;
                renderNoteSprite(endSprite, endX, endY, 0.49f, &tint, scale, scale);
            }
            C2D_SetTintMode(C2D_TintSolid);
        }

        C2D_SetTintMode(C2D_TintMult);
        renderNoteSprite(noteSprite, noteDrawX, noteDrawY, 0.5f, &tint, scale, scale, ANGLES[noteData]);
        C2D_SetTintMode(C2D_TintSolid);
    } else {
        if (!noteSheetNormal || noteSubsNormal.empty()) return;
        int group = noteData;
        int noteIdx = group * 6 + 2;
        int tailIdx = group * 6 + 4;
        int endIdx = group * 6 + 5;

        if (noteIdx >= (int)noteSubsNormal.size()) return;

        u32 color = C2D_Color32(ClientPrefs::noteColors[noteData][0],
                                ClientPrefs::noteColors[noteData][1],
                                ClientPrefs::noteColors[noteData][2], 255);
        u8 r = (color >> 0)  & 0xFF;
        u8 g = (color >> 8)  & 0xFF;
        u8 b = (color >> 16) & 0xFF;
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 255), 0.5f);

        NoteSprite noteSprite = noteSubsNormal[noteIdx];
        float origW = noteSprite.frameWidth ? noteSprite.frameWidth : noteSprite.w;
        float origH = noteSprite.frameHeight ? noteSprite.frameHeight : noteSprite.h;
        float expectedCenterX = x + (origW * scale / 2.0f);
        float expectedCenterY = y + (origH * scale / 2.0f);
        float noteDrawX, noteDrawY;
        if (noteSprite.rotated) {
            noteDrawX = expectedCenterX - noteSprite.h * scale * 0.5f;
            noteDrawY = expectedCenterY - noteSprite.w * scale * 0.5f;
        } else {
            noteDrawX = expectedCenterX - noteSprite.w * scale * 0.5f;
            noteDrawY = expectedCenterY - noteSprite.h * scale * 0.5f;
        }

        if (tailIdx < (int)noteSubsNormal.size()) {
            NoteSprite tailSprite = noteSubsNormal[tailIdx];
            float tailW = tailSprite.w * scale;
            float tailX = expectedCenterX - (tailW / 2.0f);
            float tailY = expectedCenterY;
            float tailScaleY = 4.5f * scale;
            renderNoteSprite(tailSprite, tailX, tailY, 0.49f, &tint, scale, tailScaleY);

            if (endIdx < (int)noteSubsNormal.size()) {
                NoteSprite endSprite = noteSubsNormal[endIdx];
                float endW = endSprite.w * scale;
                float endX = expectedCenterX - (endW / 2.0f);
                float endY = expectedCenterY + (tailSprite.h * tailScaleY) - 1.5f;
                renderNoteSprite(endSprite, endX, endY, 0.49f, &tint, scale, scale);
            }
        }

        renderNoteSprite(noteSprite, noteDrawX, noteDrawY, 0.5f, &tint, scale, scale);
    }
}

void OptionsMenuState::initCheckboxesForCategory(int catIdx) {
    checkboxStates.clear();
    OptionCategory* cat = OptionManager::get().getCategory(catIdx);
    if (!cat) return;

    for (const auto& opt : cat->options) {
        CheckboxState cb;
        if (opt.type == OptionType::BOOL) {
            cb.checked = opt.boolVal;
            cb.currentAnim = cb.checked ? "checked" : "unchecked";
        }
        checkboxStates.push_back(cb);
    }
}

void OptionsMenuState::triggerCheckbox(int idx, bool checked) {
    if (idx >= 0 && idx < (int)checkboxStates.size()) {
        checkboxStates[idx].checked = checked;
        checkboxStates[idx].currentAnim = checked ? "checking" : "unchecking";
        checkboxStates[idx].animTime = 0.0f;
    }
}

void OptionsMenuState::updateCheckboxAnims(float dt) {
    for (auto& cb : checkboxStates) {
        if (cb.currentAnim == "checking" || cb.currentAnim == "unchecking") {
            cb.animTime += dt;
            float duration = (cb.currentAnim == "checking") ? (10.0f / 24.0f) : (8.0f / 24.0f);
            if (cb.animTime >= duration) {
                cb.currentAnim = cb.checked ? "checked" : "unchecked";
                cb.animTime = 0.0f;
            }
        }
    }
}

void OptionsMenuState::update(float dt) {
    updateCheckboxAnims(dt);
    gridOffset = fmodf(gridOffset + dt * 25.0f, 80.0f);
    lerpSelected += ((float)curSelected - lerpSelected) * dt * 10.0f;
    lerpColorNoteSelected += ((float)colorNoteSelected - lerpColorNoteSelected) * dt * 15.0f;
    u32 kDown = hidKeysDown();

    auto& categories = OptionManager::get().getCategories();

    // MAIN
    if (menuState == STATE_MAIN) {
        if (keyJustPressed(KEY_B)) {
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            OptionManager::get().saveValues();
            if (onPlayState) {
                onPlayState = false;
                MusicPlayer::stop();
                if (isStoryMode) {
                    switchState(new PlayState(storyWeek, storySongIdx, difficultyName));
                } else {
                    switchState(new PlayState(songName, difficultyName));
                }
            } else {
                switchState(new MainMenuState());
            }
            return;
        }
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            curSelected--;
            if (curSelected < 0) curSelected = (int)categories.size() - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            curSelected++;
            if (curSelected >= (int)categories.size()) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_A | KEY_START)) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            OptionCategory* cat = OptionManager::get().getCategory(curSelected);
            if (cat) {
                if (cat->type == OptionType::ACTION_CATEGORY) {
                    if (cat->action == "openControlsMenu") {
                        menuState = STATE_CONTROLS;
                        curSelected = 0;
                        lerpSelected = 0.0f;
                    } else if (cat->action == "openNoteColorsMenu") {
                        menuState = STATE_NOTE_COLORS;
                        curSelected = 0;
                        lerpSelected = 0.0f;
                    } else if (cat->action == "resetToDefaults") {
                        OptionManager::get().resetToDefaults();
                        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                    } else if (cat->action == "eraseSaveData") {
                        Highscores::reset();
                        Achievements::resetAchievements();
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                    }
                } else {
                    activeCategoryIndex = curSelected;
                    menuState = STATE_CATEGORY;
                    curSelected = 0;
                    lerpSelected = 0.0f;
                    initCheckboxesForCategory(activeCategoryIndex);
                }
            }
        }
    }

    // DYNAMIC CATEGORY
    else if (menuState == STATE_CATEGORY) {
        OptionCategory* cat = OptionManager::get().getCategory(activeCategoryIndex);
        if (!cat || cat->options.empty()) {
            menuState = STATE_MAIN;
            curSelected = 0;
            lerpSelected = 0.0f;
            return;
        }

        int numOpts = (int)cat->options.size();

        if (keyJustPressed(KEY_B)) {
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            OptionManager::get().saveValues();
            menuState = STATE_MAIN;
            curSelected = activeCategoryIndex;
            lerpSelected = (float)curSelected;
            return;
        }
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            curSelected--;
            if (curSelected < 0) curSelected = numOpts - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            curSelected++;
            if (curSelected >= numOpts) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        OptionItem& opt = cat->options[curSelected];

        if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
            if (opt.type == OptionType::INT) {
                opt.intVal -= (int)opt.step;
                if (opt.intVal < (int)opt.minVal) opt.intVal = (int)opt.maxVal;
                if (opt.modFolder.empty()) OptionManager::get().setInt(opt.id, opt.intVal);
                else OptionManager::get().setModInt(opt.modFolder, opt.id, opt.intVal);
                OptionManager::get().syncToClientPrefs();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            } else if (opt.type == OptionType::FLOAT) {
                opt.floatVal -= opt.step;
                if (opt.floatVal < opt.minVal - 0.001f) opt.floatVal = opt.maxVal;
                opt.floatVal = std::round(opt.floatVal * 100.0f) / 100.0f;
                if (opt.modFolder.empty()) OptionManager::get().setFloat(opt.id, opt.floatVal);
                else OptionManager::get().setModFloat(opt.modFolder, opt.id, opt.floatVal);
                OptionManager::get().syncToClientPrefs();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            } else if (opt.type == OptionType::STRING_LIST && !opt.stringOptions.empty()) {
                int curIdx = 0;
                for (int k = 0; k < (int)opt.stringOptions.size(); k++) {
                    if (opt.stringOptions[k] == opt.stringVal) { curIdx = k; break; }
                }
                curIdx = (curIdx - 1 + (int)opt.stringOptions.size()) % (int)opt.stringOptions.size();
                opt.stringVal = opt.stringOptions[curIdx];
                if (opt.modFolder.empty()) OptionManager::get().setString(opt.id, opt.stringVal);
                else OptionManager::get().setModString(opt.modFolder, opt.id, opt.stringVal);
                OptionManager::get().syncToClientPrefs();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
        }

        if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            if (opt.type == OptionType::INT) {
                opt.intVal += (int)opt.step;
                if (opt.intVal > (int)opt.maxVal) opt.intVal = (int)opt.minVal;
                if (opt.modFolder.empty()) OptionManager::get().setInt(opt.id, opt.intVal);
                else OptionManager::get().setModInt(opt.modFolder, opt.id, opt.intVal);
                OptionManager::get().syncToClientPrefs();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            } else if (opt.type == OptionType::FLOAT) {
                opt.floatVal += opt.step;
                if (opt.floatVal > opt.maxVal + 0.001f) opt.floatVal = opt.minVal;
                opt.floatVal = std::round(opt.floatVal * 100.0f) / 100.0f;
                if (opt.modFolder.empty()) OptionManager::get().setFloat(opt.id, opt.floatVal);
                else OptionManager::get().setModFloat(opt.modFolder, opt.id, opt.floatVal);
                OptionManager::get().syncToClientPrefs();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            } else if (opt.type == OptionType::STRING_LIST && !opt.stringOptions.empty()) {
                int curIdx = 0;
                for (int k = 0; k < (int)opt.stringOptions.size(); k++) {
                    if (opt.stringOptions[k] == opt.stringVal) { curIdx = k; break; }
                }
                curIdx = (curIdx + 1) % (int)opt.stringOptions.size();
                opt.stringVal = opt.stringOptions[curIdx];
                if (opt.modFolder.empty()) OptionManager::get().setString(opt.id, opt.stringVal);
                else OptionManager::get().setModString(opt.modFolder, opt.id, opt.stringVal);
                OptionManager::get().syncToClientPrefs();
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
        }

        if (kDown & (KEY_A | KEY_START)) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            if (opt.type == OptionType::BOOL) {
                opt.boolVal = !opt.boolVal;
                triggerCheckbox(curSelected, opt.boolVal);
                if (opt.modFolder.empty()) OptionManager::get().setBool(opt.id, opt.boolVal);
                else OptionManager::get().setModBool(opt.modFolder, opt.id, opt.boolVal);
                OptionManager::get().syncToClientPrefs();
            } else if (opt.type == OptionType::ACTION) {
                if (opt.action == "openCustomizeCombo") {
                    switchState(new CustomizeComboState());
                }
            } else if (opt.type == OptionType::STRING_LIST && !opt.stringOptions.empty()) {
                int curIdx = 0;
                for (int k = 0; k < (int)opt.stringOptions.size(); k++) {
                    if (opt.stringOptions[k] == opt.stringVal) { curIdx = k; break; }
                }
                curIdx = (curIdx + 1) % (int)opt.stringOptions.size();
                opt.stringVal = opt.stringOptions[curIdx];
                if (opt.modFolder.empty()) OptionManager::get().setString(opt.id, opt.stringVal);
                else OptionManager::get().setModString(opt.modFolder, opt.id, opt.stringVal);
                OptionManager::get().syncToClientPrefs();
            }
        }
    }

    // CONTROLS
    else if (menuState == STATE_CONTROLS) {
        if (!isBinding) {
            if (keyJustPressed(KEY_B)) {
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                OptionManager::get().saveValues();
                menuState = STATE_MAIN; curSelected = 0; lerpSelected = 0.0f;
                return;
            }
            if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
                curSelected -= 2;
                if (curSelected < 0) curSelected += 8;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
                curSelected += 2;
                if (curSelected >= 8) curSelected -= 8;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                if (curSelected % 2 == 1) curSelected--;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                if (curSelected % 2 == 0) curSelected++;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            if (kDown & (KEY_A | KEY_START)) {
                isBinding   = true;
                bindingLane = curSelected / 2;
                bindingIdx  = curSelected % 2;
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            }
        } else {
            if (keyJustPressed(KEY_START) || keyJustPressed(KEY_SELECT)) {
                isBinding = false;
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                return;
            }
            static const unsigned int ALLOWED_KEYS[] = {
                KEY_A, KEY_B, KEY_X, KEY_Y,
                KEY_DUP, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT
            };
            for (unsigned int key : ALLOWED_KEYS) {
                if (keyJustPressed(key)) {
                    ClientPrefs::noteKeys[bindingLane][bindingIdx] = key;
                    isBinding = false;
                    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                    break;
                }
            }
        }
    }

    // NOTE COLORS
    else if (menuState == STATE_NOTE_COLORS) {
        if (keyJustPressed(KEY_B)) {
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            OptionManager::get().saveValues();
            menuState = STATE_MAIN; curSelected = 0; lerpSelected = 0.0f;
            return;
        }
        if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
            colorNoteSelected--;
            if (colorNoteSelected < 0) colorNoteSelected = 3;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            colorNoteSelected++;
            if (colorNoteSelected > 3) colorNoteSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        u32 kHeld = hidKeysHeld();
        if (kHeld & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (touch.px >= 20 && touch.px <= 300 && touch.py >= 40 && touch.py <= 160) {
                float h = ((float)(touch.px - 20) / 280.0f) * 360.0f;
                float s = (float)(touch.py - 40) / 120.0f;
                float v = 1.0f;
                hsvToRgb(h, s, v,
                         ClientPrefs::noteColors[colorNoteSelected][0],
                         ClientPrefs::noteColors[colorNoteSelected][1],
                         ClientPrefs::noteColors[colorNoteSelected][2]);
            }
        }
    }
}


void OptionsMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    auto& categories = OptionManager::get().getCategories();

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(123, 92, 224, 255));
    drawBG(topBG, bgSheet != nullptr, 400.0f, 240.0f);

    std::string headerText = "Options";
    if (menuState == STATE_CATEGORY) {
        OptionCategory* cat = OptionManager::get().getCategory(activeCategoryIndex);
        if (cat) headerText = cat->name;
    }
    else if (menuState == STATE_CONTROLS)    headerText = isBinding ? "Press a button..." : "Controls";
    else if (menuState == STATE_NOTE_COLORS) headerText = "Note Colors";

    float headerScale = 1.5f;
    float headerW = Alphabet::getTextWidth(headerText, headerScale);
    float maxHeaderW = 370.0f;
    if (headerW > maxHeaderW) {
        headerScale *= (maxHeaderW / headerW);
    }
    Alphabet::draw(headerText, 200.0f, 40.0f, headerScale, 1.0f, true);

    if (menuState == STATE_MAIN) {
        OptionCategory* cat = OptionManager::get().getCategory(curSelected);
        std::string desc = cat ? cat->desc : "Select a category to view options.";
        AddTextCentered(desc, 200, 140, 0.38f, 1.5f, CWhite, 370.0f);
    }
    else if (menuState == STATE_CATEGORY) {
        OptionCategory* cat = OptionManager::get().getCategory(activeCategoryIndex);
        if (cat && curSelected >= 0 && curSelected < (int)cat->options.size()) {
            std::string desc = cat->options[curSelected].desc;
            AddTextCentered(desc, 200, 140, 0.38f, 1.5f, CWhite, 370.0f);
        }
    }
    else if (menuState == STATE_CONTROLS) {
        if (isBinding) {
            AddTextCentered("Press D-Pad or ABXY\nto assign this note\n\nSTART / SELECT to cancel",
                    200, 130, 0.38f, 1.5f, CWhite, 370.0f);
        } else {
            AddTextCentered("Remap your note controls.\nOnly D-Pad and ABXY keys are allowed.",
                    200, 140, 0.38f, 1.5f, CWhite, 370.0f);
        }
    }
    else if (menuState == STATE_NOTE_COLORS) {
        static const char* noteNames[] = {"Left","Down","Up","Right"};
        float startX = 40.0f;
        float step   = 85.0f;
        float noteY  = 120.0f;
        float sc     = 1.0f;
        bool fast    = ClientPrefs::fastNotes;

        float lerpNx = startX + lerpColorNoteSelected * step;
        C2D_DrawRectSolid(lerpNx - 10, noteY - 10, 0.14f, 84, 130, C2D_Color32(255,255,255,60));

        for (int i = 0; i < 4; i++) {
            float nx = startX + i * step;
            u32 labelColor = (i == colorNoteSelected) ? CYellow : 0xFFFFFFFF;
            Alphabet::draw(noteNames[i], nx + 32.0f, noteY - 18.0f, 0.75f, 1.0f, true, labelColor);
            drawNoteSprite(i, nx, noteY, sc, fast);
        }
    }

    if (menuState != STATE_NOTE_COLORS) {
        ButtonPrompt::drawPrompt("b", "Back", 8.0f, 205.0f, 0.70f, 1.0f);
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(123, 92, 224, 255));
    drawBG(bottomBG, bottomBGSheet != nullptr, 320.0f, 240.0f);

    if (menuState == STATE_MAIN) {
        for (int i = 0; i < (int)categories.size(); i++) {
            bool sel = (i == curSelected);
            float targetY = 110.0f + (i - lerpSelected) * 35.0f;
            float targetX = 160.0f;

            if (targetY < -30.0f || targetY > 270.0f) continue;

            std::string text = categories[i].name;
            float scale = 1.2f;
            float textW = Alphabet::getTextWidth(text, scale);
            float maxW = 280.0f;
            if (textW > maxW) {
                scale *= (maxW / textW);
            }
            u32 col = 0xFFFFFFFF;
            if (categories[i].id == "reset_defaults" || categories[i].id == "erase_save") {
                col = C2D_Color32(255, 80, 80, 255);
            }
            Alphabet::draw(text, targetX, targetY, scale, sel ? 1.0f : 0.5f, true, col);
        }
    }
    else if (menuState == STATE_CATEGORY) {
        OptionCategory* cat = OptionManager::get().getCategory(activeCategoryIndex);
        if (cat) {
            for (int i = 0; i < (int)cat->options.size(); i++) {
                bool sel = (i == curSelected);
                float targetY = 110.0f + (i - lerpSelected) * 35.0f;
                float targetX = 20.0f + (i - lerpSelected) * 12.0f;

                if (targetY < -30.0f || targetY > 270.0f) continue;

                OptionItem& opt = cat->options[i];
                std::string label = opt.name;
                float scale = 1.2f;

                if (opt.type == OptionType::BOOL) {
                    Alphabet::draw(label, targetX + 35.0f, targetY, scale, sel ? 1.0f : 0.5f, false);
                    if (i < (int)checkboxStates.size()) {
                        drawCheckbox(targetX, targetY + 2.0f, checkboxStates[i], sel ? 1.0f : 0.5f);
                    }
                } else if (opt.type == OptionType::INT) {
                    std::string valStr = std::to_string(opt.intVal) + opt.suffix;
                    float totalW = Alphabet::getTextWidth(label, scale) + 15.0f + Alphabet::getTextWidth(valStr, scale);
                    float maxW = 310.0f - targetX;
                    if (totalW > maxW) scale *= (maxW / totalW);
                    Alphabet::draw(label, targetX, targetY, scale, sel ? 1.0f : 0.5f, false);
                    float textW = Alphabet::getTextWidth(label, scale);
                    Alphabet::draw(valStr, targetX + textW + 15.0f, targetY, scale, sel ? 1.0f : 0.5f, false, CYellow);
                } else if (opt.type == OptionType::FLOAT) {
                    std::string valStr = "";
                    if (opt.id == "noteUnderlayAlpha" && opt.floatVal <= 0.05f) {
                        valStr = "Off";
                    } else if (opt.id == "noteUnderlayAlpha") {
                        valStr = std::to_string((int)std::round(opt.floatVal * 100.0f)) + "%";
                    } else {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%.1f", opt.floatVal);
                        valStr = buf;
                    }
                    float totalW = Alphabet::getTextWidth(label, scale) + 15.0f + Alphabet::getTextWidth(valStr, scale);
                    float maxW = 310.0f - targetX;
                    if (totalW > maxW) scale *= (maxW / totalW);
                    Alphabet::draw(label, targetX, targetY, scale, sel ? 1.0f : 0.5f, false);
                    float textW = Alphabet::getTextWidth(label, scale);
                    Alphabet::draw(valStr, targetX + textW + 15.0f, targetY, scale, sel ? 1.0f : 0.5f, false, CYellow);
                } else if (opt.type == OptionType::STRING_LIST) {
                    std::string valStr = opt.stringVal;
                    float totalW = Alphabet::getTextWidth(label, scale) + 15.0f + Alphabet::getTextWidth(valStr, scale);
                    float maxW = 310.0f - targetX;
                    if (totalW > maxW) scale *= (maxW / totalW);
                    Alphabet::draw(label, targetX, targetY, scale, sel ? 1.0f : 0.5f, false);
                    float textW = Alphabet::getTextWidth(label, scale);
                    Alphabet::draw(valStr, targetX + textW + 15.0f, targetY, scale, sel ? 1.0f : 0.5f, false, CYellow);
                } else if (opt.type == OptionType::ACTION) {
                    Alphabet::draw(label, targetX, targetY, scale, sel ? 1.0f : 0.5f, false);
                }
            }
        }
    }
    else if (menuState == STATE_CONTROLS) {
        float size = 40.0f;
        float offset = gridOffset;
        for (float x = -size * 2.0f + offset; x < 320.0f + size; x += size) {
            for (float y = -size * 2.0f + offset; y < 240.0f + size; y += size) {
                int xi = (int)std::round((x - offset) / size);
                int yi = (int)std::round((y - offset) / size);
                if ((xi + yi) % 2 == 0) {
                    C2D_DrawRectSolid(x, y, 0.15f, size, size, C2D_Color32(255, 255, 255, 45));
                }
            }
        }

        static const char* noteNames[] = {"LEFT","DOWN","UP","RIGHT"};
        float rowH = 40.0f;
        float bh = rowH;
        for (int lane = 0; lane < 4; lane++) {
            float y = 30.0f + lane * rowH;
            bool isCurrentLane = (curSelected / 2 == lane);

            Alphabet::draw(noteNames[lane], 20.0f, y + (bh - 28.0f) / 2.0f, 1.2f,
                           isCurrentLane ? 1.0f : 0.6f, false);

            for (int keyIdx = 0; keyIdx < 2; keyIdx++) {
                int i = lane * 2 + keyIdx;
                bool sel = (i == curSelected);

                float bx = (keyIdx == 0) ? 130.0f : 210.0f;
                float bw = 70.0f;

                u32 boxCol = sel ? (isBinding ? C2D_Color32(220, 50, 50, 255)
                                              : C2D_Color32(255, 255, 255, 255))
                                 : C2D_Color32(0, 0, 0, 160);
                C2D_DrawRectSolid(bx, y, 0.16f, bw, bh, boxCol);

                std::string kName = getKeyName(ClientPrefs::noteKeys[lane][keyIdx]);
                u32 txtCol = sel ? (isBinding ? 0xFFFFFFFF : 0xFF000000) : 0xFFFFFFFF;
                AddTextCentered(kName, bx + bw / 2.0f, y + (bh - 14.0f) / 2.0f, 0.35f, 1.0f, txtCol, bw);
            }
        }
    }
    else if (menuState == STATE_NOTE_COLORS) {
        float size = 40.0f;
        float offset = gridOffset;
        for (float x = -size * 2.0f + offset; x < 320.0f + size; x += size) {
            for (float y = -size * 2.0f + offset; y < 240.0f + size; y += size) {
                int xi = (int)std::round((x - offset) / size);
                int yi = (int)std::round((y - offset) / size);
                if ((xi + yi) % 2 == 0) {
                    C2D_DrawRectSolid(x, y, 0.15f, size, size, C2D_Color32(255, 255, 255, 45));
                }
            }
        }

        C2D_DrawRectSolid(20, 40, 0.14f, 280, 120, C2D_Color32(20, 20, 20, 200));
        for (int h = 0; h < 280; h += 4) {
            float hue = ((float)h / 280.0f) * 360.0f;
            unsigned char r, g, b;
            hsvToRgb(hue, 1.0f, 1.0f, r, g, b);
            C2D_DrawRectSolid(20 + h, 40, 0.15f, 4, 120, C2D_Color32(r, g, b, 255));
        }

        AddTextCentered("Touch palette to pick color", 160, 180, 0.35f, 1.0f, CWhite, 300.0f);
    }
}

void OptionsMenuState::exitState() {
    OptionManager::get().saveValues();
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    if (noteSheetNormal) C2D_SpriteSheetFree(noteSheetNormal);
    if (noteSheetFast) C2D_SpriteSheetFree(noteSheetFast);
}
