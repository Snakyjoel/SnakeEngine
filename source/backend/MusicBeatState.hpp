#pragma once
#include <3ds.h>
#include <citro2d.h>

enum class TransitionPhase {
    NONE,
    FADE_OUT,
    FADE_IN
};

class MusicBeatState {
public:
    virtual ~MusicBeatState() {}
    virtual void init();
    virtual void update(float dt);
    virtual void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom);
    virtual void exitState();
    virtual void stepHit(int step);
    virtual void beatHit(int beat);

    static void switchState(MusicBeatState* newState);
    static MusicBeatState* nextState;

    static bool skipTransition;
    static TransitionPhase transPhase;
    static float transProgress;
    static float transTimer;
    static constexpr float TRANS_DURATION = 0.60f;

    static bool useStickerTransition;
    static void initStickerTransition();
    static void drawStickerTransition(C3D_RenderTarget* top, C3D_RenderTarget* bottom);
    static void cleanupStickerTransition();
    static void updateStickerTransition(float dt);
};

inline void switchState(MusicBeatState* newState) {
    MusicBeatState::switchState(newState);
}

inline void resetState() {
    MusicBeatState::switchState(new MusicBeatState());
}
