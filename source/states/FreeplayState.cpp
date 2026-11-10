#include "FreeplayState.hpp"
#include "PlayState.hpp"
#include "MainMenuState.hpp"
#include "VideoState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include "Highscores.hpp"
#include "../objects/Alphabet.hpp"
#include "../objects/ButtonPrompt.hpp"
#include "CharacterSelectState.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <unordered_map>

static std::string lastDifficultyName = "Normal";


static std::vector<std::string> getAllUniqueDifficulties() {
    std::vector<std::string> canonicalOrder = {"Easy", "Normal", "Hard", "Erect", "Nightmare"};
    std::vector<std::string> diffs;
    std::vector<std::string> customDiffs;

    auto toLowerStr = [](const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)tolower((unsigned char)c);
        return out;
    };

    auto isCanonical = [&](const std::string& nameLower) {
        for (const auto& c : canonicalOrder) {
            if (toLowerStr(c) == nameLower) return true;
        }
        return false;
    };

    diffs.push_back("Easy");
    diffs.push_back("Normal");
    diffs.push_back("Hard");

    bool hasErect = false;
    bool hasNightmare = false;

    for (const auto& weekName : WeekData::weeksList) {
        auto it = WeekData::weeksLoaded.find(weekName);
        if (it == WeekData::weeksLoaded.end()) continue;
        const WeekData& data = it->second;
        if (!data.difficulties.empty()) {
            std::stringstream ss(data.difficulties);
            std::string d;
            while (std::getline(ss, d, ',')) {
                size_t first = d.find_first_not_of(' ');
                if (first == std::string::npos) continue;
                size_t last = d.find_last_not_of(' ');
                std::string raw = d.substr(first, last - first + 1);
                std::string low = toLowerStr(raw);

                if (low == "erect") hasErect = true;
                else if (low == "nightmare") hasNightmare = true;
                else if (!isCanonical(low)) {
                    raw[0] = (char)toupper((unsigned char)raw[0]);
                    for (size_t k = 1; k < raw.size(); ++k) raw[k] = (char)tolower((unsigned char)raw[k]);
                    bool exists = false;
                    for (const auto& cd : customDiffs) {
                        if (toLowerStr(cd) == low) { exists = true; break; }
                    }
                    if (!exists) customDiffs.push_back(raw);
                }
            }
        }
    }

    if (hasErect) diffs.push_back("Erect");
    if (hasNightmare) diffs.push_back("Nightmare");

    for (const auto& cd : customDiffs) {
        diffs.push_back(cd);
    }

    return diffs;
}

int FreeplayState::iconEggBest = 0;
int FreeplayState::savedDifficulty = 1;
std::string FreeplayState::savedSongName = "";
std::string FreeplayState::savedCategory = "all";
std::string FreeplayState::currentChar = "bf";

FreeplayState::FreeplayState(bool isSubState)
    : isSubState(isSubState), hasFinishedExit(false), isExiting(false), hasExited(false), exitTimer(0.0f)
{
    LightLock_Init(&loadLock);
    LightEvent_Init(&loadEvent, RESET_ONESHOT);
}

FreeplayState::~FreeplayState() {
    if (!hasExited) {
        exitState();
    }
}

static inline float clampf(float v, float mn, float mx) {
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static inline float easeBackOut(float t, float s = 1.70158f) {
    t -= 1.0f;
    return t * t * ((s + 1.0f) * t + s) + 1.0f;
}

static inline float easeCubicOut(float t) {
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}

static inline float easeQuartOut(float t) {
    float f = 1.0f - t;
    return 1.0f - f * f * f * f;
}

void FreeplayState::openSubState(bool playIntro) {
    isExiting = false;
    hasFinishedExit = false;
    exitTimer = 0.0f;
    introTimer = playIntro ? 0.0f : 1.0f;
    if (!songs.empty()) {
        if (curSelected < 0 || curSelected >= (int)songs.size()) {
            curSelected = (songs.size() > 1) ? 1 : 0;
            lerpSelected = (float)curSelected;
        }
        targetColor[0] = songs[curSelected].info.color[0];
        targetColor[1] = songs[curSelected].info.color[1];
        targetColor[2] = songs[curSelected].info.color[2];
        if (playIntro) {
            curColor[0] = 253.0f;
            curColor[1] = 232.0f;
            curColor[2] = 113.0f;
        } else {
            curColor[0] = targetColor[0];
            curColor[1] = targetColor[1];
            curColor[2] = targetColor[2];
        }
        updateDifficulties();
    }
    MusicPlayer::play("romfs:/preload/music/freeplayAndCredits.adp", 0.7f);
}

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

void FreeplayState::init() {
    LightLock_Init(&loadLock);
    LightEvent_Init(&loadEvent, RESET_ONESHOT);
    ModHandler::get().currentModFolder = "";
    hasExited = false;
    
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
    
    if (!isSubState) {
        MusicPlayer::play("romfs:/preload/music/freeplayAndCredits.adp", 0.7f);
    }

    VCRFontFix();

    if (WeekData::weeksLoaded.empty()) {
        WeekData::reloadWeekFiles(true);
    }

    allSongs.clear();
    for (const auto& weekName : WeekData::weeksList) {
        if (WeekData::weeksLoaded.find(weekName) != WeekData::weeksLoaded.end()) {
            WeekData& data = WeekData::weeksLoaded[weekName];
            if (!data.hideFreeplay) {
                for (const auto& song : data.songs) {
                    if (song.freeplayCharacter == FreeplayState::currentChar) {
                        allSongs.push_back({song.name, weekName, song});
                    }
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
    } else if (songs.size() > 1) {
        curSelected = 1;
        lerpSelected = (float)curSelected;
    }
    curDifficulty = savedDifficulty;

    if (!songs.empty()) {
        if (curSelected < 0 || curSelected >= (int)songs.size()) {
            curSelected = (songs.size() > 1) ? 1 : 0;
            lerpSelected = (float)curSelected;
        }
        targetColor[0] = songs[curSelected].info.color[0];
        targetColor[1] = songs[curSelected].info.color[1];
        targetColor[2] = songs[curSelected].info.color[2];
        curColor[0] = 253.0f;
        curColor[1] = 232.0f;
        curColor[2] = 113.0f;
        updateDifficulties();
        cachedAlbumName = getAlbumNameForSelected(curSelected);
        updateSelectedWeekText();
    }
    cachedFavPromptWidth = ButtonPrompt::getPromptWidth("y", "Favorite", 0.60f);
    categoryBounceY = 0.0f;

    // Load unified freeplayAssetsBF sprite sheet and XML
    std::string bfAssetsImgPath = Paths::image("freeplay/freeplayAssetsBF");
    std::string bfAssetsXmlPath = Paths::xml("freeplay/freeplayAssetsBF");
    freeplayBFSheet = C2D_SpriteSheetLoad(bfAssetsImgPath.c_str());

    accuracyDisplay.load();
    letterStuffFrames.clear();
    starFrame = Frame();
    fireLoopFrames.clear();
    for (int i = 0; i < 10; i++) {
        numberFrames[i].clear();
    }
    bfBgFrame.tex = nullptr;
    bfBgFrame.tex = nullptr;
    capsuleFrame.tex = nullptr;
    arrowFrame.tex = nullptr;

    if (freeplayBFSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(freeplayBFSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::vector<Frame> tempFrames;
        SparrowParser::parseXml(bfAssetsXmlPath, tempFrames);
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
            for (char& c : nl) c = (char)tolower((unsigned char)c);

            // 1. Background sprite
            if (nl == "bffreeplayrbg" || nl.find("bffreeplayrbg") != std::string::npos) {
                bfBgFrame = f;
            }
            // 2. Song capsule
            else if (nl == "capsule" || nl.find("capsule") == 0) {
                capsuleFrame = f;
            }
            // 3. Arrow
            else if (nl == "arrow" || (nl.find("arrow") == 0 && nl.find("mini") == std::string::npos)) {
                arrowFrame = f;
            }
            // 4. Star
            else if (nl == "star" || nl.find("star") == 0) {
                starFrame = f;
            }
            // 5. Fire loop
            else if (nl.find("fire loop") != std::string::npos) {
                fireLoopFrames.push_back(f);
            }
            // 6. Digital numbers
            else if (nl.find("zero digital")  != std::string::npos) numberFrames[0].push_back(f);
            else if (nl.find("one digital")   != std::string::npos) numberFrames[1].push_back(f);
            else if (nl.find("two digital")   != std::string::npos) numberFrames[2].push_back(f);
            else if (nl.find("three digital") != std::string::npos) numberFrames[3].push_back(f);
            else if (nl.find("four digital")  != std::string::npos) numberFrames[4].push_back(f);
            else if (nl.find("five digital")  != std::string::npos) numberFrames[5].push_back(f);
            else if (nl.find("six digital")   != std::string::npos) numberFrames[6].push_back(f);
            else if (nl.find("seven digital") != std::string::npos) numberFrames[7].push_back(f);
            else if (nl.find("eight digital") != std::string::npos) numberFrames[8].push_back(f);
            else if (nl.find("nine digital")  != std::string::npos) numberFrames[9].push_back(f);

            // 7. LetterStuff items
            if ((nl.find("instance") != std::string::npos || nl.find("seperator") != std::string::npos) && nl.find("fire loop") == std::string::npos) {
                letterStuffFrames.push_back(f);
            }
        }

        std::sort(fireLoopFrames.begin(), fireLoopFrames.end(), [](const Frame& a, const Frame& b) {
            return a.name < b.name;
        });
    }

    // Load unified base game icons
    std::string iconsImgPath = Paths::image("freeplay/freeplayIcons");
    std::string iconsXmlPath = Paths::xml("freeplay/freeplayIcons");
    freeplayIconsSheet = C2D_SpriteSheetLoad(iconsImgPath.c_str());
    freeplayIconsFrames.clear();
    if (freeplayIconsSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(freeplayIconsSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_NEAREST, GPU_NEAREST);

        std::vector<Frame> tempFrames;
        SparrowParser::parseXml(iconsXmlPath, tempFrames);
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
            freeplayIconsFrames.push_back(f);
        }
    }

    // Load unified base game difficulties
    std::string diffImgPath = Paths::image("freeplay/freeplayDiff");
    std::string diffXmlPath = Paths::xml("freeplay/freeplayDiff");
    freeplayDiffSheet = C2D_SpriteSheetLoad(diffImgPath.c_str());
    freeplayDiffFrames.clear();
    if (freeplayDiffSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(freeplayDiffSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::vector<Frame> tempFrames;
        SparrowParser::parseXml(diffXmlPath, tempFrames);
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
            freeplayDiffFrames.push_back(f);
        }
    }

    // Load unified base game albums & album texts
    std::string albumsImgPath = Paths::image("freeplay/freeplayAlbums");
    std::string albumsXmlPath = Paths::xml("freeplay/freeplayAlbums");
    freeplayAlbumsSheet = C2D_SpriteSheetLoad(albumsImgPath.c_str());
    freeplayAlbumsFrames.clear();
    if (freeplayAlbumsSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(freeplayAlbumsSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        std::vector<Frame> tempFrames;
        SparrowParser::parseXml(albumsXmlPath, tempFrames);
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
            freeplayAlbumsFrames.push_back(f);
        }
    }

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
    currentAlbumName = (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) ? (songs[curSelected].week + ":" + getAlbumNameForSelected(curSelected)) : "";
    std::string bgPath = Paths::image("menuBG", "shared");
    if (!Paths::fileExists(bgPath)) bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        menuBgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
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

    bfdjAnimate.loadSheet("preload/images/freeplay/bf/bfdj");
    bfdjAnimate.addAnim("Boyfriend DJ", "Boyfriend DJ", 24.0f, true);
    bfdjAnimate.play("Boyfriend DJ");

    highscoreAnimTime = 0.0f;
    numbersAnimTime = 0.0f;
    starWaveTimer = 0.0f;

    std::string initDiff = (!curWeekDiffs.empty() && curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : "";
    targetStars = (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size())
        ? getStarsForSong(songs[curSelected], initDiff, curDifficulty) : 0;
    displayStars = targetStars;
    starChainTimer = 0.0f;

    for (int i = 0; i < 10; i++) {
        starSlots[i] = StarSlot();
        if (displayStars <= 0) {
            starSlots[i].visible = false;
            starSlots[i].flameTier = 0;
        } else if (displayStars <= 10) {
            starSlots[i].visible = (i < displayStars);
            starSlots[i].flameTier = 0;
        } else if (displayStars <= 20) {
            starSlots[i].visible = true;
            starSlots[i].flameTier = (i < (displayStars - 10)) ? 1 : 0;
        } else {
            starSlots[i].visible = true;
            starSlots[i].flameTier = (i < (displayStars - 20)) ? 2 : 1;
        }
        starSlots[i].flameTimer = 0.0f; // Starts from 2 first frames
        starSlots[i].isExtinguishing = false;
        starSlots[i].extinguishTier = 0;
        starSlots[i].extinguishTimer = 0.0f;
        starSlots[i].spawnScale = 1.0f;
    }
    for (int i = 0; i < 7; i++) {
        lastDigit[i] = -1;
        digitAnimTimer[i] = 1.0f;
    }
    if (!songs.empty()) {
        std::string diff = (!curWeekDiffs.empty() && curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : "Normal";
        std::string suffix = "";
        if (diff == "Easy") suffix = "easy";
        else if (diff == "Hard") suffix = "hard";
        else if (diff != "Normal") {
            suffix = diff;
            for (char& c : suffix) c = (char)tolower((unsigned char)c);
        }
        targetScore = Highscores::getScore(songs[curSelected].name, suffix);
        lerpScore = (float)targetScore;
        float acc = Highscores::getAccuracy(songs[curSelected].name, suffix);
        accuracyDisplay.setAccuracy(acc, true);
    } else {
        targetScore = 0;
        lerpScore = 0.0f;
        accuracyDisplay.setAccuracy(0.0f, true);
    }

    if (!loadThread) {
        threadRunning = true;
        loadThread = threadCreate(threadMain, this, 32 * 1024, 0x33, -1, false);
    }

    lastSelectedCheck = curSelected;
    lastDifficultyCheck = curDifficulty;
    triggerAsyncLoad();
}

void FreeplayState::update(float dt) {
    loadingAngle += dt * 3.14159f * 2.0f;
    cacheFrameCount++;

    if (leftArrowVisibleTime > 0.0f) {
        leftArrowVisibleTime -= dt;
        if (leftArrowVisibleTime < 0.0f) leftArrowVisibleTime = 0.0f;
    }
    if (rightArrowVisibleTime > 0.0f) {
        rightArrowVisibleTime -= dt;
        if (rightArrowVisibleTime < 0.0f) rightArrowVisibleTime = 0.0f;
    }
    diffOffsetX += (0.0f - diffOffsetX) * (1.0f - exp2f(-20.0f * dt));

    // Consume loaded asset buffers from background thread (max 2 sheets per frame to prevent frame drops)
    std::vector<LoadedRawData> localCompleted;
    LightLock_Lock(&loadLock);
    if (!completedDataQueue.empty()) {
        int totalSongs = (int)songs.size();
        for (auto it = completedDataQueue.begin(); it != completedDataQueue.end(); ) {
            int diff = std::abs(it->req.songIndex - curSelected);
            if (totalSongs > 0) {
                diff = diff % totalSongs;
                diff = std::min(diff, totalSongs - diff);
            }
            if (diff > 2) {
                if (it->buffer) linearFree(it->buffer);
                it = completedDataQueue.erase(it);
            } else {
                ++it;
            }
        }

        std::stable_sort(completedDataQueue.begin(), completedDataQueue.end(), [this](const LoadedRawData& a, const LoadedRawData& b) {
            bool aIsCur = (a.req.songIndex == curSelected);
            bool bIsCur = (b.req.songIndex == curSelected);
            if (aIsCur != bIsCur) return aIsCur > bIsCur;
            return a.req.priority < b.req.priority;
        });

        int takeCount = std::min((int)completedDataQueue.size(), 2);
        localCompleted.assign(completedDataQueue.begin(), completedDataQueue.begin() + takeCount);
        completedDataQueue.erase(completedDataQueue.begin(), completedDataQueue.begin() + takeCount);
    }
    LightLock_Unlock(&loadLock);

    for (auto& item : localCompleted) {
        CacheEntry entry;
        entry.sheet = nullptr;
        entry.lastAccessFrame = ++cacheFrameCount;
        entry.isCharIcon = item.req.iconIsChar;

        if (!item.req.resolvedPath.empty()) {
            entry.sheet = Paths_loadSpriteSheet(item.req.resolvedPath.c_str());
        } else if (item.buffer && item.size > 0) {
            entry.sheet = C2D_SpriteSheetLoadFromMem(item.buffer, item.size);
        }
        if (item.buffer) {
            linearFree(item.buffer);
            item.buffer = nullptr;
        }

        switch (item.req.type) {
            case AsyncLoadRequest::AssetType::ICON: {
                if (iconCache.find(item.req.key) != iconCache.end() && iconCache[item.req.key].sheet) {
                    C2D_SpriteSheetFree(iconCache[item.req.key].sheet);
                }
                if (entry.sheet) {
                    C2D_Image img = C2D_SpriteSheetGetImage(entry.sheet, 0);
                    if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
                }
                iconCache[item.req.key] = entry;
                break;
            }
            case AsyncLoadRequest::AssetType::ALBUM: {
                if (albumCache.find(item.req.key) != albumCache.end() && albumCache[item.req.key].sheet) {
                    C2D_SpriteSheetFree(albumCache[item.req.key].sheet);
                }
                if (entry.sheet) {
                    C2D_Image img = C2D_SpriteSheetGetImage(entry.sheet, 0);
                    if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
                }
                albumCache[item.req.key] = entry;
                break;
            }
            case AsyncLoadRequest::AssetType::ALBUM_TEXT: {
                if (albumTextCache.find(item.req.key) != albumTextCache.end() && albumTextCache[item.req.key].sheet) {
                    C2D_SpriteSheetFree(albumTextCache[item.req.key].sheet);
                }
                if (entry.sheet) {
                    C2D_Image img = C2D_SpriteSheetGetImage(entry.sheet, 0);
                    if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
                }
                albumTextCache[item.req.key] = entry;
                break;
            }
            case AsyncLoadRequest::AssetType::DIFFICULTY: {
                if (diffCache.find(item.req.key) != diffCache.end() && diffCache[item.req.key].sheet) {
                    C2D_SpriteSheetFree(diffCache[item.req.key].sheet);
                }
                if (entry.sheet) {
                    C2D_Image img = C2D_SpriteSheetGetImage(entry.sheet, 0);
                    if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
                }
                diffCache[item.req.key] = entry;
                break;
            }
        }
        evictLruCacheIfNeeded();
    }

    if (curSelected != lastSelectedCheck || curDifficulty != lastDifficultyCheck) {
        if (curSelected != lastSelectedCheck) {
            starWaveTimer = 0.0f;
            cachedAlbumName = getAlbumNameForSelected(curSelected);
            updateSelectedWeekText();
        }

        lastSelectedCheck = curSelected;
        lastDifficultyCheck = curDifficulty;
        prunePendingRequests();
        triggerAsyncLoad();
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
            if (isSubState) {
                hasFinishedExit = true;
                return;
            }
            MusicBeatState::skipTransition = true;
            MainMenuState::comingFromFreeplay = true;
            switchState(new MainMenuState());
        }
        return;
    }

    if (introTimer < 1.0f) {
        float introDt = std::min(dt, 1.0f / 30.0f);
        introTimer += introDt * 1.8f;
        if (introTimer > 1.0f) introTimer = 1.0f;
    }

    u32 kDown = hidKeysDown();

    int prevSelectedForEgg = curSelected;

    if (keyJustPressed(KEY_B)) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.adp", 0.7f);
        isExiting = true;
        exitTimer = 0.0f;
        return;
    }

    if (keyJustPressed(KEY_L) && !categories.empty()) {
        int startIdx = curCategoryIdx;
        do {
            curCategoryIdx--;
            if (curCategoryIdx < 0) curCategoryIdx = (int)categories.size() - 1;
            if (!hasSongsInCategory(categories[curCategoryIdx])) continue;
            break;
        } while (curCategoryIdx != startIdx);

        if (curCategoryIdx != startIdx) {
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
            categoryBounceY = -15.0f;
            applyCategoryFilter();
        }
    }
    if (keyJustPressed(KEY_R) && !categories.empty()) {
        int startIdx = curCategoryIdx;
        do {
            curCategoryIdx++;
            if (curCategoryIdx >= (int)categories.size()) curCategoryIdx = 0;
            if (!hasSongsInCategory(categories[curCategoryIdx])) continue;
            break;
        } while (curCategoryIdx != startIdx);

        if (curCategoryIdx != startIdx) {
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
            categoryBounceY = -15.0f;
            applyCategoryFilter();
        }
    }

    if (keyJustPressed(KEY_Y) && !songs.empty()) {
        std::string songName = songs[curSelected].name;
        if (songName != "Random") {
            if (favorites.count(songName) > 0) {
            favorites.erase(songName);
            heartBounceAnim[songName] = -0.01f;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
        } else {
            favorites.insert(songName);
            heartBounceAnim[songName] = 0.01f;
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.adp", 0.7f);
        }
        saveFavorites();

        rebuildCategories();
        if (categories[curCategoryIdx] == "fav" && favorites.empty()) {
            curCategoryIdx = 0;
        }
        applyCategoryFilter(true);
        }
    }

    if (!songs.empty()) {
        bool changed = false;
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            curSelected--;
            if (curSelected < 0) curSelected = (int)songs.size() - 1;
            changed = true;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            curSelected++;
            if (curSelected >= (int)songs.size()) curSelected = 0;
            changed = true;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
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
            std::string newAlbum = songs[curSelected].week + ":" + getAlbumNameForSelected(curSelected);
            if (newAlbum != currentAlbumName) {
                currentAlbumName = newAlbum;
                albumRotation = 0.0f;
                albumTextFlashTime = 0.1f;
            }
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
                        float ease = -0.5f * (cosf(3.1415926535f * t) - 1.0f);
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
                        float ease = -0.5f * (cosf(3.1415926535f * t) - 1.0f);
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
                auto toLowerStr = [](const std::string& s) {
                    std::string out = s;
                    for (char& c : out) c = (char)tolower((unsigned char)c);
                    return out;
                };
                std::string targetDiffLower = toLowerStr(lastDifficultyName);
                int idx = -1;
                for (int i = 0; i < (int)diffs.size(); ++i) {
                    if (toLowerStr(diffs[i]) == targetDiffLower) {
                        idx = i;
                        break;
                    }
                }
                if (idx < 0) idx = 1;
                idx = (idx - 1 + (int)diffs.size()) % diffs.size();
                lastDifficultyName = diffs[idx];
            }
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
            leftArrowVisibleTime = 0.1f;
            diffOffsetX = 150.0f;
            applyCategoryFilter(true);
        }
        if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
                std::vector<std::string> diffs = getAllUniqueDifficulties();
                auto toLowerStr = [](const std::string& s) {
                    std::string out = s;
                    for (char& c : out) c = (char)tolower((unsigned char)c);
                    return out;
                };
                std::string targetDiffLower = toLowerStr(lastDifficultyName);
                int idx = -1;
                for (int i = 0; i < (int)diffs.size(); ++i) {
                    if (toLowerStr(diffs[i]) == targetDiffLower) {
                        idx = i;
                        break;
                    }
                }
                if (idx < 0) idx = 1;
                idx = (idx + 1) % diffs.size();
                lastDifficultyName = diffs[idx];
            }
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
            rightArrowVisibleTime = 0.1f;
            diffOffsetX = -150.0f;
            applyCategoryFilter(true);
        }

        if (keyJustPressed(KEY_X)) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.adp", 0.7f);
            MusicPlayer::stop();
            switchState(new VideoState("introSelect", new CharacterSelectState(true), true));
            return;
        }

        if (keyJustPressed(KEY_A | KEY_START)) {
            if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return;

            int chosenIndex = curSelected;
            if (songs[chosenIndex].name == "Random") { // When como cuando te sientes bien random asi a la verga osi osi
                if (songs.size() <= 1) {
                    AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.adp", 0.7f);
                    return;
                }
                chosenIndex = 1 + (rand() % ((int)songs.size() - 1));
                curSelected = chosenIndex;
                lerpSelected = (float)curSelected;
            }

            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.adp", 0.7f);
            FreeplaySong& fs = songs[chosenIndex];
            std::string lname = fs.name;
            for (char& c : lname) c = (char)tolower((unsigned char)c);
            
            std::string selectedModFolder = "";
            if (WeekData::weeksLoaded.count(fs.week)) {
                selectedModFolder = WeekData::weeksLoaded[fs.week].modFolder;
                ModHandler::get().currentModFolder = selectedModFolder;
            } else {
                selectedModFolder = ModHandler::get().currentModFolder;
            }
            MusicPlayer::stop();
            
            std::string diff = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : lastDifficultyName;
            std::string suffix = "";
            if (diff == "Easy" || diff == "easy") suffix = "easy";
            else if (diff == "Hard" || diff == "hard") suffix = "hard";
            else if (diff != "Normal" && diff != "normal") {
                suffix = diff;
                for (char& c : suffix) c = (char)tolower((unsigned char)c);
            }

            PlayState::blueBalls = 0;
            if (!fs.info.introVideo.empty()) {
                switchState(new VideoState(fs.info.introVideo, new PlayState(fs.name, suffix, selectedModFolder)));
            } else {
                switchState(new PlayState(fs.name, suffix, selectedModFolder));
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
                        if (icon.subtex && icon.subtex->width > 75.0f) {
                            iconScale = 70.0f / (float)icon.subtex->width;
                        }
                        float iconW = icon.subtex->width * iconScale;
                        float iconH = icon.subtex->height * iconScale;
                        float defaultX = 320.0f - iconW - 15.0f;
                        float defaultY = 240.0f - iconH - 28.0f;
                        
                        // Actual position of the icon in the air
                        float iconX = defaultX + iconEggXOffset;
                        float iconY = defaultY + iconEggYOffset;
                        
                        if (touch.px >= iconX && touch.px <= iconX + iconW &&
                            touch.py >= iconY && touch.py <= iconY + iconH) {
                            AudioEngine::playSound("romfs:/preload/sounds/plush.adp", 0.7f);
                            
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
                        int chosenIndex = curSelected;
                        if (songs[chosenIndex].name == "Random") {
                            if (songs.size() <= 1) return;
                            chosenIndex = 1 + (rand() % ((int)songs.size() - 1));
                            curSelected = chosenIndex;
                            lerpSelected = (float)curSelected;
                        }

                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.adp", 0.7f);
                        FreeplaySong& fs = songs[chosenIndex];
                        std::string selectedModFolder = "";
                        if (WeekData::weeksLoaded.count(fs.week)) {
                            selectedModFolder = WeekData::weeksLoaded[fs.week].modFolder;
                            ModHandler::get().currentModFolder = selectedModFolder;
                        } else {
                            selectedModFolder = ModHandler::get().currentModFolder;
                        }
                        MusicPlayer::stop();
                        
                        std::string diff = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : lastDifficultyName;
                        std::string suffix = "";
                        if (diff == "Easy" || diff == "easy") suffix = "easy";
                        else if (diff == "Hard" || diff == "hard") suffix = "hard";
                        else if (diff != "Normal" && diff != "normal") {
                            suffix = diff;
                            for (char& c : suffix) c = (char)tolower((unsigned char)c);
                        }
                        PlayState::blueBalls = 0;
                        switchState(new PlayState(fs.name, suffix, selectedModFolder));
                        return;
                    } else {
                        curSelected = tappedIndex;
                        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
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
                        std::string newAlbum = songs[curSelected].week + ":" + getAlbumNameForSelected(curSelected);
                        if (newAlbum != currentAlbumName) {
                            currentAlbumName = newAlbum;
                            albumRotation = 0.0f;
                            albumTextFlashTime = 0.1f;
                        }
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
                std::string newAlbum = songs[curSelected].week + ":" + getAlbumNameForSelected(curSelected);
                if (newAlbum != currentAlbumName) {
                    currentAlbumName = newAlbum;
                    albumRotation = 0.0f;
                    albumTextFlashTime = 0.1f;
                }
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
    starWaveTimer += dt;

    std::string curDiffStr = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : "";
    targetStars = (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size())
        ? getStarsForSong(songs[curSelected], curDiffStr, curDifficulty) : 0;
    updateStarChain(dt);
    for (int i = 0; i < 7; i++) {
        digitAnimTimer[i] += dt;
    }

    for (auto& pair : heartBounceAnim) {
        if (pair.second > 0.0f) {
            pair.second += dt;
            if (pair.second > 0.4f) pair.second = 0.0f;
        } else if (pair.second < 0.0f) {
            pair.second -= dt;
            if (pair.second < -0.4f) pair.second = 0.0f;
        }
    }

    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size() && songs[curSelected].name != "Random" && curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) {
        std::string diff = curWeekDiffs[curDifficulty];
        std::string suffix = "";
        if (diff == "Easy" || diff == "easy") suffix = "easy";
        else if (diff == "Hard" || diff == "hard") suffix = "hard";
        else if (diff != "Normal" && diff != "normal") {
            suffix = diff;
            for (char& c : suffix) c = (char)tolower((unsigned char)c);
        }
        targetScore = Highscores::getScore(songs[curSelected].name, suffix);
        float acc = Highscores::getAccuracy(songs[curSelected].name, suffix);
        accuracyDisplay.setAccuracy(acc);
    } else {
        targetScore = 0;
        accuracyDisplay.setAccuracy(0.0f);
    }

    if (std::abs(targetScore - lerpScore) < 0.5f) {
        lerpScore = (float)targetScore;
    } else {
        lerpScore += (targetScore - lerpScore) * (1.0f - exp2f(-12.0f * dt));
    }

    accuracyDisplay.update(dt);

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
                Frame iconF = getIconFrame(fs.info.icon);
                if (iconF.tex) {
                    float logW = frameLogicalW(iconF);
                    float iconScale = 1.5f;
                    if (logW > 75.0f) {
                        iconScale = 70.0f / logW;
                    }
                    float iconW = logW * iconScale;
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

    if (lastSelectedCheck != curSelected || lastDifficultyCheck != curDifficulty) {
        if (curSelected != lastSelectedCheck) {
            starWaveTimer = 0.0f;
            cachedAlbumName = getAlbumNameForSelected(curSelected);
            updateSelectedWeekText();
        }
        lastSelectedCheck = curSelected;
        lastDifficultyCheck = curDifficulty;
        prunePendingRequests();
        triggerAsyncLoad();
    }
}

void FreeplayState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();

    const std::string& currentDifficultyStr = cachedCurrentDifficultyStr;

    Frame favFrame;
    bool hasFavFrame = false;
    for (const auto& f : letterStuffFrames) {
        if (f.name.find("fav") == 0) {
            favFrame = f;
            hasFavFrame = true;
            break;
        }
    }
    float exitT = isExiting ? clampf(exitTimer / 0.35f, 0.0f, 1.0f) : 0.0f;
    float exitAlpha = isExiting ? fmaxf(0.0f, 1.0f - exitT) : 1.0f;
    float introEaseOut = easeCubicOut(introTimer);
    float renderAlpha = introEaseOut * exitAlpha;

    // 1. Top black title bar & elements
    float topBarT = clampf(introTimer / 0.85f, 0.0f, 1.0f);
    float topBarEase = easeQuartOut(topBarT);
    float topIntroY = -45.0f * (1.0f - topBarEase) - (45.0f * exitT * exitT);

    // 2. Song capsules
    float tCapsule = clampf((introTimer - 0.10f) / 0.90f, 0.0f, 1.0f);
    float capsuleEase = easeBackOut(tCapsule, 1.9f);
    float topIntroX = 420.0f * (1.0f - capsuleEase) + (420.0f * exitT * exitT);

    // 3. bfFreeplayRBG
    float tBgRbg = clampf((introTimer - 0.04f) / 0.96f, 0.0f, 1.0f);
    float bgRbgEase = easeCubicOut(tBgRbg);
    float bgRbgSlideX = 350.0f * (1.0f - bgRbgEase) + (350.0f * exitT * exitT);

    // 4. DJ Boyfriend & Scrolling Text
    float tBf = clampf(introTimer / 0.85f, 0.0f, 1.0f);
    float bfEase = easeBackOut(tBf, 1.4f);
    float botIntroLeftX = -260.0f * (1.0f - bfEase) - (260.0f * exitT * exitT);

    // 5. Bottom Screen rotated black bars
    float barClampsEase = easeCubicOut(introTimer);
    float barTopY = -5.0f - (125.0f * (1.0f - barClampsEase)) - (125.0f * exitT * exitT);
    float barBotY = 245.0f + (125.0f * (1.0f - barClampsEase)) + (125.0f * exitT * exitT);

    // 6. Bottom Screen Album Art & Week Title
    float tAlbum = clampf((introTimer - 0.06f) / 0.94f, 0.0f, 1.0f);
    float albumEase = easeBackOut(tAlbum, 1.4f);
    float botIntroRightX = 350.0f * (1.0f - albumEase) + (350.0f * exitT * exitT);

    // 7. Health icon pop-up
    float tIcon = clampf((introTimer - 0.15f) / 0.85f, 0.0f, 1.0f);
    float iconPopScale = easeBackOut(tIcon, 2.0f);
    float botIntroBottomY = 120.0f * (1.0f - iconPopScale) + (120.0f * exitT * exitT);

    C2D_SceneBegin(top);
    if (isSubState) {
        C3D_RenderTargetClear(top, C3D_CLEAR_DEPTH, 0, 0);
        float bgAlpha = introEaseOut * exitAlpha;
        C2D_DrawRectSolid(0, 0, 0.01f, 400, 240, C2D_Color32((int)curColor[0], (int)curColor[1], (int)curColor[2], (u8)(bgAlpha * 255.0f)));
    } else {
        C2D_TargetClear(top, C2D_Color32((int)curColor[0], (int)curColor[1], (int)curColor[2], 255));
    }

    // Difficulty selector
    if (!songs.empty()) {
        float diffCenterX = 60.0f; // Center pivot for difficulty moved more to the left
        float diffY = 40.0f + topIntroY;
        const std::string& lowerDiff = cachedCurDiffLower;

        C2D_Image dImg = getDifficultyImage(lowerDiff);

        float dW = 0, dH = 0;
        float scale = 0.6f;
        float diffDrawX = diffCenterX;
        
        C2D_ImageTint diffTint;
        C2D_AlphaImageTint(&diffTint, exitAlpha);

        std::string week = songs[curSelected].week;
        std::string modFolder = "";
        if (WeekData::weeksLoaded.count(week) && !WeekData::weeksLoaded[week].modFolder.empty()) {
            modFolder = WeekData::weeksLoaded[week].modFolder;
        }
        std::string assetScope = modFolder.empty() ? "base" : modFolder;
        bool isMod = WeekData::weeksLoaded.count(week) && WeekData::weeksLoaded[week].isMod;
        bool hasGlobal = false;
        for (const auto& m : ModHandler::get().getMods()) {
            if (m.active && m.runsGlobally) {
                hasGlobal = true;
                break;
            }
        }
        std::string fullDiffKey = assetScope + ":" + lowerDiff;
        bool isDiffCached = (dImg.tex != nullptr || diffCache.find(fullDiffKey) != diffCache.end() || (!isMod && !hasGlobal && hasBaseDiff(lowerDiff)));

        if (dImg.tex) {
            dW = dImg.subtex->width;
            dH = dImg.subtex->height;
            float dW_scaled = dW * scale;
            diffDrawX = diffCenterX - (dW_scaled / 2.0f);

            // Draw difficulty sprite with displacement offset
            C2D_DrawImageAt(dImg, diffDrawX + diffOffsetX, diffY, 0.08f, &diffTint, scale, scale);
        } else {
            if (!isDiffCached) {
                // Still loading from SD card: draw rotating loading square
                dW = 75.0f;
                dH = 20.0f;
                float dW_scaled = dW * scale;
                float dH_scaled = dH * scale;
                diffDrawX = diffCenterX - (dW_scaled / 2.0f);
                float loadingCenterY = diffY + (dH_scaled * 0.5f);
                drawRotatedRect(diffCenterX, loadingCenterY, 20.0f, 20.0f, loadingAngle, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)), 0.5f);
            } else {
                // Asset is cached BUT no sprite exists in any of the 3 paths:
                // Draw VCR font text showing the difficulty name (e.g. "Normal", "Hard")
                ClearTextBuf();
                C2D_Text diffTextObj;
                const std::string& upperDiffStr = cachedUpperDiffStr;
                C2D_TextFontParse(&diffTextObj, vcrFont, vcrFontBuf, upperDiffStr.c_str());
                C2D_TextOptimize(&diffTextObj);
                float dtW = 0.0f, dtH = 0.0f;
                C2D_TextGetDimensions(&diffTextObj, scale, scale, &dtW, &dtH);
                dW = (dtW > 0) ? (dtW / scale) : 75.0f;
                dH = (dtH > 0) ? (dtH / scale) : 20.0f;
                float dW_scaled = dW * scale;
                diffDrawX = diffCenterX - (dW_scaled / 2.0f);

                C2D_DrawText(&diffTextObj, C2D_WithColor, diffDrawX + diffOffsetX, diffY, 0.08f, scale, scale, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)));
            }
        }

        // Draw left and right arrows next to difficulty selector ALWAYS
        if (arrowFrame.tex) {
            float aW = frameLogicalW(arrowFrame) * scale;
            float aH = frameLogicalH(arrowFrame) * scale;
            float dH_scaled = (dH > 0 ? dH : 20.0f) * scale;
            float dW_scaled = (dW > 0 ? dW : 75.0f) * scale;
            
            // Fixed positions based on static diffDrawX (so arrows don't move with diffOffsetX animation)
            float leftArrowX = diffDrawX - 6.0f - aW;
            float rightArrowX = diffDrawX + dW_scaled + 6.0f - aW;
            float arrowY = diffY + (dH_scaled - aH) * 0.5f;
            
            C2D_ImageTint arrowLeftTint = diffTint;
            C2D_ImageTint arrowRightTint = diffTint;
            if (leftArrowVisibleTime > 0.0f) {
                C2D_PlainImageTint(&arrowLeftTint, C2D_Color32(255, 255, 0, (u8)(exitAlpha * 255.0f)), 1.0f);
            }
            if (rightArrowVisibleTime > 0.0f) {
                C2D_PlainImageTint(&arrowRightTint, C2D_Color32(255, 255, 0, (u8)(exitAlpha * 255.0f)), 1.0f);
            }

            // Left arrow
            drawFrameAt(arrowFrame, leftArrowX, arrowY, 0.5f, &arrowLeftTint, scale, scale);

            // Right arrow
            if (arrowFrame.rotated) {
                drawFrameAt(arrowFrame, rightArrowX, arrowY, 0.5f, &arrowRightTint, scale, -scale);
            } else {
                drawFrameAt(arrowFrame, rightArrowX, arrowY, 0.5f, &arrowRightTint, -scale, scale);
            }
        }
    }

    // Draw background sprite
    if (bfBgFrame.tex) {
        float bgW = frameLogicalW(bfBgFrame);
        float drawX = 400.0f - bgW + bgRbgSlideX;
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, exitAlpha);
        drawFrameAt(bfBgFrame, drawX, 0, 0.1f, &tint);
    }

    // Black title bar
    C2D_DrawRectSolid(0, topIntroY - 60.0f, 0.9f, 400, 30.0f + 60.0f, C2D_Color32(0, 0, 0, (u8)(exitAlpha * 255.0f)));
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
    for (char& c : ostName) c = (char)toupper((unsigned char)c);

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

    // Draw cleared box and accuracy inside
    float scale = 0.65f;
    float cbX = 400.0f - (82.4f * scale) - 10.0f;
    float cbY = 32.0f + topIntroY;
    accuracyDisplay.draw(cbX, cbY, 0.5f, exitAlpha, scale);

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

        // Draw capsule for all songs
        if (capsuleFrame.tex) {
            // Position capsule centered horizontally at song item location
            float capsuleW = frameLogicalW(capsuleFrame);
            float capsuleX = itemCenterX - (capsuleW / 2.0f);
            
            float itemAlpha = isSelected ? exitAlpha : (0.6f * exitAlpha);

            C2D_ImageTint capTint;
            C2D_AlphaImageTint(&capTint, itemAlpha);
            drawFrameAt(capsuleFrame, capsuleX, itemY - 5, 0.2f, &capTint);

            // Draw heart for favorites
            bool isFav = (fs.name != "Random") && (favorites.count(fs.name) > 0);
            float animTime = (fs.name != "Random") ? heartBounceAnim[fs.name] : 0.0f;
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
            std::string rating = (fs.name != "Random") ? Highscores::getRating(fs.name, currentDifficultyStr) : "";
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

    if (!g_isRightEye && bottom) {
        C2D_SceneBegin(bottom);
        if (isSubState) {
            C3D_RenderTargetClear(bottom, C3D_CLEAR_DEPTH, 0, 0);
            float bgAlpha = introEaseOut * exitAlpha;
            C2D_DrawRectSolid(0, 0, 0.01f, 320, 240, C2D_Color32((int)(curColor[0]*0.5f), (int)(curColor[1]*0.5f), (int)(curColor[2]*0.5f), (u8)(bgAlpha * 255.0f)));
        } else {
            C2D_TargetClear(bottom, C2D_Color32((int)curColor[0]*0.5f, (int)curColor[1]*0.5f, (int)curColor[2]*0.5f, 255));
        }

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
        drawRotatedRect(160.0f, barTopY, 450.0f, 100.0f, barAngleRad, barCol, 0.15f);
        drawRotatedRect(160.0f, barBotY, 450.0f, 100.0f, barAngleRad, barCol, 0.15f);

        if (!songs.empty()) {
            C2D_ImageTint uiTint;
            C2D_AlphaImageTint(&uiTint, exitAlpha);

            // Draw highscore & digital numbers UI
            if (highscoreSheet) {
                float scale = 0.65f;
                float padding = 2.0f;
                
                if (!highscoreFrames.empty()) {
                    // Highscore animation at 12 fps
                    int hsFrameIdx = (int)(highscoreAnimTime * 12.0f) % highscoreFrames.size();
                    const Frame& hsFrame = highscoreFrames[hsFrameIdx];
                    
                    // Position highscore sprite on the left side
                    float hsW = frameLogicalW(hsFrame) * scale;
                    float hsH = frameLogicalH(hsFrame) * scale;
                    float hsX = 10.0f + botIntroLeftX;
                    float hsY = 10.0f + topIntroY;
                    
                    drawFrameAt(hsFrame, hsX, hsY, 0.71f, &uiTint, scale, scale);
                    
                    // Format score to 7 digits, padding with leading zeroes
                    int scoreVal = (int)std::round(lerpScore);
                    if (scoreVal < 0) scoreVal = 0;
                    std::string scoreStr = std::to_string(scoreVal);
                    if (scoreStr.length() < 7) {
                        scoreStr = std::string(7 - scoreStr.length(), '0') + scoreStr;
                    }
                    
                    // Position digital numbers score counter below highscore sprite on the left side
                    float numStartX = 25.0f + botIntroLeftX;
                    float numY = hsY + hsH + padding;
                    float maxDigitW = 37.0f;
                    float digitCellW = maxDigitW * scale;
                    float digitSpacing = -3.0f; // -3 pixel distance between numbers
                    
                    for (size_t i = 0; i < scoreStr.length() && i < 7; i++) {
                        int digit = scoreStr[i] - '0';
                        if (digit != lastDigit[i]) {
                            lastDigit[i] = digit;
                            digitAnimTimer[i] = 0.0f;
                        }

                        if (digit >= 0 && digit <= 9 && !numberFrames[digit].empty()) {
                            int maxFrames = (int)numberFrames[digit].size();
                            int idleFrame = maxFrames - 1;
                            int animFrame = (int)(digitAnimTimer[i] * 24.0f);
                            int numFrameIdx = (animFrame < maxFrames) ? animFrame : idleFrame;

                            const Frame& numFrame = numberFrames[digit][numFrameIdx];
                            
                            float numW = frameLogicalW(numFrame) * scale;
                            
                            // Fixed column position
                            float cellX = numStartX + (float)i * (digitCellW + digitSpacing);
                            // Center digit inside its fixed width cell so '1' looks natural and aligned
                            float numX = cellX + (digitCellW - numW) * 0.5f;
                            
                            drawFrameAt(numFrame, numX, numY, 0.7f, &uiTint, scale, scale);
                        }
                    }
                }
            }

            FreeplaySong& fs = songs[curSelected];

            // Draw week name using Alphabet font (to the right of the album photo, using storyName)
            if (fs.name != "Random" && !cachedWeekLines.lines.empty()) {
                for (size_t i = 0; i < cachedWeekLines.lines.size(); i++) {
                    Alphabet::draw(cachedWeekLines.lines[i], 140.0f + botIntroRightX, cachedWeekLines.startY + (float)i * cachedWeekLines.lineHeight, cachedWeekLines.scale, exitAlpha, false);
                }

                // Draw difficulty stars and fire loop above storyName
                bool anyStarActive = false;
                for (int i = 0; i < 10; i++) {
                    if (starSlots[i].visible || starSlots[i].isExtinguishing) {
                        anyStarActive = true;
                        break;
                    }
                }

                if (anyStarActive && starFrame.tex != nullptr) {
                    float starScale = 0.28f;
                    float fireScale = 0.30f;
                    float starSpacing = 16.0f;
                    float startStarX = 140.0f + botIntroRightX;
                    float starCenterY = cachedWeekLines.startY - 14.0f;
                    float fireCenterY = starCenterY - 14.0f;

                const float cycleDuration = 100.0f / 24.0f;
                float globalCycleTime = fmodf(starWaveTimer, cycleDuration);
                if (globalCycleTime < 0.0f) globalCycleTime += cycleDuration;
                int globalFrame = (int)(globalCycleTime * 24.0f);
                if (globalFrame < 0) globalFrame = 0;
                if (globalFrame > 99) globalFrame = 99;

                float starOffsetsY[10];
                float starScales[10];
                int starFlashModes[10];

                for (int i = 0; i < 10; i++) {
                    if (!starSlots[i].visible && !starSlots[i].isExtinguishing) {
                        starOffsetsY[i] = 0.0f;
                        starScales[i] = 0.0f;
                        starFlashModes[i] = 0;
                        continue;
                    }

                    float yOffset = 0.0f;
                    float scaleMult = 1.0f;
                    int flash = 0;

                    if (starSlots[i].flameTier > 0) {
                        // Request 1 & 2: Stars with fire stay fixed ("tiesas") at peak height and scale,
                        // and use the illuminated cyan-white glowing tint (Atlas 0 look).
                        yOffset = -4.5f;
                        scaleMult = 1.15f;
                        flash = 1;
                    } else if (starSlots[i].isExtinguishing) {
                        // When extinguishing, smoothly return from peak height and scale
                        float p = clampf(starSlots[i].extinguishTimer / (2.0f / 24.0f), 0.0f, 1.0f);
                        yOffset = -4.5f * (1.0f - p);
                        scaleMult = 1.0f + 0.15f * (1.0f - p);
                        flash = (p < 0.5f) ? 1 : 0;
                    } else {
                        // Stars without fire: follow the 100-frame wave animation from Animation.json
                        int ff = 22 - (i * 2);
                        int starFrameIdx = (globalFrame + ff) % 100;

                        if (starFrameIdx == 63 || starFrameIdx == 64) {
                            yOffset = -1.5f;
                            scaleMult = 1.05f;
                        } else if (starFrameIdx == 65 || starFrameIdx == 66) {
                            yOffset = -4.5f;
                            scaleMult = 1.15f;
                            flash = 1;
                        } else if (starFrameIdx == 67) {
                            yOffset = -3.8f;
                            scaleMult = 1.10f;
                            flash = 2;
                        } else if (starFrameIdx >= 68 && starFrameIdx <= 70) {
                            yOffset = -1.0f;
                            scaleMult = 1.03f;
                        }
                    }

                    starOffsetsY[i] = yOffset;
                    starScales[i] = scaleMult * starSlots[i].spawnScale;
                    starFlashModes[i] = flash;
                }

                // 1. Draw fire loop behind stars if active
                for (int i = 0; i < 10; i++) {
                    if (!starSlots[i].visible && !starSlots[i].isExtinguishing) continue;

                    Frame fireF;
                    int tier = 0;
                    if (starSlots[i].flameTier > 0) {
                        tier = starSlots[i].flameTier;
                        fireF = getFireFrame(starSlots[i].flameTimer);
                    } else if (starSlots[i].isExtinguishing) {
                        tier = starSlots[i].extinguishTier;
                        if (!fireLoopFrames.empty()) {
                            // Reverse 2 initial intro frames: frame 1, then frame 0
                            int fIdx = (starSlots[i].extinguishTimer < (1.0f / 24.0f)) ? 1 : 0;
                            if (fIdx >= (int)fireLoopFrames.size()) fIdx = (int)fireLoopFrames.size() - 1;
                            fireF = fireLoopFrames[fIdx];
                        }
                    }

                    if (tier > 0 && fireF.tex != nullptr) {
                        float starCenterX = startStarX + (float)i * starSpacing + (58.0f * starScale * 0.5f);
                        float curFireY = fireCenterY + starOffsetsY[i];
                        float curFireScale = fireScale * starScales[i];

                        // Request 3: Stars with red fire tremble slightly
                        float shakeX = 0.0f;
                        float shakeY = 0.0f;
                        if (tier == 2) {
                            float shakePhase = starWaveTimer * 65.0f + (float)i * 12.3f;
                            shakeX = sinf(shakePhase * 1.7f) * 0.75f;
                            shakeY = cosf(shakePhase * 2.3f) * 0.75f;
                        }

                        C2D_ImageTint fireTint;
                        if (tier == 2) {
                            // Bright vibrant red fire
                            C2D_PlainImageTint(&fireTint, C2D_Color32(255, 45, 55, (u8)(exitAlpha * 255.0f)), 0.70f);
                            C2D_SetTintMode(C2D_TintSolid);
                            drawFrameCentered(fireF, starCenterX + shakeX, curFireY + shakeY, 0.70f, &fireTint, curFireScale, curFireScale);
                        } else {
                            // Normal blue/cyan fire (natural texture with alpha)
                            C2D_AlphaImageTint(&fireTint, exitAlpha);
                            C2D_SetTintMode(C2D_TintMult);
                            drawFrameCentered(fireF, starCenterX, curFireY, 0.70f, &fireTint, curFireScale, curFireScale);
                        }
                    }
                }
                C2D_SetTintMode(C2D_TintMult);

                // 2. Draw stars on top
                for (int i = 0; i < 10; i++) {
                    if (!starSlots[i].visible) continue;

                    float starCenterX = startStarX + (float)i * starSpacing + (58.0f * starScale * 0.5f);
                    float curStarY = starCenterY + starOffsetsY[i];
                    float curStarScale = starScale * starScales[i];

                    // Request 3: Tremble red-flame stars
                    if (starSlots[i].flameTier == 2) {
                        float shakePhase = starWaveTimer * 65.0f + (float)i * 12.3f;
                        starCenterX += sinf(shakePhase * 1.7f) * 0.75f;
                        curStarY += cosf(shakePhase * 2.3f) * 0.75f;
                    }

                    C2D_ImageTint starTint;
                    if (starFlashModes[i] == 1) {
                        // Request 1: Atlas 0 cyan-white glow (used at jump peak and permanently on fire stars)
                        C2D_PlainImageTint(&starTint, C2D_Color32(120, 235, 255, (u8)(exitAlpha * 255.0f)), 0.60f);
                        C2D_SetTintMode(C2D_TintSolid);
                    } else if (starFlashModes[i] == 2) {
                        // Atlas 1: White highlight
                        C2D_PlainImageTint(&starTint, C2D_Color32(255, 255, 255, (u8)(exitAlpha * 255.0f)), 0.50f);
                        C2D_SetTintMode(C2D_TintSolid);
                    } else {
                        // Normal unlit star: use TintMult for normal texture rendering with alpha
                        C2D_AlphaImageTint(&starTint, exitAlpha);
                        C2D_SetTintMode(C2D_TintMult);
                    }
                    drawFrameCentered(starFrame, starCenterX, curStarY, 0.72f, &starTint, curStarScale, curStarScale);
                }
                C2D_SetTintMode(C2D_TintMult);
            }
        }

        // Draw selected song's health icon in the bottom-right corner (larger, with bounce)
        if (!fs.info.icon.empty()) {
            Frame iconF = getIconFrame(fs.info.icon);
            if (iconF.tex) {
                float logW = frameLogicalW(iconF);
                float logH = frameLogicalH(iconF);
                float iconScale = 1.5f;
                if (logW > 75.0f) {
                    iconScale = 70.0f / logW;
                }
                float iconW = logW * iconScale;
                float iconH = logH * iconScale;
                float defaultX = 320.0f - iconW - 15.0f;
                float defaultY = 240.0f - iconH - 28.0f;
                
                float iconX = defaultX + iconEggXOffset;
                float iconY = defaultY + iconBounceY + botIntroBottomY + iconEggYOffset;
                
                C2D_ImageTint iconTint;
                C2D_AlphaImageTint(&iconTint, exitAlpha);
                
                if (iconEggActive || iconEggScaleY != 1.0f || iconEggScaleX != 1.0f || iconRotation != 0.0f) {
                    float iconCenterX = iconX + iconW / 2.0f;
                    float iconCenterY = iconY + iconH / 2.0f;
                    C2D_Image img = { iconF.tex, &iconF.uv };
                    float totalAngle = iconRotation + (iconF.rotated ? -(3.14159265f / 2.0f) : 0.0f);
                    C2D_DrawImageAtRotated(img, iconCenterX, iconCenterY, 0.7f, totalAngle, &iconTint, iconScale * iconEggScaleX, iconScale * iconEggScaleY);
                } else {
                    drawFrameAt(iconF, iconX, iconY, 0.7f, &iconTint, iconScale, iconScale);
                }
            } else {
                float defaultX = 320.0f - 75.0f - 15.0f;
                float defaultY = 240.0f - 75.0f - 28.0f;
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
        const std::string& albumName = cachedAlbumName;
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
    float promptAlpha = renderAlpha;
    if (promptAlpha > 0.001f) {
        float promptY = 210.0f + 25.0f * (1.0f - introEaseOut) + (25.0f * exitT * exitT);
        float favWidth = cachedFavPromptWidth;
        ButtonPrompt::drawPrompt("y", "Favorite", 8.0f, promptY, 0.60f, promptAlpha);
        ButtonPrompt::drawPrompt("x", "Change Character", 8.0f + favWidth + 12.0f, promptY, 0.60f, promptAlpha);
    }
    }
}

void FreeplayState::exitState() {
    if (hasExited) return;
    hasExited = true;

    savedDifficulty = curDifficulty;
    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
        savedSongName = songs[curSelected].name;
    }
    if (curCategoryIdx >= 0 && curCategoryIdx < (int)categories.size()) {
        savedCategory = categories[curCategoryIdx];
    }

    if (freeplayBFSheet) C2D_SpriteSheetFree(freeplayBFSheet);
    freeplayBFSheet = nullptr;
    
    threadRunning = false;
    LightEvent_Signal(&loadEvent);
    if (loadThread) {
        threadJoin(loadThread, 2000000000ULL); // 2s safe timeout
        threadFree(loadThread);
        loadThread = nullptr;
    }

    LightLock_Lock(&loadLock);
    for (auto& data : completedDataQueue) {
        if (data.buffer) linearFree(data.buffer);
    }
    completedDataQueue.clear();
    pendingRequests.clear();
    LightLock_Unlock(&loadLock);

    clearAssetCache();
    
    if (highscoreSheet) C2D_SpriteSheetFree(highscoreSheet);
    highscoreSheet = nullptr;
    highscoreFrames.clear();

    for (int i = 0; i < 10; i++) numberFrames[i].clear();

    if (menuBgSheet) C2D_SpriteSheetFree(menuBgSheet);
    menuBgSheet = nullptr;

    if (freeplayIconsSheet) C2D_SpriteSheetFree(freeplayIconsSheet);
    freeplayIconsSheet = nullptr;
    freeplayIconsFrames.clear();

    if (freeplayDiffSheet) C2D_SpriteSheetFree(freeplayDiffSheet);
    freeplayDiffSheet = nullptr;
    freeplayDiffFrames.clear();

    if (freeplayAlbumsSheet) C2D_SpriteSheetFree(freeplayAlbumsSheet);
    freeplayAlbumsSheet = nullptr;
    freeplayAlbumsFrames.clear();

    letterStuffFrames.clear();
    fireLoopFrames.clear();
    starFrame = Frame();

    currentAlbumName = "";

    C2D_TextBufDelete(vcrFontBuf);
    vcrFontBuf = nullptr;
}

C2D_Image FreeplayState::getBfBackgroundImage() {
    if (bfBgFrame.tex) {
        return { bfBgFrame.tex, &bfBgFrame.uv };
    }
    return {nullptr, nullptr};
}

bool FreeplayState::hasBaseDiff(const std::string& name) {
    if (name.empty()) return false;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    for (const auto& f : freeplayDiffFrames) {
        if (f.name == lower) return true;
    }
    return false;
}

Frame FreeplayState::getDifficultyFrame(const std::string& name) {
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return Frame();
    std::string week = songs[curSelected].week;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    std::string modFolder = "";
    if (WeekData::weeksLoaded.count(week) && !WeekData::weeksLoaded[week].modFolder.empty()) {
        modFolder = WeekData::weeksLoaded[week].modFolder;
    }
    std::string assetScope = modFolder.empty() ? "base" : modFolder;

    // 1. Check custom mod difficulty in diffCache
    std::string fullKey = assetScope + ":" + lower;
    auto it = diffCache.find(fullKey);
    if (it == diffCache.end()) it = diffCache.find(week + ":" + lower);

    if (it != diffCache.end()) {
        it->second.lastAccessFrame = ++cacheFrameCount;
        if (it->second.sheet) {
            C2D_Image img = C2D_SpriteSheetGetImage(it->second.sheet, 0);
            if (img.tex) {
                Frame f = {};
                f.tex = img.tex;
                if (img.subtex) f.uv = *img.subtex;
                else f.uv = { (u16)img.tex->width, (u16)img.tex->height, 0.0f, 0.0f, 1.0f, 1.0f };
                f.w = f.uv.width;
                f.h = f.uv.height;
                f.frameW = f.w;
                f.frameH = f.h;
                f.rotated = false;
                return f;
            }
        } else {
            for (const auto& f : freeplayDiffFrames) {
                if (f.name == lower) return f;
            }
            return Frame();
        }
    }

    // 2. Base game difficulty atlas (immediate for base song when no global mods)
    bool isMod = WeekData::weeksLoaded.count(week) && WeekData::weeksLoaded[week].isMod;
    bool hasGlobal = false;
    for (const auto& m : ModHandler::get().getMods()) {
        if (m.active && m.runsGlobally) {
            hasGlobal = true;
            break;
        }
    }
    if (!isMod && !hasGlobal && hasBaseDiff(lower)) {
        for (const auto& f : freeplayDiffFrames) {
            if (f.name == lower) return f;
        }
    }

    return Frame();
}

C2D_Image FreeplayState::getDifficultyImage(const std::string& name) {
    Frame f = getDifficultyFrame(name);
    if (f.tex) {
        static Tex3DS_SubTexture sub;
        sub = f.uv;
        return { f.tex, &sub };
    }
    return {nullptr, nullptr};
}

C2D_Image FreeplayState::getCapsuleImage() {
    if (capsuleFrame.tex) {
        return { capsuleFrame.tex, &capsuleFrame.uv };
    }
    return {nullptr, nullptr};
}

void FreeplayState::updateDifficulties() {
    curWeekDiffs.clear();
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return;

    auto toLowerStr = [](const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)tolower((unsigned char)c);
        return out;
    };

    if (songs[curSelected].name == "Random") {
        curWeekDiffs = getAllUniqueDifficulties();
        std::string targetDiffLower = toLowerStr(lastDifficultyName);
        int found = -1;
        for (int i = 0; i < (int)curWeekDiffs.size(); ++i) {
            if (toLowerStr(curWeekDiffs[i]) == targetDiffLower) {
                found = i;
                break;
            }
        }
        curDifficulty = (found != -1) ? found : 0;
        return;
    }

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
                std::string cleanD = d.substr(first, last - first + 1);
                if (!cleanD.empty()) {
                    cleanD[0] = (char)toupper((unsigned char)cleanD[0]);
                    for (size_t k = 1; k < cleanD.size(); ++k) cleanD[k] = (char)tolower((unsigned char)cleanD[k]);
                    curWeekDiffs.push_back(cleanD);
                }
            }
        }
    } else {
        curWeekDiffs = {"Easy", "Normal", "Hard"};
    }

    std::string targetDiffLower = toLowerStr(lastDifficultyName);
    int normalIndex = -1;
    int lastDiffIndex = -1;
    for (int i = 0; i < (int)curWeekDiffs.size(); ++i) {
        std::string dLower = toLowerStr(curWeekDiffs[i]);
        if (dLower == targetDiffLower) {
            lastDiffIndex = i;
        }
        if (dLower == "normal") {
            normalIndex = i;
        }
    }

    if (lastDiffIndex != -1) {
        curDifficulty = lastDiffIndex;
    } else if (normalIndex != -1) {
        curDifficulty = normalIndex;
    } else if (!curWeekDiffs.empty()) {
        curDifficulty = 0;
    } else {
        curDifficulty = 0;
    }
    if (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) {
        lastDifficultyName = curWeekDiffs[curDifficulty];
    }

    std::string diff = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : lastDifficultyName;
    if (diff == "Easy" || diff == "easy") cachedCurrentDifficultyStr = "easy";
    else if (diff == "Hard" || diff == "hard") cachedCurrentDifficultyStr = "hard";
    else if (diff != "Normal" && diff != "normal") {
        cachedCurrentDifficultyStr = diff;
        for (char& c : cachedCurrentDifficultyStr) c = (char)tolower((unsigned char)c);
    } else {
        cachedCurrentDifficultyStr = "";
    }
    
    std::string curDiffStr = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : lastDifficultyName;
    cachedCurDiffLower = curDiffStr;
    for (char& c : cachedCurDiffLower) c = (char)tolower((unsigned char)c);
    cachedUpperDiffStr = curDiffStr;
    for (char& c : cachedUpperDiffStr) c = (char)toupper((unsigned char)c);
}

void FreeplayState::updateSelectedWeekText() {
    cachedWeekLines.lines.clear();
    cachedWeekLines.scale = 0.8f;
    cachedWeekLines.lineHeight = 16.0f;
    cachedWeekLines.startY = 120.0f;

    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return;
    const FreeplaySong& fs = songs[curSelected];
    if (fs.name == "Random") return;

    std::string weekDisplayName = fs.week;
    if (WeekData::weeksLoaded.find(fs.week) != WeekData::weeksLoaded.end()) {
        const WeekData& data = WeekData::weeksLoaded[fs.week];
        weekDisplayName = data.storyName.empty() ? data.weekName : data.storyName;
    }
    for (char& c : weekDisplayName) c = (char)toupper((unsigned char)c);

    float currentScale = 0.8f;
    float maxAlphaW = 165.0f;
    std::stringstream ss(weekDisplayName);
    std::string word;
    std::string currentLine = "";

    while (ss >> word) {
        std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
        float w = Alphabet::getTextWidth(testLine, currentScale);
        if (w > maxAlphaW && !currentLine.empty()) {
            cachedWeekLines.lines.push_back(currentLine);
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    if (!currentLine.empty()) {
        cachedWeekLines.lines.push_back(currentLine);
    }

    float maxLineWidth = 0.0f;
    for (const auto& line : cachedWeekLines.lines) {
        float w = Alphabet::getTextWidth(line, currentScale);
        if (w > maxLineWidth) maxLineWidth = w;
    }
    if (maxLineWidth > maxAlphaW && maxLineWidth > 0.0f) {
        currentScale *= (maxAlphaW / maxLineWidth);
    }

    cachedWeekLines.scale = currentScale;
    cachedWeekLines.lineHeight = 20.0f * currentScale;
    float totalHeight = (float)cachedWeekLines.lines.size() * cachedWeekLines.lineHeight;
    cachedWeekLines.startY = 120.0f - (totalHeight / 2.0f) + (cachedWeekLines.lineHeight * 0.15f);
}

bool FreeplayState::hasBaseIcon(const std::string& name) {
    if (name.empty()) return false;
    for (const auto& f : freeplayIconsFrames) {
        if (f.name == name || f.name == name + "pixel") return true;
        if (name.length() > 5 && name.rfind("pixel") == name.length() - 5) {
            if (f.name == name.substr(0, name.length() - 5)) return true;
        }
    }
    return false;
}

Frame FreeplayState::getIconFrame(const std::string& name) {
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return Frame();
    std::string week = songs[curSelected].week;

    std::string modFolder = "";
    if (WeekData::weeksLoaded.count(week) && !WeekData::weeksLoaded[week].modFolder.empty()) {
        modFolder = WeekData::weeksLoaded[week].modFolder;
    }
    std::string assetScope = modFolder.empty() ? "base" : modFolder;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    // 1. First, check if custom mod icon is in LRU cache
    std::string fullKey = assetScope + ":" + name;
    auto it = iconCache.find(fullKey);
    if (it == iconCache.end()) it = iconCache.find(assetScope + ":" + lower);
    if (it == iconCache.end()) it = iconCache.find(week + ":" + name);
    if (it == iconCache.end()) it = iconCache.find(week + ":" + lower);

    if (it != iconCache.end()) {
        it->second.lastAccessFrame = ++cacheFrameCount;
        if (it->second.sheet) {
            C2D_Image img = C2D_SpriteSheetGetImage(it->second.sheet, 0);
            if (img.tex) {
                Frame f = {};
                f.tex = img.tex;
                if (img.subtex) {
                    f.uv = *img.subtex;
                } else {
                    f.uv = { (u16)img.tex->width, (u16)img.tex->height, 0.0f, 0.0f, 1.0f, 1.0f };
                }
                if (it->second.isCharIcon && img.subtex != nullptr) {
                    float u0 = f.uv.left;
                    float du = f.uv.right - u0;
                    f.uv.width = f.uv.width / 2;
                    f.uv.right = u0 + du * 0.5f;
                }
                f.w = f.uv.width;
                f.h = f.uv.height;
                f.frameW = f.w;
                f.frameH = f.h;
                f.rotated = false;
                return f;
            }
        } else {
            for (const auto& f : freeplayIconsFrames) {
                if (f.name == name || f.name == name + "pixel") return f;
                if (name.length() > 5 && name.rfind("pixel") == name.length() - 5) {
                    if (f.name == name.substr(0, name.length() - 5)) return f;
                }
            }
            for (const auto& f : freeplayIconsFrames) {
                if (f.name == "placeholder") return f;
            }
            return Frame();
        }
    }

    // 2. Base game icons from unified atlas (immediate for base song when no global mods)
    bool isMod = WeekData::weeksLoaded.count(week) && WeekData::weeksLoaded[week].isMod;
    bool hasGlobal = false;
    for (const auto& m : ModHandler::get().getMods()) {
        if (m.active && m.runsGlobally) {
            hasGlobal = true;
            break;
        }
    }
    if (!isMod && !hasGlobal && hasBaseIcon(name)) {
        for (const auto& f : freeplayIconsFrames) {
            if (f.name == name || f.name == name + "pixel") return f;
            if (name.length() > 5 && name.rfind("pixel") == name.length() - 5) {
                if (f.name == name.substr(0, name.length() - 5)) return f;
            }
        }
    }

    // Still loading
    return Frame();
}

C2D_Image FreeplayState::getIconImage(const std::string& name) {
    Frame f = getIconFrame(name);
    if (f.tex) {
        static Tex3DS_SubTexture sub;
        sub = f.uv;
        return { f.tex, &sub };
    }
    return {nullptr, nullptr};
}

bool FreeplayState::hasBaseAlbum(const std::string& name) {
    if (name.empty()) return false;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    for (const auto& f : freeplayAlbumsFrames) {
        if (f.name == lower) return true;
    }
    return false;
}

Frame FreeplayState::getAlbumFrame(const std::string& name) {
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return Frame();
    std::string week = songs[curSelected].week;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    std::string modFolder = "";
    if (WeekData::weeksLoaded.count(week) && !WeekData::weeksLoaded[week].modFolder.empty()) {
        modFolder = WeekData::weeksLoaded[week].modFolder;
    }
    std::string assetScope = modFolder.empty() ? "base" : modFolder;

    // 1. Check custom mod album in LRU cache
    std::string fullKey = assetScope + ":" + lower;
    auto it = albumCache.find(fullKey);
    if (it == albumCache.end()) it = albumCache.find(week + ":" + lower);

    if (it != albumCache.end()) {
        it->second.lastAccessFrame = ++cacheFrameCount;
        if (it->second.sheet) {
            C2D_Image img = C2D_SpriteSheetGetImage(it->second.sheet, 0);
            if (img.tex) {
                Frame f = {};
                f.tex = img.tex;
                if (img.subtex) f.uv = *img.subtex;
                else f.uv = { (u16)img.tex->width, (u16)img.tex->height, 0.0f, 0.0f, 1.0f, 1.0f };
                f.w = f.uv.width;
                f.h = f.uv.height;
                f.frameW = f.w;
                f.frameH = f.h;
                f.rotated = false;
                return f;
            }
        } else {
            for (const auto& f : freeplayAlbumsFrames) {
                if (f.name == lower) return f;
            }
            for (const auto& f : freeplayAlbumsFrames) {
                if (f.name == "placeholder") return f;
            }
            return Frame();
        }
    }

    // 2. Base game album atlas (immediate for base song when no global mods)
    bool isMod = WeekData::weeksLoaded.count(week) && WeekData::weeksLoaded[week].isMod;
    bool hasGlobal = false;
    for (const auto& m : ModHandler::get().getMods()) {
        if (m.active && m.runsGlobally) {
            hasGlobal = true;
            break;
        }
    }
    if (!isMod && !hasGlobal && hasBaseAlbum(lower)) {
        for (const auto& f : freeplayAlbumsFrames) {
            if (f.name == lower) return f;
        }
    }

    // Still loading: return Frame() with tex = nullptr so rotating white square is drawn!
    return Frame();
}

Frame FreeplayState::getAlbumTextFrame(const std::string& name) {
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return Frame();
    std::string week = songs[curSelected].week;

    std::string textName = (name.length() >= 5 && name.substr(name.length() - 5) == "-text") ? name : (name + "-text");
    for (char& c : textName) c = (char)tolower((unsigned char)c);

    std::string modFolder = "";
    if (WeekData::weeksLoaded.count(week) && !WeekData::weeksLoaded[week].modFolder.empty()) {
        modFolder = WeekData::weeksLoaded[week].modFolder;
    }
    std::string assetScope = modFolder.empty() ? "base" : modFolder;

    // 1. Check custom mod album text in LRU cache
    std::string fullKey = assetScope + ":" + textName;
    auto it = albumTextCache.find(fullKey);
    if (it == albumTextCache.end()) it = albumTextCache.find(week + ":" + textName);

    if (it != albumTextCache.end()) {
        it->second.lastAccessFrame = ++cacheFrameCount;
        if (it->second.sheet) {
            C2D_Image img = C2D_SpriteSheetGetImage(it->second.sheet, 0);
            if (img.tex) {
                Frame f = {};
                f.tex = img.tex;
                if (img.subtex) f.uv = *img.subtex;
                else f.uv = { (u16)img.tex->width, (u16)img.tex->height, 0.0f, 0.0f, 1.0f, 1.0f };
                f.w = f.uv.width;
                f.h = f.uv.height;
                f.frameW = f.w;
                f.frameH = f.h;
                f.rotated = false;
                return f;
            }
        } else {
            for (const auto& f : freeplayAlbumsFrames) {
                if (f.name == textName) return f;
            }
            return Frame();
        }
    }

    // 2. Base game album atlas
    bool isMod = WeekData::weeksLoaded.count(week) && WeekData::weeksLoaded[week].isMod;
    bool hasGlobal = false;
    for (const auto& m : ModHandler::get().getMods()) {
        if (m.active && m.runsGlobally) {
            hasGlobal = true;
            break;
        }
    }
    if (!isMod && !hasGlobal && hasBaseAlbum(textName)) {
        for (const auto& f : freeplayAlbumsFrames) {
            if (f.name == textName) return f;
        }
    }

    return Frame();
}

C2D_Image FreeplayState::getAlbumImage(const std::string& name) {
    Frame f = getAlbumFrame(name);
    if (f.tex) {
        static Tex3DS_SubTexture sub;
        sub = f.uv;
        return { f.tex, &sub };
    }
    return {nullptr, nullptr};
}

C2D_Image FreeplayState::getAlbumTextImage(const std::string& name) {
    Frame f = getAlbumTextFrame(name);
    if (f.tex) {
        static Tex3DS_SubTexture sub;
        sub = f.uv;
        return { f.tex, &sub };
    }
    return {nullptr, nullptr};
}

std::string FreeplayState::getAlbumNameForSelected(int songIdx) {
    int idx = (songIdx >= 0) ? songIdx : curSelected;
    if (songs.empty() || idx < 0 || idx >= (int)songs.size()) {
        return "placeholder";
    }
    
    const FreeplaySong& fs = songs[idx];
    if (fs.name == "Random" || WeekData::weeksLoaded.find(fs.week) == WeekData::weeksLoaded.end()) {
        return "placeholder";
    }
    
    const WeekData& wd = WeekData::weeksLoaded[fs.week];
    
    std::string songNameLower = fs.name;
    for (char& c : songNameLower) c = (char)tolower((unsigned char)c);
    
    std::string diffLower = "";
    if (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) {
        diffLower = curWeekDiffs[curDifficulty];
        for (char& c : diffLower) c = (char)tolower((unsigned char)c);
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
    
    if (!fs.info.album.empty()) {
        return fs.info.album;
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

    auto toLowerStr = [](const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)tolower((unsigned char)c);
        return out;
    };

    std::string targetDiffLower = toLowerStr(lastDifficultyName);

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

        for (const auto& d : songDiffs) {
            if (toLowerStr(d) == targetDiffLower) {
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

    // Insert Random capsule at index 0 if we have playable songs
    if (!songs.empty()) {
        FreeplaySong randomSong;
        randomSong.name = "Random";
        randomSong.week = "";
        randomSong.info.name = "Random";
        randomSong.info.icon = "";
        randomSong.info.color[0] = 146.0f;
        randomSong.info.color[1] = 113.0f;
        randomSong.info.color[2] = 253.0f;
        songs.insert(songs.begin(), randomSong);
    }

    if (keepSelection && !previousSongName.empty()) {
        if (previousSongName == "Random") {
            curSelected = 0;
            lerpSelected = 0.0f;
        } else {
            // Try to find the previously selected song in the new list (from index 1 onward)
            int foundIdx = -1;
            for (int i = 1; i < (int)songs.size(); i++) {
                if (songs[i].name == previousSongName) {
                    foundIdx = i;
                    break;
                }
            }
            // If not found, try matching base song name prefix (e.g. Spookeez -> Spookeez Erect)
            if (foundIdx < 0) {
                std::string prevLower = toLowerStr(previousSongName);
                for (int i = 1; i < (int)songs.size(); i++) {
                    std::string candLower = toLowerStr(songs[i].name);
                    if (candLower.rfind(prevLower, 0) == 0) {
                        foundIdx = i;
                        break;
                    }
                }
            }
            // If still not found, clamp to closest valid song index (findClosestDiff)
            if (foundIdx < 0) {
                if (songs.size() > 1) {
                    foundIdx = std::max(1, std::min(curSelected, (int)songs.size() - 1));
                } else {
                    foundIdx = 0;
                }
            }
            curSelected = foundIdx;
            lerpSelected = (float)curSelected;
        }
    } else {
        curSelected = (songs.size() > 1) ? 1 : 0;
        lerpSelected = (float)curSelected;
    }

    updateDifficulties();

    // 1. Synchronize display properties for newly selected song in category
    if (!songs.empty() && curSelected >= 0 && curSelected < (int)songs.size()) {
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

        std::string newAlbum = songs[curSelected].week + ":" + getAlbumNameForSelected(curSelected);
        if (newAlbum != currentAlbumName) {
            currentAlbumName = newAlbum;
            albumRotation = 0.0f;
            albumTextFlashTime = 0.1f;
        }
        cachedAlbumName = getAlbumNameForSelected(curSelected);
        updateSelectedWeekText();

        std::string curDiffStr = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : "";
        targetStars = getStarsForSong(songs[curSelected], curDiffStr, curDifficulty);
    } else {
        targetStars = 0;
    }

    // 2. Clear stale async requests from previous category
    LightLock_Lock(&loadLock);
    pendingRequests.clear();
    for (auto& raw : completedDataQueue) {
        if (raw.buffer) {
            linearFree(raw.buffer);
            raw.buffer = nullptr;
        }
    }
    completedDataQueue.clear();
    LightLock_Unlock(&loadLock);

    // 3. Mark checks and immediately queue asset loads for new category's selected song
    lastSelectedCheck = curSelected;
    lastDifficultyCheck = curDifficulty;
    triggerAsyncLoad();
}

bool FreeplayState::hasSongsInCategory(const std::string& catName) {
    if (catName == "all") return true;

    auto toLowerStr = [](const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)tolower((unsigned char)c);
        return out;
    };
    std::string targetDiffLower = toLowerStr(lastDifficultyName);

    for (const auto& song : allSongs) {
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

        for (const auto& d : songDiffs) {
            if (toLowerStr(d) == targetDiffLower) {
                supportsDifficulty = true;
                break;
            }
        }
        if (!supportsDifficulty) continue;

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
    if (prefix == "P" || prefix == "GP") prefix = "p";
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
    if (rating == "GP") return C2D_Color32(0xFF, 0xD7, 0x00, alpha); // Gold
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
void FreeplayState::evictLruCacheIfNeeded() {
    while (iconCache.size() > MAX_ICON_CACHE) {
        auto oldest = iconCache.begin();
        for (auto it = iconCache.begin(); it != iconCache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        if (oldest != iconCache.end()) {
            if (oldest->second.sheet) C2D_SpriteSheetFree(oldest->second.sheet);
            iconCache.erase(oldest);
        }
    }

    while (albumCache.size() > MAX_ALBUM_CACHE) {
        auto oldest = albumCache.begin();
        for (auto it = albumCache.begin(); it != albumCache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        if (oldest != albumCache.end()) {
            if (oldest->second.sheet) C2D_SpriteSheetFree(oldest->second.sheet);
            albumCache.erase(oldest);
        }
    }

    while (albumTextCache.size() > MAX_ALBUM_TEXT_CACHE) {
        auto oldest = albumTextCache.begin();
        for (auto it = albumTextCache.begin(); it != albumTextCache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        if (oldest != albumTextCache.end()) {
            if (oldest->second.sheet) C2D_SpriteSheetFree(oldest->second.sheet);
            albumTextCache.erase(oldest);
        }
    }

    while (diffCache.size() > MAX_DIFF_CACHE) {
        auto oldest = diffCache.begin();
        for (auto it = diffCache.begin(); it != diffCache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        if (oldest != diffCache.end()) {
            if (oldest->second.sheet) C2D_SpriteSheetFree(oldest->second.sheet);
            diffCache.erase(oldest);
        }
    }
}

void FreeplayState::clearAssetCache() {
    for (auto& pair : iconCache) {
        if (pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet);
    }
    iconCache.clear();

    for (auto& pair : albumCache) {
        if (pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet);
    }
    albumCache.clear();

    for (auto& pair : albumTextCache) {
        if (pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet);
    }
    albumTextCache.clear();

    for (auto& pair : diffCache) {
        if (pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet);
    }
    diffCache.clear();
}

void FreeplayState::prunePendingRequests() {
    LightLock_Lock(&loadLock);
    int totalSongs = (int)songs.size();
    auto it = pendingRequests.begin();
    while (it != pendingRequests.end()) {
        int diff = std::abs(it->songIndex - curSelected);
        if (totalSongs > 0) {
            diff = diff % totalSongs;
            diff = std::min(diff, totalSongs - diff);
        }
        if (diff > 2) {
            it = pendingRequests.erase(it);
        } else {
            if (it->songIndex == curSelected) {
                it->priority = 0;
            } else {
                it->priority = diff * 2;
            }
            ++it;
        }
    }
    std::stable_sort(pendingRequests.begin(), pendingRequests.end(), [](const AsyncLoadRequest& a, const AsyncLoadRequest& b) {
        return a.priority < b.priority;
    });
    LightLock_Unlock(&loadLock);
}

void FreeplayState::queueAssetLoad(AsyncLoadRequest::AssetType type, const std::string& key, const AsyncLoadRequest& baseReq, int priority) {
    if (key.empty()) return;

    if (type == AsyncLoadRequest::AssetType::ICON) {
        if (iconCache.count(key) > 0) return;
    }
    else if (type == AsyncLoadRequest::AssetType::ALBUM) {
        if (albumCache.count(key) > 0) return;
    }
    else if (type == AsyncLoadRequest::AssetType::ALBUM_TEXT) {
        if (albumTextCache.count(key) > 0) return;
    }
    else if (type == AsyncLoadRequest::AssetType::DIFFICULTY) {
        if (diffCache.count(key) > 0) return;
    }

    LightLock_Lock(&loadLock);
    for (auto& req : pendingRequests) {
        if (req.type == type && req.key == key) {
            if (priority < req.priority) {
                req.priority = priority;
                req.songIndex = baseReq.songIndex;
            }
            LightLock_Unlock(&loadLock);
            return;
        }
    }

    AsyncLoadRequest req = baseReq;
    req.type = type;
    req.key = key;
    req.priority = priority;

    pendingRequests.push_back(req);

    std::stable_sort(pendingRequests.begin(), pendingRequests.end(), [](const AsyncLoadRequest& a, const AsyncLoadRequest& b) {
        return a.priority < b.priority;
    });

    LightLock_Unlock(&loadLock);
}

void FreeplayState::triggerAsyncLoad() {
    if (songs.empty() || curSelected < 0 || curSelected >= (int)songs.size()) return;

    int totalSongs = (int)songs.size();
    int offsets[] = {0, 1, -1, 2, -2};

    std::string diffName = (curDifficulty >= 0 && curDifficulty < (int)curWeekDiffs.size()) ? curWeekDiffs[curDifficulty] : "Normal";
    std::string diffNameLower = diffName;
    for (char& c : diffNameLower) c = (char)tolower((unsigned char)c);

    bool hasGlobal = false;
    for (const auto& m : ModHandler::get().getMods()) {
        if (m.active && m.runsGlobally) {
            hasGlobal = true;
            break;
        }
    }

    for (int i = 0; i < 5; i++) {
        int targetIdx = (curSelected + offsets[i] + totalSongs) % totalSongs;
        if (targetIdx < 0 || targetIdx >= totalSongs) continue;

        int prio = i;
        const FreeplaySong& fs = songs[targetIdx];

        AsyncLoadRequest baseReq;
        baseReq.songIndex = targetIdx;
        baseReq.songName = fs.name;
        baseReq.week = fs.week;
        baseReq.iconName = fs.info.icon;
        baseReq.albumName = getAlbumNameForSelected(targetIdx);
        baseReq.difficultyName = diffName;

        bool isMod = WeekData::weeksLoaded.count(fs.week) && WeekData::weeksLoaded[fs.week].isMod;
        std::string modFolder = "";
        if (isMod && !WeekData::weeksLoaded[fs.week].modFolder.empty()) {
            modFolder = WeekData::weeksLoaded[fs.week].modFolder;
        }
        std::string assetScope = modFolder.empty() ? "base" : modFolder;

        // 1. Difficulty sprite (curSelected only)
        if (offsets[i] == 0) {
            if (!isMod && !hasGlobal && hasBaseDiff(diffNameLower)) {
                // In memory atlas
            } else {
                std::string diffKey = assetScope + ":" + diffNameLower;
                queueAssetLoad(AsyncLoadRequest::AssetType::DIFFICULTY, diffKey, baseReq, prio);
            }
        }

        // 2. Icon sprite
        std::string iconToLoad = fs.info.icon.empty() ? "face" : fs.info.icon;
        baseReq.iconName = iconToLoad;
        if (!isMod && !hasGlobal && hasBaseIcon(iconToLoad)) {
            // In memory atlas
        } else {
            std::string iconKey = assetScope + ":" + iconToLoad;
            queueAssetLoad(AsyncLoadRequest::AssetType::ICON, iconKey, baseReq, prio);
        }

        // 3. Album sprite & text
        std::string albumToLoad = baseReq.albumName.empty() ? "placeholder" : baseReq.albumName;
        std::string albumToLoadLower = albumToLoad;
        for (char& c : albumToLoadLower) c = (char)tolower((unsigned char)c);
        baseReq.albumName = albumToLoad;

        if (!isMod && !hasGlobal && hasBaseAlbum(albumToLoadLower)) {
            // In memory atlas
        } else {
            std::string albumKey = assetScope + ":" + albumToLoadLower;
            queueAssetLoad(AsyncLoadRequest::AssetType::ALBUM, albumKey, baseReq, prio);
        }

        std::string textName = (albumToLoad.length() >= 5 && albumToLoad.substr(albumToLoad.length() - 5) == "-text") ? albumToLoad : (albumToLoad + "-text");
        std::string textNameLower = textName;
        for (char& c : textNameLower) c = (char)tolower((unsigned char)c);

        if (!isMod && !hasGlobal && hasBaseAlbum(textNameLower)) {
            // In memory atlas
        } else {
            std::string albumTextKey = assetScope + ":" + textNameLower;
            queueAssetLoad(AsyncLoadRequest::AssetType::ALBUM_TEXT, albumTextKey, baseReq, prio);
        }
    }

    LightEvent_Signal(&loadEvent);
}

FreeplayState::LoadedRawData FreeplayState::loadRawFile(AsyncLoadRequest& req) {
    LoadedRawData data;

    std::string path = "";
    bool isChar = false;

    std::string modFolder = "";

    if (WeekData::weeksLoaded.count(req.week) && !WeekData::weeksLoaded[req.week].modFolder.empty()) {
        modFolder = WeekData::weeksLoaded[req.week].modFolder;
    }
    if (modFolder.empty()) {
        std::string songLower = req.songName;
        for (char& c : songLower) c = (char)tolower((unsigned char)c);
        modFolder = ModHandler::get().getModFolderOfFile("data/" + songLower + "/" + songLower + ".json");
    }

    std::string base = ModHandler::getWorkingBase();
    if (base.empty()) {
        #ifdef _3DS
        base = "sdmc:/SnakeEngine/";
        #else
        base = "SnakeEngine/";
        #endif
    }
    if (!base.empty() && base.back() != '/') base += "/";

    auto checkFile = [&](const std::string& rel) -> std::string {
        if (!modFolder.empty()) {
            std::string p = base + modFolder + "/" + rel;
            if (Paths::fileExists(p)) return p;
        }
        return ModHandler::get().getModPath(rel);
    };

    if (req.type == AsyncLoadRequest::AssetType::DIFFICULTY) {
        std::string diffNameLower = req.difficultyName;
        for (char& c : diffNameLower) c = (char)tolower((unsigned char)c);

        // Check mod-specific paths first
        std::string modPath = checkFile("images/freeplay/" + diffNameLower + ".t3x");
        if (modPath.empty()) modPath = checkFile("images/freeplay/" + diffNameLower + ".rawtex");
        if (modPath.empty()) modPath = checkFile("images/freeplay/" + diffNameLower + ".png");
        if (modPath.empty()) modPath = checkFile("images/freeplay/" + req.difficultyName + ".t3x");
        if (modPath.empty()) modPath = checkFile("images/freeplay/" + req.difficultyName + ".rawtex");
        if (modPath.empty()) modPath = checkFile("images/freeplay/" + req.difficultyName + ".png");

        if (modPath.empty()) modPath = checkFile("images/freeplay/diff/" + diffNameLower + ".t3x");
        if (modPath.empty()) modPath = checkFile("images/freeplay/diff/" + diffNameLower + ".rawtex");
        if (modPath.empty()) modPath = checkFile("images/freeplay/diff/" + diffNameLower + ".png");
        if (modPath.empty()) modPath = checkFile("images/freeplay/diff/" + req.difficultyName + ".t3x");
        if (modPath.empty()) modPath = checkFile("images/freeplay/diff/" + req.difficultyName + ".rawtex");
        if (modPath.empty()) modPath = checkFile("images/freeplay/diff/" + req.difficultyName + ".png");

        if (modPath.empty()) modPath = checkFile("images/menudifficulties/" + diffNameLower + ".t3x");
        if (modPath.empty()) modPath = checkFile("images/menudifficulties/" + diffNameLower + ".rawtex");
        if (modPath.empty()) modPath = checkFile("images/menudifficulties/" + diffNameLower + ".png");
        if (modPath.empty()) modPath = checkFile("images/menudifficulties/" + req.difficultyName + ".t3x");
        if (modPath.empty()) modPath = checkFile("images/menudifficulties/" + req.difficultyName + ".rawtex");
        if (modPath.empty()) modPath = checkFile("images/menudifficulties/" + req.difficultyName + ".png");

        if (!modPath.empty()) {
            path = modPath;
        } else if (!hasBaseDiff(diffNameLower)) {
            path = Paths::image("freeplay/" + diffNameLower);
            if (!Paths::fileExists(path)) path = Paths::image("freeplay/" + req.difficultyName);
            if (!Paths::fileExists(path)) path = Paths::image("freeplay/diff/" + diffNameLower);
            if (!Paths::fileExists(path)) path = Paths::image("freeplay/diff/" + req.difficultyName);
            if (!Paths::fileExists(path)) path = Paths::image("menudifficulties/" + diffNameLower);
            if (!Paths::fileExists(path)) path = Paths::image("menudifficulties/" + req.difficultyName);
            if (!Paths::fileExists(path)) path.clear();
        } else {
            path.clear();
        }
    }
    else if (req.type == AsyncLoadRequest::AssetType::ICON) {
        std::string iconName = req.iconName.empty() ? "face" : req.iconName;

        path = checkFile("images/freeplayIcons/" + iconName + ".t3x");
        if (path.empty()) path = checkFile("images/freeplayIcons/" + iconName + ".rawtex");
        if (path.empty()) path = checkFile("images/freeplayIcons/" + iconName + ".png");

        if (!path.empty()) {
            isChar = false;
        } else {
            path = Paths::image("freeplayIcons/" + iconName);
            if (Paths::fileExists(path)) {
                isChar = false;
            } else {
                path = Paths::healthIcon(iconName);
                if (Paths::fileExists(path)) {
                    isChar = true;
                } else if (!hasBaseIcon(iconName)) {
                    path = Paths::image("freeplayIcons/placeholder", "preload");
                    isChar = false;
                    if (!Paths::fileExists(path)) {
                        path = Paths::healthIcon("face");
                        isChar = true;
                    }
                } else {
                    path.clear();
                }
            }
        }
    }
    else if (req.type == AsyncLoadRequest::AssetType::ALBUM) {
        std::string albumName = req.albumName.empty() ? "placeholder" : req.albumName;
        std::string albumLower = albumName;
        for (char& c : albumLower) c = (char)tolower((unsigned char)c);

        bool albumFound = false;
        if (albumLower != "placeholder") {
            std::string modImg = checkFile("images/freeplay/album/" + albumLower + ".t3x");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + albumLower + ".rawtex");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + albumLower + ".png");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + albumName + ".t3x");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + albumName + ".rawtex");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + albumName + ".png");
            if (!modImg.empty()) {
                path = modImg;
                albumFound = true;
            }
        }
        if (!albumFound && !modFolder.empty()) {
            std::string p = base + modFolder + "/pack.t3x";
            if (Paths::fileExists(p)) { path = p; albumFound = true; }
            if (!albumFound) { p = base + modFolder + "/pack.rawtex"; if (Paths::fileExists(p)) { path = p; albumFound = true; } }
            if (!albumFound) { p = base + modFolder + "/pack.png"; if (Paths::fileExists(p)) { path = p; albumFound = true; } }
            if (!albumFound) { p = base + modFolder + "/images/pack.t3x"; if (Paths::fileExists(p)) { path = p; albumFound = true; } }
            if (!albumFound) { p = base + modFolder + "/images/pack.rawtex"; if (Paths::fileExists(p)) { path = p; albumFound = true; } }
            if (!albumFound) { p = base + modFolder + "/images/pack.png"; if (Paths::fileExists(p)) { path = p; albumFound = true; } }
        }
        if (!albumFound) {
            if (hasBaseAlbum(albumLower)) {
                path.clear();
            } else {
                path = Paths::image("freeplay/album/" + albumName);
                if (!Paths::fileExists(path)) path = Paths::image("freeplay/album/" + albumLower);
                if (!Paths::fileExists(path)) {
                    path.clear();
                }
            }
        }
    }
    else if (req.type == AsyncLoadRequest::AssetType::ALBUM_TEXT) {
        std::string albumName = req.albumName.empty() ? "placeholder" : req.albumName;
        std::string albumLower = albumName;
        for (char& c : albumLower) c = (char)tolower((unsigned char)c);

        std::string textNameLower = (albumLower.length() >= 5 && albumLower.substr(albumLower.length() - 5) == "-text") ? albumLower : (albumLower + "-text");
        std::string textName = (albumName.length() >= 5 && albumName.substr(albumName.length() - 5) == "-text") ? albumName : (albumName + "-text");

        bool textFound = false;
        if (albumLower != "placeholder") {
            std::string modImg = checkFile("images/freeplay/album/" + textNameLower + ".t3x");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + textNameLower + ".rawtex");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + textNameLower + ".png");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + textName + ".t3x");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + textName + ".rawtex");
            if (modImg.empty()) modImg = checkFile("images/freeplay/album/" + textName + ".png");
            if (!modImg.empty()) {
                path = modImg;
                textFound = true;
            }
        }
        if (!textFound) {
            if (hasBaseAlbum(textNameLower)) {
                path.clear();
            } else {
                path = Paths::image("freeplay/album/" + textNameLower);
                if (!Paths::fileExists(path)) path = Paths::image("freeplay/album/" + textName);
                if (!Paths::fileExists(path)) path.clear();
            }
        }
    }

    req.resolvedPath = path;
    req.iconIsChar = isChar;
    data.req = req;

    return data;
}

void FreeplayState::threadMain(void* arg) {
    FreeplayState* state = (FreeplayState*)arg;
    while (state->threadRunning) {
        AsyncLoadRequest req;
        bool hasRequest = false;

        LightLock_Lock(&state->loadLock);
        if (!state->pendingRequests.empty()) {
            req = state->pendingRequests.front();
            state->pendingRequests.erase(state->pendingRequests.begin());
            hasRequest = true;
        }
        LightLock_Unlock(&state->loadLock);

        if (!hasRequest) {
            LightEvent_Wait(&state->loadEvent);
            continue;
        }

        LoadedRawData raw = state->loadRawFile(req);
        LightLock_Lock(&state->loadLock);
        state->completedDataQueue.push_back(raw);
        LightLock_Unlock(&state->loadLock);
    }
}


int FreeplayState::getStarsForSong(const FreeplaySong& fs, const std::string& diffName, int diffIdx) {
    if (fs.name == "Random") return 0;
    int s = fs.info.getStars(diffName, diffIdx);
    if (s < 0) s = 0;
    if (s > 30) s = 30;
    return s;
}

Frame FreeplayState::getFireFrame(float timer) {
    if (fireLoopFrames.empty()) return Frame();
    if (fireLoopFrames.size() < 10) return fireLoopFrames[0];

    // Frame duration at 24 fps is 1.0f / 24.0f (~0.04167s)
    // Intro consists of the first 2 frames: index 0 and index 1
    const float introDuration = 2.0f / 24.0f;
    if (timer < introDuration) {
        int idx = (int)(timer * 24.0f);
        if (idx < 0) idx = 0;
        if (idx > 1) idx = 1;
        return fireLoopFrames[idx];
    } else {
        // Once the 2-frame intro finishes, loop over frames 2..9 (8 frames),
        // never returning to frames 0 or 1 during the loop.
        float loopTime = timer - introDuration;
        int loopIdx = (int)(loopTime * 24.0f) % 8;
        return fireLoopFrames[2 + loopIdx];
    }
}

void FreeplayState::updateStarChain(float dt) {
    // 1. Advance individual star slot timers and animations
    for (int i = 0; i < 10; i++) {
        if (starSlots[i].flameTier > 0) {
            starSlots[i].flameTimer += dt;
        }
        if (starSlots[i].isExtinguishing) {
            starSlots[i].extinguishTimer += dt;
            if (starSlots[i].extinguishTimer >= (2.0f / 24.0f)) {
                starSlots[i].isExtinguishing = false;
            }
        }
        if (starSlots[i].spawnScale > 1.0f) {
            starSlots[i].spawnScale += (1.0f - starSlots[i].spawnScale) * (1.0f - exp2f(-20.0f * dt));
            if (starSlots[i].spawnScale < 1.001f) starSlots[i].spawnScale = 1.0f;
        }
    }

    // 2. Chain stepper towards targetStars
    if (displayStars != targetStars) {
        int diff = std::abs(targetStars - displayStars);
        float stepDelay = 0.04f;
        if (diff > 10) {
            stepDelay = 0.40f / (float)diff;
        }

        starChainTimer += dt;
        while (starChainTimer >= stepDelay && displayStars != targetStars) {
            starChainTimer -= stepDelay;
            if (displayStars < targetStars) {
                displayStars++;
                if (displayStars <= 10) {
                    int slot = displayStars - 1;
                    starSlots[slot].visible = true;
                    starSlots[slot].spawnScale = 1.35f;
                    starSlots[slot].flameTier = 0;
                    starSlots[slot].isExtinguishing = false;
                } else if (displayStars <= 20) {
                    int slot = displayStars - 11;
                    starSlots[slot].visible = true;
                    starSlots[slot].flameTier = 1;
                    starSlots[slot].flameTimer = 0.0f; // Intro frame 0 then 1 then loop
                    starSlots[slot].isExtinguishing = false;
                } else { // 21..30
                    int slot = displayStars - 21;
                    starSlots[slot].visible = true;
                    starSlots[slot].flameTier = 2;
                    starSlots[slot].flameTimer = 0.0f; // Intro frame 0 then 1 then loop
                    starSlots[slot].isExtinguishing = false;
                }
            } else { // displayStars > targetStars
                int oldDisplay = displayStars;
                displayStars--;
                if (oldDisplay >= 21) {
                    int slot = oldDisplay - 21;
                    // Red flame downgrades to blue flame: starts from 2 intro frames
                    starSlots[slot].visible = true;
                    starSlots[slot].flameTier = 1;
                    starSlots[slot].flameTimer = 0.0f;
                    starSlots[slot].isExtinguishing = false;
                } else if (oldDisplay >= 11) {
                    int slot = oldDisplay - 11;
                    // Blue flame loses flame completely: plays 2 intro frames in reverse, then flame is removed
                    starSlots[slot].flameTier = 0;
                    starSlots[slot].isExtinguishing = true;
                    starSlots[slot].extinguishTier = 1;
                    starSlots[slot].extinguishTimer = 0.0f;
                } else { // 1..10
                    int slot = oldDisplay - 1;
                    starSlots[slot].visible = false;
                    starSlots[slot].flameTier = 0;
                    starSlots[slot].isExtinguishing = false;
                }
            }
        }
    } else {
        starChainTimer = 0.0f;
    }
}
