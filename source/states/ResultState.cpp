#include "ResultState.hpp"
#include "StoryMenuState.hpp"
#include "FreeplayState.hpp"
#include "../debug/DebugMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "Highscores.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>

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

static void applyAntialiasing(C2D_SpriteSheet sheet) {
    if (!sheet) return;
    C2D_Image img = C2D_SpriteSheetGetImage(sheet, 0);
    if (img.tex) {
        C3D_TexSetFilter(img.tex,
                         ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                         ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
    }
}

static void applyAntialiasing(CachedSpritesheet* sheet) {
    if (!sheet || sheet->frames.empty()) return;
    C3D_Tex* tex = sheet->frames.front().tex;
    if (tex) {
        C3D_TexSetFilter(tex,
                         ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                         ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
    }
}

// Number stats tuning
static constexpr float kStatScale        = 0.7f;
static constexpr float kDigitSpacing     = 1.0f;
static constexpr float kStatStartX       = 145.0f;
static constexpr float kStatStartY       = 15.0f;
static constexpr float kStatRowStep      = 23.0f;
static constexpr float kStatsDelay       = 0.15f;
static constexpr float kStatCountDuration = 0.25f;
static constexpr float kZeroStatDelay    = 0.15f;

// Per-row X offsets
static constexpr float kOffsetTotalNotes = 0.0f;
static constexpr float kOffsetMaxCombo   = 0.0f;
static constexpr float kOffsetSicks      = -50.0f;
static constexpr float kOffsetGoods      = -55.0f;
static constexpr float kOffsetBads       = -65.0f;
static constexpr float kOffsetShits      = -60.0f;
static constexpr float kOffsetMisses     = -40.0f;

// Score digit tuning
static constexpr float kScoreDigitScale   = 0.6f;
static constexpr float kScoreDigitSpacing = -12.0f;
static constexpr float kScoreRowRightX    = 300.0f;
static constexpr float kScoreRowY         = 210.0f;


static void drawSmallNumber(const std::map<char, Frame>& numSmallFrames, int num, float x, float y, float depth, u32 color, float scale) {
    if (num < 0) num = 0;
    std::string str = std::to_string(num);
    float cursorX = x;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            auto it = numSmallFrames.find(c);
            if (it != numSmallFrames.end()) {
                C2D_ImageTint tint;
                C2D_PlainImageTint(&tint, color, 1.0f);
                C2D_SetTintMode(C2D_TintMult);
                drawFrameAt(it->second, cursorX, y, depth, &tint, scale, scale);
                C2D_SetTintMode(C2D_TintSolid);
                cursorX += frameLogicalW(it->second) * scale + kDigitSpacing;
            }
        }
    }
}

// Debug simulation — set kResultDebug = true to preview any score without playing a song
static constexpr bool kResultDebug      = false;

static constexpr long long kDbgScore    = 99999999999;
static constexpr int  kDbgTotalNotes    = 800;
static constexpr int  kDbgMaxCombo      = 750;
static constexpr int  kDbgSicks         = 600;
static constexpr int  kDbgGoods         = 120;
static constexpr int  kDbgBads          = 50;
static constexpr int  kDbgShits         = 20;
static constexpr int  kDbgMisses        = 10;

ResultState::ResultState(bool isStoryMode, bool fromDebug, const std::string& name, const std::string& difficulty,
    int totalNotes, int maxCombo, int sicks, int goods, int bads, int shits, int misses, long long score)
    : isStoryMode(isStoryMode), fromDebug(fromDebug), name(name), difficulty(difficulty),
      totalNotes(totalNotes), maxCombo(maxCombo), sicks(sicks), goods(goods), bads(bads), shits(shits), misses(misses), score(score) {

    if (kResultDebug) {
        this->score      = kDbgScore;
        this->totalNotes = kDbgTotalNotes;
        this->maxCombo   = kDbgMaxCombo;
        this->sicks      = kDbgSicks;
        this->goods      = kDbgGoods;
        this->bads       = kDbgBads;
        this->shits      = kDbgShits;
        this->misses     = kDbgMisses;
    }

    float totalPossible = totalNotes * 1.0f;
    if (totalPossible > 0.0f) {
        percentage = ((float)(sicks + goods) / totalPossible) * 100.0f;
    } else {
        percentage = 0.0f;
    }

    // Round percentage for display
    percentage = roundf(percentage);

    // Final rating
    if (misses == 0 && bads == 0 && shits == 0) {
        finalRating = "P"; // Perfect
    } else if (percentage >= 90.0f) {
        finalRating = "E"; // Excellent
    } else if (percentage >= 80.0f) {
        finalRating = "G"; // Great
    } else if (percentage >= 60.0f) {
        finalRating = "G"; // Good
    } else {
        finalRating = "L"; // Loss
    }

    if (misses == 0 && bads == 0 && shits == 0) {
        rank = "PERFECT";
    } else if (misses == 0) {
        rank = "FULL COMBO";
    } else {
        rank = "CLEAR";
    }

    Highscores::saveScore(name, (int)this->score, difficulty);
    Highscores::saveAccuracy(name, percentage, difficulty);
    std::string ratingKey = finalRating;
    if (finalRating == "G" && percentage < 80.0f) {
        ratingKey = "g"; // lowercase g for Good
    }
    Highscores::saveRating(name, ratingKey, difficulty, percentage);
}

void ResultState::init() {
    VCRFontFix();
    timer = 0.0f;
    
    // Load achievements spritesheet if needed
    if (!Achievements::sessionUnlocks.empty()) {
        std::string achSheetPath = "romfs:/preload/images/menus/achievements.t3x";
        if (!Paths::fileExists(achSheetPath)) achSheetPath = "romfs:/preload/images/menus/achievements.png";
        achievementsSheet = C2D_SpriteSheetLoad(achSheetPath.c_str());
        if (achievementsSheet) {
            achievementsImage = C2D_SpriteSheetGetImage(achievementsSheet, 0);
            if (achievementsImage.tex) applyAntialiasing(achievementsSheet);
            SparrowParser::parseXml("romfs:/preload/images/menus/achievements.xml", achievementsFrames);
            
            float rw = achievementsImage.subtex->right - achievementsImage.subtex->left;
            float rh = achievementsImage.subtex->bottom - achievementsImage.subtex->top;
            for (auto& f : achievementsFrames) {
                f.tex = achievementsImage.tex;
                f.uv.width = (u16)f.w;
                f.uv.height = (u16)f.h;
                f.uv.left = achievementsImage.subtex->left + ((float)f.x * rw / (float)achievementsImage.subtex->width);
                f.uv.top = achievementsImage.subtex->top + ((float)f.y * rh / (float)achievementsImage.subtex->height);
                f.uv.right = achievementsImage.subtex->left + ((float)(f.x + f.w) * rw / (float)achievementsImage.subtex->width);
                f.uv.bottom = achievementsImage.subtex->top + ((float)(f.y + f.h) * rh / (float)achievementsImage.subtex->height);
            }
        }
    }

    resultsSheet = SpritesheetCache::get().load("preload/images/results/rankAssets1");
    resultsAnimFrames.clear();
    resultsCurFrame = 0;
    resultsFrameTime = 0.0f;
    soundSystemFrameIdx = -1;
    tallyAnimFrames.clear();
    tallyCurFrame = 0;
    tallyFrameTime = 0.0f;
    tallyAnimDone = false;
    if (resultsSheet && !resultsSheet->frames.empty()) {
        for (const auto& f : resultsSheet->frames) {
            if (f.name.find("results instance ") == 0) {
                resultsAnimFrames.push_back(f.index);
            }
            if (f.name.find("sound system") == 0) {
                soundSystemFrameIdx = f.index;
            }
            if (f.name.find("tally score") == 0) {
                tallyAnimFrames.push_back(f.index);
            }
        }
        applyAntialiasing(resultsSheet);
    }

    categoriesSheet = SpritesheetCache::get().load("preload/images/results/rankAssets2");
    categoriesAnimFrames.clear();
    categoriesCurFrame = 0;
    categoriesFrameTime = 0.0f;
    categoriesStarted = false;
    categoriesAnimDone = false;
    for (int i = 0; i < 10; i++) {
        scoreDigitFrames[i].clear();
        slotAnimFrame[i] = 0;
        slotAnimTimer[i] = 0.0f;
        slotPrevDigit[i] = -1;
    }
    goneFrame.tex = nullptr;
    disabledFrame.tex = nullptr;
    holyScoreMode  = false;
    holyScoreTimer = 0.0f;
    scoreDisplayValue = 0;
    scoreCountTimer   = 0.0f;
    clearPercentFrame.tex = nullptr;
    for (int i = 0; i < 10; i++) pctNumberFrames[i].tex = nullptr;
    pctFadeAlpha = 1.0f;
    pctBlinkTimer = 0.0f;
    pctBlinkState = false;

    if (categoriesSheet && !categoriesSheet->frames.empty()) {
        static const std::string dPrefixes[10] = {
            "ZERO DIGITAL", "ONE DIGITAL", "TWO DIGITAL", "THREE DIGITAL", "FOUR DIGITAL",
            "FIVE DIGITAL", "SIX DIGITAL", "SEVEN DIGITAL", "EIGHT DIGITAL", "NINE DIGITAL"
        };
        for (const auto& f : categoriesSheet->frames) {
            if (f.name.find("Categories") == 0) {
                categoriesAnimFrames.push_back(f.index);
            }
            if (f.name == "clearPercentText") {
                clearPercentFrame = f;
            }
            for (int d = 0; d < 10; d++) {
                std::string numName = "number " + std::to_string(d) + " 0001";
                if (f.name == numName) {
                    pctNumberFrames[d] = f;
                }
            }
            for (int d = 0; d < 10; d++) {
                if (f.name.find(dPrefixes[d]) == 0) {
                    scoreDigitFrames[d].push_back(f);
                    break;
                }
            }
            if (f.name == "GONE0001") goneFrame = f;
            if (f.name == "DISABLED0001") disabledFrame = f;
        }
        applyAntialiasing(categoriesSheet);
    }
    // Detect holy-score overflow (score won't fit in 10 digits)
    holyScoreMode = (score > 9999999999LL);

    textSheet = SpritesheetCache::get().load("preload/images/results/rankAssetsText");
    numSmallFrames.clear();
    if (textSheet && !textSheet->frames.empty()) {
        for (const auto& f : textSheet->frames) {
            if (f.name.size() > 2 && f.name[1] == ' ' && f.name[2] == 's') {
                char digit = f.name[0];
                if (digit >= '0' && digit <= '9') {
                    numSmallFrames[digit] = f;
                }
            }
        }
        printf("[ResultState] Loaded %zu small digit frames\n", numSmallFrames.size());
        applyAntialiasing(textSheet);
    }

    numberStats.clear();
    numberStats.push_back({totalNotes, 0, CWhite,                             kStatStartX + kOffsetTotalNotes, kOffsetTotalNotes, kStatStartY + 0 * kStatRowStep, kStatScale, false, false});
    numberStats.push_back({maxCombo,   0, CWhite,                             kStatStartX + kOffsetMaxCombo,   kOffsetMaxCombo,   kStatStartY + 1 * kStatRowStep, kStatScale, false, false});
    numberStats.push_back({sicks,      0, C2D_Color32(0x90, 0xF4, 0xA2, 255), kStatStartX + kOffsetSicks,      kOffsetSicks,      kStatStartY + 2 * kStatRowStep, kStatScale, false, false});
    numberStats.push_back({goods,      0, C2D_Color32(0x8F, 0xCD, 0xE6, 255), kStatStartX + kOffsetGoods,      kOffsetGoods,      kStatStartY + 3 * kStatRowStep, kStatScale, false, false});
    numberStats.push_back({bads,       0, C2D_Color32(0xE6, 0xD0, 0x8F, 255), kStatStartX + kOffsetBads,       kOffsetBads,       kStatStartY + 4 * kStatRowStep, kStatScale, false, false});
    numberStats.push_back({shits,      0, C2D_Color32(0xE3, 0x82, 0x8C, 255), kStatStartX + kOffsetShits,      kOffsetShits,      kStatStartY + 5 * kStatRowStep, kStatScale, false, false});
    numberStats.push_back({misses,     0, C2D_Color32(0xC2, 0x7D, 0xE2, 255), kStatStartX + kOffsetMisses,     kOffsetMisses,     kStatStartY + 6 * kStatRowStep, kStatScale, false, false});
    // Find the correct background scrolling text frame based on rating
    std::string scrollTargetName = "rankScrollGOOD"; // default fallback
    std::string scrollTextTargetName = "rankTextGOOD";
    if (finalRating == "P") {
        scrollTargetName = "rankScrollPERFECT";
        scrollTextTargetName = "rankTextPERFECT";
    } else if (finalRating == "E") {
        scrollTargetName = "rankScrollEXCELLENT";
        scrollTextTargetName = "rankTextEXCELLENT";
    } else if (finalRating == "G" && percentage >= 80.0f) {
        scrollTargetName = "rankScrollGREAT";
        scrollTextTargetName = "rankTextGREAT";
    } else if (finalRating == "G") {
        scrollTargetName = "rankScrollGOOD";
        scrollTextTargetName = "rankTextGOOD";
    } else {
        scrollTargetName = "rankScrollLOSS";
        scrollTextTargetName = "rankTextLOSS";
    }

    bgScrollFrame.tex = nullptr;
    bgTextVerticalFrame.tex = nullptr;
    if (resultsSheet) {
        for (const auto& f : resultsSheet->frames) {
            if (f.name == scrollTargetName) {
                bgScrollFrame = f;
            }
            if (f.name == scrollTextTargetName) {
                bgTextVerticalFrame = f;
            }
        }
    }

    bgScrollOffset = 0.0f;
    bgFadeAlpha = 0.0f;
    flashAlpha = 0.0f;
    flashTriggered = false;
 
    achEnterTimer = 0.0f;
    achScrollOffset = 0.0f;

    currentStatIndex = 0;
    statsStarted = false;
    tallyStarted = false;
    statsTimer = 0.0f;

    musicIntroPath = "";
    musicLoopPath = "";
    musicIntroPlaying = false;
    musicLoopPlaying = false;
    musicStarted = false;
    musicIntroDurationMs = 0.0;
    musicIntroElapsed = 0.0f;
 
    rankAnim = RankAnim();
    rankAnim.started = false;
    rankAnim.introPlaying = false;
    rankAnim.curFrame = 0;
    rankAnim.frameTimer = 0.0f;
    gfOutPlaying = false;
    gfOutX = -200.0f;
    gfOutTimer = 0.0f;
    gfOutFrame = 0;
    gfOutFrameTimer = 0.0f;
    gfOffsetY = 0.0f;
    gfScale = 0.25f;
    gfFps = 12.0f;
    heartsTimer = 0.0f;
    heartsFrame = 0;
    heartsFrameTimer = 0.0f;
    heartsScale = 1.0f;
    heartsOffsetX = 0.0f;
    heartsOffsetY = 0.0f;
    heartsFps = 12.0f;
    goodGfIntroPlaying = false;
    goodGfAlpha = 0.0f;
    goodGfFps = 12.0f;
    goodGfOffsetX = 0.0f;
    goodGfOffsetY = 0.0f;
    goodGfScale = 0.25f;
    goodGfCurFrame = 0;
    goodGfFrameTimer = 0.0f;
    lossOffsetY = 0.0f;
    lossBaseH = 0.0f;

    if (finalRating == "P") {
        rankAnim.scale = 1.2f;
        rankAnim.offsetY = 20.0f;
        rankAnim.fps = 24.0f;
        heartsScale = 1.2f;
        heartsOffsetX = -40.0f;
        heartsOffsetY = 60.0f;
        heartsFps = 12.0f;
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/perfect/introPart1");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) {
                    int frameNum = -1;
                    sscanf(f.name.c_str(), "New animation%d", &frameNum);
                    if (frameNum >= 0 && frameNum <= 83) rankAnim.introFrames.push_back(f);
                }
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/perfect/introPart2");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) {
                    int frameNum = -1;
                    sscanf(f.name.c_str(), "New animation%d", &frameNum);
                    if (frameNum >= 0 && frameNum <= 83) rankAnim.introFrames.push_back(f);
                }
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/perfect/loop");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) {
                    int frameNum = -1;
                    sscanf(f.name.c_str(), "New animation%d", &frameNum);
                    if (frameNum >= 86) rankAnim.loopFrames.push_back(f);
                }
            }
            applyAntialiasing(rankAnim.sheet);
        }
        heartsSheet = SpritesheetCache::get().load("preload/images/results/ranks/perfect/hearts");
        if (heartsSheet && !heartsSheet->frames.empty()) {
            for (const auto& f : heartsSheet->frames) heartsFrames.push_back(f.index);
        }
        applyAntialiasing(heartsSheet);
        musicLoopPath = "romfs:/preload/music/results/resultsPERFECT.ogg";
    } else if (finalRating == "E") {
        rankAnim.scale = 1.2f;
        rankAnim.offsetY = 0.0f;
        rankAnim.fps = 24.0f;
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/excelent/intro");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) rankAnim.introFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/excelent/loop");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) rankAnim.loopFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        musicIntroPath = "romfs:/preload/music/results/resultsEXCELLENT-intro.ogg";
        musicLoopPath = "romfs:/preload/music/results/resultsEXCELLENT.ogg";
    } else if (finalRating == "G" && percentage >= 80.0f) {
        rankAnim.scale = 1.0f;
        rankAnim.offsetY = 0.0f;
        rankAnim.fps = 24.0f;
        gfScale = 1.0f;
        gfOffsetY = 50.0f;
        gfFps = 24.0f;
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/great/intro");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) rankAnim.introFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/great/loop");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("New animation") == 0) rankAnim.loopFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        gfSheet = SpritesheetCache::get().load("preload/images/results/ranks/great/gf");
        if (gfSheet && !gfSheet->frames.empty()) {
            for (const auto& f : gfSheet->frames) gfFrames.push_back(f.index);
        }
        applyAntialiasing(gfSheet);
        musicIntroPath = "romfs:/preload/music/results/resultsEXCELLENT-intro.ogg";
        musicLoopPath = "romfs:/preload/music/results/resultsEXCELLENT.ogg";
    } else if (finalRating == "G") {
        rankAnim.scale = 1.0f;
        rankAnim.offsetY = 0.0f;
        rankAnim.fps = 24.0f;
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/good/good");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("intro") == 0) rankAnim.introFrames.push_back(f);
                else if (f.name.find("loop") == 0) rankAnim.loopFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        goodGfSheet = SpritesheetCache::get().load("preload/images/results/ranks/good/gf");
        if (goodGfSheet && !goodGfSheet->frames.empty()) {
            for (const auto& f : goodGfSheet->frames) {
                if (f.name.find("intro") == 0) goodGfIntroFrames.push_back(f);
                else if (f.name.find("loop") == 0) goodGfLoopFrames.push_back(f);
            }
            applyAntialiasing(goodGfSheet);
        }
        goodGfIntroPlaying = false;
        goodGfAlpha = 0.0f;
        goodGfFps = 12.0f;
        goodGfOffsetX = 0.0f;
        goodGfOffsetY = 0.0f;
        goodGfScale = 0.25f;
        goodGfCurFrame = 0;
        goodGfFrameTimer = 0.0f;
        musicLoopPath = "romfs:/preload/music/results/resultsNORMAL.ogg";
    } else {
        rankAnim.scale = 0.85f;
        rankAnim.offsetY = 0.0f;
        rankAnim.fps = 24.0f;
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/loss/introPart1");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("Intro") == 0) rankAnim.introFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/loss/introPart2");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("Intro") == 0) rankAnim.introFrames.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/loss/loopPart1");
        std::vector<Frame> loopPart1;
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("Loop Start") == 0) loopPart1.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.sheet = SpritesheetCache::get().load("preload/images/results/ranks/loss/loopPart2");
        if (rankAnim.sheet && !rankAnim.sheet->frames.empty()) {
            for (const auto& f : rankAnim.sheet->frames) {
                if (f.name.find("Loop Start") == 0) loopPart1.push_back(f);
            }
            applyAntialiasing(rankAnim.sheet);
        }
        rankAnim.loopFrames = loopPart1;
        if (!rankAnim.loopFrames.empty()) {
            float baseFrameY = rankAnim.loopFrames[0].frameY;
            for (auto& f : rankAnim.loopFrames) f.frameY -= baseFrameY;
        }
        musicIntroPath = "romfs:/preload/music/results/resultsSHIT-intro.ogg";
        musicLoopPath = "romfs:/preload/music/results/resultsSHIT.ogg";
    }

    pctAnimTimer = 0.0f;
    pctDisplayValue = 0.0f;
    pctLastInt = -1;
    pctAnimActive = false;
    pctPausePhase = false;
    pctPauseTimer = 0.0f;
    pctAnimDone = false;
 
    float baseIntroDuration = 3.6f;
    if (!musicIntroPath.empty()) {
        if (finalRating == "L") {
            baseIntroDuration = 4.2f;
        } else {
            baseIntroDuration = 3.6f;
        }
    } else {
        baseIntroDuration = kPctFallbackDuration;
    }
    pctAnimDuration = std::max(0.1f, (percentage / 100.0f) * baseIntroDuration);
}

void ResultState::update(float dt) {
    timer += dt;
    if (holyScoreMode) holyScoreTimer += dt;

    if (!resultsAnimFrames.empty()) {
        resultsFrameTime += dt;
        if (resultsCurFrame < (int)resultsAnimFrames.size() - 1) {
            while (resultsFrameTime >= 1.0f / resultsFps) {
                resultsFrameTime -= 1.0f / resultsFps;
                resultsCurFrame++;
                if (resultsCurFrame >= (int)resultsAnimFrames.size()) {
                    resultsCurFrame = (int)resultsAnimFrames.size() - 1;
                    resultsFrameTime = 0.0f;
                    break;
                }
            }
        }
    }

    if (!tallyAnimDone) {
        if (!tallyAnimFrames.empty()) {
            tallyFrameTime += dt;
            if (tallyCurFrame < (int)tallyAnimFrames.size() - 1) {
                while (tallyFrameTime >= 1.0f / tallyFps) {
                    tallyFrameTime -= 1.0f / tallyFps;
                    tallyCurFrame++;
                    if (tallyCurFrame >= (int)tallyAnimFrames.size()) {
                        tallyCurFrame = (int)tallyAnimFrames.size() - 1;
                        tallyFrameTime = 0.0f;
                        tallyAnimDone = true;
                        break;
                    }
                }
            }
        }
    }

    if (!categoriesAnimDone) {
        if (!categoriesAnimFrames.empty() && categoriesStarted) {
            categoriesFrameTime += dt;
            if (categoriesCurFrame < (int)categoriesAnimFrames.size() - 1) {
                while (categoriesFrameTime >= 1.0f / categoriesFps) {
                    categoriesFrameTime -= 1.0f / categoriesFps;
                    categoriesCurFrame++;
                    if (categoriesCurFrame >= (int)categoriesAnimFrames.size()) {
                        categoriesCurFrame = (int)categoriesAnimFrames.size() - 1;
                        categoriesFrameTime = 0.0f;
                        categoriesAnimDone = true;
                        break;
                    }
                }
            }
        }
    }

    if (!statsStarted && soundSystemFrameIdx >= 0 && timer >= kStatsDelay) {
        statsStarted      = true;
        categoriesStarted = true;
        tallyStarted      = true;
    } else if (soundSystemFrameIdx < 0) {
        statsStarted      = true;
        categoriesStarted = true;
        tallyStarted      = true;
    }
 
    if (statsStarted) {
        statsTimer += dt;
        float slotStart = 0.0f;
        for (int i = 0; i < (int)numberStats.size(); i++) {
            NumberStat& s = numberStats[i];
            float elapsed = statsTimer - slotStart; // relative to statsTimer start
            if (elapsed < 0.0f) break; // slot hasn't started yet
            s.active = true;
            if (s.targetValue == 0) {
                s.currentDisplay = 0;
                s.complete = true;
                slotStart += kZeroStatDelay;   // short pause before next stat
            } else {
                float progress = std::min(elapsed / kStatCountDuration, 1.0f);
                s.currentDisplay = (int)((float)s.targetValue * progress);
                if (progress >= 1.0f) {
                    s.currentDisplay = s.targetValue;
                    s.complete = true;
                }
                slotStart += kStatCountDuration; // full count-up window before next stat
            }
        }

        scoreCountTimer += dt;
        if (holyScoreMode) {
            scoreDisplayValue = score;
        } else {
            float prog = std::min(scoreCountTimer / kScoreCountDuration, 1.0f);
            scoreDisplayValue = (long long)((double)score * prog);
        }
 
        std::string curStr = std::to_string(scoreDisplayValue);
        int scoreLen = (int)curStr.length();
        int firstSig = 10 - scoreLen; // index of first non-disabled slot
        if (scoreLen < 10)
            curStr = std::string(10 - scoreLen, '0') + curStr;
        else if (scoreLen > 10)
            curStr = curStr.substr(scoreLen - 10);

        const float kDigitAnimFps = 12.0f;
        for (int di = 0; di < 10; di++) {
            int curDigit;
            if (holyScoreMode)
                curDigit = (int)(fmodf(holyScoreTimer * 23.7f + di * 7.3f, 10.0f));
            else if (di < firstSig)
                curDigit = -1;
            else
                curDigit = curStr[di] - '0';
 
            if (curDigit != slotPrevDigit[di]) {
                slotAnimFrame[di] = 0;
                slotAnimTimer[di] = 0.0f;
                slotPrevDigit[di] = curDigit;
            }

            if (curDigit >= 0 && curDigit <= 9 && !scoreDigitFrames[curDigit].empty()) {
                int maxFrame = (int)scoreDigitFrames[curDigit].size() - 1;
                if (slotAnimFrame[di] < maxFrame) {
                    slotAnimTimer[di] += dt;
                    while (slotAnimTimer[di] >= 1.0f / kDigitAnimFps) {
                        slotAnimTimer[di] -= 1.0f / kDigitAnimFps;
                        slotAnimFrame[di]++;
                        if (slotAnimFrame[di] >= maxFrame) {
                            slotAnimFrame[di] = maxFrame;
                            slotAnimTimer[di] = 0.0f;
                            break;
                        }
                    }
                }
            }
        }
    }

    bool allStatsDone = true;
    for (const auto& s : numberStats) {
        if (!s.complete) { allStatsDone = false; break; }
    }
    if (scoreDisplayValue < score) allStatsDone = false;
 
    bool shouldPlayMusic = true;
    if (finalRating == "P" && (!allStatsDone || !pctAnimDone)) {
        shouldPlayMusic = false;
    }

    if (shouldPlayMusic && !musicStarted) {
        musicStarted = true;
        if (finalRating == "P") {
            if (!musicLoopPath.empty()) {
                MusicPlayer::play(musicLoopPath.c_str(), 0.7f);
                musicLoopPlaying = true;
            }
        } else {
            if (!musicIntroPath.empty()) {
                MusicPlayer::play(musicIntroPath.c_str(), 0.7f);
                musicIntroDurationMs = MusicPlayer::getDuration();
                if (musicIntroDurationMs > 0.0) {
                    float loadedDurationSecs = (float)(musicIntroDurationMs / 1000.0);
                    if (loadedDurationSecs > 5.0f) loadedDurationSecs = 3.6f;
                    pctAnimDuration = std::max(0.1f, (percentage / 100.0f) * loadedDurationSecs);
                }
                musicIntroElapsed    = 0.0f;
                musicIntroPlaying    = true;
            } else if (!musicLoopPath.empty()) {
                MusicPlayer::play(musicLoopPath.c_str(), 0.7f);
                musicLoopPlaying = true;
            }
        }
    }

    if (statsStarted && !pctAnimDone) pctAnimActive = true;
 
    if (pctAnimActive && !pctAnimDone) {
        if (!pctPausePhase) {
            pctAnimTimer += dt;
            float progress = pctAnimTimer / pctAnimDuration;
            if (progress >= 1.0f) {
                progress = 1.0f;
                pctPausePhase = true;
                pctPauseTimer = 0.0f;
            }

            float easeOut = progress * (2.0f - progress);
            float targetValue = std::max(0.0f, percentage - 1.0f);
            pctDisplayValue = targetValue * easeOut;
 
            int curInt = (int)std::floor(pctDisplayValue);
            if (curInt != pctLastInt && curInt >= 0) {
                pctLastInt = curInt;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.5f);
            }
        } else {
            pctPauseTimer += dt;
            pctDisplayValue = std::max(0.0f, percentage - 1.0f);
 
            if (pctPauseTimer >= kPctPauseDuration) {
                pctDisplayValue = percentage;
                pctAnimDone = true;
                pctAnimActive = false;
 
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                flashAlpha = 1.0f;
                flashTriggered = true;
 
                if (finalRating == "P") {
                    if (!musicLoopPath.empty() && !musicLoopPlaying) {
                        MusicPlayer::play(musicLoopPath.c_str(), 0.7f);
                        musicLoopPlaying = true;
                        musicStarted = true;
                    }
                }
            }
        }
    }

    if (flashTriggered) {
        if (bgFadeAlpha < 1.0f) {
            bgFadeAlpha += dt * 2.0f;
            if (bgFadeAlpha > 1.0f) bgFadeAlpha = 1.0f;
        }
    }
    bgScrollOffset += dt * 30.0f;
 
    if (flashAlpha > 0.0f) {
        flashAlpha -= dt * 4.0f;
        if (flashAlpha < 0.0f) flashAlpha = 0.0f;
    }

    if (pctAnimDone && !rankAnim.started) {
        rankAnim.started = true;
        rankAnim.alpha = 0.0f;
        if (!rankAnim.introFrames.empty()) {
            rankAnim.introPlaying = true;
            rankAnim.introTimer = 0.0f;
            rankAnim.curFrame = 0;
            rankAnim.frameTimer = 0.0f;
        } else if (!rankAnim.loopFrames.empty()) {
            rankAnim.curFrame = 0;
            rankAnim.frameTimer = 0.0f;
        }
    }

    if (rankAnim.started) {
        if (rankAnim.alpha < 1.0f) {
            rankAnim.alpha += dt * 3.0f;
            if (rankAnim.alpha > 1.0f) rankAnim.alpha = 1.0f;
        }
        rankAnim.frameTimer += dt;
        while (rankAnim.frameTimer >= 1.0f / rankAnim.fps) {
            rankAnim.frameTimer -= 1.0f / rankAnim.fps;
            if (rankAnim.introPlaying) {
                if (rankAnim.curFrame < (int)rankAnim.introFrames.size() - 1) {
                    rankAnim.curFrame++;
                } else {
                    rankAnim.introPlaying = false;
                    rankAnim.curFrame = 0;
                    rankAnim.frameTimer = 0.0f;
                    if (finalRating == "G" && percentage >= 80.0f) gfOutPlaying = true;
                    else if (finalRating == "G" && percentage < 80.0f) goodGfIntroPlaying = true;
                }
            } else {
                if (!rankAnim.loopFrames.empty()) {
                    rankAnim.curFrame = (rankAnim.curFrame + 1) % (int)rankAnim.loopFrames.size();
                }
            }
        }

        if (finalRating == "G" && gfOutPlaying) {
            gfOutTimer += dt;
            float t = std::min(gfOutTimer / gfOutDuration, 1.0f);
            float ease = 1.0f - std::pow(1.0f - t, 3.0f);
            gfOutX = -200.0f + (gfOutTargetX - (-200.0f)) * ease;
            gfOutFrameTimer += dt;
            while (gfOutFrameTimer >= 1.0f / gfFps) {
                gfOutFrameTimer -= 1.0f / gfFps;
                gfOutFrame = (gfOutFrame + 1) % (int)gfFrames.size();
            }
        }

        if (finalRating == "G" && !(percentage >= 80.0f) && goodGfIntroPlaying) {
            if (goodGfAlpha < 1.0f) {
                goodGfAlpha += dt * 3.0f;
                if (goodGfAlpha > 1.0f) goodGfAlpha = 1.0f;
            }
            goodGfFrameTimer += dt;
            while (goodGfFrameTimer >= 1.0f / goodGfFps) {
                goodGfFrameTimer -= 1.0f / goodGfFps;
                if (goodGfIntroPlaying) {
                    if (!goodGfIntroFrames.empty() && goodGfCurFrame < (int)goodGfIntroFrames.size() - 1) {
                        goodGfCurFrame++;
                    } else {
                        goodGfIntroPlaying = false;
                        goodGfCurFrame = 0;
                        goodGfFrameTimer = 0.0f;
                    }
                } else if (!goodGfLoopFrames.empty()) {
                    goodGfCurFrame = (goodGfCurFrame + 1) % (int)goodGfLoopFrames.size();
                }
            }
        }

        if (finalRating == "P" && !rankAnim.introPlaying) {
            heartsTimer += dt;
            heartsFrameTimer += dt;
            while (heartsFrameTimer >= 1.0f / heartsFps) {
                heartsFrameTimer -= 1.0f / heartsFps;
                heartsFrame = (heartsFrame + 1) % (int)heartsFrames.size();
            }
        }
    }

    if (pctAnimDone) {
        if (pctFadeAlpha > 0.0f) {
            pctFadeAlpha -= dt * 0.67f;
            if (pctFadeAlpha < 0.0f) pctFadeAlpha = 0.0f;
        }
 
        pctBlinkTimer += dt;
        if (pctBlinkTimer >= 0.08f) {
            pctBlinkTimer -= 0.08f;
            pctBlinkState = !pctBlinkState;
        }
 
        if (!Achievements::sessionUnlocks.empty()) {
            if (achEnterTimer < 1.0f) {
                achEnterTimer += dt * 1.5f; // slide in over ~0.67 seconds
                if (achEnterTimer > 1.0f) achEnterTimer = 1.0f;
            }
            achScrollOffset += dt * 50.0f; // continuous scroll speed
        }
    }

    if (musicIntroPlaying) {
        musicIntroElapsed += dt;
        double elapsedMs = (double)musicIntroElapsed * 1000.0;
        if (musicIntroDurationMs > 0.0 && elapsedMs >= musicIntroDurationMs) {
            musicIntroPlaying = false;
            if (!musicLoopPath.empty()) {
                MusicPlayer::play(musicLoopPath.c_str(), 0.7f);
                musicLoopPlaying = true;
            }
        }
    }

    u32 kDown = hidKeysDown();
    
    bool touchPressed = false;
    if (kDown & KEY_TOUCH) {
        touchPressed = true;
    }

    if (keyJustPressed(KEY_A | KEY_START) || touchPressed) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        
        MusicBeatState* nextState = nullptr;
        if (fromDebug) {
            nextState = new DebugMenuState();
        } else if (isStoryMode) {
            nextState = new StoryMenuState();
        } else {
            nextState = new FreeplayState();
        }
        
        Achievements::sessionUnlocks.clear();
        MusicBeatState::useStickerTransition = true;
        switchState(nextState);
    }
}

static void drawBackgroundScrollingText(const Frame& f, float offset, float alpha, float angleRad, float screenW, float screenH) {
    if (!f.tex) return;
    float spriteW = frameLogicalW(f);
    float spriteH = frameLogicalH(f);
    if (spriteW <= 0.0f) return;

    float cx = screenW / 2.0f;
    float cy = screenH / 2.0f;

    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
 
    C2D_Image img = { f.tex, &f.uv };
 
    float rowStep = spriteH + 3.0f;
    for (float yPos = -120.0f; yPos < 360.0f; yPos += rowStep) {
        int rowIndex = (int)((yPos + 120.0f) / rowStep);
        float speedMultiplier = (rowIndex % 2 == 0) ? 1.0f : -1.0f;
        
        float rowOffset = fmodf(offset * speedMultiplier, spriteW);
        if (rowOffset < 0.0f) rowOffset += spriteW;

        for (float xPos = -spriteW; xPos < screenW + spriteW; xPos += spriteW) {
            float drawX = xPos + rowOffset;
            
            float tileCenterX = drawX + spriteW * 0.5f;
            float tileCenterY = yPos + spriteH * 0.5f;
 
            float dx = tileCenterX - cx;
            float dy = tileCenterY - cy;
            float cosA = std::cos(angleRad);
            float sinA = std::sin(angleRad);
 
            float rx = cx + dx * cosA - dy * sinA;
            float ry = cy + dx * sinA + dy * cosA;
 
            float finalAngle = angleRad;
            if (f.rotated) {
                finalAngle -= (3.14159265f / 2.0f);
            }

            C2D_DrawImageAtRotated(img, rx, ry, 0.12f, finalAngle, &tint, 1.0f, 1.0f);
        }
    }
}

void ResultState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    ClearTextBuf();

    float barAngleRad = -5.0f * (3.14159265f / 180.0f);

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(248, 202, 97, 255));
 
    if (bgFadeAlpha > 0.0f && bgScrollFrame.tex) {
        drawBackgroundScrollingText(bgScrollFrame, bgScrollOffset, bgFadeAlpha, barAngleRad, 400.0f, 240.0f);
    }

    if (bgFadeAlpha > 0.0f && bgTextVerticalFrame.tex) {
        float spriteW = frameLogicalW(bgTextVerticalFrame);
        float spriteH = frameLogicalH(bgTextVerticalFrame);
        if (spriteW > 0.0f && spriteH > 0.0f) {
            C2D_ImageTint tint;
            C2D_AlphaImageTint(&tint, bgFadeAlpha * 1.0f);
            C2D_Image img = { bgTextVerticalFrame.tex, &bgTextVerticalFrame.uv };
 
            float rightEdgeX = 400.0f - spriteW * 0.5f - 4.0f;
            float stepY = spriteH + 25.0f;
            
            float vScrollOffset = fmodf(bgScrollOffset * 1.5f, stepY);
            if (vScrollOffset < 0.0f) vScrollOffset += stepY;
 
            for (float yPos = -stepY; yPos < 240.0f + stepY; yPos += stepY) {
                float drawY = yPos - vScrollOffset + stepY * 0.5f;

                float finalAngle = 0.0f;
                if (bgTextVerticalFrame.rotated) {
                    finalAngle -= (3.14159265f / 2.0f);
                }

                C2D_DrawImageAtRotated(img, rightEdgeX, drawY, 0.13f, finalAngle, &tint, 1.0f, 1.0f);
            }
        }
    }

    if (flashAlpha > 0.0f) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.95f, 400.0f, 240.0f, C2D_Color32(0xFD, 0xF1, 0xB1, (u8)(flashAlpha * 255.0f)));
    }
 
    drawRotatedRect(160.0f, -5.0f, 500.0f, 100.0f, barAngleRad, C2D_Color32(0, 0, 0, 255), 0.15f);
 
    if (!resultsAnimFrames.empty() && resultsSheet) {
        int frameIdx = resultsAnimFrames[resultsCurFrame % resultsAnimFrames.size()];
        const Frame& f = resultsSheet->frames[frameIdx];
        float scale = 0.85f;

        const Frame& fBase = resultsSheet->frames[resultsAnimFrames[0]];
        float fW = (fBase.frameW > 0) ? (float)fBase.frameW : (fBase.rotated ? (float)fBase.h : (float)fBase.w);
        float fH = (fBase.frameH > 0) ? (float)fBase.frameH : (fBase.rotated ? (float)fBase.w : (float)fBase.h);

        float halfWidth = (fW * scale) / 2.0f;
        float x = 200.0f - halfWidth;
        float y = 35.0f - (fH * scale / 2.0f);

        drawFrameAt(f, x - halfWidth / 2, y, 0.9f, nullptr, scale, scale);
    }
 
    if (clearPercentFrame.tex) {
        float scale = 0.85f;
        float textW = frameLogicalW(clearPercentFrame) * scale;
        float textH = frameLogicalH(clearPercentFrame) * scale;

        int currentPct = (int)std::round(pctDisplayValue);
        std::string pctStr = std::to_string(currentPct);

        float digitH = 0.0f;
        for (char c : pctStr) {
            int d = c - '0';
            if (d >= 0 && d <= 9 && pctNumberFrames[d].tex) {
                digitH = std::max(digitH, frameLogicalH(pctNumberFrames[d]) * scale);
            }
        }

        float groupStartX = 200.0f - textW * 0.5f;
        float groupStartY = 110.0f;
 
        C2D_ImageTint textTint;
        C2D_AlphaImageTint(&textTint, pctFadeAlpha);
        drawFrameAt(clearPercentFrame, groupStartX, groupStartY - textH * 0.5f, 0.84f, &textTint, scale, scale);
 
        float numRightBoundaryX = groupStartX + textW + 6.0f + 52.0f + kPctNumOffsetX;
        float numY = groupStartY - digitH * 0.5f + kPctNumOffsetY;

        C2D_ImageTint numTint;
        bool usingBlinkTint = pctAnimDone && pctBlinkState;
        if (usingBlinkTint) {
            C2D_PlainImageTint(&numTint, C2D_Color32(255, 255, 255, (u8)(pctFadeAlpha * 255.0f)), 1.0f);
            C2D_SetTintMode(C2D_TintSolid);
        } else {
            C2D_AlphaImageTint(&numTint, pctFadeAlpha);
        }
 
        float currentX = numRightBoundaryX;
        for (int i = (int)pctStr.size() - 1; i >= 0; i--) {
            int d = pctStr[i] - '0';
            if (d >= 0 && d <= 9 && pctNumberFrames[d].tex) {
                float dW = frameLogicalW(pctNumberFrames[d]) * scale;
                currentX -= (dW - 2.0f); // move cursor to the left with slight overlap spacing
                drawFrameAt(pctNumberFrames[d], currentX, numY, 0.84f, &numTint, scale, scale);
            }
        }
        if (usingBlinkTint) C2D_SetTintMode(C2D_TintSolid);
    }

    if (rankAnim.started) {
        std::vector<Frame>* activeFrames = rankAnim.introPlaying ? &rankAnim.introFrames : &rankAnim.loopFrames;
        if (!activeFrames->empty()) {
            int fi = rankAnim.curFrame % (int)activeFrames->size();
            const Frame& rf = (*activeFrames)[fi];
            float rScale = rankAnim.scale;
            float rW = frameLogicalW(rf) * rScale;
            float rH = frameLogicalH(rf) * rScale;
            float rX = 200.0f - rW / 2.0f;
            float rY = 100.0f - rH / 2.0f + rankAnim.offsetY;
            C2D_ImageTint rankTint;
            C2D_AlphaImageTint(&rankTint, rankAnim.alpha);
            drawFrameAt(rf, rX, rY, 0.85f, &rankTint, rScale, rScale);
        }
    }

    if (finalRating == "G" && gfOutPlaying && gfSheet && !gfSheet->frames.empty() && !gfFrames.empty()) {
        int fi = gfOutFrame % (int)gfFrames.size();
        const Frame& gf = gfSheet->frames[gfFrames[fi]];
        float gfDrawScale = gfScale;
        float gfH = frameLogicalH(gf) * gfDrawScale;
        float gfY = 100.0f - gfH / 2.0f + rankAnim.offsetY + gfOffsetY;
        C2D_ImageTint gfTint;
        C2D_AlphaImageTint(&gfTint, rankAnim.alpha);
        drawFrameAt(gf, gfOutX, gfY, 0.84f, &gfTint, gfDrawScale, gfDrawScale);
    }

    if (finalRating == "G" && !(percentage >= 80.0f) && goodGfIntroPlaying && goodGfSheet && !goodGfSheet->frames.empty() && !goodGfIntroFrames.empty()) {
        std::vector<Frame>* activeFrames = goodGfIntroPlaying ? &goodGfIntroFrames : &goodGfLoopFrames;
        if (!activeFrames->empty()) {
            int fi = goodGfCurFrame % (int)activeFrames->size();
            const Frame& gf = (*activeFrames)[fi];
            float gfDrawScale = goodGfScale;
            float gfH = frameLogicalH(gf) * gfDrawScale;
            float gfY = 100.0f - gfH / 2.0f + rankAnim.offsetY + goodGfOffsetY;
            C2D_ImageTint gfTint;
            C2D_AlphaImageTint(&gfTint, goodGfAlpha);
            drawFrameAt(gf, 200.0f - frameLogicalW(gf) * gfDrawScale / 2.0f + goodGfOffsetX, gfY, 0.84f, &gfTint, gfDrawScale, gfDrawScale);
        }
    }

    if (finalRating == "P" && !rankAnim.introPlaying && heartsSheet && !heartsSheet->frames.empty() && !heartsFrames.empty()) {
        int fi = heartsFrame % (int)heartsFrames.size();
        const Frame& hf = heartsSheet->frames[heartsFrames[fi]];
        float hScale = heartsScale;
        float hW = frameLogicalW(hf) * hScale;
        float hH = frameLogicalH(hf) * hScale;
        float floatX = sinf(heartsTimer * 2.0f) * 8.0f;
        float floatY = cosf(heartsTimer * 1.5f) * 6.0f;
        float hX = 200.0f - hW / 2.0f + heartsOffsetX + floatX;
        float hY = 60.0f - hH / 2.0f + rankAnim.offsetY + heartsOffsetY + floatY;
        C2D_ImageTint heartsTint;
        C2D_AlphaImageTint(&heartsTint, rankAnim.alpha);
        drawFrameAt(hf, hX, hY, 0.84f, &heartsTint, hScale, hScale);
    }

    if (achEnterTimer > 0.0f && !Achievements::sessionUnlocks.empty()) {
        float easeProgress = 1.0f - std::pow(1.0f - achEnterTimer, 3.0f);
        float yPosText = 300.0f + (185.0f - 255.0f) * easeProgress;

        // Blinking title
        u32 titleColor = pctBlinkState ? CWhite : C2D_Color32(255, 215, 0, 255);
        AddTextDepth("ACHIEVEMENTS UNLOCKED!", 200.0f, yPosText - 20.0f, 0.4f, true, 1.5f, titleColor, 0.0f, 0.86f);

        bool singleAchievement = Achievements::sessionUnlocks.size() == 1;
        size_t repeatCount = singleAchievement ? 4 : 1;
        size_t totalEntries = Achievements::sessionUnlocks.size() * repeatCount;

        std::vector<Frame*> rowFrames;
        std::vector<std::string> rowNames;
        std::vector<float> rowEntryWidths;
        std::vector<float> rowTextWidths;
        rowFrames.reserve(totalEntries);
        rowNames.reserve(totalEntries);
        rowEntryWidths.reserve(totalEntries);
        rowTextWidths.reserve(totalEntries);

        for (size_t r = 0; r < repeatCount; r++) {
            for (size_t i = 0; i < Achievements::sessionUnlocks.size(); i++) {
                std::string tag = Achievements::sessionUnlocks[i];
                Frame* iconFrame = nullptr;
                for (auto& f : achievementsFrames) {
                    if (f.name.find(tag) == 0) {
                        iconFrame = &f;
                        break;
                    }
                }
                rowFrames.push_back(iconFrame);
                int idx = Achievements::getAchievementIndex(tag);
                std::string nameStr = (idx >= 0) ? Achievements::achievementsStuff[idx].name : tag;
                rowNames.push_back(nameStr);

                ClearTextBuf();
                C2D_Text gText;
                C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, nameStr.c_str());
                C2D_TextOptimize(&gText);
                float tw = 0.0f, th = 0.0f;
                C2D_TextGetDimensions(&gText, 0.4f, 0.4f, &tw, &th);

                rowTextWidths.push_back(tw);
                float w = 16.0f + 4.0f + tw + 10.0f;
                rowEntryWidths.push_back(w);
            }
        }

        float totalAchW = 0.0f;
        for (size_t i = 0; i < rowEntryWidths.size(); i++) totalAchW += rowEntryWidths[i];

        float scrollOffset = fmodf(achScrollOffset, totalAchW);
        if (scrollOffset < 0.0f) scrollOffset += totalAchW;

        for (float xPos = -totalAchW; xPos < 400.0f + totalAchW; xPos += totalAchW) {
            float curX = xPos - scrollOffset;
            float accX = 0.0f;
            for (size_t i = 0; i < rowEntryWidths.size(); i++) {
                float drawX = curX + accX;
                float entryW = rowEntryWidths[i];

                if (drawX > -entryW && drawX < 400.0f) {
                    float iconCenterX = drawX + 8.0f;
                    float textCenterX = drawX + 16.0f + 2.0f + rowTextWidths[i] / 2.0f;

                    if (rowFrames[i]) {
                        drawFrameCentered(*rowFrames[i], iconCenterX, yPosText, 0.87f, nullptr, 0.25f, 0.25f);
                    }
                    AddTextDepth(rowNames[i], textCenterX, yPosText, 0.4f, true, 1.5f, CWhite, 0.0f, 0.87f);
                }
                accX += entryW;
            }
        }
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(248, 202, 97, 255));
 
    if (bgFadeAlpha > 0.0f && bgScrollFrame.tex) {
        drawBackgroundScrollingText(bgScrollFrame, bgScrollOffset, bgFadeAlpha, barAngleRad, 320.0f, 240.0f);
    }

    if (flashAlpha > 0.0f) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.95f, 320.0f, 240.0f, C2D_Color32(0xFD, 0xF1, 0xB1, (u8)(flashAlpha * 255.0f)));
    }
 
    if (soundSystemFrameIdx >= 0 && resultsSheet) {
        const Frame& ssFrame = resultsSheet->frames[soundSystemFrameIdx];
        float ssScale = 0.675f;

        float animProgress = std::min(timer / 0.5f, 1.0f);
        float easeProgress = 1.0f - std::pow(1.0f - animProgress, 3.0f); // Ease Out Cubic

        float startX = -250.0f;
        float targetX = 0.0f;
        float ssX = startX + (targetX - startX) * easeProgress;
        float ssY = -105.0f;

        drawFrameAt(ssFrame, ssX, ssY, 0.15f, nullptr, ssScale, ssScale);
    }

    if (!tallyAnimFrames.empty() && resultsSheet && tallyStarted) {
        int frameIdx = tallyAnimFrames[tallyCurFrame % tallyAnimFrames.size()];
        const Frame& tallyFrame = resultsSheet->frames[frameIdx];
        float tallyScale = 0.65f;

        float tallyX = 70.0f - (frameLogicalW(tallyFrame) * tallyScale) / 2.0f;
        float tallyY = 160.0f;

        drawFrameAt(tallyFrame, tallyX, tallyY, 0.15f, nullptr, tallyScale, tallyScale);
    }

    if (!categoriesAnimFrames.empty() && categoriesSheet && categoriesStarted) {
        int frameIdx = categoriesAnimFrames[categoriesCurFrame % categoriesAnimFrames.size()];
        const Frame& catFrame = categoriesSheet->frames[frameIdx];
        float catScale = 0.6f;

        float catX = 40.0f - (frameLogicalW(catFrame) * catScale) / 2.0f;
        float catY = 10.0f;

        drawFrameAt(catFrame, catX, catY, 0.15f, nullptr, catScale, catScale);
    }

    for (size_t i = 0; i < numberStats.size(); i++) {
        const NumberStat& s = numberStats[i];
        if (numSmallFrames.empty() || !s.active) continue;
        drawSmallNumber(numSmallFrames, s.currentDisplay, s.x, s.y, 0.15f, s.color, s.scale);
    }

    {
        float digitW = 0.0f;
        if (!scoreDigitFrames[0].empty()) {
            digitW = frameLogicalW(scoreDigitFrames[0][0]) * kScoreDigitScale;
        } else if (goneFrame.tex) {
            digitW = frameLogicalW(goneFrame) * kScoreDigitScale;
        }
        float totalRowW = digitW * 10.0f + kScoreDigitSpacing * 9.0f;
        float rowStartX = kScoreRowRightX - totalRowW;

        if (!statsStarted) {
            if (goneFrame.tex) {
                for (int di = 0; di < 10; di++) {
                    float dX = rowStartX + di * (digitW + kScoreDigitSpacing);
                    float dH = frameLogicalH(goneFrame) * kScoreDigitScale;
                    float dY = kScoreRowY + 8.0f - dH / 2.0f;
                    drawFrameAt(goneFrame, dX, dY, 0.85f, nullptr, kScoreDigitScale, kScoreDigitScale);
                }
            }
        } else {
            std::string scoreStr = std::to_string(scoreDisplayValue);
            int scoreLength = (int)scoreStr.length();
            int firstSignificantIndex = 10 - scoreLength;
            if (scoreLength < 10)
                scoreStr = std::string(10 - scoreLength, '0') + scoreStr;
            else if (scoreLength > 10)
                scoreStr = scoreStr.substr(scoreLength - 10);

            for (int di = 0; di < 10; di++) {
                float dX = rowStartX + di * (digitW + kScoreDigitSpacing);
                float dY = kScoreRowY;
 
                if (holyScoreMode) {
                    int digit = slotPrevDigit[di];
                    if (digit >= 0 && digit <= 9 && !scoreDigitFrames[digit].empty()) {
                        int fi = std::min(slotAnimFrame[di], (int)scoreDigitFrames[digit].size() - 1);
                        const Frame& fr = scoreDigitFrames[digit][fi];
                        float dH = frameLogicalH(fr) * kScoreDigitScale;
                        drawFrameAt(fr, dX, dY + 8.0f - dH / 2.0f, 0.85f, nullptr, kScoreDigitScale, kScoreDigitScale);
                    }
                } else if (di < firstSignificantIndex) {
                    if (disabledFrame.tex) {
                        float dH = frameLogicalH(disabledFrame) * kScoreDigitScale;
                        drawFrameAt(disabledFrame, dX, dY + 8.0f - dH / 2.0f, 0.85f, nullptr, kScoreDigitScale, kScoreDigitScale);
                    }
                } else {
                    int digit = scoreStr[di] - '0';
                    if (digit >= 0 && digit <= 9 && !scoreDigitFrames[digit].empty()) {
                        int fi = std::min(slotAnimFrame[di], (int)scoreDigitFrames[digit].size() - 1);
                        const Frame& fr = scoreDigitFrames[digit][fi];
                        float dH = frameLogicalH(fr) * kScoreDigitScale;
                        drawFrameAt(fr, dX, dY + 8.0f - dH / 2.0f, 0.85f, nullptr, kScoreDigitScale, kScoreDigitScale);
                    }
                }
            }
        }
    }
}

void ResultState::exitState() {
    if (achievementsSheet) C2D_SpriteSheetFree(achievementsSheet);
    C2D_TextBufDelete(vcrFontBuf);
}
