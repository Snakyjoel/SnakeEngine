#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdio.h>
#include <vector>
#include <malloc.h>
#include "backend/CustomFadeTransition.hpp"
#include "backend/MusicBeatState.hpp"
#include "backend/AudioEngine.hpp"
#include "Highscores.hpp"
#include "backend/AsyncAssetManager.hpp"
#include "states/TitleState.hpp"
#include "states/PlayState.hpp"

C2D_Font globalVCRFont = nullptr;

// Force nearest-neighbor filtering on all glyph sheets so the font looks pixel-perfect
void makeFontPixelPerfect(C2D_Font font) {
    if (!font) return;
    struct My_C2D_Font_s {
        CFNT_s*         cfnt;
        C3D_Tex*        glyphSheets;
        float           textScale;
    };
    auto* f = (struct My_C2D_Font_s*)font;
    if (!f || !f->cfnt || !f->glyphSheets) return;
    int numSheets = f->cfnt->finf.tglp->nSheets;
    for (int i = 0; i < numSheets; i++) {
        C3D_Tex* tex = &f->glyphSheets[i];
        tex->param = GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | 
                     GPU_TEXTURE_MIN_FILTER(GPU_NEAREST) | 
                     GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_BORDER) | 
                     GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_BORDER);
    }
}

static C2D_TextBuf menuDebugBuf = nullptr;
bool g_inTransition = false;

extern "C" {
    u32 __ctru_heap_size = 12 * 1024 * 1024;        // 12MB heap
    u32 __ctru_linear_heap_size = 48 * 1024 * 1024; // 48MB linear (textures/audio)
}



bool g_keepMusicPlayingDuringSleep = false;
static aptHookCookie s_aptHookCookie;
static bool s_wasAudioPausedByOS = false;
static bool s_wasMusicPausedByOS = false;
static bool s_wasAsyncSuspendedByOS = false;

static void aptHookFunc(APT_HookType hook, void* param) {
    if (hook == APTHOOK_ONSUSPEND || hook == APTHOOK_ONEXIT) {
        s_wasAudioPausedByOS = !AudioEngine::paused;
        s_wasMusicPausedByOS = MusicPlayer::isPlaying();
        s_wasAsyncSuspendedByOS = !AsyncAssetManager::get().isSuspended();

        if (s_wasAudioPausedByOS && !g_keepMusicPlayingDuringSleep) AudioEngine::pause();
        if (s_wasMusicPausedByOS && !g_keepMusicPlayingDuringSleep) MusicPlayer::pause();
        if (s_wasAsyncSuspendedByOS) AsyncAssetManager::get().suspend();
    } else if (hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP) {
        if (s_wasAudioPausedByOS && !g_keepMusicPlayingDuringSleep) AudioEngine::resume();
        if (s_wasMusicPausedByOS && !g_keepMusicPlayingDuringSleep) MusicPlayer::resume();
        if (s_wasAsyncSuspendedByOS) AsyncAssetManager::get().resume();
    }
}

int main(int argc, char* argv[]) {
    osSetSpeedupEnable(true); // N3DS 804MHz
    
    if (R_FAILED(romfsInit())) {
        gfxInitDefault();
        consoleInit(GFX_BOTTOM, NULL);
        printf("\x1b[10;10HERROR: RomFS not mounted!\x1b[0K");
        while (aptMainLoop()) { gspWaitForVBlank(); hidScanInput(); if (hidKeysDown() & KEY_START) break; }
        gfxExit();
        return 0;
    }
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    Result ndspRes = ndspInit();
    if (R_FAILED(ndspRes)) {
        C2D_Fini();
        C3D_Fini();
        consoleInit(GFX_BOTTOM, NULL);
        printf("\x1b[8;10HERROR: ndspInit failed (0x%08lX)\x1b[0K", ndspRes);
        printf("\x1b[10;10HDSP Firmware is likely missing.\x1b[0K");
        printf("\x1b[11;10HDump it using DSP1 or GodMode9.\x1b[0K");
        printf("\x1b[13;10HYou need sdmc:/3ds/dspfirm.cdc\x1b[0K");
        printf("\x1b[15;10HPress START to exit.\x1b[0K");
        while (aptMainLoop()) { 
            gspWaitForVBlank(); 
            hidScanInput(); 
            if (hidKeysDown() & KEY_START) break; 
        }
        gfxExit();
        romfsExit();
        return 0;
    }

    aptHook(&s_aptHookCookie, aptHookFunc, nullptr);

    globalVCRFont = C2D_FontLoad("romfs:/fonts/vcr.bcfnt");
    makeFontPixelPerfect(globalVCRFont);
    C3D_RenderTarget* top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA); //Fucking Long Line

    ClientPrefs::loadSettings();
    Highscores::load();

    AsyncAssetManager::get().init();

    MusicBeatState* currentState = new TitleState();
    currentState->init();

    u64 lastTime = osGetTime();

    while (aptMainLoop()) {
        u64 frameStart = osGetTime();
        g_inTransition = (MusicBeatState::transPhase != TransitionPhase::NONE);
        hidScanInput();

        u32 kDown = hidKeysDown();
        (void)kDown;

        u64 currentTime = osGetTime();
        float dt = (float)(currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        if (MusicBeatState::transPhase == TransitionPhase::FADE_OUT) {
            MusicBeatState::transTimer   += dt;
            float duration = MusicBeatState::useStickerTransition ? 0.80f : MusicBeatState::TRANS_DURATION;
            MusicBeatState::transProgress = MusicBeatState::transTimer / duration;

            if (MusicBeatState::useStickerTransition) {
                MusicBeatState::updateStickerTransition(dt);
            }

            if (MusicBeatState::transProgress >= 1.0f) {
                MusicBeatState::transProgress = 1.0f;

                if (currentState) {
                    currentState->exitState();
                    delete currentState;
                    currentState = nullptr;
                }

                if (MusicBeatState::nextState != nullptr) {
                    currentState = MusicBeatState::nextState;
                    MusicBeatState::nextState = nullptr;
                    currentState->init();

                    lastTime = osGetTime();

                    if (MusicBeatState::skipTransition) {
                        MusicBeatState::transPhase = TransitionPhase::NONE;
                        MusicBeatState::skipTransition = false;
                        if (MusicBeatState::useStickerTransition) {
                            MusicBeatState::cleanupStickerTransition();
                            MusicBeatState::useStickerTransition = false;
                        }
                    } else {
                        MusicBeatState::transPhase    = TransitionPhase::FADE_IN;
                        MusicBeatState::transTimer    = 0.0f;
                        MusicBeatState::transProgress = 0.0f;
                    }
                } else {
                    MusicBeatState::transPhase = TransitionPhase::NONE;
                    if (MusicBeatState::useStickerTransition) {
                        MusicBeatState::cleanupStickerTransition();
                        MusicBeatState::useStickerTransition = false;
                    }
                }
            }
        } else if (MusicBeatState::transPhase == TransitionPhase::FADE_IN) {
            MusicBeatState::transTimer   += dt;
            float duration = MusicBeatState::useStickerTransition ? 0.80f : MusicBeatState::TRANS_DURATION;
            MusicBeatState::transProgress = MusicBeatState::transTimer / duration;

            if (MusicBeatState::useStickerTransition) {
                MusicBeatState::updateStickerTransition(dt);
            }

            if (MusicBeatState::transProgress >= 1.0f) {
                MusicBeatState::transProgress = 1.0f;
                MusicBeatState::transPhase    = TransitionPhase::NONE;
                if (MusicBeatState::useStickerTransition) {
                    MusicBeatState::cleanupStickerTransition();
                    MusicBeatState::useStickerTransition = false;
                }
            }
        }

        if (currentState) {
            // Skip first frame update to avoid dt spike after a state load
            if (MusicBeatState::transPhase != TransitionPhase::FADE_IN ||
                MusicBeatState::transTimer > 0.001f) {
                currentState->update(dt);
            }
            MusicPlayer::update();

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            currentState->draw(top, bottom);

            // Global debug overlay (L+R+SELECT outside of PlayState)
            u32 keys_held = hidKeysHeld();
            if (PlayState::instance == nullptr && ClientPrefs::debugInfo && (keys_held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT)) {
                if (!menuDebugBuf) {
                    menuDebugBuf = C2D_TextBufNew(256);
                } else {
                    C2D_TextBufClear(menuDebugBuf);
                }

                extern u32 __ctru_linear_heap_size;
                float lramTotal = (float)__ctru_linear_heap_size / (1024.0f * 1024.0f);
                float lramUsed = lramTotal - ((float)linearSpaceFree() / (1024.0f * 1024.0f));
                float vramUsed = (float)(6 * 1024 * 1024 - vramSpaceFree()) / (1024.0f * 1024.0f);
                float vramTotal = 6.0f;

                struct mallinfo mi = mallinfo();
                float stdRamUsed = (float)mi.uordblks / (1024.0f * 1024.0f);

                bool isNew3DS = false;
                APT_CheckNew3DS(&isNew3DS);

                char debugStr[256];
                sprintf(debugStr, "SYSTEM RAM: %.2f MB\nLINEAR RAM: %.2f / %.2f MB\nVRAM: %.2f / %.2f MB\nMODEL: %s", 
                        stdRamUsed, lramUsed, lramTotal, vramUsed, vramTotal, isNew3DS ? "New 3DS" : "Old 3DS");

                C2D_Text debugObj;
                C2D_TextFontParse(&debugObj, globalVCRFont, menuDebugBuf, debugStr);
                C2D_TextOptimize(&debugObj);

                C2D_SceneBegin(bottom);
                float textW = 0, textH = 0;
                C2D_TextGetDimensions(&debugObj, 0.5f, 0.5f, &textW, &textH);
                
                C2D_DrawRectSolid(5.0f, 5.0f, 0.98f, textW + 10.0f, textH + 10.0f, C2D_Color32(0, 0, 0, 180));
                C2D_DrawText(&debugObj, C2D_WithColor, 10.0f, 10.0f, 0.99f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
            }

            if (MusicBeatState::transPhase != TransitionPhase::NONE && !MusicBeatState::skipTransition) {
                if (MusicBeatState::useStickerTransition) {
                    MusicBeatState::drawStickerTransition(top, bottom);
                } else {
                    drawWipeOverlay(top,    400.0f, 240.0f);
                    drawWipeOverlay(bottom, 320.0f, 240.0f);
                }
            }

            C3D_FrameEnd(0);
        }

        if (ClientPrefs::fpsLimit < 60) {
            float targetFrameTime = 1.0f / (float)ClientPrefs::fpsLimit;
            u64 frameTimeMs = osGetTime() - frameStart;
            float frameTimeSecs = (float)frameTimeMs / 1000.0f;
            if (frameTimeSecs < targetFrameTime) {
                float sleepTime = targetFrameTime - frameTimeSecs;
                svcSleepThread((s64)(sleepTime * 1000000000.0f));
            }
        }
    }


    aptUnhook(&s_aptHookCookie);

    if (currentState) {
        currentState->exitState();
        delete currentState;
        currentState = nullptr;
    }

    AsyncAssetManager::get().shutdown();

    AudioEngine::exit();
    MusicPlayer::stop();

    if (globalVCRFont) {
        C2D_FontFree(globalVCRFont);
        globalVCRFont = nullptr;
    }

    if (menuDebugBuf) {
        C2D_TextBufDelete(menuDebugBuf);
        menuDebugBuf = nullptr;
    }

    C2D_Fini();
    C3D_Fini();
    ndspExit();
    gfxExit();
    romfsExit();
    return 0;
}
