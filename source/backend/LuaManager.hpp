#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <lua5.1/lua.h>
#include <lua5.1/lualib.h>
#include <lua5.1/lauxlib.h>
#ifdef __cplusplus
}
#endif

#include <string>
#include <vector>

class LuaManager {
public:
    static LuaManager& get() {
        static LuaManager instance;
        return instance;
    }

    void init();
    void close();
    void clearAllSprites();

    bool runScript(const std::string& scriptPath);
    bool callFunction(const std::string& funcName, const std::vector<std::string>& args = {});
    void syncGlobals();


    // Scaling to FunkinLua macros/systems
    void registerFunctions(lua_State* L);

    // Standard FunkinLua Binds
    static int lua_makeLuaSprite(lua_State* L);
    static int lua_loadGraphic(lua_State* L);
    static int lua_makeGraphic(lua_State* L);
    static int lua_scaleObject(lua_State* L);
    static int lua_setScrollFactor(lua_State* L);
    static int lua_makeAnimatedLuaSprite(lua_State* L);
    static int lua_precacheImage(lua_State* L);
    static int lua_addAnimationByPrefix(lua_State* L);
    static int lua_objectPlayAnimation(lua_State* L);
    static int lua_playAnimFES(lua_State* L);
    static int lua_addLuaSprite(lua_State* L);
    static int lua_setSpriteVram(lua_State* L);
    
    // Manual Texture Mode Functions
    static int lua_setTextureManualMode(lua_State* L);
    static int lua_loadTexture(lua_State* L);
    static int lua_unloadTexture(lua_State* L);
    static int lua_preloadTexture(lua_State* L);
    static int lua_isTextureReady(lua_State* L);
    
    // Future Expansion (getProperty/setProperty)
    static int lua_getProperty(lua_State* L);
    static int lua_setProperty(lua_State* L);
    static int lua_debugPrint(lua_State* L);
    static int lua_setObjectCamera(lua_State* L);
    static int lua_setCameraExtended(lua_State* L);
    static int lua_screenCenter(lua_State* L);
    static int lua_cameraShake(lua_State* L);
    static int lua_triggerEvent(lua_State* L);
    static int lua_doTweenX(lua_State* L);
    static int lua_doTweenY(lua_State* L);
    static int lua_doTweenAngle(lua_State* L);
    static int lua_doTweenAlpha(lua_State* L);
    static int lua_doTweenZoom(lua_State* L);
    static int lua_doTweenColor(lua_State* L);
    static int lua_doTweenScale(lua_State* L);
    static int lua_doTweenScaleX(lua_State* L);
    static int lua_doTweenScaleY(lua_State* L);
    static int lua_noteTweenX(lua_State* L);
    static int lua_noteTweenY(lua_State* L);
    static int lua_noteTweenAngle(lua_State* L);
    static int lua_noteTweenAlpha(lua_State* L);
    static int lua_noteTweenDirection(lua_State* L);
    static int lua_cancelTween(lua_State* L);
    static int lua_runTimer(lua_State* L);
    static int lua_cancelTimer(lua_State* L);
    static int lua_setObjectOrder(lua_State* L);
    static int lua_getObjectOrder(lua_State* L);
    static int lua_playSound(lua_State* L);
    static int lua_startVideo(lua_State* L);
    static int lua_stopVideo(lua_State* L);
    static int lua_pauseVideo(lua_State* L);
    static int lua_resumeVideo(lua_State* L);
    static int lua_getColorFromHex(lua_State* L);
    static int lua_keyJustPressed(lua_State* L);
    static int lua_keyPressed(lua_State* L);
    static int lua_keyReleased(lua_State* L);
    static int lua_setFpsLimit(lua_State* L);


    // Position functions
    static int lua_getCharacterX(lua_State* L);
    static int lua_getCharacterY(lua_State* L);
    static int lua_setCharacterX(lua_State* L);
    static int lua_setCharacterY(lua_State* L);
    static int lua_getMidpointX(lua_State* L);
    static int lua_getMidpointY(lua_State* L);

    // String & Utility functions
    static int lua_getRandomBool(lua_State* L);
    static int lua_getRandomFloat(lua_State* L);
    static int lua_stringStartsWith(lua_State* L);
    static int lua_stringEndsWith(lua_State* L);
    static int lua_stringTrim(lua_State* L);
    static int lua_stringSplit(lua_State* L);

    // Audio functions
    static int lua_precacheSound(lua_State* L);
    static int lua_precacheMusic(lua_State* L);
    static int lua_stopSound(lua_State* L);
    static int lua_pauseSound(lua_State* L);
    static int lua_resumeSound(lua_State* L);
    static int lua_getSoundVolume(lua_State* L);
    static int lua_setSoundVolume(lua_State* L);

    // Missing Psych API stubs to prevent crashes
    static int lua_getPropertyFromGroup(lua_State* L);
    static int lua_setPropertyFromGroup(lua_State* L);
    static int lua_removeLuaSprite(lua_State* L);
    static int lua_getRandomInt(lua_State* L);
    static int lua_updateHitbox(lua_State* L);

    // Mouse & Key Input Functions (3DS Touch Adaptation)
    static int lua_mouseClicked(lua_State* L);
    static int lua_mousePressed(lua_State* L);
    static int lua_mouseReleased(lua_State* L);
    static int lua_getMouseX(lua_State* L);
    static int lua_getMouseY(lua_State* L);
    static int lua_mouseClickedOnSprite(lua_State* L);
    static int lua_mouseOverlapSprite(lua_State* L);

    // Text Functions
    static int lua_makeLuaText(lua_State* L);
    static int lua_setTextString(lua_State* L);
    static int lua_setTextSize(lua_State* L);
    static int lua_setTextColor(lua_State* L);
    static int lua_setTextAlignment(lua_State* L);
    static int lua_setTextBorder(lua_State* L);
    static int lua_addLuaText(lua_State* L);
    static int lua_luaTextExists(lua_State* L);
    static int lua_luaSpriteExists(lua_State* L);
    
    // Stats & Control
    static int lua_getHealth(lua_State* L);
    static int lua_setHealth(lua_State* L);
    static int lua_addHealth(lua_State* L);
    static int lua_getScore(lua_State* L);
    static int lua_setScore(lua_State* L);
    static int lua_addScore(lua_State* L);

    // Shader System
    static int lua_setCameraShader(lua_State* L);
    static int lua_removeCameraShader(lua_State* L);
    static int lua_setShaderFloat(lua_State* L);
    static int lua_setShaderParam(lua_State* L);

    static int lua_getMisses(lua_State* L);
    static int lua_setMisses(lua_State* L);
    static int lua_addMisses(lua_State* L);
    static int lua_getHits(lua_State* L);
    static int lua_setHits(lua_State* L);
    static int lua_addHits(lua_State* L);
    static int lua_getSongPosition(lua_State* L);
    static int lua_restartSong(lua_State* L);
    static int lua_exitSong(lua_State* L);
    static int lua_characterDance(lua_State* L);
    static int lua_setGridVisible(lua_State* L);

    // 3DS Hardware Control
    static int lua_setScreenState(lua_State* L);   // Turn screens on/off
    static int lua_setHomeAllowed(lua_State* L);   // Block/unblock Home button
    static int lua_setLedColor(lua_State* L);      // Control notification LED color/pattern
    static int lua_setLedFlash(lua_State* L);      // One-shot flash: instant on, smooth decay
    static int lua_crashGame(lua_State* L);        // Intentionally crash the game

    // Option Manager API
    static int lua_getOption(lua_State* L);
    static int lua_setOption(lua_State* L);
    static int lua_getModOption(lua_State* L);
    static int lua_setModOption(lua_State* L);


private:
    LuaManager() = default;
    ~LuaManager() = default;

    void registerExcludedFunctions(lua_State* L);
    static bool checkCustomTweenTarget(const std::string& tag, const std::string& prop, float& outStartValue);

    lua_State* L = nullptr;
    int scriptCount = 0;
};
