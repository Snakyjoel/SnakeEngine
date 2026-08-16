#include "FreeplayState.hpp"
#include "PlayState.hpp"
#include "MainMenuState.hpp"
#include "VideoState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include "Highscores.hpp"
#include "../objects/Alphabet.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <unordered_map>

static std::string lastDifficultyName = "Normal";
static std::unordered_map<std::string, bool> isCharacterIconMap;

static void enforceLRUCache(std::unordered_map<std::string, SpriteCacheEntry>& cache, size_t maxSize) {
    if (cache.size() > maxSize) {
        auto oldest = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        if (oldest->second.sheet) {
            C2D_SpriteSheetFree(oldest->second.sheet);
        }
        cache.erase(oldest);
    }
}

static std::vector<std::string> getAllUniqueDifficulties() {
    std::vector<std::string> diffs;
    if (WeekData::weeksList.empty()) return {"Easy", "Normal", "Hard"};
    for (const auto& weekName : WeekData::weeksList) {
        auto it = WeekData::weeksLoaded.find(weekName);
        if (it == WeekData::weeksLoaded.end()) continue;
        const WeekData& data = it->second;
        if (data.difficulties.empty()) {
            if (std::find(diffs.begin(), diffs.end(), "Easy") == diffs.end()) diffs.push_back("Easy");
            if (std::find(diffs.begin(), diffs.end(), "Normal") == diffs.end()) diffs.push_back("Normal");
            if (std::find(diffs.begin(), diffs.end(), "Hard") == diffs.end()) diffs.push_back("Hard");
        } else {
            std::stringstream ss(data.difficulties);
            std::string d;
            while (std::getline(ss, d, ',')) {
                size_t first = d.find_first_not_of(' ');
                if (first == std::string::npos) continue;
                size_t last = d.find_last_not_of(' ');
                std::string diff = d.substr(first, last - first + 1);
                if (std::find(diffs.begin(), diffs.end(), diff) == diffs.end()) {
                    diffs.push_back(diff);
                }
            }
        }
    }
    if (diffs.empty()) diffs = {"Easy", "Normal", "Hard"};
    return diffs;
}

int FreeplayState::iconEggBest = 0;
int FreeplayState::savedDifficulty = 1;
std::string FreeplayState::savedSongName = "";
std::string FreeplayState::savedCategory = "all";

static void drawRotatedRect(float cx, float cy, float w, float h, float angleRad, u32 color, float depth) {
    float c = std::cos(angleRad), s = std::sin(angleRad);
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

void FreeplayState::init() {
    ModHandler::get().currentModFolder = "";
    
    iconEggTime = -1.0f;
    iconEggY = 0.0f;
    iconRotation = 0.0f;
    iconEggActive = false;
    iconEggXOffset = 0.0f;
    iconEggYOffset = 0.0f;
    iconEggVelY = 0.0f;
    iconEggVelX = 0.0f;
    iconEggScaleX = 1.0f;
    iconEggScaleY = 1.0f;
    iconEggRotVel = 0.0f;
    iconEggCombo = 0;
    iconEggTextBounce = 0.0f;
    iconEggLossTimer = 0.0f;
    iconEggLastCombo = 0;
    
    FILE* f = fopen("sdmc:/SnakeEngine/juggles.txt", "r");
    if (f) {
        fscanf(f, "%d", &iconEggBest);
        fclose(f);
    }
    
    MusicPlayer::play("romfs:/preload/music/freeplayAndCredits.ogg", 0.7f);

    VCRFontFix();

    WeekData::reloadWeekFiles(true);

    allSongs.clear();
    for (const auto& weekName : WeekData::weeksList) {
        if (WeekData::weeksLoaded.find(weekName) != WeekData::weeksLoaded.end()) {
            WeekData& data = WeekData::weeksLoaded[weekName];
            if (!data.hideFreeplay) {
                for (const auto& song : data.songs) {
                    allSongs.push_back({song.name, weekName, song});
                }
            }
        }
    }

    loadFavorites();
    rebuildCategories();
    
    curCategoryIdx = 0;
    for (int i = 0; i < (int)categories.size(); i++) {
        if (categories[i] == savedCategory) {
            curCategoryIdx = i;
            break;
        }
    }
    
    applyCategoryFilter(false);

    if (!savedSongName.empty()) {
        for (int i = 0; i < (int)songs.size(); i++) {
            if (songs[i].name == savedSongName) {
                curSelected = i;
                lerpSelected = (float)curSelected;
                break;
            }
        }
    }
    curDifficulty = savedDifficulty;

    if (!songs.empty()) {
        targetColor[0] = songs[curSelected].info.color[0];
        targetColor[1] = songs[curSelected].info.color[1];
        targetColor[2] = songs[curSelected].info.color[2];
        curColor[0] = 253.0f;
        curColor[1] = 232.0f;
        curColor[2] = 113.0f;
        updateDifficulties();
    }
    categoryBounceY = 0.0f;

    // Load letterStuff sprite sheet and XML
    std::string lsImgPath = Paths::image("freeplay/letterStuff");
    letterStuffSheet = C2D_SpriteSheetLoad(lsImgPath.c_str());
    letterStuffFrames.clear();
    if (letterStuffSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(letterStuffSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::string xmlPath = Paths::xml("freeplay/letterStuff");
        SparrowParser::parseXml(xmlPath, letterStuffFrames);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : letterStuffFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top = mainImg.subtex->top + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);
        }
    }

    getBfBackgroundImage();
    getCapsuleImage();

    scrollingText = "";
    lastScrolledSongIndex = -1;
    scrollTime = 0.0f;
    scrollOffset = 0.0f;
    scrollState = 0;
    scrollStateTime = 0.0f;
    iconBounceY = 0.0f;
    iconBounceVelocity = 0.0f;
    albumRotation = -5.0f;
    albumTextFlashTime = 0.0f;
    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        menuBgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
    }
    
    std::string arrowPath = Paths::image("freeplay/arrow");
    if (Paths::fileExists(arrowPath)) {
        arrowSheet = C2D_SpriteSheetLoad(arrowPath.c_str());
        if (arrowSheet) {
            C2D_Image img = C2D_SpriteSheetGetImage(arrowSheet, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }

    std::string hsPath = Paths::image("freeplay/highscore");
    highscoreSheet = C2D_SpriteSheetLoad(hsPath.c_str());
    highscoreFrames.clear();
    if (highscoreSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(highscoreSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::string xmlPath = Paths::xml("freeplay/highscore");
        SparrowParser::parseXml(xmlPath, highscoreFrames);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : highscoreFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top = mainImg.subtex->top + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);
        }
    }

    std::string numPath = Paths::image("freeplay/digital_numbers");
    numbersSheet = C2D_SpriteSheetLoad(numPath.c_str());
    for (int i = 0; i < 10; i++) numberFrames[i].clear();

    if (numbersSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(numbersSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::vector<Frame> tempFrames;
        std::string xmlPath = Paths::xml("freeplay/digital_numbers");
        SparrowParser::parseXml(xmlPath, tempFrames);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : tempFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left   = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top    = mainImg.subtex->top  + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right  = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top  + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);

            std::string nl = f.name;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            if      (nl.find("zero")  != std::string::npos) numberFrames[0].push_back(f);
            else if (nl.find("one")   != std::string::npos) numberFrames[1].push_back(f);
            else if (nl.find("two")   != std::string::npos) numberFrames[2].push_back(f);
            else if (nl.find("three") != std::string::npos) numberFrames[3].push_back(f);
            else if (nl.find("four")  != std::string::npos) numberFrames[4].push_back(f);
            else if (nl.find("five")  != std::string::npos) numberFrames[5].push_back(f);
            else if (nl.find("six")   != std::string::npos) numberFrames[6].push_back(f);
            else if (nl.find("seven") != std::string::npos) numberFrames[7].push_back(f);
            else if (nl.find("eight") != std::string::npos) numberFrames[8].push_back(f);
            else if (nl.find("nine")  != std::string::npos) numberFrames[9].push_back(f);
        }
    }

    std::string clPath = Paths::image("freeplay/cleared");
    clearedSheet = C2D_SpriteSheetLoad(clPath.c_str());
    for (int i = 0; i < 10; i++) clearedNumberFrames[i].clear();
    clearedBoxFrame.tex = nullptr;
    
    if (clearedSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(clearedSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::vector<Frame> tempFrames;
        std::string xmlPath = Paths::xml("freeplay/cleared");
        SparrowParser::parseXml(xmlPath, tempFrames);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : tempFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top = mainImg.subtex->top + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);

            if (f.name.find("clearBox") != std::string::npos) {
                clearedBoxFrame = f;
            } else {
                std::string nameLower = f.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                for (int i = 0; i < 10; i++) {
                    std::string numPrefix = std::to_string(i) + "0000";
                    if (nameLower.find(numPrefix) != std::string::npos) {
                        clearedNumberFrames[i].push_back(f);
                        break;
                    }
                }
            }
        }
    }

    bfdjAnimate.loadSheet("preload/images/freeplay/bf/bfdj");
    bfdjAnimate.addAnim("Boyfriend DJ", "Boyfriend DJ", 24.0f, true);
    bfdjAnimate.play("Boyfriend DJ");

    highscoreAnimTime = 0.0f;
    numbersAnimTime = 0.0f;
    if (!songs.empty()) {
        std::string diff = curWeekDiffs[curDifficulty];
        std::string suffix = "";
        if (diff == "Easy") suffix = "easy";
        else if (diff == "Hard") suffix = "hard";
        else if (diff != "Normal") {
            suffix = diff;
            std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
        }
        targetScore = Highscores::getScore(songs[curSelected].name, suffix);
        lerpScore = (float)targetScore;
        targetAccuracy = Highscores::getAccuracy(songs[curSelected].name, suffix);
        lerpAccuracy = targetAccuracy;
    } else {
        targetScore = 0;
        lerpScore = 0.0f;
        targetAccuracy = 0.0f;
        lerpAccuracy = 0.0f;
    }
}

void FreeplayState::update(float dt) {
    cacheFrameCount++;
    loadingAngle += dt * 3.14159f * 2.0f;
    
    static int prevSelectedForCache = -1;
    if (prevSelectedForCache != curSelected) {
        timeSinceSelectionChange = 0.0f;
        prevSelectedForCache = curSelected;
    } else {
        timeSinceSelectionChange += dt;
    }

    if (isExiting) {
        exitTimer += dt;
        targetColor[0] = 253.0f;
        targetColor[1] = 232.0f;
        targetColor[2] = 113.0f;
        for(int i=0; i<3; i++) {
            curColor[i] += (targetColor[i] - curColor[i]) * (1.0f - exp2f(-12.0f * dt));
        }
        if (exitTimer >= 0.35f) {
            MusicBeatState::skipTransition = true;
            MainMenuState::comingFromFreeplay = true;
            switchState(new MainMenuState());
        }
        return;
    }

    if (introTimer < 1.0f) {
        introTimer += dt * 2.5f;
        if (introTimer > 1.0f) introTimer = 1.0f;
    }

    u32 kDown = hidKeysDown();

    int prevSelectedForEgg = curSelected;

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        isExiting = true;
        exitTimer = 0.0f;
        return;
    }

    if (keyJustPressed(KEY_L)) {
        int startIdx = curCategoryIdx;
        do {
            curCategoryIdx--;
            if (curCategoryIdx < 0) curCategoryIdx = (int)categories.size() - 1;
            if (!hasSongsInCategory(categories[curCategoryIdx])) continue;
            break;
        } while (curCategoryIdx != startIdx);

        if (curCategoryIdx != startIdx) {
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            categoryBounceY = -15.0f;
            applyCategoryFilter();
        }
    }
    if (keyJustPressed(KEY_R)) {
        int startIdx = curCategoryIdx;
        do {
            curCategoryIdx++;
            if (curCategoryIdx >= (int)categories.size()) curCategoryIdx = 0;
            if (!hasSongsInCategory(categories[curCategoryIdx])) continue;
            break;
        } while (curCategoryIdx != startIdx);

        if (curCategoryIdx != startIdx) {
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            categoryBounceY = -15.0f;
            applyCategoryFilter();
        }
    }

    if (keyJustPressed(KEY_Y) && !songs.empty()) {
        std::string songName = songs[curSelected].name;
        if (favorites.count(songName) > 0) {
            favorites.erase(songName);
            heartBounceAnim[songName] = -0.01f;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        } else {
            favorites.insert(songName);
            heartBounceAnim[songName] = 0.01f;
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        }
        saveFavorites();

        rebuildCategories();
        if (categories[curCategoryIdx] == "fav" && favorites.empty()) {
            curCategoryIdx = 0;
        }
        applyCategoryFilter(true);
    }

    if (!songs.empty()) {
        bool changed = false;
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            curSelected--;
            if (curSelected < 0) curSelected = (int)songs.size() - 1;
            changed = true;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            curSelected++;
            if (curSelected >= (int)songs.size()) curSelected = 0;
            changed = true;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (changed) {
            targetColor[0] = songs[curSelected].info.color[0];
            targetColor[1] = songs[curSelected].info.color[1];
            targetColor[2] = songs[curSelected].info.color[2];
            // Reset scrolling when song changes
            lastScrolledSongIndex = curSelected;
            scrollingText = songs[curSelected].name;
            scrollTime = 0.0f;
            scrollOffset = 0.0f;
            scrollState = 0;
            scrollStateTime = 0.0f;
            iconBounceY = -15.0f;
            albumRotation = 0.0f;
            albumTextFlashTime = 0.1f;
            updateDifficulties();
        }
        
        if (curSelected == lastScrolledSongIndex && !scrollingText.empty()) {
            C2D_Text gText;
            C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, scrollingText.c_str());
            float textW, textH;
            C2D_TextGetDimensions(&gText, 0.45f, 0.45f, &textW, &textH);
            
            float maxScroll = textW - CAPSULE_TEXT_WIDTH;
            if (maxScroll > 0.0f) {
                scrollStateTime += dt;
                if (scrollState == 0) { // STATE_WAIT_RIGHT (0.6s)
                    scrollOffset = 0.0f;
                    if (scrollStateTime >= 0.6f) {
                        scrollState = 1;
                        scrollStateTime = 0.0f;
                    }
                }
                else if (scrollState == 1) { // STATE_MOVE_RIGHT (2.0s tween)
                    float t = scrollStateTime / 2.0f;
                    if (t >= 1.0f) {
                        scrollOffset = maxScroll;
                        scrollState = 2;
                        scrollStateTime = 0.0f;
                    } else {
                        float ease = -0.5f * (std::cos(3.14159265f * t) - 1.0f);
                        scrollOffset = ease * maxScroll;
                    }
                }
                else if (scrollState == 2) { // STATE_WAIT_LEFT (0.3s)
                    scrollOffset = maxScroll;
                    if (scrollStateTime >= 0.3f) {
                        scrollState = 3;
                        scrollStateTime = 0.0f;
                    }
                }
                else if (scrollState == 3) { // STATE_MOVE_LEFT (2.0s tween)
                    float t = scrollStateTime / 2.0f;
                    if (t >= 1.0f) {
                        scrollOffset = 0.0f;
                        scrollState = 0;
                        scrollStateTime = 0.0f;
                    } else {
                        float ease = -0.5f * (std::cos(3.14159265f * t) - 1.0f);
                        scrollOffset = (1.0f - ease) * maxScroll;
                    }
                }
            } else {
                scrollOffset = 0.0f;
            }
        }

        if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
            if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
                std::vector<std::string> diffs = getAllUniqueDifficulties();
                auto it = std::find(diffs.begin(), diffs.end(), lastDifficultyName);
                int idx = (it != diffs.end()) ? (int)(it - diffs.begin()) : -1;
                if (idx < 0) idx = 0;
                idx = (idx - 1 + (int)diffs.size()) % diffs.size();
                lastDifficultyName = diffs[idx];
            }
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            iconBounceY = -15.0f;
            albumRotation = 0.0f;
            albumTextFlashTime = 0.1f;
            leftArrowVisibleTime = 0.1f;
            diffOffsetX = 150.0f;
            applyCategoryFilter(true);
        }
        if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
                std::vector<std::string> diffs = getAllUniqueDifficulties();
                auto it = std::find(diffs.begin(), diffs.end(), lastDifficultyName);
                int idx = (it != diffs.end()) ? (int)(it - diffs.begin()) : -1;
                if (idx < 0) idx = 0;
                idx = (idx + 1) % diffs.size();
                lastDifficultyName = diffs[idx];
            }
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            iconBounceY = -15.0f;
            albumRotation = 0.0f;
            albumTextFlashTime = 0.1f;
            rightArrowVisibleTime = 0.1f;
            diffOffsetX = -150.0f;
            applyCategoryFilter(true);
        }

        if (keyJustPressed(KEY_A | KEY_START)) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            FreeplaySong& fs = songs[curSelected];
            std::string lname = fs.name;
            std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
            
            if (WeekData::weeksLoaded.count(fs.week)) {
                ModHandler::get().currentModFolder = WeekData::weeksLoaded[fs.week].modFolder;
            }
            MusicPlayer::stop();
            
            std::string diff = curWeekDiffs[curDifficulty];
            std::string suffix = "";
            if (diff == "Easy") suffix = "easy";
            else if (diff == "Hard") suffix = "hard";
            else if (diff != "Normal") {
                suffix = diff;
                std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
            }

            if (!fs.info.introVideo.empty()) {
                switchState(new VideoState(fs.info.introVideo, new PlayState(fs.name, suffix)));
            } else {
                switchState(new PlayState(fs.name, suffix));
            }
        }
        
        u32 kHeld = hidKeysHeld();
        u32 kUp = hidKeysUp();
        if (kDown & KEY_TOUCH) {
            touchPosition touch; hidTouchRead(&touch);
            
            bool touchedIcon = false;
            if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
                FreeplaySong& fs = songs[curSelected];
                if (!fs.info.icon.empty()) {
                    C2D_Image icon = getIconImage(fs.info.icon);
                    if (icon.tex) {
                        static Tex3DS_SubTexture defaultSubtex;
                        if (icon.subtex == nullptr) {
                            defaultSubtex.width = icon.tex ? icon.tex->width : 0;
                            defaultSubtex.height = icon.tex ? icon.tex->height : 0;
                            defaultSubtex.left = 0.0f;
                            defaultSubtex.top = 0.0f;
                            defaultSubtex.right = 1.0f;
                            defaultSubtex.bottom = 1.0f;
                            icon.subtex = &defaultSubtex;
                        }
                        float iconScale = 1.5f;
                        if (isCharacterIconMap[fs.info.icon] && icon.subtex) {
                            iconScale = 70.0f / (float)icon.subtex->width;
                        }
                        float iconW = icon.subtex->width * iconScale;
                        float iconH = icon.subtex->height * iconScale;
                        float defaultX = 320.0f - iconW - 15.0f;
                        float defaultY = 240.0f - iconH - 15.0f;
                        
                        // Actual position of the icon in the air
                        float iconX = defaultX + iconEggXOffset;
                        float iconY = defaultY + iconEggYOffset;
                        
                        if (touch.px >= iconX && touch.px <= iconX + iconW &&
                            touch.py >= iconY && touch.py <= iconY + iconH) {
                            AudioEngine::playSound("romfs:/preload/sounds/plush.ogg", 0.7f);
                            
                            iconEggCombo++;
                            if (iconEggCombo > iconEggBest) {
                                iconEggBest = iconEggCombo;
                                FILE* f = fopen("sdmc:/SnakeEngine/juggles.txt", "w");
                                if (f) {
                                    fprintf(f, "%d", iconEggBest);
                                    fclose(f);
                                }
                            }

                            iconEggTextBounce = -10.0f;
                            iconEggActive = true;
                            iconEggVelY = -340.0f;
                            iconEggRotVel = 360.0f * (3.14159265f / 180.0f) / 0.6f;
                            
                            if (iconEggCombo >= 5) {
                                float speed = 50.0f + (float)iconEggCombo * 10.0f;
                                iconEggVelX = (rand() % 2 == 0 ? 1.0f : -1.0f) * speed;
                            } else {
                                iconEggVelX = 0.0f;
                            }

                            iconEggScaleY = 0.65f;
                            iconEggScaleX = 1.35f;
                            iconEggTime = 0.0f;
                            touchedIcon = true;
                        }
                    }
                }
            }
            
            if (!touchedIcon) {
                touchActive = true;
                touchStartY = touch.py;
                lastTouchY = touch.py;
                touchStartLerp = lerpSelected;
                isDragging = false;
                scrollVelocity = 0.0f;
                touchHoldTime = 0.0f;
            }
        } else if (kHeld & KEY_TOUCH && touchActive) {
            touchPosition touch; hidTouchRead(&touch);
            touchHoldTime += dt;
            if (std::abs(touch.py - touchStartY) > 5) {
                isDragging = true;
            }
            if (isDragging) {
                float dist = touch.py - lastTouchY;
                scrollVelocity = -dist / 55.0f;
                lerpSelected -= dist / 55.0f;
            }
            lastTouchY = touch.py;
        } else if (kUp & KEY_TOUCH && touchActive) {
            touchActive = false;
            if (!isDragging && touchHoldTime < 0.5f) {
                touchPosition touch; hidTouchRead(&touch);
                float centerY = 130.0f;
                int tappedIndex = -1;
                for (size_t i = 0; i < songs.size(); i++) {
                    float itemY = centerY + ((int)i - lerpSelected) * 55;
                    if (itemY >= 50 && itemY <= 230) {
                        if (touch.py >= itemY - 20 && touch.py <= itemY + 20) {
                            tappedIndex = i;
                            break;
                        }
                    }
                }
                if (tappedIndex != -1) {
                    if (curSelected == tappedIndex) {
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                        FreeplaySong& fs = songs[curSelected];
                        if (WeekData::weeksLoaded.count(fs.week)) {
                            ModHandler::get().currentModFolder = WeekData::weeksLoaded[fs.week].modFolder;
                        }
                        MusicPlayer::stop();
                        
                        std::string diff = curWeekDiffs[curDifficulty];
                        std::string suffix = "";
                        if (diff == "Easy") suffix = "easy";
                        else if (diff == "Hard") suffix = "hard";
                        else if (diff != "Normal") {
                            suffix = diff;
                            std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
                        }
                        switchState(new PlayState(fs.name, suffix));
                        return;
                    } else {
                        curSelected = tappedIndex;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                        targetColor[0] = songs[curSelected].info.color[0];
                        targetColor[1] = songs[curSelected].info.color[1];
                        targetColor[2] = songs[curSelected].info.color[2];
                        lastScrolledSongIndex = curSelected;
                        scrollingText = songs[curSelected].name;
                        scrollTime = 0.0f;
                        scrollOffset = 0.0f;
                        scrollState = 0;
                        scrollStateTime = 0.0f;
                        iconBounceY = -15.0f;
                        albumRotation = 0.0f;
                        albumTextFlashTime = 0.1f;
                        updateDifficulties();
                    }
                }
            }
        }
        
        if (!touchActive && std::abs(scrollVelocity) > 0.01f) {
            lerpSelected += scrollVelocity;
            scrollVelocity *= 0.9f;
            int prevSelected = curSelected;
            curSelected = std::max(0, std::min((int)std::round(lerpSelected), (int)songs.size() - 1));
            if (prevSelected != curSelected) {
                targetColor[0] = songs[curSelected].info.color[0];
                targetColor[1] = songs[curSelected].info.color[1];
                targetColor[2] = songs[curSelected].info.color[2];
                lastScrolledSongIndex = curSelected;
                scrollingText = songs[curSelected].name;
                scrollTime = 0.0f;
                scrollOffset = 0.0f;
                scrollState = 0;
                scrollStateTime = 0.0f;
                iconBounceY = -15.0f;
                albumRotation = 0.0f;
                albumTextFlashTime = 0.1f;
                updateDifficulties();
            }
        } else if (!touchActive && std::abs(scrollVelocity) <= 0.01f) {
            scrollVelocity = 0.0f;
        }
        
        // Ensure bounds
        if (lerpSelected < 0) lerpSelected = 0;
        if (lerpSelected > (int)songs.size() - 1) lerpSelected = (int)songs.size() - 1;
        if (curSelected < 0) curSelected = 0;
        if (curSelected > (int)songs.size() - 1) curSelected = (int)songs.size() - 1;
    }

    lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-12.0f * dt));

    for (int i = 0; i < 3; i++) {
        curColor[i] += (targetColor[i] - curColor[i]) * (1.0f - exp2f(-6.0f * dt));
    }

    highscoreAnimTime += dt;
    numbersAnimTime += dt;

    for (auto& pair : heartBounceAnim) {
        if (pair.second > 0.0f) {
            pair.second += dt;
            if (pair.second > 0.4f) pair.second = 0.0f;
        } else if (pair.second < 0.0f) {
            pair.second -= dt;
            if (pair.second < -0.4f) pair.second = 0.0f;
        }
    }

    if (!songs.empty() && curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) {
        std::string diff = curWeekDiffs[curDifficulty];
        std::string suffix = "";
        if (diff == "Easy") suffix = "easy";
        else if (diff == "Hard") suffix = "hard";
        else if (diff != "Normal") {
            suffix = diff;
            std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
        }
        targetScore = Highscores::getScore(songs[curSelected].name, suffix);
        targetAccuracy = Highscores::getAccuracy(songs[curSelected].name, suffix);
    } else {
        targetScore = 0;
        targetAccuracy = 0.0f;
    }

    if (std::abs(targetScore - lerpScore) < 0.5f) {
        lerpScore = (float)targetScore;
    } else {
        lerpScore += (targetScore - lerpScore) * (1.0f - exp2f(-12.0f * dt));
    }

    if (std::abs(targetAccuracy - lerpAccuracy) < 0.05f) {
        lerpAccuracy = targetAccuracy;
    } else {
        lerpAccuracy += (targetAccuracy - lerpAccuracy) * (1.0f - exp2f(-12.0f * dt));
    }

    iconBounceY += (0.0f - iconBounceY) * (1.0f - exp2f(-8.0f * dt));
    categoryBounceY += (0.0f - categoryBounceY) * (1.0f - exp2f(-8.0f * dt));

    if (iconEggActive) {
        float gravity = 800.0f;
        iconEggVelY += gravity * dt;
        
        iconEggXOffset += iconEggVelX * dt;
        iconEggYOffset += iconEggVelY * dt;
        iconRotation += iconEggRotVel * dt;
        
        // Squash and stretch calculations in the air
        iconEggScaleY = 1.0f - (iconEggVelY * 0.0009f);
        iconEggScaleX = 2.0f - iconEggScaleY;
        
        // Bounds limit for scale
        if (iconEggScaleY < 0.7f) iconEggScaleY = 0.7f;
        if (iconEggScaleY > 1.3f) iconEggScaleY = 1.3f;
        if (iconEggScaleX < 0.7f) iconEggScaleX = 0.7f;
        if (iconEggScaleX > 1.3f) iconEggScaleX = 1.3f;

        // Bounding box checks
        if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
            FreeplaySong& fs = songs[curSelected];
            if (!fs.info.icon.empty()) {
                C2D_Image icon = getIconImage(fs.info.icon);
                if (icon.tex) {
                    static Tex3DS_SubTexture defaultSubtex;
                    if (icon.subtex == nullptr) {
                        defaultSubtex.width = icon.tex ? icon.tex->width : 0;
                        defaultSubtex.height = icon.tex ? icon.tex->height : 0;
                        defaultSubtex.left = 0.0f;
                        defaultSubtex.top = 0.0f;
                        defaultSubtex.right = 1.0f;
                        defaultSubtex.bottom = 1.0f;
                        icon.subtex = &defaultSubtex;
                    }
                    float iconScale = 1.5f;
                    if (isCharacterIconMap[fs.info.icon] && icon.subtex) {
                        iconScale = 70.0f / (float)icon.subtex->width;
                    }
                    float iconW = icon.subtex->width * iconScale;
                    float iconH = icon.subtex->height * iconScale;
                    float defaultX = 320.0f - iconW - 15.0f;
                    float iconX = defaultX + iconEggXOffset;
                    
                    // Bounce off sides
                    if (iconX < 10.0f) {
                        iconEggXOffset = 10.0f - defaultX;
                        iconEggVelX = -iconEggVelX;
                        iconEggRotVel = -iconEggRotVel;
                        // squash slightly on impact
                        iconEggScaleX = 0.75f;
                        iconEggScaleY = 1.25f;
                    }
                    else if (iconX + iconW > 310.0f) {
                        iconEggXOffset = 310.0f - iconW - defaultX;
                        iconEggVelX = -iconEggVelX;
                        iconEggRotVel = -iconEggRotVel;
                        // squash slightly on impact
                        iconEggScaleX = 0.75f;
                        iconEggScaleY = 1.25f;
                    }
                }
            }
        }

        // Hit the ground
        if (iconEggYOffset >= 0.0f) {
            iconEggYOffset = 0.0f;
            iconEggVelY = 0.0f;
            iconEggVelX = 0.0f;
            iconRotation = 0.0f;
            iconEggRotVel = 0.0f;
            
            // Trigger loss state, flashing text starts
            iconEggLossTimer = 1.0f; // For blinking and fade out!
            iconEggLastCombo = iconEggCombo; // Save combo to display
            
            iconEggCombo = 0;
            iconEggActive = false;
            iconEggTime = -1.0f;
            // squash upon landing
            iconEggScaleY = 0.6f;
            iconEggScaleX = 1.4f;
        }
    } else {
        // Recover scale
        iconEggScaleY += (1.0f - iconEggScaleY) * (1.0f - exp2f(-12.0f * dt));
        iconEggScaleX += (1.0f - iconEggScaleX) * (1.0f - exp2f(-12.0f * dt));
        
        // Slide X smoothly back to original X position when not active
        iconEggXOffset += (0.0f - iconEggXOffset) * (1.0f - exp2f(-8.0f * dt));
    }

    // Update text bounce
    iconEggTextBounce += (0.0f - iconEggTextBounce) * (1.0f - exp2f(-12.0f * dt));

    // Update loss timer
    if (iconEggLossTimer > 0.0f) {
        iconEggLossTimer -= dt;
        if (iconEggLossTimer < 0.0f) iconEggLossTimer = 0.0f;
    }

    // Reset easter egg if song selection changed
    if (curSelected != prevSelectedForEgg) {
        iconEggTime = -1.0f;
        iconEggY = 0.0f;
        iconRotation = 0.0f;
        iconEggActive = false;
        iconEggXOffset = 0.0f;
        iconEggYOffset = 0.0f;
        iconEggVelY = 0.0f;
        iconEggVelX = 0.0f;
        iconEggScaleX = 1.0f;
        iconEggScaleY = 1.0f;
        iconEggRotVel = 0.0f;
        iconEggCombo = 0;
    }

    // Update album rotation tilt transition (-5 degrees target)
    albumRotation += (-5.0f - albumRotation) * (1.0f - exp2f(-8.0f * dt));

    // Update album text flash timer
    if (albumTextFlashTime > 0.0f) {
        albumTextFlashTime -= dt;
        if (albumTextFlashTime < 0.0f) albumTextFlashTime = 0.0f;
    }

    // Update arrow visibility timers
    if (leftArrowVisibleTime > 0.0f) {
        leftArrowVisibleTime -= dt;
        if (leftArrowVisibleTime < 0.0f) leftArrowVisibleTime = 0.0f;
    }
    if (rightArrowVisibleTime > 0.0f) {
        rightArrowVisibleTime -= dt;
        if (rightArrowVisibleTime < 0.0f) rightArrowVisibleTime = 0.0f;
    }

    bfdjAnimate.update(dt);
    textScrollTime += dt;

    // Decay the difficulty displacement offset back to zero
    diffOffsetX += (0.0f - diffOffsetX) * (1.0f - exp2f(-12.0f * dt));
}

void FreeplayState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();

    std::string currentDifficultyStr = "";
    if (!songs.empty() && curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) {
        std::string diff = curWeekDiffs[curDifficulty];
        if (diff == "Easy") currentDifficultyStr = "easy";
        else if (diff == "Hard") currentDifficultyStr = "hard";
        else if (diff != "Normal") {
            currentDifficultyStr = diff;
            std::transform(currentDifficultyStr.begin(), currentDifficultyStr.end(), currentDifficultyStr.begin(), ::tolower);
        }
    }

    Frame favFrame;
    bool hasFavFrame = false;
    for (const auto& f : letterStuffFrames) {
        if (f.name.find("fav") == 0) {
            favFrame = f;
            hasFavFrame = true;
            break;
        }
    }
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32((int)curColor[0], (int)curColor[1], (int)curColor[2], 255));

    float t = 1.0f - introTimer;
    float introEaseOut = 1.0f - (t * t * t);
    
    float topIntroY = -150.0f * (1.0f - introEaseOut);
    float topIntroX = 400.0f * (1.0f - introEaseOut);
    
    float botIntroLeftX = -200.0f * (1.0f - introEaseOut);
    float botIntroRightX = 400.0f * (1.0f - introEaseOut);
    float botIntroBottomY = 200.0f * (1.0f - introEaseOut);

    float exitAlpha = isExiting ? std::max(0.0f, 1.0f - (exitTimer / 0.35f)) : 1.0f;
    float renderAlpha = introTimer * exitAlpha;

    // Difficulty selector
    if (!songs.empty()) {
        float diffCenterX = 60.0f; // Center pivot for difficulty moved more to the left
        float diffY = 40.0f + topIntroY;
        std::string curDiffStr = curWeekDiffs[curDifficulty];
        std::string lowerDiff = curDiffStr;
        std::transform(lowerDiff.begin(), lowerDiff.end(), lowerDiff.begin(), ::tolower);

        C2D_Image dImg = getDifficultyImage(lowerDiff);

        float dW = 0, dH = 0;
        if (dImg.tex) {
            dW = dImg.subtex->width;
            dH = dImg.subtex->height;
            float scale = 0.6f;
            
            float dW_scaled = dW * scale;
            float dH_scaled = dH * scale;
            
            float diffDrawX = diffCenterX - (dW_scaled / 2.0f);
            
            C2D_ImageTint diffTint;
            C2D_AlphaImageTint(&diffTint, exitAlpha);

            // Draw difficulty sprite with displacement offset (behind bfFreeplayRBG, in front of scroll text)
            C2D_DrawImageAt(dImg, diffDrawX + diffOffsetX, diffY, 0.08f, &diffTint, scale, scale);

            if (arrowSheet) {
                C2D_Image arrowImg = C2D_SpriteSheetGetImage(arrowSheet, 0);
                if (arrowImg.tex) {
                    float aW = arrowImg.subtex->width * scale;
                    float aH = arrowImg.subtex->height * scale;
                    
                    float arrowSpacing = 8.0f;
                    float leftArrowX = diffDrawX - arrowSpacing - aW;
                    float rightArrowX = diffDrawX + dW_scaled + arrowSpacing - 10.0f; // Adjust right arrow slightly left to account for flipX offset
                    
                    // Draw left arrow if visibility timer is up (arrow points left naturally)
                    if (leftArrowVisibleTime <= 0.0f) {
                        C2D_DrawImageAt(arrowImg, leftArrowX, diffY + (dH_scaled - aH) / 2.0f, 0.5f, &diffTint, scale, scale);
                    }
                    
                    // Draw right arrow if visibility timer is up (arrow flipped horizontally)
                    if (rightArrowVisibleTime <= 0.0f) {
                        C2D_DrawImageAt(arrowImg, rightArrowX + aW, diffY + (dH_scaled - aH) / 2.0f, 0.5f, &diffTint, -scale, scale);
                    }
                }
            }
        } else {
            drawRotatedRect(diffCenterX, diffY, 20.0f, 20.0f, loadingAngle, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)), 0.5f);
        }
    }

    // Draw background sprite (right side, behind the song list)
    C2D_Image bgImg = getBfBackgroundImage();
    if (bgImg.tex) {
        float bgW = bgImg.subtex->width;
        float drawX = 400.0f - bgW;
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, renderAlpha);
        C2D_DrawImageAt(bgImg, drawX, 0, 0.1f, &tint);
    }

    // Black title bar
    C2D_DrawRectSolid(0, topIntroY, 0.9f, 400, 30, C2D_Color32(0, 0, 0, (u8)(exitAlpha * 255.0f)));
    AddTextDepth("FREEPLAY", 10, 5 + topIntroY, 0.7f, false, 0.0f, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)), 0.0f, 0.95f);

    // Draw custom OST text on the right side of the title bar
    std::string ostName = "OFFICIAL OST";
    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
        std::string weekName = songs[curSelected].week;
        if (WeekData::weeksLoaded.find(weekName) != WeekData::weeksLoaded.end()) {
            const WeekData& data = WeekData::weeksLoaded[weekName];
            if (!data.ost.empty()) {
                ostName = data.ost;
            }
        }
    }
    std::transform(ostName.begin(), ostName.end(), ostName.begin(), ::toupper);

    ClearTextBuf();
    C2D_Text ostTextObj;
    C2D_TextFontParse(&ostTextObj, vcrFont, vcrFontBuf, ostName.c_str());
    C2D_TextOptimize(&ostTextObj);
    float ostW, ostH;
    C2D_TextGetDimensions(&ostTextObj, 0.7f, 0.7f, &ostW, &ostH);
    float ostX = 390.0f - ostW; // 10px margin on the right
    float ostY = 5.0f + topIntroY;
    C2D_DrawText(&ostTextObj, C2D_WithColor, ostX, ostY, 0.95f, 0.7f, 0.7f, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)));
    C2D_Flush();

    // Draw category organizer in the title bar, centered between FREEPLAY and OST
    drawCategoryOrganizer(topIntroY, exitAlpha, ostX);

    // Draw scrolling background text
    auto drawScrollingText = [&](const std::string& text, float y, float scale, float speed, float dir, u32 color, u32 bgColor = 0) {
        ClearTextBuf();
        C2D_Text gText;
        C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, text.c_str());
        C2D_TextOptimize(&gText);
        float tw, th;
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
        
        float spacing = 35.0f; // Gap between text clones
        float scrollOffset = fmodf(textScrollTime * speed, tw + spacing);
        
        float startX = 0;
        if (dir < 0) startX = -scrollOffset; // scroll left
        else startX = scrollOffset - tw - spacing; // scroll right
        
        startX += botIntroLeftX; // slide in with DJ
        
        if (bgColor != 0) {
            float barY = y + 4.0f * scale;
            float barH = 26.0f * scale;
            C2D_DrawRectSolid(0, barY, 0.02f, 400, barH, bgColor);
        }
        
        AddTextDepth(text, startX, y, scale, false, 0.0f, color, 0.0f, 0.05f);
        AddTextDepth(text, startX + tw + spacing, y, scale, false, 0.0f, color, 0.0f, 0.05f);
        AddTextDepth(text, startX + (tw + spacing) * 2, y, scale, false, 0.0f, color, 0.0f, 0.05f);
    };

    float currTextY = 35.0f + topIntroY;
    u32 colorMoreWays = C2D_Color32(
        std::min(255, (int)curColor[0] + 0x30),
        std::min(255, (int)curColor[1] + 0x30),
        std::min(255, (int)curColor[2] + 0x30),
        (u8)(exitAlpha * 255.0f)
    );
    u32 colorBoyfriend = C2D_Color32(
        (int)(curColor[0] * 0.8f),
        (int)(curColor[1] * 0.8f),
        (int)(curColor[2] * 0.8f),
        (u8)(exitAlpha * 255.0f)
    );
    u32 colorProtect = C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f));
    u32 colorLastBg = C2D_Color32(
        (int)(curColor[0] * 0.9f),
        (int)(curColor[1] * 0.9f),
        (int)(curColor[2] * 0.9f),
        (u8)(exitAlpha * 255.0f)
    );
    u32 colorLastText = C2D_Color32(
        (int)(curColor[0] * 0.2f),
        (int)(curColor[1] * 0.2f),
        (int)(curColor[2] * 0.2f),
        (u8)(exitAlpha * 255.0f)
    );

    drawScrollingText("MORE WAYS THAN ONE HOT BLOODED IN ", currTextY, 0.6f, 120.0f, -1.0f, colorMoreWays);
    currTextY += 20.0f;
    drawScrollingText("BOYFRIEND ", currTextY, 1.0f, 70.0f, 1.0f, colorBoyfriend);
    currTextY += 35.0f;
    drawScrollingText("PROTECT YO NUTS ", currTextY, 0.6f, 30.0f, -1.0f, colorProtect);
    currTextY += 20.0f;
    drawScrollingText("BOYFRIEND ", currTextY, 1.0f, 70.0f, 1.0f, colorBoyfriend);
    currTextY += 30.0f;
    drawScrollingText("MORE WAYS THAN ONE HOT BLOODED IN ", currTextY, 0.6f, 120.0f, -1.0f, colorMoreWays);
    currTextY += 20.0f;
    drawScrollingText("BOYFRIEND ", currTextY, 1.0f, 70.0f, 1.0f, colorBoyfriend, colorLastBg);

    if (bfdjAnimate.curAnim == "Boyfriend DJ") {
        float bfScale = 0.85f;
        float bfX = 5.0f + botIntroLeftX;
        float bfY = 240.0f - bfdjAnimate.height() * bfScale + 5.0f;
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, exitAlpha);
        bfdjAnimate.draw(bfX, bfY, 0.3f, bfScale, bfScale, &tint);
    }

    // Draw cleared box and accuracy inside (top screen, upper right, below the black bar)
    if (clearedSheet && clearedBoxFrame.tex) {
        float scale = 0.65f;
        float cbX = 400.0f - (82.4f * scale) - 10.0f;
        float cbY = 32.0f + topIntroY;
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, exitAlpha);
        drawFrameAt(clearedBoxFrame, cbX, cbY, 0.5f, &tint, scale, scale);

        int accVal = (int)std::round(lerpAccuracy);
        if (accVal < 0) accVal = 0;
        if (accVal > 100) accVal = 100;
        std::string accStr = std::to_string(accVal);
        
        float currentX = cbX + (65.0f * scale);
        float currentY = cbY + (18.0f * scale);
        float padding = 1.0f * scale;

        for (int idx = (int)accStr.length() - 1; idx >= 0; idx--) {
            int digit = accStr[idx] - '0';
            if (digit >= 0 && digit <= 9 && !clearedNumberFrames[digit].empty()) {
                const Frame& f = clearedNumberFrames[digit][0];
                float digitW = (float)f.w * scale;
                currentX -= digitW;
                drawFrameAt(f, currentX, currentY, 0.6f, &tint, scale, scale);
                currentX -= padding;
            }
        }
    }

    // Song list (on top)
    float centerY = 130.0f;
    for (size_t i = 0; i < songs.size(); i++) {
        FreeplaySong& fs = songs[i];
        bool isSelected = ((int)i == curSelected);
        
        float scale = isSelected ? 0.6f : 0.45f;
        float itemY = centerY + ((int)i - lerpSelected) * 55;

        if (itemY < -15.0f || itemY > 255.0f) continue;

        // Staircase effect: offset X smoothly based on float distance from selected
        float distFromSelectedFloat = (float)i - lerpSelected;
        float distFloat = std::abs(distFromSelectedFloat);
        float itemOffsetX = -std::min(distFloat, 2.0f) * 30.0f;  // Max offset of 60px
        float itemCenterX = 270.0f + itemOffsetX + topIntroX;

        // Draw capsule for all songs (no scaling, fixed size)
        C2D_Image capsule = getCapsuleImage();
        if (capsule.tex) {
            // Position capsule centered horizontally at song item location
            float capsuleW = capsule.subtex->width;
            float capsuleX = itemCenterX - (capsuleW / 2.0f);
            
            float itemAlpha = isSelected ? exitAlpha : (0.6f * exitAlpha);

            C2D_ImageTint capTint;
            C2D_AlphaImageTint(&capTint, itemAlpha);
            C2D_DrawImageAt(capsule, capsuleX, itemY - 5, 0.2f, &capTint);
// Draw heart for favorites
            bool isFav = (favorites.count(fs.name) > 0);
            float animTime = heartBounceAnim[fs.name];
            bool shouldDrawHeart = isFav || (animTime < 0.0f);
            
            if (shouldDrawHeart && hasFavFrame) {
        // Heart scaled down by 1.77083f to match new high-res letterStuff (255x216 vs 144x122)
                float heartScale = 0.8f / 1.77083f;
                float heartW = favFrame.w * heartScale;
                float heartH = favFrame.h * heartScale;
                
                // Sway animation (rotation around bottom-center)
                float swayAngle = 0.15f * sinf(highscoreAnimTime * 3.5f);
                
                // Scalebeat animation (every 1.5 seconds)
                float timeInPeriod = fmodf(highscoreAnimTime, 1.5f);
                float beatScale = 1.0f;
                if (timeInPeriod < 0.15f) {
                    float beatProgress = timeInPeriod / 0.15f;
                    beatScale = 1.0f + 0.12f * sinf(beatProgress * 3.14159265f);
                }
                
                // Pivot point (bottom center of the heart)
                float pivotX = capsuleX + capsuleW - (heartW * 0.5f);
                float pivotY = itemY - 5 + (heartH * 0.5f);
                
                float hScaleMult = 1.0f;
                float hAlphaMult = 1.0f;
                
                if (animTime > 0.0f) {
                    // Jump-in animation (Back Ease Out tween)
                    float progress = animTime / 0.2f;
                    if (progress > 1.0f) progress = 1.0f;
                    float t = progress - 1.0f;
                    float s = 1.70158f;
                    hScaleMult = t * t * ((s + 1.0f) * t + s) + 1.0f;
                } else if (animTime < 0.0f) {
                    // Jump-out animation (Shrink)
                    float progress = -animTime / 0.15f;
                    if (progress > 1.0f) progress = 1.0f;
                    hScaleMult = 1.0f - progress;
                    hAlphaMult = 1.0f - progress;
                }
                
                // Rotated center coordinates
                float centerX = pivotX + (heartH * 0.5f) * sinf(swayAngle);
                float centerY = pivotY - (heartH * 0.5f) * cosf(swayAngle);
                
                C2D_Image img = { favFrame.tex, &favFrame.uv };
                C2D_ImageTint heartTint;
                C2D_PlainImageTint(&heartTint, C2D_Color32(255, 0, 0, (u8)(itemAlpha * hAlphaMult * 255.0f)), 1.0f);
                C2D_DrawImageAtRotated(img, centerX, centerY, 0.25f, swayAngle, &heartTint, heartScale * hScaleMult * beatScale, heartScale * hScaleMult * beatScale);
            }
            
            // Draw rating letter
            std::string rating = Highscores::getRating(fs.name, currentDifficultyStr);
            if (!rating.empty()) {
                Frame rFrame = getRatingFrame(rating);
                if (rFrame.tex) {
                    // Rating scaled down by 1.77083f to match new high-res letterStuff
                    float rScale = 0.8f / 1.77083f;
                    float rH = rFrame.h * rScale;
                    
                    // Fixed position inside the capsule (using 27.0f height of the capsule)
                    float rX = capsuleX + capsuleW - 30.0f;
                    float rY = (itemY - 5) + (27.0f * 0.5f) - (rH * 0.5f);
                    
                    C2D_ImageTint rTint;
                    C2D_PlainImageTint(&rTint, getRatingColor(rating, (u8)(itemAlpha * 255.0f)), 1.0f);
                    drawFrameAt(rFrame, rX, rY, 0.25f, &rTint, rScale, rScale);
                }
            }
            
            // Flush capsule image draw queue to render it before the scissor is configured
            C2D_Flush();
            
            // Draw text inside capsule area with clipping
            // Text area in capsule: X: 21, Y: 7, Width: 121, Height: 13
            float textBoxX = capsuleX + CAPSULE_TEXT_START_X;
            float textBoxY = itemY - 5 + CAPSULE_TEXT_START_Y;
            float textBoxXEnd = textBoxX + CAPSULE_TEXT_WIDTH;
            
            // Set scissor box in physical coordinates (CW rotation)
            u32 phys_left = (u32)(240.0f - (textBoxY + CAPSULE_TEXT_HEIGHT));
            u32 phys_right = (u32)(240.0f - textBoxY);
            u32 phys_top = (u32)(400.0f - textBoxXEnd);
            u32 phys_bottom = (u32)(400.0f - textBoxX);
            C3D_SetScissor(GPU_SCISSOR_NORMAL, phys_left, phys_top, phys_right, phys_bottom);
            
            u32 textCol = C2D_Color32(255, 255, 255, (u8)(itemAlpha * 255.0f));
            if (isSelected) {
                // Draw scrolling text for selected song
                float displayX = textBoxX - scrollOffset;
                AddText(fs.name, displayX, textBoxY - 1.0f, 0.45f, false, 0.0f, textCol, 0.0f);
            } else {
                // Draw static text left-aligned for non-selected songs
                AddText(fs.name, textBoxX, textBoxY - 1.0f, 0.45f, false, 0.0f, textCol, 0.0f);
            }

            // Disable scissor box
            C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
        } else {
            // Fallback: just text without capsule (spaced by 55)
            float itemAlpha = isSelected ? exitAlpha : (0.6f * exitAlpha);
            u32 textCol = C2D_Color32(255, 255, 255, (u8)(itemAlpha * 255.0f));
            AddText(fs.name, itemCenterX, itemY, scale, true, 2.0f, textCol, 0.0f);
        }
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32((int)curColor[0]*0.5f, (int)curColor[1]*0.5f, (int)curColor[2]*0.5f, 255));

    // Draw menuBG decoration tinted with the current selection color
    if (menuBgSheet) {
        C2D_Image bgImg = C2D_SpriteSheetGetImage(menuBgSheet, 0);
        if (bgImg.tex) {
            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32((int)curColor[0], (int)curColor[1], (int)curColor[2], (u8)(renderAlpha * 255.0f)), 1.0f);
            drawCenteredBG(bgImg, 320.0f, 240.0f, 0.11f, &tint);
        }
    }

    // Draw rotated cinematic black bars at top and bottom of bottom screen
    // Drawn at depth 0.15f so they are behind other sprites and text.
    // Heights extended to 100.0f and centers shifted to prevent any background gaps on corners due to rotation
    float barAngleRad = -5.0f * (3.14159265f / 180.0f);
    u32 barCol = C2D_Color32(0, 0, 0, (u8)(renderAlpha * 255.0f));
    drawRotatedRect(160.0f, -5.0f, 450.0f, 100.0f, barAngleRad, barCol, 0.15f);
    drawRotatedRect(160.0f, 245.0f, 450.0f, 100.0f, barAngleRad, barCol, 0.15f);

    if (!songs.empty()) {
        C2D_ImageTint uiTint;
        C2D_AlphaImageTint(&uiTint, exitAlpha);

        // Draw highscore & digital numbers UI
        if (highscoreSheet && numbersSheet) {
            float scale = 0.7f;
            float padding = 5.0f;
            
            if (!highscoreFrames.empty()) {
                // Highscore animation at 12 fps
                int hsFrameIdx = (int)(highscoreAnimTime * 12.0f) % highscoreFrames.size();
                const Frame& hsFrame = highscoreFrames[hsFrameIdx];
                
                // Position highscore sprite centered vertically at Y = 20
                float hsW = frameLogicalW(hsFrame) * scale;
                float hsH = frameLogicalH(hsFrame) * scale;
                float hsX = 10.0f;
                float hsY = 20.0f - hsH / 2.0f + topIntroY;
                
                drawFrameAt(hsFrame, hsX, hsY, 0.7f, &uiTint, scale, scale);
                
                // Format score to 7 digits, padding with leading zeroes
                int scoreVal = (int)std::round(lerpScore);
                if (scoreVal < 0) scoreVal = 0;
                std::string scoreStr = std::to_string(scoreVal);
                if (scoreStr.length() < 7) {
                    scoreStr = std::string(7 - scoreStr.length(), '0') + scoreStr;
                }
                
                float numStartX = hsX + hsW + padding;
                float numSpacing = 25.0f * scale; // 17.5px spacing at 0.7 scale
                
                for (size_t i = 0; i < scoreStr.length(); i++) {
                    int digit = scoreStr[i] - '0';
                    if (digit >= 0 && digit <= 9 && !numberFrames[digit].empty()) {
                        // Digital numbers animation at 24 fps
                        int numFrameIdx = (int)(numbersAnimTime * 24.0f) % numberFrames[digit].size();
                        const Frame& numFrame = numberFrames[digit][numFrameIdx];
                        
                        float numH = frameLogicalH(numFrame) * scale;
                        float numX = numStartX + (float)i * numSpacing;
                        float numY = 20.0f - numH / 2.0f + topIntroY;
                        
                        drawFrameAt(numFrame, numX, numY, 0.7f, &uiTint, scale, scale);
                    }
                }
            }
        }

        FreeplaySong& fs = songs[curSelected];

        // Draw week name using Alphabet font (to the right of the album photo, using storyName)
        std::string weekDisplayName = fs.week;
        if (WeekData::weeksLoaded.find(fs.week) != WeekData::weeksLoaded.end()) {
            const WeekData& data = WeekData::weeksLoaded[fs.week];
            weekDisplayName = data.storyName.empty() ? data.weekName : data.storyName;
        }
        std::transform(weekDisplayName.begin(), weekDisplayName.end(), weekDisplayName.begin(), ::toupper);

        // Split by space and wrap words to fit in maxAlphaW
        std::vector<std::string> lines;
        std::stringstream ss(weekDisplayName);
        std::string word;
        std::string currentLine = "";
        float currentScale = 0.8f; // Increased base scale
        float maxAlphaW = 165.0f; // Almost 67 NOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
        
        while (ss >> word) {
            std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
            float w = Alphabet::getTextWidth(testLine, currentScale);
            if (w > maxAlphaW && !currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine = word;
            } else {
                currentLine = testLine;
            }
        }
        if (!currentLine.empty()) {
            lines.push_back(currentLine);
        }

        // Dynamically scale down if any single line is still too wide (e.g. long word)
        float maxLineWidth = 0.0f;
        for (const auto& line : lines) {
            float w = Alphabet::getTextWidth(line, currentScale);
            if (w > maxLineWidth) maxLineWidth = w;
        }
        if (maxLineWidth > maxAlphaW) {
            currentScale *= (maxAlphaW / maxLineWidth);
        }

        // Center vertically in Y axis relative to the album center (120.0f)
        float lineHeight = 20.0f * currentScale;
        float totalHeight = lines.size() * lineHeight;
        float startY = 120.0f - (totalHeight / 2.0f) + (lineHeight * 0.15f);

        for (size_t i = 0; i < lines.size(); i++) {
            Alphabet::draw(lines[i], 140.0f + botIntroRightX, startY + (float)i * lineHeight, currentScale, exitAlpha, false);
        }

        // Draw selected song's health icon in the bottom-right corner (larger, with bounce)
        if (!fs.info.icon.empty()) {
            C2D_Image icon = getIconImage(fs.info.icon);
            if (icon.tex) {
                static Tex3DS_SubTexture defaultSubtex;
                if (icon.subtex == nullptr) {
                    defaultSubtex.width = icon.tex ? icon.tex->width : 0;
                    defaultSubtex.height = icon.tex ? icon.tex->height : 0;
                    defaultSubtex.left = 0.0f;
                    defaultSubtex.top = 0.0f;
                    defaultSubtex.right = 1.0f;
                    defaultSubtex.bottom = 1.0f;
                    icon.subtex = &defaultSubtex;
                }
                float iconScale = 1.5f;
                if (isCharacterIconMap[fs.info.icon] && icon.subtex) {
                    iconScale = 70.0f / (float)icon.subtex->width;
                }
                float iconW = icon.subtex->width * iconScale;
                float iconH = icon.subtex->height * iconScale;
                float defaultX = 320.0f - iconW - 15.0f;
                float defaultY = 240.0f - iconH - 15.0f;
                
                float iconX = defaultX + iconEggXOffset;
                float iconY = defaultY + iconBounceY + botIntroBottomY + iconEggYOffset;
                
                C2D_ImageTint iconTint;
                C2D_AlphaImageTint(&iconTint, exitAlpha);
                
                if (iconEggActive || iconEggScaleY != 1.0f || iconEggScaleX != 1.0f) {
                    float iconCenterX = iconX + iconW / 2.0f;
                    float iconCenterY = iconY + iconH / 2.0f;
                    C2D_DrawImageAtRotated(icon, iconCenterX, iconCenterY, 0.7f, iconRotation, &iconTint, iconScale * iconEggScaleX, iconScale * iconEggScaleY);
                } else {
                    C2D_DrawImageAt(icon, iconX, iconY, 0.7f, &iconTint, iconScale, iconScale);
                }
            } else {
                float defaultX = 320.0f - 75.0f - 15.0f;
                float defaultY = 240.0f - 75.0f - 15.0f;
                float iconCenterX = defaultX + 75.0f / 2.0f;
                float iconCenterY = defaultY + 75.0f / 2.0f + botIntroBottomY;
                
                drawRotatedRect(iconCenterX, iconCenterY, 20.0f, 20.0f, loadingAngle, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)), 0.7f);
            }
        }
        
        // Draw juggle combo game text on bottom screen
        float drawAlpha = 0.0f;
        int comboVal = 0;
        bool showText = false;
        
        if (iconEggCombo >= 5) {
            drawAlpha = exitAlpha;
            comboVal = iconEggCombo;
            showText = true;
        } else if (iconEggLossTimer > 0.0f) {
            drawAlpha = exitAlpha * iconEggLossTimer; // Fade out
            comboVal = iconEggLastCombo;
            // Blink effect (70% on, 30% off, alternating 10 times a second)
            showText = (fmodf(iconEggLossTimer * 10.0f, 1.0f) > 0.3f);
        }
        
        if (showText || (iconEggBest >= 5 && iconEggCombo >= 5) || (iconEggLossTimer > 0.0f && iconEggBest >= 5)) {
            float textYOffset = iconEggTextBounce; // Bounce effect
            
            // Draw SQUISHES :D
            if (showText && comboVal >= 5) {
                std::string comboStr = "SQUISHES: " + std::to_string(comboVal);
                u32 comboCol = C2D_Color32(255, 255, 255, (u8)(drawAlpha * 255.0f));
                
                // Rainbow color if you are matching or beating best score
                if (comboVal >= iconEggBest && iconEggBest > 0) {
                    float h = fmodf(textScrollTime * 180.0f, 360.0f) / 60.0f;
                    float x_col = 1.0f - std::abs(fmodf(h, 2.0f) - 1.0f);
                    float r = 0, g = 0, b = 0;
                    if (h < 1.0f) { r = 1; g = x_col; }
                    else if (h < 2.0f) { r = x_col; g = 1; }
                    else if (h < 3.0f) { g = 1; b = x_col; }
                    else if (h < 4.0f) { g = x_col; b = 1; }
                    else if (h < 5.0f) { r = x_col; b = 1; }
                    else { r = 1; b = x_col; }
                    comboCol = C2D_Color32((int)(r*255), (int)(g*255), (int)(b*255), (u8)(drawAlpha * 255.0f));
                }
                AddTextCentered(comboStr, 160.0f, 65.0f + textYOffset, 0.5f, 1.5f, comboCol, 0.0f);
            }
            
            // Draw BEST (yellow, solid but fades)
            if (iconEggBest >= 5) {
                std::string bestStr = "BEST: " + std::to_string(iconEggBest);
                float bestAlpha = (iconEggCombo >= 5) ? exitAlpha : (exitAlpha * iconEggLossTimer);
                u32 bestCol = C2D_Color32(255, 255, 0, (u8)(bestAlpha * 255.0f)); // Yellow
                AddTextCentered(bestStr, 160.0f, 80.0f + textYOffset, 0.4f, 1.5f, bestCol, 0.0f);
            }
        }

        // Draw Album Art image on the left side of the bottom screen, rotated -5 degrees with transition
        std::string albumName = getAlbumNameForSelected();
        C2D_Image albumImg = getAlbumImage(albumName);
        float angleRad = albumRotation * (3.14159265f / 180.0f);
        float albumCenterX = 70.0f + botIntroLeftX;  // centered horizontally on the left half
        float albumCenterY = 120.0f; // centered vertically
        if (albumImg.tex) {
            float albW = albumImg.subtex->width;
            float albH = albumImg.subtex->height;
            float targetSize = 100.0f;
            float scaleX = targetSize / albW;
            float scaleY = targetSize / albH;
            
            C2D_DrawImageAtRotated(albumImg, albumCenterX, albumCenterY, 0.65f, angleRad, &uiTint, scaleX, scaleY);
        } else {
            float targetSize = 100.0f;
            drawRotatedRect(albumCenterX, albumCenterY, targetSize, targetSize, angleRad, C2D_Color32(0, 0, 0, (u8)(exitAlpha * 255.0f)), 0.65f);
            drawRotatedRect(albumCenterX, albumCenterY, 25.0f, 25.0f, loadingAngle, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)), 0.66f);
        }

        // Draw Album Art text sprite positioned below the album photo
        C2D_Image albumTextImg = getAlbumTextImage(albumName);
        if (albumTextImg.tex) {
            float txtW = albumTextImg.subtex->width;
            float scaleX = 100.0f / txtW;
            float scaleY = scaleX; // keep aspect ratio
            
            float textCenterX = 70.0f + botIntroLeftX;
            float textCenterY = 185.0f;
            
            C2D_ImageTint tint;
            C2D_ImageTint* tintPtr = &tint;
            bool changedTintMode = false;
            if (albumTextFlashTime > 0.0f) {
                C2D_PlainImageTint(&tint, C2D_Color32(0x4B, 0x97, 0xF3, (u8)(exitAlpha * 255.0f)), 1.0f);
                C2D_SetTintMode(C2D_TintAdd);
                changedTintMode = true;
            } else if (exitAlpha < 1.0f) {
                C2D_AlphaImageTint(&tint, exitAlpha);
            } else {
                if (albumTextImg.tex && (albumTextImg.tex->fmt == GPU_A8 || albumTextImg.tex->fmt == GPU_A4)) {
                    C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, 255), 1.0f);
                } else {
                    C2D_AlphaImageTint(&tint, 1.0f);
                }
            }
            C2D_DrawImageAtRotated(albumTextImg, textCenterX, textCenterY, 0.66f, angleRad, tintPtr, scaleX, scaleY);
            if (changedTintMode) {
                C2D_SetTintMode(C2D_TintSolid);
            }
        }
    }
}

void FreeplayState::exitState() {
    savedDifficulty = curDifficulty;
    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
        savedSongName = songs[curSelected].name;
    }
    if (curCategoryIdx >= 0 && curCategoryIdx < (int)categories.size()) {
        savedCategory = categories[curCategoryIdx];
    }

    if (bfBgSheet) C2D_SpriteSheetFree(bfBgSheet);
    bfBgSheet = nullptr;
    
    if (capsuleSheet) C2D_SpriteSheetFree(capsuleSheet);
    capsuleSheet = nullptr;
    
    if (arrowSheet) C2D_SpriteSheetFree(arrowSheet);
    arrowSheet = nullptr;
    
    for (auto const& pair : diffCache) { if(pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet); }
    diffCache.clear();
    
    for (auto const& pair : iconCache) { if(pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet); }
    iconCache.clear();
    
    if (highscoreSheet) C2D_SpriteSheetFree(highscoreSheet);
    highscoreSheet = nullptr;
    highscoreFrames.clear();

    if (numbersSheet) C2D_SpriteSheetFree(numbersSheet);
    numbersSheet = nullptr;
    for (int i = 0; i < 10; i++) numberFrames[i].clear();

    for (auto const& pair : albumCache) { if(pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet); }
    albumCache.clear();
    
    for (auto const& pair : albumTextCache) { if(pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet); }
    albumTextCache.clear();

    if (menuBgSheet) C2D_SpriteSheetFree(menuBgSheet);
    menuBgSheet = nullptr;

    if (clearedSheet) C2D_SpriteSheetFree(clearedSheet);
    clearedSheet = nullptr;
    for (int i = 0; i < 10; i++) clearedNumberFrames[i].clear();


    if (letterStuffSheet) C2D_SpriteSheetFree(letterStuffSheet);
    letterStuffSheet = nullptr;
    letterStuffFrames.clear();

    currentAlbumName = "";

    C2D_TextBufDelete(vcrFontBuf);
}

C2D_Image FreeplayState::getBfBackgroundImage() {
    if (bfBgSheet) {
        return C2D_SpriteSheetGetImage(bfBgSheet, 0);
    }

    std::string path = Paths::image("freeplay/bf/bfFreeplayRBG", "preload");
    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            bfBgSheet = s;
            return img;
        }
    }
    return {nullptr, nullptr};
}

C2D_Image FreeplayState::getDifficultyImage(const std::string& name) {
    if (diffCache.count(name)) {
        diffCache[name].lastAccessFrame = cacheFrameCount;
        return C2D_SpriteSheetGetImage(diffCache[name].sheet, 0);
    }
    
    if (timeSinceSelectionChange < 0.10f) {
        return {nullptr, nullptr};
    }
    
    std::string path = Paths::image("freeplay/" + name, "preload");
    if (!Paths::fileExists(path)) {
        path = Paths::image("menudifficulties/placeholder", "preload");
    }
    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            
            enforceLRUCache(diffCache, 10);
            diffCache[name] = {s, cacheFrameCount};
            return img;
        }
    }
    return {nullptr, nullptr};
}



C2D_Image FreeplayState::getCapsuleImage() {
    if (capsuleSheet) {
        return C2D_SpriteSheetGetImage(capsuleSheet, 0);
    }

    std::string path = Paths::image("freeplay/bf/capsule", "preload");
    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            capsuleSheet = s;
            return img;
        }
    }
    return {nullptr, nullptr};
}

void FreeplayState::updateDifficulties() {
    curWeekDiffs.clear();
    if (songs.empty()) return;
    std::string weekName = songs[curSelected].week;
    if (WeekData::weeksLoaded.find(weekName) != WeekData::weeksLoaded.end()) {
        WeekData& data = WeekData::weeksLoaded[weekName];
        std::string diffs = data.difficulties;
        if (diffs.empty()) {
            curWeekDiffs = {"Easy", "Normal", "Hard"};
        } else {
            std::stringstream ss(diffs);
            std::string d;
            while (std::getline(ss, d, ',')) {
                size_t first = d.find_first_not_of(' ');
                if (first == std::string::npos) continue;
                size_t last = d.find_last_not_of(' ');
                curWeekDiffs.push_back(d.substr(first, last - first + 1));
            }
        }
    } else {
        curWeekDiffs = {"Easy", "Normal", "Hard"};
    }

    int normalIndex = -1;
    int lastDiffIndex = -1;
    for (int i = 0; i < (int)curWeekDiffs.size(); ++i) {
        if (curWeekDiffs[i] == lastDifficultyName) {
            lastDiffIndex = i;
        }
        if (curWeekDiffs[i] == "Normal") {
            normalIndex = i;
        }
    }

    if (lastDiffIndex != -1) {
        curDifficulty = lastDiffIndex;
    } else if (normalIndex != -1) {
        curDifficulty = normalIndex;
    } else if (!curWeekDiffs.empty()) {
        lastDifficultyName = curWeekDiffs.front();
        curDifficulty = 0;
    } else {
        curDifficulty = 0;
    }
}

C2D_Image FreeplayState::getIconImage(const std::string& name) {
    if (name.empty()) return {nullptr, nullptr};
    
    static std::unordered_map<std::string, Tex3DS_SubTexture> customIconSubtexs;
    if (iconCache.count(name)) {
        iconCache[name].lastAccessFrame = cacheFrameCount;
        C2D_Image img = C2D_SpriteSheetGetImage(iconCache[name].sheet, 0);
        if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
        if (customIconSubtexs.count(name) > 0) {
            img.subtex = &customIconSubtexs[name];
        }
        return img;
    }

    if (timeSinceSelectionChange < 0.10f) {
        return {nullptr, nullptr};
    }

    std::string prevModFolder = ModHandler::get().currentModFolder;
    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
        std::string week = songs[curSelected].week;
        if (WeekData::weeksLoaded.count(week) && WeekData::weeksLoaded[week].isMod) {
            ModHandler::get().currentModFolder = WeekData::weeksLoaded[week].modFolder;
        } else {
            ModHandler::get().currentModFolder = "";
        }
    }

    bool isCharacterIcon = false;
    std::string path = Paths::image("freeplayIcons/" + name, "preload");
    if (!Paths::fileExists(path)) {
        path = Paths::healthIcon(name);
        isCharacterIcon = true;
    }
    if (!Paths::fileExists(path)) {
        path = Paths::image("freeplayIcons/placeholder", "preload");
        isCharacterIcon = false;
    }
    if (!Paths::fileExists(path)) {
        path = Paths::healthIcon("face");
        isCharacterIcon = true;
    }
    ModHandler::get().currentModFolder = prevModFolder;
    isCharacterIconMap[name] = isCharacterIcon;

    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            
            if (isCharacterIcon && img.subtex != nullptr) {
                float u0 = img.subtex->left;
                float u1 = img.subtex->right;
                float v0 = img.subtex->top;
                float v1 = img.subtex->bottom;
                float fullW = (float)img.subtex->width;
                float halfW = fullW * 0.5f;
                float fullH = (float)img.subtex->height;
                float du = u1 - u0;

                Tex3DS_SubTexture sub;
                sub.width  = (u16)halfW;
                sub.height = (u16)fullH;
                sub.left   = u0;
                sub.right  = u0 + du * 0.5f;
                sub.top    = v0;
                sub.bottom = v1;

                customIconSubtexs[name] = sub;
                img.subtex = &customIconSubtexs[name];
            }
            
            enforceLRUCache(iconCache, 30);
            iconCache[name] = {s, cacheFrameCount};
            return img;
        }
    }
    return {nullptr, nullptr};
}

C2D_Image FreeplayState::getAlbumImage(const std::string& name) {
    std::string targetName = name;
    if (targetName.empty()) {
        targetName = "placeholder";
    }

    if (albumCache.count(targetName)) {
        albumCache[targetName].lastAccessFrame = cacheFrameCount;
        return C2D_SpriteSheetGetImage(albumCache[targetName].sheet, 0);
    }

    if (timeSinceSelectionChange < 0.10f) {
        return {nullptr, nullptr};
    }

    currentAlbumName = targetName;
    C2D_SpriteSheet albumSheet = nullptr;

    std::string prevModFolder = ModHandler::get().currentModFolder;
    std::string modFolder = "";

    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
        std::string week = songs[curSelected].week;
        bool exists = WeekData::weeksLoaded.count(week) > 0;
        bool isMod = exists && WeekData::weeksLoaded[week].isMod;
        if (isMod) {
            modFolder = WeekData::weeksLoaded[week].modFolder;
        }
        
        // Fallback: If modFolder is empty, check where the song chart JSON resides
        if (modFolder.empty()) {
            std::string songNameLower = songs[curSelected].name;
            std::transform(songNameLower.begin(), songNameLower.end(), songNameLower.begin(), ::tolower);
            std::string chartPath = "data/" + songNameLower + "/" + songNameLower + ".json";
            modFolder = ModHandler::get().getModFolderOfFile(chartPath);
        }



        if (!modFolder.empty()) {
            ModHandler::get().currentModFolder = modFolder;
        } else {
            ModHandler::get().currentModFolder = "";
        }
    }

    // 1. Try loading freeplay/album/targetName
    if (targetName != "placeholder") {
        std::string path = Paths::image("freeplay/album/" + targetName);
        if (Paths::fileExists(path)) {
            albumSheet = Paths_loadSpriteSheet(path.c_str());
        }
    }

    // 2. Fallback to mod pack icon (pack.t3x or pack.rawtex)
    if (!albumSheet && !modFolder.empty()) {
        std::string bases[] = {
            ModHandler::get().getWorkingBase(),
            "sdmc:/SnakeEngine/",
            "/SnakeEngine/",
            "SnakeEngine/"
        };
        for (std::string base : bases) {
            if (base.empty()) continue;
            if (base.back() != '/') base += "/";
            
            std::string packPath = base + modFolder + "/pack.t3x";
            if (Paths::fileExists(packPath)) {
                albumSheet = Paths_loadSpriteSheet(packPath.c_str());
                if (albumSheet) {
                    break;
                }
            }
            packPath = base + modFolder + "/pack.rawtex";
            if (Paths::fileExists(packPath)) {
                albumSheet = Paths_loadSpriteSheet(packPath.c_str());
                if (albumSheet) {
                    break;
                }
            }
        }
    }

    // 3. Fallback to placeholder
    if (!albumSheet) {
        std::string path = Paths::image("freeplay/album/placeholder");
        if (Paths::fileExists(path)) {
            albumSheet = Paths_loadSpriteSheet(path.c_str());
        }
    }



    ModHandler::get().currentModFolder = prevModFolder;

    if (albumSheet) {
        C2D_Image img = C2D_SpriteSheetGetImage(albumSheet, 0);
        if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
        
        enforceLRUCache(albumCache, 5);
        albumCache[targetName] = {albumSheet, cacheFrameCount};
        
        return img;
    }
    return {nullptr, nullptr};
}

C2D_Image FreeplayState::getAlbumTextImage(const std::string& name) {
    std::string targetName = name;
    if (targetName.empty()) {
        targetName = "placeholder";
    }
    
    std::string lookupName = targetName + "-text";
    
    if (albumTextCache.count(targetName)) {
        albumTextCache[targetName].lastAccessFrame = cacheFrameCount;
        return C2D_SpriteSheetGetImage(albumTextCache[targetName].sheet, 0);
    }
    
    if (timeSinceSelectionChange < 0.10f) {
        return {nullptr, nullptr};
    }
    
    std::string path = Paths::image("freeplay/album/" + lookupName);
    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            
            enforceLRUCache(albumTextCache, 5);
            albumTextCache[targetName] = {s, cacheFrameCount};
            return img;
        }
    }
    return {nullptr, nullptr};
}

std::string FreeplayState::getAlbumNameForSelected() {
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) {
        return "placeholder";
    }
    
    const FreeplaySong& fs = songs[curSelected];
    if (WeekData::weeksLoaded.find(fs.week) == WeekData::weeksLoaded.end()) {
        return "placeholder";
    }
    
    const WeekData& wd = WeekData::weeksLoaded[fs.week];
    
    std::string songNameLower = fs.name;
    std::transform(songNameLower.begin(), songNameLower.end(), songNameLower.begin(), ::tolower);
    
    std::string diffLower = "";
    if (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) {
        diffLower = curWeekDiffs[curDifficulty];
        std::transform(diffLower.begin(), diffLower.end(), diffLower.begin(), ::tolower);
    }
    
    std::string lookupKeyWithDiff = songNameLower + "-" + diffLower;
    std::string lookupKeySongOnly = songNameLower;
    
    if (!wd.songAlbum.empty()) {
        if (!diffLower.empty() && wd.songAlbum.count(lookupKeyWithDiff)) {
            return wd.songAlbum.at(lookupKeyWithDiff);
        }
        if (wd.songAlbum.count(lookupKeySongOnly)) {
            return wd.songAlbum.at(lookupKeySongOnly);
        }
    }
    
    if (!wd.album.empty()) {
        return wd.album;
    }
    
    return "placeholder";
}

void FreeplayState::rebuildCategories() {
    categories.clear();
    categories.push_back("all");
    categories.push_back("fav");

    bool hasHash = false;
    for (const auto& song : allSongs) {
        if (!song.name.empty() && !std::isalpha((unsigned char)song.name[0])) {
            hasHash = true;
            break;
        }
    }
    if (hasHash) {
        categories.push_back("#");
    }

    for (char c = 'a'; c <= 'z'; ++c) {
        bool hasLetter = false;
        for (const auto& song : allSongs) {
            if (!song.name.empty() && std::tolower((unsigned char)song.name[0]) == c) {
                hasLetter = true;
                break;
            }
        }
        if (hasLetter) {
            std::string letterStr(1, c);
            categories.push_back(letterStr);
        }
    }
}

void FreeplayState::applyCategoryFilter(bool keepSelection) {
    std::string currentCategory = categories.empty() ? "all" : categories[curCategoryIdx];

    // Save the current song name to restore selection after filtering
    std::string previousSongName;
    if (keepSelection && !songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
        previousSongName = songs[curSelected].name;
    }

    songs.clear();
    for (const auto& song : allSongs) {
        // First check if the song supports the currently selected difficulty (lastDifficultyName)
        bool supportsDifficulty = false;
        std::vector<std::string> songDiffs;
        if (WeekData::weeksLoaded.find(song.week) != WeekData::weeksLoaded.end()) {
            std::string diffs = WeekData::weeksLoaded[song.week].difficulties;
            if (diffs.empty()) {
                songDiffs = {"Easy", "Normal", "Hard"};
            } else {
                std::stringstream ss(diffs);
                std::string d;
                while (std::getline(ss, d, ',')) {
                    size_t first = d.find_first_not_of(' ');
                    if (first == std::string::npos) continue;
                    size_t last = d.find_last_not_of(' ');
                    songDiffs.push_back(d.substr(first, last - first + 1));
                }
            }
        } else {
            songDiffs = {"Easy", "Normal", "Hard"};
        }

        std::string targetDiffLower = lastDifficultyName;
        std::transform(targetDiffLower.begin(), targetDiffLower.end(), targetDiffLower.begin(), ::tolower);

        for (const auto& d : songDiffs) {
            std::string dLower = d;
            std::transform(dLower.begin(), dLower.end(), dLower.begin(), ::tolower);
            if (dLower == targetDiffLower) {
                supportsDifficulty = true;
                break;
            }
        }

        if (!supportsDifficulty) continue;

        if (currentCategory == "all") {
            songs.push_back(song);
        } else if (currentCategory == "fav") {
            if (favorites.count(song.name) > 0) {
                songs.push_back(song);
            }
        } else if (currentCategory == "#") {
            if (!song.name.empty() && !std::isalpha((unsigned char)song.name[0])) {
                songs.push_back(song);
            }
        } else {
            char targetChar = currentCategory[0];
            if (!song.name.empty() && std::tolower((unsigned char)song.name[0]) == targetChar) {
                songs.push_back(song);
            }
        }
    }

    // Fallback to "all" if the current category is empty after filtering
    if (currentCategory != "all" && songs.empty()) {
        curCategoryIdx = 0;
        applyCategoryFilter(keepSelection);
        return;
    }

    if (keepSelection && !previousSongName.empty()) {
        // Try to find the previously selected song in the new list
        int foundIdx = -1;
        for (int i = 0; i < (int)songs.size(); i++) {
            if (songs[i].name == previousSongName) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx >= 0) {
            curSelected = foundIdx;
            lerpSelected = (float)curSelected;
        } else {
            // Song not in new filter (e.g. removed from fav), clamp to valid range
            curSelected = std::min(curSelected, (int)songs.size() - 1);
            if (curSelected < 0) curSelected = 0;
            lerpSelected = (float)curSelected;
        }
    } else {
        curSelected = 0;
        lerpSelected = 0;
    }

    updateDifficulties();
}

bool FreeplayState::hasSongsInCategory(const std::string& catName) {
    if (catName == "all") return true;

    for (const auto& song : allSongs) {
        if (catName == "fav") {
            if (favorites.count(song.name) > 0) return true;
        } else if (catName == "#") {
            if (!song.name.empty() && !std::isalpha((unsigned char)song.name[0])) return true;
        } else {
            char targetChar = catName[0];
            if (!song.name.empty() && std::tolower((unsigned char)song.name[0]) == targetChar) return true;
        }
    }
    return false;
}

void FreeplayState::loadFavorites() {
    favorites.clear();
    FILE* f = fopen("sdmc:/SnakeEngine/favorites.txt", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        std::string songName(line);
        songName.erase(std::remove(songName.begin(), songName.end(), '\n'), songName.end());
        songName.erase(std::remove(songName.begin(), songName.end(), '\r'), songName.end());
        if (!songName.empty()) {
            favorites.insert(songName);
        }
    }
    fclose(f);
}

#include <sys/stat.h>
void FreeplayState::saveFavorites() {
    mkdir("sdmc:/SnakeEngine", 0777);
    FILE* f = fopen("sdmc:/SnakeEngine/favorites.txt", "w");
    if (!f) return;
    for (const auto& song : favorites) {
        fprintf(f, "%s\n", song.c_str());
    }
    fclose(f);
}

Frame FreeplayState::getLetterFrame(const std::string& categoryName) {
    std::string prefix = categoryName;
    if (prefix == "all") prefix = "ALL";
    for (const auto& f : letterStuffFrames) {
        if (f.name.find(prefix + " ") == 0 || f.name == prefix) {
            return f;
        }
    }
    if (!letterStuffFrames.empty()) return letterStuffFrames[0];
    return Frame();
}

Frame FreeplayState::getRatingFrame(const std::string& rating) {
    std::string prefix = rating;
    if (prefix == "P") prefix = "p";
    else if (prefix == "E") prefix = "e";
    else if (prefix == "G" || prefix == "g") prefix = "g";
    else if (prefix == "L") prefix = "l";
    
    for (const auto& f : letterStuffFrames) {
        if (f.name.find(prefix + " ") == 0 || f.name == prefix) {
            return f;
        }
    }
    return Frame();
}

u32 FreeplayState::getRatingColor(const std::string& rating, u8 alpha) {
    if (rating == "P") return C2D_Color32(0xFF, 0xA8, 0xFF, alpha); // ffa8ff
    if (rating == "E") return C2D_Color32(0xFF, 0xFF, 0xB9, alpha); // ffffb9
    if (rating == "G") return C2D_Color32(0xEF, 0xFD, 0xFF, alpha); // effdff (great)
    if (rating == "g") return C2D_Color32(0xF3, 0xA3, 0x80, alpha); // f3a380 (good)
    if (rating == "L") return C2D_Color32(0x6B, 0x8C, 0xFB, alpha); // 6b8cfb (loss)
    return C2D_Color32(255, 255, 255, alpha);
}

Frame FreeplayState::getArrowFrame() {
    for (const auto& f : letterStuffFrames) {
        if (f.name.find("mini arrow") == 0 || f.name == "mini arrow") return f;
    }
    if (!letterStuffFrames.empty()) return letterStuffFrames[0];
    return Frame();
}

void FreeplayState::drawCategoryOrganizer(float topIntroY, float exitAlpha, float ostX) {
    if (categories.empty() || letterStuffFrames.empty()) return;

    // "FREEPLAY" text ends at approx x=115 (10px start + ~105px width at scale 0.7)
    float freeplayEndX = 115.0f;
    float centerX = freeplayEndX + (ostX - freeplayEndX) / 2.0f;
    float centerY = 15.0f + topIntroY;
    // baseScale scaled down by 1.77083f to match new high-res letterStuff
    float baseScale = 0.6f / 1.77083f;  // Fit inside 30px bar (sprites ~25px * 0.6 = 15px)
    float depth = 0.96f;     // Above the black bar (z=0.9f) and OST text (z=0.95f)

    // Draw Left and Right arrows — Texture is LA44 (black text)
    Frame arrowF = getArrowFrame();
    if (arrowF.tex) {
        C2D_ImageTint tintArr;
        C2D_AlphaImageTint(&tintArr, exitAlpha);
        // Target height on screen is 21.0f pixels (equivalent to old 17.5px * 1.2 scale)
        float arrowScale = 21.0f / arrowF.h;
        // Left arrow (flipped horizontally, adjusted X)
        drawFrameCentered(arrowF, centerX - 45.0f, centerY, depth, &tintArr, -arrowScale, arrowScale);
        // Right arrow
        drawFrameCentered(arrowF, centerX + 45.0f - arrowF.w * arrowScale, centerY, depth, &tintArr, arrowScale, arrowScale);
    }

    // Previous Item (dimmed, moved closer to center: 15px)
    int prevIdx = curCategoryIdx - 1;
    if (prevIdx < 0) prevIdx = (int)categories.size() - 1;
    std::string prevCat = categories[prevIdx];
    Frame prevFrame = getLetterFrame(prevCat);
    if (prevFrame.tex) {
        C2D_ImageTint tint;
        if (prevCat == "fav" && favorites.empty()) {
            C2D_AlphaImageTint(&tint, exitAlpha * 0.10f);
        } else {
            C2D_AlphaImageTint(&tint, exitAlpha * 0.25f);
        }
        drawFrameCentered(prevFrame, centerX - 15.0f, centerY, depth, &tint, baseScale * 0.75f, baseScale * 0.75f);
    }

    // Next Item (dimmed, moved closer to center: 15px)
    int nextIdx = curCategoryIdx + 1;
    if (nextIdx >= (int)categories.size()) nextIdx = 0;
    std::string nextCat = categories[nextIdx];
    Frame nextFrame = getLetterFrame(nextCat);
    if (nextFrame.tex) {
        C2D_ImageTint tint;
        if (nextCat == "fav" && favorites.empty()) {
            C2D_AlphaImageTint(&tint, exitAlpha * 0.10f);
        } else {
            C2D_AlphaImageTint(&tint, exitAlpha * 0.25f);
        }
        drawFrameCentered(nextFrame, centerX + 15.0f, centerY, depth, &tint, baseScale * 0.75f, baseScale * 0.75f);
    }

    // Current Item (full brightness, scaled 1.25x larger: 0.75f)
    std::string curCat = categories[curCategoryIdx];
    Frame curFrame = getLetterFrame(curCat);
    if (curFrame.tex) {
        C2D_ImageTint tint;
        if (curCat == "fav" && favorites.empty()) {
            C2D_AlphaImageTint(&tint, exitAlpha * 0.20f);
        } else {
            C2D_AlphaImageTint(&tint, exitAlpha);
        }
        drawFrameCentered(curFrame, centerX, centerY + categoryBounceY, depth, &tint, baseScale * 1.25f, baseScale * 1.25f);
    }
}

