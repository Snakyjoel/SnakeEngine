#include "MusicBeatState.hpp"
#include "AudioEngine.hpp"
#include "SparrowParser.hpp"
#include "Macros.hpp"
#include <cstdlib>

struct TransitionSticker {
    int frameIdx;
    float x, y;
    float angle;
    float targetScale;
    float currentScale;
    float timing;
    float popDuration;
    float popTimer;
    bool visible;
};

static C2D_SpriteSheet stickerSheet = nullptr;
static std::vector<Frame> stickerFrames;
static std::vector<TransitionSticker> topStickers;
static std::vector<TransitionSticker> bottomStickers;
static bool stickerDegenInit = false;

static const std::string stickerSounds[] = {
    "romfs:/preload/sounds/stickers/keyClick1.adp",
    "romfs:/preload/sounds/stickers/keyClick2.adp",
    "romfs:/preload/sounds/stickers/keyClick3.adp",
    "romfs:/preload/sounds/stickers/keyClick4.adp",
    "romfs:/preload/sounds/stickers/keyClick5.adp",
    "romfs:/preload/sounds/stickers/keyClick7.adp",
    "romfs:/preload/sounds/stickers/keyClick8.adp",
    "romfs:/preload/sounds/stickers/keyClick9.adp"
};

MusicBeatState* MusicBeatState::currentState = nullptr;
MusicBeatState* MusicBeatState::nextState    = nullptr;
TransitionPhase  MusicBeatState::transPhase  = TransitionPhase::NONE;
float            MusicBeatState::transProgress = 0.0f;
float            MusicBeatState::transTimer  = 0.0f;
bool             MusicBeatState::skipTransition = false;
bool             MusicBeatState::requestExit = false;
bool             MusicBeatState::useStickerTransition = false;
bool             MusicBeatState::useFadeTransition = false;

void MusicBeatState::switchState(MusicBeatState* newState) {
    if (transPhase == TransitionPhase::FADE_OUT) {
        delete nextState;
        nextState = newState;
        return;
    }

    if ((newState && !newState->hasTransitions()) || (currentState && !currentState->hasTransitions())) {
        skipTransition = true;
    }

    nextState    = newState;
    if (skipTransition) {
        transPhase   = TransitionPhase::FADE_OUT;
        transProgress = 1.0f;
        transTimer   = TRANS_DURATION;
    } else {
        transPhase   = TransitionPhase::FADE_OUT;
        transProgress = 0.0f;
        transTimer   = 0.0f;
        if (useStickerTransition) {
            initStickerTransition();
        }
    }
}

void MusicBeatState::initStickerTransition() {
    if (stickerSheet != nullptr) return;

    srand(osGetTime());

    std::string packName = (rand() % 2 == 0) ? "iconPack1" : "iconPack2";
    std::string sheetPath = "romfs:/preload/images/iconpacks/" + packName + ".t3x";
    std::string xmlPath = "romfs:/preload/images/iconpacks/" + packName + ".xml";

    stickerSheet = C2D_SpriteSheetLoad(sheetPath.c_str());
    if (stickerSheet) {
        C2D_Image img = C2D_SpriteSheetGetImage(stickerSheet, 0);
        if (img.tex) {
            SetTextureAntialiasing(img.tex);
        }
        SparrowParser::parseXml(xmlPath, stickerFrames);

        if (img.tex) {
            float rw = img.subtex->right - img.subtex->left;
            float rh = img.subtex->bottom - img.subtex->top;
            for (auto& f : stickerFrames) {
                f.tex = img.tex;
                f.uv.width = (u16)f.w;
                f.uv.height = (u16)f.h;
                f.uv.left = img.subtex->left + ((float)f.x * rw / (float)img.subtex->width);
                f.uv.top = img.subtex->top + ((float)f.y * rh / (float)img.subtex->height);
                f.uv.right = img.subtex->left + ((float)(f.x + f.w) * rw / (float)img.subtex->width);
                f.uv.bottom = img.subtex->top + ((float)(f.y + f.h) * rh / (float)img.subtex->height);
            }
        }
    }

    if (stickerFrames.empty()) return;

    topStickers.clear();
    bottomStickers.clear();
    stickerDegenInit = false;

    // Grid for Top Screen (400x240)
    float xPos = -60.0f;
    float yPos = -60.0f;
    while (xPos <= 400.0f) {
        int frameIdx = rand() % (int)stickerFrames.size();
        const auto& f = stickerFrames[frameIdx];
        float logicalW = f.rotated ? (float)f.h : (float)f.w;
        float logicalH = f.rotated ? (float)f.w : (float)f.h;

        TransitionSticker s;
        s.frameIdx = frameIdx;
        s.x = xPos + logicalW * 0.5f;
        s.y = yPos + logicalH * 0.5f;

        int deg = -60 + (rand() % 131);
        s.angle = (float)deg * (3.141592653589793f / 180.0f);
        s.targetScale = 0.97f + ((rand() % 6) / 100.0f);
        s.currentScale = 1.05f;
        s.popDuration = (float)(rand() % 3) * (1.0f / 24.0f);
        s.popTimer = s.popDuration;
        s.visible = false;
        topStickers.push_back(s);

        xPos += logicalW * 0.5f;
        if (xPos >= 400.0f) {
            if (yPos <= 240.0f) {
                xPos = -60.0f;
                yPos += 28.0f + (rand() % 22);
            }
        }
    }

    // Grid for Bottom Screen (320x240)
    xPos = -60.0f;
    yPos = -60.0f;
    while (xPos <= 320.0f) {
        int frameIdx = rand() % (int)stickerFrames.size();
        const auto& f = stickerFrames[frameIdx];
        float logicalW = f.rotated ? (float)f.h : (float)f.w;
        float logicalH = f.rotated ? (float)f.w : (float)f.h;

        TransitionSticker s;
        s.frameIdx = frameIdx;
        s.x = xPos + logicalW * 0.5f;
        s.y = yPos + logicalH * 0.5f;

        int deg = -60 + (rand() % 131);
        s.angle = (float)deg * (3.141592653589793f / 180.0f);
        s.targetScale = 0.97f + ((rand() % 6) / 100.0f);
        s.currentScale = 1.05f;
        s.popDuration = (float)(rand() % 3) * (1.0f / 24.0f);
        s.popTimer = s.popDuration;
        s.visible = false;
        bottomStickers.push_back(s);

        xPos += logicalW * 0.5f;
        if (xPos >= 320.0f) {
            if (yPos <= 240.0f) {
                xPos = -60.0f;
                yPos += 28.0f + (rand() % 22);
            }
        }
    }

    // Shuffle grid stickers
    for (size_t i = topStickers.size() - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        std::swap(topStickers[i], topStickers[j]);
    }
    for (size_t i = bottomStickers.size() - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        std::swap(bottomStickers[i], bottomStickers[j]);
    }

    // Add Center Sticker for Top Screen
    {
        int frameIdx = rand() % (int)stickerFrames.size();
        TransitionSticker lastTop;
        lastTop.frameIdx = frameIdx;
        lastTop.x = 200.0f;
        lastTop.y = 120.0f;
        lastTop.angle = 0.0f;
        lastTop.targetScale = 0.97f + ((rand() % 6) / 100.0f);
        lastTop.currentScale = 1.05f;
        lastTop.popDuration = 2.0f * (1.0f / 24.0f);
        lastTop.popTimer = lastTop.popDuration;
        lastTop.visible = false;
        topStickers.push_back(lastTop);
    }

    // Add Center Sticker for Bottom Screen
    {
        int frameIdx = rand() % (int)stickerFrames.size();
        TransitionSticker lastBot;
        lastBot.frameIdx = frameIdx;
        lastBot.x = 160.0f;
        lastBot.y = 120.0f;
        lastBot.angle = 0.0f;
        lastBot.targetScale = 0.97f + ((rand() % 6) / 100.0f);
        lastBot.currentScale = 1.05f;
        lastBot.popDuration = 2.0f * (1.0f / 24.0f);
        lastBot.popTimer = lastBot.popDuration;
        lastBot.visible = false;
        bottomStickers.push_back(lastBot);
    }

    // Assign sequential timing (0.0s to 0.90s)
    for (size_t i = 0; i < topStickers.size(); ++i) {
        topStickers[i].timing = (topStickers.size() > 1) 
            ? ((float)i / (float)(topStickers.size() - 1)) * 0.90f 
            : 0.0f;
    }
    for (size_t i = 0; i < bottomStickers.size(); ++i) {
        bottomStickers[i].timing = (bottomStickers.size() > 1) 
            ? ((float)i / (float)(bottomStickers.size() - 1)) * 0.90f 
            : 0.0f;
    }
}

void MusicBeatState::drawStickerTransition(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    if (!useStickerTransition || stickerFrames.empty()) return;

    if (top) {
        C2D_SceneBegin(top);
        float baseDepth = 0.90f;
        float depthStep = topStickers.empty() ? 0.0f : (0.08f / (float)topStickers.size());
        for (size_t i = 0; i < topStickers.size(); ++i) {
            const auto& s = topStickers[i];
            if (!s.visible) continue;
            const auto& f = stickerFrames[s.frameIdx];
            C2D_Image img;
            img.tex = f.tex;
            img.subtex = (Tex3DS_SubTexture*)&f.uv;
            float finalAngle = s.angle + (f.rotated ? -(3.141592653589793f / 2.0f) : 0.0f);
            float d = baseDepth + (float)i * depthStep;
            C2D_DrawImageAtRotated(img, s.x, s.y, d, finalAngle, nullptr, s.currentScale, s.currentScale);
        }
    }

    if (!g_isRightEye && bottom) {
        C2D_SceneBegin(bottom);
        float baseDepth = 0.90f;
        float depthStep = bottomStickers.empty() ? 0.0f : (0.08f / (float)bottomStickers.size());
        for (size_t i = 0; i < bottomStickers.size(); ++i) {
            const auto& s = bottomStickers[i];
            if (!s.visible) continue;
            const auto& f = stickerFrames[s.frameIdx];
            C2D_Image img;
            img.tex = f.tex;
            img.subtex = (Tex3DS_SubTexture*)&f.uv;
            float finalAngle = s.angle + (f.rotated ? -(3.141592653589793f / 2.0f) : 0.0f);
            float d = baseDepth + (float)i * depthStep;
            C2D_DrawImageAtRotated(img, s.x, s.y, d, finalAngle, nullptr, s.currentScale, s.currentScale);
        }
    }
}

void MusicBeatState::cleanupStickerTransition() {
    if (stickerSheet) {
        C2D_SpriteSheetFree(stickerSheet);
        stickerSheet = nullptr;
    }
    stickerFrames.clear();
    topStickers.clear();
    bottomStickers.clear();
    stickerDegenInit = false;

    AudioEngine::clearSoundCache();
}

void MusicBeatState::updateStickerTransition(float dt) {
    if (!useStickerTransition || stickerFrames.empty()) return;

    int soundsPlayedThisFrame = 0;
    const int maxSoundsPerFrame = 4;

    if (transPhase == TransitionPhase::FADE_OUT) {
        for (auto& s : topStickers) {
            if (!s.visible && transTimer >= s.timing) {
                s.visible = true;
                s.currentScale = 1.05f;
                s.popTimer = s.popDuration;
                if (soundsPlayedThisFrame < maxSoundsPerFrame) {
                    int randIdx = rand() % 8;
                    AudioEngine::playSound(stickerSounds[randIdx], 0.7f);
                    soundsPlayedThisFrame++;
                }
            }
            if (s.visible && s.popTimer > 0.0f) {
                s.popTimer -= dt;
                if (s.popTimer <= 0.0f) {
                    s.currentScale = s.targetScale;
                }
            }
        }

        for (auto& s : bottomStickers) {
            if (!s.visible && transTimer >= s.timing) {
                s.visible = true;
                s.currentScale = 1.05f;
                s.popTimer = s.popDuration;
                if (soundsPlayedThisFrame < maxSoundsPerFrame) {
                    int randIdx = rand() % 8;
                    AudioEngine::playSound(stickerSounds[randIdx], 0.7f);
                    soundsPlayedThisFrame++;
                }
            }
            if (s.visible && s.popTimer > 0.0f) {
                s.popTimer -= dt;
                if (s.popTimer <= 0.0f) {
                    s.currentScale = s.targetScale;
                }
            }
        }
    } else if (transPhase == TransitionPhase::FADE_IN) {
        if (!stickerDegenInit) {
            for (auto& s : topStickers) {
                s.visible = true;
                s.currentScale = s.targetScale;
            }
            for (auto& s : bottomStickers) {
                s.visible = true;
                s.currentScale = s.targetScale;
            }
            stickerDegenInit = true;
        }

        for (auto& s : topStickers) {
            if (s.visible && transTimer >= s.timing) {
                s.visible = false;
                if (soundsPlayedThisFrame < maxSoundsPerFrame) {
                    int randIdx = rand() % 8;
                    AudioEngine::playSound(stickerSounds[randIdx], 0.7f);
                    soundsPlayedThisFrame++;
                }
            }
        }

        for (auto& s : bottomStickers) {
            if (s.visible && transTimer >= s.timing) {
                s.visible = false;
                if (soundsPlayedThisFrame < maxSoundsPerFrame) {
                    int randIdx = rand() % 8;
                    AudioEngine::playSound(stickerSounds[randIdx], 0.7f);
                    soundsPlayedThisFrame++;
                }
            }
        }
    }
}

void MusicBeatState::init() {}
void MusicBeatState::update(float dt) {}
void MusicBeatState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {}
void MusicBeatState::exitState() {}
void MusicBeatState::stepHit(int step) {}
void MusicBeatState::beatHit(int beat) {}
