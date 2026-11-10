#ifndef PAUSESUBSTATE_HPP
#define PAUSESUBSTATE_HPP

#include <vector>
#include <string>
#include <memory>
#include <citro2d.h>

class IPauseRenderer;
class MemoryDebugState;

class PauseSubState {
public:
    PauseSubState();
    virtual ~PauseSubState();

    void update(float dt);
    void draw();

    // Menu state
    int pauseSelection = 0;
    float pauseLerpSelection = 0.0f;

    enum PauseMenuState {
        PAUSE_MAIN,
        PAUSE_DIFFICULTY
    };
    PauseMenuState pauseMenuState = PAUSE_MAIN;

    std::vector<std::string> pauseMenuItems;
    MemoryDebugState* memoryDebugState = nullptr;
    bool isExitingMenu = false;

    // Static audio cleanup for PlayState
    static void freeNesAudio();

    // Change menu items and notify the renderer
    void changeMenu(const std::vector<std::string>& items, const std::string& title = "PAUSED");

private:
    std::vector<std::string> buildMainItems();
    void executeAction(const std::string& action);

    std::unique_ptr<IPauseRenderer> renderer;
};

#endif
