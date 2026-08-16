#include "TitleState.hpp"
#include "MainMenuState.hpp"
#include "../debug/DebugMenuState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include <citro2d.h>
#include "../objects/Alphabet.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include "VideoState.hpp"
#include "OutdatedState.hpp"
#include "../backend/UpdateChecker.hpp"

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

    gf.loadSheet("preload/images/gfDanceTitle");
    CachedSpritesheet* tempGf = SpritesheetCache::get().load("preload/images/gfDanceTitle");
    if (tempGf) {
        int matchedCount = 0;
        for (const auto& f : tempGf->frames) {
            if (f.name.find("gfDance") == 0) matchedCount++;
        }
        int mid = matchedCount / 2;
        std::vector<int> leftIdx, rightIdx;
        for (int i = 0; i < matchedCount; i++) {
            if (i < mid) leftIdx.push_back(i);
            else rightIdx.push_back(i);
        }
        gf.addAnim("danceLeft", "gfDance", 24.0f, false, 0.0f, 0.0f, leftIdx);
        gf.addAnim("danceRight", "gfDance", 24.0f, false, 0.0f, 0.0f, rightIdx);
    }
    gf.play("danceLeft");
    gf.antialiasing = ClientPrefs::globalAntialiasing;

    SpritesheetCache::get().load("shared/images/Alphabet");

    // Start menu music
    if (!MusicPlayer::isPlaying()) {
        MusicPlayer::playMenuMusic();
    }

    // Pick random wacky text
    static bool seedSet = false;
    if (!seedSet) {
        srand(time(NULL));
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
                std::string part1 = lineStr.substr(0, splitPos);
                std::string part2 = lineStr.substr(splitPos + 2);
                loadedTexts.push_back({part1, part2});
            }
        }
        fclose(fIntro);
    }

    if (!loadedTexts.empty()) {
        int idx = rand() % loadedTexts.size();
        line1 = loadedTexts[idx].first;
        line2 = loadedTexts[idx].second;
    } else {
        int idx = rand() % defaultWackyTexts.size();
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

    // GF dancing logic
    gfDanceLeftActive = !gfDanceLeftActive;
    if (gfDanceLeftActive) {
        gf.play("danceLeft", true);
    } else {
        gf.play("danceRight", true);
    }

    // Intro sequence
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
    // Update music stream
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

    // Logo scale zoom
    logoScale = 1.0f + (logoScale - 1.0f) * std::exp(-12.0f * dt);

    // Switch state timer
    if (transitioning) {
        switchTimer -= dt;
        if (switchTimer <= 0.0f) {
            if (konamiInput == konamiTarget) {
                switchState(new DebugMenuState());
            } else {
                if (ClientPrefs::checkForUpdates && UpdateChecker::isChecking() && !UpdateChecker::isFinished()) {
                    updateWaitTimer += dt;
                    if (updateWaitTimer < 3.0f) {
                        return; // Wait up to 3s for version check thread
                    }
                }
                if (ClientPrefs::checkForUpdates && UpdateChecker::isFinished() && !UpdateChecker::getOnlineVersion().empty()) {
                    int comp = UpdateChecker::compareVersions(UpdateChecker::getCurrentVersion(), UpdateChecker::getOnlineVersion());
                    if (comp != 0) {
                        switchState(new OutdatedState(comp, UpdateChecker::getOnlineVersion()));
                        return;
                    }
                }
                switchState(new MainMenuState());
            }
        }
    }

    // Key presses
    if (!transitioning) {
        // Track Konami Code
        u32 keysJust = hidKeysDown();
        if (keysJust) {
            u32 checkKeys[] = {KEY_DUP, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT, KEY_B, KEY_A};
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
                        flash(topScreen, 1.0f, CWhite);
                        flash(bottomScreen, 1.0f, CWhite);
                    }
                    break;
                }
            }
        }

        if (transitioning && konamiInput == konamiTarget) {
        } else if (keyJustPressed(KEY_START | KEY_A | KEY_TOUCH)) {
            if (!skippedIntro) {
                skipIntro();
            } else {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                transitioning = true;
                switchTimer = 2.0f;
                titleEnter.play("press");
                titleEnter2.play("press");
                flash(topScreen, 1.0f, CWhite);
                flash(bottomScreen, 1.0f, CWhite);
            }
        }
    }

    // Random video
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
            MusicBeatState::skipTransition = true; // Skip normal curtain fade transition
            switchState(new VideoState(promoChosenVideo, new TitleState()));
            return;
        }
    }
}

void TitleState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    beginScreen(top);

    if (!skippedIntro) {
        for (size_t i = 0; i < introLines.size(); i++) {
            float lineY = 60.0f + i * 28.0f;
            Alphabet::draw(introLines[i], 200.0f, lineY, 1.2f, 1.0f, true);
        }
    } else {
        gf.drawCentered(270.0f, 125.0f, 0.5f, 0.575f, 0.575f);
 
        logo.scaleX = 0.7f * logoScale;
        logo.scaleY = 0.7f * logoScale;
        logo.drawCentered(100.0f, 70.0f, 0.6f);
 
        drawFlash(topScreen);
    }

    if (promoFadingOut) {
        float progress = promoFadeTime / 1.5f;
        if (progress > 1.0f) progress = 1.0f;
        u8 alpha = (u8)(progress * 255.0f);
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
        
        drawFlash(bottomScreen);
    }

    if (promoFadingOut) {
        float progress = promoFadeTime / 1.5f;
        if (progress > 1.0f) progress = 1.0f;
        u8 alpha = (u8)(progress * 255.0f);
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

        flash(topScreen, 1.0f, CWhite);
        flash(bottomScreen, 1.0f, CWhite);

        promoTimer = 40.0f;
        promoPending = true;
    }
}

void TitleState::exitState() {
}
