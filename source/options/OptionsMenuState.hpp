#pragma once
#include <string>
#include <vector>
#include "../backend/MusicBeatState.hpp"
#include "../backend/WeekData.hpp"
#include "../backend/savedata/OptionManager.hpp"
#include <citro2d.h>

class OptionsMenuState : public MusicBeatState {
public:
    static bool onPlayState;
    static bool isStoryMode;
    static std::string songName;
    static std::string difficultyName;
    static WeekData storyWeek;
    static int storySongIdx;

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;
    
private:
    enum MenuState {
        STATE_MAIN,
        STATE_CATEGORY,
        STATE_CONTROLS,
        STATE_NOTE_COLORS
    };
    MenuState menuState = STATE_MAIN;
    int activeCategoryIndex = 0;
    int curSelected = 0;
    float lerpSelected = 0.0f;
    float gridOffset = 0.0f;
    
    // Controls rebinding
    bool isBinding   = false;
    int  bindingLane = 0;
    int  bindingIdx  = 0;

    // Note Colors: which note is being edited (0-3)
    int colorNoteSelected = 0;
    float lerpColorNoteSelected = 0.0f;

    // Background images
    C2D_SpriteSheet bgSheet       = nullptr;
    C2D_SpriteSheet bottomBGSheet = nullptr;
    C2D_Image topBG;
    C2D_Image bottomBG;

    // Note sprites for color picker preview
    C2D_SpriteSheet noteSheetNormal = nullptr;
    C2D_SpriteSheet noteSheetFast   = nullptr;
    C2D_Image       baseNoteImgNormal;
    C2D_Image       baseNoteImgFast;
    std::vector<NoteSprite> noteSubsNormal;
    std::vector<NoteSprite> noteSubsFast;

    C2D_Font   vcrFont    = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;

public:
    struct CheckboxState {
        bool checked = false;
        std::string currentAnim = "unchecked"; // "unchecked", "checking", "checked", "unchecking"
        float animTime = 0.0f;
    };

private:
    std::vector<CheckboxState> checkboxStates;

    void initCheckboxesForCategory(int catIdx);
    void triggerCheckbox(int idx, bool checked);
    void updateCheckboxAnims(float dt);

    std::string getKeyName(unsigned int key);
    void drawNoteSprite(int noteData, float x, float y, float scale, bool fast);
};
