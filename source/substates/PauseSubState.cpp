#include "PauseSubState.hpp"
#include "../states/PlayState.hpp"
#include "../options/OptionsMenuState.hpp"
#include "../states/StoryMenuState.hpp"
#include "../states/FreeplayState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/Alphabet.hpp"
#include "../objects/InGameVideoPlayer.hpp"
#include "../backend/Macros.hpp"

extern C2D_Font globalVCRFont;

PauseSubState::PauseSubState() {
    pauseTextBuf = C2D_TextBufNew(1024);
    
    std::vector<std::string> mainItems = {"Resume", "Restart Song"};
    if (PlayState::instance->curSongDifficulties.size() >= 2) {
        mainItems.push_back("Change Difficulty");
    }
    mainItems.push_back("Options");
    mainItems.push_back("Exit to menu");
    
    setupPauseMenu(mainItems, "PAUSED");
}

PauseSubState::~PauseSubState() {
    if (pauseTextBuf) {
        C2D_TextBufDelete(pauseTextBuf);
    }
}

void PauseSubState::setupPauseMenu(const std::vector<std::string>& items, const std::string& title) {
    if (!pauseTextBuf || !globalVCRFont) return;
    pauseMenuItems = items;
    C2D_TextBufClear(pauseTextBuf);
    C2D_TextFontParse(&pauseTitleObj, globalVCRFont, pauseTextBuf, title.c_str());
    C2D_TextOptimize(&pauseTitleObj);
    for (size_t i = 0; i < pauseMenuItems.size() && i < 15; i++) {
        C2D_TextFontParse(&pauseOptionsObj[i], globalVCRFont, pauseTextBuf, pauseMenuItems[i].c_str());
        C2D_TextOptimize(&pauseOptionsObj[i]);
    }
}

void PauseSubState::update(float dt) {
    u32 kDown = hidKeysDown();
    
    pauseLerpSelection += (pauseSelection - pauseLerpSelection) * (dt * 15.0f);
    int maxSel = (int)pauseMenuItems.size() - 1;
    if (maxSel < 0) maxSel = 0;
    
    if (kDown & (KEY_DUP | KEY_CPAD_UP)) { 
        pauseSelection--; 
        if (pauseSelection < 0) pauseSelection = maxSel; 
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) { 
        pauseSelection++; 
        if (pauseSelection > maxSel) pauseSelection = 0; 
    }
    
    if (kDown & KEY_B) {
        if (pauseMenuState == PAUSE_DIFFICULTY) {
            pauseMenuState = PAUSE_MAIN;
            std::vector<std::string> mainItems = {"Resume", "Restart Song"};
            if (PlayState::instance->curSongDifficulties.size() >= 2) mainItems.push_back("Change Difficulty");
            mainItems.push_back("Options");
            mainItems.push_back("Exit to menu");
            setupPauseMenu(mainItems, "PAUSED");
            pauseSelection = 0;
            pauseLerpSelection = 0.0f;
        } else {
            PlayState::instance->paused = false;
            AudioEngine::resume();
            if (PlayState::instance->inGameVideo) PlayState::instance->inGameVideo->isPaused = false;
        }
        return;
    }
    
    if (kDown & KEY_A) {
        if (pauseMenuState == PAUSE_MAIN) {
            std::string sel = pauseMenuItems[pauseSelection];
            if (sel == "Resume") {
                PlayState::instance->paused = false;
                AudioEngine::resume();
                if (PlayState::instance->inGameVideo) PlayState::instance->inGameVideo->isPaused = false;
            } else if (sel == "Restart Song") {
                if (PlayState::instance->isStoryMode) {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->weekData, PlayState::instance->curSongIdx, PlayState::instance->currentDifficulty));
                } else {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, PlayState::instance->currentDifficulty));
                }
            } else if (sel == "Change Difficulty") {
                pauseMenuState = PAUSE_DIFFICULTY;
                std::vector<std::string> diffItems = PlayState::instance->curSongDifficulties;
                diffItems.push_back("BACK");
                setupPauseMenu(diffItems, "CHANGE DIFFICULTY");
                pauseSelection = 0;
                pauseLerpSelection = 0.0f;
            } else if (sel == "Options") {
                OptionsMenuState::onPlayState = true;
                OptionsMenuState::isStoryMode = PlayState::instance->isStoryMode;
                OptionsMenuState::songName = PlayState::instance->curSong;
                OptionsMenuState::difficultyName = PlayState::instance->currentDifficulty;
                if (PlayState::instance->isStoryMode) {
                    OptionsMenuState::storyWeek = PlayState::instance->weekData;
                    OptionsMenuState::storySongIdx = PlayState::instance->curSongIdx;
                }
                MusicBeatState::switchState(new OptionsMenuState());
            } else if (sel == "Exit to menu") {
                if (PlayState::instance->isStoryMode) {
                    MusicBeatState::switchState(new StoryMenuState());
                } else {
                    MusicBeatState::switchState(new FreeplayState());
                }
            }
        } else if (pauseMenuState == PAUSE_DIFFICULTY) {
            std::string sel = pauseMenuItems[pauseSelection];
            if (sel == "BACK" || pauseSelection == (int)pauseMenuItems.size() - 1) {
                pauseMenuState = PAUSE_MAIN;
                std::vector<std::string> mainItems = {"Resume", "Restart Song"};
                if (PlayState::instance->curSongDifficulties.size() >= 2) mainItems.push_back("Change Difficulty");
                mainItems.push_back("Options");
                mainItems.push_back("Exit to menu");
                setupPauseMenu(mainItems, "PAUSED");
                pauseSelection = 0;
                pauseLerpSelection = 0.0f;
            } else {
                if (PlayState::instance->isStoryMode) {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->weekData, PlayState::instance->curSongIdx, sel));
                } else {
                    MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, sel));
                }
            }
        }
    }
}

void PauseSubState::draw() {
    C2D_SceneBegin(PlayState::instance->top);
    float bw = (float)ScreenWidthTop;
    float bh = (float)ScreenHeight;
    
    C2D_DrawRectSolid(0, 0, 0.99f, bw, bh, C2D_Color32(0, 0, 0, 150));
    
    if (ClientPrefs::alphabetPause) {
        for (int i = 0; i < (int)pauseMenuItems.size(); i++) {
            bool sel = (i == pauseSelection);
            
            float targetY = (bh / 2.0f) - (35.0f / 2.0f) + (i - pauseLerpSelection) * 38.0f;
            float targetX = 10.0f + (i - pauseLerpSelection) * 20.0f;
            
            float scale = 1.5f;
            float alpha = sel ? 1.0f : 0.6f;
            Alphabet::draw(pauseMenuItems[i], targetX, targetY, scale, alpha, false, 0xFFFFFFFF, 1.0f);
        }
    } else {
        for (int i = 0; i < (int)pauseMenuItems.size() && i < 15; i++) {
            bool sel = (i == pauseSelection);
            
            float targetY = (bh / 2.0f) - (35.0f / 2.0f) + (i - pauseLerpSelection) * 38.0f;
            float targetX = 10.0f + (i - pauseLerpSelection) * 20.0f;
            
            float fScale = sel ? 0.85f : 0.6f;
            u32 color = sel ? CWhite : C2D_Color32(160, 160, 160, 200);
            
            DrawTextBorderFull(&pauseOptionsObj[i], targetX, targetY, 0.995f, fScale, fScale, 2.0f, CBlack);
            C2D_DrawText(&pauseOptionsObj[i], C2D_WithColor, targetX, targetY, 1.0f, fScale, fScale, color);
        }
    }
}
