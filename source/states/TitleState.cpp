#include "TitleState.hpp"
#include "MainMenuState.hpp"
#include "../debug/DebugMenuState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/savedata/Achievements.hpp"
#include <citro2d.h>
#include "../objects/Alphabet.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include "VideoState.hpp"
#include "OutdatedState.hpp"
#include "../backend/UpdateChecker.hpp"
#include "../objects/ButtonPrompt.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Pre-calculated lookup table for progress ring rendering (40 segments)
static void drawProgressRing(float cx, float cy, float r_in, float r_out, float progress, u32 color, float depth = 0.9f) {
    if (progress <= 0.0f) return;
    if (progress > 1.0f) progress = 1.0f;

    const int maxSegments = 40;
    int segmentsToDraw = (int)(progress * (float)maxSegments);
    if (segmentsToDraw < 1) segmentsToDraw = 1;

    static bool tableInitialized = false;
    static float cosTable[maxSegments + 1];
    static float sinTable[maxSegments + 1];

    if (!tableInitialized) {
        for (int i = 0; i <= maxSegments; i++) {
            float theta = -M_PI / 2.0f + ((float)i * 2.0f * M_PI / (float)maxSegments);
            cosTable[i] = cosf(theta);
            sinTable[i] = sinf(theta);
        }
        tableInitialized = true;
    }

    for (int i = 0; i < segmentsToDraw; i++) {
        float cos1 = cosTable[i];
        float sin1 = sinTable[i];
        float cos2 = cosTable[i + 1];
        float sin2 = sinTable[i + 1];

        float ix1 = cx + r_in * cos1;
        float iy1 = cy + r_in * sin1;
        float ix2 = cx + r_in * cos2;
        float iy2 = cy + r_in * sin2;

        float ox1 = cx + r_out * cos1;
        float oy1 = cy + r_out * sin1;
        float ox2 = cx + r_out * cos2;
        float oy2 = cy + r_out * sin2;

        C2D_DrawTriangle(ox1, oy1, color, ix1, iy1, color, ix2, iy2, color, depth);
        C2D_DrawTriangle(ox1, oy1, color, ix2, iy2, color, ox2, oy2, color, depth);
    }
}

static bool titleInitialized = false;
static const std::vector<std::pair<std::string, std::string>> defaultWackyTexts = {
    {"Did you just...", "Deleted introText.txt?"}
};

void TitleState::init() {
    ModHandler::get().scanMods(); // Scan all mods on startup
    UpdateChecker::startCheck();

    timer = 0.0f;
    curBeat = -1;
    skippedIntro = false;
    showNewgrounds = false;
    transitioning = false;
    switchTimer = 0.0f;
    logoScale = 1.0f;
    promoPending = false;
    promoFadingOut = false;
    promoFadeTime = 0.0f;
    promoChosenVideo = "";
    exitProgress = 0.0f;
    ringAlpha = 0.0f;

    // Load animated title sprites
    logo.loadSheet("preload/images/logoBumpin");
    logo.addAnim("bump", "default", 24.0f, true);
    logo.play("bump");
    logo.antialiasing = ClientPrefs::globalAntialiasing;

    ng.loadSheet("preload/images/newgrounds");
    ng.addAnim("flash", "newgrounds logo instance", 6.0f, true);
    ng.play("flash");
    ng.antialiasing = ClientPrefs::globalAntialiasing;

    titleEnter.loadSheet("preload/images/titleEnter");
    titleEnter.addAnim("idle", "Press Enter to Begin0", 12.0f, true);
    titleEnter.addAnim("press", "ENTER PRESSED0", 24.0f, true);
    titleEnter.play("idle");
    titleEnter.ignoreFrameOffsets = true;
    titleEnter.antialiasing = ClientPrefs::globalAntialiasing;

    titleEnter2.loadSheet("preload/images/titleEnter");
    titleEnter2.addAnim("idle", "Press Enter to Begin200", 12.0f, true);
    titleEnter2.addAnim("press", "ENTER PRESSED200", 24.0f, true);
    titleEnter2.play("idle");
    titleEnter2.ignoreFrameOffsets = true;
    titleEnter2.antialiasing = ClientPrefs::globalAntialiasing;

    // Load Girlfriend Title Dance
    gf.loadSheet("preload/images/gfDanceTitle");
    CachedSpritesheet* tempGf = SpritesheetCache::get().load("preload/images/gfDanceTitle");
    if (tempGf) {
        int matchedCount = 0;
        for (const auto& f : tempGf->frames) {
            if (f.name.find("gfDance") == 0) matchedCount++;
        }
        int mid = matchedCount / 2;
        std::vector<int> leftIdx, rightIdx;
        leftIdx.reserve(mid);
        rightIdx.reserve(matchedCount - mid);
        for (int i = 0; i < matchedCount; i++) {
            if (i < mid) leftIdx.push_back(i);
            else rightIdx.push_back(i);
        }
        gf.addAnim("danceLeft", "gfDance", 24.0f, false, 0.0f, 0.0f, leftIdx);
        gf.addAnim("danceRight", "gfDance", 24.0f, false, 0.0f, 0.0f, rightIdx);
    }
    gf.play("danceLeft");
    gf.antialiasing = ClientPrefs::globalAntialiasing;

    // Pre-warm audio and fonts
    SpritesheetCache::get().load("shared/images/Alphabet");
    AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.0f);
    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.0f);
    AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.0f);

    // Start menu music if not already playing
    if (!MusicPlayer::isPlaying()) {
        MusicPlayer::playMenuMusic();
    }

    // Pick random wacky text from introText.txt
    static bool seedSet = false;
    if (!seedSet) {
        srand((unsigned int)time(NULL));
        seedSet = true;
    }

    std::string line1 = "Snake Engine";
    std::string line2 = "by Snakyjoel";
    std::vector<std::pair<std::string, std::string>> loadedTexts;

    FILE* fIntro = fopen("romfs:/preload/data/introText.txt", "r");
    if (!fIntro) {
        fIntro = fopen("romfs:/preload/introText.txt", "r");
    }
    if (fIntro) {
        char lineBuf[256];
        while (fgets(lineBuf, sizeof(lineBuf), fIntro)) {
            std::string lineStr(lineBuf);
            while (!lineStr.empty() && (lineStr.back() == '\n' || lineStr.back() == '\r' || lineStr.back() == ' ' || lineStr.back() == '\t')) {
                lineStr.pop_back();
            }
            if (lineStr.empty() || lineStr[0] == '#') continue;
            size_t splitPos = lineStr.find("--");
            if (splitPos != std::string::npos) {
                loadedTexts.push_back({lineStr.substr(0, splitPos), lineStr.substr(splitPos + 2)});
            }
        }
        fclose(fIntro);
    }

    if (!loadedTexts.empty()) {
        int idx = rand() % (int)loadedTexts.size();
        line1 = loadedTexts[idx].first;
        line2 = loadedTexts[idx].second;
    } else {
        int idx = rand() % (int)defaultWackyTexts.size();
        line1 = defaultWackyTexts[idx].first;
        line2 = defaultWackyTexts[idx].second;
    }
    wackyText1 = line1;
    wackyText2 = line2;

    if (titleInitialized) {
        skipIntro();
    } else {
        titleInitialized = true;
    }
}

void TitleState::beatHit(int beat) {
    logoScale = 1.15f;

    // GF dancing alternation on each beat
    gfDanceLeftActive = !gfDanceLeftActive;
    gf.play(gfDanceLeftActive ? "danceLeft" : "danceRight", true);

    // Intro text sequence beats
    if (!skippedIntro) {
        switch (beat) {
            case 1: createCoolText({"THE", "FUNKIN CREW INC"}); break;
            case 3: addMoreText("PRESENTS"); break;
            case 4: deleteCoolText(); break;
            case 5: createCoolText({"IN ASSOCIATION", "WITH"}); break;
            case 7: addMoreText("NEWGROUNDS"); showNewgrounds = true; break;
            case 8: deleteCoolText(); showNewgrounds = false; break;
            case 9: createCoolText({wackyText1}); break;
            case 11: addMoreText(wackyText2); break;
            case 12: deleteCoolText(); break;
            case 13: addMoreText("Friday"); break;
            case 14: addMoreText("Night"); break;
            case 15: addMoreText("Funkin"); break;
            case 16: skipIntro(); break;
        }
    }
}

void TitleState::update(float dt) {
    MusicPlayer::update();

    if (MusicPlayer::isPlaying()) {
        timer = MusicPlayer::getPosition() / 1000.0f;
    } else {
        timer += dt;
    }

    // Sync beats (102 BPM)
    float crochet = 60.0f / 102.0f;
    int newBeat = (int)(timer / crochet);
    if (newBeat > curBeat) {
        curBeat = newBeat;
        beatHit(curBeat);
    }

    gf.update(dt);
    logo.update(dt);
    if (showNewgrounds) {
        ng.update(dt);
    }
    titleEnter.update(dt);
    titleEnter2.update(dt);

    // Smooth logo beat zoom bounce (single precision float expf)
    logoScale = 1.0f + (logoScale - 1.0f) * expf(-12.0f * dt);

    // State transition logic
    if (transitioning) {
        switchTimer -= dt;
        if (switchTimer <= 0.0f) {
            if (konamiInput == konamiTarget) {
                switchState(new DebugMenuState());
            } else {
                if (ClientPrefs::checkForUpdates && UpdateChecker::isChecking() && !UpdateChecker::isFinished()) {
                    updateWaitTimer += dt;
                    if (updateWaitTimer < 3.0f) {
                        return; // Wait up to 3 seconds for update checker thread
                    }
                }
                if (ClientPrefs::checkForUpdates && UpdateChecker::isFinished() && !UpdateChecker::getOnlineVersion().empty()) {
                    int comp = UpdateChecker::compareVersions(UpdateChecker::getCurrentVersion(), UpdateChecker::getOnlineVersion());
                    if (comp != 0) {
                        Achievements::unlockAchievement("startgame");
                        switchState(new OutdatedState(comp, UpdateChecker::getOnlineVersion(), UpdateChecker::getUpdateSource()));
                        return;
                    }
                }
                Achievements::unlockAchievement("startgame");
                switchState(new MainMenuState());
            }
        }
    }

    // Hold B button to exit application
    bool isHoldingExit = false;
    if (skippedIntro && !transitioning) {
        u32 kHeld = hidKeysHeld();
        isHoldingExit = (kHeld & KEY_B) != 0;
    }

    if (isHoldingExit) {
        ringAlpha += dt * 4.0f;
        if (ringAlpha > 1.0f) ringAlpha = 1.0f;

        exitProgress += dt / 3.0f;
        if (exitProgress >= 1.0f) {
            exitProgress = 1.0f;
            exit(0);
        }
    } else {
        ringAlpha -= dt * 4.0f;
        if (ringAlpha < 0.0f) ringAlpha = 0.0f;

        exitProgress -= dt * 2.5f;
        if (exitProgress < 0.0f) exitProgress = 0.0f;
    }

    // Key input detection
    if (!transitioning) {
        // Konami code detection
        u32 keysJust = hidKeysDown();
        if (keysJust) {
            static const u32 checkKeys[] = {KEY_DUP, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT, KEY_B, KEY_A};
            for (u32 k : checkKeys) {
                if (keysJust & k) {
                    konamiInput.push_back(k);
                    if (konamiInput.size() > konamiTarget.size()) {
                        konamiInput.erase(konamiInput.begin());
                    }
                    
                    if (konamiInput == konamiTarget) {
                        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                        MusicPlayer::stop();
                        transitioning = true;
                        switchTimer = 0.5f;
                        if (ClientPrefs::flashing) {
                            flash(topScreen, 1.0f, CWhite);
                            flash(bottomScreen, 1.0f, CWhite);
                        }
                    }
                    break;
                }
            }
        }

        if (transitioning && konamiInput == konamiTarget) {
            // Pending Konami debug transition
        } else if (keyJustPressed(KEY_START | KEY_A | KEY_TOUCH)) {
            if (!skippedIntro) {
                skipIntro();
            } else {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                transitioning = true;
                switchTimer = 2.0f;
                titleEnter.play("press");
                titleEnter2.play("press");
                if (ClientPrefs::flashing) {
                    flash(topScreen, 1.0f, CWhite);
                    flash(bottomScreen, 1.0f, CWhite);
                }
            }
        }
    }

    // Random promotional video countdown
    if (promoPending) {
        promoTimer -= dt;

        if (promoTimer <= 0.0f) {
            promoPending = false;
            promoFadingOut = true;
            promoFadeTime = 0.0f;

            static const std::vector<std::string> promoVideos = {
                "romfs:/preload/videos/boyfriendEverywhere.snaky",
                "romfs:/preload/videos/mobileRelease.snaky",
                "romfs:/preload/videos/riftCollabTrailer.snaky"
            };

            promoChosenVideo = promoVideos[rand() % promoVideos.size()];
        }
    }

    if (promoFadingOut) {
        promoFadeTime += dt;
        float progress = promoFadeTime / 1.5f; // 1.5 second fade out
        if (progress > 1.0f) progress = 1.0f;

        MusicPlayer::setVolume(0.7f * (1.0f - progress));

        if (progress >= 1.0f) {
            promoFadingOut = false;
            MusicPlayer::stop();
            MusicBeatState::skipTransition = true;
            switchState(new VideoState(promoChosenVideo, new TitleState()));
            return;
        }
    }
}

void TitleState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    beginScreen(top);

    if (!skippedIntro) {
        for (size_t i = 0; i < introLines.size(); i++) {
            float lineY = 60.0f + (float)i * 28.0f;
            Alphabet::draw(introLines[i], 200.0f + get3DOffset(10.0f), lineY, 1.2f, 1.0f, true);
        }
    } else {
        gf.drawCentered(270.0f + get3DOffset(-12.0f), 125.0f, 0.5f, 0.575f, 0.575f);
 
        logo.scaleX = 0.7f * logoScale;
        logo.scaleY = 0.7f * logoScale;
        logo.drawCentered(100.0f + get3DOffset(-10.0f), 70.0f, 0.6f);
 
        drawFlash(topScreen);
    }

    float promoProgress = promoFadingOut ? (promoFadeTime / 1.5f) : 0.0f;
    if (promoProgress > 1.0f) promoProgress = 1.0f;

    if (promoFadingOut) {
        u8 alpha = (u8)(promoProgress * 255.0f);
        C2D_DrawRectSolid(0, 0, 0.99f, 400.0f, 240.0f, C2D_Color32(0, 0, 0, alpha));
    }

    // BOTTOM SCREEN
    beginScreen(bottom);
 
    if (!skippedIntro) {
        if (showNewgrounds) {
            ng.drawCentered(160.0f, 120.0f, 0.8f, 1.0f, 1.0f);
        }
    } else {
        titleEnter.drawCentered(160.0f, 108.0f, 0.8f, 0.85f, 0.85f);
        titleEnter2.drawCentered(160.0f, 138.0f, 0.8f, 0.85f, 0.85f);

        if (ringAlpha > 0.0f) {
            float cx = 285.0f;
            float cy = 205.0f;
            float r_in = 12.0f;
            float r_out = 16.0f;

            u8 alphaBack = (u8)(ringAlpha * 100.0f);
            u8 alphaFront = (u8)(ringAlpha * 255.0f);

            u32 colorBack = C2D_Color32(0, 0, 0, alphaBack);
            drawProgressRing(cx, cy, r_in, r_out, 1.0f, colorBack, 0.9f);

            u32 colorFront = C2D_Color32(255, 255, 255, alphaFront);
            drawProgressRing(cx, cy, r_in, r_out, exitProgress, colorFront, 0.91f);
        }
        
        drawFlash(bottomScreen);
    }

    if (promoFadingOut) {
        u8 alpha = (u8)(promoProgress * 255.0f);
        C2D_DrawRectSolid(0, 0, 0.99f, 320.0f, 240.0f, C2D_Color32(0, 0, 0, alpha));
    }
}

void TitleState::createCoolText(const std::vector<std::string>& textArray) {
    introLines = textArray;
}

void TitleState::addMoreText(const std::string& text) {
    introLines.push_back(text);
}

void TitleState::deleteCoolText() {
    introLines.clear();
}

void TitleState::skipIntro() {
    if (!skippedIntro) {
        skippedIntro = true;
        introLines.clear();
        showNewgrounds = false;

        if (ClientPrefs::flashing) {
            flash(topScreen, 1.0f, CWhite);
            flash(bottomScreen, 1.0f, CWhite);
        }

        promoTimer = 40.0f;
        promoPending = true;
    }
}

void TitleState::exitState() {
}


//               Come, sit with me,
//                  And watch the sky.         *
//                The stars are bright,
//              The moon is high.
//                    *               *                          *
//      *      Don't be afraid,
//           Don't ask me why.
//            Just stay with me
//           A little while.    *          *
// *             *                                    *
//                   Look at the moon,
//                             How close it seems.               *
//       *                                  Maybe tomorrow
//                   *                 We'll wake from this dream.
//                             *
//       *                     So count the stars,
//                         One by one...
//         And when they're gone,               *
//                 We'll wait for dawn.......