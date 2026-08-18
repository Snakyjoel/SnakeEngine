#ifndef PAUSESUBSTATE_HPP
#define PAUSESUBSTATE_HPP

#include <vector>
#include <string>
#include <citro2d.h>

class PauseSubState {
public:
    PauseSubState();
    ~PauseSubState();

    void update(float dt);
    void draw();

    // Menu selections
    int pauseSelection = 0;
    float pauseLerpSelection = 0.0f;

    enum PauseMenuState {
        PAUSE_MAIN,
        PAUSE_DIFFICULTY
    };
    PauseMenuState pauseMenuState = PAUSE_MAIN;

    std::vector<std::string> pauseMenuItems;
    
private:
    void setupPauseMenu(const std::vector<std::string>& items, const std::string& title = "PAUSED");

    C2D_TextBuf pauseTextBuf;
    C2D_Text pauseTitleObj;
    C2D_Text pauseOptionsObj[15];
};

#endif
