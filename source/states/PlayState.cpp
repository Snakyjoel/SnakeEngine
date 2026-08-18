#include "PlayState.hpp"
#include "OptionsMenuState.hpp"
#include <sstream>
#include "../backend/MusicBeatState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/Conductor.hpp"
#include "SongParser.hpp"
#include "MainMenuState.hpp"
#include "FreeplayState.hpp"
#include "ShaderManager.hpp"
#include "StoryMenuState.hpp"
#include "VideoState.hpp"
#include "../objects/InGameVideoPlayer.hpp"
#include "../backend/LuaManager.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../backend/Ease.hpp"
#include "Achievements.hpp"
#include "ResultState.hpp"
#include <time.h>
#include "../backend/ModHandler.hpp"
#include "../backend/AsyncAssetManager.hpp"
#include <algorithm>
#include <3ds/allocator/vram.h>
#include <algorithm>
#include <cmath>
#include "../objects/Alphabet.hpp"
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "../substates/PauseSubState.hpp"

extern void makeFontPixelPerfect(C2D_Font font);

static inline bool addrIsVRAM(const void* addr) {
    u32 v = (u32)addr;
    return v >= 0x1F000000 && v < 0x1F600000;
}

static void parseNoteXml(const std::string& xmlPath, C3D_Tex* tex, C2D_Image baseImg, std::vector<NoteSprite>& subs, std::vector<std::vector<NoteSprite>>* animFrames = nullptr) {
    subs.clear();
    subs.resize(24);
    for (auto& s : subs) {
        s.tex = tex;
    }
    if (animFrames) {
        animFrames->clear();
        animFrames->resize(24);
    }

    std::ifstream f(xmlPath);
    if (!f.is_open()) return;
    
    static Tex3DS_SubTexture defaultSubtex;
    if (baseImg.subtex == nullptr) {
        defaultSubtex.width = baseImg.tex ? baseImg.tex->width : 0;
        defaultSubtex.height = baseImg.tex ? baseImg.tex->height : 0;
        defaultSubtex.left = 0.0f;
        defaultSubtex.top = 0.0f;
        defaultSubtex.right = 1.0f;
        defaultSubtex.bottom = 1.0f;
        baseImg.subtex = &defaultSubtex;
    }
    
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
            NoteSprite ns;
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

            // Store the first frame in subs for backward compatibility fallback
            if (subs[destIdx].w == 0.0f) {
                subs[destIdx] = ns;
            }

            if (animFrames) {
                (*animFrames)[destIdx].push_back(ns);
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
    
    static Tex3DS_SubTexture defaultSubtex;
    if (baseImg.subtex == nullptr) {
        defaultSubtex.width = baseImg.tex ? baseImg.tex->width : 0;
        defaultSubtex.height = baseImg.tex ? baseImg.tex->height : 0;
        defaultSubtex.left = 0.0f;
        defaultSubtex.top = 0.0f;
        defaultSubtex.right = 1.0f;
        defaultSubtex.bottom = 1.0f;
        baseImg.subtex = &defaultSubtex;
    }
    
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

struct RawTexHeader {
    char magic[4];    // "RWTX"
    uint16_t width;   // Pow2 Width
    uint16_t height;  // Pow2 Height
    uint16_t origW;   // Original Width
    uint16_t origH;   // Original Height
};

void PlayState::updateLuaText(LuaText& t) {
    if (!t.dirty) return;
    if (t.buf) C2D_TextBufDelete(t.buf);
    t.buf = C2D_TextBufNew(t.text.length() + 32);
    C2D_TextFontParse(&t.c2dObj, vcrFont, t.buf, t.text.c_str());
    C2D_TextOptimize(&t.c2dObj);
    t.dirty = false;
}

PlayState* PlayState::instance = nullptr;

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

PlayState::PlayState(const std::string& songName, const std::string& difficulty) 
    : curSong(songName), currentDifficulty(difficulty) {
    isStoryMode = false;
}

PlayState::PlayState(const WeekData& week, int songIdx, const std::string& difficulty) 
    : weekData(week), curSongIdx(songIdx) {
    isStoryMode = true;
    if (songIdx >= 0 && songIdx < (int)week.songs.size()) {
        curSong = week.songs[songIdx].name;
    }
    currentDifficulty = difficulty;
}



void PlayState::init() {
    MusicPlayer::stop();
    SpritesheetCache::get().clear();
    AudioEngine::clearSoundCache();
    spacing = 38.0f;
    if (ClientPrefs::middleScroll) {
        playerX = (ScreenWidthTop / 2.0f) - (spacing * 1.5f) - (spacing / 2.0f);
    } else {
        playerX = ScreenWidthTop - (spacing * 4.0f) - 16.0f;
    }
    for (int i = 0; i < 4; ++i) {
        customPlayerStrumX[i] = -9999.0f;
        customPlayerStrumY[i] = -9999.0f;
        customPlayerStrumAngle[i] = 0.0f;
        customPlayerStrumAlpha[i] = -1.0f;
        customPlayerStrumDirection[i] = 90.0f;
        customPlayerStrumScaleX[i] = 1.0f;
        customPlayerStrumScaleY[i] = 1.0f;
        customPlayerStrumColor[i] = 0xFFFFFFFF;
        customPlayerStrumVisible[i] = true;
        customPlayerStrumFlipX[i] = false;
        customPlayerStrumFlipY[i] = false;
        customPlayerStrumAntialiasing[i] = true;

        customOpponentStrumX[i] = -9999.0f;
        customOpponentStrumY[i] = -9999.0f;
        customOpponentStrumAngle[i] = 0.0f;
        customOpponentStrumAlpha[i] = -1.0f;
        customOpponentStrumDirection[i] = 90.0f;
        customOpponentStrumScaleX[i] = 1.0f;
        customOpponentStrumScaleY[i] = 1.0f;
        customOpponentStrumColor[i] = 0xFFFFFFFF;
        customOpponentStrumVisible[i] = true;
        customOpponentStrumFlipX[i] = false;
        customOpponentStrumFlipY[i] = false;
        customOpponentStrumAntialiasing[i] = true;
    }
    
    healthBarX = -9999.0f;
    healthBarY = -9999.0f;
    healthBarScaleX = 1.0f;
    healthBarScaleY = 1.0f;
    healthBarAlpha = 1.0f;
    healthBarColor = 0xFFFFFFFF;
    healthBarAngle = 0.0f;
    healthBarVisible = true;
    healthBarFlipX = false;
    healthBarFlipY = false;
    healthBarAntialiasing = true;

    healthBarBGX = -9999.0f;
    healthBarBGY = -9999.0f;
    healthBarBGScaleX = 1.0f;
    healthBarBGScaleY = 1.0f;
    healthBarBGAlpha = 1.0f;
    healthBarBGColor = 0xFFFFFFFF;
    healthBarBGAngle = 0.0f;
    healthBarBGVisible = true;
    healthBarBGFlipX = false;
    healthBarBGFlipY = false;
    healthBarBGAngle = 0.0f;

    timeBarX_val = -9999.0f;
    timeBarY_val = -9999.0f;
    timeBarScaleX = 1.0f;
    timeBarScaleY = 1.0f;
    timeBarAlpha = 1.0f;
    timeBarAngle = 0.0f;
    timeBarFlipX = false;
    timeBarFlipY = false;
    timeBarAntialiasing = true;

    timeBarBGX = -9999.0f;
    timeBarBGY = -9999.0f;
    timeBarBGScaleX = 1.0f;
    timeBarBGScaleY = 1.0f;
    timeBarBGAlpha = 1.0f;
    timeBarBGColor = 0xFFFFFFFF;
    timeBarBGAngle = 0.0f;
    timeBarBGVisible = true;
    timeBarBGFlipX = false;
    timeBarBGFlipY = false;
    timeBarBGAntialiasing = true;

    iconP1ScaleX = 1.0f;
    iconP1ScaleY = 1.0f;
    iconP1Color = 0xFFFFFFFF;
    iconP1Angle = 0.0f;
    iconP1FlipX = false;
    iconP1FlipY = false;
    iconP1Antialiasing = true;

    iconP2ScaleX = 1.0f;
    iconP2ScaleY = 1.0f;
    iconP2Color = 0xFFFFFFFF;
    iconP2Angle = 0.0f;
    iconP2FlipX = false;
    iconP2FlipY = false;
    iconP2Antialiasing = true;

    scoreTxtX = -9999.0f;
    scoreTxtY = -9999.0f;
    scoreTxtScaleX = 1.0f;
    scoreTxtScaleY = 1.0f;
    scoreTxtAlpha = 1.0f;
    scoreTxtAngle = 0.0f;
    scoreTxtFlipX = false;
    scoreTxtFlipY = false;
    scoreTxtAntialiasing = true;

    timeTxtX = -9999.0f;
    timeTxtY = -9999.0f;
    timeTxtScaleX = 1.0f;
    timeTxtScaleY = 1.0f;
    timeTxtAlpha = 1.0f;
    timeTxtAngle = 0.0f;
    timeTxtFlipX = false;
    timeTxtFlipY = false;
    timeTxtAntialiasing = true;

    countdownX = -9999.0f;
    countdownY = -9999.0f;
    countdownScaleX = 0.75f;
    countdownScaleY = 0.75f;
    countdownAngle = 0.0f;
    countdownVisible = true;
    countdownFlipX = false;
    countdownFlipY = false;
    countdownAntialiasing = true;
    countdownColor = 0xFFFFFFFF;

    camX_offset = 0.0f;
    camY_offset = 0.0f;
    camScaleX = 1.0f;
    camScaleY = 1.0f;
    camAngle = 0.0f;
    camAlpha = 1.0f;
    camVisible = true;
    camFlipX = false;
    camFlipY = false;

    hudX_offset = 0.0f;
    hudY_offset = 0.0f;
    hudScaleX = 1.0f;
    hudScaleY = 1.0f;
    hudAngle = 0.0f;
    hudAlpha = 1.0f;
    hudVisible = true;
    hudFlipX = false;
    hudFlipY = false;

    otherX_offset = 0.0f;
    otherY_offset = 0.0f;
    otherZoom = 1.0f;
    otherScaleX = 1.0f;
    otherScaleY = 1.0f;
    otherAngle = 0.0f;
    otherAlpha = 1.0f;
    otherVisible = true;
    otherFlipX = false;
    otherFlipY = false;

    scoreTxtVisible = true;
    scoreTxtColor = 0xFFFFFFFF;
    timeTxtVisible = true;
    timeTxtColor = 0xFFFFFFFF;
    
    iconP1X = -9999.0f;
    iconP1Y = -9999.0f;
    iconP1Scale = 1.0f;
    iconP1Alpha = 1.0f;
    iconP1Visible = true;

    iconP2X = -9999.0f;
    iconP2Y = -9999.0f;
    iconP2Scale = 1.0f;
    iconP2Alpha = 1.0f;
    iconP2Visible = true;

    healthBarColor = 0xFFFFFFFF;
    healthBarBGColor = 0xFFFFFFFF;
    timeBarColor = 0xFFFFFFFF;
    timeBarBGColor = 0xFFFFFFFF;
    timeBarVisible = true;
    healthBarVisible = true;
    healthBarBGVisible = true;
    endedSong = false;
    printf("\x1b[1;1HPlayState::init start\n");
    
    time_t now = time(nullptr);
    tm* ltm = localtime(&now);
    if (ltm->tm_wday == 5) Achievements::unlockAchievement("friday_night_play");
    
    AsyncAssetManager::get().suspend();
    
    extern C2D_Font globalVCRFont;
    vcrFont = globalVCRFont;
    
    printf("\x1b[2;1H[System] Loading %s...\n", curSong.c_str());
    
    PlayState::instance = this;
    
    // Load song JSON early to ensure arrowSkin and ratingSkin are available
    std::string sPath = Paths::songJson(curSong, currentDifficulty);
    songData = SongParser::loadJson(sPath);
    
    bool isPixelSkinGlobal =
        SongParser::arrowSkin.size() >= 6 &&
        SongParser::arrowSkin.compare(SongParser::arrowSkin.size() - 6, 6, "-pixel") == 0;
        
    songNotes = songData.notes;
    for (auto& n : songNotes) {
        if (isPixelSkinGlobal) {
            n.antialiasing = false;
        } else if (!n.texture.empty() && n.texture.size() >= 6 && n.texture.compare(n.texture.size() - 6, 6, "-pixel") == 0) {
            n.antialiasing = false;
        }
    }
    
    if (isPixelSkinGlobal) {
        for (int i = 0; i < 4; i++) {
            customPlayerStrumAntialiasing[i] = false;
            customOpponentStrumAntialiasing[i] = false;
        }
    }
    
    showGrid = ClientPrefs::drawGrid;
    vcrFontBuf = C2D_TextBufNew(128);
    lyricsTextBuf = C2D_TextBufNew(256);
    currentLyrics = "";
    
    arrowSkinScaleFactor = 1.0f;
    if (!ClientPrefs::fastNotes) {
        auto* csNote = SpritesheetCache::get().load("shared/images/noteSkins/NOTE_assets");
        noteSheet = csNote ? csNote->sheet : nullptr;
        
        if (noteSheet) {
            C2D_Image mainNoteImg = C2D_SpriteSheetGetImage(noteSheet, 0);
            if (mainNoteImg.tex) C3D_TexSetFilter(mainNoteImg.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
            if (mainNoteImg.subtex == nullptr) {
                 printf("\x1b[10;1HERROR: NOTE_assets subtex is NULL\n");
                 return;
            }
            parseNoteXml(Paths::resolve("romfs:/shared/images/noteSkins/NOTE_assets.xml"), mainNoteImg.tex, mainNoteImg, noteSubtexs, &noteSubtexFrames);
            float defaultArrowW = (noteSubtexs.size() > 1) ? noteSubtexs[0 * 6 + 1].w : 0.0f;
            if (defaultArrowW > 0.0f) {
                arrowSkinScaleFactor = 73.0f / defaultArrowW;
            }
        } else {
            printf("\x1b[6;1HERROR: Could not load NOTE_assets\n");
        }

        if (!SongParser::arrowSkin.empty()) {
            const std::string& skinName = SongParser::arrowSkin;
            std::string skinKey = "noteSkins/" + skinName;

            std::string t3xPath = Paths::image(skinKey);          // handles mod + .rawtex + romfs
            std::string xmlPath = Paths::xml(skinKey);             // handles mod + romfs

            if (Paths::fileExists(t3xPath) && Paths::fileExists(xmlPath)) {
                C2D_SpriteSheet skinSheet = C2D_SpriteSheetLoad(t3xPath.c_str());
                if (skinSheet) {
                    C2D_Image skinImg = C2D_SpriteSheetGetImage(skinSheet, 0);
                    if (skinImg.tex && skinImg.subtex) {

                        bool isPixelSkin =
                            skinName.size() >= 6 &&
                            skinName.compare(skinName.size() - 6, 6, "-pixel") == 0;

                        C3D_TexSetFilter(
                            skinImg.tex,
                            (ClientPrefs::globalAntialiasing && !isPixelSkin) ? GPU_LINEAR : GPU_NEAREST,
                            (ClientPrefs::globalAntialiasing && !isPixelSkin) ? GPU_LINEAR : GPU_NEAREST
                        );

                        parseNoteXml(xmlPath, skinImg.tex, skinImg, noteSubtexs, &noteSubtexFrames);

                        // Compute scale so the custom skin visually matches the default note size (73.0f reference)
                        float customArrowW = (noteSubtexs.size() > 1) ? noteSubtexs[0 * 6 + 1].w : 0.0f;
                        if (customArrowW > 0.0f) {
                            arrowSkinScaleFactor = 73.0f / customArrowW;
                        }

                        // Replace the note spritesheet with the custom skin
                        noteSheet = skinSheet;

                        printf("\x1b[6;1H[ArrowSkin] Loaded: %s (scale x%.2f)\x1b[K\n",
                               skinName.c_str(), arrowSkinScaleFactor);
                    } else {
                        printf("\x1b[6;1HWARN: ArrowSkin sheet invalid: %s\x1b[K\n", skinName.c_str());
                    }
                } else {
                    printf("\x1b[6;1HWARN: ArrowSkin could not load: %s\x1b[K\n", skinName.c_str());
                }
            } else {
                printf("\x1b[6;1HWARN: ArrowSkin files missing for: %s\x1b[K\n", skinName.c_str());
            }
        }
    }

    auto* csFastNote = SpritesheetCache::get().load("shared/images/noteSkins/NoteSheetFast");
    fastNoteSheet = csFastNote ? csFastNote->sheet : nullptr;
    if (fastNoteSheet) {
        fastNoteBaseImg = C2D_SpriteSheetGetImage(fastNoteSheet, 0);
        if (fastNoteBaseImg.tex) {
            bool isPixelFast =
                SongParser::arrowSkin.size() >= 6 &&
                SongParser::arrowSkin.compare(SongParser::arrowSkin.size() - 6, 6, "-pixel") == 0;
            C3D_TexSetFilter(
                fastNoteBaseImg.tex,
                (ClientPrefs::globalAntialiasing && !isPixelFast) ? GPU_LINEAR : GPU_NEAREST,
                (ClientPrefs::globalAntialiasing && !isPixelFast) ? GPU_LINEAR : GPU_NEAREST
            );
        }
        
        static Tex3DS_SubTexture defaultFastNoteSubtex;
        if (fastNoteBaseImg.subtex == nullptr) {
            defaultFastNoteSubtex.width = fastNoteBaseImg.tex ? fastNoteBaseImg.tex->width : 0;
            defaultFastNoteSubtex.height = fastNoteBaseImg.tex ? fastNoteBaseImg.tex->height : 0;
            defaultFastNoteSubtex.left = 0.0f;
            defaultFastNoteSubtex.top = 0.0f;
            defaultFastNoteSubtex.right = 1.0f;
            defaultFastNoteSubtex.bottom = 1.0f;
            fastNoteBaseImg.subtex = &defaultFastNoteSubtex;
        }
        
        if (fastNoteBaseImg.subtex) {
            parseNoteFastXml("romfs:/shared/images/noteSkins/NoteSheetFast.xml", fastNoteBaseImg.tex, fastNoteBaseImg, fastNoteSubtexs);
            float fastArrowW = (fastNoteSubtexs.size() > 0) ? fastNoteSubtexs[0].w : 0.0f;
            if (fastArrowW > 0.0f) {
                fastNoteSkinScaleFactor = 73.0f / fastArrowW;
            }
        }
    }

    AudioEngine::initMissSounds();

    bool loadedCustomRating = false;
    if (!SongParser::ratingSkin.empty()) {
        std::string skinName = SongParser::ratingSkin;
        std::string t3xPath = Paths::image("ratingSkins/" + skinName);
        std::string xmlPath = Paths::xml("ratingSkins/" + skinName);
        
        if (Paths::fileExists(t3xPath) && Paths::fileExists(xmlPath)) {
            auto* csSkin = SpritesheetCache::get().load("images/ratingSkins/" + skinName);
            ratingSheet = csSkin ? csSkin->sheet : nullptr;
            if (ratingSheet) {
                ratingBaseImage = C2D_SpriteSheetGetImage(ratingSheet, 0);
                 if (ratingBaseImage.tex) {
                     bool isPixelRating =
                         skinName.size() >= 6 &&
                         skinName.compare(skinName.size() - 6, 6, "-pixel") == 0;
                     C3D_TexSetFilter(
                         ratingBaseImage.tex,
                         (ClientPrefs::globalAntialiasing && !isPixelRating) ? GPU_LINEAR : GPU_NEAREST,
                         (ClientPrefs::globalAntialiasing && !isPixelRating) ? GPU_LINEAR : GPU_NEAREST
                     );
                 }
                
                static Tex3DS_SubTexture defaultRatingSubtex;
                if (ratingBaseImage.subtex == nullptr) {
                    defaultRatingSubtex.width = ratingBaseImage.tex ? ratingBaseImage.tex->width : 0;
                    defaultRatingSubtex.height = ratingBaseImage.tex ? ratingBaseImage.tex->height : 0;
                    defaultRatingSubtex.left = 0.0f;
                    defaultRatingSubtex.top = 0.0f;
                    defaultRatingSubtex.right = 1.0f;
                    defaultRatingSubtex.bottom = 1.0f;
                    ratingBaseImage.subtex = &defaultRatingSubtex;
                }
                
                if (ratingBaseImage.subtex != nullptr) {
                    std::ifstream file(xmlPath);
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
                            
                            Tex3DS_SubTexture sub;
                            float x = atof(getVal("x").c_str());
                            float y = atof(getVal("y").c_str());
                            float w = atof(getVal("width").c_str());
                            float h = atof(getVal("height").c_str());
                            
                            sub.width = (u16)w;
                            sub.height = (u16)h;
                            if (ratingBaseImage.subtex) {
                                sub.left = ratingBaseImage.subtex->left + (x * rw / (float)ratingBaseImage.subtex->width);
                                sub.top = ratingBaseImage.subtex->top + (y * rh / (float)ratingBaseImage.subtex->height);
                                sub.right = ratingBaseImage.subtex->left + ((x + w) * rw / (float)ratingBaseImage.subtex->width);
                                sub.bottom = ratingBaseImage.subtex->top + ((y + h) * rh / (float)ratingBaseImage.subtex->height);
                            }
                            
                            ratingSubtexs[id] = sub;
                        }
                    }
                    loadedCustomRating = true;
                    printf("[RatingSkin] Loaded custom rating skin: %s\n", skinName.c_str());
                }
            }
        }
    }

    if (!loadedCustomRating) {
        auto* csRating = SpritesheetCache::get().load("shared/images/ratingSkins/rating");
        ratingSheet = csRating ? csRating->sheet : nullptr;
        if (ratingSheet) {
            ratingBaseImage = C2D_SpriteSheetGetImage(ratingSheet, 0);
            if (ratingBaseImage.tex) C3D_TexSetFilter(ratingBaseImage.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
            
            static Tex3DS_SubTexture defaultRatingSubtex;
            if (ratingBaseImage.subtex == nullptr) {
                defaultRatingSubtex.width = ratingBaseImage.tex ? ratingBaseImage.tex->width : 0;
                defaultRatingSubtex.height = ratingBaseImage.tex ? ratingBaseImage.tex->height : 0;
                defaultRatingSubtex.left = 0.0f;
                defaultRatingSubtex.top = 0.0f;
                defaultRatingSubtex.right = 1.0f;
                defaultRatingSubtex.bottom = 1.0f;
                ratingBaseImage.subtex = &defaultRatingSubtex;
            }
            
            if (ratingBaseImage.subtex != nullptr) {
                std::ifstream file("romfs:/shared/images/ratingSkins/rating.xml");
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
                        
                        Tex3DS_SubTexture sub;
                        float x = atof(getVal("x").c_str());
                        float y = atof(getVal("y").c_str());
                        float w = atof(getVal("width").c_str());
                        float h = atof(getVal("height").c_str());
                        
                        sub.width = (u16)w;
                        sub.height = (u16)h;
                        if (ratingBaseImage.subtex) {
                            sub.left = ratingBaseImage.subtex->left + (x * rw / (float)ratingBaseImage.subtex->width);
                            sub.top = ratingBaseImage.subtex->top + (y * rh / (float)ratingBaseImage.subtex->height);
                            sub.right = ratingBaseImage.subtex->left + ((x + w) * rw / (float)ratingBaseImage.subtex->width);
                            sub.bottom = ratingBaseImage.subtex->top + ((y + h) * rh / (float)ratingBaseImage.subtex->height);
                        }
                        
                        ratingSubtexs[id] = sub;
                    }
                }
            }
        } else {
            printf("\x1b[6;1HERROR: Could not load ratingSheet\n");
        }
    }

    AudioEngine::initCountdownSounds();
    auto* csCountdown = SpritesheetCache::get().load("shared/images/countdown");
    countdownSheet = csCountdown ? csCountdown->sheet : nullptr;
    if (countdownSheet) {
        C2D_Image cBaseImage = C2D_SpriteSheetGetImage(countdownSheet, 0);
        if (cBaseImage.tex) C3D_TexSetFilter(cBaseImage.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        if (cBaseImage.subtex != nullptr) {
            std::ifstream file("romfs:/shared/images/countdown.xml");
            std::string line;
            float rw = cBaseImage.subtex->right - cBaseImage.subtex->left;
            float rh = cBaseImage.subtex->bottom - cBaseImage.subtex->top;
            
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
                    std::string id = "go";
                    if (nameInfo.find("ready") != std::string::npos) id = "ready";
                    else if (nameInfo.find("set") != std::string::npos) id = "set";
                    
                    Tex3DS_SubTexture sub;
                    float x = atof(getVal("x").c_str());
                    float y = atof(getVal("y").c_str());
                    float w = atof(getVal("width").c_str());
                    float h = atof(getVal("height").c_str());
                    
                    sub.width = (u16)w;
                    sub.height = (u16)h;
                    sub.left = cBaseImage.subtex->left + (x * rw / (float)cBaseImage.subtex->width);
                    sub.top = cBaseImage.subtex->top + (y * rh / (float)cBaseImage.subtex->height);
                    sub.right = cBaseImage.subtex->left + ((x + w) * rw / (float)cBaseImage.subtex->width);
                    sub.bottom = cBaseImage.subtex->top + ((y + h) * rh / (float)cBaseImage.subtex->height);
                    
                    countdownSubtexs[id] = sub;
                }
            }
        }
    }

    printf("\x1b[4;1HNotes: %d\x1b[K\n", (int)songNotes.size());

    if (songNotes.empty()) {
        printf("\x1b[9;1HWARN: Chart empty or not found!\n");
    }
    
    printf("\x1b[5;1HSorting notes...\n");
    std::sort(songNotes.begin(), songNotes.end(), [](const Note& a, const Note& b) {
        return a.strumTime < b.strumTime;
    });
    printf("\x1b[5;1HSorting notes... OK\n");

    std::string instPath = Paths::audio("songs/" + curSong, "inst.ogg");
    std::string vocPath = Paths::audio("songs/" + curSong, "Voices.ogg");
    if (!Paths::fileExists(vocPath)) vocPath = Paths::audio("songs/" + curSong, "voices.ogg");

    if (!AudioEngine::init(instPath.c_str(), vocPath.c_str())) {
        printf("\x1b[9;1HERROR: Could not load audio!\n");
    }
    
    gf = new Character();
    gf->loadFromPsychJson(Paths::characterJson(SongParser::gfVersion));
    if (gf->curAnim.empty()) gf->loadFromPsychJson(Paths::characterJson("gf")); // fallback
    if (gf->curCharacterName.empty() || gf->curCharacterName == "bf-pixel") {
        gf->curCharacterName = SongParser::gfVersion.empty() ? "gf" : SongParser::gfVersion;
    }
    AsyncAssetManager::get().cacheCharacter(gf->curCharacterName, gf);
    
    bf = new Character();
    bf->loadFromPsychJson(Paths::characterJson(SongParser::player1));
    if (bf->curAnim.empty()) bf->loadFromPsychJson(Paths::characterJson("bf-pixel")); // fallback
    bf->isPlayer = true;
    if (bf->curCharacterName.empty()) {
        bf->curCharacterName = SongParser::player1.empty() ? "bf" : SongParser::player1;
    }
    AsyncAssetManager::get().cacheCharacter(bf->curCharacterName, bf);

    dad = new Character();
    dad->loadFromPsychJson(Paths::characterJson(SongParser::player2));
    if (dad->curAnim.empty()) dad->loadFromPsychJson(Paths::characterJson("bf-pixel")); // fallback
    if (dad->curCharacterName.empty()) {
        dad->curCharacterName = SongParser::player2.empty() ? "dad" : SongParser::player2;
    }
    AsyncAssetManager::get().cacheCharacter(dad->curCharacterName, dad);

    currentStage = new Stage(Paths::stageJson(SongParser::stage));
    
    // Set initial character positions from stage data
    if (bf) { bf->x += currentStage->bfX; bf->y += currentStage->bfY; }
    if (dad) { dad->x += currentStage->dadX; dad->y += currentStage->dadY; }
    if (gf) { gf->x += currentStage->gfX; gf->y += currentStage->gfY; }

    targetZoom = currentStage->defaultZoom;
    camZoom = targetZoom;
    
    if (!songData.sections.empty()) {
        curSection = 0;
        focusCamera(songData.sections[0].mustHitSection);
        camX = camFollowX;
        camY = camFollowY;
    } else if (currentStage && dad) {
        focusCamera(false);
        camX = camFollowX;
        camY = camFollowY;
    }
    
    nextNoteIndex = 0;

    receptorY = ClientPrefs::downscroll ? ScreenHeight - 60.0f : 20.0f;
    spacing = 38.0f;
    bool useFast = ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 2;
    noteScale = 0.5f * (useFast ? fastNoteSkinScaleFactor : arrowSkinScaleFactor);
    
    startTimerActive = true;
    countdownTick = 0;
    countdownActive = false;
    countdownAlpha = 0.0f;
    countdownScale = 1.0f;
    countdownYOffset = 0.0f;
    countdownTimer = 0.0f;
    currentCountdownFrame = "";
    
    Conductor::songPosition = -(Conductor::crochet * 5);
    
    musicStarted = false;
    lastBeat = 0;
    health = 1.0f;
    vocalMuteTimer = 0.0f;
    songLength = AudioEngine::getTotalTime();
    
    score = 0;
    misses = 0;
    combo = 0;
    maxCombo = 0;
    totalNoteScore = 0;
    accuracy = 0.0f;
    iconBump = 1.0f;
    hudZoom = 1.0f;
    scoreZoom = 1.0f;
    
    autoIconPosition = true;
    iconP1X = -9999.0f;
    iconP1Y = -9999.0f;
    iconP2X = -9999.0f;
    iconP2Y = -9999.0f;
    iconP1Alpha = 1.0f;
    iconP2Alpha = 1.0f;
    iconP1Scale = 1.0f;
    iconP2Scale = 1.0f;
    iconP1Visible = true;
    iconP2Visible = true;
    
    curSongDifficulties.clear();
    std::string diffStr = "";
    if (isStoryMode) {
        diffStr = weekData.difficulties;
    } else {
        for (const auto& pair : WeekData::weeksLoaded) {
            for (const auto& s : pair.second.songs) {
                if (s.name == curSong) {
                    diffStr = pair.second.difficulties;
                    break;
                }
            }
            if (!diffStr.empty()) break;
        }
    }
    if (!diffStr.empty()) {
        std::stringstream ss(diffStr);
        std::string d;
        while (std::getline(ss, d, ',')) {
            size_t first = d.find_first_not_of(' ');
            if (first == std::string::npos) continue;
            size_t last = d.find_last_not_of(' ');
            curSongDifficulties.push_back(d.substr(first, last - first + 1));
        }
    }
    if (curSongDifficulties.empty()) {
        curSongDifficulties = {"Easy", "Normal", "Hard"};
    }

    lastBeat = -1;

    if (Conductor::stepCrochet == 0) Conductor::stepCrochet = 1.0f; // Prevent div by zero
    
    MusicBeatState::init();
    printf("\x1b[12;1HInit Base... OK\n");
    ratingName = "?";
    gridOffset = 0.0f;
    
    loadHealthIcon(iconBf,  bf  ? bf->healthIcon  : "face");
    loadHealthIcon(iconDad, dad ? dad->healthIcon : "face");

    score = 0; misses = 0; hits = 0; combo = 0; maxCombo = 0;
    lastBeat = -1;
    pOffsetX = 0; pOffsetY = 0; eOffsetX = 0; eOffsetY = 0;

    
    for(int i=0; i<4; i++) {
        keyPressed[i] = false; keyHeld[i] = false; keyHitRecently[i] = false;
        receptorTimer[i] = 0; receptorTimer[4+i] = 0;
    }
    for(int i=0; i<8; i++) {
        receptorActiveSlot[i] = 1;
        receptorAnimTime[i] = 0.0f;
    }

    LuaManager::get().init();

    // --- Instantiate Note Underlay Sprites ---
    {
        auto createUnderlaySprite = [&](const std::string& tag, float defaultAlpha, float x, float w, u32 defaultColor) {
            StageSprite s;
            s.name = tag;
            s.isGraphic = true;
            s.graphicColor = defaultColor;
            s.alpha = defaultAlpha;
            s.visible = (defaultAlpha > 0.01f);
            s.camera = "camHUD";
            s.front = false; // Behind receptors
            s.x = x;
            s.y = 0.0f;
            s.graphicWidth = w;
            s.graphicHeight = ScreenHeight; // 240.0f
            s.scaleX = 1.0f;
            s.scaleY = 1.0f;
            s.scrollX = 1.0f;
            s.scrollY = 1.0f;
            
            luaSpriteIndices[tag] = luaSprites.size();
            luaSprites.push_back(s);
        };

        float padding = 4.0f;
        // 1. bfNoteUnderlay
        createUnderlaySprite("bfNoteUnderlay", ClientPrefs::noteUnderlayAlpha, getLaneX(0, true) - padding, (getLaneX(3, true) + spacing) - getLaneX(0, true) + 2.0f * padding, playerUnderlayColor);

        // 2. dadNoteUnderlay (always created, but set visible = false if middleScroll is active)
        bool showOpp = ClientPrefs::opponentStrums && ClientPrefs::opponentUnderlay;
        createUnderlaySprite("dadNoteUnderlay", (!ClientPrefs::middleScroll && showOpp) ? ClientPrefs::noteUnderlayAlpha : 0.0f, getLaneX(0, false) - padding, (getLaneX(3, false) + spacing) - getLaneX(0, false) + 2.0f * padding, opponentUnderlayColor);
        if (ClientPrefs::middleScroll) {
            luaSprites[luaSpriteIndices["dadNoteUnderlay"]].visible = false;
        }

        // 3. dadNoteUnderlayL
        createUnderlaySprite("dadNoteUnderlayL", (ClientPrefs::middleScroll && showOpp) ? ClientPrefs::noteUnderlayAlpha : 0.0f, getLaneX(0, false) - padding, (getLaneX(1, false) + spacing) - getLaneX(0, false) + 2.0f * padding, opponentUnderlayColor);

        // 4. dadNoteUnderlayR
        createUnderlaySprite("dadNoteUnderlayR", (ClientPrefs::middleScroll && showOpp) ? ClientPrefs::noteUnderlayAlpha : 0.0f, getLaneX(2, false) - padding, (getLaneX(3, false) + spacing) - getLaneX(2, false) + 2.0f * padding, opponentUnderlayColor);
    }
    
    // Use same base path as stageJson but swap extension to .lua
    std::string stageName = SongParser::stage;
    std::string luaPath = "romfs:/preload/stages/" + stageName + ".lua";
    
    std::string modLuaPath = ModHandler::get().getModPath("stages/" + stageName + ".lua");
    if (!modLuaPath.empty()) {
        luaPath = modLuaPath;
    } else if (!Paths::fileExists(luaPath) && !Paths::fileExists(Paths::stageJson(stageName))) {
        stageName = "StageTest";
        luaPath = "romfs:/preload/stages/StageTest.lua";
    }

    // Load global Lua scripts first
    auto globalScripts = Paths::globalLuaScripts();
    for (const auto& scriptPath : globalScripts) {
        LuaManager::get().runScript(scriptPath);
    }

    if (Paths::fileExists(luaPath)) {
        LuaManager::get().runScript(luaPath);
    }

    // Load song-specific Lua scripts from romfs:/preload/data/<song>/
    auto songScripts = Paths::songLuaScripts(curSong);
    for (const auto& scriptPath : songScripts) {
        LuaManager::get().runScript(scriptPath);
    }

    // Load custom events from the song chart
    std::vector<std::string> eventTypes;
    for (const auto& ev : songData.events) {
        if (std::find(eventTypes.begin(), eventTypes.end(), ev.name) == eventTypes.end()) {
            eventTypes.push_back(ev.name);
        }
    }
    auto eventScripts = Paths::eventLuaScripts(eventTypes);
    for (const auto& scriptPath : eventScripts) {
        LuaManager::get().runScript(scriptPath);
    }
    
    // Load custom notes from the song chart
    std::vector<std::string> noteTypes;
    for (const auto& n : songData.notes) {
        if (!n.noteType.empty() && std::find(noteTypes.begin(), noteTypes.end(), n.noteType) == noteTypes.end()) {
            noteTypes.push_back(n.noteType);
        }
    }
    for (const auto& nt : noteTypes) {
        std::string scriptPath = Paths::customNoteLuaScript(nt);
        if (!scriptPath.empty()) {
            LuaManager::get().runScript(scriptPath);
        }
    }

    LuaManager::get().callFunction("onCreate");
    LuaManager::get().callFunction("onCreatePost");


    // Cache any custom textures applied by Lua scripts in onCreatePost
    for (const auto& n : songNotes) {
        if (n.texture.empty()) continue;
        if (customNoteSheets.find(n.texture) != customNoteSheets.end()) continue;
        if (customNoteImages.find(n.texture) != customNoteImages.end()) continue;

        std::string tPath = Paths::image(n.texture);
        std::string rawPath = ModHandler::get().getModPath("images/" + n.texture + ".rawtex");
        if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + n.texture + ".rawtex")) {
            rawPath = "romfs:/preload/images/" + n.texture + ".rawtex";
        }
        if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + n.texture + ".rawtex")) {
            rawPath = "romfs:/shared/images/" + n.texture + ".rawtex";
        }
        
        if (Paths::fileExists(rawPath)) {
            FILE* f = fopen(rawPath.c_str(), "rb");
            if (f) {
                struct RawTexHeader { char magic[4]; uint16_t w; uint16_t h; uint16_t ow; uint16_t oh; } header;
                if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                    size_t sz = (size_t)header.w * header.h * 4;
                    void* tempBuf = linearAlloc(sz);
                    if (tempBuf) {
                        fread(tempBuf, sz, 1, f);
                        C3D_Tex* tex = new C3D_Tex();
                        if (C3D_TexInit(tex, header.w, header.h, GPU_RGBA8)) {
                            if (tex->data) linearFree(tex->data);
                            tex->data = tempBuf;
                            C3D_TexFlush(tex);
                            
                            bool isPixelNote =
                                n.texture.size() >= 6 &&
                                n.texture.compare(n.texture.size() - 6, 6, "-pixel") == 0;
                            C3D_TexSetFilter(
                                tex,
                                (ClientPrefs::globalAntialiasing && !isPixelNote) ? GPU_LINEAR : GPU_NEAREST,
                                (ClientPrefs::globalAntialiasing && !isPixelNote) ? GPU_LINEAR : GPU_NEAREST
                            );
                            
                            Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                            sub->width = header.ow;
                            sub->height = header.oh;
                            sub->left = 0.0f;
                            sub->top = 1.0f;
                            sub->right = (float)header.ow / header.w;
                            sub->bottom = 1.0f - ((float)header.oh / header.h);
                            
                            C2D_Image img;
                            img.tex = tex;
                            img.subtex = sub;
                            customNoteImages[n.texture] = img;
                        } else {
                            delete tex;
                            linearFree(tempBuf);
                        }
                    }
                }
                fclose(f);
            }
        } else if (Paths::fileExists(tPath)) {
            auto* csCustom = SpritesheetCache::get().load("images/" + n.texture);
            C2D_SpriteSheet sheet = csCustom ? csCustom->sheet : nullptr;
            if (sheet) {
                customNoteSheets[n.texture] = sheet;
                C2D_Image img = C2D_SpriteSheetGetImage(sheet, 0);
                if (img.tex) {
                    bool isPixelNote =
                        n.texture.size() >= 6 &&
                        n.texture.compare(n.texture.size() - 6, 6, "-pixel") == 0;
                    C3D_TexSetFilter(
                        img.tex,
                        (ClientPrefs::globalAntialiasing && !isPixelNote) ? GPU_LINEAR : GPU_NEAREST,
                        (ClientPrefs::globalAntialiasing && !isPixelNote) ? GPU_LINEAR : GPU_NEAREST
                    );
                }
            } else {
                printf("\x1b[14;1HWARN: Custom note texture not found: %s\x1b[K\n", n.texture.c_str());
            }
        } else {
            printf("\x1b[14;1HWARN: Custom note texture not found: %s\x1b[K\n", n.texture.c_str());
        }
    }

    // Resume background loading thread now that all init IO is complete
    AsyncAssetManager::get().resume();
}

void PlayState::updateCamera(float dt) {
    gridOffset += dt * 32.0f;

    if (!songData.sections.empty()) {
        for (int i = 0; i < (int)songData.sections.size(); i++) {
            if (Conductor::songPosition >= songData.sections[i].startTime && Conductor::songPosition < songData.sections[i].endTime) {
                if (curSection != i) {
                    curSection = i;
                    focusCamera(songData.sections[i].mustHitSection);
                }
                break;
            }
        }
    }

    camZoom = lerp(camZoom, targetZoom, dt * 3.1f);
    hudZoom = lerp(hudZoom, 1.0f, dt * 3.1f);
    iconBump = lerp(iconBump, 1.0f, dt * 8.0f);
    scoreZoom = lerp(scoreZoom, 1.0f, dt * 10.0f);

    if (autoIconPosition) {
        float screenScale = 240.0f / 720.0f;
        float healthBarW = 200.0f;
        float healthBarH = 5.0f;
        float healthBarX = (ScreenWidthTop - healthBarW) / 2.0f;
        float healthBarY = ClientPrefs::downscroll ? 20.0f : ScreenHeight - 20.0f;
        float healthPerc = health / 2.0f;

        float unzoomed_divX  = healthBarX + healthBarW * (1.0f - healthPerc);
        float unzoomed_iconSz = 42.0f;
        float iconHalf = unzoomed_iconSz * 0.5f;               // 21px
        float iconCenterY = healthBarY + healthBarH * 0.5f;    // vertical center of the bar

        // P2 (enemy, positive scale): C2D_DrawImageAtRotated is centered → store center = divX - iconHalf
        // P1 (player, negative scale): anchor is LEFT EDGE of flipped image → store divX so center lands at divX + iconHalf
        iconP2X = (unzoomed_divX - iconHalf) / screenScale;
        iconP2Y = iconCenterY / screenScale;
        iconP1X = unzoomed_divX / screenScale;   // store visual center of P1 (= divX)
        iconP1Y = iconCenterY / screenScale;
    }

    if (ratingActive) {
        ratingVelY += ratingAccelY * dt;
        ratingY += ratingVelY * dt;
        
        // Start fading when falling
        if (ratingVelY > 0) {
            ratingAlpha -= dt * 3.5f; // Fade during fall
            if (ratingAlpha <= 0.0f) ratingActive = false;
        }
    }

    float camSpeedMult = 1.0f;
    if (currentStage) camSpeedMult = currentStage->cameraSpeed;
    
    float camLerp = dt * 2.4f * camSpeedMult;
    if (camLerp > 1.0f) camLerp = 1.0f;
    camX = lerp(camX, camFollowX, camLerp);
    camY = lerp(camY, camFollowY, camLerp);
}

void PlayState::updateNotesLogic(float dt) {
    p3DS = (240.0f/720.0f) * SongParser::songSpeed * 0.45f;
    if (ClientPrefs::middleScroll) {
        playerX = (ScreenWidthTop / 2.0f) - (spacing * 1.5f) - (spacing / 2.0f);
    } else {
        playerX = ScreenWidthTop - (spacing * 4.0f) - 16.0f;
    }

    if (!endedSong && AudioEngine::isFinished()) {
        endedSong = true;
        endSong();
        return;
    }


    while (nextNoteIndex < songNotes.size()) {
        Note& n = songNotes[nextNoteIndex];
        float noteEnd = n.strumTime + n.sustainLength;
        if (Conductor::songPosition > noteEnd + std::max(1000.0f, (ScreenHeight/p3DS))) {
            nextNoteIndex++;
        } else break;
    }

    for (int i = 0; i < 4; i++) {
        if (receptorTimer[4+i] > 0) receptorTimer[4+i] -= dt;
        if (receptorTimer[i] > 0) receptorTimer[i] -= dt;

        // Opponent receptor (lane i)
        int oppTargetSlot = 1; // Static
        if (receptorTimer[i] > 0.0f) {
            oppTargetSlot = 0; // Confirm
        }
        if (oppTargetSlot != receptorActiveSlot[i]) {
            receptorActiveSlot[i] = oppTargetSlot;
            receptorAnimTime[i] = 0.0f;
        } else {
            receptorAnimTime[i] += dt;
        }

        // Player receptor (lane 4 + i)
        int playerTargetSlot = 1; // Static
        bool isBotHolding = ClientPrefs::botPlay && (receptorTimer[4 + i] > 0.0f);
        if (keyHeld[i] || isBotHolding) {
            playerTargetSlot = (keyHitRecently[i] || isBotHolding) ? 0 : 3; // Confirm or Pressed
        }
        if (playerTargetSlot != receptorActiveSlot[4 + i]) {
            receptorActiveSlot[4 + i] = playerTargetSlot;
            receptorAnimTime[4 + i] = 0.0f;
        } else {
            receptorAnimTime[4 + i] += dt;
        }
    }

    size_t logicLimit = nextNoteIndex + 80;
    if (logicLimit > songNotes.size()) logicLimit = songNotes.size();

    for (size_t i = nextNoteIndex; i < logicLimit; i++) {
        Note& n = songNotes[i];
        
        // Skip fully processed notes
        if (n.hit && !n.sustainActive) continue;

        float diff = n.strumTime - Conductor::songPosition;

        // Optimization: don't process logic for notes too far in the future
        float spawnTime = std::min(5000.0f, std::max(2000.0f, (ScreenHeight / p3DS) + 100.0f));
        if (diff > spawnTime) break;

        // --- OPPONENT HIT LOGIC ---
        if (!n.hit && !n.isPlayer && diff <= 0) {
            n.hit = true;
            n.wasGoodHit = true;
            if (n.sustainLength > 0.0f) n.sustainActive = true;
            
            receptorTimer[n.noteData] = 0.12f;
            
            Character* singer = n.isPlayer ? bf : dad;
            if (n.gfNote || (curSection >= 0 && curSection < (int)songData.sections.size() && songData.sections[curSection].gfSection))
                singer = gf;

            moveChar(n.noteData, singer);
            AudioEngine::setVocalsVolume(1.0f);
            
            LuaManager::get().callFunction("opponentNoteHit", {std::to_string(i), std::to_string(n.noteData), n.noteType, n.sustainLength > 0.0f ? "true" : "false"});
        }

        // --- BOT PLAY AUTO-HIT LOGIC ---
        if (!n.hit && n.isPlayer && diff <= 0 && ClientPrefs::botPlay) {
            n.hit = true;
            n.wasGoodHit = true;
            if (n.sustainLength > 0.0f) n.sustainActive = true;

            receptorTimer[4 + n.noteData] = 0.10f;

            Character* singer = bf;
            if (n.gfNote || (curSection >= 0 && curSection < (int)songData.sections.size() && songData.sections[curSection].gfSection))
                singer = gf;

            moveChar(n.noteData, singer);
            AudioEngine::setVocalsVolume(1.0f);
            
            keyHitRecently[n.noteData] = true;

            health += 0.023f; if (health > 2.0f) health = 2.0f;

            size_t id = &n - &songNotes[0];
            LuaManager::get().callFunction("goodNoteHit", {std::to_string(id), std::to_string(n.noteData), n.noteType, n.sustainLength > 0.0f ? "true" : "false"});
        }

        // --- PLAYER MISS LOGIC (Head missed) ---
        if (!n.hit && !n.ignoreNote && n.isPlayer && diff < -166.0f) {
            misses++; combo = 0; health -= 0.0475f; if (health < 0.0f) health = 0.0f;
            n.hit = true; n.ignoreNote = true;
            AudioEngine::setVocalsVolume(0.0f);
            AudioEngine::playMissSound();
            vocalMuteTimer = 0.5f;
            totalNotesHit++; accuracy = (totalNoteScore / totalNotesHit) * 100.0f;
            
            LuaManager::get().callFunction("noteMiss", {std::to_string(i), std::to_string(n.noteData), n.noteType, n.sustainLength > 0.0f ? "true" : "false"});
            
            Character* misser = n.isPlayer ? bf : dad;
            if (n.gfNote || (curSection >= 0 && curSection < (int)songData.sections.size() && songData.sections[curSection].gfSection))
                misser = gf;

            if (misser) {
                static const std::string dirs[] = {"LEFT", "DOWN", "UP", "RIGHT"};
                misser->playAnim("sing" + dirs[n.noteData] + "miss", true);
            }
        }

        // --- SUSTAIN LOGIC (Both Player and Opponent) ---
        if (n.wasGoodHit && n.sustainLength > 0.0f && n.sustainActive) {
            float endDiff = (n.strumTime + n.sustainLength) - Conductor::songPosition;
            if (endDiff > 0) {
                // Still holding
                if (n.isPlayer) {
                    Character* singer = bf;
                    if (n.gfNote || (curSection >= 0 && curSection < (int)songData.sections.size() && songData.sections[curSection].gfSection))
                        singer = gf;

                    if (singer) singer->holdTimer = 0;
                    if (singer && singer->curAnim.find("sing") == std::string::npos && singer->curAnim.find("miss") == std::string::npos) {
                        moveChar(n.noteData, singer); health += 0.005f; if (health > 2.0f) health = 2.0f;
                    }
                    if (ClientPrefs::botPlay) {
                        receptorTimer[4 + n.noteData] = 0.12f;
                    }
                } else {
                    Character* singer = dad;
                    if (n.gfNote || (curSection >= 0 && curSection < (int)songData.sections.size() && songData.sections[curSection].gfSection))
                        singer = gf;

                    if (singer) singer->holdTimer = 0;
                    if (singer && singer->curAnim.find("sing") == std::string::npos && singer->curAnim.find("miss") == std::string::npos) {
                        moveChar(n.noteData, singer);
                    }
                    receptorTimer[n.noteData] = 0.12f;
                }
            } else {
                // Sustain finished
                n.sustainActive = false;
            }

            // Sync: Reset holdTimer while hitting a sustain to avoid idle mid-sing
            if (n.isPlayer && bf) bf->holdTimer = 0;
            else if (n.sustainActive && dad) dad->holdTimer = 0;
        }
    }
}

#include <jansson.h>
#include <malloc.h>

void PlayState::update(float dt) {
    if (gameOver) {
        LuaManager::get().callFunction("onUpdate", {std::to_string(dt)});
        LuaManager::get().callFunction("onUpdatePost", {std::to_string(dt)});

        gameOverTimer += dt;

        if (deadBF) {
            deadBF->update(dt);
        }

        // Camera follow (instant follow start, then smooth lerp)
        if (gameOverTimer >= 0.5f) {
            deathCamFollowStarted = true;
        }

        if (deathCamFollowStarted) {
            float camLerp = 1.0f - expf(-dt * 0.6f);
            camX = camX + (deathCamFollowX - camX) * camLerp;
            camY = camY + (deathCamFollowY - camY) * camLerp;
            // Original FNF does not lerp camera zoom in Game Over, it stays at the stage's default
        }

        // Update conductor song position based on MusicPlayer time to run steps/beats/Lua updates
        if (MusicPlayer::isPlaying()) {
            Conductor::songPosition = MusicPlayer::getPosition();
            Conductor::update(Conductor::songPosition);
        }

        // Loop music after first death animation finishes
        if (deadBF && (deadBF->animFinished || deadBF->curAnim != "firstDeath")) {
            if (!deathLoopPlaying && !deathConfirmActive) {
                deathLoopPlaying = true;
                std::string oggPath = Paths::audio("shared/sounds", "gameOver.ogg");
                bool ok = false;
                if (Paths::fileExists(oggPath)) {
                    ok = MusicPlayer::play(oggPath.c_str(), 1.0f);
                }
                if (!ok) {
                    MusicPlayer::playMenuMusic();
                }
                if (deadBF->hasAnimation("deathLoop")) {
                    deadBF->playAnim("deathLoop");
                }
            }
        }

        // Handle inputs
        u32 kDown = hidKeysDown();
        if (kDown & (KEY_A | KEY_START)) {
            if (!deathConfirmActive) {
                deathConfirmActive = true;
                deathConfirmTimer = 0.0f;
                MusicPlayer::stop();

                std::string confirmSfx = Paths::audio("shared/sounds", "gameOverEnd.ogg");
                if (Paths::fileExists(confirmSfx)) {
                    AudioEngine::playSound(confirmSfx, 1.0f);
                } else {
                    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 1.0f);
                }

                if (deadBF && deadBF->hasAnimation("deathConfirm")) {
                    deadBF->playAnim("deathConfirm", true);
                }

                LuaManager::get().callFunction("onGameOverConfirm", {"true"});
            }
        } else if (kDown & (KEY_B | KEY_SELECT)) {
            if (!deathConfirmActive) {
                MusicPlayer::stop();
                PlayState::instance = nullptr;

                if (isStoryMode) {
                    MusicBeatState::switchState(new StoryMenuState());
                } else {
                    MusicBeatState::switchState(new FreeplayState());
                }
                MusicPlayer::playMenuMusic();
                return;
            }
        }

        // Re-attempt restart timer and fade
        if (deathConfirmActive) {
            deathConfirmTimer += dt;
            if (deathConfirmTimer >= 2.5f) {
                if (isStoryMode) {
                    MusicBeatState::switchState(new PlayState(weekData, curSongIdx, currentDifficulty));
                } else {
                    MusicBeatState::switchState(new PlayState(curSong, currentDifficulty));
                }
                return;
            }
        }
        return;
    }

    ShaderManager::get().update(dt);
    if (inGameVideo) {
        inGameVideo->update(dt);
        if (inGameVideo->finished) {
            stopVideo();
            LuaManager::get().callFunction("onVideoEnded");
        }
    }

    if (bf && bf->curAnim.find("idle") != std::string::npos) {
        bfWentIdle = true;
    }
    
    // Process deferred character cleanup — safe here because C3D_FrameBegin already
    // synced the GPU for the previous frame before update() is called.
    if (deferredClearUnused) {
        deferredClearUnused = false;
        std::vector<std::string> activeNames;
        std::vector<Character*> activePointers;
        if (bf)  { activePointers.push_back(bf);  if (!bf->curCharacterName.empty())  activeNames.push_back(bf->curCharacterName); }
        if (dad) { activePointers.push_back(dad); if (!dad->curCharacterName.empty()) activeNames.push_back(dad->curCharacterName); }
        if (gf)  { activePointers.push_back(gf);  if (!gf->curCharacterName.empty())  activeNames.push_back(gf->curCharacterName); }
        
        // Keep any pending swap targets alive too
        for (const auto& swap : pendingSwaps) {
            if (!swap.charName.empty()) activeNames.push_back(swap.charName);
        }

        int keptFutureCount = 0;
        std::vector<std::string> keptFutureNames;
        float keepTimeLimit = Conductor::songPosition + 15000.0f; // 15 seconds ahead
        for (size_t i = nextEventIndex; i < songData.events.size(); i++) {
            const auto& ev = songData.events[i];
            if (ev.strumTime > keepTimeLimit || keptFutureCount >= 2) break;
            if (ev.name == "Change Character") {
                if (std::find(keptFutureNames.begin(), keptFutureNames.end(), ev.value2) == keptFutureNames.end()) {
                    keptFutureNames.push_back(ev.value2);
                    keptFutureCount++;
                }
                activeNames.push_back(ev.value2);
            }
        }
        AsyncAssetManager::get().clearUnused(activeNames, activePointers);
    }

    AsyncAssetManager::get().update();

    if (!pendingSwaps.empty()) {
        for (auto it = pendingSwaps.begin(); it != pendingSwaps.end(); ) {
            Character* newChar = AsyncAssetManager::get().getCharacter(it->charName);
            if (newChar) {
                applyCharacterSwap(it->charType, it->charName, newChar);
                it = pendingSwaps.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Lookahead for Change Character events (limit to next 15 seconds and max 2 upcoming characters to protect Linear RAM)
    float lookaheadTime = Conductor::songPosition + 15000.0f; // 15 seconds ahead
    int requestedCount = 0;
    std::vector<std::string> requestedNames;
    for (size_t i = nextEventIndex; i < songData.events.size(); i++) {
        const auto& ev = songData.events[i];
        if (ev.strumTime > lookaheadTime || requestedCount >= 2) break;
        if (ev.name == "Change Character") {
            if (std::find(requestedNames.begin(), requestedNames.end(), ev.value2) == requestedNames.end()) {
                requestedNames.push_back(ev.value2);
                requestedCount++;
            }
            AsyncAssetManager::get().requestCharacter(ev.value2);
            Character* cachedChar = AsyncAssetManager::get().getCharacter(ev.value2);
            if (cachedChar) {
                AsyncAssetManager::get().requestHealthIcon(cachedChar->healthIcon);
                if (AsyncAssetManager::get().isHealthIconReady(cachedChar->healthIcon) && healthIconCache.count(cachedChar->healthIcon) == 0) {
                    HealthIconData dummyIcon;
                    loadHealthIcon(dummyIcon, cachedChar->healthIcon);
                }
            }
        }
    }


    while (nextEventIndex < songData.events.size() && songData.events[nextEventIndex].strumTime <= Conductor::songPosition) {
        triggerEvent(songData.events[nextEventIndex]);
        nextEventIndex++;
    }

    if (dt > 0.05f) dt = 0.05f; // Cap at ~20 FPS to prevent huge lag spikes from skipping the countdown
    
    u32 kDown = hidKeysDown();
    if (kDown & KEY_START) {
        paused = !paused;
        if (paused) {
            if (!pauseSubState) {
                pauseSubState = new PauseSubState();
            }
            AudioEngine::pause();
            if (inGameVideo) inGameVideo->isPaused = true;
        } else {
            if (pauseSubState) {
                delete pauseSubState;
                pauseSubState = nullptr;
            }
            AudioEngine::resume();
            if (inGameVideo) inGameVideo->isPaused = false;
        }
    }

    if (paused) {
        if (pauseSubState) {
            pauseSubState->update(dt);
        }
        return;
    }

    LuaManager::get().callFunction("onUpdate", {std::to_string(dt)});
    LuaManager::get().callFunction("onUpdatePost", {std::to_string(dt)});



    if (health <= 0.0f && !gameOver) {
        gameOver = true;
        gameOverTimer = 0.0f;
        deathSoundPlayed = false;
        deathLoopPlaying = false;
        deathConfirmActive = false;
        deathConfirmTimer = 0.0f;
        deathCamFollowStarted = false;

        LuaManager::get().callFunction("onGameOverStart");
        LuaManager::get().callFunction("onGameOver", {});

        AudioEngine::pause();

        // Calculate camera targets after creating deadBF to match FNF getGraphicMidpoint()
        // Store positions of Boyfriend for spawning deadBF
        float deadBfX = bf ? bf->x : 0.0f;
        float deadBfY = bf ? bf->y : 0.0f;
        std::string livingBfName = bf ? bf->curCharacterName : "bf";

        // Null out living characters so PlayState doesn't reference them, and let AsyncAssetManager delete them safely
        bf = nullptr;
        dad = nullptr;
        gf = nullptr;
        if (currentStage) { delete currentStage; currentStage = nullptr; }

        for (auto& pair : healthIconCache) {
            HealthIconData& ic = pair.second;
            if (ic.sheet) {
                C2D_SpriteSheetFree(ic.sheet);
                ic.sheet = nullptr;
            } else if (ic.loaded && ic.vramData) {
                ic.tex.data = nullptr;
                C3D_TexDelete(&ic.tex);
                if (addrIsVRAM(ic.vramData)) vramFree(ic.vramData);
                else linearFree(ic.vramData);
                ic.vramData = nullptr;
            }
        }
        healthIconCache.clear();
        iconBf = HealthIconData();
        iconDad = HealthIconData();

        AsyncAssetManager::get().suspend();
        AsyncAssetManager::get().clearAll();
        SpritesheetCache::get().clear();

        // Now load the Game Over Boyfriend
        deadBF = new Character();
        deadBF->loadFromPsychJson(Paths::characterJson("bf-dead"));
        if (!deadBF->hasValidTexture()) {
            deadBF->loadFromPsychJson(Paths::characterJson(livingBfName));
            if (!deadBF->hasValidTexture()) {
                deadBF->loadFromPsychJson(Paths::characterJson("bf-pixel"));
            }
        }
        deadBF->isPlayer = true;
        // Apply dead Boyfriend offsets from JSON relative to original Boyfriend position
        deadBF->x = deadBfX + deadBF->x;
        deadBF->y = deadBfY + deadBF->y;
        
        // Exact FNF getGraphicMidpoint approximation
        deathCamFollowX = deadBF->x + 200.0f;
        deathCamFollowY = deadBF->y + 200.0f;

        // Reset conductor position and change BPM to 100
        Conductor::songPosition = 0;
        Conductor::changeBPM(100.0f);

        std::string lossPath = Paths::audio("shared/sounds", "fnf_loss_sfx.ogg");
        if (Paths::fileExists(lossPath)) {
            AudioEngine::playSound(lossPath, 1.0f);
        }

        if (deadBF->hasAnimation("firstDeath")) {
            deadBF->playAnim("firstDeath", true);
        } else {
            if (deadBF->hasAnimation("singUP")) {
                deadBF->playAnim("singUP", true);
            }
        }
        return;
    }

    if (vocalMuteTimer > 0) {
        vocalMuteTimer -= dt;
        if (vocalMuteTimer <= 0) {
            vocalMuteTimer = 0;
            AudioEngine::setVocalsVolume(1.0f);
        }
    }
    handleInput(dt);
    
    if (startTimerActive) {
        Conductor::songPosition += dt * 1000.0f;
        
        float crochet = Conductor::crochet;
        
        // Use a while loop to ensure we don't miss ticks if framerate drops
        while (countdownTick < 5 && Conductor::songPosition >= -(crochet * (4 - countdownTick))) {
            tickCountdown();
        }
        
        Conductor::update(Conductor::songPosition);
        
        if (Conductor::songPosition >= 0) {
            startTimerActive = false;
            AudioEngine::start();
            LuaManager::get().callFunction("onSongStart");
        }
    } else {
        Conductor::update(AudioEngine::getSongPosition());
    }

    if (countdownActive) {
        countdownTimer += dt;
        float progress = countdownTimer / (Conductor::crochet / 1000.0f);
        if (progress > 1.0f) {
            progress = 1.0f;
            countdownActive = false;
            if (countdownTick >= 4 && countdownSheet) {
                countdownSheet = nullptr;
                countdownSubtexs.clear();
                AudioEngine::freeCountdownSounds();
            }
        }
        
        float eased = Ease::get("cubeinout", progress);
        countdownAlpha = 1.0f - eased;
        countdownYOffset = eased * 40.0f; // Tween Y down by 40 pixels total
    }

    if (countdownSheet && !countdownActive && Conductor::songPosition > 1000.0f) {
        countdownSheet = nullptr;
        countdownSubtexs.clear();
        AudioEngine::freeCountdownSounds();
    }

    while (nextEventIndex < songData.events.size() && Conductor::songPosition >= songData.events[nextEventIndex].strumTime) {
        triggerEvent(songData.events[nextEventIndex]);
        nextEventIndex++;
    }

    int curStep = (int)std::floor(Conductor::getStep());
    if (curStep > lastStep) {
        lastStep = curStep;
        stepHit(curStep);
    }

    int curBeat = Conductor::getBeat();
    if (curBeat > lastBeat) {
        lastBeat = curBeat;
        beatHit(curBeat);
    }
    
    AudioEngine::update();
    
    updateCamera(dt);
    updateNotesLogic(dt);

    fpsUpdateTimer += dt;
    if (fpsUpdateTimer >= 0.2f) {
        curFPS = 1.0f / dt;
        fpsUpdateTimer = 0;
    }

    if (camShakeTimer > 0) {
        camShakeTimer -= dt;
        csX = (float)((rand() % 100) - 50) / 50.0f * camShakeIntensity * 400.0f;
        csY = (float)((rand() % 100) - 50) / 50.0f * camShakeIntensity * 240.0f;
    } else { csX = 0; csY = 0; }

    if (hudShakeTimer > 0) {
        hudShakeTimer -= dt;
        hsX = (float)((rand() % 100) - 50) / 50.0f * hudShakeIntensity * 400.0f;
        hsY = (float)((rand() % 100) - 50) / 50.0f * hudShakeIntensity * 240.0f;
    } else { hsX = 0; hsY = 0; }

    if (otherShakeTimer > 0) {
        otherShakeTimer -= dt;
        osX = (float)((rand() % 100) - 50) / 50.0f * otherShakeIntensity * 400.0f;
        osY = (float)((rand() % 100) - 50) / 50.0f * otherShakeIntensity * 240.0f;
    } else { osX = 0; osY = 0; }



    pOffsetX = lerp(pOffsetX, 0.0f, dt * 8.0f);
    pOffsetY = lerp(pOffsetY, 0.0f, dt * 8.0f);
    
    float zoomLerp = std::min(1.0f, dt * 3.1f);
    float bumpLerp = std::min(1.0f, dt * 8.0f);
    float scoreLerp = std::min(1.0f, dt * 10.0f);

    // Zoom Lerping (Restored Backup Values with spike protection)
    camZoom = lerp(camZoom, targetZoom, zoomLerp);
    hudZoom = lerp(hudZoom, 1.0f, zoomLerp);
    iconBump = lerp(iconBump, 1.0f, bumpLerp);
    scoreZoom = lerp(scoreZoom, 1.0f, scoreLerp);

    if (bf) bf->update(dt);
    if (dad) dad->update(dt);
    if (gf) gf->update(dt);

    for (auto& ls : luaSprites) {
        if (ls.animated) ls.update(dt);
    }

    // Update debug logs
    for (int i = debugLogs.size() - 1; i >= 0; i--) {
        debugLogs[i].timer -= dt;
        if (debugLogs[i].timer <= 0) {
            debugLogs.erase(debugLogs.begin() + i);
        }
    }

    std::vector<std::string> completedTweens;
    for (auto it = activeTweens.begin(); it != activeTweens.end();) {
        it->timer += dt;
        float progress = std::min(1.0f, it->timer / it->duration);
        float eased = Ease::get(it->ease, progress);
        float currentVal = it->startValue + (it->endValue - it->startValue) * eased;

        auto spriteIt = luaSpriteIndices.find(it->targetTag);
        if (spriteIt != luaSpriteIndices.end()) {
            auto& s = luaSprites[spriteIt->second];
            if (it->prop == "x") s.x = currentVal;
            else if (it->prop == "y") s.y = currentVal;
            else if (it->prop == "alpha") s.alpha = currentVal;
            else if (it->prop == "scale") { s.scale = currentVal; s.scaleX = currentVal; s.scaleY = currentVal; }
            else if (it->prop == "scale.x") s.scaleX = currentVal;
            else if (it->prop == "scale.y") s.scaleY = currentVal;
            else if (it->prop == "angle") s.angle = currentVal;
        }
        else if (luaTextIndices.count(it->targetTag)) {
            auto& txt = luaTexts[luaTextIndices[it->targetTag]];
            if (it->prop == "x") txt.x = currentVal;
            else if (it->prop == "y") txt.y = currentVal;
            else if (it->prop == "alpha") txt.alpha = currentVal;
        }
        else if (it->targetTag == "boyfriend" && bf) {
            if (it->prop == "x") bf->x = currentVal;
            else if (it->prop == "y") bf->y = currentVal;
            else if (it->prop == "alpha") bf->alpha = currentVal;
            else if (it->prop == "scale" || it->prop == "scale.x" || it->prop == "scale.y") bf->charScale = currentVal;
            else if (it->prop == "angle") bf->angle = currentVal;
        }
        else if (it->targetTag == "dad" && dad) {
            if (it->prop == "x") dad->x = currentVal;
            else if (it->prop == "y") dad->y = currentVal;
            else if (it->prop == "alpha") dad->alpha = currentVal;
            else if (it->prop == "scale" || it->prop == "scale.x" || it->prop == "scale.y") dad->charScale = currentVal;
            else if (it->prop == "angle") dad->angle = currentVal;
        }
        else if (it->targetTag == "gf" && gf) {
            if (it->prop == "x") gf->x = currentVal;
            else if (it->prop == "y") gf->y = currentVal;
            else if (it->prop == "alpha") gf->alpha = currentVal;
            else if (it->prop == "scale" || it->prop == "scale.x" || it->prop == "scale.y") gf->charScale = currentVal;
            else if (it->prop == "angle") gf->angle = currentVal;
        }
        // Targets: camHUD / camGame / camOther
        else if (it->targetTag == "camHUD" || it->targetTag == "hud") {
            if (it->prop == "zoom") hudZoom = currentVal;
            else if (it->prop == "angle") hudAngle = currentVal;
            else if (it->prop == "alpha") hudAlpha = currentVal;
            else if (it->prop == "x") hudX_offset = currentVal;
            else if (it->prop == "y") hudY_offset = currentVal;
            else if (it->prop == "scaleX") hudScaleX = currentVal;
            else if (it->prop == "scaleY") hudScaleY = currentVal;
            else if (it->prop == "visible") hudVisible = (currentVal > 0.5f);
        }
        else if (it->targetTag == "camGame" || it->targetTag == "game") {
            if (it->prop == "zoom") camZoom = currentVal;
            else if (it->prop == "angle") camAngle = currentVal;
            else if (it->prop == "alpha") camAlpha = currentVal;
            else if (it->prop == "x") camX_offset = currentVal;
            else if (it->prop == "y") camY_offset = currentVal;
            else if (it->prop == "scaleX") camScaleX = currentVal;
            else if (it->prop == "scaleY") camScaleY = currentVal;
            else if (it->prop == "visible") camVisible = (currentVal > 0.5f);
        }
        else if (it->targetTag == "camOther") {
            if (it->prop == "zoom") otherZoom = currentVal;
            else if (it->prop == "angle") otherAngle = currentVal;
            else if (it->prop == "alpha") otherAlpha = currentVal;
            else if (it->prop == "x") otherX_offset = currentVal;
            else if (it->prop == "y") otherY_offset = currentVal;
            else if (it->prop == "scaleX") otherScaleX = currentVal;
            else if (it->prop == "scaleY") otherScaleY = currentVal;
            else if (it->prop == "visible") otherVisible = (currentVal > 0.5f);
        }
        // Targets: healthbar
        else if (it->targetTag == "healthbar") {
            if (it->prop == "x") healthBarX = currentVal;
            else if (it->prop == "y") healthBarY = currentVal;
            else if (it->prop == "scale.x" || it->prop == "scaleX") healthBarScaleX = currentVal;
            else if (it->prop == "scale.y" || it->prop == "scaleY") healthBarScaleY = currentVal;
            else if (it->prop == "alpha") healthBarAlpha = currentVal;
            else if (it->prop == "angle") healthBarAngle = currentVal;
            else if (it->prop == "visible") healthBarVisible = (currentVal > 0.5f);
        }
        // Targets: healthbarBg
        else if (it->targetTag == "healthbarBg") {
            if (it->prop == "x") healthBarBGX = currentVal;
            else if (it->prop == "y") healthBarBGY = currentVal;
            else if (it->prop == "scale.x" || it->prop == "scaleX") healthBarBGScaleX = currentVal;
            else if (it->prop == "scale.y" || it->prop == "scaleY") healthBarBGScaleY = currentVal;
            else if (it->prop == "alpha") healthBarBGAlpha = currentVal;
            else if (it->prop == "angle") healthBarBGAngle = currentVal;
            else if (it->prop == "visible") healthBarBGVisible = (currentVal > 0.5f);
        }
        // Targets: timebar
        else if (it->targetTag == "timebar") {
            if (it->prop == "x") timeBarX_val = currentVal;
            else if (it->prop == "y") timeBarY_val = currentVal;
            else if (it->prop == "scale.x" || it->prop == "scaleX") timeBarScaleX = currentVal;
            else if (it->prop == "scale.y" || it->prop == "scaleY") timeBarScaleY = currentVal;
            else if (it->prop == "alpha") timeBarAlpha = currentVal;
            else if (it->prop == "angle") timeBarAngle = currentVal;
            else if (it->prop == "visible") timeBarVisible = (currentVal > 0.5f);
        }
        // Targets: timebarBg
        else if (it->targetTag == "timebarBg") {
            if (it->prop == "x") timeBarBGX = currentVal;
            else if (it->prop == "y") timeBarBGY = currentVal;
            else if (it->prop == "scale.x" || it->prop == "scaleX") timeBarBGScaleX = currentVal;
            else if (it->prop == "scale.y" || it->prop == "scaleY") timeBarBGScaleY = currentVal;
            else if (it->prop == "alpha") timeBarBGAlpha = currentVal;
            else if (it->prop == "angle") timeBarBGAngle = currentVal;
            else if (it->prop == "visible") timeBarBGVisible = (currentVal > 0.5f);
        }
        // Targets: iconP1
        else if (it->targetTag == "iconP1") {
            if (it->prop == "x") iconP1X = currentVal;
            else if (it->prop == "y") iconP1Y = currentVal;
            else if (it->prop == "scale" || it->prop == "scaleX" || it->prop == "scale.x") { iconP1Scale = currentVal; iconP1ScaleX = currentVal; }
            else if (it->prop == "scaleY" || it->prop == "scale.y") iconP1ScaleY = currentVal;
            else if (it->prop == "alpha") iconP1Alpha = currentVal;
            else if (it->prop == "angle") iconP1Angle = currentVal;
            else if (it->prop == "visible") iconP1Visible = (currentVal > 0.5f);
        }
        // Targets: iconP2
        else if (it->targetTag == "iconP2") {
            if (it->prop == "x") iconP2X = currentVal;
            else if (it->prop == "y") iconP2Y = currentVal;
            else if (it->prop == "scale" || it->prop == "scaleX" || it->prop == "scale.x") { iconP2Scale = currentVal; iconP2ScaleX = currentVal; }
            else if (it->prop == "scaleY" || it->prop == "scale.y") iconP2ScaleY = currentVal;
            else if (it->prop == "alpha") iconP2Alpha = currentVal;
            else if (it->prop == "angle") iconP2Angle = currentVal;
            else if (it->prop == "visible") iconP2Visible = (currentVal > 0.5f);
        }
        // Targets: scoretxt
        else if (it->targetTag == "scoretxt" || it->targetTag == "scoreTxt") {
            if (it->prop == "x") scoreTxtX = currentVal;
            else if (it->prop == "y") scoreTxtY = currentVal;
            else if (it->prop == "scale.x" || it->prop == "scaleX") scoreTxtScaleX = currentVal;
            else if (it->prop == "scale.y" || it->prop == "scaleY") scoreTxtScaleY = currentVal;
            else if (it->prop == "alpha") scoreTxtAlpha = currentVal;
            else if (it->prop == "angle") scoreTxtAngle = currentVal;
            else if (it->prop == "visible") scoreTxtVisible = (currentVal > 0.5f);
        }
        // Targets: timetxt
        else if (it->targetTag == "timetxt" || it->targetTag == "timeTxt") {
            if (it->prop == "x") timeTxtX = currentVal;
            else if (it->prop == "y") timeTxtY = currentVal;
            else if (it->prop == "scale.x" || it->prop == "scaleX") timeTxtScaleX = currentVal;
            else if (it->prop == "scale.y" || it->prop == "scaleY") timeTxtScaleY = currentVal;
            else if (it->prop == "alpha") timeTxtAlpha = currentVal;
            else if (it->prop == "angle") timeTxtAngle = currentVal;
            else if (it->prop == "visible") timeTxtVisible = (currentVal > 0.5f);
        }
        // Targets: countdown
        else if (it->targetTag == "countdown") {
            if (it->prop == "x") countdownX = currentVal;
            else if (it->prop == "y") countdownY = currentVal;
            else if (it->prop == "scale" || it->prop == "scaleX" || it->prop == "scale.x") { countdownScale = currentVal; countdownScaleX = currentVal; }
            else if (it->prop == "scaleY" || it->prop == "scale.y") countdownScaleY = currentVal;
            else if (it->prop == "alpha") countdownAlpha = currentVal;
            else if (it->prop == "angle") countdownAngle = currentVal;
            else if (it->prop == "visible") countdownVisible = (currentVal > 0.5f);
        }
        // Targets: noteStrum
        else if (it->targetTag.find("noteStrum_") == 0) {
            int noteID = std::stoi(it->targetTag.substr(10));
            if (noteID >= 0 && noteID < 8) {
                int lane = noteID % 4;
                bool isPlayer = (noteID >= 4);
                if (it->prop == "noteX" || it->prop == "x") {
                    if (isPlayer) customPlayerStrumX[lane] = currentVal;
                    else customOpponentStrumX[lane] = currentVal;
                } else if (it->prop == "noteY" || it->prop == "y") {
                    if (isPlayer) customPlayerStrumY[lane] = currentVal;
                    else customOpponentStrumY[lane] = currentVal;
                } else if (it->prop == "noteAngle" || it->prop == "angle") {
                    if (isPlayer) customPlayerStrumAngle[lane] = currentVal;
                    else customOpponentStrumAngle[lane] = currentVal;
                } else if (it->prop == "noteAlpha" || it->prop == "alpha") {
                    if (isPlayer) customPlayerStrumAlpha[lane] = currentVal;
                    else customOpponentStrumAlpha[lane] = currentVal;
                } else if (it->prop == "noteDirection" || it->prop == "direction") {
                    if (isPlayer) customPlayerStrumDirection[lane] = currentVal;
                    else customOpponentStrumDirection[lane] = currentVal;
                } else if (it->prop == "scaleX" || it->prop == "scale.x") {
                    if (isPlayer) customPlayerStrumScaleX[lane] = currentVal;
                    else customOpponentStrumScaleX[lane] = currentVal;
                } else if (it->prop == "scaleY" || it->prop == "scale.y") {
                    if (isPlayer) customPlayerStrumScaleY[lane] = currentVal;
                    else customOpponentStrumScaleY[lane] = currentVal;
                } else if (it->prop == "color") {
                    if (isPlayer) customPlayerStrumColor[lane] = (u32)currentVal;
                    else customOpponentStrumColor[lane] = (u32)currentVal;
                } else if (it->prop == "visible") {
                    if (isPlayer) customPlayerStrumVisible[lane] = (currentVal > 0.5f);
                    else customOpponentStrumVisible[lane] = (currentVal > 0.5f);
                } else if (it->prop == "flipX") {
                    if (isPlayer) customPlayerStrumFlipX[lane] = (currentVal > 0.5f);
                    else customOpponentStrumFlipX[lane] = (currentVal > 0.5f);
                } else if (it->prop == "flipY") {
                    if (isPlayer) customPlayerStrumFlipY[lane] = (currentVal > 0.5f);
                    else customOpponentStrumFlipY[lane] = (currentVal > 0.5f);
                } else if (it->prop == "antialiasing") {
                    if (isPlayer) customPlayerStrumAntialiasing[lane] = (currentVal > 0.5f);
                    else customOpponentStrumAntialiasing[lane] = (currentVal > 0.5f);
                }
            }
        }

        if (progress >= 1.0f) {
            it->finished = true;
            completedTweens.push_back(it->tag);
            it = activeTweens.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& tag : completedTweens) {
        LuaManager::get().callFunction("onTweenCompleted", {tag});
    }

    // UPDATE LUA TIMERS
    std::vector<std::string> completedTimers;
    for (auto it = activeTimers.begin(); it != activeTimers.end();) {
        it->timer += dt;
        if (it->timer >= it->duration) {
            completedTimers.push_back(it->tag);
            
            if (it->loops > 0) {
                it->loopsLeft--;
            }
            if (it->loops == 0 || it->loopsLeft > 0) {
                it->timer = 0.0f;
                ++it;
            } else {
                it = activeTimers.erase(it);
            }
        } else {
            ++it;
        }
    }

    for (const auto& tag : completedTimers) {
        LuaManager::get().callFunction("onTimerCompleted", {tag});
    }
}

void PlayState::handleInput(float dt) {
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    u32 mapDown[] = {KEY_DLEFT | KEY_Y, KEY_DDOWN | KEY_B, KEY_DUP | KEY_X, KEY_DRIGHT | KEY_A};
    
    bool touchPressed[4] = {false, false, false, false};
    bool touchHeld[4] = {false, false, false, false};
    static int lastTouchLane = -1;
    if (ClientPrefs::hitboxEnabled) {
        if (kHeld & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            int touchLane = touch.px / 80;
            if (touchLane >= 0 && touchLane < 4) {
                touchHeld[touchLane] = true;
                if ((kDown & KEY_TOUCH) || touchLane != lastTouchLane) {
                    touchPressed[touchLane] = true;
                }
                lastTouchLane = touchLane;
            }
        } else {
            lastTouchLane = -1;
        }
    }

    for(int i=0; i<4; i++) {
        if (ClientPrefs::botPlay) {
            keyPressed[i] = false;
            keyHeld[i] = false;
        } else {
            keyPressed[i] = ((kDown & mapDown[i]) != 0) || touchPressed[i];
            keyHeld[i] = ((kHeld & mapDown[i]) != 0) || touchHeld[i];
        }
        if (!keyHeld[i]) {
            keyHitRecently[i] = false;
            currentHoldTime[i] = 0.0f;
        } else {
            currentHoldTime[i] += dt;
            if (currentHoldTime[i] > maxHoldTime) maxHoldTime = currentHoldTime[i];
            if (std::find(keysUsed.begin(), keysUsed.end(), i) == keysUsed.end()) {
                keysUsed.push_back(i);
            }
        }
        
        // Handle Head Hits
        if (keyPressed[i]) {
            receptorTimer[4+i] = 0.10f;
            float bestDiff = 166.0f;
            Note* bestNote = nullptr;
            
            float pos = Conductor::songPosition;
            for(auto& n : songNotes) {
                if (n.strumTime < pos - 350.0f) continue;
                if (n.strumTime > pos + 350.0f) break;
                
                if (n.hit || !n.isPlayer || n.noteData != i) continue;
                float diff = abs(n.strumTime - pos);
                if (diff < bestDiff) { bestDiff = diff; bestNote = &n; }
            }
            if (bestNote) {
                bestNote->hit = true; 
                bestNote->wasGoodHit = true; 
                if (bestNote->sustainLength > 0) bestNote->sustainActive = true;
                
                keyHitRecently[i] = true; 

                Character* singer = bestNote->isPlayer ? bf : dad;
                if (bestNote->gfNote || (curSection >= 0 && curSection < (int)songData.sections.size() && songData.sections[curSection].gfSection))
                    singer = gf;

                moveChar(i, singer);
                
                size_t id = bestNote - &songNotes[0];
                LuaManager::get().callFunction("goodNoteHit", {std::to_string(id), std::to_string(bestNote->noteData), bestNote->noteType, bestNote->sustainLength > 0.0f ? "true" : "false"});
                
                totalNotesHit++;
                if (bestDiff < 45.0f) { 
                    score += 350; totalNoteScore += 1.0f; ratingName = "Sick!"; 
                    sicks++;
                    if (ClientPrefs::scoreZoom) scoreZoom = 1.15f;
                    currentRatingStr = "sick";
                }
                else if (bestDiff < 90.0f) { 
                    score += 200; totalNoteScore += 0.75f; ratingName = "Good"; 
                    goods++;
                    if (ClientPrefs::scoreZoom) scoreZoom = 1.1f;
                    currentRatingStr = "good";
                }
                else if (bestDiff < 135.0f) { 
                    score += 100; totalNoteScore += 0.5f; ratingName = "Bad"; 
                    bads++;
                    currentRatingStr = "bad";
                }
                else { 
                    score += 50; totalNoteScore += 0.25f; ratingName = "Shit"; 
                    shits++;
                    currentRatingStr = "shit";
                }

                ratingActive = true;
                ratingVelY = -120.0f; // Jump
                ratingAlpha = 1.0f;
                ratingTimer = 0.0f;
                ratingScale = 0.4f * ClientPrefs::comboScale; 
                ratingX = (ScreenWidthTop - 50.0f) + ClientPrefs::comboOffsetX; 
                ratingY = 35.0f + ClientPrefs::comboOffsetY;

                accuracy = (totalNoteScore / totalNotesHit) * 100.0f;

                health += 0.023f; if (health > 2.0f) health = 2.0f;

                AudioEngine::setVocalsVolume(1.0f);
                combo++; hits++; if (combo > maxCombo) maxCombo = combo;
            } else if (!ClientPrefs::ghostTapping) {
                misses++;
                combo = 0;
                health -= 0.0475f;
                if (health < 0.0f) health = 0.0f;
                AudioEngine::setVocalsVolume(0.0f);
                AudioEngine::playMissSound();
                vocalMuteTimer = 0.5f;
                totalNotesHit++;
                accuracy = (totalNoteScore / totalNotesHit) * 100.0f;
                LuaManager::get().callFunction("ghostTap", {std::to_string(i)});
                if (bf) {
                    static const std::string dirs[] = {"LEFT", "DOWN", "UP", "RIGHT"};
                    bf->playAnim("sing" + dirs[i] + "miss", true);
                }
            }
        }
        
        // Handle Sustains (Drops and Misses)
        float pos = Conductor::songPosition;
        for(auto& n : songNotes) {
            if (n.strumTime < pos - 3000.0f) continue;
            if (n.strumTime > pos + 500.0f) break;

            if (!n.isPlayer || n.noteData != i || n.sustainLength <= 0.0f) continue;
            if (n.ignoreNote && !n.wasGoodHit) continue; 
            
            float tailEnd = n.strumTime + n.sustainLength;
            bool isInsideSustain = (pos >= n.strumTime && pos <= tailEnd);

            if (isInsideSustain && n.sustainActive) {
                if (!keyHeld[i] && !ClientPrefs::botPlay) {
                    float releaseBuffer = 150.0f;
                    if (pos < (tailEnd - releaseBuffer)) {
                        n.sustainActive = false; 
                        n.ignoreNote = true; 
                        n.wasGoodHit = false;
                        misses++; combo = 0; health -= 0.05f; if (health < 0.0f) health = 0.0f;
                        AudioEngine::setVocalsVolume(0.0f);
                        AudioEngine::playMissSound();
                    }
                } else {
                    if (bf) bf->holdTimer = 0;
                }
            }
        }
    }
}

static void C2D_DrawRectRotated(float cx, float cy, float w, float h, float angle_deg, u32 color, float depth = 0.5f) {
    if (angle_deg == 0.0f) {
        C2D_DrawRectSolid(cx - w * 0.5f, cy - h * 0.5f, depth, w, h, color);
        return;
    }
    float rad = angle_deg * (3.14159265f / 180.0f);
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    auto rotX = [&](float x, float y) { return x * cosA - y * sinA; };
    auto rotY = [&](float x, float y) { return x * sinA + y * cosA; };

    float x0 = cx + rotX(-hw, -hh); float y0 = cy + rotY(-hw, -hh);
    float x1 = cx + rotX(hw, -hh);  float y1 = cy + rotY(hw, -hh);
    float x2 = cx + rotX(hw, hh);   float y2 = cy + rotY(hw, hh);
    float x3 = cx + rotX(-hw, hh);  float y3 = cy + rotY(-hw, hh);

    C2D_DrawTriangle(x0, y0, color, x1, y1, color, x2, y2, color, depth);
    C2D_DrawTriangle(x0, y0, color, x2, y2, color, x3, y3, color, depth);
}

void PlayState::drawHUD(float shakeX, float shakeY) {
    float br = bf ? bf->healthbarR : 0.4f, bg = bf ? bf->healthbarG : 1.0f, bb = bf ? bf->healthbarB : 0.2f;

    float gridSize = 32.0f;
    float bw = (float)ScreenWidthBot, bh = (float)ScreenHeight;
    float gx = fmodf(gridOffset + shakeX, gridSize);
    float gy = fmodf(gridOffset + shakeY, gridSize);
    u32 gridCol = C2D_Color32f(br, bg, bb, 0.15f);

    float hudY = 30.0f;
    float hdScale = 0.35f * scoreZoom;
    float textW = 0.0f, textH = 0.0f;
    float drawX = 0.0f, drawY = 0.0f;

    bool anyExtended = ShaderManager::get().isCameraExtended("camGame") || 
                       ShaderManager::get().isCameraExtended("camHUD") || 
                       ShaderManager::get().isCameraExtended("camOther");
    if (showGrid && !ClientPrefs::lowQuality && !anyExtended) {
        for (float x = -gridSize; x < bw + gridSize; x += gridSize) {
            for (float y = -gridSize; y < bh + gridSize; y += gridSize) {
                if ((int(x / gridSize) + int(y / gridSize)) % 2 == 0) {
                    C2D_DrawRectSolid(x + gx, y + gy, 0, gridSize, gridSize, gridCol);
                }
            }
        }
    }

    if (scoreTxtVisible) {
        if (scoreTextNeedsUpdate || score != cachedScore || misses != cachedMisses || combo != cachedCombo) {
            cachedScore = score;
            cachedMisses = misses;
            cachedCombo = combo;
            scoreTextNeedsUpdate = false;
            
            char scoreStr[256];
            sprintf(scoreStr, "Score: %d | Misses: %d | Combo: %d", score, misses, combo);
            C2D_TextFontParse(&scoreTextObj, vcrFont, vcrFontBuf, scoreStr);
            C2D_TextOptimize(&scoreTextObj);
        }
        
        float baseScaleX = hdScale;
        float baseScaleY = hdScale;
        float unscaledW = 0, unscaledH = 0;
        C2D_TextGetDimensions(&scoreTextObj, baseScaleX, baseScaleY, &unscaledW, &unscaledH);
        
        float sX = (scoreTxtX != -9999.0f) ? scoreTxtX : ((bw/2.0f) - (unscaledW/2.0f));
        float sY = (scoreTxtY != -9999.0f) ? scoreTxtY : (hudY - (unscaledH/2.0f));
        
        float centerX = sX + unscaledW / 2.0f;
        float centerY = sY + unscaledH / 2.0f;
        
        float textScaleX = hdScale * scoreTxtScaleX * hudZoom;
        float textScaleY = hdScale * scoreTxtScaleY * hudZoom;
        C2D_TextGetDimensions(&scoreTextObj, textScaleX, textScaleY, &textW, &textH);
        
        float centerXT = ScreenWidthTop / 2.0f;
        float centerYT = ScreenHeight / 2.0f;
        float drawCenterX = centerXT + (centerX - centerXT) * hudZoom + shakeX;
        float drawCenterY = centerYT + (centerY - centerYT) * hudZoom + shakeY;
        
        drawX = drawCenterX - (textW / 2.0f);
        drawY = drawCenterY - (textH / 2.0f);
        
        u8 sa = (u8)(255 * scoreTxtAlpha * hudAlpha);
        u32 sCol = (scoreTxtColor & 0x00FFFFFF) | ((u32)sa << 24);
        
        DrawTextBorderCardinal(&scoreTextObj, drawX, drawY, 0.84f, textScaleX, textScaleY, 1.5f, C2D_Color32(0,0,0,sa));
        C2D_DrawText(&scoreTextObj, C2D_WithColor, drawX, drawY, 0.85f, textScaleX, textScaleY, sCol);
    }


    
    // Draw Lyrics
    if (!currentLyrics.empty() && lyricsTextObj.width > 0) {
        float textW = 0, textH = 0;
        C2D_TextGetDimensions(&lyricsTextObj, currentLyricsSize, currentLyricsSize, &textW, &textH);
        
        float centerX = bw / 2.0f + shakeX;
        float ly = (bh - textH) / 2.0f + shakeY;
        
        // Semi-transparent black background with padding
        float padX = 10.0f * currentLyricsSize;
        float padY = 5.0f * currentLyricsSize;
        // Background is still drawn using the top-left logic to match the dimensions
        C2D_DrawRectSolid(centerX - (textW / 2.0f) - padX, ly - padY, 0.98f, textW + padX*2.0f, textH + padY*2.0f, C2D_Color32(0, 0, 0, 150));
        
        C2D_DrawText(&lyricsTextObj, C2D_WithColor | C2D_AlignCenter, centerX, ly, 0.99f, currentLyricsSize, currentLyricsSize, currentLyricsColor);
    }

    // Draw Countdown
    if (countdownVisible && countdownActive && countdownSheet && countdownSubtexs.count(currentCountdownFrame)) {
        Tex3DS_SubTexture& sub = countdownSubtexs[currentCountdownFrame];
        C2D_Image cBaseImage = C2D_SpriteSheetGetImage(countdownSheet, 0);
        C2D_Image img = { cBaseImage.tex, &sub };

        float cdScaleX = countdownScale * 1.6f * countdownScaleX;
        float cdScaleY = countdownScale * 1.6f * countdownScaleY;
        float cdX = (countdownX != -9999.0f) ? countdownX : ((bw / 2.0f) - (sub.width * cdScaleX / 2.0f));
        float cdY = (countdownY != -9999.0f) ? countdownY : ((bh / 2.0f) - (sub.height * cdScaleY / 2.0f) + countdownYOffset);
        
        float centerXT = ScreenWidthTop / 2.0f;
        float centerYT = ScreenHeight / 2.0f;
        float drawX = centerXT + (cdX - centerXT) * hudZoom + shakeX;
        float drawY = centerYT + (cdY - centerYT) * hudZoom + shakeY;

        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        u32 col = countdownColor;
        u8 cr = (col >> 24) & 0xFF;
        u8 cg = (col >> 16) & 0xFF;
        u8 cb = (col >> 8) & 0xFF;
        u8 ca = (u8)((col & 0xFF) * countdownAlpha * hudAlpha);
        if ((col & 0xFFFFFF00) == 0xFFFFFF00) {
            C2D_AlphaImageTint(&tint, countdownAlpha * hudAlpha);
        } else {
            C2D_PlainImageTint(&tint, C2D_Color32(cr, cg, cb, ca), 0.5f);
        }
        tintPtr = &tint;

        GPU_TEXTURE_FILTER_PARAM f = countdownAntialiasing ? GPU_LINEAR : GPU_NEAREST;
        if (img.tex) C3D_TexSetFilter(img.tex, f, f);

        float finalScaleX = cdScaleX * hudZoom * (countdownFlipX ? -1.0f : 1.0f);
        float finalScaleY = cdScaleY * hudZoom * (countdownFlipY ? -1.0f : 1.0f);

        float cx = drawX + sub.width * finalScaleX * 0.5f;
        float cy = drawY + sub.height * finalScaleY * 0.5f;
        C2D_DrawImageAtRotated(img, cx, cy, 0.95f, countdownAngle * (3.14159265f / 180.0f), tintPtr, finalScaleX, finalScaleY);
    }
}

float PlayState::getLaneX(int lane, bool isPlayer) {
    if (lane >= 0 && lane < 4) {
        if (isPlayer) {
            if (customPlayerStrumX[lane] != -9999.0f) return customPlayerStrumX[lane];
        } else {
            if (customOpponentStrumX[lane] != -9999.0f) return customOpponentStrumX[lane];
        }
    }
    if (isPlayer) {
        return playerX + (lane * spacing);
    } else {
        if (ClientPrefs::middleScroll) {
            if (lane == 0) return 16.0f;
            if (lane == 1) return 16.0f + spacing;
            if (lane == 2) return ScreenWidthTop - 16.0f - 2 * spacing;
            return ScreenWidthTop - 16.0f - spacing;
        } else {
            return 16.0f + (lane * spacing);
        }
    }
}

float PlayState::getLaneY(int lane, bool isPlayer) {
    if (lane >= 0 && lane < 4) {
        if (isPlayer) {
            if (customPlayerStrumY[lane] != -9999.0f) return customPlayerStrumY[lane];
        } else {
            if (customOpponentStrumY[lane] != -9999.0f) return customOpponentStrumY[lane];
        }
    }
    return receptorY;
}

float PlayState::getLaneAngle(int lane, bool isPlayer) {
    if (lane >= 0 && lane < 4) {
        if (isPlayer) return customPlayerStrumAngle[lane];
        else return customOpponentStrumAngle[lane];
    }
    return 0.0f;
}

float PlayState::getLaneAlpha(int lane, bool isPlayer) {
    if (lane >= 0 && lane < 4) {
        if (isPlayer) {
            if (customPlayerStrumAlpha[lane] >= 0.0f) return customPlayerStrumAlpha[lane] * hudAlpha;
        } else {
            if (customOpponentStrumAlpha[lane] >= 0.0f) return customOpponentStrumAlpha[lane] * hudAlpha;
        }
    }
    float baseAlpha = isPlayer ? 1.0f : (ClientPrefs::middleScroll ? 0.35f : 1.0f);
    return baseAlpha * hudAlpha;
}

float PlayState::getLaneDirection(int lane, bool isPlayer) {
    if (lane >= 0 && lane < 4) {
        if (isPlayer) return customPlayerStrumDirection[lane];
        else return customOpponentStrumDirection[lane];
    }
    return 90.0f;
}

void PlayState::drawNotes(float shakeX, float shakeY) {
    float centerXT = ScreenWidthTop / 2.0f;
    float centerYT = ScreenHeight / 2.0f;
    if (timeTxtVisible || timeBarVisible) {
        float timeBarW = 150.0f * timeBarScaleX;
        float timeBarH = 5.0f * timeBarScaleY;
        float tbX = (timeBarX_val != -9999.0f) ? timeBarX_val : ((ScreenWidthTop - 150.0f) / 2.0f);
        float tbY = (timeBarY_val != -9999.0f) ? timeBarY_val : (ClientPrefs::downscroll ? ScreenHeight - 15.0f : 10.0f);
        float progress = (songLength > 0) ? (float)(Conductor::songPosition / songLength) : 0;
        if (progress > 1.0f) progress = 1.0f;
        if (progress < 0.0f) progress = 0.0f;
        
        float tbBGX = (timeBarBGX != -9999.0f) ? timeBarBGX : tbX;
        float tbBGY = (timeBarBGY != -9999.0f) ? timeBarBGY : tbY;
        float timeBarBGW = 150.0f * timeBarBGScaleX;
        float timeBarBGH = 5.0f * timeBarBGScaleY;

        tbX = centerXT + (tbX - centerXT) * hudZoom + shakeX;
        tbY = centerYT + (tbY - centerYT) * hudZoom + shakeY;
        timeBarW *= hudZoom;
        timeBarH *= hudZoom;

        tbBGX = centerXT + (tbBGX - centerXT) * hudZoom + shakeX;
        tbBGY = centerYT + (tbBGY - centerYT) * hudZoom + shakeY;
        timeBarBGW *= hudZoom;
        timeBarBGH *= hudZoom;

        if (ClientPrefs::timeBarType != 3) {
            if (timeBarVisible) {
                u8 bg_a = (u8)(255 * timeBarBGAlpha * hudAlpha);
                float bg_cx = tbBGX + timeBarBGW * 0.5f;
                float bg_cy = tbBGY + timeBarBGH * 0.5f;
                float bg_w = timeBarBGW + 4.0f * hudZoom * timeBarBGScaleX;
                float bg_h = timeBarBGH + 4.0f * hudZoom * timeBarBGScaleY;
                if (timeBarBGVisible) {
                    C2D_DrawRectRotated(bg_cx, bg_cy, bg_w, bg_h, timeBarBGAngle, C2D_Color32(0, 0, 0, bg_a), 0.70f);
                }

                u8 bar_a = (u8)(255 * timeBarAlpha * hudAlpha);
                u32 barCol = (timeBarColor & 0x00FFFFFF) | ((u32)bar_a << 24);
                float bar_cx = tbX + timeBarW * 0.5f;
                float bar_cy = tbY + timeBarH * 0.5f;
                float rad = timeBarAngle * (3.14159265f / 180.0f);
                float dx = -timeBarW * 0.5f * (1.0f - progress);
                float rx = dx * cosf(rad);
                float ry = dx * sinf(rad);
                C2D_DrawRectRotated(bar_cx + rx, bar_cy + ry, timeBarW * progress, timeBarH, timeBarAngle, barCol, 0.72f);
            }

            if (timeTxtVisible) {
                int timeLeft = (songLength > Conductor::songPosition) ? (int)((songLength - Conductor::songPosition) / 1000) : 0;
                char timeStr[16];
                sprintf(timeStr, "%d:%02d", timeLeft / 60, timeLeft % 60);

                C2D_Text timeObj;
                C2D_TextFontParse(&timeObj, vcrFont, vcrFontBuf, timeStr);
                C2D_TextOptimize(&timeObj);
                
                float textScale = 0.35f * hudZoom;
                float textScaleX = textScale * timeTxtScaleX;
                float textScaleY = textScale * timeTxtScaleY;
                float tw, th;
                C2D_TextGetDimensions(&timeObj, textScaleX, textScaleY, &tw, &th);
                
                float dx, dy;
                if (timeTxtX != -9999.0f) {
                    dx = centerXT + (timeTxtX - centerXT) * hudZoom + shakeX;
                } else {
                    dx = tbX + (timeBarW / 2.0f) - (tw / 2.0f);
                }
                if (timeTxtY != -9999.0f) {
                    dy = centerYT + (timeTxtY - centerYT) * hudZoom + shakeY;
                } else {
                    dy = tbY + (timeBarH / 2.0f) - (th / 2.0f);
                }
                u8 ta = (u8)(255 * timeTxtAlpha * hudAlpha);
                u32 tCol = (timeTxtColor & 0x00FFFFFF) | ((u32)ta << 24);

                DrawTextBorderCardinal(&timeObj, dx, dy, 0.72f, textScaleX, textScaleY, 1.5f, C2D_Color32(0,0,0,ta));
                C2D_DrawText(&timeObj, C2D_WithColor, dx, dy, 0.73f, textScaleX, textScaleY, tCol);
            }
        }
    }

    float healthBarW = 200.0f;
    float healthBarH = 5.0f;
    float healthBarX_default = (ScreenWidthTop - healthBarW) / 2.0f;
    float healthBarY_default = ClientPrefs::downscroll ? 20.0f : ScreenHeight - 20.0f;
    
    float hbX = (healthBarX != -9999.0f) ? healthBarX : healthBarX_default;
    float hbY = (healthBarY != -9999.0f) ? healthBarY : healthBarY_default;
    float hbW = healthBarW * healthBarScaleX;
    float hbH = healthBarH * healthBarScaleY;

    float hbBGX = (healthBarBGX != -9999.0f) ? healthBarBGX : hbX;
    float hbBGY = (healthBarBGY != -9999.0f) ? healthBarBGY : hbY;
    float hbBGW = healthBarW * healthBarBGScaleX;
    float hbBGH = healthBarH * healthBarBGScaleY;

    hbX = centerXT + (hbX - centerXT) * hudZoom + shakeX;
    hbY = centerYT + (hbY - centerYT) * hudZoom + shakeY;
    hbW *= hudZoom;
    hbH *= hudZoom;

    hbBGX = centerXT + (hbBGX - centerXT) * hudZoom + shakeX;
    hbBGY = centerYT + (hbBGY - centerYT) * hudZoom + shakeY;
    hbBGW *= hudZoom;
    hbBGH *= hudZoom;

    float healthPerc = health / 2.0f;

    if (ClientPrefs::healthBar) {
        float bg_cx = hbBGX + hbBGW * 0.5f;
        float bg_cy = hbBGY + hbBGH * 0.5f;
        float bg_w = hbBGW + 4.0f * hudZoom * healthBarBGScaleX;
        float bg_h = hbBGH + 4.0f * hudZoom * healthBarBGScaleY;
        
        if (healthBarBGVisible) {
            u8 bg_a = (u8)(255 * healthBarBGAlpha * hudAlpha);
            C2D_DrawRectRotated(bg_cx, bg_cy, bg_w, bg_h, healthBarBGAngle, C2D_Color32(0, 0, 0, bg_a), 0.70f);
        }
        
        float dadR = 1.0f, dadG = 0.0f, dadB = 0.0f; // Red
        float bfR = 1.0f, bfG = 1.0f, bfB = 0.0f; // Yellow
        
        dadR = dad ? dad->healthbarR : 1.0f; dadG = dad ? dad->healthbarG : 0.0f; dadB = dad ? dad->healthbarB : 0.0f;
        bfR  = bf  ? bf->healthbarR  : 0.4f; bfG  = bf  ? bf->healthbarG  : 1.0f; bfB  = bf  ? bf->healthbarB  : 0.2f;

        float dad_cx = hbX + hbW * 0.5f;
        float dad_cy = hbY + hbH * 0.5f;

        if (healthBarVisible) {
            u8 dad_a = (u8)(255 * healthBarAlpha * hudAlpha);
            u32 dadCol = C2D_Color32f(dadR, dadG, dadB, dad_a / 255.0f);
            C2D_DrawRectRotated(dad_cx, dad_cy, hbW, hbH, healthBarAngle, dadCol, 0.71f);
            
            float rad = healthBarAngle * (3.14159265f / 180.0f);
            float dx = hbW * 0.5f * (1.0f - healthPerc);
            float rx = dx * cosf(rad);
            float ry = dx * sinf(rad);
            u8 bf_a = (u8)(255 * healthBarAlpha * hudAlpha);
            u32 bfCol = C2D_Color32f(bfR, bfG, bfB, bf_a / 255.0f);
            C2D_DrawRectRotated(dad_cx + rx, dad_cy + ry, hbW * healthPerc, hbH, healthBarAngle, bfCol, 0.72f);
        }

        float screenScale = 240.0f / 720.0f;
        float p1_x = iconP1X;
        float p1_y = iconP1Y;
        float p2_x = iconP2X;
        float p2_y = iconP2Y;

        float unzoomed_divX = (hbX - shakeX - centerXT) / hudZoom + centerXT + hbW * (1.0f - healthPerc) / hudZoom;
        float unzoomed_iconSz = 42.0f;
        float iconHalf = unzoomed_iconSz * 0.5f;
        float iconCenterY = (hbY - shakeY - centerYT) / hudZoom + centerYT + hbH * 0.5f / hudZoom;

        // Fallback positions — must match autoIconPosition logic above.
        if (p2_x == -9999.0f) p2_x = (unzoomed_divX - iconHalf) / screenScale; // enemy: centered at divX-iconHalf
        if (p2_y == -9999.0f) p2_y = iconCenterY / screenScale;
        if (p1_x == -9999.0f) p1_x = unzoomed_divX / screenScale;   // store visual center of P1 (= divX)
        if (p1_y == -9999.0f) p1_y = iconCenterY / screenScale;

        float p1_3ds_x = p1_x * screenScale;
        float p1_3ds_y = p1_y * screenScale;
        float drawX1 = centerXT + (p1_3ds_x - centerXT) * hudZoom + shakeX;
        float drawY1 = centerYT + (p1_3ds_y - centerYT) * hudZoom + shakeY;

        float p2_3ds_x = p2_x * screenScale;
        float p2_3ds_y = p2_y * screenScale;
        float drawX2 = centerXT + (p2_3ds_x - centerXT) * hudZoom + shakeX;
        float drawY2 = centerYT + (p2_3ds_y - centerYT) * hudZoom + shakeY;

        bool bfLosing  = health < 0.4f;
        bool dadLosing = health > 1.6f;

        C2D_ImageTint tint1, tint2;
        C2D_ImageTint* tint1Ptr = nullptr;
        C2D_ImageTint* tint2Ptr = nullptr;
        
        u32 col1 = iconP1Color;
        u8 r1 = (col1 >> 24) & 0xFF;
        u8 g1 = (col1 >> 16) & 0xFF;
        u8 b1 = (col1 >> 8) & 0xFF;
        u8 a1 = (u8)((col1 & 0xFF) * iconP1Alpha * hudAlpha);
        if ((col1 & 0xFFFFFF00) == 0xFFFFFF00) {
            C2D_AlphaImageTint(&tint1, iconP1Alpha * hudAlpha);
        } else {
            C2D_PlainImageTint(&tint1, C2D_Color32(r1, g1, b1, a1), 0.5f);
        }
        tint1Ptr = &tint1;
        
        u32 col2 = iconP2Color;
        u8 r2 = (col2 >> 24) & 0xFF;
        u8 g2 = (col2 >> 16) & 0xFF;
        u8 b2 = (col2 >> 8) & 0xFF;
        u8 a2 = (u8)((col2 & 0xFF) * iconP2Alpha * hudAlpha);
        if ((col2 & 0xFFFFFF00) == 0xFFFFFF00) {
            C2D_AlphaImageTint(&tint2, iconP2Alpha * hudAlpha);
        } else {
            C2D_PlainImageTint(&tint2, C2D_Color32(r2, g2, b2, a2), 0.5f);
        }
        tint2Ptr = &tint2;

        if (iconDad.loaded && iconP2Visible) {
            Tex3DS_SubTexture* sub = dadLosing ? &iconDad.losingSub : &iconDad.normalSub;
            C2D_Image iconImg = { &iconDad.tex, sub };
            float baseSc = (42.0f * hudZoom * iconBump * iconP2Scale) / 64.0f;
            float scX = baseSc * iconP2ScaleX * (iconP2FlipX ? -1.0f : 1.0f);
            float scY = baseSc * iconP2ScaleY * (iconP2FlipY ? -1.0f : 1.0f);
            GPU_TEXTURE_FILTER_PARAM f = iconP2Antialiasing ? GPU_LINEAR : GPU_NEAREST;
            if (iconDad.loaded) C3D_TexSetFilter(&iconDad.tex, f, f);
            C2D_DrawImageAtRotated(iconImg, drawX2, drawY2, 0.73f, iconP2Angle * (3.14159265f / 180.0f), tint2Ptr, scX, scY);
        }
        if (iconBf.loaded && iconP1Visible) {
            Tex3DS_SubTexture* sub = bfLosing ? &iconBf.losingSub : &iconBf.normalSub;
            C2D_Image iconImg = { &iconBf.tex, sub };
            float baseSc = (42.0f * hudZoom * iconBump * iconP1Scale) / 64.0f;
            float scX = -baseSc * iconP1ScaleX * (iconP1FlipX ? -1.0f : 1.0f);
            float scY = baseSc * iconP1ScaleY * (iconP1FlipY ? -1.0f : 1.0f);
            float drawnW = 64.0f * fabsf(scX);
            GPU_TEXTURE_FILTER_PARAM f = iconP1Antialiasing ? GPU_LINEAR : GPU_NEAREST;
            if (iconBf.loaded) C3D_TexSetFilter(&iconBf.tex, f, f);
            C2D_DrawImageAtRotated(iconImg, drawX1 - drawnW * 0.5f, drawY1, 0.73f, iconP1Angle * (3.14159265f / 180.0f), tint1Ptr, scX, scY);
        }
    }
    
    bool useFastReceptors = ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 2;

    // Draw opponent receptors if enabled
    if (ClientPrefs::opponentStrums) {
        for (int i = 0; i < 4; i++) {
            if (!customOpponentStrumVisible[i]) continue;
            float lx = getLaneX(i, false);
            float ly = getLaneY(i, false);
            float oppAlpha = getLaneAlpha(i, false);
            float oppAngle = getLaneAngle(i, false) * (3.14159265f / 180.0f);
            float sx = noteScale * customOpponentStrumScaleX[i];
            float sy = noteScale * customOpponentStrumScaleY[i];
            
            lx = centerXT + (lx - centerXT) * hudZoom + shakeX;
            ly = centerYT + (ly - centerYT) * hudZoom + shakeY;
            sx *= hudZoom; sy *= hudZoom;

            NoteSprite ns;
            bool isHit = false;
            if (useFastReceptors) {
                ns = fastNoteSubtexs[0];
                if (receptorTimer[i] > 0.0f) {
                    isHit = true;
                }
            } else {
                int groupIdx = i;
                int slot = receptorActiveSlot[groupIdx];
                size_t idx = groupIdx * 6 + slot;
                if (noteSubtexFrames.size() > idx && !noteSubtexFrames[idx].empty()) {
                    int numFrames = noteSubtexFrames[idx].size();
                    int frameIdx = (int)(receptorAnimTime[groupIdx] * 24.0f);
                    if (frameIdx >= numFrames) frameIdx = numFrames - 1;
                    ns = noteSubtexFrames[idx][frameIdx];
                } else {
                    ns = noteSubtexs[idx];
                }
            }

            if (!ns.tex) continue;
            float laneCenterX = lx + (spacing * hudZoom / 2.0f);
            float laneCenterY = ly + (spacing * hudZoom / 2.0f);
            float origW = ns.frameWidth ? ns.frameWidth : ns.w;
            float origH = ns.frameHeight ? ns.frameHeight : ns.h;
            float expectedCenterX = laneCenterX + (origW / 2.0f - ns.w / 2.0f + ns.frameX) * sx;
            float expectedCenterY = laneCenterY + (origH / 2.0f - ns.h / 2.0f + ns.frameY) * sy;
            float drawX, drawY;
            if (ns.rotated) {
                drawX = expectedCenterX - ns.h * sy * 0.5f;
                drawY = expectedCenterY - ns.w * sx * 0.5f;
            } else {
                drawX = expectedCenterX - ns.w * sx * 0.5f;
                drawY = expectedCenterY - ns.h * sy * 0.5f;
            }

            C2D_ImageTint tint;
            C2D_ImageTint* tintPtr = nullptr;
            u32 col = customOpponentStrumColor[i];
            u8 a_val = (col >> 24) & 0xFF;
            if (a_val == 0 && col != 0) a_val = 255;
            u8 a = (u8)(a_val * oppAlpha);
            u8 r = (col >> 16) & 0xFF;
            u8 g = (col >> 8) & 0xFF;
            u8 b = col & 0xFF;
            
            if (col != 0xFFFFFFFF || useFastReceptors) {
                if (useFastReceptors) {
                    if (isHit) {
                        C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, a), 0.6f);
                        C2D_SetTintMode(C2D_TintSolid);
                    } else {
                        C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, a), 1.0f);
                        C2D_SetTintMode(C2D_TintMult);
                    }
                } else {
                    C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, a), 0.7f);
                    C2D_SetTintMode(C2D_TintSolid);
                }
                tintPtr = &tint;
            } else {
                if (a < 255) {
                    C2D_AlphaImageTint(&tint, a / 255.0f);
                    tintPtr = &tint;
                }
            }

            if (useFastReceptors) {
                static const float FAST_ANGLES[4] = { -M_PI/2.0f, M_PI, 0.0f, M_PI/2.0f };
                renderNoteSprite(ns, drawX, drawY, 0.75f, tintPtr, sx, sy, FAST_ANGLES[i] + oppAngle, customOpponentStrumFlipX[i], customOpponentStrumFlipY[i], customOpponentStrumAntialiasing[i]);
                C2D_SetTintMode(C2D_TintMult);
            } else {
                renderNoteSprite(ns, drawX, drawY, 0.75f, tintPtr, sx, sy, oppAngle, customOpponentStrumFlipX[i], customOpponentStrumFlipY[i], customOpponentStrumAntialiasing[i]);
                C2D_SetTintMode(C2D_TintMult);
            }
        }
    }

    // Draw player receptors
    for (int i = 0; i < 4; i++) {
        if (!customPlayerStrumVisible[i]) continue;
        float lx = getLaneX(i, true);
        float ly = getLaneY(i, true);
        float playerAlpha = getLaneAlpha(i, true);
        float playerAngle = getLaneAngle(i, true) * (3.14159265f / 180.0f);
        float sx = noteScale * customPlayerStrumScaleX[i];
        float sy = noteScale * customPlayerStrumScaleY[i];
        
        lx = centerXT + (lx - centerXT) * hudZoom + shakeX;
        ly = centerYT + (ly - centerYT) * hudZoom + shakeY;
        sx *= hudZoom; sy *= hudZoom;

        NoteSprite ns;
        bool isHit = false;
        bool isPress = false;
        bool isBotHolding = ClientPrefs::botPlay && (receptorTimer[4 + i] > 0.0f);
        if (useFastReceptors) {
            ns = fastNoteSubtexs[0];
            if (keyHitRecently[i] || isBotHolding) {
                isHit = true;
            } else if (keyHeld[i]) {
                isPress = true;
            }
        } else {
            int groupIdx = i;
            int slot = receptorActiveSlot[4 + groupIdx];
            size_t idx = groupIdx * 6 + slot;
            if (noteSubtexFrames.size() > idx && !noteSubtexFrames[idx].empty()) {
                int numFrames = noteSubtexFrames[idx].size();
                int frameIdx = (int)(receptorAnimTime[4 + groupIdx] * 24.0f);
                if (frameIdx >= numFrames) frameIdx = numFrames - 1;
                ns = noteSubtexFrames[idx][frameIdx];
            } else {
                ns = noteSubtexs[idx];
            }
        }

        if (!ns.tex) continue; 
        float laneCenterX = lx + (spacing * hudZoom / 2.0f);
        float laneCenterY = ly + (spacing * hudZoom / 2.0f);
        float origW = ns.frameWidth ? ns.frameWidth : ns.w;
        float origH = ns.frameHeight ? ns.frameHeight : ns.h;
        float expectedCenterX = laneCenterX + (origW / 2.0f - ns.w / 2.0f + ns.frameX) * sx;
        float expectedCenterY = laneCenterY + (origH / 2.0f - ns.h / 2.0f + ns.frameY) * sy;
        float drawX, drawY;
        if (ns.rotated) {
            drawX = expectedCenterX - ns.h * sx * 0.5f;
            drawY = expectedCenterY - ns.w * sy * 0.5f;
        } else {
            drawX = expectedCenterX - ns.w * sx * 0.5f;
            drawY = expectedCenterY - ns.h * sy * 0.5f;
        }

        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        u32 col = customPlayerStrumColor[i];
        u8 a_val = (col >> 24) & 0xFF;
        if (a_val == 0 && col != 0) a_val = 255;
        u8 a = (u8)(a_val * playerAlpha);
        u8 r = (col >> 16) & 0xFF;
        u8 g = (col >> 8) & 0xFF;
        u8 b = col & 0xFF;

        if (col != 0xFFFFFFFF || useFastReceptors) {
            if (useFastReceptors) {
                if (isHit) {
                    C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, a), 0.6f);
                    C2D_SetTintMode(C2D_TintSolid);
                } else if (isPress) {
                    C2D_PlainImageTint(&tint, C2D_Color32((u8)(r * 0.43f), (u8)(g * 0.43f), (u8)(b * 0.43f), a), 1.0f);
                    C2D_SetTintMode(C2D_TintMult);
                } else {
                    C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, a), 1.0f);
                    C2D_SetTintMode(C2D_TintMult);
                }
            } else {
                C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, a), 0.7f);
                C2D_SetTintMode(C2D_TintSolid);
            }
            tintPtr = &tint;
        } else {
            if (a < 255) {
                C2D_AlphaImageTint(&tint, a / 255.0f);
                tintPtr = &tint;
            }
        }

        if (useFastReceptors) {
            static const float FAST_ANGLES[4] = { -M_PI/2.0f, M_PI, 0.0f, M_PI/2.0f };
            renderNoteSprite(ns, drawX, drawY, 0.75f, tintPtr, sx, sy, FAST_ANGLES[i] + playerAngle, customPlayerStrumFlipX[i], customPlayerStrumFlipY[i], customPlayerStrumAntialiasing[i]);
            C2D_SetTintMode(C2D_TintMult);
        } else {
            renderNoteSprite(ns, drawX, drawY, 0.75f, tintPtr, sx, sy, playerAngle, customPlayerStrumFlipX[i], customPlayerStrumFlipY[i], customPlayerStrumAntialiasing[i]);
            C2D_SetTintMode(C2D_TintMult);
        }
    }


    // Cap note iterations for performance
    size_t noteLimit = nextNoteIndex + 80;
    if (noteLimit > songNotes.size()) noteLimit = songNotes.size();

    for (size_t i = nextNoteIndex; i < noteLimit; i++) {
        Note& n = songNotes[i];
        if (!n.visible) continue;
        if (n.sustainLength <= 0.0f) continue;
        if (n.wasGoodHit && !n.sustainActive) continue;
        if (!n.isPlayer) {
            if (!ClientPrefs::opponentStrums || !ClientPrefs::opponentNotes) continue;
        }

        float diff = n.strumTime - Conductor::songPosition;
        float endDiff = (n.strumTime + n.sustainLength) - Conductor::songPosition;

        if (endDiff <= 0.0f) continue;

        float recX = getLaneX(n.noteData, n.isPlayer);
        float recY = getLaneY(n.noteData, n.isPlayer);
        float strumDir = getLaneDirection(n.noteData, n.isPlayer);
        if (ClientPrefs::downscroll && strumDir == 90.0f) {
            strumDir = 270.0f;
        }
        float dirRad = strumDir * (3.14159265f / 180.0f);

        // Head position (start of sustain)
        float headDiff = diff;
        if (n.sustainActive && endDiff > 0 && diff <= 0) {
            headDiff = 0.0f; // Locked to receptor during active hold
        }

        float laneCenterX = recX + (spacing / 2.0f);
        float laneCenterY = recY + (spacing / 2.0f);

        float headX = laneCenterX - cosf(dirRad) * (headDiff * p3DS);
        float headY = laneCenterY + sinf(dirRad) * (headDiff * p3DS);

        // Tail end position
        float tailX = laneCenterX - cosf(dirRad) * (endDiff * p3DS);
        float tailY = laneCenterY + sinf(dirRad) * (endDiff * p3DS);

        if (headY < -300.0f && tailY < -300.0f) continue;
        if (headY > ScreenHeight + 300.0f && tailY > ScreenHeight + 300.0f) continue;

        NoteSprite holdPiece;
        NoteSprite holdEnd;
        bool hasHoldEnd = false;
        bool customImgFound = false;

        if (!n.texture.empty()) {
            auto itImg = customNoteImages.find(n.texture);
            if (itImg != customNoteImages.end()) {
                C2D_Image imgT = itImg->second;
                holdPiece.tex = imgT.tex;
                holdPiece.sub = *imgT.subtex;
                holdPiece.w = imgT.subtex->width;
                holdPiece.h = imgT.subtex->height;
                customImgFound = true;
            }
        }

        if (!customImgFound) {
            if (!n.texture.empty() && customNoteSheets.find(n.texture) != customNoteSheets.end()) {
                C2D_SpriteSheet sheet = customNoteSheets[n.texture];
                int numImages = C2D_SpriteSheetCount(sheet);
                int idxTail = n.noAnimation ? 0 : ((n.noteData % numImages) + 4);
                C2D_Image imgT = C2D_SpriteSheetGetImage(sheet, idxTail % numImages);
                holdPiece.tex = imgT.tex;
                holdPiece.sub = *imgT.subtex;
                holdPiece.w = imgT.subtex->width;
                holdPiece.h = imgT.subtex->height;
            } else {
                if (ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 3) {
                    holdPiece = fastNoteSubtexs[1];
                    holdEnd = fastNoteSubtexs[2];
                    hasHoldEnd = true;
                } else if (ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() == 2) {
                    holdPiece = fastNoteSubtexs[1];
                } else {
                    int groupIdx = n.noteData;
                    holdPiece = noteSubtexs[groupIdx * 6 + 4];
                    holdEnd = noteSubtexs[groupIdx * 6 + 5];
                    hasHoldEnd = true;
                }
            }
        }
        
        if (!holdPiece.tex) continue;

        float deltaX = tailX - headX;
        float deltaY = tailY - headY;
        float totalSusH = sqrtf(deltaX * deltaX + deltaY * deltaY);
        if (totalSusH <= 0.0f) continue;

        float angleLine = atan2f(deltaY, deltaX);

        C2D_ImageTint tint;
        C2D_ImageTint* tintPtr = nullptr;
        float baseAlpha = getLaneAlpha(n.noteData, n.isPlayer) * n.multAlpha;


        static const unsigned char FAST_COLORS[4][3] = {
            {0xC2,0x4B,0x99}, {0x00,0xFF,0xFF}, {0x12,0xFA,0x05}, {0xF9,0x39,0x3F}
        };
        bool useFastTint = ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 2
                           && n.texture.empty();
        unsigned char r = 255, g = 255, b = 255;
        u8 a_note = 255;
        if (n.color != 0xFFFFFFFF) {
            a_note = (n.color >> 24) & 0xFF;
            if (a_note == 0 && n.color != 0) a_note = 255;
            r = (n.color >> 16) & 0xFF;
            g = (n.color >> 8) & 0xFF;
            b = n.color & 0xFF;
        } else if (useFastTint) {
            if (ClientPrefs::noteColorsEnabled) {
                r = ClientPrefs::noteColors[n.noteData % 4][0];
                g = ClientPrefs::noteColors[n.noteData % 4][1];
                b = ClientPrefs::noteColors[n.noteData % 4][2];
            } else {
                r = FAST_COLORS[n.noteData % 4][0];
                g = FAST_COLORS[n.noteData % 4][1];
                b = FAST_COLORS[n.noteData % 4][2];
            }
        }
 
        float aVal = baseAlpha * a_note / 255.0f;
        if (n.isPlayer && !n.sustainActive && diff <= 0) {
            aVal *= 0.5f;
        }
        if (n.color != 0xFFFFFFFF || useFastTint) {
            C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, 255), 1.0f);
            for (int ci = 0; ci < 4; ci++) {
                tint.corners[ci].color = (tint.corners[ci].color & 0x00FFFFFF) | ((u32)(aVal * 255) << 24);
            }
            tintPtr = &tint;
        } else {
            if (aVal < 1.0f) {
                C2D_AlphaImageTint(&tint, aVal);
                tintPtr = &tint;
            }
        }

        // Note head lookup to align sustain start with actual note head center
        NoteSprite headSprite;
        bool isHeadCustom = false;
        if (!n.texture.empty()) {
            auto itImg = customNoteImages.find(n.texture);
            if (itImg != customNoteImages.end()) {
                C2D_Image imgT = itImg->second;
                headSprite.tex = imgT.tex;
                headSprite.sub = *imgT.subtex;
                headSprite.w = imgT.subtex->width;
                headSprite.h = imgT.subtex->height;
                isHeadCustom = true;
            }
        }
        if (!isHeadCustom) {
            if (!n.texture.empty() && customNoteSheets.find(n.texture) != customNoteSheets.end()) {
                C2D_SpriteSheet sheet = customNoteSheets[n.texture];
                int numImages = C2D_SpriteSheetCount(sheet);
                int imgIdx = n.noAnimation ? 0 : (n.noteData % numImages);
                C2D_Image imgT = C2D_SpriteSheetGetImage(sheet, imgIdx);
                headSprite.tex = imgT.tex;
                headSprite.sub = *imgT.subtex;
                headSprite.w = imgT.subtex->width;
                headSprite.h = imgT.subtex->height;
            } else if (ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 2) {
                headSprite = fastNoteSubtexs[0];
            } else {
                int groupIdx = n.noteData;
                headSprite = noteSubtexs[groupIdx * 6 + 2];
            }
        }

        float rotHeadOffsetX = 0.0f;
        float rotHeadOffsetY = 0.0f;
        if (headSprite.tex) {
            float headOrigW = headSprite.frameWidth ? headSprite.frameWidth : headSprite.w;
            float headOrigH = headSprite.frameHeight ? headSprite.frameHeight : headSprite.h;
            float sx = noteScale * n.scaleX * hudZoom;
            float sy = noteScale * n.scaleY * hudZoom;
            float headOffsetX = (headOrigW / 2.0f - headSprite.w / 2.0f + headSprite.frameX) * sx;
            float headOffsetY = (headOrigH / 2.0f - headSprite.h / 2.0f + headSprite.frameY) * sy;

            float noteAngle = getLaneAngle(n.noteData, n.isPlayer) * (3.14159265f / 180.0f);
            rotHeadOffsetX = headOffsetX * cosf(noteAngle) - headOffsetY * sinf(noteAngle);
            rotHeadOffsetY = headOffsetX * sinf(noteAngle) + headOffsetY * cosf(noteAngle);
        }

        // Attachment pivot point at note head center in HUD screen space
        float pivotX = centerXT + (headX - centerXT) * hudZoom;
        float pivotY = centerYT + (headY - centerYT) * hudZoom;
        if (!n.sustainActive) {
            pivotX += rotHeadOffsetX;
            pivotY += rotHeadOffsetY;
        }



        float endTipH = (hasHoldEnd && holdEnd.tex) ? (holdEnd.h * noteScale * n.scaleY) : 0.0f;
        if (endTipH > totalSusH) {
            endTipH = totalSusH;
        }
        float stretchedSusH = totalSusH - endTipH;

        // Draw hold piece (anchored directly at pivot point)
        if (stretchedSusH > 0.0f) {
            float pieceH_screen = (stretchedSusH + 3.0f) * hudZoom;
            float cx = pivotX + cosf(angleLine) * (pieceH_screen * 0.5f) + (n.offsetX * hudZoom);
            float cy = pivotY + sinf(angleLine) * (pieceH_screen * 0.5f) + (n.offsetY * hudZoom);


            float dsX, dsY;
            float drawAngle;
            if (holdPiece.rotated) {
                drawAngle = angleLine;
                dsX = noteScale * n.scaleX * hudZoom;
                dsY = pieceH_screen / holdPiece.sub.width;
            } else {
                drawAngle = angleLine - (3.14159265f / 2.0f);
                dsX = noteScale * n.scaleX * hudZoom;
                dsY = pieceH_screen / holdPiece.sub.height;
            }

            float baseScaleY = noteScale * n.scaleY * hudZoom;
            // Account for trimmed frame offsets (frameX, frameY, frameWidth, frameHeight) in Sparrow XML
            float origW_piece = holdPiece.frameWidth ? holdPiece.frameWidth : holdPiece.w;
            float origH_piece = holdPiece.frameHeight ? holdPiece.frameHeight : holdPiece.h;
            float pieceOffsetX, pieceOffsetY;
            if (holdPiece.rotated) {
                pieceOffsetX = 0.0f;
                pieceOffsetY = (origH_piece / 2.0f - holdPiece.h / 2.0f + holdPiece.frameY) * dsX;
            } else {
                pieceOffsetX = (origW_piece / 2.0f - holdPiece.w / 2.0f + holdPiece.frameX) * dsX;
                pieceOffsetY = 0.0f;
            }

            // Rotate local offsets to screen space based on drawAngle
            float offsetXS = pieceOffsetX * cosf(drawAngle) - pieceOffsetY * sinf(drawAngle);
            float offsetYS = pieceOffsetX * sinf(drawAngle) + pieceOffsetY * cosf(drawAngle);

            float finalCx = cx + offsetXS;
            float finalCy = cy + offsetYS;
            C2D_Image img = { holdPiece.tex, &holdPiece.sub };
            if (useFastTint) C2D_SetTintMode(C2D_TintMult);
            GPU_TEXTURE_FILTER_PARAM f = n.antialiasing ? GPU_LINEAR : GPU_NEAREST;
            if (holdPiece.tex) C3D_TexSetFilter(holdPiece.tex, f, f);
            float finalScaleX = dsX * (n.flipX ? -1.0f : 1.0f);
            float finalScaleY = dsY * (n.flipY ? -1.0f : 1.0f);
            if (holdPiece.rotated) {
                C2D_DrawImageAtRotated(img, finalCx, finalCy, 0.80f, drawAngle, tintPtr, finalScaleY, finalScaleX);
            } else {
                C2D_DrawImageAtRotated(img, finalCx, finalCy, 0.80f, drawAngle, tintPtr, finalScaleX, finalScaleY);
            }
            if (useFastTint) C2D_SetTintMode(C2D_TintMult);
        }

        // Draw hold end tip (anchored directly where hold piece ends)
        if (hasHoldEnd && holdEnd.tex && endTipH > 0.0f) {
            float endH_screen = endTipH * hudZoom;
            float endDist_screen = (stretchedSusH * hudZoom) + (endH_screen * 0.5f);
            float endCx = pivotX + cosf(angleLine) * endDist_screen + (n.offsetX * hudZoom);
            float endCy = pivotY + sinf(angleLine) * endDist_screen + (n.offsetY * hudZoom);

            float endDsX, endDsY;
            float endDrawAngle;
            if (holdEnd.rotated) {
                endDrawAngle = angleLine + n.angle * (3.14159265f / 180.0f);
                endDsX = noteScale * n.scaleX * hudZoom;
                endDsY = endH_screen / holdEnd.sub.width;
            } else {
                endDrawAngle = angleLine - (3.14159265f / 2.0f) + n.angle * (3.14159265f / 180.0f);
                endDsX = noteScale * n.scaleX * hudZoom;
                endDsY = endH_screen / holdEnd.sub.height;
            }

            float baseScaleY = noteScale * n.scaleY * hudZoom;
            // Account for trimmed frame offsets in Sparrow XML
            float origW_end = holdEnd.frameWidth ? holdEnd.frameWidth : holdEnd.w;
            float origH_end = holdEnd.frameHeight ? holdEnd.frameHeight : holdEnd.h;
            float endOffsetX, endOffsetY;
            if (holdEnd.rotated) {
                endOffsetX = 0.0f;
                endOffsetY = (origH_end / 2.0f - holdEnd.h / 2.0f + holdEnd.frameY) * endDsX;
            } else {
                endOffsetX = (origW_end / 2.0f - holdEnd.w / 2.0f + holdEnd.frameX) * endDsX;
                endOffsetY = 0.0f;
            }

            // Rotate local offsets to screen space based on endDrawAngle
            float endOffsetXS = endOffsetX * cosf(endDrawAngle) - endOffsetY * sinf(endDrawAngle);
            float endOffsetYS = endOffsetX * sinf(endDrawAngle) + endOffsetY * cosf(endDrawAngle);

            float finalEndCx = endCx + endOffsetXS;
            float finalEndCy = endCy + endOffsetYS;

            C2D_Image imgEnd = { holdEnd.tex, &holdEnd.sub };
            if (useFastTint) C2D_SetTintMode(C2D_TintMult);
            GPU_TEXTURE_FILTER_PARAM f_end = n.antialiasing ? GPU_LINEAR : GPU_NEAREST;
            if (holdEnd.tex) C3D_TexSetFilter(holdEnd.tex, f_end, f_end);
            float finalEndScaleX = endDsX * (n.flipX ? -1.0f : 1.0f);
            float finalEndScaleY = endDsY * (n.flipY ? -1.0f : 1.0f);
            if (holdEnd.rotated) {
                C2D_DrawImageAtRotated(imgEnd, finalEndCx, finalEndCy, 0.81f, endDrawAngle, tintPtr, finalEndScaleY, finalEndScaleX);
            } else {
                C2D_DrawImageAtRotated(imgEnd, finalEndCx, finalEndCy, 0.81f, endDrawAngle, tintPtr, finalEndScaleX, finalEndScaleY);
            }
            if (useFastTint) C2D_SetTintMode(C2D_TintMult);
        }
    }

    // Draw note heads
    for (size_t i = nextNoteIndex; i < noteLimit; i++) {
        Note& n = songNotes[i];
        if (!n.visible) continue;
        if (n.isPlayer) {
            if (n.hit && !n.ignoreNote) continue;
        } else {
            if (!ClientPrefs::opponentStrums || !ClientPrefs::opponentNotes) continue;
            if (n.hit) continue;
        }

        float diff = n.strumTime - Conductor::songPosition;
        float targetRecX = getLaneX(n.noteData, n.isPlayer);
        float targetRecY = getLaneY(n.noteData, n.isPlayer);
        float strumDir = getLaneDirection(n.noteData, n.isPlayer);
        if (ClientPrefs::downscroll && strumDir == 90.0f) {
            strumDir = 270.0f;
        }
        float dirRad = strumDir * (3.14159265f / 180.0f);
        float dist = diff * p3DS;

        float lx = targetRecX - cosf(dirRad) * dist;
        float ly = targetRecY + sinf(dirRad) * dist;
        
        if (ly < -200.0f || ly > ScreenHeight + 200.0f) {
            if (ClientPrefs::downscroll && ly < -300.0f) break;
            if (!ClientPrefs::downscroll && ly > ScreenHeight + 300.0f) break;
        }
        
        float sx = noteScale, sy = noteScale;
        
        lx = centerXT + (lx - centerXT) * hudZoom;
        ly = centerYT + (ly - centerYT) * hudZoom;
        sx *= hudZoom; sy *= hudZoom;
        
        NoteSprite ns;
        bool isCustom = false;
        if (!n.texture.empty()) {
            auto itImg = customNoteImages.find(n.texture);
            if (itImg != customNoteImages.end()) {
                C2D_Image imgT = itImg->second;
                ns.tex = imgT.tex;
                ns.sub = *imgT.subtex;
                ns.w = imgT.subtex->width;
                ns.h = imgT.subtex->height;
                isCustom = true;
            }
        }

        if (!isCustom) {
            if (!n.texture.empty() && customNoteSheets.find(n.texture) != customNoteSheets.end()) {
                C2D_SpriteSheet sheet = customNoteSheets[n.texture];
                int numImages = C2D_SpriteSheetCount(sheet);
                int imgIdx = n.noAnimation ? 0 : (n.noteData % numImages);
                C2D_Image imgT = C2D_SpriteSheetGetImage(sheet, imgIdx);
                ns.tex = imgT.tex;
                ns.sub = *imgT.subtex;
                ns.w = imgT.subtex->width;
                ns.h = imgT.subtex->height;
            } else if (ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 2) {
                ns = fastNoteSubtexs[0];
            } else {
                int groupIdx = n.noteData;
                ns = noteSubtexs[groupIdx * 6 + 2];
            }
        }

        if (!ns.tex) continue;
        float scaleX = sx * n.scaleX;
        float scaleY = sy * n.scaleY;
        float laneCenterX = lx + (spacing * hudZoom / 2.0f) + (n.offsetX * hudZoom);
        float laneCenterY = ly + (spacing * hudZoom / 2.0f) + (n.offsetY * hudZoom);
        float origW = ns.frameWidth ? ns.frameWidth : ns.w;
        float origH = ns.frameHeight ? ns.frameHeight : ns.h;
        float expectedCenterX = laneCenterX + (origW / 2.0f - ns.w / 2.0f + ns.frameX) * scaleX;
        float expectedCenterY = laneCenterY + (origH / 2.0f - ns.h / 2.0f + ns.frameY) * scaleY;
        float drawX, drawY;
        if (ns.rotated) {
            drawX = expectedCenterX - ns.h * scaleX * 0.5f;
            drawY = expectedCenterY - ns.w * scaleY * 0.5f;
        } else {
            drawX = expectedCenterX - ns.w * scaleX * 0.5f;
            drawY = expectedCenterY - ns.h * scaleY * 0.5f;
        }

        C2D_ImageTint alphaTint;
        C2D_ImageTint* passTint = nullptr;
        float baseAlpha = getLaneAlpha(n.noteData, n.isPlayer) * n.multAlpha;
        float noteAngle = (getLaneAngle(n.noteData, n.isPlayer) + n.angle) * (3.14159265f / 180.0f);

        bool useFastTintHead = ClientPrefs::fastNotes && fastNoteSheet && fastNoteSubtexs.size() >= 2
                               && n.texture.empty();
        static const unsigned char FAST_COLORS_H[4][3] = {
            {0xC2,0x4B,0x99}, {0x00,0xFF,0xFF}, {0x12,0xFA,0x05}, {0xF9,0x39,0x3F}
        };
        unsigned char r = 255, g = 255, b = 255;
        u8 a_note = 255;
        if (n.color != 0xFFFFFFFF) {
            a_note = (n.color >> 24) & 0xFF;
            if (a_note == 0 && n.color != 0) a_note = 255;
            r = (n.color >> 16) & 0xFF;
            g = (n.color >> 8) & 0xFF;
            b = n.color & 0xFF;
        } else if (useFastTintHead) {
            if (ClientPrefs::noteColorsEnabled) {
                r = ClientPrefs::noteColors[n.noteData % 4][0];
                g = ClientPrefs::noteColors[n.noteData % 4][1];
                b = ClientPrefs::noteColors[n.noteData % 4][2];
            } else {
                r = FAST_COLORS_H[n.noteData % 4][0];
                g = FAST_COLORS_H[n.noteData % 4][1];
                b = FAST_COLORS_H[n.noteData % 4][2];
            }
        }
        
        float aVal = baseAlpha * a_note / 255.0f;
        if (n.ignoreNote && n.hit) {
            aVal *= 0.3f;
        }
        if (n.color != 0xFFFFFFFF || useFastTintHead) {
            C2D_PlainImageTint(&alphaTint, C2D_Color32(r, g, b, 255), 1.0f);
            for (int ci = 0; ci < 4; ci++) {
                alphaTint.corners[ci].color = (alphaTint.corners[ci].color & 0x00FFFFFF) | ((u32)(aVal * 255) << 24);
            }
            passTint = &alphaTint;
        } else {
            if (aVal < 1.0f) {
                C2D_AlphaImageTint(&alphaTint, aVal);
                passTint = &alphaTint;
            }
        }

        // For fast notes draw rotated head
        if (useFastTintHead) {
            static const float FAST_ANGLES[4] = { -M_PI/2.0f, M_PI, 0.0f, M_PI/2.0f };
            C2D_SetTintMode(C2D_TintMult);
            renderNoteSprite(ns, drawX, drawY, 0.90f, passTint, sx * n.scaleX, sy * n.scaleY, FAST_ANGLES[n.noteData % 4] + noteAngle, n.flipX, n.flipY, n.antialiasing);
            C2D_SetTintMode(C2D_TintMult);
        } else {
            renderNoteSprite(ns, drawX, drawY, 0.90f, passTint, sx * n.scaleX, sy * n.scaleY, noteAngle, n.flipX, n.flipY, n.antialiasing);
        }
    }

    // Draw debug logs
    int maxLogs = 5;
    for (int i = 0; i < std::min((int)debugLogs.size(), maxLogs); i++) {
        float scale = 0.3f;
        float drawY = 10.0f + (i * 12.0f);
        
        // Custom alpha tint (very simplified)
        u32 color = C2D_Color32(255, 255, 255, 255);
        if (debugLogs[i].timer < 1.0f) {
            u8 a = (u8)(debugLogs[i].timer * 255);
            color = C2D_Color32(255, 255, 255, a);
        }

        C2D_Text logObj;
        C2D_TextFontParse(&logObj, vcrFont, vcrFontBuf, debugLogs[i].text.c_str());
        C2D_TextOptimize(&logObj);
        
        // Manual draw to support alpha without affecting others
        float tw, th;
        C2D_TextGetDimensions(&logObj, scale, scale, &tw, &th);
        
        // Border
        u32 borderColor = C2D_Color32(0, 0, 0, (u8)((color >> 24) & 0xFF));
        DrawTextBorderCardinal(&logObj, 10.0f, drawY, 0.84f, scale, scale, 1.5f, borderColor);
        C2D_DrawText(&logObj, C2D_WithColor, 10.0f, drawY, 0.85f, scale, scale, color);
    }
    
    if (ClientPrefs::botPlay) {
        float playerStartX = getLaneX(0, true);
        float playerEndX = getLaneX(3, true) + spacing;
        float strumCenterX = playerStartX + (playerEndX - playerStartX) * 0.5f;
        float strumCenterY = receptorY + (spacing * 0.5f);

        strumCenterX = centerXT + (strumCenterX - centerXT) * hudZoom + shakeX;
        strumCenterY = centerYT + (strumCenterY - centerYT) * hudZoom + shakeY;

        float textY = strumCenterY;

        C2D_Text botplayTextObj;
        C2D_TextFontParse(&botplayTextObj, vcrFont, vcrFontBuf, "BOTPLAY");
        C2D_TextOptimize(&botplayTextObj);

        float bpScale = 0.55f * hudZoom;
        float bpW = 0.0f, bpH = 0.0f;
        C2D_TextGetDimensions(&botplayTextObj, bpScale, bpScale, &bpW, &bpH);

        float bpX = strumCenterX - (bpW * 0.5f);
        float bpDrawY = textY - (bpH * 0.5f);

        DrawTextBorderCardinal(&botplayTextObj, bpX, bpDrawY, 0.88f, bpScale, bpScale, 1.5f, C2D_Color32(0, 0, 0, 255));
        C2D_DrawText(&botplayTextObj, C2D_WithColor, bpX, bpDrawY, 0.89f, bpScale, bpScale, CWhite);
    }

    // Lua Texts are now handled in the main draw loop for better layering control
}

static void DrawAlignedC2DTextWithBorder(C2D_Text* textObj, u32 flags, float x, float y, float depth, float scaleX, float scaleY, float borderSize, u32 borderColor, u32 textColor) {
    if (borderSize > 0.0f) {
        C2D_DrawText(textObj, flags, x - borderSize, y, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x + borderSize, y, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x, y - borderSize, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x, y + borderSize, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x - borderSize, y - borderSize, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x + borderSize, y - borderSize, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x - borderSize, y + borderSize, depth, scaleX, scaleY, borderColor);
        C2D_DrawText(textObj, flags, x + borderSize, y + borderSize, depth, scaleX, scaleY, borderColor);
    }
    C2D_DrawText(textObj, flags, x, y, depth + 0.01f, scaleX, scaleY, textColor);
}

void PlayState::drawLuaTextsForCamera(const std::string& camera, bool front, float shakeX, float shakeY) {
    for (auto& t : luaTexts) {
        if (!t.active || !t.visible || t.camera != camera || t.front != front) continue;
        
        updateLuaText(t);

        float finalX = t.x + shakeX;
        float finalY = t.y + shakeY;
        float finalDepth = front ? 0.92f : 0.70f; // Default for HUD

        // Scale by camera zoom if it's camGame
        float drawScale = t.size;
        if (camera == "camGame") {
             // Treat t.x and t.y as 3DS-scaled world coords, but apply camera offset
             float screenScale = 240.0f / 720.0f;
             float centerXT = 400.0f / 2.0f;
             float centerYT = 240.0f / 2.0f;
             
             // Scroll factor 1.0 assumed for text by default
             float scrollX = 1.0f; 
             float scrollY = 1.0f;

             finalX = (t.x - (camX * scrollX * screenScale)) * camZoom + centerXT;
             finalY = (t.y - (camY * scrollY * screenScale)) * camZoom + centerYT;
             drawScale *= camZoom;
             finalDepth = front ? 0.50f : 0.30f; // Game depth (BF is 0.45, Dad 0.4, GF 0.35)
        } else if (camera == "camHUD" || camera == "hud") {
             float centerXT = ScreenWidthTop / 2.0f;
             float centerYT = ScreenHeight / 2.0f;
             finalX = centerXT + (t.x - centerXT) * hudZoom + shakeX;
             finalY = centerYT + (t.y - centerYT) * hudZoom + shakeY;
             drawScale *= hudZoom;
        }

        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&t.c2dObj, drawScale, drawScale, &tw, &th);
        
        u32 drawFlags = C2D_WithColor;
        float drawX = finalX;
        if (t.alignment == "center") {
            drawFlags |= C2D_AlignCenter;
            drawX = finalX + tw / 2.0f;
        } else if (t.alignment == "right") {
            drawFlags |= C2D_AlignRight;
            drawX = finalX + tw;
        }

        u8 a = (u8)(t.alpha * 255.0f);
        u32 bColor = (t.borderColor & 0x00FFFFFF) | ((u32)a << 24);
        u32 finalColor = (t.color & 0x00FFFFFF) | ((u32)a << 24);

        DrawAlignedC2DTextWithBorder(&t.c2dObj, drawFlags, drawX, finalY, finalDepth, drawScale, drawScale, t.borderSize, bColor, finalColor);
    }
}

void PlayState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    ClearTextBuf();
    if (!noteSheet) return;

    this->top = top;
    this->bottom = bottom;

    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));

    ShaderManager::get().init(); // Ensure targets are ready
    ShaderManager::get().beginCamera("camGame", top);

    if (gameOver) {
        if (currentStage) currentStage->draw(camX, camY, camZoom, false, csX, csY);
        if (deadBF) {
            deadBF->draw(0, 0, 0.45f, camZoom, camX, camY, csX, csY);
        }
        if (currentStage) currentStage->draw(camX, camY, camZoom, true, csX, csY);
    } else {
        if (currentStage) currentStage->draw(camX, camY, camZoom, false, csX, csY);

        // Layer 1: camGame (Back)
        drawLuaSpritesForCamera("camGame", false, csX, csY);
        drawLuaTextsForCamera("camGame", false, csX, csY);

        if (gf && (!currentStage || !currentStage->hideGirlfriend)) gf->draw(0, 0, 0.35f, camZoom, camX, camY, csX, csY);
        if (dad) dad->draw(0, 0, 0.4f, camZoom, camX, camY, csX, csY);
        if (bf)  bf->draw(0, 0, 0.45f, camZoom, camX, camY, csX, csY);

        if (currentStage) currentStage->draw(camX, camY, camZoom, true, csX, csY);
        // Layer 2: camGame (Front)
        drawLuaSpritesForCamera("camGame", true, csX, csY);
        drawLuaTextsForCamera("camGame", true, csX, csY);
        if (inGameVideo && !inGameVideo->inFrontOfHUD) inGameVideo->draw();
    }

    ShaderManager::get().endCamera("camGame", top, bottom);

    if (!gameOver) {
        // Layer 3: camHUD (Back)
        ShaderManager::get().beginCamera("camHUD", top);
        
        drawLuaSpritesForCamera("camHUD", false, hsX, hsY);
        drawLuaTextsForCamera("camHUD", false, hsX, hsY);

        drawNotes(hsX, hsY); // Includes HUD elements like healthbar and strums


        // Layer 4: camHUD (Front)
        drawLuaSpritesForCamera("camHUD", true, hsX, hsY);
        drawLuaTextsForCamera("camHUD", true, hsX, hsY);

        // Draw Rating inside HUD camera so it gets affected by HUD shaders and zoom
        if (ClientPrefs::showRatings && ratingActive && ratingSheet && ratingSubtexs.count(currentRatingStr)) {
            Tex3DS_SubTexture& sub = ratingSubtexs[currentRatingStr];
            C2D_Image img = { ratingBaseImage.tex, &sub };

            C2D_ImageTint tint;
            C2D_AlphaImageTint(&tint, ratingAlpha * ClientPrefs::comboAlpha); // Transparency
            
            float drawX = ratingX - (sub.width * ratingScale / 2.0f) + hsX;
            float drawY = ratingY - (sub.height * ratingScale / 2.0f) + hsY;
            
            drawImageScaledTinted(img, drawX, drawY, 0.95f, ratingScale, ratingScale, &tint);
        }

        ShaderManager::get().endCamera("camHUD", top, bottom);
    }

    // Layer 5: camOther
    ShaderManager::get().beginCamera("camOther", top);
    drawLuaSpritesForCamera("camOther", false, 0, 0);
    drawLuaTextsForCamera("camOther", false, 0, 0);
    drawLuaSpritesForCamera("camOther", true, 0, 0);
    drawLuaTextsForCamera("camOther", true, 0, 0);
    if (inGameVideo && inGameVideo->inFrontOfHUD) inGameVideo->draw();

    ShaderManager::get().endCamera("camOther", top, bottom);

    if (!gameOver) {
        // Layer 6: camBottom (Always on top of bottom screen after extended game/HUD cameras are presented)
        ShaderManager::get().beginCamera("camBottom", bottom);
        drawLuaSpritesForCamera("camBottom", false, hsX, hsY);
        drawLuaTextsForCamera("camBottom", false, hsX, hsY);
        drawHUD(hsX, hsY);
        drawLuaSpritesForCamera("camBottom", true, hsX, hsY);
        drawLuaTextsForCamera("camBottom", true, hsX, hsY);
        
        ShaderManager::get().endCamera("camBottom", bottom, nullptr);
    }

    if (ClientPrefs::hitboxEnabled && !gameOver) {
        C2D_SceneBegin(bottom);
        drawHitboxOverlay();
        C2D_SceneBegin(top);
    }

    if (gameOver && deathConfirmActive) {
        float alpha = 0.0f;
        if (deathConfirmTimer > 0.7f) {
            alpha = (deathConfirmTimer - 0.7f) / 2.0f;
            if (alpha > 1.0f) alpha = 1.0f;
        }
        u8 a = (u8)(alpha * 255);
        C2D_DrawRectSolid(0, 0, 0.99f, ScreenWidthTop, ScreenHeight, C2D_Color32(0, 0, 0, a));
        
        // Also ensure bottom screen is black (cleared to black by default, but draw just in case)
        C2D_SceneBegin(bottom);
        C2D_DrawRectSolid(0, 0, 0.99f, 320, 240, C2D_Color32(0, 0, 0, a));
        C2D_SceneBegin(top);
    }

    if (paused && pauseSubState) pauseSubState->draw();

    if (ClientPrefs::debugInfo) {
        C2D_SceneBegin(bottom);
        extern u32 __ctru_linear_heap_size;
        float lramTotal = (float)__ctru_linear_heap_size / (1024.0f * 1024.0f);
        float lramUsed = lramTotal - ((float)linearSpaceFree() / (1024.0f * 1024.0f));
        
        char debugStr[256];
        if (!ClientPrefs::extendedDebug) {
            sprintf(debugStr, "FPS: %d | L-RAM: %.1f/%.1f MB", (int)curFPS, lramUsed, lramTotal);
        } else {
            float vramUsed = (float)(6 * 1024 * 1024 - vramSpaceFree()) / (1024.0f * 1024.0f);
            float vramTotal = 6.0f;
            struct mallinfo mi = mallinfo();
            float stdRamUsed = (float)mi.uordblks / (1024.0f * 1024.0f);
            bool isNew3DS = false;
            APT_CheckNew3DS(&isNew3DS);
            sprintf(debugStr, "FPS: %d | Model: %s\nRAM: %.1f MB | L-RAM: %.1f/%.1f MB\nVRAM: %.1f/%.1f MB", 
                    (int)curFPS, isNew3DS ? "New" : "Old", stdRamUsed, lramUsed, lramTotal, vramUsed, vramTotal);
        }
        
        std::vector<std::string> debugLines;
        std::string temp = "";
        for (int i = 0; debugStr[i] != '\0'; i++) {
            if (debugStr[i] == '\n') {
                debugLines.push_back(temp);
                temp = "";
            } else {
                temp += debugStr[i];
            }
        }
        if (!temp.empty()) debugLines.push_back(temp);
        
        float scale = 0.35f;
        float lineSpacing = 10.0f;
        float totalHeight = debugLines.size() * lineSpacing;
        float startY = 240.0f - totalHeight - 6.0f;
        
        if (!debugTextBuf) {
            debugTextBuf = C2D_TextBufNew(512);
        } else {
            C2D_TextBufClear(debugTextBuf);
        }
        std::vector<C2D_Text> lineObjs(debugLines.size());
        std::vector<float> lineWidths(debugLines.size(), 0.0f);
        float maxLineW = 0.0f;
        
        for (size_t i = 0; i < debugLines.size(); i++) {
            C2D_TextFontParse(&lineObjs[i], vcrFont, debugTextBuf, debugLines[i].c_str());
            C2D_TextOptimize(&lineObjs[i]);
            
            float lineW = 0, lineH = 0;
            C2D_TextGetDimensions(&lineObjs[i], scale, scale, &lineW, &lineH);
            lineWidths[i] = lineW;
            if (lineW > maxLineW) maxLineW = lineW;
        }
        
        for (size_t i = 0; i < debugLines.size(); i++) {
            float lx = 160.0f - (lineWidths[i] / 2.0f);
            float ly = startY + (float)i * lineSpacing;
            
            // Draw an independent background rectangle for this line specifically
            float lineRectW = lineWidths[i] + 8.0f;
            float lineRectH = lineSpacing;
            float lineRectX = 160.0f - (lineRectW / 2.0f);
            float lineRectY = ly - 1.0f;
            
            // Use depths greater than 0.999f so it renders over touchscreen hitboxes
            C2D_DrawRectSolid(lineRectX, lineRectY, 0.9992f, lineRectW, lineRectH, C2D_Color32(0, 0, 0, 180));
            
            DrawTextBorderCardinal(&lineObjs[i], lx, ly, 0.9995f, scale, scale, 1.2f, C2D_Color32(0, 0, 0, 255));
            C2D_DrawText(&lineObjs[i], C2D_WithColor, lx, ly, 0.9998f, scale, scale, CWhite);
        }
    }
}

void PlayState::drawHitboxOverlay() {
    if (!ClientPrefs::hitboxEnabled || ClientPrefs::hitboxMode == 2) return;

    for (int i = 0; i < 4; i++) {
        bool pressed = keyHeld[i];
        int alpha = 0;
        if (ClientPrefs::hitboxMode == 0) { // Siempre visible
            alpha = pressed ? (int)(255.0f * (ClientPrefs::hitboxAlphaTouch / 100.0f)) : (int)(255.0f * (ClientPrefs::hitboxAlphaNormal / 100.0f));
        } else if (ClientPrefs::hitboxMode == 1) { // Solo al tocar
            alpha = pressed ? (int)(255.0f * (ClientPrefs::hitboxAlphaTouch / 100.0f)) : 0;
        }

        if (alpha <= 0) continue;

        u8 r = 0, g = 0, b = 0;
        if (i == 0) { r = 0x7E; g = 0x6A; b = 0xB5; } // Left
        else if (i == 1) { r = 0x00; g = 0xFF; b = 0xFF; } // Down
        else if (i == 2) { r = 0x12; g = 0xFA; b = 0x05; } // Up
        else if (i == 3) { r = 0xF9; g = 0x39; b = 0x3F; } // Right

        u32 col = C2D_Color32(r, g, b, alpha);
        float x = i * 80.0f;
        
        if (ClientPrefs::hitboxStyle == 0) { // Completa
            C2D_DrawRectSolid(x, 0.0f, 0.999f, 80.0f, 240.0f, col);
        } else { // Extremos superiores e inferiores
            C2D_DrawRectSolid(x, 0.0f, 0.999f, 80.0f, 15.0f, col);
            C2D_DrawRectSolid(x, 240.0f - 15.0f, 0.999f, 80.0f, 15.0f, col);
        }
    }
}


void PlayState::moveChar(int lane, Character* c) {
    if (!c) return;
    static const std::string dirs[] = {"LEFT", "DOWN", "UP", "RIGHT"};
    c->playAnim("sing" + dirs[lane], true);
    c->holdTimer = 0;
}

void PlayState::addDebugMessage(const std::string& text) {
    DebugLog log;
    log.text = text;
    log.timer = 5.0f;
    debugLogs.push_back(log);
    if (debugLogs.size() > 10) debugLogs.erase(debugLogs.begin());
}



void PlayState::stepHit(int step) {
    LuaManager::get().callFunction("onStepHit", {std::to_string(step)});
}

void PlayState::beatHit(int beat) {
    iconBump = 1.2f;

    if (camZooming && beat % 4 == 0) {
        camZoom += 0.015f;
        hudZoom += 0.030f;
    }

    if (gf && (beat % gf->danceEveryNumBeats == 0)) {
        gf->dance();
    }
    if (dad && (beat % dad->danceEveryNumBeats == 0)) dad->dance();
    if (bf && (beat % bf->danceEveryNumBeats == 0)) bf->dance();
    
    LuaManager::get().callFunction("onBeatHit", {std::to_string(beat)});
}

void PlayState::endSong() {
    SongInfo info = WeekData::findSongInfo(curSong);

    if (!ClientPrefs::botPlay) {
        if (accuracy < 20.0f) Achievements::unlockAchievement("ur_bad");
        if (accuracy == 100.0f) Achievements::unlockAchievement("ur_good");
        if (maxHoldTime >= 10.0f) Achievements::unlockAchievement("oversinging");
        if (!bfWentIdle) Achievements::unlockAchievement("hype");
        if (keysUsed.size() <= 2) Achievements::unlockAchievement("two_keys");

        std::string lowerDiff = currentDifficulty;
        std::transform(lowerDiff.begin(), lowerDiff.end(), lowerDiff.begin(), ::tolower);
        if (lowerDiff == "hard" && misses == 0) {
            if (curSong == "Bopeebo" || curSong == "Fresh" || curSong == "Dad Battle") Achievements::unlockAchievement("week1_nomiss");
            if (curSong == "Spookeez" || curSong == "South" || curSong == "Monster") Achievements::unlockAchievement("week2_nomiss");
            if (curSong == "Pico" || curSong == "Philly Nice" || curSong == "Blammed") Achievements::unlockAchievement("week3_nomiss");
            if (curSong == "Satin Panties" || curSong == "High" || curSong == "MILF") Achievements::unlockAchievement("week4_nomiss");
            if (curSong == "Cocoa" || curSong == "Eggnog" || curSong == "Winter Horrorland") Achievements::unlockAchievement("week5_nomiss");
            if (curSong == "Senpai" || curSong == "Roses" || curSong == "Thorns") Achievements::unlockAchievement("week6_nomiss");
            if (curSong == "Ugh" || curSong == "Guns" || curSong == "Stress") Achievements::unlockAchievement("week7_nomiss");
        }
    }

    MusicBeatState* nextState = nullptr;
    if (isStoryMode && curSongIdx + 1 < (int)weekData.songs.size()) {
        nextState = new PlayState(weekData, curSongIdx + 1, currentDifficulty);
    } else {
        std::string resName = isStoryMode ? weekData.weekName : curSong;
        nextState = new ResultState(isStoryMode, false, resName, currentDifficulty, (int)totalNotesHit + misses, maxCombo, sicks, goods, bads, shits, misses, score);
    }

    if (!info.outroVideo.empty()) {
        switchState(new VideoState(info.outroVideo, nextState));
    } else {
        switchState(nextState);
    }
}



void PlayState::focusCamera(bool isPlayer) {
    if (camFollowLocked) {
        camFollowX = lockedCamX;
        camFollowY = lockedCamY;
        return;
    }
    if (!currentStage) return;
    Character* c = isPlayer ? bf : dad;
    if (!c) return;

    if (isPlayer) {
        camFollowX = c->x + 150.0f + c->camOffsetX + currentStage->bfCamX;
        camFollowY = c->y + 150.0f + c->camOffsetY + currentStage->bfCamY;
    } else {
        camFollowX = c->x + 150.0f + c->camOffsetX + currentStage->dadCamX;
        camFollowY = c->y + 150.0f + c->camOffsetY + currentStage->dadCamY;
    }
}


void PlayState::exitState() {
    if (deadBF) {
        delete deadBF;
        deadBF = nullptr;
    }
    stopVideo();
    aptSetHomeAllowed(true); // Ensure Home button is re-enabled if a Lua script disabled it
    ShaderManager::get().cleanup(); // Free all render targets and active shaders from L-RAM/VRAM

    // Stop the async background thread FIRST to prevent deadlocks during teardown.
    // The thread accesses Character objects and texture data — must not run concurrently.
    AsyncAssetManager::get().suspend();
    
    fastNoteSheet = nullptr;
    for (auto& pair : customNoteSheets) {
        C2D_SpriteSheet sheet = pair.second;
        if (sheet && !SpritesheetCache::get().contains(sheet)) {
            C2D_SpriteSheetFree(sheet);
        }
    }
    customNoteSheets.clear();

    for (auto& pair : customNoteImages) {
        C2D_Image& img = pair.second;
        if (img.tex) {
            if (img.tex->data) {
                linearFree(img.tex->data);
                img.tex->data = nullptr;
            }
            C3D_TexDelete(img.tex);
            delete img.tex;
        }
        if (img.subtex) {
            delete img.subtex;
        }
    }
    customNoteImages.clear();

    for (auto& t : luaTexts) {
        if (t.buf) C2D_TextBufDelete(t.buf);
    }
    luaTexts.clear();
    luaTextIndices.clear();

    LuaManager::get().clearAllSprites();
    luaSprites.clear();
    luaSpriteIndices.clear();
    activeTweens.clear();

    LuaManager::get().close();
    PlayState::instance = nullptr;
    
    AudioEngine::exit();
    
    noteSheet = nullptr;
    countdownSheet = nullptr;
    AudioEngine::freeCountdownSounds();
    ratingSheet = nullptr;

    for (auto& pair : healthIconCache) {
        HealthIconData& ic = pair.second;
        if (ic.sheet) {
            C2D_SpriteSheetFree(ic.sheet);
            ic.sheet = nullptr;
        } else if (ic.loaded && ic.vramData) {
            ic.tex.data = nullptr;
            C3D_TexDelete(&ic.tex);
            if (addrIsVRAM(ic.vramData)) vramFree(ic.vramData);
            else linearFree(ic.vramData);
            ic.vramData = nullptr;
        }
    }
    healthIconCache.clear();
    iconBf = HealthIconData();
    iconDad = HealthIconData();
    bf = nullptr;
    dad = nullptr;
    gf = nullptr;

    // clearAll deletes Character objects (their destructors decrement charTextureCache refs).
    // Must happen BEFORE SpritesheetCache::clear() which wipes the spritesheet memory.
    AsyncAssetManager::get().clearAll();

    // Now that all characters and sprites are gone, clear the shared texture pools.
    SpritesheetCache::get().clear();

    delete currentStage;
    if (vcrFontBuf) C2D_TextBufDelete(vcrFontBuf);
    if (pauseSubState) {
        delete pauseSubState;
        pauseSubState = nullptr;
    }
    if (lyricsTextBuf) C2D_TextBufDelete(lyricsTextBuf);
    if (debugTextBuf) {
        C2D_TextBufDelete(debugTextBuf);
        debugTextBuf = nullptr;
    }
    if (lazyBGSheet) C2D_SpriteSheetFree(lazyBGSheet);
    
    if (lazyIsRaw) {
        if (lazyRawTex) {
            C3D_TexDelete(lazyRawTex);
            delete lazyRawTex;
        }
        if (lazyRawSub) delete lazyRawSub;
    }
    lazyRawTex = nullptr;
    lazyRawSub = nullptr;
    lazyIsRaw = false;

    // Resume background asset loading for the next state (e.g. FreeplayState / StoryMenuState)
    AsyncAssetManager::get().resume();
}



void PlayState::drawText(C2D_Text* textObj, float x, float y, float scale, bool centered, float maxWidth) {
    float textW, textH;
    C2D_TextGetDimensions(textObj, scale, scale, &textW, &textH);
    
    float actualScale = scale;
    if (maxWidth > 0 && textW > maxWidth) {
        actualScale = scale * (maxWidth / textW);
        C2D_TextGetDimensions(textObj, actualScale, actualScale, &textW, &textH);
    }
    
    float drawX = centered ? x - (textW / 2.0f) : x;
    float drawY = centered ? y - (textH / 2.0f) : y;
    
    C2D_DrawText(textObj, C2D_WithColor, drawX, drawY, 0.85f, actualScale, actualScale, CWhite);
}

void PlayState::drawLuaSpritesForCamera(const std::string& camera, bool front, float shakeX, float shakeY) {
    float screenScale = 240.0f / 720.0f;
    
    // We expect camera parameter to be exactly "camGame", "camHUD" or "camOther"

    std::vector<StageSprite*> sortedSprites;
    for (auto& ls : luaSprites) {
        if (ls.camera != camera || ls.front != front) continue;
        if (!ls.visible) continue;
        if (!ls.isGraphic && !ls.img.tex) continue;
        sortedSprites.push_back(&ls);
    }

    std::sort(sortedSprites.begin(), sortedSprites.end(), [](StageSprite* a, StageSprite* b) {
        return a->zOrder < b->zOrder;
    });

    for (auto* lsPtr : sortedSprites) {
        auto& ls = *lsPtr;

        C2D_Image useImg = ls.img;
        bool frameRotated = false;
        
        const Frame* curFrame = nullptr;
        if (ls.animated && ls.currentAnim && !ls.currentAnim->indices.empty()) {
            int animFrame = (int)ls.curFrame;
            // Clamp to valid range to prevent out-of-bounds crash with mods that
            // have animation indices but missing/empty frame data.
            if (animFrame < 0) animFrame = 0;
            if (animFrame >= (int)ls.currentAnim->indices.size())
                animFrame = (int)ls.currentAnim->indices.size() - 1;
            int frameIdx = ls.currentAnim->indices[animFrame];
            if (frameIdx >= 0 && frameIdx < (int)ls.frames.size()) {
                curFrame = &ls.frames[frameIdx];
            }
        }

        if (curFrame) {
            useImg.subtex = &curFrame->uv;
            frameRotated = curFrame->rotated;
        }

        float currentZoom = 1.0f;
        float finalX = 0, finalY = 0;
        
        if (camera == "camGame" || camera == "game") {
            currentZoom = camZoom;
            finalX = (ls.x - (camX * ls.scrollX)) * currentZoom * screenScale + (ScreenWidthTop / 2.0f);
            finalY = (ls.y - (camY * ls.scrollY)) * currentZoom * screenScale + (ScreenHeight / 2.0f);
        } else if (camera == "camHUD" || camera == "hud") {
            currentZoom = hudZoom;
            float centerXT = ScreenWidthTop / 2.0f;
            float centerYT = ScreenHeight / 2.0f;
            finalX = centerXT + (ls.x - centerXT) * currentZoom;
            finalY = centerYT + (ls.y - centerYT) * currentZoom;
        } else { // camOther / other / static
            finalX = ls.x;
            finalY = ls.y;
        }

        finalX += shakeX;
        finalY += shakeY;

        float absScaleX = fabsf(ls.scaleX * currentZoom);
        float absScaleY = fabsf(ls.scaleY * currentZoom);
        if (camera == "camGame" || camera == "game") {
            absScaleX *= screenScale;
            absScaleY *= screenScale;
        }
        float drawScaleX = ls.flipX ? -absScaleX : absScaleX;
        float drawScaleY = ls.flipY ? -absScaleY : absScaleY;

        float depth = 0.11f; // Default back
        if (front) depth = 0.52f; // Default front
        if (camera == "camHUD" || camera == "hud") depth += 0.4f;
        if (camera == "camOther" || camera == "other") depth += 0.47f;

        float finalAlpha = ls.alpha;
        if (camera == "camGame" || camera == "game") finalAlpha *= camAlpha;
        else if (camera == "camHUD" || camera == "hud") finalAlpha *= hudAlpha;

        if (ls.isGraphic) {
            float w = ls.graphicWidth * drawScaleX;
            float h = ls.graphicHeight * drawScaleY;

            u32 color = ls.graphicColor;
            if (finalAlpha < 1.0f) {
                u8 r = color & 0xFF;
                u8 g = (color >> 8) & 0xFF;
                u8 b = (color >> 16) & 0xFF;
                u8 a = (color >> 24) & 0xFF;
                a = (u8)(a * finalAlpha);
                color = C2D_Color32(r, g, b, a);
            }
            C2D_DrawRectSolid(finalX, finalY, depth, w, h, color);
            continue;
        }

        // Guard: never call GPU draw with a null texture — this causes GPU address crashes
        // (visible in Citra as "Read from unknown GPU address") when mods have missing .t3x files.
        if (!useImg.tex) continue;

        C2D_ImageTint* tintPtr = nullptr;
        C2D_ImageTint tint;
        if (finalAlpha < 1.0f) {
            C2D_AlphaImageTint(&tint, finalAlpha);
            tintPtr = &tint;
        }

        // Apply frameX/frameY trim offsets and animation offsets
        float drawX = finalX;
        float drawY = finalY;

        // Step 1: HaxeFlixel origin pivot compensation (same as Character::draw).
        // When scale != 1, HaxeFlixel scales from the center of the LOGICAL (untrimmed) frame,
        // not from the top-left corner. We must shift drawX/drawY to compensate.
        if (curFrame) {
            float baseScaleUnit = (camera == "camGame" || camera == "game") ? (screenScale * currentZoom) : currentZoom;
            float originX = curFrame->frameW / 2.0f;
            float originY = curFrame->frameH / 2.0f;
            drawX += originX * (1.0f - ls.scaleX) * baseScaleUnit;
            drawY += originY * (1.0f - ls.scaleY) * baseScaleUnit;
        }

        // Step 2: Apply frameX/frameY trim offsets and animation offsets
        if (curFrame && ls.currentAnim) {
            float frameX = curFrame->frameX;
            float frameY = curFrame->frameY;
            float animOffX = ls.currentAnim->offsetX;
            float animOffY = ls.currentAnim->offsetY;
            if (ls.flipX) {
                drawX += (curFrame->frameW + frameX - animOffX) * absScaleX;
            } else {
                drawX -= (frameX + animOffX) * absScaleX;
            }
            if (ls.flipY) {
                drawY += (curFrame->frameH + frameY - animOffY) * absScaleY;
            } else {
                drawY -= (frameY + animOffY) * absScaleY;
            }
        }

        static Tex3DS_SubTexture defaultSubtex;
        if (useImg.subtex == nullptr) {
            defaultSubtex.width = useImg.tex ? useImg.tex->width : 0;
            defaultSubtex.height = useImg.tex ? useImg.tex->height : 0;
            defaultSubtex.left = 0.0f;
            defaultSubtex.top = 0.0f;
            defaultSubtex.right = 1.0f;
            defaultSubtex.bottom = 1.0f;
            useImg.subtex = &defaultSubtex;
        }

        float w = (float)useImg.subtex->width;
        float h = (float)useImg.subtex->height;

        float imgW = frameRotated ? h : w;
        float imgH = frameRotated ? w : h;

        float scaleX = ls.flipX ? -absScaleX : absScaleX;
        float scaleY = ls.flipY ? -absScaleY : absScaleY;
        if (frameRotated) {
            scaleX = ls.flipY ? -absScaleX : absScaleX;
            scaleY = ls.flipX ? -absScaleY : absScaleY;
        }

        float centerX = drawX + (imgW * (ls.flipX ? -absScaleX : absScaleX)) / 2.0f;
        float centerY = drawY + (imgH * (ls.flipY ? -absScaleY : absScaleY)) / 2.0f;
        
        float angleRad = ls.angle * (3.14159265f / 180.0f);
        if (frameRotated) {
            angleRad -= (3.14159265f / 2.0f);
        }

        // When flipping, always draw around the fixed center so the flip pivots in
        // place instead of shifting the sprite by its (per-frame) width.
        if (ls.angle != 0.0f || frameRotated || !legacyPositioning || ls.flipX || ls.flipY) {
            C2D_DrawImageAtRotated(useImg, centerX, centerY, depth, angleRad, tintPtr, scaleX, scaleY);
        } else {
            C2D_DrawImageAt(useImg, roundf(drawX), roundf(drawY), depth, tintPtr, scaleX, scaleY);
        }
    }
}

void PlayState::applyCharacterSwap(const std::string& charType, const std::string& charName, Character* newChar) {
    if (!newChar) return;

    if (newChar->curCharacterName.empty()) {
        newChar->curCharacterName = charName;
    }

    std::string type = charType;
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "dad" || type == "0" || type == "opponent") {
        float oldSpawnX = dad ? (dad->baseX + (currentStage ? currentStage->dadX : 0.0f)) : 0.0f;
        float oldSpawnY = dad ? (dad->baseY + (currentStage ? currentStage->dadY : 0.0f)) : 0.0f;
        float offsetX = dad ? (dad->x - oldSpawnX) : 0.0f;
        float offsetY = dad ? (dad->y - oldSpawnY) : 0.0f;
        bool oldVisible = dad ? dad->visible : true;

        dad = newChar;
        dad->x = dad->baseX + offsetX; dad->y = dad->baseY + offsetY;
        if (currentStage) { dad->x += currentStage->dadX; dad->y += currentStage->dadY; }
        dad->visible = oldVisible;
        // Suspend async thread to prevent SD contention if loadHealthIcon falls back to disk
        AsyncAssetManager::get().suspend();
        loadHealthIcon(iconDad, dad->healthIcon);
        AsyncAssetManager::get().resume();
    } else if (type == "bf" || type == "boyfriend" || type == "1" || type == "player") {
        float oldSpawnX = bf ? (bf->baseX + (currentStage ? currentStage->bfX : 0.0f)) : 0.0f;
        float oldSpawnY = bf ? (bf->baseY + (currentStage ? currentStage->bfY : 0.0f)) : 0.0f;
        float offsetX = bf ? (bf->x - oldSpawnX) : 0.0f;
        float offsetY = bf ? (bf->y - oldSpawnY) : 0.0f;
        bool oldVisible = bf ? bf->visible : true;

        newChar->isPlayer = true;
        bf = newChar;
        bf->x = bf->baseX + offsetX; bf->y = bf->baseY + offsetY;
        if (currentStage) { bf->x += currentStage->bfX; bf->y += currentStage->bfY; }
        bf->visible = oldVisible;
        AsyncAssetManager::get().suspend();
        loadHealthIcon(iconBf, bf->healthIcon);
        AsyncAssetManager::get().resume();
    } else if (type == "gf" || type == "girlfriend" || type == "2") {
        float oldSpawnX = gf ? (gf->baseX + (currentStage ? currentStage->gfX : 0.0f)) : 0.0f;
        float oldSpawnY = gf ? (gf->baseY + (currentStage ? currentStage->gfY : 0.0f)) : 0.0f;
        float offsetX = gf ? (gf->x - oldSpawnX) : 0.0f;
        float offsetY = gf ? (gf->y - oldSpawnY) : 0.0f;
        bool oldVisible = gf ? gf->visible : true;

        gf = newChar;
        gf->x = gf->baseX + offsetX; gf->y = gf->baseY + offsetY;
        if (currentStage) { gf->x += currentStage->gfX; gf->y += currentStage->gfY; }
        gf->visible = oldVisible;
    }

    // Schedule GPU-safe cleanup for next frame
    deferredClearUnused = true;
}

void PlayState::triggerEvent(const Event& event) {
    if (event.name == "Lyrics") {
        currentLyrics = event.value1;
        currentLyricsColor = C2D_Color32(255, 255, 255, 255); // Default
        currentLyricsSize = 0.7f; // Default

        if (!event.value2.empty()) {
            size_t comma = event.value2.find(',');
            std::string hexStr = event.value2;
            if (comma != std::string::npos) {
                hexStr = event.value2.substr(0, comma);
                currentLyricsSize = std::atof(event.value2.substr(comma + 1).c_str()) / 30.0f; // Scale rough guess for 3DS
            }
            
            // Trim spaces
            hexStr.erase(remove_if(hexStr.begin(), hexStr.end(), isspace), hexStr.end());
            
            if (!hexStr.empty()) {
                int col = (int)strtol(hexStr.c_str(), NULL, 16);
                currentLyricsColor = C2D_Color32((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, 255);
            }
        }

        std::string wrappedLyrics = wrapString(currentLyrics, currentLyricsSize, 300.0f);

        if (!wrappedLyrics.empty()) {
            if (lyricsTextBuf) C2D_TextBufClear(lyricsTextBuf);
            C2D_TextFontParse(&lyricsTextObj, vcrFont, lyricsTextBuf, wrappedLyrics.c_str());
            C2D_TextOptimize(&lyricsTextObj);
        }
    }
    else if (event.name == "Set GF Speed") {
        if (gf && !event.value1.empty()) {
            gf->danceEveryNumBeats = std::atoi(event.value1.c_str());
        }
    }
    else if (event.name == "Add Camera Zoom") {
        if (!event.value1.empty()) camZoom += std::atof(event.value1.c_str());
        if (!event.value2.empty()) hudZoom += std::atof(event.value2.c_str());
    }
    else if (event.name == "Play Animation") {
        Character* c = nullptr;
        if (event.value2 == "bf" || event.value2 == "1" || event.value2 == "boyfriend") c = bf;
        else if (event.value2 == "dad" || event.value2 == "0" || event.value2 == "") c = dad;
        else if (event.value2 == "gf" || event.value2 == "2") c = gf;
        
        if (c) {
            c->playAnim(event.value1, true);
        }
    }
    else if (event.name == "Screen Shake") {
        auto parseShake = [](const std::string& str, float& dur, float& inten) {
            size_t comma = str.find(',');
            if (comma != std::string::npos) {
                dur = std::atof(str.substr(0, comma).c_str());
                inten = std::atof(str.substr(comma + 1).c_str());
            }
        };
        parseShake(event.value1, camShakeTimer, camShakeIntensity);
        parseShake(event.value2, hudShakeTimer, hudShakeIntensity);
    }
    else if (event.name == "Change Scroll Speed") {
        if (!event.value1.empty()) SongParser::songSpeed = std::atof(event.value1.c_str());
        LuaManager::get().callFunction("onEvent", { event.name, event.value1, event.value2 });
    } else if (event.name == "Change Character") {

        std::string charType = event.value1;
        std::string newCharName = event.value2;
        std::transform(charType.begin(), charType.end(), charType.begin(), ::tolower);

        Character* newChar = AsyncAssetManager::get().getCharacter(newCharName);
        if (newChar) {
            // Already in cache — apply immediately (the common case with 10s lookahead)
            applyCharacterSwap(charType, newCharName, newChar);
        } else {
            // Not ready yet — request it and defer; applyCharacterSwap fires from update()
            // once the background thread finishes. No stutter, no SD contention.
            AsyncAssetManager::get().requestCharacter(newCharName);
            PendingCharSwap swap;
            swap.charType = charType;
            swap.charName = newCharName;
            pendingSwaps.push_back(swap);
        }

    } else if (event.name == "Camera Follow Pos") {
        std::string xStr = event.value1;
        std::string yStr = event.value2;
        xStr.erase(remove_if(xStr.begin(), xStr.end(), isspace), xStr.end());
        yStr.erase(remove_if(yStr.begin(), yStr.end(), isspace), yStr.end());
        
        if (xStr.empty() && yStr.empty()) {
            camFollowLocked = false;
            // Immediately focus camera on the active character
            bool isPlayer = false;
            if (curSection >= 0 && curSection < (int)songData.sections.size()) {
                isPlayer = songData.sections[curSection].mustHitSection;
            }
            focusCamera(isPlayer);
        } else {
            camFollowLocked = true;
            if (!xStr.empty()) lockedCamX = std::atof(xStr.c_str());
            if (!yStr.empty()) lockedCamY = std::atof(yStr.c_str());
            camFollowX = lockedCamX;
            camFollowY = lockedCamY;
        }
    }

    LuaManager::get().callFunction("onEvent", {event.name, event.value1, event.value2});
}

std::string PlayState::wrapString(const std::string& text, float scale, float maxWidth) {
    if (text.empty()) return "";
    
    std::string result = "";
    std::string line = "";
    std::string word = "";
    std::stringstream ss(text);
    
    // We'll use a temporary text object to measure widths
    C2D_TextBuf tempBuf = C2D_TextBufNew(text.length() + 32);
    
    while (ss >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        
        C2D_Text tempText;
        C2D_TextFontParse(&tempText, vcrFont, tempBuf, testLine.c_str());
        float w = tempText.width * scale;
        
        if (w > maxWidth && !line.empty()) {
            result += line + "\n";
            line = word;
        } else {
            line = testLine;
        }
        C2D_TextBufClear(tempBuf);
    }
    result += line;
    
    C2D_TextBufDelete(tempBuf);
    return result;
}

void PlayState::tickCountdown() {
    countdownTick++;
    AudioEngine::playCountdownSound(countdownTick - 1);
    
    if (countdownTick == 2) {
        currentCountdownFrame = "ready";
        countdownActive = true;
        countdownAlpha = 1.0f;
        countdownScale = 1.0f;
        countdownYOffset = 0.0f;
        countdownTimer = 0.0f;
    } else if (countdownTick == 3) {
        currentCountdownFrame = "set";
        countdownActive = true;
        countdownAlpha = 1.0f;
        countdownScale = 1.0f;
        countdownYOffset = 0.0f;
        countdownTimer = 0.0f;
    } else if (countdownTick == 4) {
        currentCountdownFrame = "go";
        countdownActive = true;
        countdownAlpha = 1.0f;
        countdownScale = 1.0f;
        countdownYOffset = 0.0f;
        countdownTimer = 0.0f;
    }
}

void PlayState::loadHealthIcon(HealthIconData& icon, const std::string& name) {
    bool isPixel = (name.find("-pixel") != std::string::npos);
    if (!isPixel) {
        std::string resolvedPath = Paths::healthIcon(name);
        if (resolvedPath.find("-pixel") != std::string::npos) {
            isPixel = true;
        }
    }

    if (&icon == &iconBf) {
        iconP1Antialiasing = !isPixel;
    } else if (&icon == &iconDad) {
        iconP2Antialiasing = !isPixel;
    }

    if (icon.loaded && icon.iconName == name) return;
    if (healthIconCache.count(name) > 0 && healthIconCache[name].loaded) {
        icon = healthIconCache[name];
        return;
    }
    
    // Clear any stale pointers from previous loads to prevent duplicate references and double-free crashes
    icon.sheet = nullptr;
    icon.vramData = nullptr;
    memset(&icon.tex, 0, sizeof(C3D_Tex));
    icon.loaded = false;
    icon.iconName = name;

    std::string path = Paths::healthIcon(name);
    bool isRawtex = (path.find(".rawtex") != std::string::npos);

    ImageData* preloaded = AsyncAssetManager::get().consumeHealthIcon(name);

    if (isRawtex) {
        void* vramBuf = nullptr;
        uint16_t w = 0, h = 0, ow = 0, oh = 0;

        if (preloaded) {
            vramBuf = preloaded->fileBuffer; // We stored the vram alloc here in AsyncAssetManager hack
            w = preloaded->rawWidth;
            h = preloaded->rawHeight;
            ow = preloaded->origWidth;
            oh = preloaded->origHeight;
            delete preloaded;
        } else {
            FILE* f = fopen(path.c_str(), "rb");
            if (!f) {
                printf("\x1b[14;1HWARN: Icon file not found: %s\x1b[K\n", name.c_str());
                return;
            }
            RawTexHeader header;
            if (fread(&header, sizeof(RawTexHeader), 1, f) != 1 || strncmp(header.magic, "RWTX", 4) != 0) {
                fclose(f);
                printf("\x1b[14;1HWARN: Invalid rawtex icon: %s\x1b[K\n", name.c_str());
                return;
            }

            size_t dataSize = (size_t)header.width * header.height * 4;
            vramBuf = linearAlloc(dataSize);
            if (!vramBuf) {
                fclose(f);
                printf("\x1b[14;1HWARN: Linear RAM Full for Icon: %s\x1b[K\n", name.c_str());
                return;
            }

            fread(vramBuf, dataSize, 1, f);
            fclose(f);
            
            w = header.width;
            h = header.height;
            ow = header.origW;
            oh = header.origH;
        }

        if (!C3D_TexInit(&icon.tex, w, h, GPU_RGBA8)) {
            if (addrIsVRAM(vramBuf)) vramFree(vramBuf);
            else linearFree(vramBuf);
            printf("\x1b[14;1HERROR: C3D_TexInit failed for Icon: %s\x1b[K\n", name.c_str());
            return;
        }

        // C3D_TexInit allocates a default linear buffer we don't need, free it to avoid leaks!
        if (icon.tex.data) {
            linearFree(icon.tex.data);
        }

        icon.tex.data = vramBuf;
        icon.vramData = vramBuf;

        float u0 = 0.0f;
        float u1 = (float)ow / w;
        float v0 = 1.0f;
        float v1 = 1.0f - ((float)oh / h);

        float halfW = (float)ow * 0.5f;
        float fullH = (float)oh;

        float du = u1 - u0;

        icon.normalSub.width  = (u16)halfW;
        icon.normalSub.height = (u16)fullH;
        icon.normalSub.left   = u0;
        icon.normalSub.right  = u0 + du * 0.5f;
        icon.normalSub.top    = v0;
        icon.normalSub.bottom = v1;

        icon.losingSub.width  = (u16)halfW;
        icon.losingSub.height = (u16)fullH;
        icon.losingSub.left   = u0 + du * 0.5f;
        icon.losingSub.right  = u1;
        icon.losingSub.top    = v0;
        icon.losingSub.bottom = v1;

        icon.loaded = true;
        GPU_TEXTURE_FILTER_PARAM filter = isPixel ? GPU_NEAREST : (ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        C3D_TexSetFilter(&icon.tex, filter, filter);
        healthIconCache[name] = icon;
    } else {
        C2D_SpriteSheet s = nullptr;
        if (preloaded && preloaded->sheet) {
            s = preloaded->sheet;
            preloaded->sheet = nullptr; // Take ownership
            delete preloaded;
        } else {
            s = C2D_SpriteSheetLoad(path.c_str());
        }
        if (!s) {
            printf("\x1b[14;1HWARN: Icon not found: %s\x1b[K\n", name.c_str());
            return;
        }
        icon.sheet = s;
        C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
        if (img.subtex == nullptr) {
            return;
        }
        icon.sheet = s;
        icon.tex   = *img.tex;

        float u0 = img.subtex->left,  u1 = img.subtex->right;
        float v0 = img.subtex->top,   v1 = img.subtex->bottom;
        float fullW = (float)img.subtex->width;
        float halfW = fullW * 0.5f;
        float fullH = (float)img.subtex->height;

        float du = u1 - u0;

        icon.normalSub.width  = (u16)halfW;
        icon.normalSub.height = (u16)fullH;
        icon.normalSub.left   = u0;
        icon.normalSub.right  = u0 + du * 0.5f;
        icon.normalSub.top    = v0;
        icon.normalSub.bottom = v1;

        icon.losingSub.width  = (u16)halfW;
        icon.losingSub.height = (u16)fullH;
        icon.losingSub.left   = u0 + du * 0.5f;
        icon.losingSub.right  = u1;
        icon.losingSub.top    = v0;
        icon.losingSub.bottom = v1;

        icon.loaded = true;
        GPU_TEXTURE_FILTER_PARAM filter = isPixel ? GPU_NEAREST : (ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
        C3D_TexSetFilter(&icon.tex, filter, filter);
        healthIconCache[name] = icon;
    }
}

void PlayState::startVideo(const std::string& name, bool inFrontOfHUD, bool loop) {
    stopVideo();
    inGameVideo = new InGameVideoPlayer(name, inFrontOfHUD, loop);
}

void PlayState::stopVideo() {
    if (inGameVideo) {
        delete inGameVideo;
        inGameVideo = nullptr;
    }
}
