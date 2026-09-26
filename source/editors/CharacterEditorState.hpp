#ifndef CHARACTEREDITORSTATE_HPP
#define CHARACTEREDITORSTATE_HPP

#include "../backend/MusicBeatState.hpp"
#include "../objects/Character.hpp"
#include "../objects/Stage.hpp"
#include <citro2d.h>
#include <citro3d.h>
#include <vector>
#include <string>

class CharacterEditorState : public MusicBeatState {
public:
    struct UIWindow {
        std::string title;
        float x;
        float y;
        float w;
        float h;
        bool expanded;
        int id; // 0 = Settings, 1 = Ghost, 2 = Character, 3 = Animations
    };

    CharacterEditorState();
    ~CharacterEditorState();

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    C2D_TextBuf textBuf = nullptr;
    Character* charObj = nullptr;
    Character* ghostObj = nullptr;
    Stage* currentStage = nullptr;
    
    std::string currentCharacter = "bf";
    std::vector<std::string> characterList;
    int curCharIndex = 0;

    std::vector<std::string> animList;
    int curAnimIndex = 0;
    int curGhostAnimIndex = 0;

    bool showGhost = true;
    bool pcMode = false;
    bool uiExpanded = true;
    
    std::vector<UIWindow> windows;
    int draggedWindowId = -1;
    float dragWindowOffsetX = 0.0f;
    float dragWindowOffsetY = 0.0f;

    int lastTouchX = -1;
    int lastTouchY = -1;
    float saveMessageTimer = 0.0f;
    
    // UI state
    int currentTab = 0; // 0 = Settings, 1 = Ghost, 2 = Character, 3 = Animations
    int curSelected = 0;
    float ghostAlpha = 0.5f;
    bool ghostHighlight = false;
    int animSliderIndex = 0;
    float charScrollY = 0.0f;
    float animScrollY = 0.0f;
    bool isDraggingCharList = false;
    bool isDraggingAnimList = false;
    float dragStartY = 0.0f;
    float dragStartScroll = 0.0f;
    bool touchHeldLastFrame = false;
    float touchStartPx = 0.0f;
    float touchStartPy = 0.0f;

    // Camera panning
    float camX = 0;
    float camY = 0;
    float camZoom = 1.0f;

    C2D_Font vcrFont = nullptr;
    bool hasDraggedList = false;
    int resizingWindowId = -1;
    float pcUiScale = 1.0f;
    bool touchMoved = false;
    bool showAnimOverlay = true;
    bool showCamBounds = false;
    float simCamZoom = 0.90f;

    // Reference character
    Character* refObj = nullptr;
    int curRefCharIndex = 0;
    bool showRefChar = true;
    float refAlpha = 0.6f;

    std::vector<std::string> refAnimList;
    int curRefAnimIndex = 0;
    int refAnimSliderIndex = 0;
    float refCharScrollY = 0.0f;
    float refAnimScrollY = 0.0f;
    bool isDraggingRefCharList = false;
    bool isDraggingRefAnimList = false;

    int dragTargetMode = 0; // 0 = Player (P), 1 = Reference (R)

    // Helpers
    void drawText(const std::string& text, float x, float y, float scale, bool centered = false, u32 color = C2D_Color32(255,255,255,255), float depth = 0.85f, float maxWidth = 0.0f, bool rightAlign = false, bool centerY = false);
    void drawStyledButton(float x, float y, float w, float h, const std::string& title, const std::string& subtitle, u32 topCol, u32 botCol, u32 borderCol);

    void renderCheckbox(float x, float y, bool checked, bool selected, float depth = 0.5f);
    void renderSlider(float x, float y, float w, float valPct, bool selected, float depth = 0.5f);
    void renderButton(float x, float y, float w, float h, const std::string& label, bool selected, float depth = 0.5f);
    void renderStepper(float itemX, float itemY, float itemW, const std::string& label, const std::string& valStr, bool selected, float depth = 0.5f);
    void renderArrowsVal(float itemX, float itemY, float itemW, const std::string& label, const std::string& valStr, bool selected, float depth = 0.5f);
    void renderListBox(float bx, float by, float bw, float bh, const std::vector<std::string>& items, int selectedIdx, float scrollY, bool activeSelected, float depth = 0.5f);
    void renderLeftAccent(float itemX, float itemY, float h, float depth = 0.5f);

    void loadCharacter(const std::string& name);
    void loadRefCharacter(const std::string& name);
    void updateAnimList();
    void updateRefAnimList();
    void playCurAnim();
    void playCurRefAnim();
    void saveCharacter();
    void centerCameraOnTarget();
};

#endif
