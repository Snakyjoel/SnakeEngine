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
#include "Achievements.hpp"
#include "backend/AsyncAssetManager.hpp"
#include "backend/UpdateChecker.hpp"
#include "states/TitleState.hpp"
#include "states/egg/EggDialogueState.hpp"
#include "states/PlayState.hpp"

C2D_Font globalVCRFont = nullptr;
C2D_Font globalPixelFont = nullptr;

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





bool g_keepMusicPlayingDuringSleep = false;
static aptHookCookie s_aptHookCookie;
static bool s_wasAudioPausedByOS = false;
static bool s_wasMusicPausedByOS = false;
static bool s_wasAsyncSuspendedByOS = false;

static void aptHookFunc(APT_HookType hook, void* param) {
    if (hook == APTHOOK_ONSUSPEND) {
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
    gfxInitDefault();

    bool isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);
    if (isNew3DS) {
        osSetSpeedupEnable(true); // N3DS 804MHz
        APT_SetAppCpuTimeLimit(80); // Enable up to 80% CPU usage on Core 1 (SysCore) for background threads
    }
    
    Result ndspRes = ndspInit();
    if (R_SUCCEEDED(ndspRes)) {
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspSetMasterVol(1.0f);
    }

    if (R_FAILED(romfsInit())) {
        consoleInit(GFX_BOTTOM, NULL);
        printf("\x1b[10;10HERROR: RomFS not mounted!\x1b[0K");
        while (aptMainLoop()) { gspWaitForVBlank(); hidScanInput(); if (hidKeysDown() & KEY_START) break; }
        if (R_SUCCEEDED(ndspRes)) ndspExit();
        gfxExit();
        return 0;
    }

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

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
    AudioEngine::startAudioThread();

    globalVCRFont = C2D_FontLoad("romfs:/fonts/vcr.bcfnt");
    makeFontPixelPerfect(globalVCRFont);

    globalPixelFont = C2D_FontLoad("romfs:/fonts/pixel.bcfnt");
    makeFontPixelPerfect(globalPixelFont);
    gfxSet3D(true);
    C3D_RenderTarget* top      = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget* bottom   = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA); //Fucking Long Line

    ClientPrefs::loadSettings();
    Highscores::load();
    Achievements::loadAchievements();

    AsyncAssetManager::get().init();

    MusicBeatState* currentState = nullptr;
    if (ClientPrefs::acceptedEgg) {
        MusicBeatState::skipTransition = true;
        currentState = new EggDialogueState();
    } else {
        currentState = new TitleState();
    }
    MusicBeatState::currentState = currentState;
    currentState->init();

    u64 lastTime = osGetTime();

    while (aptMainLoop() && !MusicBeatState::requestExit) {
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

                // Pre-fill audio buffers to prevent stutter during initial state load
                MusicPlayer::update();

                // Render and present 100% closed transition frame BEFORE blocking in init()
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                g_isRightEye = false;
                if (currentState) currentState->draw(top, bottom);
                if (g_current3DSlider > 0.001f) {
                    g_isRightEye = true;
                    if (currentState) currentState->draw(topRight, bottom);
                }
                g_isRightEye = false;

                if (!MusicBeatState::skipTransition) {
                    if (MusicBeatState::useStickerTransition) {
                        MusicBeatState::drawStickerTransition(top, bottom);
                        if (g_current3DSlider > 0.001f) {
                            MusicBeatState::drawStickerTransition(topRight, nullptr);
                        }
                    } else {
                        drawWipeOverlay(top, 400.0f, 240.0f);
                        if (g_current3DSlider > 0.001f) {
                            drawWipeOverlay(topRight, 400.0f, 240.0f);
                        }
                        drawWipeOverlay(bottom, 320.0f, 240.0f);
                    }
                }
                C3D_FrameEnd(0);

                if (currentState) {
                    currentState->exitState();
                    delete currentState;
                    currentState = nullptr;
                    MusicBeatState::currentState = nullptr;
                }

                if (MusicBeatState::nextState != nullptr) {
                    currentState = MusicBeatState::nextState;
                    MusicBeatState::currentState = currentState;
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

            bool want3D = ClientPrefs::enable3DEffect && (osGet3DSliderState() > 0.001f);
            gfxSet3D(want3D);
            g_current3DSlider = want3D ? osGet3DSliderState() : 0.0f;
            
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            g_isRightEye = false;
            currentState->draw(top, bottom);

            if (g_current3DSlider > 0.001f) {
                g_isRightEye = true;
                currentState->draw(topRight, bottom);
            }
            g_isRightEye = false;

            // Global debug overlay (L+R+SELECT outside of PlayState)
            u32 keys_held = hidKeysHeld();
            if (PlayState::instance == nullptr && ClientPrefs::debugInfo && (keys_held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT)) {
                if (!menuDebugBuf) {
                    menuDebugBuf = C2D_TextBufNew(256);
                } else {
                    C2D_TextBufClear(menuDebugBuf);
                }

                extern u32 __ctru_heap_size;
                extern u32 __ctru_linear_heap_size;
                float heapTotal    = (float)__ctru_heap_size / (1024.0f * 1024.0f);
                float linearTotal  = (float)__ctru_linear_heap_size / (1024.0f * 1024.0f);
                struct mallinfo mi = mallinfo();
                float heapUsed     = (float)mi.uordblks / (1024.0f * 1024.0f);
                float linearFree   = (float)linearSpaceFree() / (1024.0f * 1024.0f);
                float linearUsed   = linearTotal - linearFree;
                float totalUsed    = heapUsed + linearUsed;
                float totalAvail   = heapTotal + linearTotal;
                float vramUsed = (float)(6 * 1024 * 1024 - vramSpaceFree()) / (1024.0f * 1024.0f);
                float cpuPercent = (C3D_GetProcessingTime() * 6.0f);
                float gpuPercent = (C3D_GetDrawingTime() * 6.0f);
                if (cpuPercent > 100.0f) cpuPercent = 100.0f;
                if (cpuPercent < 0.0f)   cpuPercent = 0.0f;
                if (gpuPercent > 100.0f) gpuPercent = 100.0f;
                if (gpuPercent < 0.0f)   gpuPercent = 0.0f;

                bool isNew3DS = false;
                APT_CheckNew3DS(&isNew3DS);

                int menuFps = (int)(1.0f / (dt > 0.0001f ? dt : 0.0166f));
                if (menuFps > 60) menuFps = 60;

                char debugStr[256];
                sprintf(debugStr, "FPS:   %d\nRAM:   %.1f / %.1f MB\nVRAM:  %.1f / 6.0 MB\nCPU:   %d%%\nGPU:   %d%%\nMODEL: %s", 
                        menuFps, totalUsed, totalAvail, vramUsed, (int)cpuPercent, (int)gpuPercent, isNew3DS ? "New 3DS" : "Old 3DS");

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
                    if (g_current3DSlider > 0.001f) {
                        MusicBeatState::drawStickerTransition(topRight, nullptr);
                    }
                } else {
                    drawWipeOverlay(top,    400.0f, 240.0f);
                    if (g_current3DSlider > 0.001f) {
                        drawWipeOverlay(topRight, 400.0f, 240.0f);
                    }
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

    UpdateChecker::shutdown();

    if (currentState) {
        currentState->exitState();
        delete currentState;
        currentState = nullptr;
    }

    AsyncAssetManager::get().shutdown();

    AudioEngine::shutdown();

    if (globalVCRFont) {
        C2D_FontFree(globalVCRFont);
        globalVCRFont = nullptr;
    }

    if (globalPixelFont) {
        C2D_FontFree(globalPixelFont);
        globalPixelFont = nullptr;
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
