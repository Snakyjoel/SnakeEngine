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

static unsigned char copiedColor[3] = {0, 0, 0};
static bool hasCopiedColor = false;

static void drawBG(C2D_Image img, bool valid, float w, float h) {
    if (valid) {
        // Re-apply filter every frame: other C3D_TexSetFilter calls (checkboxanim,
        // noteSprites, etc.) can overwrite it after init() set it.
        if (img.tex) {
            GPU_TEXTURE_FILTER_PARAM f = ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST;
            C3D_TexSetFilter(img.tex, f, f);
        }
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(159, 42, 73, 255), 1.0f);
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

void OptionsMenuState::rgbToHsv(unsigned char r, unsigned char g, unsigned char b, float& h, float& s, float& v) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;
    float maxC = std::max(std::max(rf, gf), bf);
    float minC = std::min(std::min(rf, gf), bf);
    float delta = maxC - minC;
    v = maxC;
    if (maxC == 0.0f) {
        s = 0.0f;
        h = 0.0f;
    } else {
        s = delta / maxC;
        if (delta == 0.0f) {
            h = 0.0f;
        } else {
            if (maxC == rf) h = 60.0f * fmodf(((gf - bf) / delta) + 6.0f, 6.0f);
            else if (maxC == gf) h = 60.0f * (((bf - rf) / delta) + 2.0f);
            else if (maxC == bf) h = 60.0f * (((rf - gf) / delta) + 4.0f);
            if (h < 0.0f) h += 360.0f;
        }
    }
}

void OptionsMenuState::hsvToRgb(float h, float s, float v, unsigned char& r, unsigned char& g, unsigned char& b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rr = 0, gg = 0, bb = 0;
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

static void parseUiXml(const std::string& xmlPath, C3D_Tex* tex, C2D_Image baseImg, OptionsMenuState::UiSprite& copySprite, OptionsMenuState::UiSprite& pasteSprite) {
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

        OptionsMenuState::UiSprite* dest = nullptr;
        if (name == "copy") dest = &copySprite;
        else if (name == "paste") dest = &pasteSprite;

        if (dest) {
            dest->tex = tex;
            dest->w = w;
            dest->h = h;
            dest->sub.width = (u16)w;
            dest->sub.height = (u16)h;
            dest->sub.left = baseImg.subtex->left + (x * nw / baseImg.subtex->width);
            dest->sub.top = baseImg.subtex->top + (y * nh / baseImg.subtex->height);
            dest->sub.right = baseImg.subtex->left + ((x + w) * nw / baseImg.subtex->width);
            dest->sub.bottom = baseImg.subtex->top + ((y + h) * nh / baseImg.subtex->height);
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
        if (name.find("NoteHoldEnd") != std::string::npos) { slot = 2; }
        else if (name.find("Tail") != std::string::npos) { slot = 1; }
        else if (name.find("Note") != std::string::npos) { slot = 0; }

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
    noteSheetNormal = C2D_SpriteSheetLoad("romfs:/shared/images/noteSkins/NOTE_assets.t3x");
    if (noteSheetNormal) {
        baseNoteImgNormal = C2D_SpriteSheetGetImage(noteSheetNormal, 0);
        if (baseNoteImgNormal.tex) C3D_TexSetFilter(baseNoteImgNormal.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        parseNoteXml("romfs:/shared/images/noteSkins/NOTE_assets.xml", baseNoteImgNormal.tex, baseNoteImgNormal, noteSubsNormal);
    }
    noteSheetFast = C2D_SpriteSheetLoad("romfs:/shared/images/noteSkins/NoteSheetFast.t3x");
    if (noteSheetFast) {
        baseNoteImgFast = C2D_SpriteSheetGetImage(noteSheetFast, 0);
        if (baseNoteImgFast.tex) C3D_TexSetFilter(baseNoteImgFast.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        parseNoteFastXml("romfs:/shared/images/noteSkins/NoteSheetFast.xml", baseNoteImgFast.tex, baseNoteImgFast, noteSubsFast);
    }

    colorWheelSheet = C2D_SpriteSheetLoad("romfs:/preload/images/menus/colorWheel.t3x");
    if (colorWheelSheet) {
        colorWheel = C2D_SpriteSheetGetImage(colorWheelSheet, 0);
        if (colorWheel.tex) C3D_TexSetFilter(colorWheel.tex, GPU_LINEAR, GPU_LINEAR);
    }

    copyPasteSheet = C2D_SpriteSheetLoad("romfs:/preload/images/menus/copypaste.t3x");
    if (copyPasteSheet) {
        C2D_Image baseImg = C2D_SpriteSheetGetImage(copyPasteSheet, 0);
        if (baseImg.tex) {
            C3D_TexSetFilter(baseImg.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
            parseUiXml("romfs:/preload/images/menus/copypaste.xml", baseImg.tex, baseImg, copyBtnSprite, pasteBtnSprite);
        }
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
        case KEY_DLEFT:  return "D Left";
        case KEY_DRIGHT: return "D Right";
        case KEY_DUP:    return "D Up";
        case KEY_DDOWN:  return "D Down";
        case KEY_L:      return "L";
        case KEY_R:      return "R";
        case KEY_ZL:     return "ZL";
        case KEY_ZR:     return "ZR";
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
                        if (colorNoteSelected == -1) colorNoteSelected = 0;
                        rgbToHsv(ClientPrefs::noteColors[colorNoteSelected][0], ClientPrefs::noteColors[colorNoteSelected][1], ClientPrefs::noteColors[colorNoteSelected][2], currentHue, currentSat, currentVal);
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
        bool isNew3DS = false;
        APT_CheckNew3DS(&isNew3DS);
        int totalSelections = isNew3DS ? 12 : 10;

        int selRow = (curSelected < 8) ? (curSelected / 2) : (4 + (curSelected - 8));
        float targetScroll = 100.0f - selRow * 40.0f;
        controlScrollY += (targetScroll - controlScrollY) * (1.0f - exp2f(-12.0f * dt));

        if (!isBinding) {
            if (keyJustPressed(KEY_B)) {
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                OptionManager::get().saveValues();
                menuState = STATE_MAIN; curSelected = 0; lerpSelected = 0.0f;
                return;
            }
            if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
                if (curSelected < 8) {
                    if (curSelected <= 1) {
                        curSelected = totalSelections - 1;
                    } else {
                        curSelected -= 2;
                    }
                } else if (curSelected == 8) {
                    curSelected = 6;
                } else {
                    curSelected -= 1;
                }
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
                if (curSelected < 8) {
                    if (curSelected >= 6) {
                        curSelected = 8;
                    } else {
                        curSelected += 2;
                    }
                } else {
                    curSelected += 1;
                    if (curSelected >= totalSelections) {
                        curSelected = 0;
                    }
                }
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                if (curSelected < 8 && curSelected % 2 == 1) {
                    curSelected--;
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                }
            }
            if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                if (curSelected < 8 && curSelected % 2 == 0) {
                    curSelected++;
                    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                }
            }
            if (kDown & (KEY_A | KEY_START)) {
                isBinding   = true;
                if (curSelected < 8) {
                    bindingLane = curSelected / 2;
                    bindingIdx  = curSelected % 2;
                } else {
                    bindingLane = 4 + (curSelected - 8);
                    bindingIdx  = 0;
                }
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
                KEY_DUP, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT,
                KEY_L, KEY_R, KEY_ZL, KEY_ZR
            };
            for (unsigned int key : ALLOWED_KEYS) {
                if (keyJustPressed(key)) {
                    if (bindingLane < 4) {
                        ClientPrefs::noteKeys[bindingLane][bindingIdx] = key;
                    } else {
                        int actIdx = bindingLane - 4;
                        if (actIdx >= 0 && actIdx < 4) {
                            ClientPrefs::actionKeys[actIdx] = key;
                        }
                    }
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
            ClientPrefs::saveSettings();
            menuState = STATE_MAIN; curSelected = 0; lerpSelected = 0.0f;
            return;
        }
        
        if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
            colorNoteSelected--;
            if (colorNoteSelected < 0) colorNoteSelected = 3;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            rgbToHsv(ClientPrefs::noteColors[colorNoteSelected][0], ClientPrefs::noteColors[colorNoteSelected][1], ClientPrefs::noteColors[colorNoteSelected][2], currentHue, currentSat, currentVal);
        }
        if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            colorNoteSelected++;
            if (colorNoteSelected > 3) colorNoteSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            rgbToHsv(ClientPrefs::noteColors[colorNoteSelected][0], ClientPrefs::noteColors[colorNoteSelected][1], ClientPrefs::noteColors[colorNoteSelected][2], currentHue, currentSat, currentVal);
        }

        // Reset to default
        if (keyJustPressed(KEY_X) || keyJustPressed(KEY_Y)) {
            static const unsigned char FAST_COLORS[4][3] = {
                {0xC2, 0x4B, 0x99}, {0x00, 0xFF, 0xFF}, {0x12, 0xFA, 0x05}, {0xF9, 0x39, 0x3F}
            };
            ClientPrefs::noteColors[colorNoteSelected][0] = FAST_COLORS[colorNoteSelected][0];
            ClientPrefs::noteColors[colorNoteSelected][1] = FAST_COLORS[colorNoteSelected][1];
            ClientPrefs::noteColors[colorNoteSelected][2] = FAST_COLORS[colorNoteSelected][2];
            rgbToHsv(ClientPrefs::noteColors[colorNoteSelected][0], ClientPrefs::noteColors[colorNoteSelected][1], ClientPrefs::noteColors[colorNoteSelected][2], currentHue, currentSat, currentVal);
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        }
        
        u32 kHeld = hidKeysHeld();
        u32 kDownTouch = hidKeysDown();
        if (kDownTouch & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            
            // Copy button: x=262, copyY = cy - totalBtnH/2 = 120 - 55.4 ≈ 65, h=50.4
            if (touch.px >= 262 && touch.px <= 309 && touch.py >= 65 && touch.py <= 115) {
                copiedColor[0] = ClientPrefs::noteColors[colorNoteSelected][0];
                copiedColor[1] = ClientPrefs::noteColors[colorNoteSelected][1];
                copiedColor[2] = ClientPrefs::noteColors[colorNoteSelected][2];
                hasCopiedColor = true;
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            }
            // Paste button: pasteY = 65 + 50.4 + 10 ≈ 125, h=50.4
            else if (touch.px >= 262 && touch.px <= 309 && touch.py >= 125 && touch.py <= 175) {
                if (hasCopiedColor) {
                    ClientPrefs::noteColors[colorNoteSelected][0] = copiedColor[0];
                    ClientPrefs::noteColors[colorNoteSelected][1] = copiedColor[1];
                    ClientPrefs::noteColors[colorNoteSelected][2] = copiedColor[2];
                    rgbToHsv(copiedColor[0], copiedColor[1], copiedColor[2], currentHue, currentSat, currentVal);
                    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                } else {
                    AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                }
            }
        }

        if (kHeld & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            
            float wheelScale = 0.55f;
            float cwWidth = (colorWheel.subtex ? colorWheel.subtex->width : 200.0f) * wheelScale;
            float cwHeight = (colorWheel.subtex ? colorWheel.subtex->height : 200.0f) * wheelScale;
            float radius = std::min(cwWidth, cwHeight) / 2.0f;
            float cx = 150.0f;
            float cy = 120.0f;
            
            float dx = touch.px - cx;
            float dy = touch.py - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            
            if (dist <= radius) {
                float angle = atan2f(dy, dx) * 180.0f / (float)M_PI;
                float hue = angle - 270.0f;
                while (hue < 0.0f) hue += 360.0f;
                while (hue >= 360.0f) hue -= 360.0f;
                currentHue = hue;
                currentSat = std::min(1.0f, dist / radius);
                
                unsigned char r, g, b;
                hsvToRgb(currentHue, currentSat, currentVal, r, g, b);
                ClientPrefs::noteColors[colorNoteSelected][0] = r;
                ClientPrefs::noteColors[colorNoteSelected][1] = g;
                ClientPrefs::noteColors[colorNoteSelected][2] = b;
            } 
            // Brightness bar: x=18, w=20, y=30 to 210
            else if (touch.px >= 18 && touch.px <= 38 && touch.py >= 30 && touch.py <= 210) {
                currentVal = 1.0f - ((touch.py - 30.0f) / 180.0f);
                if (currentVal < 0.0f) currentVal = 0.0f;
                if (currentVal > 1.0f) currentVal = 1.0f;
                
                unsigned char r, g, b;
                hsvToRgb(currentHue, currentSat, currentVal, r, g, b);
                ClientPrefs::noteColors[colorNoteSelected][0] = r;
                ClientPrefs::noteColors[colorNoteSelected][1] = g;
                ClientPrefs::noteColors[colorNoteSelected][2] = b;
            }
        }
    }
}


void OptionsMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    auto& categories = OptionManager::get().getCategories();

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(182, 91, 138, 255));
    drawBG(topBG, bgSheet != nullptr, 400.0f, 240.0f);

    // Grid overlay for controls and note colors
    if (menuState == STATE_CONTROLS || menuState == STATE_NOTE_COLORS) {
        float gsize = 40.0f;
        float goff = gridOffset;
        for (float gx = -gsize * 2.0f + goff; gx < 400.0f + gsize; gx += gsize) {
            for (float gy = -gsize * 2.0f + goff; gy < 240.0f + gsize; gy += gsize) {
                int xi = (int)std::round((gx - goff) / gsize);
                int yi = (int)std::round((gy - goff) / gsize);
                if ((xi + yi) % 2 == 0) {
                    C2D_DrawRectSolid(gx, gy, 0.15f, gsize, gsize, C2D_Color32(255, 255, 255, 45));
                }
            }
        }
    }

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
        // Draw the 4 notes (Left, Down, Up, Right)
        float totalWidth = 4 * 80.0f;
        float startX = (400.0f - totalWidth) / 2.0f;
        float noteY = 80.0f;
        
        static const float baseScale = 0.65f;
        
        for (int i = 0; i < 4; i++) {
            float xPos = startX + i * 80.0f;
            float scale = baseScale;
            float alpha = 0.6f;
            
            if (i == colorNoteSelected) {
                scale = baseScale * 1.15f;
                alpha = 1.0f;
                // Center the selection rect around the note
                float noteW = (noteSubsNormal.size() > (size_t)(i*6+1) && noteSubsNormal[i*6+1].w > 0)
                    ? (noteSubsNormal[i*6+1].rotated ? noteSubsNormal[i*6+1].h : noteSubsNormal[i*6+1].w) * scale
                    : 80.0f * scale;
                float noteH = (noteSubsNormal.size() > (size_t)(i*6+1) && noteSubsNormal[i*6+1].h > 0)
                    ? (noteSubsNormal[i*6+1].rotated ? noteSubsNormal[i*6+1].w : noteSubsNormal[i*6+1].h) * scale
                    : 80.0f * scale;
                float noteCX = xPos + 40.0f;
                float noteCY = noteY + 40.0f;
                float pad = 8.0f;
                C2D_DrawRectSolid(noteCX - noteW/2.0f - pad, noteCY - noteH/2.0f - pad,
                    0.4f, noteW + pad*2, noteH + pad*2, C2D_Color32(255, 255, 255, 100));
            }
            
            int noteIdx = i * 6 + 1;
            if (noteIdx < (int)noteSubsNormal.size()) {
                NoteSprite noteSprite = noteSubsNormal[noteIdx];
                float cx = xPos + 40.0f;
                float cy = noteY + 40.0f;
                float drawX = cx - (noteSprite.rotated ? noteSprite.h : noteSprite.w) * scale * 0.5f;
                float drawY = cy - (noteSprite.rotated ? noteSprite.w : noteSprite.h) * scale * 0.5f;
                
                unsigned char r = ClientPrefs::noteColors[i][0];
                unsigned char g = ClientPrefs::noteColors[i][1];
                unsigned char b = ClientPrefs::noteColors[i][2];
                C2D_ImageTint noteTint;
                C2D_PlainImageTint(&noteTint, C2D_Color32(r, g, b, 255), alpha);

                C2D_SetTintMode(C2D_TintMult);
                renderNoteSprite(noteSprite, drawX, drawY, 0.5f, &noteTint, scale, scale);
                C2D_SetTintMode(C2D_TintSolid);
            }
        }

        // Hex label using Alphabet (top of screen) — colored with the current note color
        char buf[32];
        snprintf(buf, sizeof(buf), "HEX: %02X%02X%02X",
            ClientPrefs::noteColors[colorNoteSelected][0],
            ClientPrefs::noteColors[colorNoteSelected][1],
            ClientPrefs::noteColors[colorNoteSelected][2]);
        float hexScale = 1.0f;
        u32 hexColor = C2D_Color32(
            ClientPrefs::noteColors[colorNoteSelected][0],
            ClientPrefs::noteColors[colorNoteSelected][1],
            ClientPrefs::noteColors[colorNoteSelected][2],
            255);
        Alphabet::draw(buf, 200.0f, 15.0f, hexScale, 1.0f, true, hexColor);

        // Note name at the bottom of the top screen
        static const char* noteNameLabels[] = {"LEFT NOTE", "DOWN NOTE", "UP NOTE", "RIGHT NOTE"};
        std::string noteLabel = noteNameLabels[colorNoteSelected];
        float labelScale = 1.5f;
        float labelW = Alphabet::getTextWidth(noteLabel, labelScale);
        float maxLabelW = 370.0f;
        if (labelW > maxLabelW) labelScale *= (maxLabelW / labelW);
        Alphabet::draw(noteLabel, 200.0f, 195.0f, labelScale, 0.85f, true);
    }

    if (menuState != STATE_NOTE_COLORS) {
        ButtonPrompt::drawPrompt("b", "Back", 8.0f, 205.0f, 0.70f, 1.0f);
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(182, 91, 138, 255));
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

        bool isNew3DS = false;
        APT_CheckNew3DS(&isNew3DS);
        int totalLanes = isNew3DS ? 8 : 6; // 4 notes + 2 o 4 acts

        static const char* noteNames[] = {"LEFT","DOWN","UP","RIGHT","ACT 1","ACT 2","ACT 3","ACT 4"};
        float rowH = 40.0f;
        float bh = 32.0f;

        for (int lane = 0; lane < totalLanes; lane++) {
            float y = controlScrollY + lane * rowH;
            if (y < -40.0f || y > 240.0f) continue; // Skip rendering off-screen rows

            bool isCurrentLane = false;
            if (curSelected < 8) {
                isCurrentLane = (curSelected / 2 == lane);
            } else {
                isCurrentLane = (4 + (curSelected - 8) == lane);
            }

            // Draw action name
            Alphabet::draw(noteNames[lane], 20.0f, y + 6.0f, 1.1f,
                           isCurrentLane ? 1.0f : 0.5f, false);

            if (lane < 4) {
                // Render note keybind boxes
                for (int keyIdx = 0; keyIdx < 2; keyIdx++) {
                    int idx = lane * 2 + keyIdx;
                    bool sel = (idx == curSelected);

                    float bx = (keyIdx == 0) ? 140.0f : 225.0f;
                    float bw = 75.0f;

                    u32 boxCol = sel ? (isBinding ? C2D_Color32(220, 50, 50, 160)
                                                  : C2D_Color32(255, 255, 255, 160))
                                     : C2D_Color32(0, 0, 0, 160);
                    C2D_DrawRectSolid(bx, y + (rowH - bh) / 2.0f, 0.16f, bw, bh, boxCol);

                    std::string kName = getKeyName(ClientPrefs::noteKeys[lane][keyIdx]);
                    // Draw keybind
                    Alphabet::draw(kName, bx + bw / 2.0f, y + 12.0f, 0.65f, sel ? 1.0f : 0.6f, true, 0xFFFFFFFF);
                }
            } else {
                // Render action keybind box (single column)
                int actIdx = lane - 4;
                bool sel = (curSelected == 8 + actIdx);

                float bx = 140.0f;
                float bw = 75.0f;

                u32 boxCol = sel ? (isBinding ? C2D_Color32(220, 50, 50, 160)
                                              : C2D_Color32(255, 255, 255, 160))
                                 : C2D_Color32(0, 0, 0, 160);
                C2D_DrawRectSolid(bx, y + (rowH - bh) / 2.0f, 0.16f, bw, bh, boxCol);

                std::string kName = getKeyName(ClientPrefs::actionKeys[actIdx]);
                // Draw keybind
                Alphabet::draw(kName, bx + bw / 2.0f, y + 12.0f, 0.65f, sel ? 1.0f : 0.6f, true, 0xFFFFFFFF);
            }
        }
    }
    else if (menuState == STATE_NOTE_COLORS) {
        // Draw grid overlay (bottom screen)
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

        // Draw dark semi-transparent panel background
        C2D_DrawRectSolid(10, 15, 0.2f, 300, 210, C2D_Color32(0, 0, 0, 100));

        // Layout constants:
        // Brightness bar: x=18, w=20 → right edge x=38
        // Copy/Paste buttons sprite w = 64 * 0.7 = 44.8 → placed at x=262 → left edge x=262
        // Usable center zone: 38 to 262 → center = 38 + (262-38)/2 = 38 + 112 = 150
        // Wheel at scale 0.55: approx radius 55 → cx=150, cy=120
        // Buttons: copyH=50.4, pasteH=50.4, gap=10 → total=110.8 → startY = 120 - 55.4 = 64.6

        // --- Brightness bar ---
        for (int y = 0; y < 180; y++) {
            float val = 1.0f - (y / 180.0f);
            unsigned char rr, gg, bb;
            hsvToRgb(currentHue, currentSat, val, rr, gg, bb);
            C2D_DrawRectSolid(18, 30 + y, 0.5f, 20, 1, C2D_Color32(rr, gg, bb, 255));
        }
        float sliderSelY = 30 + (1.0f - currentVal) * 180.0f;
        C2D_DrawRectSolid(16, sliderSelY - 2, 0.6f, 24, 4, C2D_Color32(255, 255, 255, 255));

        // --- Color wheel (centered between bar right=38 and btn left=262) ---
        float cx = 150.0f;
        float cy = 120.0f;
        float wheelScale = 0.55f;
        float cwWidth  = (colorWheel.subtex ? colorWheel.subtex->width  : 200.0f) * wheelScale;
        float cwHeight = (colorWheel.subtex ? colorWheel.subtex->height : 200.0f) * wheelScale;
        float radius = std::min(cwWidth, cwHeight) / 2.0f;

        if (colorWheel.tex) {
            C2D_ImageTint valTint;
            C2D_PlainImageTint(&valTint, C2D_Color32(0, 0, 0, 255), 1.0f - currentVal);
            C2D_DrawImageAt(colorWheel, cx - cwWidth/2, cy - cwHeight/2, 0.5f, &valTint, wheelScale, wheelScale);
        }

        float selAngleRad = (currentHue + 270.0f) * M_PI / 180.0f;
        float selDist = currentSat * radius;
        float selX = cx + selDist * cos(selAngleRad);
        float selY = cy + selDist * sin(selAngleRad);
        C2D_DrawRectSolid(selX - 3, selY - 3, 0.6f, 6, 6, C2D_Color32(255, 255, 255, 255));

        // --- Copy/Paste buttons (vertically centered on right side, x=262) ---
        // copyH = 72 * 0.7 = 50.4; pasteH = 72 * 0.7 = 50.4; gap = 10
        // totalH = 50.4 + 10 + 50.4 = 110.8 → startY = cy - totalH/2 = 120 - 55.4 = 64.6
        float btnX    = 255.0f;
        float btnScale = 0.7f;
        float copyH   = 72.0f * btnScale;   // 50.4
        float pasteH  = 72.0f * btnScale;   // 50.4
        float btnGap  = 10.0f;
        float totalBtnH = copyH + btnGap + pasteH;
        float copyY   = cy - totalBtnH / 2.0f;
        float pasteY  = copyY + copyH + btnGap;

        if (copyBtnSprite.tex) {
            C2D_Image img;
            img.tex = copyBtnSprite.tex;
            img.subtex = &copyBtnSprite.sub;
            C2D_DrawImageAt(img, btnX, copyY, 0.5f, nullptr, btnScale, btnScale);
        }
        if (pasteBtnSprite.tex) {
            C2D_Image img;
            img.tex = pasteBtnSprite.tex;
            img.subtex = &pasteBtnSprite.sub;
            C2D_ImageTint tint;
            C2D_ImageTint* tintPtr = nullptr;
            if (!hasCopiedColor) {
                C2D_AlphaImageTint(&tint, 0.4f);
                tintPtr = &tint;
            }
            C2D_DrawImageAt(img, btnX, pasteY, 0.5f, tintPtr, btnScale, btnScale);
        }
    }
}

void OptionsMenuState::exitState() {
    OptionManager::get().saveValues();
    ClientPrefs::saveSettings();
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    if (noteSheetNormal) C2D_SpriteSheetFree(noteSheetNormal);
    if (noteSheetFast) C2D_SpriteSheetFree(noteSheetFast);
    if (colorWheelSheet) C2D_SpriteSheetFree(colorWheelSheet);
    if (copyPasteSheet) C2D_SpriteSheetFree(copyPasteSheet);
    if (vcrFontBuf) C2D_TextBufDelete(vcrFontBuf);
}
