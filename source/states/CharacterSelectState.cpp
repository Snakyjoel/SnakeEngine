#include "CharacterSelectState.hpp"
#include "FreeplayState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/Alphabet.hpp"
#include <citro2d.h>

void CharacterSelectState::init() {
    SpritesheetCache::get().load("shared/images/Alphabet");
}

void CharacterSelectState::update(float dt) {
    u32 kDown = hidKeysDown();
    
    if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
        curSelected--;
        if (curSelected < 0) curSelected = characters.size() - 1;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        curSelected++;
        if (curSelected >= (int)characters.size()) curSelected = 0;
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }
    
    if (kDown & KEY_A) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
        FreeplayState::currentChar = characters[curSelected];
        switchState(new FreeplayState());
    }
    
    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        switchState(new FreeplayState());
    }
}

void CharacterSelectState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    
    for (int i = 0; i < (int)characters.size(); i++) {
        float alpha = (i == curSelected) ? 1.0f : 0.6f;
        Alphabet::draw(characters[i], 100.0f, 60.0f + (i * 60.0f), 0.8f, alpha, false);
    }
    
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
}

void CharacterSelectState::exitState() {
}
