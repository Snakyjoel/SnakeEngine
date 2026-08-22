#pragma once

class ClientPrefs {
public:
    static bool downscroll;
    static bool ghostTapping;
    static bool debugInfo;
    static bool extendedDebug;
    static bool middleScroll;
    static bool opponentStrums;
    static bool opponentNotes;
    
    static bool camZooms;
    static int timeBarType;
    static bool healthBar;
    static bool scoreZoom;
    static bool lowQuality;
    static bool showRatings;
    static bool fastNotes;
    static bool globalAntialiasing;
    
    static bool flashing;
    static bool disableReset;
    static bool botPlay;
    static int noteOffset;
    static bool noteColorsEnabled;
    static unsigned int noteKeys[4][2];
    static unsigned char noteColors[4][3];
    
    static bool drawGrid;
    static float comboOffsetX;
    static float comboOffsetY;
    static float comboScale;
    static float comboAlpha;
    
    static bool alphabetPause;
    static bool checkForUpdates;
    static bool buttonPrompts;
    
    static bool hitboxEnabled;
    static int hitboxMode;
    static int hitboxStyle;
    static int hitboxAlphaNormal;
    static int hitboxAlphaTouch;
    
    static float noteUnderlayAlpha;
    static bool opponentUnderlay;
    
    static bool acceptedEgg;
    static bool eggInteractionOccurred;
    static int fpsLimit;
    
    static void loadSettings();
    static void saveSettings();
};

