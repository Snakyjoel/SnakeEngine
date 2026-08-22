#include "LuaManager.hpp"
#include "ModHandler.hpp"
#include "ShaderManager.hpp"
#include "../objects/InGameVideoPlayer.hpp"
#include "SpritesheetCache.hpp"
#include "savedata/OptionManager.hpp"
extern void logToFile(const std::string& msg);
#include "../states/MainMenuState.hpp"
#include "AsyncAssetManager.hpp"
#include "HexParser.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

#include "stb_image.h"

static inline bool addrIsVRAM(const void* addr) {
    uintptr_t a = (uintptr_t)addr;
    return a >= 0x1F000000 && a < (0x1F000000 + 0x600000);
}

static inline u32 getLuaColorOrHex(lua_State* L, int idx, u32 defaultColor = 0xFFFFFFFF) {
    if (lua_type(L, idx) == LUA_TSTRING) {
        return HexParser::parseStringToC2D(lua_tostring(L, idx), defaultColor);
    }
    return (u32)(long long)lua_tonumber(L, idx);
}

void LuaManager::init() {
    close(); // Clean up previous state if any
    L = luaL_newstate();
    if (!L) {
        printf("WARN: Failed to create Lua state!\n");
        return;
    }
    luaL_openlibs(L);
    registerFunctions(L);
    scriptCount = 0;

    // Create the hook registry table: __hooks = {}
    lua_newtable(L);
    lua_setglobal(L, "__hooks");

    // Register screen dimensions for Psych Engine Lua scripts compatibility
    lua_pushinteger(L, 400);
    lua_setglobal(L, "screenWidth");

    lua_pushinteger(L, 240);
    lua_setglobal(L, "screenHeight");

    // Psych Engine Constants
    lua_pushinteger(L, 1);
    lua_setglobal(L, "Function_Stop");

    lua_pushinteger(L, 0);
    lua_setglobal(L, "Function_Continue");

    lua_pushinteger(L, 2);
    lua_setglobal(L, "Function_StopLua");

    // metatable proxy system for direct direct variable access
    const char* bootScript = 
        "local function makeProxy(path)\n"
        "    local proxy = {}\n"
        "    setmetatable(proxy, {\n"
        "        __index = function(t, key)\n"
        "            if key == 'scale' or key == 'scroll' or key == 'origin' or key == 'velocity' or key == 'offset' then\n"
        "                return makeProxy(path .. '.' .. key)\n"
        "            end\n"
        "            return getProperty(path .. '.' .. key)\n"
        "        end,\n"
        "        __newindex = function(t, key, val)\n"
        "            setProperty(path .. '.' .. key, val)\n"
        "        end\n"
        "    })\n"
        "    return proxy\n"
        "end\n"
        "local mt = {\n"
        "    __index = function(t, key)\n"
        "        if key == 'iconP1' or key == 'iconP2' or key == 'boyfriend' or key == 'dad' or key == 'gf' or key == 'camGame' or key == 'camHUD' or key == 'camFollow' or key == 'scoreTxt' or key == 'timeTxt' or key == 'timeBar' or key == 'timeBarBG' then\n"
        "            local proxy = makeProxy(key)\n"
        "            rawset(t, key, proxy)\n"
        "            return proxy\n"
        "        end\n"
        "        return nil\n"
        "    end\n"
        "}\n"
        "setmetatable(_G, mt)\n";
    luaL_dostring(L, bootScript);
}

void LuaManager::close() {
    aptSetHomeAllowed(true);
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    scriptCount = 0;
}

// Hook names that need to be isolated per-script
static const char* HOOK_NAMES[] = {
    "onCreate", "onCreatePost",
    "onUpdate", "onUpdatePost",
    "onBeatHit", "onStepHit",
    "onTweenCompleted", "onTimerCompleted",
    "onEvent",
    "goodNoteHit", "opponentNoteHit",
    "onSongStart",
    nullptr
};

bool LuaManager::runScript(const std::string& scriptPath) {
    if (!L) return false;

    //logtofile("[C++] runScript: Loading '" + scriptPath + "'...");
    if (luaL_dofile(L, scriptPath.c_str()) != 0) {
        std::string err = lua_tostring(L, -1);
        printf("WARN: Lua Script Error: %s\n", err.c_str());
        //logtofile("[C++] WARN: Lua Script Error in " + scriptPath + ": " + err);
        lua_pop(L, 1);
        return false;
    }
    //logtofile("[C++] runScript: '" + scriptPath + "' loaded successfully!");

    // Save all hook functions into __hooks[scriptCount] and clear globals
    lua_getglobal(L, "__hooks");           // stack: [__hooks]
    lua_newtable(L);                        // stack: [__hooks, {}]

    for (int i = 0; HOOK_NAMES[i]; i++) {
        lua_getglobal(L, HOOK_NAMES[i]);   // stack: [__hooks, {}, func/nil]
        if (lua_isfunction(L, -1)) {
            lua_setfield(L, -2, HOOK_NAMES[i]); // {}.hookName = func
            lua_pushnil(L);
            lua_setglobal(L, HOOK_NAMES[i]);    // clear global
        } else {
            lua_pop(L, 1);
        }
    }

    lua_rawseti(L, -2, scriptCount + 1);   // __hooks[scriptCount+1] = {}
    lua_pop(L, 1);                          // pop __hooks

    scriptCount++;
    return true;
}

bool LuaManager::callFunction(const std::string& funcName, const std::vector<std::string>& args) {
    if (!L) return false;

    syncGlobals();

    bool anySuccess = false;

    // Call from each script's hook table
    lua_getglobal(L, "__hooks");           // stack: [__hooks]
    for (int i = 1; i <= scriptCount; i++) {
        lua_rawgeti(L, -1, i);             // stack: [__hooks, hooks[i]]
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, funcName.c_str()); // stack: [__hooks, hooks[i], func/nil]
            if (lua_isfunction(L, -1)) {
                if (funcName != "onUpdate" && funcName != "onUpdatePost" && funcName != "onStepHit") {
                    //logtofile("[C++] callFunction: Calling '" + funcName + "' in script index " + std::to_string(i));
                }
                for (const auto& arg : args) {
                    if (arg == "true") {
                        lua_pushboolean(L, true);
                    } else if (arg == "false") {
                        lua_pushboolean(L, false);
                    } else {
                        char* p;
                        double val = strtod(arg.c_str(), &p);
                        if (*p == 0 && !arg.empty()) {
                            lua_pushnumber(L, val);
                        } else {
                            lua_pushstring(L, arg.c_str());
                        }
                    }
                }
                if (lua_pcall(L, args.size(), 0, 0) != 0) {
                    std::string err = lua_tostring(L, -1);
                    printf("WARN: Lua Call Error (%s) in script %d: %s\n", funcName.c_str(), i, err.c_str());
                    //logtofile("[C++] WARN: Lua Call Error (" + funcName + ") in script " + std::to_string(i) + ": " + err);
                    lua_pop(L, 1);
                } else {
                    anySuccess = true;
                }
            } else {
                lua_pop(L, 1); // pop nil func
            }
        }
        lua_pop(L, 1); // pop hooks[i]
    }
    lua_pop(L, 1); // pop __hooks

    return anySuccess;
}


#include <vector>
#include <algorithm>
#include <cmath>
#include "../states/PlayState.hpp"
#include "../backend/Conductor.hpp"
#include "../backend/AudioEngine.hpp"
#include "SparrowParser.hpp"
#include <citro2d.h>
#include <3ds/allocator/vram.h>
#include <malloc.h>

void LuaManager::syncGlobals() {
    if (!L || !PlayState::instance) return;

    lua_pushnumber(L, Conductor::getBeat());
    lua_setglobal(L, "curBeat");

    lua_pushnumber(L, (int)floor(Conductor::getStep()));
    lua_setglobal(L, "curStep");

    lua_pushnumber(L, PlayState::instance->health);
    lua_setglobal(L, "health");

    lua_pushinteger(L, PlayState::instance->score);
    lua_setglobal(L, "score");

    lua_pushinteger(L, PlayState::instance->misses);
    lua_setglobal(L, "misses");

    lua_pushinteger(L, PlayState::instance->combo);
    lua_setglobal(L, "combo");

    lua_pushnumber(L, Conductor::crochet);
    lua_setglobal(L, "crochet");

    lua_pushnumber(L, Conductor::stepCrochet);
    lua_setglobal(L, "stepCrochet");

    std::string lowerSong = PlayState::instance->curSong;
    std::transform(lowerSong.begin(), lowerSong.end(), lowerSong.begin(), ::tolower);
    lua_pushstring(L, lowerSong.c_str());
    lua_setglobal(L, "songName");

    // Settings & State Toggles
    lua_pushboolean(L, ClientPrefs::downscroll);
    lua_setglobal(L, "downscroll");

    lua_pushboolean(L, ClientPrefs::middleScroll);
    lua_setglobal(L, "middlescroll");

    lua_pushboolean(L, ClientPrefs::opponentStrums);
    lua_setglobal(L, "opponentStrums");

    lua_pushboolean(L, ClientPrefs::opponentNotes);
    lua_setglobal(L, "opponentNotes");

    lua_pushboolean(L, ClientPrefs::opponentStrums);
    lua_setglobal(L, "opponentStrums");

    lua_pushinteger(L, ClientPrefs::fpsLimit); // framerate
    lua_setglobal(L, "framerate");


    lua_pushboolean(L, ClientPrefs::ghostTapping);
    lua_setglobal(L, "ghostTapping");

    // Character Names
    lua_pushstring(L, SongParser::player1.c_str());
    lua_setglobal(L, "boyfriendName");

    lua_pushstring(L, SongParser::player2.c_str());
    lua_setglobal(L, "dadName");

    lua_pushstring(L, SongParser::gfVersion.c_str());
    lua_setglobal(L, "gfName");

    // Metadata & Constants
    lua_pushnumber(L, PlayState::instance->songLength);
    lua_setglobal(L, "songLength");

    lua_pushnumber(L, Conductor::bpm);
    lua_setglobal(L, "bpm");

    lua_pushnumber(L, Conductor::bpm);
    lua_setglobal(L, "curBpm");

    lua_pushnumber(L, SongParser::songSpeed);
    lua_setglobal(L, "scrollSpeed");

    lua_pushstring(L, SongParser::stage.c_str());
    lua_setglobal(L, "curStage");

    // default strum positions
    for (int i = 0; i < 4; i++) {
        // Player
        lua_pushnumber(L, PlayState::instance->getLaneX(i, true));
        lua_setglobal(L, ("defaultPlayerStrumX" + std::to_string(i)).c_str());
        lua_pushnumber(L, PlayState::instance->receptorY);
        lua_setglobal(L, ("defaultPlayerStrumY" + std::to_string(i)).c_str());

        // Opponent
        lua_pushnumber(L, PlayState::instance->getLaneX(i, false));
        lua_setglobal(L, ("defaultOpponentStrumX" + std::to_string(i)).c_str());
        lua_pushnumber(L, PlayState::instance->receptorY);
        lua_setglobal(L, ("defaultOpponentStrumY" + std::to_string(i)).c_str());
    }

    // default character positions
    lua_pushnumber(L, PlayState::instance->bf ? PlayState::instance->bf->baseX : 0.0f);
    lua_setglobal(L, "defaultBoyfriendX");
    lua_pushnumber(L, PlayState::instance->bf ? PlayState::instance->bf->baseY : 0.0f);
    lua_setglobal(L, "defaultBoyfriendY");

    lua_pushnumber(L, PlayState::instance->dad ? PlayState::instance->dad->baseX : 0.0f);
    lua_setglobal(L, "defaultOpponentX");
    lua_pushnumber(L, PlayState::instance->dad ? PlayState::instance->dad->baseY : 0.0f);
    lua_setglobal(L, "defaultOpponentY");

    lua_pushnumber(L, PlayState::instance->gf ? PlayState::instance->gf->baseX : 0.0f);
    lua_setglobal(L, "defaultGirlfriendX");
    lua_pushnumber(L, PlayState::instance->gf ? PlayState::instance->gf->baseY : 0.0f);
    lua_setglobal(L, "defaultGirlfriendY");


}

void LuaManager::setVar(const std::string& varName, const std::string& val) {
    if (L) {
        lua_pushstring(L, val.c_str());
        lua_setglobal(L, varName.c_str());
    }
}
void LuaManager::registerFunctions(lua_State* L) {
    if (!L) return;

    #define BIND_LUA_FUNC(name) lua_register(L, #name, lua_##name)

    BIND_LUA_FUNC(makeLuaSprite);
    BIND_LUA_FUNC(loadGraphic);
    BIND_LUA_FUNC(makeGraphic);
    BIND_LUA_FUNC(scaleObject);
    BIND_LUA_FUNC(setScrollFactor);
    BIND_LUA_FUNC(makeAnimatedLuaSprite);
    BIND_LUA_FUNC(precacheImage);
    BIND_LUA_FUNC(addAnimationByPrefix);
    BIND_LUA_FUNC(objectPlayAnimation);
    lua_register(L, "playAnim", lua_objectPlayAnimation);
    BIND_LUA_FUNC(playAnimFES);
    BIND_LUA_FUNC(addLuaSprite);
    BIND_LUA_FUNC(setSpriteVram);
    BIND_LUA_FUNC(setTextureManualMode);
    BIND_LUA_FUNC(loadTexture);
    BIND_LUA_FUNC(unloadTexture);
    BIND_LUA_FUNC(preloadTexture);
    BIND_LUA_FUNC(isTextureReady);
    BIND_LUA_FUNC(getProperty);
    BIND_LUA_FUNC(setProperty);
    BIND_LUA_FUNC(getPropertyFromGroup);
    BIND_LUA_FUNC(setPropertyFromGroup);
    BIND_LUA_FUNC(removeLuaSprite);
    BIND_LUA_FUNC(getRandomInt);
    BIND_LUA_FUNC(updateHitbox);
    BIND_LUA_FUNC(debugPrint);
    BIND_LUA_FUNC(setObjectCamera);
    BIND_LUA_FUNC(setCameraExtended);
    BIND_LUA_FUNC(screenCenter);
    BIND_LUA_FUNC(cameraShake);
    BIND_LUA_FUNC(triggerEvent);

    BIND_LUA_FUNC(doTweenX);
    BIND_LUA_FUNC(doTweenY);
    BIND_LUA_FUNC(doTweenAngle);
    BIND_LUA_FUNC(doTweenAlpha);
    BIND_LUA_FUNC(doTweenZoom);
    BIND_LUA_FUNC(doTweenColor);
    BIND_LUA_FUNC(doTweenScale);
    BIND_LUA_FUNC(doTweenScaleX);
    BIND_LUA_FUNC(doTweenScaleY);
    BIND_LUA_FUNC(noteTweenX);
    BIND_LUA_FUNC(noteTweenY);
    BIND_LUA_FUNC(noteTweenAngle);
    BIND_LUA_FUNC(noteTweenAlpha);
    BIND_LUA_FUNC(noteTweenDirection);
    BIND_LUA_FUNC(cancelTween);
    BIND_LUA_FUNC(runTimer);
    BIND_LUA_FUNC(cancelTimer);
    BIND_LUA_FUNC(setObjectOrder);
    BIND_LUA_FUNC(getObjectOrder);
    BIND_LUA_FUNC(playSound);
    BIND_LUA_FUNC(startVideo);
    lua_register(L, "playVideo", lua_startVideo);
    BIND_LUA_FUNC(stopVideo);
    lua_register(L, "closeVideo", lua_stopVideo);
    BIND_LUA_FUNC(pauseVideo);
    BIND_LUA_FUNC(resumeVideo);
    BIND_LUA_FUNC(getColorFromHex);
    
    // Text Functions
    BIND_LUA_FUNC(makeLuaText);
    BIND_LUA_FUNC(setTextString);
    BIND_LUA_FUNC(setTextSize);
    BIND_LUA_FUNC(setTextColor);
    BIND_LUA_FUNC(setTextAlignment);
    BIND_LUA_FUNC(setTextBorder);
    BIND_LUA_FUNC(addLuaText);
    BIND_LUA_FUNC(luaTextExists);
    BIND_LUA_FUNC(luaSpriteExists);

    BIND_LUA_FUNC(keyJustPressed);
    BIND_LUA_FUNC(keyPressed);
    BIND_LUA_FUNC(keyReleased);
    BIND_LUA_FUNC(setFpsLimit);
    lua_register(L, "setFramerate", lua_setFpsLimit);

    // Mouse & Touch Inputs (3DS Adaptation)
    BIND_LUA_FUNC(mouseClicked);
    BIND_LUA_FUNC(mousePressed);
    BIND_LUA_FUNC(mouseReleased);
    BIND_LUA_FUNC(getMouseX);
    BIND_LUA_FUNC(getMouseY);
    BIND_LUA_FUNC(mouseClickedOnSprite);
    lua_register(L, "isSpriteClicked", lua_mouseClickedOnSprite);
    BIND_LUA_FUNC(mouseOverlapSprite);
    lua_register(L, "objectsOverlapMouse", lua_mouseOverlapSprite);
    lua_register(L, "isSpriteTouched", lua_mouseOverlapSprite);

    // Position bindings
    BIND_LUA_FUNC(getCharacterX);
    BIND_LUA_FUNC(getCharacterY);
    BIND_LUA_FUNC(setCharacterX);
    BIND_LUA_FUNC(setCharacterY);
    BIND_LUA_FUNC(getMidpointX);
    BIND_LUA_FUNC(getMidpointY);

    // Utils bindings
    BIND_LUA_FUNC(getRandomBool);
    BIND_LUA_FUNC(getRandomFloat);
    BIND_LUA_FUNC(stringStartsWith);
    BIND_LUA_FUNC(stringEndsWith);
    BIND_LUA_FUNC(stringTrim);
    BIND_LUA_FUNC(stringSplit);

    // Audio bindings
    BIND_LUA_FUNC(precacheSound);
    BIND_LUA_FUNC(precacheMusic);
    BIND_LUA_FUNC(stopSound);
    BIND_LUA_FUNC(pauseSound);
    BIND_LUA_FUNC(resumeSound);
    BIND_LUA_FUNC(getSoundVolume);
    BIND_LUA_FUNC(setSoundVolume);

    BIND_LUA_FUNC(getHealth); BIND_LUA_FUNC(setHealth); BIND_LUA_FUNC(addHealth);
    BIND_LUA_FUNC(getScore); BIND_LUA_FUNC(setScore); BIND_LUA_FUNC(addScore);
    BIND_LUA_FUNC(getMisses); BIND_LUA_FUNC(setMisses); BIND_LUA_FUNC(addMisses);
    BIND_LUA_FUNC(getHits); BIND_LUA_FUNC(setHits); BIND_LUA_FUNC(addHits);
    BIND_LUA_FUNC(getSongPosition);
    BIND_LUA_FUNC(restartSong); BIND_LUA_FUNC(exitSong);
    BIND_LUA_FUNC(characterDance);
    BIND_LUA_FUNC(setGridVisible);

    // 3DS Hardware Control
    BIND_LUA_FUNC(setScreenState);
    BIND_LUA_FUNC(setHomeAllowed);
    BIND_LUA_FUNC(setLedColor);
    BIND_LUA_FUNC(setLedFlash);
    BIND_LUA_FUNC(crashGame);

    // Option Manager API
    BIND_LUA_FUNC(getOption);
    BIND_LUA_FUNC(setOption);
    BIND_LUA_FUNC(getModOption);
    BIND_LUA_FUNC(setModOption);
    
    // Shader System
    BIND_LUA_FUNC(setCameraShader);
    BIND_LUA_FUNC(removeCameraShader);
    BIND_LUA_FUNC(setShaderFloat);
    BIND_LUA_FUNC(setShaderParam);

    registerExcludedFunctions(L);

    #undef BIND_LUA_FUNC
}

void LuaManager::registerExcludedFunctions(lua_State* L) {
    static const char* excluded[] = {
        "initLuaShader", "setSpriteShader", "removeSpriteShader",
        "addHScript", "runHScript",
        "changeDiscordPresence", "callOnScripts", "setOnScripts",
        "getRunningScripts", "getPropertyFromClass", "setPropertyFromClass",
        "instanceArg", "createInstance", "addInstance"
    };

    for (const char* name : excluded) {
        lua_pushstring(L, name);
        lua_pushcclosure(L, [](lua_State* L) -> int {
            const char* funcName = lua_tostring(L, lua_upvalueindex(1));
            char msg[256];
            snprintf(msg, sizeof(msg), "SnakeEngine: Function '%s' is excluded on 3DS hardware.", funcName);
            if (PlayState::instance) PlayState::instance->addDebugMessage(msg);
            printf("\x1b[31;1m%s\x1b[0m\n", msg);
            return 0;
        }, 1);
        lua_setglobal(L, name);
    }
}




struct RawTexHeader {
    char magic[4];    // "RWTX"
    uint16_t width;   // Pow2 Width
    uint16_t height;  // Pow2 Height
    uint16_t origW;   // Original Width
    uint16_t origH;   // Original Height
};

static void releaseLuaSpriteMemory(StageSprite& oldSprite) {
    C2D_SpriteSheet sheetToFree = oldSprite.sheet;
    C3D_Tex* texToFree = oldSprite.img.tex;
    void* vramDataToFree = oldSprite.vramData;
    Tex3DS_SubTexture* subtexToFree = (Tex3DS_SubTexture*)oldSprite.img.subtex;

    if (sheetToFree == nullptr && texToFree == nullptr) {
        return;
    }

    // Count how many active luaSprites currently use this sheet/texture
    int usageCount = 0;
    if (PlayState::instance) {
        for (const auto& s : PlayState::instance->luaSprites) {
            if (sheetToFree != nullptr && s.sheet == sheetToFree) {
                usageCount++;
            } else if (texToFree != nullptr && s.img.tex == texToFree) {
                usageCount++;
            }
        }
    }

    // If more than 0 objects in active luaSprites list are using this texture/spritesheet,
    // do NOT free the GPU RAM so remaining sprites can continue rendering safely!
    if (usageCount > 0) {
        if (PlayState::instance) {
            for (auto& pair : PlayState::instance->luaTextureCache) {
                if ((pair.second.sheet != nullptr && pair.second.sheet == sheetToFree) ||
                    (pair.second.tex != nullptr && pair.second.tex == texToFree)) {
                    pair.second.refCount--;
                    break;
                }
            }
        }
        return;
    }

    // 0 remaining objects using this texture — MUST free RAM/VRAM resources!
    if (sheetToFree) {
        if (vramDataToFree) {
            C3D_Tex* tex = C2D_SpriteSheetGetImage(sheetToFree, 0).tex;
            if (tex) tex->data = nullptr;
            vramFree(vramDataToFree);
        }
        bool unloadedFromCache = SpritesheetCache::get().unloadBySheet(sheetToFree);
        if (!unloadedFromCache) {
            C2D_SpriteSheetFree(sheetToFree);
        }
    } else if (texToFree) {
        if (vramDataToFree) {
            texToFree->data = nullptr;
            vramFree(vramDataToFree);
        }
        C3D_TexDelete(texToFree);
        delete texToFree;
        if (subtexToFree) delete subtexToFree;
    }

    // Remove from luaTextureCache if present
    if (PlayState::instance) {
        for (auto it = PlayState::instance->luaTextureCache.begin(); it != PlayState::instance->luaTextureCache.end(); ) {
            if ((it->second.sheet != nullptr && it->second.sheet == sheetToFree) ||
                (it->second.tex != nullptr && it->second.tex == texToFree)) {
                it = PlayState::instance->luaTextureCache.erase(it);
                break;
            } else {
                ++it;
            }
        }
    }

    oldSprite.sheet = nullptr;
    oldSprite.img.tex = nullptr;
    oldSprite.img.subtex = nullptr;
    oldSprite.vramData = nullptr;
}

void LuaManager::clearAllSprites() {
    if (!PlayState::instance) return;
    
    // First, safely release all individual sprites
    for (auto& s : PlayState::instance->luaSprites) {
        releaseLuaSpriteMemory(s);
    }
    
    // Any remaining cache entries (if any leaked ref counts, though shouldn't happen)
    for (auto& pair : PlayState::instance->luaTextureCache) {
        SharedLuaTexture& shared = pair.second;
        if (shared.sheet) {
            if (!SpritesheetCache::get().contains(shared.sheet)) {
                if (shared.vramData) {
                    C3D_Tex* tex = C2D_SpriteSheetGetImage(shared.sheet, 0).tex;
                    if (tex) tex->data = nullptr;
                    vramFree(shared.vramData);
                }
                C2D_SpriteSheetFree(shared.sheet);
            }
        } else if (shared.tex) {
            if (shared.vramData) {
                shared.tex->data = nullptr;
                vramFree(shared.vramData);
            }
            C3D_TexDelete(shared.tex);
            delete shared.tex;
            if (shared.subtex) delete (Tex3DS_SubTexture*)shared.subtex;
        }
    }
    PlayState::instance->luaTextureCache.clear();
    
    for (auto& pair : PlayState::instance->namedTextureRegistry) {
        SharedLuaTexture& shared = pair.second;
        if (shared.sheet) {
            if (!SpritesheetCache::get().contains(shared.sheet)) {
                if (shared.vramData) {
                    if (addrIsVRAM(shared.vramData)) {
                        C3D_Tex* tex = C2D_SpriteSheetGetImage(shared.sheet, 0).tex;
                        if (tex) tex->data = nullptr;
                        vramFree(shared.vramData);
                    } else {
                        linearFree(shared.vramData);
                    }
                    shared.vramData = nullptr;
                }
                C2D_SpriteSheetFree(shared.sheet);
            }
        } else if (shared.tex) {
            if (shared.vramData) {
                if (addrIsVRAM(shared.vramData)) {
                    shared.tex->data = nullptr;
                    vramFree(shared.vramData);
                } else {
                    linearFree(shared.vramData);
                }
                shared.vramData = nullptr;
            }
            C3D_TexDelete(shared.tex);
            delete shared.tex;
            if (shared.subtex) delete (Tex3DS_SubTexture*)shared.subtex;
        }
    }
    PlayState::instance->namedTextureRegistry.clear();
    PlayState::instance->nextTextureHandle = 1;
    PlayState::instance->luaManualTextureMode = false;
}

int LuaManager::lua_makeLuaSprite(lua_State* L) {
    if (!PlayState::instance) return 0;
    
    // Tag, image, x, y
    if (lua_gettop(L) >= 4) {
        std::string tag = luaL_checkstring(L, 1);
        int handle = 0;
        std::string imgName = "";
        if (lua_type(L, 2) == LUA_TNUMBER) {
            handle = (int)lua_tointeger(L, 2);
        } else {
            imgName = luaL_checkstring(L, 2);
        }
        float x = (float)luaL_checknumber(L, 3);
        float y = (float)luaL_checknumber(L, 4);
        
        StageSprite s;
        s.name = imgName;
        s.x = x;
        s.y = y;
        s.scrollX = 1.0f;
        s.scrollY = 1.0f;
        s.scale = 1.0f;
        s.scaleX = 1.0f;
        s.scaleY = 1.0f;
        s.front = false;
        s.alpha = 1.0f;
        s.antialiasing = ClientPrefs::globalAntialiasing;
        s.sheet = nullptr;
        s.img.tex = nullptr;
        s.img.subtex = nullptr;
        
        bool skipLoad = false;
        if (handle != 0) {
            auto itRegistry = PlayState::instance->namedTextureRegistry.find(handle);
            if (itRegistry != PlayState::instance->namedTextureRegistry.end()) {
                SharedLuaTexture& shared = itRegistry->second;
                s.sheet = shared.sheet;
                if (s.sheet) {
                    s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
                } else {
                    s.img.tex = shared.tex;
                    s.img.subtex = shared.subtex;
                }
                s.vramData = shared.vramData;
            }
            skipLoad = true;
        } else if (imgName.empty() || PlayState::instance->luaManualTextureMode) {
            skipLoad = true;
        }

        if (skipLoad) {
            auto it = PlayState::instance->luaSpriteIndices.find(tag);
            if (it != PlayState::instance->luaSpriteIndices.end()) {
                StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
                releaseLuaSpriteMemory(oldSprite);
                PlayState::instance->luaSprites[it->second] = s;
            } else {
                PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
                PlayState::instance->luaSprites.push_back(s);
            }
            return 0;
        }
        
        // Check cache first!
        auto cacheIt = PlayState::instance->luaTextureCache.find(imgName);
        if (cacheIt != PlayState::instance->luaTextureCache.end()) {
            SharedLuaTexture& shared = cacheIt->second;
            shared.refCount++;
            
            s.sheet = shared.sheet;
            if (s.sheet) {
                s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
            } else {
                s.img.tex = shared.tex;
                s.img.subtex = shared.subtex;
            }
            s.vramData = shared.vramData;
            
            auto it = PlayState::instance->luaSpriteIndices.find(tag);
            if (it != PlayState::instance->luaSpriteIndices.end()) {
                StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
                releaseLuaSpriteMemory(oldSprite);
                PlayState::instance->luaSprites[it->second] = s;
            } else {
                PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
                PlayState::instance->luaSprites.push_back(s);
            }
            return 0;
        }
        
        // Load Sprite
        auto* cs = SpritesheetCache::get().load("images/" + imgName);
        s.sheet = cs ? cs->sheet : nullptr;
        if (s.sheet) {
            s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
            C3D_TexSetFilter(s.img.tex, s.antialiasing ? GPU_LINEAR : GPU_NEAREST, s.antialiasing ? GPU_LINEAR : GPU_NEAREST);
            
            // Save to mapping (preventing duplicates)
            auto it = PlayState::instance->luaSpriteIndices.find(tag);
            if (it != PlayState::instance->luaSpriteIndices.end()) {
                StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
                releaseLuaSpriteMemory(oldSprite);
                PlayState::instance->luaSprites[it->second] = s;
            } else {
                PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
                PlayState::instance->luaSprites.push_back(s);
            }
            //logtofile("[C++] makeLuaSprite: successfully loaded sprite sheet for '" + imgName + "' under tag '" + tag + "'");
        } else {
            // Fallback: Check for rawtex file
            std::string rawPath = ModHandler::get().getModPath("images/" + imgName + ".rawtex");
            //logtofile("[C++] makeLuaSprite: getModPath for '" + imgName + "' returned '" + rawPath + "'");
            
            if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".rawtex")) {
                rawPath = "romfs:/preload/images/" + imgName + ".rawtex";
            }
            if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".rawtex")) {
                rawPath = "romfs:/shared/images/" + imgName + ".rawtex";
            }
            
            //logtofile("[C++] makeLuaSprite: final rawPath to load = '" + rawPath + "'");

            bool loadedRaw = false;
            
            // First check if AsyncAssetManager has it!
            if (AsyncAssetManager::get().isImageReady(imgName)) {
                ImageData* data = AsyncAssetManager::get().consumeImage(imgName);
                if (data && data->tex) {
                    s.img.tex = data->tex;
                    s.img.subtex = data->subtex;
                    s.sheet = nullptr;
                    loadedRaw = true;
                    
                    // Save to mapping
                    auto it = PlayState::instance->luaSpriteIndices.find(tag);
                    if (it != PlayState::instance->luaSpriteIndices.end()) {
                        StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
                        releaseLuaSpriteMemory(oldSprite);
                        PlayState::instance->luaSprites[it->second] = s;
                    } else {
                        PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
                        PlayState::instance->luaSprites.push_back(s);
                    }
                }
            }

            if (!loadedRaw && !rawPath.empty()) {
                FILE* f = fopen(rawPath.c_str(), "rb");
                if (f) {
                    RawTexHeader header;
                    if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                        C3D_Tex* tex = new C3D_Tex();
                        if (!C3D_TexInit(tex, header.width, header.height, GPU_RGBA8)) {
                            delete tex;
                            tex = nullptr;
                            //logtofile("[C++] ERROR: VRAM Full inside makeLuaSprite rawtex loader!");
                        } else {
                            C3D_TexSetFilter(tex, s.antialiasing ? GPU_LINEAR : GPU_NEAREST, s.antialiasing ? GPU_LINEAR : GPU_NEAREST);
                            
                            size_t dataSize = (size_t)header.width * header.height * 4;
                            void* data = linearAlloc(dataSize);
                            if (data) {
                                fread(data, dataSize, 1, f);
                                C3D_TexUpload(tex, data);
                                C3D_TexFlush(tex);
                                linearFree(data);
                                
                                Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                                sub->width = header.origW;
                                sub->height = header.origH;
                                sub->left = 0.0f;
                                sub->top = 1.0f;
                                sub->right = (float)header.origW / header.width;
                                sub->bottom = 1.0f - ((float)header.origH / header.height);
                                
                                s.img.tex = tex;
                                s.img.subtex = sub;
                                s.sheet = nullptr; // Explicitly no sheet
                                loadedRaw = true;
                                //logtofile("[C++] makeLuaSprite: successfully loaded rawtex for '" + imgName + "' under tag '" + tag + "'. Orig dimensions: " + std::to_string(header.origW) + "x" + std::to_string(header.origH));

                                // Save to mapping (preventing duplicates)
                                auto it = PlayState::instance->luaSpriteIndices.find(tag);
                                if (it != PlayState::instance->luaSpriteIndices.end()) {
                                    // Clean up old manual sprite if it exists
                                    StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
                                        releaseLuaSpriteMemory(oldSprite);
                                    PlayState::instance->luaSprites[it->second] = s;
                                } else {
                                    PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
                                    PlayState::instance->luaSprites.push_back(s);
                                }
                            } else {
                                delete tex;
                                //logtofile("[C++] ERROR: linearAlloc failed for " + std::to_string(dataSize) + " bytes!");
                            }
                        }
                    } else {
                        //logtofile("[C++] ERROR: rawtex header magic check failed or read failed for '" + rawPath + "'");
                    }
                    fclose(f);
                } else {
                    //logtofile("[C++] ERROR: fopen failed for rawPath '" + rawPath + "'");
                }
            }

            if (!loadedRaw) {
                char debugMsg[128];
                snprintf(debugMsg, sizeof(debugMsg), "SnakeEngine: makeLuaSprite('%s') FAILED to load image", tag.c_str());
                PlayState::instance->addDebugMessage(debugMsg);
                //logtofile("[C++] ERROR: makeLuaSprite '" + tag + "' FAILED to load image");

                // Save blank sprite to mapping so makeGraphic can act on it
                auto it = PlayState::instance->luaSpriteIndices.find(tag);
                if (it != PlayState::instance->luaSpriteIndices.end()) {
                    PlayState::instance->luaSprites[it->second] = s;
                } else {
                    PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
                    PlayState::instance->luaSprites.push_back(s);
                }
            }
            if (s.sheet || s.img.tex) {
                SharedLuaTexture shared;
                shared.sheet = s.sheet;
                shared.tex = s.img.tex;
                shared.subtex = s.img.subtex;
                shared.vramData = s.vramData;
                shared.refCount = 1;
                PlayState::instance->luaTextureCache[imgName] = shared;
            }
        }
    }
    return 0; 
}

int LuaManager::lua_precacheImage(lua_State* L) {
    if (lua_gettop(L) >= 1) {
        std::string imgName = luaL_checkstring(L, 1);
        AsyncAssetManager::get().requestImageLoad(imgName);
    }
    return 0;
}

int LuaManager::lua_loadGraphic(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 2) return 0;
    std::string tag = luaL_checkstring(L, 1);
    int handle = 0;
    std::string imgName = "";
    if (lua_type(L, 2) == LUA_TNUMBER) {
        handle = (int)lua_tointeger(L, 2);
    } else {
        imgName = luaL_checkstring(L, 2);
    }

    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        StageSprite& s = PlayState::instance->luaSprites[it->second];
        
        // Check if the current texture is owned by the registry (loaded via loadTexture handle).
        // If so, just clear the pointer — unloadTexture is responsible for freeing it.
        bool ownedByRegistry = false;
        for (auto& kv : PlayState::instance->namedTextureRegistry) {
            SharedLuaTexture& reg = kv.second;
            if ((s.sheet && s.sheet == reg.sheet) || (s.img.tex && s.img.tex == reg.tex)) {
                ownedByRegistry = true;
                break;
            }
        }

        if (ownedByRegistry) {
            // Just null the pointers — do NOT free the texture, registry owns it
            s.sheet      = nullptr;
            s.img.tex    = nullptr;
            s.img.subtex = nullptr;
            s.vramData   = nullptr;
        } else {
            // Old-style sprite: we own the memory, free it
            if (s.sheet) {
                if (s.vramData) {
                    C3D_Tex* tex = C2D_SpriteSheetGetImage(s.sheet, 0).tex;
                    if (tex) tex->data = nullptr;
                    vramFree(s.vramData);
                }
                C2D_SpriteSheetFree(s.sheet);
                s.sheet = nullptr;
            } else if (s.img.tex) {
                if (s.vramData) {
                    vramFree(s.vramData);
                    s.img.tex->data = nullptr;
                }
                C3D_TexDelete(s.img.tex);
                delete s.img.tex;
                delete s.img.subtex;
                s.img.tex    = nullptr;
                s.img.subtex = nullptr;
            }
            s.vramData = nullptr;
        }

        s.animated = false;
        s.frames.clear();
        s.animations.clear();
        s.currentAnim = nullptr;

        bool skipLoad = false;
        if (handle != 0) {
            auto itRegistry = PlayState::instance->namedTextureRegistry.find(handle);
            if (itRegistry != PlayState::instance->namedTextureRegistry.end()) {
                SharedLuaTexture& shared = itRegistry->second;
                s.sheet = shared.sheet;
                if (s.sheet) {
                    s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
                } else {
                    s.img.tex = shared.tex;
                    s.img.subtex = shared.subtex;
                }
                s.vramData = shared.vramData;
            }
            skipLoad = true;
        } else if (imgName.empty() || PlayState::instance->luaManualTextureMode) {
            skipLoad = true;
        }
        
        if (skipLoad) {
            return 0;
        }

        // Load new Sprite sheet
        s.sheet = C2D_SpriteSheetLoad(Paths::image(imgName, "preload").c_str());
        if (s.sheet) {
            s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
            C3D_TexSetFilter(s.img.tex, s.antialiasing ? GPU_LINEAR : GPU_NEAREST, s.antialiasing ? GPU_LINEAR : GPU_NEAREST);
        } else {
            // Fallback: Check for rawtex file
            std::string rawPath = ModHandler::get().getModPath("images/" + imgName + ".rawtex");
            
            if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".rawtex")) {
                rawPath = "romfs:/preload/images/" + imgName + ".rawtex";
            }
            if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".rawtex")) {
                rawPath = "romfs:/shared/images/" + imgName + ".rawtex";
            }
            
            if (!rawPath.empty()) {
                FILE* f = fopen(rawPath.c_str(), "rb");
                if (f) {
                    RawTexHeader header;
                    if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                        C3D_Tex* tex = new C3D_Tex();
                        if (C3D_TexInit(tex, header.width, header.height, GPU_RGBA8)) {
                            C3D_TexSetFilter(tex, s.antialiasing ? GPU_LINEAR : GPU_NEAREST, s.antialiasing ? GPU_LINEAR : GPU_NEAREST);
                            
                            size_t dataSize = (size_t)header.width * header.height * 4;
                            void* data = linearAlloc(dataSize);
                            if (data) {
                                fread(data, dataSize, 1, f);
                                C3D_TexUpload(tex, data);
                                C3D_TexFlush(tex);
                                linearFree(data);
                                
                                Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                                sub->width = header.origW;
                                sub->height = header.origH;
                                sub->left = 0.0f;
                                sub->top = 1.0f;
                                sub->right = (float)header.origW / header.width;
                                sub->bottom = 1.0f - ((float)header.origH / header.height);
                                
                                s.img.tex = tex;
                                s.img.subtex = sub;
                            } else {
                                delete tex;
                            }
                        } else {
                            delete tex;
                        }
                    }
                    fclose(f);
                }
            }
        }
    }
    return 0;
}

int LuaManager::lua_makeGraphic(lua_State* L) {
    if (!PlayState::instance) return 0;
    
    int nargs = lua_gettop(L);
    if (nargs < 3) return 0;
    
    std::string tag = luaL_checkstring(L, 1);
    float width = luaL_checknumber(L, 2);
    float height = luaL_checknumber(L, 3);
    
    std::string colorStr = "ffffff";
    if (nargs >= 4) {
        colorStr = luaL_checkstring(L, 4);
    }
    
    // Parse color
    u32 color = HexParser::parseStringToC2D(colorStr, C2D_Color32(255, 255, 255, 255));
    
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        StageSprite& s = PlayState::instance->luaSprites[it->second];
        s.isGraphic = true;
        s.graphicColor = color;
        s.graphicWidth = width;
        s.graphicHeight = height;
    } else {
        StageSprite s;
        s.name = tag;
        s.x = 0.0f;
        s.y = 0.0f;
        s.scrollX = 1.0f;
        s.scrollY = 1.0f;
        s.scale = 1.0f;
        s.scaleX = 1.0f;
        s.scaleY = 1.0f;
        s.front = false;
        s.alpha = 1.0f;
        s.visible = true;
        s.antialiasing = ClientPrefs::globalAntialiasing;
        s.isGraphic = true;
        s.graphicColor = color;
        s.graphicWidth = width;
        s.graphicHeight = height;
        PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
        PlayState::instance->luaSprites.push_back(s);
    }
    return 0;
}

int LuaManager::lua_setObjectCamera(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 2) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string cam = luaL_checkstring(L, 2);
    
    //logtofile("[C++] setObjectCamera: tag='" + tag + "', cam='" + cam + "'");
    
    // Normalization
    std::string lowCam = cam;
    std::transform(lowCam.begin(), lowCam.end(), lowCam.begin(), ::tolower);
    if (lowCam == "hud" || lowCam == "camhud") cam = "camHUD";
    else if (lowCam == "game" || lowCam == "camgame") cam = "camGame";
    else if (lowCam == "other" || lowCam == "camother") cam = "camOther";
    else if (lowCam == "bottom" || lowCam == "cambottom") cam = "camBottom";

    // Check Sprites
    auto sit = PlayState::instance->luaSpriteIndices.find(tag);
    if (sit != PlayState::instance->luaSpriteIndices.end()) {
        PlayState::instance->luaSprites[sit->second].camera = cam;
        //logtofile("[C++] setObjectCamera: set sprite '" + tag + "' camera to '" + cam + "'");
        return 0;
    }

    // Check Texts
    auto tit = PlayState::instance->luaTextIndices.find(tag);
    if (tit != PlayState::instance->luaTextIndices.end()) {
        PlayState::instance->luaTexts[tit->second].camera = cam;
        //logtofile("[C++] setObjectCamera: set text '" + tag + "' camera to '" + cam + "'");
    }

    return 0;
}

int LuaManager::lua_setCameraExtended(lua_State* L) {
    if (lua_gettop(L) < 2) return 0;
    std::string cam = luaL_checkstring(L, 1);
    bool ext = lua_toboolean(L, 2);
    
    // Normalization
    std::string lowCam = cam;
    std::transform(lowCam.begin(), lowCam.end(), lowCam.begin(), ::tolower);
    if (lowCam == "hud" || lowCam == "camhud") cam = "camHUD";
    else if (lowCam == "game" || lowCam == "camgame") cam = "camGame";
    else if (lowCam == "other" || lowCam == "camother") cam = "camOther";
    else if (lowCam == "bottom" || lowCam == "cambottom") cam = "camBottom";

    ShaderManager::get().setCameraExtended(cam, ext);
    return 0;
}

int LuaManager::lua_setSpriteVram(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string tag = luaL_checkstring(L, 1);
    
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        StageSprite& s = PlayState::instance->luaSprites[it->second];
        if (s.vramData) return 0; // Already in VRAM
        
        C3D_Tex* tex = nullptr;
        if (s.sheet) {
            tex = C2D_SpriteSheetGetImage(s.sheet, 0).tex;
        } else if (s.img.tex) {
            tex = s.img.tex;
        }

        if (tex && tex->data) {
            s.vramData = vramAlloc(tex->size);
            if (s.vramData) {
                u32 gxFormat = GX_TRANSFER_FMT_RGBA8;
                if (tex->fmt == GPU_RGBA8) gxFormat = GX_TRANSFER_FMT_RGBA8;
                else if (tex->fmt == GPU_RGB8) gxFormat = GX_TRANSFER_FMT_RGB8;
                else if (tex->fmt == GPU_RGB565) gxFormat = GX_TRANSFER_FMT_RGB565;
                else if (tex->fmt == GPU_RGBA5551) gxFormat = GX_TRANSFER_FMT_RGB5A1;
                else if (tex->fmt == GPU_RGBA4) gxFormat = GX_TRANSFER_FMT_RGBA4;

                GSPGPU_FlushDataCache(tex->data, tex->size);
                GX_DisplayTransfer((u32*)tex->data, GX_BUFFER_DIM(tex->width, tex->height),
                                   (u32*)s.vramData, GX_BUFFER_DIM(tex->width, tex->height),
                                   GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(1) | 
                                   GX_TRANSFER_IN_FORMAT(gxFormat) | GX_TRANSFER_OUT_FORMAT(gxFormat) | 
                                   GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
                gspWaitForPPF();
                if (!s.sheet) {
                    linearFree(tex->data);
                }
                tex->data = s.vramData;
            }
        }
    }
    return 0;
}

int LuaManager::lua_scaleObject(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 2) return 0;
    std::string tag = luaL_checkstring(L, 1);
    float scaleX = (float)luaL_checknumber(L, 2);
    float scaleY = (lua_gettop(L) >= 3) ? (float)luaL_checknumber(L, 3) : scaleX;
    
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        auto& s = PlayState::instance->luaSprites[it->second];
        s.scale = scaleX;
        s.scaleX = scaleX;
        s.scaleY = scaleY;
    }
    return 0;
}

int LuaManager::lua_setScrollFactor(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    std::string tag = luaL_checkstring(L, 1);
    float scrollX = luaL_checknumber(L, 2);
    float scrollY = luaL_checknumber(L, 3);
    
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        PlayState::instance->luaSprites[it->second].scrollX = scrollX;
        PlayState::instance->luaSprites[it->second].scrollY = scrollY;
    }
    return 0;
}

int LuaManager::lua_makeAnimatedLuaSprite(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    int handle = 0;
    std::string imgName = "";
    if (lua_type(L, 2) == LUA_TNUMBER) {
        handle = (int)lua_tointeger(L, 2);
    } else {
        imgName = luaL_checkstring(L, 2);
    }
    float x = luaL_optnumber(L, 3, 0.0);
    float y = luaL_optnumber(L, 4, 0.0);
    
    StageSprite s;
    s.name = tag;
    s.x = x; s.y = y;
    s.scale = 1.0f; s.scaleX = 1.0f; s.scaleY = 1.0f; s.scrollX = 1.0f; s.scrollY = 1.0f;
    s.front = false; s.alpha = 1.0f;
    s.animated = true;
    s.antialiasing = ClientPrefs::globalAntialiasing;
    s.sheet = nullptr;
    s.img.tex = nullptr;
    s.img.subtex = nullptr;

    bool skipLoad = false;
    if (handle != 0) {
        auto itRegistry = PlayState::instance->namedTextureRegistry.find(handle);
        if (itRegistry != PlayState::instance->namedTextureRegistry.end()) {
            SharedLuaTexture& shared = itRegistry->second;
            s.sheet = shared.sheet;
            if (s.sheet) {
                s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
                auto* cs = SpritesheetCache::get().load("images/" + shared.originalPath);
                if (cs && !cs->frames.empty()) {
                    s.frames = cs->frames;
                }
            } else {
                s.img.tex = shared.tex;
                s.img.subtex = shared.subtex;
                
                std::string xmlPath = ModHandler::get().getModPath("images/" + shared.originalPath + ".xml");
                if (xmlPath.empty() && Paths::fileExists("romfs:/preload/images/" + shared.originalPath + ".xml")) {
                    xmlPath = "romfs:/preload/images/" + shared.originalPath + ".xml";
                }
                if (xmlPath.empty() && Paths::fileExists("romfs:/shared/images/" + shared.originalPath + ".xml")) {
                    xmlPath = "romfs:/shared/images/" + shared.originalPath + ".xml";
                }
                
                if (!xmlPath.empty() && Paths::fileExists(xmlPath)) {
                    SparrowParser::parseXml(xmlPath, s.frames);
                    for (auto& frame : s.frames) {
                        frame.tex = s.img.tex;
                        frame.uv.width = (u16)frame.w;
                        frame.uv.height = (u16)frame.h;
                        frame.uv.left = ((float)frame.x / (float)s.img.subtex->width);
                        frame.uv.top = 1.0f - ((float)frame.y / (float)s.img.subtex->height);
                        frame.uv.right = ((float)(frame.x + frame.w) / (float)s.img.subtex->width);
                        frame.uv.bottom = 1.0f - ((float)(frame.y + frame.h) / (float)s.img.subtex->height);
                    }
                }
            }
            s.vramData = shared.vramData;
        }
        skipLoad = true;
    } else if (imgName.empty() || PlayState::instance->luaManualTextureMode) {
        skipLoad = true;
    }

    if (skipLoad) {
        auto it = PlayState::instance->luaSpriteIndices.find(tag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
            releaseLuaSpriteMemory(oldSprite);
            PlayState::instance->luaSprites[it->second] = s;
        } else {
            PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
            PlayState::instance->luaSprites.push_back(s);
        }
        return 0;
    }
    
    auto* cs = SpritesheetCache::get().load("images/" + imgName);
    s.sheet = cs ? cs->sheet : nullptr;
    bool loaded = false;
    if (s.sheet && cs && !cs->frames.empty()) {
        s.img = C2D_SpriteSheetGetImage(s.sheet, 0);
        if (s.img.tex) {
            C3D_TexSetFilter(s.img.tex, s.antialiasing ? GPU_LINEAR : GPU_NEAREST, s.antialiasing ? GPU_LINEAR : GPU_NEAREST);
            s.frames = cs->frames;
            loaded = true;
        }
    } else {
        // Fallback: Check for rawtex + XML
        std::string rawPath = ModHandler::get().getModPath("images/" + imgName + ".rawtex");
        
        // First check if AsyncAssetManager has it!
        if (AsyncAssetManager::get().isImageReady(imgName)) {
            ImageData* data = AsyncAssetManager::get().consumeImage(imgName);
            if (data && data->tex) {
                s.img.tex = data->tex;
                s.img.subtex = data->subtex;
                s.sheet = nullptr;
                
                // Parse corresponding XML coordinates
                std::string xmlPath = ModHandler::get().getModPath("images/" + imgName + ".xml");
                if (xmlPath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".xml")) {
                    xmlPath = "romfs:/preload/images/" + imgName + ".xml";
                }
                if (xmlPath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".xml")) {
                    xmlPath = "romfs:/shared/images/" + imgName + ".xml";
                }
                
                if (!xmlPath.empty() && Paths::fileExists(xmlPath)) {
                    SparrowParser::parseXml(xmlPath, s.frames);
                    for (auto& frame : s.frames) {
                        frame.tex = data->tex;
                        frame.uv.width = (u16)frame.w;
                        frame.uv.height = (u16)frame.h;
                        frame.uv.left = ((float)frame.x / (float)data->rawWidth);
                        frame.uv.top = 1.0f - ((float)frame.y / (float)data->rawHeight);
                        frame.uv.right = ((float)(frame.x + frame.w) / (float)data->rawWidth);
                        frame.uv.bottom = 1.0f - ((float)(frame.y + frame.h) / (float)data->rawHeight);
                    }
                    loaded = true;
                } else {
                    // Fallback to simple un-animated behavior if XML is missing
                    C3D_TexDelete(data->tex);
                    delete data->tex;
                    delete data->subtex;
                }
                delete data;
            }
        }
        
        if (!loaded && rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".rawtex")) {
            rawPath = "romfs:/preload/images/" + imgName + ".rawtex";
        }
        if (!loaded && rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".rawtex")) {
            rawPath = "romfs:/shared/images/" + imgName + ".rawtex";
        }

        if (!loaded && !rawPath.empty()) {
            FILE* f = fopen(rawPath.c_str(), "rb");
            if (f) {
                printf("[LUA_RAW] Successfully opened animated rawtex: '%s'\n", rawPath.c_str());
                RawTexHeader header;
                if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                    C3D_Tex* tex = new C3D_Tex();
                    if (!C3D_TexInit(tex, header.width, header.height, GPU_RGBA8)) {
                        delete tex;
                    } else {
                        C3D_TexSetFilter(tex, s.antialiasing ? GPU_LINEAR : GPU_NEAREST, s.antialiasing ? GPU_LINEAR : GPU_NEAREST);
                    
                    size_t dataSize = (size_t)header.width * header.height * 4;
                    void* data = linearAlloc(dataSize);
                    if (data) {
                        fread(data, dataSize, 1, f);
                        C3D_TexUpload(tex, data);
                        C3D_TexFlush(tex);
                        linearFree(data);
                        
                        Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                        sub->width = header.origW;
                        sub->height = header.origH;
                        sub->left = 0.0f;
                        sub->top = 1.0f;
                        sub->right = (float)header.origW / header.width;
                        sub->bottom = 1.0f - ((float)header.origH / header.height);
                        
                        s.img.tex = tex;
                        s.img.subtex = sub;
                        s.sheet = nullptr; // Explicitly no sheet
                        
                        // Parse corresponding XML coordinates
                        std::string xmlPath = ModHandler::get().getModPath("images/" + imgName + ".xml");
                        if (xmlPath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".xml")) {
                            xmlPath = "romfs:/preload/images/" + imgName + ".xml";
                        }
                        if (xmlPath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".xml")) {
                            xmlPath = "romfs:/shared/images/" + imgName + ".xml";
                        }
                        
                        if (!xmlPath.empty() && Paths::fileExists(xmlPath)) {
                            SparrowParser::parseXml(xmlPath, s.frames);
                            for (auto& frame : s.frames) {
                                frame.tex = tex;
                                frame.uv.width = (u16)frame.w;
                                frame.uv.height = (u16)frame.h;
                                frame.uv.left = ((float)frame.x / (float)header.width);
                                frame.uv.top = 1.0f - ((float)frame.y / (float)header.height);
                                frame.uv.right = ((float)(frame.x + frame.w) / (float)header.width);
                                frame.uv.bottom = 1.0f - ((float)(frame.y + frame.h) / (float)header.height);
                            }
                            loaded = true;
                            printf("[LUA_RAW] Successfully parsed %d frames from XML: '%s'\n", (int)s.frames.size(), xmlPath.c_str());
                        } else {
                            delete tex;
                            delete sub;
                        }
                    } else {
                        delete tex;
                    }
                    } // close else from C3D_TexInit
                }
                fclose(f);
            }
        }
    }

    if (!loaded) {
        char debugMsg[128];
        snprintf(debugMsg, sizeof(debugMsg), "SnakeEngine: makeAnimatedLuaSprite('%s') FAILED to load image", tag.c_str());
        PlayState::instance->addDebugMessage(debugMsg);
        printf("WARN: Lua makeAnimatedLuaSprite couldn't load %s\n", imgName.c_str());
        return 0;
    }
    
    // Save to mapping (preventing duplicates)
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        // Clean up old manual sprite if it exists
        StageSprite& oldSprite = PlayState::instance->luaSprites[it->second];
        if (oldSprite.vramData) {
            vramFree(oldSprite.vramData);
            if (oldSprite.img.tex) oldSprite.img.tex->data = nullptr;
            oldSprite.vramData = nullptr;
        }
        if (oldSprite.sheet) {
            // Only free if not owned by the global SpritesheetCache
            if (!SpritesheetCache::get().contains(oldSprite.sheet)) {
                C2D_SpriteSheetFree(oldSprite.sheet);
            }
            oldSprite.sheet = nullptr;
        } else if (oldSprite.img.tex) {
            C3D_TexDelete(oldSprite.img.tex);
            delete oldSprite.img.tex;
            delete oldSprite.img.subtex;
        }
        PlayState::instance->luaSprites[it->second] = s;
    } else {
        PlayState::instance->luaSpriteIndices[tag] = PlayState::instance->luaSprites.size();
        PlayState::instance->luaSprites.push_back(s);
    }
    return 0;
}

int LuaManager::lua_addAnimationByPrefix(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string name = luaL_checkstring(L, 2);
    std::string prefix = luaL_checkstring(L, 3);
    float fps = luaL_optnumber(L, 4, 24.0);
    bool loop = lua_toboolean(L, 5);

    if (PlayState::instance->luaSpriteIndices.count(tag)) {
        StageSprite& s = PlayState::instance->luaSprites[PlayState::instance->luaSpriteIndices[tag]];
        if (s.animated) {
            Animation anim;
            anim.name = name;
            anim.prefix = prefix;
            anim.fps = fps;
            anim.loop = loop;
            anim.offsetX = 0; anim.offsetY = 0;

            for (int i = 0; i < (int)s.frames.size(); i++) {
                std::string frameName = s.frames[i].name;
                if (frameName.find(prefix) == 0) {
                    bool matches = false;
                    if (frameName.length() == prefix.length()) matches = true;
                    else {
                        std::string rest = frameName.substr(prefix.length());
                        size_t firstPos = rest.find_first_not_of(" \t");
                        if (firstPos != std::string::npos) {
                            std::string actualRest = rest.substr(firstPos);
                            if (isdigit(actualRest[0]) || actualRest.find("instancia") == 0) {
                                matches = true;
                            }
                        }
                    }

                    if (matches) {
                        anim.indices.push_back(i);
                    }
                }
            }
            s.animations[name] = anim;
            
            // Auto play if it's the first animation
            if (!s.currentAnim && !anim.indices.empty()) {
                s.playAnim(name, true);
            }
        }
    }
    return 0;
}

int LuaManager::lua_objectPlayAnimation(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string name = luaL_checkstring(L, 2);
    bool force = lua_toboolean(L, 3);

    auto slotsMatch = [](const std::string& type1, const std::string& type2) {
        auto normalize = [](std::string s) -> std::string {
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (s == "opponent" || s == "0" || s == "dad") return "dad";
            if (s == "boyfriend" || s == "1" || s == "player" || s == "bf") return "bf";
            if (s == "girlfriend" || s == "2" || s == "gf") return "gf";
            return s;
        };
        return normalize(type1) == normalize(type2);
    };

    // If there is a pending swap for this slot, cache the animation so it gets played once loaded
    for (auto& swap : PlayState::instance->pendingSwaps) {
        if (slotsMatch(swap.charType, tag)) {
            swap.pendingAnim = name;
            swap.pendingAnimForce = force;
        }
    }

    std::string lowerTag = tag;
    for (auto& c : lowerTag) c = std::tolower(c);

    Character* charObj = nullptr;
    if (lowerTag == "bf" || lowerTag == "boyfriend") {
        charObj = PlayState::instance->bf;
    } else if (lowerTag == "dad" || lowerTag == "opponent") {
        charObj = PlayState::instance->dad;
    } else if (lowerTag == "gf" || lowerTag == "girlfriend") {
        charObj = PlayState::instance->gf;
    }

    if (charObj) {
        charObj->playAnim(name, force);
    } else if (PlayState::instance->luaSpriteIndices.count(tag)) {
        StageSprite& s = PlayState::instance->luaSprites[PlayState::instance->luaSpriteIndices[tag]];
        if (s.animated) {
            s.playAnim(name, force);
        }
    }
    return 0;
}

int LuaManager::lua_addLuaSprite(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string tag = luaL_checkstring(L, 1);
    bool front = (lua_gettop(L) >= 2) ? lua_toboolean(L, 2) : false;
    
    //logtofile("[C++] addLuaSprite: tag='" + tag + "', front=" + (front ? "true" : "false"));
    
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        PlayState::instance->luaSprites[it->second].front = front;
        PlayState::instance->luaSprites[it->second].zOrder = PlayState::instance->currentZOrder++;
        //logtofile("[C++] addLuaSprite: set front of '" + tag + "' to " + (front ? "true" : "false"));
    } else {
        //logtofile("[C++] ERROR: addLuaSprite: tag '" + tag + "' NOT found in luaSpriteIndices!");
    }
    return 0;
}

int LuaManager::lua_getProperty(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string variable = luaL_checkstring(L, 1);

    if (variable == "health") lua_pushnumber(L, PlayState::instance->health);
    else if (variable == "score") lua_pushnumber(L, PlayState::instance->score);
    else if (variable == "misses") lua_pushnumber(L, PlayState::instance->misses);
    else if (variable == "combo") lua_pushnumber(L, PlayState::instance->combo);
    else if (variable == "inGameOver") lua_pushboolean(L, PlayState::instance->gameOver);
    else if (variable == "scoreTxt.visible") lua_pushboolean(L, PlayState::instance->scoreTxtVisible);
    else if (variable == "timeTxt.visible") lua_pushboolean(L, PlayState::instance->timeTxtVisible);
    else if (variable == "timeBar.visible" || variable == "timeBarBG.visible") lua_pushboolean(L, PlayState::instance->timeBarVisible);
    else if (variable == "unspawnNotes.length" || variable == "notes.length") lua_pushnumber(L, PlayState::instance->songNotes.size());
    else if (variable == "songLength") lua_pushnumber(L, PlayState::instance->songLength);
    else if (variable == "curBeat") lua_pushnumber(L, Conductor::getBeat());
    else if (variable == "camZoom") lua_pushnumber(L, PlayState::instance->camZoom);
    else if (variable == "hudZoom") lua_pushnumber(L, PlayState::instance->hudZoom);
    else if (variable == "camFollowX") lua_pushnumber(L, PlayState::instance->camFollowX);
    else if (variable == "camFollowY") lua_pushnumber(L, PlayState::instance->camFollowY);
    else if (variable == "camFollowLocked") lua_pushboolean(L, PlayState::instance->camFollowLocked);
    else if (variable == "showGrid") lua_pushboolean(L, PlayState::instance->showGrid);
    else if (variable == "legacyPositioning") lua_pushboolean(L, PlayState::instance->legacyPositioning);
    else if (variable == "playerUnderlayAlpha") {
        float val = PlayState::instance->playerUnderlayAlpha;
        if (val < 0.0f) val = ClientPrefs::noteUnderlayAlpha;
        lua_pushnumber(L, val);
    }
    else if (variable == "opponentUnderlayAlpha") {
        float val = PlayState::instance->opponentUnderlayAlpha;
        if (val < 0.0f) val = ClientPrefs::noteUnderlayAlpha;
        lua_pushnumber(L, val);
    }
    else if (variable == "playerUnderlayVisible") lua_pushboolean(L, PlayState::instance->playerUnderlayVisible);
    else if (variable == "opponentUnderlayVisible") lua_pushboolean(L, PlayState::instance->opponentUnderlayVisible);
    else if (variable == "playerUnderlayColor") lua_pushinteger(L, PlayState::instance->playerUnderlayColor);
    else if (variable == "opponentUnderlayColor") lua_pushinteger(L, PlayState::instance->opponentUnderlayColor);
    else {
        std::string obj = "";
        std::string prop = variable;
        size_t firstDot = variable.find_first_of('.');
        if (firstDot != std::string::npos) {
            obj = variable.substr(0, firstDot);
            prop = variable.substr(firstDot + 1);
        } else {
            // Direct object query (e.g. getProperty('boyfriend'))
            if (prop == "boyfriend") { lua_pushboolean(L, PlayState::instance->bf != nullptr); return 1; }
            if (prop == "dad") { lua_pushboolean(L, PlayState::instance->dad != nullptr); return 1; }
            if (prop == "gf") { lua_pushboolean(L, PlayState::instance->gf != nullptr); return 1; }
            if (prop == "autoIconPosition") { lua_pushboolean(L, PlayState::instance->autoIconPosition); return 1; }
            if (PlayState::instance->luaSpriteIndices.count(prop)) { lua_pushboolean(L, true); return 1; }
            if (PlayState::instance->luaTextIndices.count(prop)) { lua_pushboolean(L, true); return 1; }
        }

        if (obj == "iconP1" || obj == "iconP2") {
            bool isP1 = (obj == "iconP1");
            if (prop == "x") {
                float val = isP1 ? PlayState::instance->iconP1X : PlayState::instance->iconP2X;
                if (val == -9999.0f) {
                    float screenScale = 240.0f / 720.0f;
                    float healthBarW = 200.0f;
                    float healthBarX = (400.0f - healthBarW) / 2.0f;
                    float healthPerc = PlayState::instance->health / 2.0f;
                    float unzoomed_divX = healthBarX + healthBarW * (1.0f - healthPerc);
                    if (isP1) val = unzoomed_divX / screenScale;
                    else val = (unzoomed_divX - 42.0f) / screenScale;
                }
                lua_pushnumber(L, val);
            }
            else if (prop == "y") {
                float val = isP1 ? PlayState::instance->iconP1Y : PlayState::instance->iconP2Y;
                if (val == -9999.0f) {
                    float screenScale = 240.0f / 720.0f;
                    float healthBarH = 5.0f;
                    float healthBarY = ClientPrefs::downscroll ? 20.0f : 240.0f - 20.0f;
                    float unzoomed_iconY = healthBarY + healthBarH * 0.5f - 42.0f * 0.5f;
                    val = unzoomed_iconY / screenScale;
                }
                lua_pushnumber(L, val);
            }
            else if (prop == "alpha") lua_pushnumber(L, isP1 ? PlayState::instance->iconP1Alpha : PlayState::instance->iconP2Alpha);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, isP1 ? PlayState::instance->iconP1ScaleX : PlayState::instance->iconP2ScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, isP1 ? PlayState::instance->iconP1ScaleY : PlayState::instance->iconP2ScaleY);
            else if (prop == "scale") lua_pushnumber(L, isP1 ? PlayState::instance->iconP1Scale : PlayState::instance->iconP2Scale);
            else if (prop == "visible") lua_pushboolean(L, isP1 ? PlayState::instance->iconP1Visible : PlayState::instance->iconP2Visible);
            else if (prop == "angle") lua_pushnumber(L, isP1 ? PlayState::instance->iconP1Angle : PlayState::instance->iconP2Angle);
            else if (prop == "color") lua_pushnumber(L, isP1 ? PlayState::instance->iconP1Color : PlayState::instance->iconP2Color);
            else if (prop == "flipX") lua_pushboolean(L, isP1 ? PlayState::instance->iconP1FlipX : PlayState::instance->iconP2FlipX);
            else if (prop == "flipY") lua_pushboolean(L, isP1 ? PlayState::instance->iconP1FlipY : PlayState::instance->iconP2FlipY);
            else if (prop == "antialiasing") lua_pushboolean(L, isP1 ? PlayState::instance->iconP1Antialiasing : PlayState::instance->iconP2Antialiasing);
            else lua_pushnil(L);
        }
        else if (obj == "healthbar" || obj == "healthbarBg" || obj == "healthBar" || obj == "healthBarBG" || obj == "healthbarBG") {
            bool isBG = (obj == "healthbarBg" || obj == "healthBarBG" || obj == "healthbarBG");
            if (prop == "x") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGX : PlayState::instance->healthBarX);
            else if (prop == "y") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGY : PlayState::instance->healthBarY);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGScaleX : PlayState::instance->healthBarScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGScaleY : PlayState::instance->healthBarScaleY);
            else if (prop == "alpha") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGAlpha : PlayState::instance->healthBarAlpha);
            else if (prop == "angle") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGAngle : PlayState::instance->healthBarAngle);
            else if (prop == "visible") lua_pushboolean(L, isBG ? PlayState::instance->healthBarBGVisible : PlayState::instance->healthBarVisible);
            else if (prop == "color") lua_pushnumber(L, isBG ? PlayState::instance->healthBarBGColor : PlayState::instance->healthBarColor);
            else if (prop == "flipX") lua_pushboolean(L, isBG ? PlayState::instance->healthBarBGFlipX : PlayState::instance->healthBarFlipX);
            else if (prop == "flipY") lua_pushboolean(L, isBG ? PlayState::instance->healthBarBGFlipY : PlayState::instance->healthBarFlipY);
            else if (prop == "antialiasing") lua_pushboolean(L, isBG ? PlayState::instance->healthBarBGAntialiasing : PlayState::instance->healthBarAntialiasing);
            else lua_pushnil(L);
        }
        else if (obj == "timebar" || obj == "timebarBg" || obj == "timeBar" || obj == "timeBarBG" || obj == "timebarBG") {
            bool isBG = (obj == "timebarBg" || obj == "timeBarBG" || obj == "timebarBG");
            if (prop == "x") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGX : PlayState::instance->timeBarX_val);
            else if (prop == "y") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGY : PlayState::instance->timeBarY_val);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGScaleX : PlayState::instance->timeBarScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGScaleY : PlayState::instance->timeBarScaleY);
            else if (prop == "alpha") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGAlpha : PlayState::instance->timeBarAlpha);
            else if (prop == "angle") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGAngle : PlayState::instance->timeBarAngle);
            else if (prop == "visible") lua_pushboolean(L, isBG ? PlayState::instance->timeBarBGVisible : PlayState::instance->timeBarVisible);
            else if (prop == "color") lua_pushnumber(L, isBG ? PlayState::instance->timeBarBGColor : PlayState::instance->timeBarColor);
            else if (prop == "flipX") lua_pushboolean(L, isBG ? PlayState::instance->timeBarBGFlipX : PlayState::instance->timeBarFlipX);
            else if (prop == "flipY") lua_pushboolean(L, isBG ? PlayState::instance->timeBarBGFlipY : PlayState::instance->timeBarFlipY);
            else if (prop == "antialiasing") lua_pushboolean(L, isBG ? PlayState::instance->timeBarBGAntialiasing : PlayState::instance->timeBarAntialiasing);
            else lua_pushnil(L);
        }
        else if (obj == "scoretxt" || obj == "scoreTxt" || obj == "timetxt" || obj == "timeTxt") {
            bool isScore = (obj == "scoretxt" || obj == "scoreTxt");
            if (prop == "x") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtX : PlayState::instance->timeTxtX);
            else if (prop == "y") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtY : PlayState::instance->timeTxtY);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtScaleX : PlayState::instance->timeTxtScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtScaleY : PlayState::instance->timeTxtScaleY);
            else if (prop == "alpha") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtAlpha : PlayState::instance->timeTxtAlpha);
            else if (prop == "angle") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtAngle : PlayState::instance->timeTxtAngle);
            else if (prop == "visible") lua_pushboolean(L, isScore ? PlayState::instance->scoreTxtVisible : PlayState::instance->timeTxtVisible);
            else if (prop == "color") lua_pushnumber(L, isScore ? PlayState::instance->scoreTxtColor : PlayState::instance->timeTxtColor);
            else if (prop == "flipX") lua_pushboolean(L, isScore ? PlayState::instance->scoreTxtFlipX : PlayState::instance->timeTxtFlipX);
            else if (prop == "flipY") lua_pushboolean(L, isScore ? PlayState::instance->scoreTxtFlipY : PlayState::instance->timeTxtFlipY);
            else if (prop == "antialiasing") lua_pushboolean(L, isScore ? PlayState::instance->scoreTxtAntialiasing : PlayState::instance->timeTxtAntialiasing);
            else lua_pushnil(L);
        }
        else if (obj == "countdown") {
            if (prop == "x") lua_pushnumber(L, PlayState::instance->countdownX);
            else if (prop == "y") lua_pushnumber(L, PlayState::instance->countdownY);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, PlayState::instance->countdownScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, PlayState::instance->countdownScaleY);
            else if (prop == "scale") lua_pushnumber(L, PlayState::instance->countdownScale);
            else if (prop == "alpha") lua_pushnumber(L, PlayState::instance->countdownAlpha);
            else if (prop == "angle") lua_pushnumber(L, PlayState::instance->countdownAngle);
            else if (prop == "visible") lua_pushboolean(L, PlayState::instance->countdownVisible);
            else if (prop == "color") lua_pushnumber(L, PlayState::instance->countdownColor);
            else if (prop == "flipX") lua_pushboolean(L, PlayState::instance->countdownFlipX);
            else if (prop == "flipY") lua_pushboolean(L, PlayState::instance->countdownFlipY);
            else if (prop == "antialiasing") lua_pushboolean(L, PlayState::instance->countdownAntialiasing);
            else lua_pushnil(L);
        }
        else if (obj == "camGame" || obj == "game") {
            if (prop == "x") lua_pushnumber(L, PlayState::instance->camX_offset);
            else if (prop == "y") lua_pushnumber(L, PlayState::instance->camY_offset);
            else if (prop == "zoom") lua_pushnumber(L, PlayState::instance->camZoom);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, PlayState::instance->camScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, PlayState::instance->camScaleY);
            else if (prop == "angle") lua_pushnumber(L, PlayState::instance->camAngle);
            else if (prop == "alpha") lua_pushnumber(L, PlayState::instance->camAlpha);
            else if (prop == "visible") lua_pushboolean(L, PlayState::instance->camVisible);
            else if (prop == "flipX") lua_pushboolean(L, PlayState::instance->camFlipX);
            else if (prop == "flipY") lua_pushboolean(L, PlayState::instance->camFlipY);
            else lua_pushnil(L);
        }
        else if (obj == "camHUD" || obj == "hud") {
            if (prop == "x") lua_pushnumber(L, PlayState::instance->hudX_offset);
            else if (prop == "y") lua_pushnumber(L, PlayState::instance->hudY_offset);
            else if (prop == "zoom") lua_pushnumber(L, PlayState::instance->hudZoom);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, PlayState::instance->hudScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, PlayState::instance->hudScaleY);
            else if (prop == "angle") lua_pushnumber(L, PlayState::instance->hudAngle);
            else if (prop == "alpha") lua_pushnumber(L, PlayState::instance->hudAlpha);
            else if (prop == "visible") lua_pushboolean(L, PlayState::instance->hudVisible);
            else if (prop == "flipX") lua_pushboolean(L, PlayState::instance->hudFlipX);
            else if (prop == "flipY") lua_pushboolean(L, PlayState::instance->hudFlipY);
            else lua_pushnil(L);
        }
        else if (obj == "camOther" || obj == "other") {
            if (prop == "x") lua_pushnumber(L, PlayState::instance->otherX_offset);
            else if (prop == "y") lua_pushnumber(L, PlayState::instance->otherY_offset);
            else if (prop == "zoom") lua_pushnumber(L, PlayState::instance->otherZoom);
            else if (prop == "scale.x" || prop == "scaleX") lua_pushnumber(L, PlayState::instance->otherScaleX);
            else if (prop == "scale.y" || prop == "scaleY") lua_pushnumber(L, PlayState::instance->otherScaleY);
            else if (prop == "angle") lua_pushnumber(L, PlayState::instance->otherAngle);
            else if (prop == "alpha") lua_pushnumber(L, PlayState::instance->otherAlpha);
            else if (prop == "visible") lua_pushboolean(L, PlayState::instance->otherVisible);
            else if (prop == "flipX") lua_pushboolean(L, PlayState::instance->otherFlipX);
            else if (prop == "flipY") lua_pushboolean(L, PlayState::instance->otherFlipY);
            else lua_pushnil(L);
        }
        else if (obj == "boyfriend" || obj == "dad" || obj == "gf") {
            Character* c = nullptr;
            if (obj == "boyfriend") c = PlayState::instance->bf;
            else if (obj == "dad") c = PlayState::instance->dad;
            else if (obj == "gf") c = PlayState::instance->gf;

            if (c) {
                if (prop == "alpha") lua_pushnumber(L, c->alpha);
                else if (prop == "x") lua_pushnumber(L, c->x);
                else if (prop == "y") lua_pushnumber(L, c->y);
                else if (prop == "scale.x" || prop == "scale.y" || prop == "scale") lua_pushnumber(L, c->charScale);
                else if (prop == "visible") lua_pushboolean(L, c->visible);
                else if (prop == "angle") lua_pushnumber(L, c->angle);
                else if (prop == "curCharacter" || prop == "curCharacterName") lua_pushstring(L, c->curCharacterName.c_str());
                else if (prop == "healthIcon") lua_pushstring(L, c->healthIcon.c_str());
                else if (prop == "holdTimer") lua_pushnumber(L, c->holdTimer);
                else if (prop == "singDuration") lua_pushnumber(L, c->singDuration);
                else lua_pushnil(L);
            } else {
                lua_pushnil(L);
            }
        }
        else if (obj == "camGame" || obj == "game") {
            if (prop == "angle") lua_pushnumber(L, PlayState::instance->camAngle);
            else if (prop == "zoom") lua_pushnumber(L, PlayState::instance->camZoom);
            else lua_pushnil(L);
        }
        else if (obj == "camHUD" || obj == "hud") {
            if (prop == "angle") lua_pushnumber(L, PlayState::instance->hudAngle);
            else if (prop == "zoom") lua_pushnumber(L, PlayState::instance->hudZoom);
            else lua_pushnil(L);
        }
        else if (obj == "camFollow") {
            if (prop == "x") lua_pushnumber(L, PlayState::instance->camFollowX);
            else if (prop == "y") lua_pushnumber(L, PlayState::instance->camFollowY);
            else lua_pushnil(L);
        }
        else if (PlayState::instance->luaSpriteIndices.count(obj)) {
            auto& s = PlayState::instance->luaSprites[PlayState::instance->luaSpriteIndices[obj]];
            if (prop == "alpha") lua_pushnumber(L, s.alpha);
            else if (prop == "x") lua_pushnumber(L, s.x);
            else if (prop == "y") lua_pushnumber(L, s.y);
            else if (prop == "scale.x" || prop == "scale") lua_pushnumber(L, s.scaleX);
            else if (prop == "scale.y") lua_pushnumber(L, s.scaleY);
            else if (prop == "width") {
                float baseW = s.isGraphic ? s.graphicWidth : (s.img.subtex ? s.img.subtex->width : 0.0f);
                lua_pushnumber(L, baseW * s.scaleX);
            }
            else if (prop == "height") {
                float baseH = s.isGraphic ? s.graphicHeight : (s.img.subtex ? s.img.subtex->height : 0.0f);
                lua_pushnumber(L, baseH * s.scaleY);
            }
            else if (prop == "visible") lua_pushboolean(L, s.visible);
            else if (prop == "angle") lua_pushnumber(L, s.angle);
            else if (prop == "flipX") lua_pushboolean(L, s.flipX);
            else if (prop == "flipY") lua_pushboolean(L, s.flipY);
            else if (prop == "antialiasing") lua_pushboolean(L, s.antialiasing);
            else lua_pushnil(L);
        }
        else if (PlayState::instance->luaTextIndices.count(obj)) {
            auto& t = PlayState::instance->luaTexts[PlayState::instance->luaTextIndices[obj]];
            if (prop == "alpha") lua_pushnumber(L, t.alpha);
            else if (prop == "x") lua_pushnumber(L, t.x);
            else if (prop == "y") lua_pushnumber(L, t.y);
            else if (prop == "visible") lua_pushboolean(L, t.visible);
            else if (prop == "size") lua_pushnumber(L, t.size * 32.0f);
        }
        else if (obj == "boyfriend" || obj == "dad" || obj == "gf") {
            Character* c = nullptr;
            if (obj == "boyfriend") c = PlayState::instance->bf;
            else if (obj == "dad") c = PlayState::instance->dad;
            else if (obj == "gf") c = PlayState::instance->gf;

            if (c) {
                if (prop == "alpha") lua_pushnumber(L, c->alpha);
                else if (prop == "x") lua_pushnumber(L, c->x);
                else if (prop == "y") lua_pushnumber(L, c->y);
                else if (prop == "visible") lua_pushboolean(L, c->visible);
                else if (prop == "antialiasing") lua_pushboolean(L, !c->noAntialiasing);
                else lua_pushnil(L);
            } else {
                lua_pushnil(L);
            }
        }
        else {
            lua_pushnil(L);
        }
    }
    return 1;
}

int LuaManager::lua_setProperty(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 2) return 0;
    
    auto getBoolSafe = [&](int idx) {
        if (lua_isboolean(L, idx)) return lua_toboolean(L, idx) != 0;
        if (lua_isstring(L, idx)) {
            std::string s = lua_tostring(L, idx);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return (s == "true" || s == "1");
        }
        if (lua_isnumber(L, idx)) return lua_tointeger(L, idx) != 0;
        return false;
    };

    std::string variable = luaL_checkstring(L, 1);
    
    std::string obj = "";
    std::string prop = variable;
    size_t firstDot = variable.find_first_of('.');
    if (firstDot != std::string::npos) {
        obj = variable.substr(0, firstDot);
        prop = variable.substr(firstDot + 1);
    }

    if (obj == "" || obj == "this") {
        if (prop == "health") {
            float val = (float)luaL_checknumber(L, 2);
            PlayState::instance->health = val;
            if (PlayState::instance->health > 2.0f) PlayState::instance->health = 2.0f;
            if (PlayState::instance->health < 0.0f) PlayState::instance->health = 0.0f;
        }
        else if (prop == "score") PlayState::instance->score = (int)luaL_checkinteger(L, 2);
        else if (prop == "misses") PlayState::instance->misses = (int)luaL_checkinteger(L, 2);
        else if (prop == "combo") PlayState::instance->combo = (int)luaL_checkinteger(L, 2);
        else if (prop == "inGameOver") PlayState::instance->gameOver = getBoolSafe(2);
        else if (prop == "camZoom") PlayState::instance->camZoom = (float)luaL_checknumber(L, 2);
        else if (prop == "hudZoom") PlayState::instance->hudZoom = (float)luaL_checknumber(L, 2);
        else if (prop == "targetZoom") PlayState::instance->targetZoom = (float)luaL_checknumber(L, 2);
        else if (prop == "defaultCamZoom") PlayState::instance->targetZoom = (float)luaL_checknumber(L, 2);
        else if (prop == "autoIconPosition") PlayState::instance->autoIconPosition = lua_toboolean(L, 2);
        else if (prop == "camFollowX") PlayState::instance->camFollowX = (float)luaL_checknumber(L, 2);
        else if (prop == "camFollowY") PlayState::instance->camFollowY = (float)luaL_checknumber(L, 2);
        else if (prop == "camFollowLocked") PlayState::instance->camFollowLocked = lua_toboolean(L, 2);
        else if (prop == "showGrid") {
            PlayState::instance->showGrid = lua_toboolean(L, 2);
            //logtofile("[C++] setProperty: showGrid set to " + std::string(PlayState::instance->showGrid ? "true" : "false"));
        }
        else if (prop == "legacyPositioning") PlayState::instance->legacyPositioning = lua_toboolean(L, 2);
        else if (prop == "playerUnderlayAlpha") PlayState::instance->playerUnderlayAlpha = (float)luaL_checknumber(L, 2);
        else if (prop == "opponentUnderlayAlpha") PlayState::instance->opponentUnderlayAlpha = (float)luaL_checknumber(L, 2);
        else if (prop == "playerUnderlayVisible") PlayState::instance->playerUnderlayVisible = getBoolSafe(2);
        else if (prop == "opponentUnderlayVisible") PlayState::instance->opponentUnderlayVisible = getBoolSafe(2);
        else if (prop == "playerUnderlayColor") PlayState::instance->playerUnderlayColor = getLuaColorOrHex(L, 2);
        else if (prop == "opponentUnderlayColor") PlayState::instance->opponentUnderlayColor = getLuaColorOrHex(L, 2);
    } else {
        if (obj == "scoretxt" || obj == "scoreTxt" || obj == "timetxt" || obj == "timeTxt") {
            bool isScore = (obj == "scoretxt" || obj == "scoreTxt");
            if (prop == "x") {
                if (isScore) PlayState::instance->scoreTxtX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeTxtX = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "y") {
                if (isScore) PlayState::instance->scoreTxtY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeTxtY = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.x" || prop == "scaleX") {
                if (isScore) PlayState::instance->scoreTxtScaleX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeTxtScaleX = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.y" || prop == "scaleY") {
                if (isScore) PlayState::instance->scoreTxtScaleY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeTxtScaleY = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "alpha") {
                if (isScore) PlayState::instance->scoreTxtAlpha = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeTxtAlpha = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "angle") {
                if (isScore) PlayState::instance->scoreTxtAngle = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeTxtAngle = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "visible") {
                if (isScore) PlayState::instance->scoreTxtVisible = getBoolSafe(2);
                else PlayState::instance->timeTxtVisible = getBoolSafe(2);
            }
            else if (prop == "color") {
                if (isScore) PlayState::instance->scoreTxtColor = getLuaColorOrHex(L, 2);
                else PlayState::instance->timeTxtColor = getLuaColorOrHex(L, 2);
            }
            else if (prop == "flipX") {
                if (isScore) PlayState::instance->scoreTxtFlipX = getBoolSafe(2);
                else PlayState::instance->timeTxtFlipX = getBoolSafe(2);
            }
            else if (prop == "flipY") {
                if (isScore) PlayState::instance->scoreTxtFlipY = getBoolSafe(2);
                else PlayState::instance->timeTxtFlipY = getBoolSafe(2);
            }
            else if (prop == "antialiasing") {
                if (isScore) PlayState::instance->scoreTxtAntialiasing = getBoolSafe(2);
                else PlayState::instance->timeTxtAntialiasing = getBoolSafe(2);
            }
        }
        else if (obj == "timebar" || obj == "timebarBg" || obj == "timeBar" || obj == "timeBarBG" || obj == "timebarBG") {
            bool isBG = (obj == "timebarBg" || obj == "timeBarBG" || obj == "timebarBG");
            if (prop == "x") {
                if (isBG) PlayState::instance->timeBarBGX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeBarX_val = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "y") {
                if (isBG) PlayState::instance->timeBarBGY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeBarY_val = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.x" || prop == "scaleX") {
                if (isBG) PlayState::instance->timeBarBGScaleX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeBarScaleX = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.y" || prop == "scaleY") {
                if (isBG) PlayState::instance->timeBarBGScaleY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeBarScaleY = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "alpha") {
                if (isBG) PlayState::instance->timeBarBGAlpha = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeBarAlpha = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "angle") {
                if (isBG) PlayState::instance->timeBarBGAngle = (float)luaL_checknumber(L, 2);
                else PlayState::instance->timeBarAngle = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "visible") {
                if (isBG) PlayState::instance->timeBarBGVisible = getBoolSafe(2);
                else PlayState::instance->timeBarVisible = getBoolSafe(2);
            }
            else if (prop == "color") {
                if (isBG) PlayState::instance->timeBarBGColor = getLuaColorOrHex(L, 2);
                else PlayState::instance->timeBarColor = getLuaColorOrHex(L, 2);
            }
            else if (prop == "flipX") {
                if (isBG) PlayState::instance->timeBarBGFlipX = getBoolSafe(2);
                else PlayState::instance->timeBarFlipX = getBoolSafe(2);
            }
            else if (prop == "flipY") {
                if (isBG) PlayState::instance->timeBarBGFlipY = getBoolSafe(2);
                else PlayState::instance->timeBarFlipY = getBoolSafe(2);
            }
            else if (prop == "antialiasing") {
                if (isBG) PlayState::instance->timeBarBGAntialiasing = getBoolSafe(2);
                else PlayState::instance->timeBarAntialiasing = getBoolSafe(2);
            }
        }
        else if (obj == "healthbar" || obj == "healthbarBg" || obj == "healthBar" || obj == "healthBarBG" || obj == "healthbarBG") {
            bool isBG = (obj == "healthbarBg" || obj == "healthBarBG" || obj == "healthbarBG");
            if (prop == "x") {
                if (isBG) PlayState::instance->healthBarBGX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->healthBarX = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "y") {
                if (isBG) PlayState::instance->healthBarBGY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->healthBarY = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.x" || prop == "scaleX") {
                if (isBG) PlayState::instance->healthBarBGScaleX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->healthBarScaleX = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.y" || prop == "scaleY") {
                if (isBG) PlayState::instance->healthBarBGScaleY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->healthBarScaleY = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "alpha") {
                if (isBG) PlayState::instance->healthBarBGAlpha = (float)luaL_checknumber(L, 2);
                else PlayState::instance->healthBarAlpha = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "angle") {
                if (isBG) PlayState::instance->healthBarBGAngle = (float)luaL_checknumber(L, 2);
                else PlayState::instance->healthBarAngle = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "visible") {
                if (isBG) PlayState::instance->healthBarBGVisible = getBoolSafe(2);
                else PlayState::instance->healthBarVisible = getBoolSafe(2);
            }
            else if (prop == "color") {
                if (isBG) PlayState::instance->healthBarBGColor = getLuaColorOrHex(L, 2);
                else PlayState::instance->healthBarColor = getLuaColorOrHex(L, 2);
            }
            else if (prop == "flipX") {
                if (isBG) PlayState::instance->healthBarBGFlipX = getBoolSafe(2);
                else PlayState::instance->healthBarFlipX = getBoolSafe(2);
            }
            else if (prop == "flipY") {
                if (isBG) PlayState::instance->healthBarBGFlipY = getBoolSafe(2);
                else PlayState::instance->healthBarFlipY = getBoolSafe(2);
            }
            else if (prop == "antialiasing") {
                if (isBG) PlayState::instance->healthBarBGAntialiasing = getBoolSafe(2);
                else PlayState::instance->healthBarAntialiasing = getBoolSafe(2);
            }
        }
        else if (obj == "iconP1" || obj == "iconP2") {
            bool isP1 = (obj == "iconP1");
            if (prop == "x") {
                if (isP1) PlayState::instance->iconP1X = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2X = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "y") {
                if (isP1) PlayState::instance->iconP1Y = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2Y = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.x" || prop == "scaleX") {
                if (isP1) PlayState::instance->iconP1ScaleX = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2ScaleX = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale.y" || prop == "scaleY") {
                if (isP1) PlayState::instance->iconP1ScaleY = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2ScaleY = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "scale") {
                if (isP1) PlayState::instance->iconP1Scale = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2Scale = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "alpha") {
                if (isP1) PlayState::instance->iconP1Alpha = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2Alpha = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "angle") {
                if (isP1) PlayState::instance->iconP1Angle = (float)luaL_checknumber(L, 2);
                else PlayState::instance->iconP2Angle = (float)luaL_checknumber(L, 2);
            }
            else if (prop == "color") {
                if (isP1) PlayState::instance->iconP1Color = getLuaColorOrHex(L, 2);
                else PlayState::instance->iconP2Color = getLuaColorOrHex(L, 2);
            }
            else if (prop == "visible") {
                if (isP1) PlayState::instance->iconP1Visible = getBoolSafe(2);
                else PlayState::instance->iconP2Visible = getBoolSafe(2);
            }
            else if (prop == "flipX") {
                if (isP1) PlayState::instance->iconP1FlipX = getBoolSafe(2);
                else PlayState::instance->iconP2FlipX = getBoolSafe(2);
            }
            else if (prop == "flipY") {
                if (isP1) PlayState::instance->iconP1FlipY = getBoolSafe(2);
                else PlayState::instance->iconP2FlipY = getBoolSafe(2);
            }
            else if (prop == "antialiasing") {
                if (isP1) PlayState::instance->iconP1Antialiasing = getBoolSafe(2);
                else PlayState::instance->iconP2Antialiasing = getBoolSafe(2);
            }
        }
        else if (obj == "countdown") {
            if (prop == "x") PlayState::instance->countdownX = (float)luaL_checknumber(L, 2);
            else if (prop == "y") PlayState::instance->countdownY = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.x" || prop == "scaleX") PlayState::instance->countdownScaleX = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.y" || prop == "scaleY") PlayState::instance->countdownScaleY = (float)luaL_checknumber(L, 2);
            else if (prop == "scale") PlayState::instance->countdownScale = (float)luaL_checknumber(L, 2);
            else if (prop == "alpha") PlayState::instance->countdownAlpha = (float)luaL_checknumber(L, 2);
            else if (prop == "angle") PlayState::instance->countdownAngle = (float)luaL_checknumber(L, 2);
            else if (prop == "visible") PlayState::instance->countdownVisible = getBoolSafe(2);
            else if (prop == "color") PlayState::instance->countdownColor = getLuaColorOrHex(L, 2);
            else if (prop == "flipX") PlayState::instance->countdownFlipX = getBoolSafe(2);
            else if (prop == "flipY") PlayState::instance->countdownFlipY = getBoolSafe(2);
            else if (prop == "antialiasing") PlayState::instance->countdownAntialiasing = getBoolSafe(2);
        }
        else if (obj == "camGame" || obj == "game") {
            if (prop == "x") PlayState::instance->camX_offset = (float)luaL_checknumber(L, 2);
            else if (prop == "y") PlayState::instance->camY_offset = (float)luaL_checknumber(L, 2);
            else if (prop == "zoom") PlayState::instance->camZoom = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.x" || prop == "scaleX") PlayState::instance->camScaleX = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.y" || prop == "scaleY") PlayState::instance->camScaleY = (float)luaL_checknumber(L, 2);
            else if (prop == "angle") PlayState::instance->camAngle = (float)luaL_checknumber(L, 2);
            else if (prop == "alpha") PlayState::instance->camAlpha = (float)luaL_checknumber(L, 2);
            else if (prop == "visible") PlayState::instance->camVisible = getBoolSafe(2);
            else if (prop == "flipX") PlayState::instance->camFlipX = getBoolSafe(2);
            else if (prop == "flipY") PlayState::instance->camFlipY = getBoolSafe(2);
        }
        else if (obj == "camHUD" || obj == "hud") {
            if (prop == "x") PlayState::instance->hudX_offset = (float)luaL_checknumber(L, 2);
            else if (prop == "y") PlayState::instance->hudY_offset = (float)luaL_checknumber(L, 2);
            else if (prop == "zoom") PlayState::instance->hudZoom = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.x" || prop == "scaleX") PlayState::instance->hudScaleX = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.y" || prop == "scaleY") PlayState::instance->hudScaleY = (float)luaL_checknumber(L, 2);
            else if (prop == "angle") PlayState::instance->hudAngle = (float)luaL_checknumber(L, 2);
            else if (prop == "alpha") PlayState::instance->hudAlpha = (float)luaL_checknumber(L, 2);
            else if (prop == "visible") PlayState::instance->hudVisible = getBoolSafe(2);
            else if (prop == "flipX") PlayState::instance->hudFlipX = getBoolSafe(2);
            else if (prop == "flipY") PlayState::instance->hudFlipY = getBoolSafe(2);
        }
        else if (obj == "camOther" || obj == "other") {
            if (prop == "x") PlayState::instance->otherX_offset = (float)luaL_checknumber(L, 2);
            else if (prop == "y") PlayState::instance->otherY_offset = (float)luaL_checknumber(L, 2);
            else if (prop == "zoom") PlayState::instance->otherZoom = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.x" || prop == "scaleX") PlayState::instance->otherScaleX = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.y" || prop == "scaleY") PlayState::instance->otherScaleY = (float)luaL_checknumber(L, 2);
            else if (prop == "angle") PlayState::instance->otherAngle = (float)luaL_checknumber(L, 2);
            else if (prop == "alpha") PlayState::instance->otherAlpha = (float)luaL_checknumber(L, 2);
            else if (prop == "visible") PlayState::instance->otherVisible = getBoolSafe(2);
            else if (prop == "flipX") PlayState::instance->otherFlipX = getBoolSafe(2);
            else if (prop == "flipY") PlayState::instance->otherFlipY = getBoolSafe(2);
        }
        else if (obj == "camFollow") {
            if (prop == "x") PlayState::instance->camFollowX = (float)luaL_checknumber(L, 2);
            else if (prop == "y") PlayState::instance->camFollowY = (float)luaL_checknumber(L, 2);
        }
        else if (PlayState::instance->luaSpriteIndices.count(obj)) {
            auto& s = PlayState::instance->luaSprites[PlayState::instance->luaSpriteIndices[obj]];
            if (prop == "alpha") s.alpha = (float)luaL_checknumber(L, 2);
            else if (prop == "x") s.x = (float)luaL_checknumber(L, 2);
            else if (prop == "y") s.y = (float)luaL_checknumber(L, 2);
            else if (prop == "scale.x" || prop == "scale") {
                float val = (float)luaL_checknumber(L, 2);
                s.scaleX = val;
                s.scale = val;
                if (prop == "scale") s.scaleY = val;
            }
            else if (prop == "scale.y") s.scaleY = (float)luaL_checknumber(L, 2);
            else if (prop == "visible") s.visible = lua_toboolean(L, 2);
            else if (prop == "angle") s.angle = (float)luaL_checknumber(L, 2);
            else if (prop == "flipX") s.flipX = getBoolSafe(2);
            else if (prop == "flipY") s.flipY = getBoolSafe(2);
            else if (prop == "color") s.graphicColor = getLuaColorOrHex(L, 2);
            else if (prop == "width") s.graphicWidth = (float)luaL_checknumber(L, 2);
            else if (prop == "height") s.graphicHeight = (float)luaL_checknumber(L, 2);
            else if (prop == "antialiasing") {
                s.antialiasing = lua_toboolean(L, 2);
                GPU_TEXTURE_FILTER_PARAM f = s.antialiasing ? GPU_LINEAR : GPU_NEAREST;
                if (s.img.tex) C3D_TexSetFilter(s.img.tex, f, f);
                for (auto& fr : s.frames) { if (fr.tex) C3D_TexSetFilter(fr.tex, f, f); }
            }
        }
        else if (PlayState::instance->luaTextIndices.count(obj)) {
            auto& t = PlayState::instance->luaTexts[PlayState::instance->luaTextIndices[obj]];
            if (prop == "alpha") t.alpha = (float)luaL_checknumber(L, 2);
            else if (prop == "x") t.x = (float)luaL_checknumber(L, 2);
            else if (prop == "y") t.y = (float)luaL_checknumber(L, 2);
            else if (prop == "visible") t.visible = getBoolSafe(2);
            else if (prop == "size") t.size = (float)luaL_checknumber(L, 2) / 32.0f;
            else if (prop == "text") {
                std::string newText = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
                if (t.text != newText) {
                    t.text = newText;
                    t.dirty = true;
                }
            }
        }
        else if (obj == "boyfriend" || obj == "dad" || obj == "gf") {
            Character* c = nullptr;
            if (obj == "boyfriend") c = PlayState::instance->bf;
            else if (obj == "dad") c = PlayState::instance->dad;
            else if (obj == "gf") c = PlayState::instance->gf;

            if (c) {
                if (prop == "alpha") c->alpha = (float)luaL_checknumber(L, 2);
                else if (prop == "x") c->x = (float)luaL_checknumber(L, 2);
                else if (prop == "y") c->y = (float)luaL_checknumber(L, 2);
                else if (prop == "scale.x" || prop == "scale") {
                    float val = (float)luaL_checknumber(L, 2);
                    c->charScaleX = val;
                    if (prop == "scale") c->charScaleY = val;
                }
                else if (prop == "scale.y") {
                    c->charScaleY = (float)luaL_checknumber(L, 2);
                }
                else if (prop == "visible") c->visible = lua_toboolean(L, 2);
                else if (prop == "angle") c->angle = (float)luaL_checknumber(L, 2);
                else if (prop == "antialiasing") c->setAntialiasing(lua_toboolean(L, 2));
            }
        }
    }
    return 0;
}int LuaManager::lua_getPropertyFromGroup(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) {
        lua_pushnil(L);
        return 1;
    }
    std::string group = luaL_checkstring(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    std::string prop = luaL_checkstring(L, 3);
    
    bool isPlayerGroup = (group == "playerStrums");
    bool isStrumGroup = (group == "playerStrums" || group == "opponentStrums");

    if (isStrumGroup && index >= 0 && index < 4) {
        if (prop == "x") {
            lua_pushnumber(L, PlayState::instance->getLaneX(index, isPlayerGroup));
            return 1;
        }
        else if (prop == "y") {
            lua_pushnumber(L, PlayState::instance->getLaneY(index, isPlayerGroup));
            return 1;
        }
        else if (prop == "scale.x" || prop == "scaleX") {
            lua_pushnumber(L, isPlayerGroup ? PlayState::instance->customPlayerStrumScaleX[index] : PlayState::instance->customOpponentStrumScaleX[index]);
            return 1;
        }
        else if (prop == "scale.y" || prop == "scaleY") {
            lua_pushnumber(L, isPlayerGroup ? PlayState::instance->customPlayerStrumScaleY[index] : PlayState::instance->customOpponentStrumScaleY[index]);
            return 1;
        }
        else if (prop == "alpha") {
            lua_pushnumber(L, PlayState::instance->getLaneAlpha(index, isPlayerGroup));
            return 1;
        }
        else if (prop == "angle") {
            lua_pushnumber(L, PlayState::instance->getLaneAngle(index, isPlayerGroup));
            return 1;
        }
        else if (prop == "visible") {
            lua_pushboolean(L, isPlayerGroup ? PlayState::instance->customPlayerStrumVisible[index] : PlayState::instance->customOpponentStrumVisible[index]);
            return 1;
        }
        else if (prop == "color") {
            lua_pushnumber(L, isPlayerGroup ? PlayState::instance->customPlayerStrumColor[index] : PlayState::instance->customOpponentStrumColor[index]);
            return 1;
        }
        else if (prop == "flipX") {
            lua_pushboolean(L, isPlayerGroup ? PlayState::instance->customPlayerStrumFlipX[index] : PlayState::instance->customOpponentStrumFlipX[index]);
            return 1;
        }
        else if (prop == "flipY") {
            lua_pushboolean(L, isPlayerGroup ? PlayState::instance->customPlayerStrumFlipY[index] : PlayState::instance->customOpponentStrumFlipY[index]);
            return 1;
        }
        else if (prop == "antialiasing") {
            lua_pushboolean(L, isPlayerGroup ? PlayState::instance->customPlayerStrumAntialiasing[index] : PlayState::instance->customOpponentStrumAntialiasing[index]);
            return 1;
        }
    }
    else if ((group == "notes" || group == "unspawnNotes") && index >= 0 && index < (int)PlayState::instance->songNotes.size()) {
        Note& n = PlayState::instance->songNotes[index];
        if (prop == "mustPress" || prop == "isPlayer") {
            lua_pushboolean(L, n.isPlayer);
            return 1;
        }
        else if (prop == "x") {
            float laneCenterX = PlayState::instance->getLaneX(n.noteData, n.isPlayer) + n.offsetX;
            lua_pushnumber(L, laneCenterX);
            return 1;
        }
        else if (prop == "y") {
            float laneCenterY = PlayState::instance->getLaneY(n.noteData, n.isPlayer) + n.offsetY;
            lua_pushnumber(L, laneCenterY);
            return 1;
        }
        else if (prop == "offsetX") {
            lua_pushnumber(L, n.offsetX);
            return 1;
        }
        else if (prop == "offsetY") {
            lua_pushnumber(L, n.offsetY);
            return 1;
        }
        else if (prop == "scale.x" || prop == "scaleX") {
            lua_pushnumber(L, n.scaleX);
            return 1;
        }
        else if (prop == "scale.y" || prop == "scaleY") {
            lua_pushnumber(L, n.scaleY);
            return 1;
        }
        else if (prop == "alpha" || prop == "multAlpha") {
            lua_pushnumber(L, n.multAlpha);
            return 1;
        }
        else if (prop == "angle") {
            lua_pushnumber(L, n.angle);
            return 1;
        }
        else if (prop == "visible") {
            lua_pushboolean(L, n.visible);
            return 1;
        }
        else if (prop == "color") {
            lua_pushnumber(L, n.color);
            return 1;
        }
        else if (prop == "flipX") {
            lua_pushboolean(L, n.flipX);
            return 1;
        }
        else if (prop == "flipY") {
            lua_pushboolean(L, n.flipY);
            return 1;
        }
        else if (prop == "antialiasing") {
            lua_pushboolean(L, n.antialiasing);
            return 1;
        }
        else if (prop == "noteType") {
            lua_pushstring(L, n.noteType.c_str());
            return 1;
        }
        else if (prop == "isSustainNote") {
            lua_pushboolean(L, n.sustainLength > 0.0f);
            return 1;
        }
    }
    
    lua_pushnil(L);
    return 1;
}

int LuaManager::lua_setPropertyFromGroup(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 4) return 0;
    std::string group = luaL_checkstring(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    std::string prop = luaL_checkstring(L, 3);
    
    bool isPlayerGroup = (group == "playerStrums");
    bool isStrumGroup = (group == "playerStrums" || group == "opponentStrums");

    if (isStrumGroup && index >= 0 && index < 4) {
        if (prop == "x") {
            float val = (float)luaL_checknumber(L, 4);
            if (isPlayerGroup) PlayState::instance->customPlayerStrumX[index] = val;
            else PlayState::instance->customOpponentStrumX[index] = val;
        }
        else if (prop == "y") {
            float val = (float)luaL_checknumber(L, 4);
            if (isPlayerGroup) PlayState::instance->customPlayerStrumY[index] = val;
            else PlayState::instance->customOpponentStrumY[index] = val;
        }
        else if (prop == "scale.x" || prop == "scaleX") {
            float val = (float)luaL_checknumber(L, 4);
            if (isPlayerGroup) PlayState::instance->customPlayerStrumScaleX[index] = val;
            else PlayState::instance->customOpponentStrumScaleX[index] = val;
        }
        else if (prop == "scale.y" || prop == "scaleY") {
            float val = (float)luaL_checknumber(L, 4);
            if (isPlayerGroup) PlayState::instance->customPlayerStrumScaleY[index] = val;
            else PlayState::instance->customOpponentStrumScaleY[index] = val;
        }
        else if (prop == "alpha") {
            float val = (float)luaL_checknumber(L, 4);
            if (isPlayerGroup) PlayState::instance->customPlayerStrumAlpha[index] = val;
            else PlayState::instance->customOpponentStrumAlpha[index] = val;
        }
        else if (prop == "angle") {
            float val = (float)luaL_checknumber(L, 4);
            if (isPlayerGroup) PlayState::instance->customPlayerStrumAngle[index] = val;
            else PlayState::instance->customOpponentStrumAngle[index] = val;
        }
        else if (prop == "visible") {
            bool val = lua_toboolean(L, 4) != 0;
            if (isPlayerGroup) PlayState::instance->customPlayerStrumVisible[index] = val;
            else PlayState::instance->customOpponentStrumVisible[index] = val;
        }
        else if (prop == "color") {
             u32 val = getLuaColorOrHex(L, 4);
             if (isPlayerGroup) PlayState::instance->customPlayerStrumColor[index] = val;
             else PlayState::instance->customOpponentStrumColor[index] = val;
         }
        else if (prop == "flipX") {
            bool val = lua_toboolean(L, 4) != 0;
            if (isPlayerGroup) PlayState::instance->customPlayerStrumFlipX[index] = val;
            else PlayState::instance->customOpponentStrumFlipX[index] = val;
        }
        else if (prop == "flipY") {
            bool val = lua_toboolean(L, 4) != 0;
            if (isPlayerGroup) PlayState::instance->customPlayerStrumFlipY[index] = val;
            else PlayState::instance->customOpponentStrumFlipY[index] = val;
        }
        else if (prop == "antialiasing") {
            bool val = lua_toboolean(L, 4) != 0;
            if (isPlayerGroup) PlayState::instance->customPlayerStrumAntialiasing[index] = val;
            else PlayState::instance->customOpponentStrumAntialiasing[index] = val;
        }
    }
    else if ((group == "notes" || group == "unspawnNotes") && index >= 0 && index < (int)PlayState::instance->songNotes.size()) {
        Note& n = PlayState::instance->songNotes[index];
        if (prop == "x") {
            float val = (float)luaL_checknumber(L, 4);
            float baseLaneX = PlayState::instance->getLaneX(n.noteData, n.isPlayer);
            n.offsetX = val - baseLaneX;
        }
        else if (prop == "y") {
            float val = (float)luaL_checknumber(L, 4);
            float baseLaneY = PlayState::instance->getLaneY(n.noteData, n.isPlayer);
            n.offsetY = val - baseLaneY;
        }
        else if (prop == "offsetX") {
            n.offsetX = (float)luaL_checknumber(L, 4);
        }
        else if (prop == "offsetY") {
            n.offsetY = (float)luaL_checknumber(L, 4);
        }
        else if (prop == "texture") {
            n.texture = luaL_checkstring(L, 4);
        }
        else if (prop == "noAnimation") {
            n.noAnimation = lua_toboolean(L, 4);
        }
        else if (prop == "isSustainNote") {
            if (!lua_toboolean(L, 4)) {
                n.sustainLength = 0.0f;
            }
        }
        else if (prop == "scale.x" || prop == "scaleX") {
            n.scaleX = (float)luaL_checknumber(L, 4);
        }
        else if (prop == "scale.y" || prop == "scaleY") {
            n.scaleY = (float)luaL_checknumber(L, 4);
        }
        else if (prop == "alpha" || prop == "multAlpha") {
            n.multAlpha = (float)luaL_checknumber(L, 4);
        }
        else if (prop == "angle") {
            n.angle = (float)luaL_checknumber(L, 4);
        }
        else if (prop == "visible") {
            n.visible = lua_toboolean(L, 4);
        }
        else if (prop == "color") {
             n.color = getLuaColorOrHex(L, 4);
         }
        else if (prop == "flipX") {
            n.flipX = lua_toboolean(L, 4);
        }
        else if (prop == "flipY") {
            n.flipY = lua_toboolean(L, 4);
        }
        else if (prop == "antialiasing") {
            n.antialiasing = lua_toboolean(L, 4);
        }
    }
    return 0;
}

int LuaManager::lua_setTextureManualMode(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    PlayState::instance->luaManualTextureMode = lua_toboolean(L, 1);
    return 0;
}

int LuaManager::lua_loadTexture(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) {
        lua_pushinteger(L, 0);
        return 1;
    }
    std::string imgName = luaL_checkstring(L, 1);

    // --- Deduplication: check if this texture is already loaded in namedTextureRegistry ---
    for (auto& pair : PlayState::instance->namedTextureRegistry) {
        if (pair.second.originalPath == imgName) {
            pair.second.refCount++;
            lua_pushinteger(L, pair.first);
            return 1;
        }
    }
    
    SharedLuaTexture shared;
    shared.originalPath = imgName;
    bool loaded = false;

    bool isAbsolute = (imgName.find("romfs:/") == 0 || imgName.find("sdmc:/") == 0);

    // --- 0. Try consuming from AsyncAssetManager FIRST ---
    if (AsyncAssetManager::get().isImageReady(imgName)) {
        ImageData* data = AsyncAssetManager::get().consumeImage(imgName);
        if (data) {
            if (data->tex) {
                C3D_TexSetFilter(data->tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                                            ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
                shared.tex    = data->tex;
                shared.subtex = data->subtex;
                shared.sheet  = nullptr;
                shared.vramData = data->fileBuffer;
                data->fileBuffer = nullptr;
                data->tex     = nullptr;
                data->subtex  = nullptr;
                loaded = true;
            } else if (data->sheet) {
                shared.sheet  = data->sheet;
                C2D_Image img = C2D_SpriteSheetGetImage(shared.sheet, 0);
                C3D_TexSetFilter(img.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                                          ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
                shared.tex    = img.tex;
                shared.subtex = (const Tex3DS_SubTexture*)img.subtex;
                shared.vramData = data->fileBuffer;
                data->fileBuffer = nullptr;
                data->sheet   = nullptr;
                loaded = true;
            }
            delete data;
        }
    }

    // --- 1. Try .t3x ---
    std::string t3xPath = "";
    if (!loaded) {
        if (isAbsolute) {
            t3xPath = imgName;
            if (t3xPath.find('.') == std::string::npos) t3xPath += ".t3x";
            if (!Paths::fileExists(t3xPath)) t3xPath = "";
        } else {
            std::string candidate = Paths::image(imgName, "preload");
            if (Paths::fileExists(candidate)) {
                t3xPath = candidate;
            } else {
                candidate = Paths::image(imgName, "shared");
                if (Paths::fileExists(candidate)) {
                    t3xPath = candidate;
                } else {
                    std::string modT3x = ModHandler::get().getModPath("images/" + imgName + ".t3x");
                    if (!modT3x.empty() && Paths::fileExists(modT3x)) t3xPath = modT3x;
                }
            }
        }

        if (!t3xPath.empty() && t3xPath.find(".t3x") != std::string::npos) {
            shared.sheet = C2D_SpriteSheetLoad(t3xPath.c_str());
            if (shared.sheet) {
                C2D_Image img = C2D_SpriteSheetGetImage(shared.sheet, 0);
                C3D_TexSetFilter(img.tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                                          ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
                shared.tex    = img.tex;
                shared.subtex = (const Tex3DS_SubTexture*)img.subtex;
                loaded = true;
            }
        }
    }

    if (!loaded) {
        std::string rawPath = "";
        if (isAbsolute) {
            std::string candidate = imgName;
            if (candidate.find('.') == std::string::npos) candidate += ".rawtex";
            if (Paths::fileExists(candidate)) rawPath = candidate;
        } else {
            rawPath = ModHandler::get().getModPath("images/" + imgName + ".rawtex");
            if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".rawtex"))
                rawPath = "romfs:/preload/images/" + imgName + ".rawtex";
            if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".rawtex"))
                rawPath = "romfs:/shared/images/" + imgName + ".rawtex";
        }

        if (!rawPath.empty()) {
            FILE* f = fopen(rawPath.c_str(), "rb");
            if (f) {
                RawTexHeader header;
                if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                    C3D_Tex* tex = new C3D_Tex();
                    if (C3D_TexInit(tex, header.width, header.height, GPU_RGBA8)) {
                        C3D_TexSetFilter(tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                                              ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
                        size_t dataSize = (size_t)header.width * header.height * 4;
                        void* data = linearAlloc(dataSize);
                        if (data) {
                            fread(data, dataSize, 1, f);
                            C3D_TexUpload(tex, data);
                            C3D_TexFlush(tex);
                            linearFree(data);

                            Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                            sub->width  = header.origW;
                            sub->height = header.origH;
                            sub->left   = 0.0f;
                            sub->top    = 1.0f;
                            sub->right  = (float)header.origW / header.width;
                            sub->bottom = 1.0f - ((float)header.origH / header.height);

                            shared.tex    = tex;
                            shared.subtex = sub;
                            shared.sheet  = nullptr;
                            loaded = true;
                        } else { delete tex; }
                    } else { delete tex; }
                }
                fclose(f);
            }
        }
    }

    if (!loaded) {
        std::string pngFilePath = "";
        if (isAbsolute) {
            std::string candidate = imgName;
            if (candidate.find('.') == std::string::npos) candidate += ".png";
            if (Paths::fileExists(candidate)) pngFilePath = candidate;
        } else {
            pngFilePath = ModHandler::get().getModPath("images/" + imgName + ".png");
            if (pngFilePath.empty() && Paths::fileExists("romfs:/preload/images/" + imgName + ".png"))
                pngFilePath = "romfs:/preload/images/" + imgName + ".png";
            if (pngFilePath.empty() && Paths::fileExists("romfs:/shared/images/" + imgName + ".png"))
                pngFilePath = "romfs:/shared/images/" + imgName + ".png";
        }

        if (!pngFilePath.empty()) {
            int w, h, c;
            unsigned char* data = stbi_load(pngFilePath.c_str(), &w, &h, &c, 4);
            if (data) {
                int pw = 1, ph = 1;
                while (pw < w) pw *= 2;
                while (ph < h) ph *= 2;

                C3D_Tex* tex = new C3D_Tex();
                if (C3D_TexInit(tex, pw, ph, GPU_RGBA8)) {
                    C3D_TexSetFilter(tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST,
                                          ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST);
                    uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                    if (swizzled) {
                        memset(swizzled, 0, pw * ph * 4);
                        for (int y = 0; y < h; y++) {
                            for (int x = 0; x < w; x++) {
                                int src = (y * w + x) * 4;
                                uint32_t px = ((uint32_t)data[src]   << 24) |
                                              ((uint32_t)data[src+1] << 16) |
                                              ((uint32_t)data[src+2] <<  8) |
                                               (uint32_t)data[src+3];
                                uint32_t i  = (x & 7) | ((y & 7) << 8);
                                i = (i ^ (i << 2)) & 0x1313;
                                i = (i ^ (i << 1)) & 0x1515;
                                uint32_t tx = x >> 3, ty = y >> 3;
                                uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                                uint32_t local_idx  = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                                swizzled[tile_start + local_idx] = px;
                            }
                        }
                        C3D_TexUpload(tex, swizzled);
                        C3D_TexFlush(tex);
                        linearFree(swizzled);

                        Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
                        sub->width  = (u16)w;
                        sub->height = (u16)h;
                        sub->left   = 0.0f;
                        sub->top    = 1.0f;
                        sub->right  = (float)w / pw;
                        sub->bottom = 1.0f - ((float)h / ph);

                        shared.tex    = tex;
                        shared.subtex = sub;
                        shared.sheet  = nullptr;
                        loaded = true;
                    } else { delete tex; }
                } else { delete tex; }
                stbi_image_free(data);
            }
        }
    }
    
    if (loaded) {
        shared.refCount = 1;
        int handle = PlayState::instance->nextTextureHandle++;
        PlayState::instance->namedTextureRegistry[handle] = shared;
        lua_pushinteger(L, handle);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int LuaManager::lua_unloadTexture(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    int handle = luaL_checkinteger(L, 1);
    
    auto it = PlayState::instance->namedTextureRegistry.find(handle);
    if (it != PlayState::instance->namedTextureRegistry.end()) {
        SharedLuaTexture& shared = it->second;
        shared.refCount--;
        if (shared.refCount > 0) {
            return 0; // Still referenced elsewhere
        }
        
        for (auto& s : PlayState::instance->luaSprites) {
            if (s.sheet == shared.sheet && s.sheet != nullptr) {
                s.sheet = nullptr;
                s.img.tex = nullptr;
            } else if (s.img.tex == shared.tex && s.img.tex != nullptr) {
                s.img.tex = nullptr;
            }
        }
        
        if (shared.sheet) {
            if (shared.vramData) {
                if (addrIsVRAM(shared.vramData)) {
                    C3D_Tex* tex = C2D_SpriteSheetGetImage(shared.sheet, 0).tex;
                    if (tex) tex->data = nullptr;
                    vramFree(shared.vramData);
                } else {
                    linearFree(shared.vramData);
                }
                shared.vramData = nullptr;
            }
            if (!SpritesheetCache::get().contains(shared.sheet)) {
                C2D_SpriteSheetFree(shared.sheet);
            }
        } else if (shared.tex) {
            if (shared.vramData) {
                if (addrIsVRAM(shared.vramData)) {
                    shared.tex->data = nullptr;
                    vramFree(shared.vramData);
                } else {
                    linearFree(shared.vramData);
                }
                shared.vramData = nullptr;
            }
            C3D_TexDelete(shared.tex);
            delete shared.tex;
            if (shared.subtex) delete (Tex3DS_SubTexture*)shared.subtex;
        }
        
        PlayState::instance->namedTextureRegistry.erase(it);
    }
    return 0;
}

// Kicks off an async background load of a texture.
// loadTexture() will instantly consume it once the thread finishes.
// Usage: preloadTexture('mySprite', [parallel/priority = false])
int LuaManager::lua_preloadTexture(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string imgName = luaL_checkstring(L, 1);

    // Deduplication: if already in manual registry or preloaded, avoid queuing again
    for (auto& pair : PlayState::instance->namedTextureRegistry) {
        if (pair.second.originalPath == imgName) return 0;
    }
    if (AsyncAssetManager::get().isImageReady(imgName)) return 0;

    bool priority = (lua_gettop(L) >= 2) ? lua_toboolean(L, 2) : false;
    AsyncAssetManager::get().requestImageLoad(imgName, priority);
    return 0;
}

// Returns true if a previously preloaded texture is ready to be consumed via loadTexture().
// Also returns true if the handle is already in the manual texture registry.
// Usage:  if isTextureReady('mySprite') then ... end
//         if isTextureReady(handle)     then ... end   (check existing handle)
int LuaManager::lua_isTextureReady(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (lua_type(L, 1) == LUA_TNUMBER) {
        // Check if a handle is already loaded in the registry
        int handle = (int)lua_tointeger(L, 1);
        bool ready = handle != 0 &&
                     PlayState::instance->namedTextureRegistry.count(handle) > 0;
        lua_pushboolean(L, ready);
    } else {
        // Check if an async preload by name has finished
        std::string imgName = luaL_checkstring(L, 1);
        lua_pushboolean(L, AsyncAssetManager::get().isImageReady(imgName));
    }
    return 1;
}

int LuaManager::lua_removeLuaSprite(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string tag = luaL_checkstring(L, 1);
    
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        size_t removedIdx = it->second;
        StageSprite s = PlayState::instance->luaSprites[removedIdx];
        s.visible = false;
        s.alpha = 0.0f;

        // Erase from vector FIRST so releaseLuaSpriteMemory correctly checks remaining active sprites
        PlayState::instance->luaSpriteIndices.erase(it);
        PlayState::instance->luaSprites.erase(PlayState::instance->luaSprites.begin() + removedIdx);
        
        for (auto& pair : PlayState::instance->luaSpriteIndices) {
            if (pair.second > removedIdx) {
                pair.second--;
            }
        }

        // Free GPU VRAM/RAM resources if this was the last sprite using the texture
        releaseLuaSpriteMemory(s);
    }
    return 0;
}


int LuaManager::lua_getRandomInt(lua_State* L) {
    if (lua_gettop(L) < 2) {
        lua_pushinteger(L, 0);
        return 1;
    }
    int min = (int)luaL_checkinteger(L, 1);
    int max = (int)luaL_checkinteger(L, 2);
    if (min > max) std::swap(min, max);
    
    int val = min + (rand() % (max - min + 1));
    lua_pushinteger(L, val);
    return 1;
}

int LuaManager::lua_updateHitbox(lua_State* L) {
    // Stub to prevent crashes since hitboxes update automatically via scale in our engine
    return 0;
}

int LuaManager::lua_debugPrint(lua_State* L) {
    int argc = lua_gettop(L);
    if (argc < 1) return 0;

    std::string msg;
    if (lua_isstring(L, 1)) {
        msg = lua_tostring(L, 1);
    } else {
        lua_getglobal(L, "tostring");
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        const char* s = lua_tostring(L, -1);
        msg = s ? s : "(nil)";
        lua_pop(L, 1);
    }

    if (PlayState::instance) {
        PlayState::instance->addDebugMessage(msg);
    }

    printf("\x1b[2;1H[Lua] %s\x1b[K\n", msg.c_str());
    return 0;
}

int LuaManager::lua_screenCenter(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string axes = (lua_gettop(L) >= 2) ? luaL_checkstring(L, 2) : "xy";

    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        auto& s = PlayState::instance->luaSprites[it->second];
        float baseWidth = 400.0f;
        float baseHeight = 240.0f;
        
        if (s.camera == "camBottom" || s.camera == "bottom") {
            baseWidth = 320.0f;
            baseHeight = 240.0f;
        } else if (s.camera == "camGame" || s.camera == "game") {
            baseWidth = 1200.0f;
            baseHeight = 720.0f;
        }

        // Use scale in centering calculation
        float w = (s.isGraphic ? s.graphicWidth : (s.img.subtex ? s.img.subtex->width : 100.0f)) * s.scaleX;
        float h = (s.isGraphic ? s.graphicHeight : (s.img.subtex ? s.img.subtex->height : 100.0f)) * s.scaleY;

        if (axes.find('x') != std::string::npos) s.x = (baseWidth - w) / 2.0f;
        if (axes.find('y') != std::string::npos) s.y = (baseHeight - h) / 2.0f;
    } else {
        auto tit = PlayState::instance->luaTextIndices.find(tag);
        if (tit != PlayState::instance->luaTextIndices.end()) {
            auto& t = PlayState::instance->luaTexts[tit->second];
            if (t.dirty) {
                PlayState::instance->updateLuaText(t);
            }
            float tw = 0.0f, th = 0.0f;
            C2D_TextGetDimensions(&t.c2dObj, t.size, t.size, &tw, &th);

            float baseWidth = 400.0f;
            float baseHeight = 240.0f;
            if (t.camera == "camBottom" || t.camera == "bottom") {
                baseWidth = 320.0f;
            }

            if (axes.find('x') != std::string::npos) t.x = (baseWidth - tw) / 2.0f;
            if (axes.find('y') != std::string::npos) t.y = (baseHeight - th) / 2.0f;
        }
    }
    return 0;
}


int LuaManager::lua_cameraShake(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    std::string cam = luaL_checkstring(L, 1);
    float intensity = luaL_checknumber(L, 2);
    float duration = luaL_checknumber(L, 3);

    if (cam == "game" || cam == "camGame") {
        PlayState::instance->camShakeIntensity = intensity;
        PlayState::instance->camShakeTimer = duration;
    } else if (cam == "hud" || cam == "camHUD") {
        PlayState::instance->hudShakeIntensity = intensity;
        PlayState::instance->hudShakeTimer = duration;
    } else if (cam == "other" || cam == "camOther") {
        PlayState::instance->otherShakeIntensity = intensity;
        PlayState::instance->otherShakeTimer = duration;
    }
    return 0;
}

int LuaManager::lua_triggerEvent(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string name = luaL_checkstring(L, 1);
    std::string v1 = "";
    std::string v2 = "";
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        if (lua_isnumber(L, 2)) v1 = std::to_string(lua_tonumber(L, 2));
        else v1 = luaL_checkstring(L, 2);
    }
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        if (lua_isnumber(L, 3)) v2 = std::to_string(lua_tonumber(L, 3));
        else v2 = luaL_checkstring(L, 3);
    }

    printf("[LUA EVENT] triggerEvent('%s', '%s', '%s') called!\n", name.c_str(), v1.c_str(), v2.c_str());

    Event e;
    e.name = name;
    e.value1 = v1;
    e.value2 = v2;
    e.strumTime = 0.0f;

    PlayState::instance->triggerEvent(e);
    return 0;
}

int LuaManager::lua_getHealth(lua_State* L) {
    lua_pushnumber(L, PlayState::instance ? PlayState::instance->health : 0.0);
    return 1;
}
int LuaManager::lua_setHealth(lua_State* L) {
    if (PlayState::instance) {
        float h = (float)luaL_checknumber(L, 1);
        PlayState::instance->health = (h > 2.0f) ? 2.0f : (h < 0.0f ? 0.0f : h);
    }
    return 0;
}
int LuaManager::lua_addHealth(lua_State* L) {
    if (PlayState::instance) {
        float h = (float)luaL_checknumber(L, 1);
        PlayState::instance->health += h;
        if (PlayState::instance->health > 2.0f) PlayState::instance->health = 2.0f;
        if (PlayState::instance->health < 0.0f) PlayState::instance->health = 0.0f;
    }
    return 0;
}

int LuaManager::lua_getScore(lua_State* L) {
    lua_pushinteger(L, PlayState::instance ? PlayState::instance->score : 0);
    return 1;
}
int LuaManager::lua_setScore(lua_State* L) {
    if (PlayState::instance) PlayState::instance->score = (int)luaL_checkinteger(L, 1);
    return 0;
}
int LuaManager::lua_addScore(lua_State* L) {
    if (PlayState::instance) PlayState::instance->score += (int)luaL_checkinteger(L, 1);
    return 0;
}

int LuaManager::lua_getMisses(lua_State* L) {
    lua_pushinteger(L, PlayState::instance ? PlayState::instance->misses : 0);
    return 1;
}
int LuaManager::lua_setMisses(lua_State* L) {
    if (PlayState::instance) PlayState::instance->misses = (int)luaL_checkinteger(L, 1);
    return 0;
}
int LuaManager::lua_addMisses(lua_State* L) {
    if (PlayState::instance) PlayState::instance->misses += (int)luaL_checkinteger(L, 1);
    return 0;
}

int LuaManager::lua_getHits(lua_State* L) {
    lua_pushinteger(L, PlayState::instance ? PlayState::instance->hits : 0);
    return 1;
}
int LuaManager::lua_setHits(lua_State* L) {
    if (PlayState::instance) PlayState::instance->hits = (int)luaL_checkinteger(L, 1);
    return 0;
}
int LuaManager::lua_addHits(lua_State* L) {
    if (PlayState::instance) PlayState::instance->hits += (int)luaL_checkinteger(L, 1);
    return 0;
}

int LuaManager::lua_getSongPosition(lua_State* L) {
    lua_pushnumber(L, Conductor::songPosition);
    return 1;
}


int LuaManager::lua_restartSong(lua_State* L) {
    if (PlayState::instance) {
        MusicBeatState::switchState(new PlayState(PlayState::instance->curSong, PlayState::instance->currentDifficulty));
    }
    return 0;
}
int LuaManager::lua_exitSong(lua_State* L) {
    MusicBeatState::switchState(new MainMenuState());
    return 0;
}

int LuaManager::lua_characterDance(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string type = luaL_checkstring(L, 1);
    Character* c = nullptr;
    if (type == "bf" || type == "boyfriend") c = PlayState::instance->bf;
    else if (type == "dad" || type == "opponent") c = PlayState::instance->dad;
    else if (type == "gf" || type == "girlfriend") c = PlayState::instance->gf;

    if (c) c->dance(true);
    return 0;
}

int LuaManager::lua_setGridVisible(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    if (!ClientPrefs::drawGrid) return 0;
    bool vis = lua_toboolean(L, 1);
    PlayState::instance->showGrid = vis;
    return 0;
}

bool LuaManager::checkCustomTweenTarget(const std::string& tag, const std::string& prop, float& outStartValue) {
    if (!PlayState::instance) return false;
    if (tag == "healthbar" || tag == "healthBar" || tag == "healthbarBg" || tag == "healthBarBG" || tag == "healthbarBG") {
        bool isBG = (tag == "healthbarBg" || tag == "healthBarBG" || tag == "healthbarBG");
        if (prop == "x") outStartValue = isBG ? PlayState::instance->healthBarBGX : PlayState::instance->healthBarX;
        else if (prop == "y") outStartValue = isBG ? PlayState::instance->healthBarBGY : PlayState::instance->healthBarY;
        else if (prop == "scaleX" || prop == "scale.x") outStartValue = isBG ? PlayState::instance->healthBarBGScaleX : PlayState::instance->healthBarScaleX;
        else if (prop == "scaleY" || prop == "scale.y") outStartValue = isBG ? PlayState::instance->healthBarBGScaleY : PlayState::instance->healthBarScaleY;
        else if (prop == "alpha") outStartValue = isBG ? PlayState::instance->healthBarBGAlpha : PlayState::instance->healthBarAlpha;
        else if (prop == "angle") outStartValue = isBG ? PlayState::instance->healthBarBGAngle : PlayState::instance->healthBarAngle;
        else outStartValue = 0.0f;
        return true;
    }
    else if (tag == "timebar" || tag == "timeBar" || tag == "timebarBg" || tag == "timeBarBG" || tag == "timebarBG") {
        bool isBG = (tag == "timebarBg" || tag == "timeBarBG" || tag == "timebarBG");
        if (prop == "x") outStartValue = isBG ? PlayState::instance->timeBarBGX : PlayState::instance->timeBarX_val;
        else if (prop == "y") outStartValue = isBG ? PlayState::instance->timeBarBGY : PlayState::instance->timeBarY_val;
        else if (prop == "scaleX" || prop == "scale.x") outStartValue = isBG ? PlayState::instance->timeBarBGScaleX : PlayState::instance->timeBarScaleX;
        else if (prop == "scaleY" || prop == "scale.y") outStartValue = isBG ? PlayState::instance->timeBarBGScaleY : PlayState::instance->timeBarScaleY;
        else if (prop == "alpha") outStartValue = isBG ? PlayState::instance->timeBarBGAlpha : PlayState::instance->timeBarAlpha;
        else if (prop == "angle") outStartValue = isBG ? PlayState::instance->timeBarBGAngle : PlayState::instance->timeBarAngle;
        else outStartValue = 0.0f;
        return true;
    }
    else if (tag == "iconP1" || tag == "iconP2") {
        bool isP1 = (tag == "iconP1");
        if (prop == "x") outStartValue = isP1 ? PlayState::instance->iconP1X : PlayState::instance->iconP2X;
        else if (prop == "y") outStartValue = isP1 ? PlayState::instance->iconP1Y : PlayState::instance->iconP2Y;
        else if (prop == "scaleX" || prop == "scale.x") outStartValue = isP1 ? PlayState::instance->iconP1ScaleX : PlayState::instance->iconP2ScaleX;
        else if (prop == "scaleY" || prop == "scale.y") outStartValue = isP1 ? PlayState::instance->iconP1ScaleY : PlayState::instance->iconP2ScaleY;
        else if (prop == "alpha") outStartValue = isP1 ? PlayState::instance->iconP1Alpha : PlayState::instance->iconP2Alpha;
        else if (prop == "angle") outStartValue = isP1 ? PlayState::instance->iconP1Angle : PlayState::instance->iconP2Angle;
        else outStartValue = 0.0f;
        return true;
    }
    else if (tag == "scoretxt" || tag == "scoreTxt" || tag == "timetxt" || tag == "timeTxt") {
        bool isScore = (tag == "scoretxt" || tag == "scoreTxt");
        if (prop == "x") outStartValue = isScore ? PlayState::instance->scoreTxtX : PlayState::instance->timeTxtX;
        else if (prop == "y") outStartValue = isScore ? PlayState::instance->scoreTxtY : PlayState::instance->timeTxtY;
        else if (prop == "scaleX" || prop == "scale.x") outStartValue = isScore ? PlayState::instance->scoreTxtScaleX : PlayState::instance->timeTxtScaleX;
        else if (prop == "scaleY" || prop == "scale.y") outStartValue = isScore ? PlayState::instance->scoreTxtScaleY : PlayState::instance->timeTxtScaleY;
        else if (prop == "alpha") outStartValue = isScore ? PlayState::instance->scoreTxtAlpha : PlayState::instance->timeTxtAlpha;
        else if (prop == "angle") outStartValue = isScore ? PlayState::instance->scoreTxtAngle : PlayState::instance->timeTxtAngle;
        else outStartValue = 0.0f;
        return true;
    }
    else if (tag == "countdown") {
        if (prop == "x") outStartValue = PlayState::instance->countdownX;
        else if (prop == "y") outStartValue = PlayState::instance->countdownY;
        else if (prop == "scaleX" || prop == "scale.x") outStartValue = PlayState::instance->countdownScaleX;
        else if (prop == "scaleY" || prop == "scale.y") outStartValue = PlayState::instance->countdownScaleY;
        else if (prop == "alpha") outStartValue = PlayState::instance->countdownAlpha;
        else if (prop == "angle") outStartValue = PlayState::instance->countdownAngle;
        else outStartValue = 0.0f;
        return true;
    }
    else if (tag == "camGame" || tag == "game" || tag == "camHUD" || tag == "hud" || tag == "camOther" || tag == "other") {
        bool isGame = (tag == "camGame" || tag == "game");
        bool isHUD = (tag == "camHUD" || tag == "hud");
        if (prop == "x") outStartValue = isGame ? PlayState::instance->camX_offset : (isHUD ? PlayState::instance->hudX_offset : PlayState::instance->otherX_offset);
        else if (prop == "y") outStartValue = isGame ? PlayState::instance->camY_offset : (isHUD ? PlayState::instance->hudY_offset : PlayState::instance->otherY_offset);
        else if (prop == "zoom") outStartValue = isGame ? PlayState::instance->camZoom : (isHUD ? PlayState::instance->hudZoom : PlayState::instance->otherZoom);
        else if (prop == "scaleX" || prop == "scale.x") outStartValue = isGame ? PlayState::instance->camScaleX : (isHUD ? PlayState::instance->hudScaleX : PlayState::instance->otherScaleX);
        else if (prop == "scaleY" || prop == "scale.y") outStartValue = isGame ? PlayState::instance->camScaleY : (isHUD ? PlayState::instance->hudScaleY : PlayState::instance->otherScaleY);
        else if (prop == "angle") outStartValue = isGame ? PlayState::instance->camAngle : (isHUD ? PlayState::instance->hudAngle : PlayState::instance->otherAngle);
        else if (prop == "alpha") outStartValue = isGame ? PlayState::instance->camAlpha : (isHUD ? PlayState::instance->hudAlpha : PlayState::instance->otherAlpha);
        else outStartValue = 0.0f;
        return true;
    }
    return false;
}

int LuaManager::lua_doTweenX(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    
    std::string target = luaL_checkstring(L, 2);
    size_t dot = target.find('.');
    if (dot != std::string::npos) {
        t.targetTag = target.substr(0, dot);
        t.prop = target.substr(dot + 1);
    } else {
        t.targetTag = target;
        t.prop = "x";
    }
    
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;
    
    // Find current start value
    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            auto& s = PlayState::instance->luaSprites[it->second];
            if (t.prop == "scale" || t.prop == "scale.x") {
                t.startValue = s.scaleX;
            } else if (t.prop == "scale.y") {
                t.startValue = s.scaleY;
            } else {
                t.startValue = s.x;
            }
            PlayState::instance->activeTweens.push_back(t);
        } else if (PlayState::instance->luaTextIndices.count(t.targetTag)) {
            t.startValue = PlayState::instance->luaTexts[PlayState::instance->luaTextIndices[t.targetTag]].x;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "boyfriend" && PlayState::instance->bf) {
            t.startValue = PlayState::instance->bf->x;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "dad" && PlayState::instance->dad) {
            t.startValue = PlayState::instance->dad->x;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "gf" && PlayState::instance->gf) {
            t.startValue = PlayState::instance->gf->x;
            PlayState::instance->activeTweens.push_back(t);
        } else {
            PlayState::instance->addDebugMessage("Tween Error: Target '" + target + "' not found!");
        }
    }
    return 0;
}

int LuaManager::lua_doTweenY(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    
    std::string target = luaL_checkstring(L, 2);
    size_t dot = target.find('.');
    if (dot != std::string::npos) {
        t.targetTag = target.substr(0, dot);
        t.prop = target.substr(dot + 1);
    } else {
        t.targetTag = target;
        t.prop = "y";
    }
    
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;
    
    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            auto& s = PlayState::instance->luaSprites[it->second];
            if (t.prop == "scale" || t.prop == "scale.x") {
                t.startValue = s.scaleX;
            } else if (t.prop == "scale.y") {
                t.startValue = s.scaleY;
            } else {
                t.startValue = s.y;
            }
            PlayState::instance->activeTweens.push_back(t);
        } else if (PlayState::instance->luaTextIndices.count(t.targetTag)) {
            t.startValue = PlayState::instance->luaTexts[PlayState::instance->luaTextIndices[t.targetTag]].y;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "boyfriend" && PlayState::instance->bf) {
            t.startValue = PlayState::instance->bf->y;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "dad" && PlayState::instance->dad) {
            t.startValue = PlayState::instance->dad->y;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "gf" && PlayState::instance->gf) {
            t.startValue = PlayState::instance->gf->y;
            PlayState::instance->activeTweens.push_back(t);
        } else {
            PlayState::instance->addDebugMessage("Tween Error: Target '" + target + "' not found!");
        }
    }
    return 0;
}

int LuaManager::lua_doTweenAlpha(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "alpha";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;
    
    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            t.startValue = PlayState::instance->luaSprites[it->second].alpha;
            PlayState::instance->activeTweens.push_back(t);
        } else if (PlayState::instance->luaTextIndices.count(t.targetTag)) {
            t.startValue = PlayState::instance->luaTexts[PlayState::instance->luaTextIndices[t.targetTag]].alpha;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "boyfriend" && PlayState::instance->bf) {
            t.startValue = PlayState::instance->bf->alpha;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "dad" && PlayState::instance->dad) {
            t.startValue = PlayState::instance->dad->alpha;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "gf" && PlayState::instance->gf) {
            t.startValue = PlayState::instance->gf->alpha;
            PlayState::instance->activeTweens.push_back(t);
        } else {
            PlayState::instance->addDebugMessage("Tween Error: Target '" + t.targetTag + "' not found!");
        }
    }
    return 0;
}

int LuaManager::lua_doTweenZoom(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 4) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "zoom";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else if (t.targetTag == "camGame" || t.targetTag == "game") {
        t.targetTag = "camGame";
        t.startValue = PlayState::instance->camZoom;
        PlayState::instance->activeTweens.push_back(t);
    } else if (t.targetTag == "camHUD" || t.targetTag == "hud") {
        t.targetTag = "camHUD";
        t.startValue = PlayState::instance->hudZoom;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        PlayState::instance->addDebugMessage("Tween Error: Camera '" + t.targetTag + "' not found!");
    }
    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_doTweenAngle(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "angle";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            t.startValue = PlayState::instance->luaSprites[it->second].angle;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "boyfriend" && PlayState::instance->bf) {
            t.startValue = PlayState::instance->bf->angle;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "dad" && PlayState::instance->dad) {
            t.startValue = PlayState::instance->dad->angle;
            PlayState::instance->activeTweens.push_back(t);
        } else if (t.targetTag == "gf" && PlayState::instance->gf) {
            t.startValue = PlayState::instance->gf->angle;
            PlayState::instance->activeTweens.push_back(t);
        } else {
            PlayState::instance->addDebugMessage("Tween Error: Target '" + t.targetTag + "' not found!");
        }
    }
    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_doTweenColor(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "color";

    std::string colorStr = luaL_checkstring(L, 3);
    u32 colorVal = 0xFFFFFFFF;
    if (colorStr == "red") colorVal = 0xFFFF0000;
    else if (colorStr == "blue") colorVal = 0xFF0000FF;
    else if (colorStr == "green") colorVal = 0xFF00FF00;
    else if (colorStr == "white") colorVal = 0xFFFFFFFF;
    else if (colorStr == "black") colorVal = 0xFF000000;
    else if (colorStr == "yellow") colorVal = 0xFFFFFF00;
    else if (colorStr == "purple") colorVal = 0xFF800080;
    else if (colorStr == "cyan") colorVal = 0xFF00FFFF;
    else {
        std::string s = colorStr;
        if (s.find("0x") == 0 || s.find("0X") == 0) s = s.substr(2);
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        std::stringstream ss;
        ss << std::hex << s;
        ss >> colorVal;
    }

    t.endValue = (float)colorVal;
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            t.startValue = (float)PlayState::instance->luaSprites[it->second].graphicColor;
            PlayState::instance->activeTweens.push_back(t);
        }
    }
    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_doTweenScale(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 4) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "scale";
    float scaleX = (float)luaL_checknumber(L, 3);
    float scaleY = (float)luaL_checknumber(L, 4);
    t.endValue = scaleX;
    t.duration = (lua_gettop(L) >= 5) ? (float)luaL_checknumber(L, 5) : 1.0f;
    t.ease = (lua_gettop(L) >= 6) ? luaL_checkstring(L, 6) : "linear";
    t.timer = 0;

    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            auto& s = PlayState::instance->luaSprites[it->second];
            t.startValue = s.scale;
            s.scaleX = scaleX;
            s.scaleY = scaleY;
            PlayState::instance->activeTweens.push_back(t);
        }
    }
    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_doTweenScaleX(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "scale.x";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            t.startValue = PlayState::instance->luaSprites[it->second].scaleX;
            PlayState::instance->activeTweens.push_back(t);
        }
    }
    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_doTweenScaleY(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    t.targetTag = luaL_checkstring(L, 2);
    t.prop = "scale.y";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    float customStartVal = 0.0f;
    if (checkCustomTweenTarget(t.targetTag, t.prop, customStartVal)) {
        t.startValue = customStartVal;
        PlayState::instance->activeTweens.push_back(t);
    } else {
        auto it = PlayState::instance->luaSpriteIndices.find(t.targetTag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            t.startValue = PlayState::instance->luaSprites[it->second].scaleY;
            PlayState::instance->activeTweens.push_back(t);
        }
    }
    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_noteTweenX(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    t.targetTag = "noteStrum_" + std::to_string(note);
    t.prop = "noteX";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    int lane = note % 4;
    bool isPlayer = (note >= 4);
    t.startValue = PlayState::instance->getLaneX(lane, isPlayer);
    PlayState::instance->activeTweens.push_back(t);

    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_noteTweenY(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    t.targetTag = "noteStrum_" + std::to_string(note);
    t.prop = "noteY";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    int lane = note % 4;
    bool isPlayer = (note >= 4);
    t.startValue = PlayState::instance->getLaneY(lane, isPlayer);
    PlayState::instance->activeTweens.push_back(t);

    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_noteTweenAngle(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    t.targetTag = "noteStrum_" + std::to_string(note);
    t.prop = "noteAngle";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    int lane = note % 4;
    bool isPlayer = (note >= 4);
    t.startValue = PlayState::instance->getLaneAngle(lane, isPlayer);
    PlayState::instance->activeTweens.push_back(t);

    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_noteTweenAlpha(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    t.targetTag = "noteStrum_" + std::to_string(note);
    t.prop = "noteAlpha";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    int lane = note % 4;
    bool isPlayer = (note >= 4);
    t.startValue = PlayState::instance->getLaneAlpha(lane, isPlayer);
    PlayState::instance->activeTweens.push_back(t);

    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_noteTweenDirection(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    LuaTween t;
    t.tag = luaL_checkstring(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    t.targetTag = "noteStrum_" + std::to_string(note);
    t.prop = "noteDirection";
    t.endValue = (float)luaL_checknumber(L, 3);
    t.duration = (float)luaL_checknumber(L, 4);
    t.ease = (lua_gettop(L) >= 5) ? luaL_checkstring(L, 5) : "linear";
    t.timer = 0;

    int lane = note % 4;
    bool isPlayer = (note >= 4);
    t.startValue = PlayState::instance->getLaneDirection(lane, isPlayer);
    PlayState::instance->activeTweens.push_back(t);

    lua_pushstring(L, t.tag.c_str());
    return 1;
}

int LuaManager::lua_cancelTween(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string tag = luaL_checkstring(L, 1);
    
    for (auto it = PlayState::instance->activeTweens.begin(); it != PlayState::instance->activeTweens.end();) {
        if (it->tag == tag) {
            it = PlayState::instance->activeTweens.erase(it);
        } else {
            ++it;
        }
    }
    return 0;
}

int LuaManager::lua_runTimer(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 2) return 0;
    std::string tag = luaL_checkstring(L, 1);
    float time = (float)luaL_checknumber(L, 2);
    int loops = (lua_gettop(L) >= 3) ? (int)luaL_checkinteger(L, 3) : 1;
    
    for (auto it = PlayState::instance->activeTimers.begin(); it != PlayState::instance->activeTimers.end();) {
        if (it->tag == tag) {
            it = PlayState::instance->activeTimers.erase(it);
        } else {
            it++;
        }
    }
    
    LuaTimer t;
    t.tag = tag;
    t.duration = time;
    t.loops = loops;
    t.loopsLeft = loops;
    PlayState::instance->activeTimers.push_back(t);
    return 0;
}

int LuaManager::lua_cancelTimer(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) return 0;
    std::string tag = luaL_checkstring(L, 1);
    for (auto it = PlayState::instance->activeTimers.begin(); it != PlayState::instance->activeTimers.end();) {
        if (it->tag == tag) {
            it = PlayState::instance->activeTimers.erase(it);
        } else {
            it++;
        }
    }
    return 0;
}

int LuaManager::lua_setObjectOrder(lua_State* L) {
    // Stub implementation to prevent errors
    return 0;
}

int LuaManager::lua_getObjectOrder(lua_State* L) {
    // Stub implementation returning a default rendering order of 10 to prevent errors
    lua_pushinteger(L, 10);
    return 1;
}

int LuaManager::lua_playSound(lua_State* L) {
    if (lua_gettop(L) < 1) return 0;
    std::string path = luaL_checkstring(L, 1);
    float vol = (lua_gettop(L) >= 2) ? (float)luaL_checknumber(L, 2) : 1.0f;
    AudioEngine::playSound(path, vol);
    return 0;
}

int LuaManager::lua_getColorFromHex(lua_State* L) {
    const char* hexStr = luaL_checkstring(L, 1);
    if (!hexStr) { lua_pushinteger(L, 0xFFFFFFFF); return 1; }

    u32 color = HexParser::parseStringToC2D(hexStr, 0xFFFFFFFF);
    lua_pushinteger(L, (lua_Integer)color);
    return 1;
}

int LuaManager::lua_startVideo(lua_State* L) {
    if (lua_gettop(L) < 1) return 0;
    std::string videoName = luaL_checkstring(L, 1);
    bool inFrontOfHUD = true;
    if (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) {
        inFrontOfHUD = lua_toboolean(L, 2);
    }
    bool loop = false;
    if (lua_gettop(L) >= 3 && lua_isboolean(L, 3)) {
        loop = lua_toboolean(L, 3);
    }
    if (PlayState::instance) {
        PlayState::instance->startVideo(videoName, inFrontOfHUD, loop);
    }
    lua_pushboolean(L, true);
    return 1;
}

int LuaManager::lua_stopVideo(lua_State* L) {
    if (PlayState::instance) {
        PlayState::instance->stopVideo();
    }
    return 0;
}

int LuaManager::lua_pauseVideo(lua_State* L) {
    if (PlayState::instance && PlayState::instance->inGameVideo) {
        PlayState::instance->inGameVideo->isPaused = true;
    }
    return 0;
}

int LuaManager::lua_resumeVideo(lua_State* L) {
    if (PlayState::instance && PlayState::instance->inGameVideo) {
        PlayState::instance->inGameVideo->isPaused = false;
    }
    return 0;
}

static u32 getKeys(const std::string& key) {
    std::string k = key;
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);
    
    // Core Actions / Face Buttons
    if (k == "accept" || k == "confirm" || k == "a") return KEY_A | KEY_START;
    if (k == "back" || k == "cancel" || k == "b") return KEY_B;
    if (k == "x") return KEY_X;
    if (k == "y") return KEY_Y;

    // Triggers & Bumpers
    if (k == "l" || k == "l1") return KEY_L;
    if (k == "r" || k == "r1") return KEY_R;
    if (k == "zl" || k == "l2") return KEY_ZL;
    if (k == "zr" || k == "r2") return KEY_ZR;

    // System & Debug Buttons
    if (k == "start") return KEY_START;
    if (k == "select") return KEY_SELECT;
    if (k == "pause") return KEY_START | KEY_SELECT;
    if (k == "reset") return KEY_X | KEY_Y;
    if (k == "debug_1") return KEY_L | KEY_ZL;
    if (k == "debug_2") return KEY_R | KEY_ZR;

    // Screen Touch
    if (k == "touch") return KEY_TOUCH;

    // Simple Directional (Fallback to DPAD + CPAD + CSTICK for compatibility)
    if (k == "left") return KEY_DLEFT | KEY_CPAD_LEFT | KEY_CSTICK_LEFT | KEY_Y;
    if (k == "down") return KEY_DDOWN | KEY_CPAD_DOWN | KEY_CSTICK_DOWN | KEY_B;
    if (k == "up") return KEY_DUP | KEY_CPAD_UP | KEY_CSTICK_UP | KEY_X;
    if (k == "right") return KEY_DRIGHT | KEY_CPAD_RIGHT | KEY_CSTICK_RIGHT | KEY_A;

    // UI Navigation
    if (k == "ui_left") return KEY_DLEFT | KEY_CPAD_LEFT | KEY_CSTICK_LEFT;
    if (k == "ui_down") return KEY_DDOWN | KEY_CPAD_DOWN | KEY_CSTICK_DOWN;
    if (k == "ui_up") return KEY_DUP | KEY_CPAD_UP | KEY_CSTICK_UP;
    if (k == "ui_right") return KEY_DRIGHT | KEY_CPAD_RIGHT | KEY_CSTICK_RIGHT;

    // DPAD Only
    if (k == "dpad-up" || k == "dpad_up") return KEY_DUP;
    if (k == "dpad-down" || k == "dpad_down") return KEY_DDOWN;
    if (k == "dpad-left" || k == "dpad_left") return KEY_DLEFT;
    if (k == "dpad-right" || k == "dpad_right") return KEY_DRIGHT;

    // Circle Pad Only
    if (k == "cpad-up" || k == "circlepad-up" || k == "cpad_up") return KEY_CPAD_UP;
    if (k == "cpad-down" || k == "circlepad-down" || k == "cpad_down") return KEY_CPAD_DOWN;
    if (k == "cpad-left" || k == "circlepad-left" || k == "cpad_left") return KEY_CPAD_LEFT;
    if (k == "cpad-right" || k == "circlepad-right" || k == "cpad_right") return KEY_CPAD_RIGHT;

    // C-Stick / Right Stick / RCirclePad Only
    if (k == "cstick-up" || k == "rcirclepad-up" || k == "rightstick-up" || k == "cstick_up") return KEY_CSTICK_UP;
    if (k == "cstick-down" || k == "rcirclepad-down" || k == "rightstick-down" || k == "cstick_down") return KEY_CSTICK_DOWN;
    if (k == "cstick-left" || k == "rcirclepad-left" || k == "rightstick-left" || k == "cstick_left") return KEY_CSTICK_LEFT;
    if (k == "cstick-right" || k == "rcirclepad-right" || k == "rightstick-right" || k == "cstick_right") return KEY_CSTICK_RIGHT;

    return 0;
}

int LuaManager::lua_keyJustPressed(lua_State* L) {
    std::string key = luaL_checkstring(L, 1);
    lua_pushboolean(L, hidKeysDown() & getKeys(key));
    return 1;
}

int LuaManager::lua_keyPressed(lua_State* L) {
    std::string key = luaL_checkstring(L, 1);
    lua_pushboolean(L, hidKeysHeld() & getKeys(key));
    return 1;
}

int LuaManager::lua_keyReleased(lua_State* L) {
    std::string key = luaL_checkstring(L, 1);
    lua_pushboolean(L, hidKeysUp() & getKeys(key));
    return 1;
}

int LuaManager::lua_setFpsLimit(lua_State* L) {
    int fps = (int)luaL_checkinteger(L, 1);
    if (fps < 15) fps = 15;
    if (fps > 60) fps = 60;
    ClientPrefs::fpsLimit = fps;
    return 0;
}


int LuaManager::lua_makeLuaText(lua_State* L) {
    if (!PlayState::instance) return 0;
    LuaText t;
    t.tag = luaL_checkstring(L, 1);
    t.text = luaL_checkstring(L, 2);
    t.width = (float)luaL_optnumber(L, 3, 0);
    t.x = (float)luaL_optnumber(L, 4, 0);
    t.y = (float)luaL_optnumber(L, 5, 0);
    t.camera = "camHUD"; // Psych Engine default
    t.active = false;
    t.dirty = true;
    
    // Cleanup if existing
    auto it = PlayState::instance->luaTextIndices.find(t.tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        LuaText& old = PlayState::instance->luaTexts[it->second];
        if (old.buf) C2D_TextBufDelete(old.buf);
        PlayState::instance->luaTexts[it->second] = t;
    } else {
        PlayState::instance->luaTextIndices[t.tag] = PlayState::instance->luaTexts.size();
        PlayState::instance->luaTexts.push_back(t);
    }
    return 0;
}

int LuaManager::lua_setTextString(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string text = luaL_checkstring(L, 2);
    
    auto it = PlayState::instance->luaTextIndices.find(tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        LuaText& t = PlayState::instance->luaTexts[it->second];
        if (t.text != text) {
            t.text = text;
            t.dirty = true;
        }
    }
    return 0;
}

int LuaManager::lua_addLuaText(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    bool front = lua_toboolean(L, 2);
    
    auto it = PlayState::instance->luaTextIndices.find(tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        LuaText& t = PlayState::instance->luaTexts[it->second];
        t.active = true;
        t.front = front;
    }
    return 0;
}

int LuaManager::lua_luaTextExists(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) {
        lua_pushboolean(L, false);
        return 1;
    }
    std::string tag = luaL_checkstring(L, 1);
    bool exists = PlayState::instance->luaTextIndices.count(tag) > 0;
    lua_pushboolean(L, exists);
    return 1;
}

int LuaManager::lua_luaSpriteExists(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) {
        lua_pushboolean(L, false);
        return 1;
    }
    std::string tag = luaL_checkstring(L, 1);
    bool exists = PlayState::instance->luaSpriteIndices.count(tag) > 0;
    lua_pushboolean(L, exists);
    return 1;
}

int LuaManager::lua_setTextSize(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    float size = (float)luaL_checknumber(L, 2);
    
    auto it = PlayState::instance->luaTextIndices.find(tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        // SnakeEngine uses 32 as base size for 1.0f scale
        PlayState::instance->luaTexts[it->second].size = size / 32.0f;
    }
    return 0;
}

int LuaManager::lua_setTextColor(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    
    u32 color = getLuaColorOrHex(L, 2, 0xFFFFFFFF);

    auto it = PlayState::instance->luaTextIndices.find(tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        PlayState::instance->luaTexts[it->second].color = color;
    }
    return 0;
}

int LuaManager::lua_setTextAlignment(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 2) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string align = luaL_checkstring(L, 2);

    auto it = PlayState::instance->luaTextIndices.find(tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        PlayState::instance->luaTexts[it->second].alignment = align;
    }
    return 0;
}

int LuaManager::lua_setTextBorder(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 3) return 0;
    std::string tag = luaL_checkstring(L, 1);
    float size = (float)luaL_checknumber(L, 2);
    
    u32 color = getLuaColorOrHex(L, 3, 0xFF000000);

    auto it = PlayState::instance->luaTextIndices.find(tag);
    if (it != PlayState::instance->luaTextIndices.end()) {
        PlayState::instance->luaTexts[it->second].borderSize = size;
        PlayState::instance->luaTexts[it->second].borderColor = color;
    }
    return 0;
}

int LuaManager::lua_playAnimFES(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string tag = luaL_checkstring(L, 1);
    std::string path = luaL_checkstring(L, 2);
    std::string anim = luaL_checkstring(L, 3);
    int fps = luaL_optinteger(L, 4, 24);
    bool loop = lua_toboolean(L, 5);
    float ox = (float)luaL_optnumber(L, 6, 0);
    float oy = (float)luaL_optnumber(L, 7, 0);

    // Check Characters
    if (tag == "boyfriend" || tag == "bf") {
        if (PlayState::instance->bf) PlayState::instance->bf->playAnimFES(path, anim, fps, loop, ox, oy);
    } else if (tag == "dad") {
        if (PlayState::instance->dad) PlayState::instance->dad->playAnimFES(path, anim, fps, loop, ox, oy);
    } else if (tag == "gf") {
        if (PlayState::instance->gf) PlayState::instance->gf->playAnimFES(path, anim, fps, loop, ox, oy);
    } else {
        // Check Lua Sprites
        auto it = PlayState::instance->luaSpriteIndices.find(tag);
        if (it != PlayState::instance->luaSpriteIndices.end()) {
            PlayState::instance->luaSprites[it->second].playAnimFES(path, anim, fps, loop, ox, oy);
        }
    }
    return 0;
}

// Position functions
int LuaManager::lua_getCharacterX(lua_State* L) {
    if (!PlayState::instance) { lua_pushnumber(L, 0); return 1; }
    std::string name = luaL_checkstring(L, 1);
    Character* c = nullptr;
    if (name == "boyfriend" || name == "bf") c = PlayState::instance->bf;
    else if (name == "dad" || name == "opponent") c = PlayState::instance->dad;
    else if (name == "gf" || name == "girlfriend") c = PlayState::instance->gf;
    lua_pushnumber(L, c ? c->x : 0.0f);
    return 1;
}

int LuaManager::lua_getCharacterY(lua_State* L) {
    if (!PlayState::instance) { lua_pushnumber(L, 0); return 1; }
    std::string name = luaL_checkstring(L, 1);
    Character* c = nullptr;
    if (name == "boyfriend" || name == "bf") c = PlayState::instance->bf;
    else if (name == "dad" || name == "opponent") c = PlayState::instance->dad;
    else if (name == "gf" || name == "girlfriend") c = PlayState::instance->gf;
    lua_pushnumber(L, c ? c->y : 0.0f);
    return 1;
}

int LuaManager::lua_setCharacterX(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    Character* c = nullptr;
    if (name == "boyfriend" || name == "bf") c = PlayState::instance->bf;
    else if (name == "dad" || name == "opponent") c = PlayState::instance->dad;
    else if (name == "gf" || name == "girlfriend") c = PlayState::instance->gf;
    if (c) c->x = val;
    return 0;
}

int LuaManager::lua_setCharacterY(lua_State* L) {
    if (!PlayState::instance) return 0;
    std::string name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    Character* c = nullptr;
    if (name == "boyfriend" || name == "bf") c = PlayState::instance->bf;
    else if (name == "dad" || name == "opponent") c = PlayState::instance->dad;
    else if (name == "gf" || name == "girlfriend") c = PlayState::instance->gf;
    if (c) c->y = val;
    return 0;
}

int LuaManager::lua_getMidpointX(lua_State* L) {
    if (!PlayState::instance) { lua_pushnumber(L, 0); return 1; }
    std::string tag = luaL_checkstring(L, 1);
    Character* c = nullptr;
    if (tag == "boyfriend" || tag == "bf") c = PlayState::instance->bf;
    else if (tag == "dad" || tag == "opponent") c = PlayState::instance->dad;
    else if (tag == "gf" || tag == "girlfriend") c = PlayState::instance->gf;
    
    if (c) {
        lua_pushnumber(L, c->x + 150.0f);
        return 1;
    }
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        StageSprite& s = PlayState::instance->luaSprites[it->second];
        float w = 0.0f;
        if (s.sheet) w = C2D_SpriteSheetGetImage(s.sheet, 0).subtex->width * s.scaleX;
        else if (s.img.subtex) w = s.img.subtex->width * s.scaleX;
        else if (s.isGraphic) w = s.graphicWidth * s.scaleX;
        lua_pushnumber(L, s.x + (w / 2.0f));
        return 1;
    }
    lua_pushnumber(L, 0);
    return 1;
}

int LuaManager::lua_getMidpointY(lua_State* L) {
    if (!PlayState::instance) { lua_pushnumber(L, 0); return 1; }
    std::string tag = luaL_checkstring(L, 1);
    Character* c = nullptr;
    if (tag == "boyfriend" || tag == "bf") c = PlayState::instance->bf;
    else if (tag == "dad" || tag == "opponent") c = PlayState::instance->dad;
    else if (tag == "gf" || tag == "girlfriend") c = PlayState::instance->gf;
    
    if (c) {
        lua_pushnumber(L, c->y + 150.0f);
        return 1;
    }
    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it != PlayState::instance->luaSpriteIndices.end()) {
        StageSprite& s = PlayState::instance->luaSprites[it->second];
        float h = 0.0f;
        if (s.sheet) h = C2D_SpriteSheetGetImage(s.sheet, 0).subtex->height * s.scaleY;
        else if (s.img.subtex) h = s.img.subtex->height * s.scaleY;
        else if (s.isGraphic) h = s.graphicHeight * s.scaleY;
        lua_pushnumber(L, s.y + (h / 2.0f));
        return 1;
    }
    lua_pushnumber(L, 0);
    return 1;
}

// String & Utility functions
int LuaManager::lua_getRandomBool(lua_State* L) {
    float chance = 50.0f;
    if (lua_gettop(L) >= 1) chance = (float)luaL_checknumber(L, 1);
    float r = (float)rand() / (float)RAND_MAX * 100.0f;
    lua_pushboolean(L, r < chance);
    return 1;
}

int LuaManager::lua_getRandomFloat(lua_State* L) {
    float min = 0.0f;
    float max = 1.0f;
    if (lua_gettop(L) >= 1) min = (float)luaL_checknumber(L, 1);
    if (lua_gettop(L) >= 2) max = (float)luaL_checknumber(L, 2);
    float r = (float)rand() / (float)RAND_MAX;
    lua_pushnumber(L, min + r * (max - min));
    return 1;
}

int LuaManager::lua_stringStartsWith(lua_State* L) {
    std::string str = luaL_checkstring(L, 1);
    std::string prefix = luaL_checkstring(L, 2);
    lua_pushboolean(L, str.rfind(prefix, 0) == 0);
    return 1;
}

int LuaManager::lua_stringEndsWith(lua_State* L) {
    std::string str = luaL_checkstring(L, 1);
    std::string suffix = luaL_checkstring(L, 2);
    bool ends = false;
    if (str.length() >= suffix.length()) {
        ends = (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
    }
    lua_pushboolean(L, ends);
    return 1;
}

int LuaManager::lua_stringTrim(lua_State* L) {
    std::string str = luaL_checkstring(L, 1);
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
    lua_pushstring(L, str.c_str());
    return 1;
}

int LuaManager::lua_stringSplit(lua_State* L) {
    std::string str = luaL_checkstring(L, 1);
    std::string delim = luaL_checkstring(L, 2);
    lua_newtable(L);
    int index = 1;
    size_t pos = 0;
    std::string token;
    while ((pos = str.find(delim)) != std::string::npos) {
        token = str.substr(0, pos);
        lua_pushstring(L, token.c_str());
        lua_rawseti(L, -2, index++);
        str.erase(0, pos + delim.length());
    }
    lua_pushstring(L, str.c_str());
    lua_rawseti(L, -2, index);
    return 1;
}

// Audio functions
int LuaManager::lua_precacheSound(lua_State* L) {
    return 0;
}

int LuaManager::lua_precacheMusic(lua_State* L) {
    return 0;
}

int LuaManager::lua_stopSound(lua_State* L) {
    return 0;
}

int LuaManager::lua_pauseSound(lua_State* L) {
    return 0;
}

int LuaManager::lua_resumeSound(lua_State* L) {
    return 0;
}

int LuaManager::lua_getSoundVolume(lua_State* L) {
    lua_pushnumber(L, 1.0f);
    return 1;
}

int LuaManager::lua_setSoundVolume(lua_State* L) {
    return 0;
}

// ============================================================
// 3DS Hardware Control Functions
// ============================================================

// setScreenState(topOn, bottomOn)
// Powers the backlight of each screen on or off independently.
// Example: setScreenState(true, false) — top on, bottom off.
int LuaManager::lua_setScreenState(lua_State* L) {
    bool topOn    = lua_toboolean(L, 1) != 0;
    bool bottomOn = lua_toboolean(L, 2) != 0;

    if (R_FAILED(gspLcdInit())) return 0;

    if (topOn)
        GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_TOP);
    else
        GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_TOP);

    if (bottomOn)
        GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM);
    else
        GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);

    gspLcdExit();
    return 0;
}

// setHomeAllowed(allowed)
// Blocks or restores the Home button. Useful during cutscenes.
// setHomeAllowed(false) — Home button does nothing.
// setHomeAllowed(true)  — restores default behaviour.
int LuaManager::lua_setHomeAllowed(lua_State* L) {
    bool allowed = lua_toboolean(L, 1) != 0;
    aptSetHomeAllowed(allowed);
    return 0;
}

// setLedColor(r, g, b [, flashSpeed [, smoothing]])
// Controls the 3DS notification LED.
//   r, g, b     — colour components 0-255
//   flashSpeed  — blink speed: 0 = static (solid), 1-255 (higher = faster blink)
//                 Internally maps to InfoLedPattern delay (1/16 s per unit).
//   smoothing   — 0 = hard blink, 1-255 = fade/pulse between values (higher = smoother)
// Examples:
//   setLedColor(0, 0, 0)           — LED off
//   setLedColor(255, 0, 0)         — solid red
//   setLedColor(0, 255, 0, 8, 16)  — green, slow pulsing glow
//   setLedColor(255, 0, 255, 2, 0) — magenta, fast hard blink
int LuaManager::lua_setLedColor(lua_State* L) {
    int r          = (int)luaL_optnumber(L, 1, 0);
    int g          = (int)luaL_optnumber(L, 2, 0);
    int b          = (int)luaL_optnumber(L, 3, 0);
    int flashSpeed = (int)luaL_optnumber(L, 4, 0); // 0 = static
    int smoothing  = (int)luaL_optnumber(L, 5, 0);

    // Clamp to byte range
    r          = r < 0 ? 0 : r > 255 ? 255 : r;
    g          = g < 0 ? 0 : g > 255 ? 255 : g;
    b          = b < 0 ? 0 : b > 255 ? 255 : b;
    flashSpeed = flashSpeed < 0 ? 0 : flashSpeed > 255 ? 255 : flashSpeed;
    smoothing  = smoothing  < 0 ? 0 : smoothing  > 255 ? 255 : smoothing;

    if (R_FAILED(mcuHwcInit())) return 0;

    InfoLedPattern pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.smoothing = (u8)smoothing;

    if (flashSpeed == 0) {
        // Static solid color — fill all 32 steps with the same value
        pattern.delay = 0;
        pattern.loopDelay = 0;
        memset(pattern.redPattern,   (u8)r, 32);
        memset(pattern.greenPattern, (u8)g, 32);
        memset(pattern.bluePattern,  (u8)b, 32);
    } else {
        // Blinking: first half of steps = full color, second half = off
        pattern.delay     = (u8)flashSpeed;
        pattern.loopDelay = 0; // loop forever
        for (int i = 0; i < 16; i++) {
            pattern.redPattern[i]   = (u8)r;
            pattern.greenPattern[i] = (u8)g;
            pattern.bluePattern[i]  = (u8)b;
        }
        // steps 16-31 remain 0 (off half of the blink cycle)
    }

    MCUHWC_SetInfoLedPattern(&pattern);
    mcuHwcExit();
    return 0;
}

// setLedFlash(r, g, b)
// Camera-flash effect: LED turns on instantly at full brightness then fades
// to black over ~512 ms. The pattern plays exactly once (no looping).
// No timer needed — the hardware turns the LED off automatically.
// Ideal for note hit feedback.
// Example: setLedFlash(255, 80, 180)  -- pink flash
int LuaManager::lua_setLedFlash(lua_State* L) {
    int r = (int)luaL_optnumber(L, 1, 255);
    int g = (int)luaL_optnumber(L, 2, 255);
    int b = (int)luaL_optnumber(L, 3, 255);
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;

    if (R_FAILED(mcuHwcInit())) return 0;

    InfoLedPattern pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.delay     = 1;    // 1/16 s per step = ~16 ms, total 32 steps = ~512 ms
    pattern.smoothing = 32;   // smooth interpolation between steps
    pattern.loopDelay = 0xFF; // 0xFF = play pattern exactly once, then LED turns off

    // Steps 0-3: full brightness (the instant "flash" punch)
    for (int i = 0; i < 4; i++) {
        pattern.redPattern[i]   = (u8)r;
        pattern.greenPattern[i] = (u8)g;
        pattern.bluePattern[i]  = (u8)b;
    }
    // Steps 4-31: linear decay from full brightness down to 0
    for (int i = 4; i < 32; i++) {
        float t = (float)(i - 4) / 27.0f; // 0.0 at step 4, 1.0 at step 31
        u8 rv = (u8)((1.0f - t) * r);
        u8 gv = (u8)((1.0f - t) * g);
        u8 bv = (u8)((1.0f - t) * b);
        pattern.redPattern[i]   = rv;
        pattern.greenPattern[i] = gv;
        pattern.bluePattern[i]  = bv;
    }

    MCUHWC_SetInfoLedPattern(&pattern);
    mcuHwcExit();
    return 0;
}

// crashGame()
// Immediately crashes the game and generates a Luma3DS crash dump.
// Use this to test crash dump collection or for dramatic effect in mods.
int LuaManager::lua_crashGame(lua_State* L) {
    (void)L;
    svcBreak(USERBREAK_PANIC);
    return 0; // never reached
}

// ============================================================
// Shader System Functions
// ============================================================


// setCameraShader(camera, shaderName [, param1, param2, param3])
// Appends shader to camera stack, or updates params if name already exists.
int LuaManager::lua_setCameraShader(lua_State* L) {
    std::string camera = luaL_checkstring(L, 1);
    std::string shader = luaL_checkstring(L, 2);
    float v1 = luaL_optnumber(L, 3, 1.0f);
    float v2 = luaL_optnumber(L, 4, 0.0f);
    float v3 = luaL_optnumber(L, 5, 0.0f);
    
    ShaderManager::get().setCameraShader(camera, shader, v1, v2, v3);
    return 0;
}

// removeCameraShader(camera [, shader1, shader2, ...])
// If only camera is given: removes ALL shaders from that camera.
// If shader names are given: removes only those specific shaders from the stack.
int LuaManager::lua_removeCameraShader(lua_State* L) {
    std::string camera = luaL_checkstring(L, 1);
    int nargs = lua_gettop(L);
    if (nargs <= 1) {
        // No shader names specified: remove ALL shaders for this camera
        ShaderManager::get().removeCameraShader(camera);
    } else {
        // Remove only the named shaders
        std::vector<std::string> names;
        for (int i = 2; i <= nargs; i++) {
            names.push_back(luaL_checkstring(L, i));
        }
        ShaderManager::get().removeCameraShaders(camera, names);
    }
    return 0;
}

// setShaderFloat(camera, paramIndex, value)
// Modifies the FIRST shader in the camera's stack (backward compatible).
// paramIndex = 1, 2, or 3
int LuaManager::lua_setShaderFloat(lua_State* L) {
    std::string camera = luaL_checkstring(L, 1);
    int index = luaL_checkinteger(L, 2);
    float value = luaL_checknumber(L, 3);
    ShaderManager::get().setShaderFloat(camera, index, value);
    return 0;
}

// setShaderParam(camera, shaderName, paramIndex, value)
// Modifies the parameter of a SPECIFIC named shader in the camera's stack.
// paramIndex = 1, 2, or 3
int LuaManager::lua_setShaderParam(lua_State* L) {
    std::string camera = luaL_checkstring(L, 1);
    std::string shaderName = luaL_checkstring(L, 2);
    int index = luaL_checkinteger(L, 3);
    float value = luaL_checknumber(L, 4);
    ShaderManager::get().setShaderParam(camera, shaderName, index, value);
    return 0;
}

// -----------------------------------------------------------------------------
// Mouse & Key Input Functions (3DS Touch Adaptation with Persistent Last Position)
// -----------------------------------------------------------------------------

static float s_lastTouchX = -9999.0f;
static float s_lastTouchY = -9999.0f;

static void updateLastTouchPosition() {
    u32 kHeld = hidKeysHeld();
    u32 kDown = hidKeysDown();
    if ((kHeld & KEY_TOUCH) || (kDown & KEY_TOUCH)) {
        touchPosition t;
        hidTouchRead(&t);
        if (t.px > 0 || t.py > 0) {
            s_lastTouchX = (float)t.px;
            s_lastTouchY = (float)t.py;
        }
    }
}

static bool isPointInsideSprite(const StageSprite& s, float rawTouchX, float rawTouchY, const std::string& camOverride) {
    if (!s.visible || s.alpha <= 0.0f) return false;

    std::string cam = camOverride.empty() ? s.camera : camOverride;
    std::transform(cam.begin(), cam.end(), cam.begin(), ::tolower);

    float w = 0.0f;
    float h = 0.0f;
    if (s.isGraphic) {
        w = s.graphicWidth;
        h = s.graphicHeight;
    } else if (s.animated && s.currentAnim && !s.currentAnim->indices.empty() && !s.frames.empty()) {
        int frameIdx = (int)s.curFrame;
        if (frameIdx >= 0 && frameIdx < (int)s.currentAnim->indices.size()) {
            int idx = s.currentAnim->indices[frameIdx];
            if (idx >= 0 && idx < (int)s.frames.size()) {
                w = (float)s.frames[idx].w;
                h = (float)s.frames[idx].h;
            }
        }
    }
    if (w <= 0.0f && s.img.subtex) w = (float)s.img.subtex->width;
    if (h <= 0.0f && s.img.subtex) h = (float)s.img.subtex->height;
    if (w <= 0.0f && s.img.tex) w = (float)s.img.tex->width;
    if (h <= 0.0f && s.img.tex) h = (float)s.img.tex->height;
    if (w <= 0.0f) w = 50.0f;
    if (h <= 0.0f) h = 50.0f;

    float scaledW = w * std::abs(s.scale * s.scaleX);
    float scaledH = h * std::abs(s.scale * s.scaleY);

    float touchX = rawTouchX;
    float touchY = rawTouchY;

    float screenCenterX = 0.0f;
    float screenCenterY = 0.0f;

    if (cam == "hud" || cam == "other" || cam == "camhud" || cam == "camother" || cam == "bottom" || cam == "cambottom" || cam == "touch" || cam == "sub") {
        screenCenterX = s.x + scaledW / 2.0f;
        screenCenterY = s.y + scaledH / 2.0f;
    } else {
        float camX = PlayState::instance ? PlayState::instance->camX : 0.0f;
        float camY = PlayState::instance ? PlayState::instance->camY : 0.0f;
        float camZoom = PlayState::instance ? PlayState::instance->camZoom : 1.0f;

        float sprWorldCenterX = s.x + scaledW / 2.0f;
        float sprWorldCenterY = s.y + scaledH / 2.0f;

        screenCenterX = (sprWorldCenterX - camX * s.scrollX) * camZoom + 160.0f;
        screenCenterY = (sprWorldCenterY - camY * s.scrollY) * camZoom + 120.0f;

        scaledW *= camZoom;
        scaledH *= camZoom;
    }

    float dx = touchX - screenCenterX;
    float dy = touchY - screenCenterY;

    if (s.angle != 0.0f) {
        float rad = -s.angle * (3.14159265358979323846f / 180.0f);
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float rotX = dx * cosA - dy * sinA;
        float rotY = dx * sinA + dy * cosA;
        dx = rotX;
        dy = rotY;
    }

    return (std::abs(dx) <= (scaledW / 2.0f)) && (std::abs(dy) <= (scaledH / 2.0f));
}

int LuaManager::lua_mouseClicked(lua_State* L) {
    updateLastTouchPosition();
    u32 kDown = hidKeysDown();
    bool clicked = (kDown & KEY_TOUCH) != 0;
    lua_pushboolean(L, clicked);
    return 1;
}

int LuaManager::lua_mousePressed(lua_State* L) {
    updateLastTouchPosition();
    u32 kHeld = hidKeysHeld();
    bool pressed = (kHeld & KEY_TOUCH) != 0;
    lua_pushboolean(L, pressed);
    return 1;
}

int LuaManager::lua_mouseReleased(lua_State* L) {
    updateLastTouchPosition();
    u32 kUp = hidKeysUp();
    bool released = (kUp & KEY_TOUCH) != 0;
    lua_pushboolean(L, released);
    return 1;
}

int LuaManager::lua_getMouseX(lua_State* L) {
    updateLastTouchPosition();
    std::string cam = (lua_gettop(L) >= 1 && lua_isstring(L, 1)) ? lua_tostring(L, 1) : "game";
    std::transform(cam.begin(), cam.end(), cam.begin(), ::tolower);

    float mouseX = s_lastTouchX;
    if (cam == "game" || cam == "camgame") {
        float camX = PlayState::instance ? PlayState::instance->camX : 0.0f;
        float camZoom = PlayState::instance ? PlayState::instance->camZoom : 1.0f;
        mouseX = (mouseX - 160.0f) / (camZoom > 0.0f ? camZoom : 1.0f) + camX;
    }
    lua_pushnumber(L, mouseX);
    return 1;
}

int LuaManager::lua_getMouseY(lua_State* L) {
    updateLastTouchPosition();
    std::string cam = (lua_gettop(L) >= 1 && lua_isstring(L, 1)) ? lua_tostring(L, 1) : "game";
    std::transform(cam.begin(), cam.end(), cam.begin(), ::tolower);

    float mouseY = s_lastTouchY;
    if (cam == "game" || cam == "camgame") {
        float camY = PlayState::instance ? PlayState::instance->camY : 0.0f;
        float camZoom = PlayState::instance ? PlayState::instance->camZoom : 1.0f;
        mouseY = (mouseY - 120.0f) / (camZoom > 0.0f ? camZoom : 1.0f) + camY;
    }
    lua_pushnumber(L, mouseY);
    return 1;
}



int LuaManager::lua_mouseClickedOnSprite(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) {
        lua_pushboolean(L, false);
        return 1;
    }
    std::string tag = luaL_checkstring(L, 1);

    updateLastTouchPosition();

    u32 kDown = hidKeysDown();
    if (!(kDown & KEY_TOUCH)) {
        lua_pushboolean(L, false);
        return 1;
    }

    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it == PlayState::instance->luaSpriteIndices.end()) {
        lua_pushboolean(L, false);
        return 1;
    }

    const auto& s = PlayState::instance->luaSprites[it->second];
    bool hit = isPointInsideSprite(s, s_lastTouchX, s_lastTouchY, s.camera);
    lua_pushboolean(L, hit);
    return 1;
}

int LuaManager::lua_mouseOverlapSprite(lua_State* L) {
    if (!PlayState::instance || lua_gettop(L) < 1) {
        lua_pushboolean(L, false);
        return 1;
    }
    std::string tag = luaL_checkstring(L, 1);

    updateLastTouchPosition();

    auto it = PlayState::instance->luaSpriteIndices.find(tag);
    if (it == PlayState::instance->luaSpriteIndices.end()) {
        lua_pushboolean(L, false);
        return 1;
    }

    const auto& s = PlayState::instance->luaSprites[it->second];
    bool hit = isPointInsideSprite(s, s_lastTouchX, s_lastTouchY, s.camera);
    lua_pushboolean(L, hit);
    return 1;
}

// ─── OPTION MANAGER LUA API ───────────────────────────────────────────────────

int LuaManager::lua_getOption(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushnil(L);
        return 1;
    }
    std::string id = luaL_checkstring(L, 1);

    if (lua_isboolean(L, 2)) {
        bool def = lua_toboolean(L, 2);
        lua_pushboolean(L, OptionManager::get().getBool(id, def));
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        float def = (float)lua_tonumber(L, 2);
        lua_pushnumber(L, OptionManager::get().getFloat(id, def));
    } else if (lua_isstring(L, 2)) {
        std::string def = lua_tostring(L, 2);
        lua_pushstring(L, OptionManager::get().getString(id, def).c_str());
    } else {
        std::string sVal = OptionManager::get().getString(id, "");
        if (!sVal.empty()) {
            lua_pushstring(L, sVal.c_str());
        } else {
            bool bVal = OptionManager::get().getBool(id, false);
            lua_pushboolean(L, bVal);
        }
    }
    return 1;
}

int LuaManager::lua_setOption(lua_State* L) {
    if (lua_gettop(L) < 2) return 0;
    std::string id = luaL_checkstring(L, 1);
    if (lua_isboolean(L, 2)) {
        OptionManager::get().setBool(id, lua_toboolean(L, 2));
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        OptionManager::get().setFloat(id, (float)lua_tonumber(L, 2));
    } else if (lua_isstring(L, 2)) {
        OptionManager::get().setString(id, lua_tostring(L, 2));
    }
    OptionManager::get().syncToClientPrefs();
    return 0;
}

int LuaManager::lua_getModOption(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushnil(L);
        return 1;
    }
    std::string id = luaL_checkstring(L, 1);
    std::string modFolder = ModHandler::currentModFolder;
    if (lua_gettop(L) >= 3 && lua_isstring(L, 3)) {
        modFolder = lua_tostring(L, 3);
    }

    if (lua_isboolean(L, 2)) {
        bool def = lua_toboolean(L, 2);
        lua_pushboolean(L, OptionManager::get().getModBool(modFolder, id, def));
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        float def = (float)lua_tonumber(L, 2);
        lua_pushnumber(L, OptionManager::get().getModFloat(modFolder, id, def));
    } else if (lua_isstring(L, 2)) {
        std::string def = lua_tostring(L, 2);
        lua_pushstring(L, OptionManager::get().getModString(modFolder, id, def).c_str());
    } else {
        std::string sVal = OptionManager::get().getModString(modFolder, id, "");
        if (!sVal.empty()) {
            lua_pushstring(L, sVal.c_str());
        } else {
            bool bVal = OptionManager::get().getModBool(modFolder, id, false);
            lua_pushboolean(L, bVal);
        }
    }
    return 1;
}

int LuaManager::lua_setModOption(lua_State* L) {
    if (lua_gettop(L) < 2) return 0;
    std::string id = luaL_checkstring(L, 1);
    std::string modFolder = ModHandler::currentModFolder;
    if (lua_gettop(L) >= 3 && lua_isstring(L, 3)) {
        modFolder = lua_tostring(L, 3);
    }

    if (lua_isboolean(L, 2)) {
        OptionManager::get().setModBool(modFolder, id, lua_toboolean(L, 2));
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        OptionManager::get().setModFloat(modFolder, id, (float)lua_tonumber(L, 2));
    } else if (lua_isstring(L, 2)) {
        OptionManager::get().setModString(modFolder, id, lua_tostring(L, 2));
    }
    return 0;
}
