#include "PauseSubState.hpp"
#include "NormalPauseRenderer.hpp"
#include "PixelPauseRenderer.hpp"
#include "MemoryDebugState.hpp"
#include "../states/PlayState.hpp"
#include "../options/OptionsMenuState.hpp"
#include "../backend/Macros.hpp"
#include "../states/StoryMenuState.hpp"
#include "../states/MainMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/InGameVideoPlayer.hpp"
#include "../backend/savedata/ClientPrefs.hpp"

std::vector<std::string> PauseSubState::buildMainItems() {
    std::vector<std::string> mainItems = {"Resume", "Restart Song"};
    if (PlayState::instance && PlayState::instance->curSongDifficulties.size() >= 2) {
        mainItems.push_back("Change Difficulty");
    }
    if (ClientPrefs::debugInfo) {
        mainItems.push_back("Memory Debug");
    }
    mainItems.push_back("Options");
    mainItems.push_back("Exit to menu");
    return mainItems;
}

PauseSubState::PauseSubState() {
    pauseMenuItems = buildMainItems();

    if (PlayState::instance && PlayState::instance->isPixelStage) {
        renderer = std::unique_ptr<IPauseRenderer>(new PixelPauseRenderer(this));
    } else {
        renderer = std::unique_ptr<IPauseRenderer>(new NormalPauseRenderer(this));
    }

    if (renderer) {
        renderer->init();
    }
}

PauseSubState::~PauseSubState() {
    if (memoryDebugState) {
        delete memoryDebugState;
        memoryDebugState = nullptr;
    }
}

void PauseSubState::freeNesAudio() {
    PixelPauseRenderer::freeAudio();
}

void PauseSubState::changeMenu(const std::vector<std::string>& items, const std::string& title) {
    pauseMenuItems = items;
    pauseSelection = 0;
    pauseLerpSelection = 0.0f;
    if (renderer) {
        renderer->onMenuChanged(items, title);
    }
}

void PauseSubState::update(float dt) {
    if (isExitingMenu) return;

    if (memoryDebugState) {
        memoryDebugState->update(dt);
        if (!memoryDebugState->active) {
            delete memoryDebugState;
            memoryDebugState = nullptr;
        }
        return;
    }

    if (renderer) {
        renderer->update(dt);
    }

    pauseLerpSelection += (pauseSelection - pauseLerpSelection) * (dt * 15.0f);
    int maxSel = (int)pauseMenuItems.size() - 1;
    if (maxSel < 0) maxSel = 0;

    u32 kDown = hidKeysDown();

    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        pauseSelection--;
        if (pauseSelection < 0) pauseSelection = maxSel;
        if (renderer) renderer->onCursorMove();
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        pauseSelection++;
        if (pauseSelection > maxSel) pauseSelection = 0;
        if (renderer) renderer->onCursorMove();
    }

    if (kDown & KEY_B) {
        if (renderer) renderer->onCancel();
        if (pauseMenuState == PAUSE_DIFFICULTY) {
            pauseMenuState = PAUSE_MAIN;
            changeMenu(buildMainItems(), "PAUSED");
        } else {
            if (PlayState::instance) {
                PlayState::instance->paused = false;
                AudioEngine::resume();
                if (PlayState::instance->inGameVideo) {
                    PlayState::instance->inGameVideo->isPaused = false;
                }
            }
        }
        return;
    }

    if (kDown & KEY_A) {
        if (renderer) renderer->onSelect();
        if (pauseSelection >= 0 && pauseSelection < (int)pauseMenuItems.size()) {
            executeAction(pauseMenuItems[pauseSelection]);
        }
    }
}

void PauseSubState::executeAction(const std::string& action) {
    if (!PlayState::instance) return;

    if (pauseMenuState == PAUSE_MAIN) {
        if (action == "Resume") {
            PlayState::instance->paused = false;
            AudioEngine::resume();
            if (PlayState::instance->inGameVideo) {
                PlayState::instance->inGameVideo->isPaused = false;
            }
        } else if (action == "Restart Song") {
            if (isExitingMenu) return;
            isExitingMenu = true;
            PlayState::blueBalls = 0;
            if (PlayState::instance->isStoryMode) {
                MusicBeatState::switchState(new PlayState(PlayState::instance->weekData, PlayState::instance->curSongIdx, PlayState::instance->currentDifficulty));
            } else {
                MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, PlayState::instance->currentDifficulty, PlayState::instance->modDirectory));
            }
        } else if (action == "Change Difficulty") {
            pauseMenuState = PAUSE_DIFFICULTY;
            std::vector<std::string> diffItems = PlayState::instance->curSongDifficulties;
            diffItems.push_back("BACK");
            changeMenu(diffItems, "CHANGE DIFFICULTY");
        } else if (action == "Options") {
            OptionsMenuState::onPlayState = true;
            OptionsMenuState::isStoryMode = PlayState::instance->isStoryMode;
            OptionsMenuState::songName = PlayState::instance->curSong;
            OptionsMenuState::difficultyName = PlayState::instance->currentDifficulty;
            OptionsMenuState::modDir = PlayState::instance->modDirectory;
            if (PlayState::instance->isStoryMode) {
                OptionsMenuState::storyWeek = PlayState::instance->weekData;
                OptionsMenuState::storySongIdx = PlayState::instance->curSongIdx;
            }
            MusicBeatState::switchState(new OptionsMenuState());
        } else if (action == "Memory Debug") {
            memoryDebugState = new MemoryDebugState();
        } else if (action == "Exit to menu") {
            if (isExitingMenu) return;
            isExitingMenu = true;
            PlayState::blueBalls = 0;
            if (PlayState::instance->isStoryMode) {
                MusicBeatState::switchState(new StoryMenuState());
            } else {
                MusicBeatState::switchState(new MainMenuState(true));
            }
        }
    } else if (pauseMenuState == PAUSE_DIFFICULTY) {
        if (action == "BACK" || pauseSelection == (int)pauseMenuItems.size() - 1) {
            pauseMenuState = PAUSE_MAIN;
            changeMenu(buildMainItems(), "PAUSED");
        } else {
            PlayState::blueBalls = 0;
            if (PlayState::instance->isStoryMode) {
                MusicBeatState::switchState(new PlayState(PlayState::instance->weekData, PlayState::instance->curSongIdx, action));
            } else {
                MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, action, PlayState::instance->modDirectory));
            }
        }
    }
}

void PauseSubState::draw() {
    if (memoryDebugState) {
        memoryDebugState->draw();
        return;
    }

    if (!PlayState::instance) return;

    if (PlayState::instance->top && renderer) {
        C2D_SceneBegin(PlayState::instance->top);
        renderer->drawTop(PlayState::instance->top, (float)ScreenWidthTop, (float)ScreenHeight);
    }

    if (!g_isRightEye && PlayState::instance->bottom && renderer) {
        C2D_SceneBegin(PlayState::instance->bottom);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
        renderer->drawBottom(PlayState::instance->bottom, (float)ScreenWidthBot, (float)ScreenHeight);
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    }
}
