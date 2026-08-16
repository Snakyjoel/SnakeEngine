#include "MusicBeatState.hpp"
#include "AudioEngine.hpp"
#include "SparrowParser.hpp"
#include <cstdlib>

struct TransitionSticker {
    int frameIdx;
    float x, y;
    float angle;
    float scale;
};

static C2D_SpriteSheet stickerSheet = nullptr;
static std::vector<Frame> stickerFrames;
static std::vector<TransitionSticker> topStickers;
static std::vector<TransitionSticker> bottomStickers;
static int lastTopCount = 0;
static int lastBottomCount = 0;

static const std::string stickerSounds[] = {
    "romfs:/preload/sounds/stickers/keyClick1.ogg",
    "romfs:/preload/sounds/stickers/keyClick2.ogg",
    "romfs:/preload/sounds/stickers/keyClick3.ogg",
    "romfs:/preload/sounds/stickers/keyClick4.ogg",
    "romfs:/preload/sounds/stickers/keyClick5.ogg",
    "romfs:/preload/sounds/stickers/keyClick7.ogg",
    "romfs:/preload/sounds/stickers/keyClick8.ogg",
    "romfs:/preload/sounds/stickers/keyClick9.ogg"
};

MusicBeatState* MusicBeatState::nextState    = nullptr;
TransitionPhase  MusicBeatState::transPhase  = TransitionPhase::NONE;
float            MusicBeatState::transProgress = 0.0f;
float            MusicBeatState::transTimer  = 0.0f;
bool             MusicBeatState::skipTransition = false;
bool             MusicBeatState::useStickerTransition = false;

void MusicBeatState::switchState(MusicBeatState* newState) {
    if (transPhase == TransitionPhase::FADE_OUT) {
        delete nextState;
        nextState = newState;
        return;
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

    float xPos = -50.0f;
    float yPos = -50.0f;
    while (xPos <= 400.0f) {
        int frameIdx = rand() % stickerFrames.size();
        const auto& f = stickerFrames[frameIdx];

        TransitionSticker s;
        s.frameIdx = frameIdx;
        s.x = xPos;
        s.y = yPos;
        s.angle = ((rand() % 100) / 100.0f) * 2.2f - 1.1f;
        s.scale = 1.2f + ((rand() % 100) / 100.0f) * 0.4f;
        topStickers.push_back(s);

        xPos += (f.w * s.scale) * 0.5f;
        if (xPos >= 400.0f) {
            if (yPos <= 240.0f) {
                xPos = -50.0f;
                yPos += 45.0f + (rand() % 35);
            }
        }
    }

    xPos = -50.0f;
    yPos = -50.0f;
    while (xPos <= 320.0f) {
        int frameIdx = rand() % stickerFrames.size();
        const auto& f = stickerFrames[frameIdx];

        TransitionSticker s;
        s.frameIdx = frameIdx;
        s.x = xPos;
        s.y = yPos;
        s.angle = ((rand() % 100) / 100.0f) * 2.2f - 1.1f;
        s.scale = 1.2f + ((rand() % 100) / 100.0f) * 0.4f;
        bottomStickers.push_back(s);

        xPos += (f.w * s.scale) * 0.5f;
        if (xPos >= 320.0f) {
            if (yPos <= 240.0f) {
                xPos = -50.0f;
                yPos += 45.0f + (rand() % 35);
            }
        }
    }

    for (size_t i = topStickers.size() - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        std::swap(topStickers[i], topStickers[j]);
    }
    for (size_t i = bottomStickers.size() - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        std::swap(bottomStickers[i], bottomStickers[j]);
    }

    if (!topStickers.empty()) {
        auto& lastTop = topStickers.back();
        lastTop.x = 200.0f;
        lastTop.y = 120.0f;
        lastTop.angle = 0.0f;
        lastTop.scale = 2.0f;
    }
    if (!bottomStickers.empty()) {
        auto& lastBottom = bottomStickers.back();
        lastBottom.x = 160.0f;
        lastBottom.y = 120.0f;
        lastBottom.angle = 0.0f;
        lastBottom.scale = 2.0f;
    }

    lastTopCount = 0;
    lastBottomCount = 0;
}

void MusicBeatState::drawStickerTransition(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    if (!useStickerTransition || stickerFrames.empty()) return;

    int totalTop = topStickers.size();
    int totalBottom = bottomStickers.size();

    int showTop = 0;
    int showBottom = 0;

    if (transPhase == TransitionPhase::FADE_OUT) {
        showTop = (int)(transProgress * totalTop);
        if (showTop > totalTop) showTop = totalTop;

        showBottom = (int)(transProgress * totalBottom);
        if (showBottom > totalBottom) showBottom = totalBottom;
    } else if (transPhase == TransitionPhase::FADE_IN) {
        showTop = (int)((1.0f - transProgress) * totalTop);
        if (showTop < 0) showTop = 0;

        showBottom = (int)((1.0f - transProgress) * totalBottom);
        if (showBottom < 0) showBottom = 0;
    } else {
        return;
    }

    if (top) {
        C2D_SceneBegin(top);
        for (int i = 0; i < showTop; ++i) {
            const auto& s = topStickers[i];
            const auto& f = stickerFrames[s.frameIdx];
            C2D_Image img;
            img.tex = f.tex;
            img.subtex = (Tex3DS_SubTexture*)&f.uv;
            C2D_DrawImageAtRotated(img, s.x, s.y, 0.98f, s.angle, nullptr, s.scale, s.scale);
        }
    }

    if (bottom) {
        C2D_SceneBegin(bottom);
        for (int i = 0; i < showBottom; ++i) {
            const auto& s = bottomStickers[i];
            const auto& f = stickerFrames[s.frameIdx];
            C2D_Image img;
            img.tex = f.tex;
            img.subtex = (Tex3DS_SubTexture*)&f.uv;
            C2D_DrawImageAtRotated(img, s.x, s.y, 0.98f, s.angle, nullptr, s.scale, s.scale);
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
    lastTopCount = 0;
    lastBottomCount = 0;

    AudioEngine::clearSoundCache();
}

void MusicBeatState::updateStickerTransition(float dt) {
    if (!useStickerTransition) return;

    int totalTop = topStickers.size();
    int totalBottom = bottomStickers.size();

    int targetTop = 0;
    int targetBottom = 0;

    if (transPhase == TransitionPhase::FADE_OUT) {
        targetTop = (int)(transProgress * totalTop);
        if (targetTop > totalTop) targetTop = totalTop;

        targetBottom = (int)(transProgress * totalBottom);
        if (targetBottom > totalBottom) targetBottom = totalBottom;

        if (targetTop > lastTopCount || targetBottom > lastBottomCount) {
            int randIdx = rand() % 8;
            AudioEngine::playSound(stickerSounds[randIdx], 0.7f);
            lastTopCount = targetTop;
            lastBottomCount = targetBottom;
        }
    } else if (transPhase == TransitionPhase::FADE_IN) {
        targetTop = (int)((1.0f - transProgress) * totalTop);
        if (targetTop < 0) targetTop = 0;

        targetBottom = (int)((1.0f - transProgress) * totalBottom);
        if (targetBottom < 0) targetBottom = 0;

        if (targetTop < lastTopCount || targetBottom < lastBottomCount) {
            int randIdx = rand() % 8;
            AudioEngine::playSound(stickerSounds[randIdx], 0.7f);
            lastTopCount = targetTop;
            lastBottomCount = targetBottom;
        }
    }
}

void MusicBeatState::init() {}
void MusicBeatState::update(float dt) {}
void MusicBeatState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {}
void MusicBeatState::exitState() {}
void MusicBeatState::stepHit(int step) {}
void MusicBeatState::beatHit(int beat) {}
