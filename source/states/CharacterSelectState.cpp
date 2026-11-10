#include "CharacterSelectState.hpp"
#include "FreeplayState.hpp"
#include "MainMenuState.hpp"
#include "VideoState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/Macros.hpp"
#include "../objects/Alphabet.hpp"
#include <citro2d.h>

CharacterSelectState::CharacterSelectState(bool playedVideo)
    : playedVideo(playedVideo) {
}

void CharacterSelectState::init() {
    if (!playedVideo) {
        switchState(new VideoState("introSelect", new CharacterSelectState(true), true));
        return;
    }
    SpritesheetCache::get().load("shared/images/Alphabet");
}

void CharacterSelectState::update(float dt) {
    u32 kDown = hidKeysDown();
    
    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        curSelected--;
        if (curSelected < 0) curSelected = characters.size() - 1;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected >= (int)characters.size()) curSelected = 0;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.adp", 0.7f);
    }
    
    if (kDown & KEY_A) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.adp", 0.7f);
        FreeplayState::currentChar = characters[curSelected];
        switchState(new MainMenuState(true));
    }
    
    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.adp", 0.7f);
        switchState(new MainMenuState(true));
    }
}

void CharacterSelectState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    
    for (int i = 0; i < (int)characters.size(); i++) {
        float alpha = (i == curSelected) ? 1.0f : 0.6f;
        Alphabet::draw(characters[i], 100.0f, 60.0f + (i * 60.0f), 0.8f, alpha, false);
    }
    
    if (!g_isRightEye && bottom) {
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
    }
}

void CharacterSelectState::exitState() {
}
