#include "CustomFadeTransition.hpp"
#include "MusicBeatState.hpp"
#include <citro2d.h>
#include <algorithm>
#include <cmath>

// Edit this file to customize the fade/wipe transition between states.
// GRAD_H controls how tall the feathered gradient edge is (in pixels).
// The curtain sweeps top-to-bottom on FADE_OUT, bottom-to-top on FADE_IN.

void drawWipeOverlay(C3D_RenderTarget* screen, float screenW, float screenH) {
    if (MusicBeatState::transPhase == TransitionPhase::NONE) return;

    C2D_SceneBegin(screen);

    const float GRAD_H = 120.0f;
    const float p      = MusicBeatState::transProgress;

    u32 opaqueBlack      = C2D_Color32(0, 0, 0, 255);
    u32 transparentBlack = C2D_Color32(0, 0, 0, 0);

    if (MusicBeatState::transPhase == TransitionPhase::FADE_OUT) {
        float edgeY = (p * (screenH + GRAD_H)) - GRAD_H;

        // Solid curtain body above the leading edge
        float solidH = edgeY;
        if (solidH > 0.0f) {
            C2D_DrawRectSolid(0, 0, 1.0f, screenW,
                              std::min(solidH, screenH),
                              opaqueBlack);
        }

        // Feathered gradient at the leading edge
        float gradTop = edgeY;
        if (gradTop + GRAD_H > 0.0f && gradTop < screenH) {
            C2D_DrawRectangle(0.0f, gradTop, 1.0f, screenW, GRAD_H,
                              opaqueBlack, opaqueBlack, transparentBlack, transparentBlack);
        }

    } else { // FADE_IN
        float edgeY = p * (screenH + GRAD_H);

        // Feathered gradient above the curtain body
        float gradTop = edgeY;
        if (gradTop + GRAD_H > 0.0f && gradTop < screenH) {
            C2D_DrawRectangle(0.0f, gradTop, 1.0f, screenW, GRAD_H,
                              transparentBlack, transparentBlack, opaqueBlack, opaqueBlack);
        }

        // Solid curtain body below the gradient
        float blackTop = edgeY + GRAD_H;
        if (blackTop < screenH) {
            float clampedTop = std::max(0.0f, blackTop);
            C2D_DrawRectSolid(0, clampedTop, 1.0f, screenW,
                              screenH - clampedTop,
                              opaqueBlack);
        }
    }
}
