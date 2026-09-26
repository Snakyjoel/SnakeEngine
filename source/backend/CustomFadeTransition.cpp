#include "CustomFadeTransition.hpp"
#include "MusicBeatState.hpp"
#include <citro2d.h>
#include <algorithm>

// Edit this file to customize the fade/wipe transition between states.
// Fully optimized for 3DS PICA200 GPU with zero offscreen geometry submission
// and 100% visually identical output.

void drawWipeOverlay(C3D_RenderTarget* screen, float screenW, float screenH) {
    const auto phase = MusicBeatState::transPhase;
    if (phase == TransitionPhase::NONE) return;

    const float p = MusicBeatState::transProgress;

    // Fast early exits when transition is fully transparent / complete
    if (phase == TransitionPhase::FADE_OUT && p <= 0.0f) return;
    if (phase == TransitionPhase::FADE_IN  && p >= 1.0f) return;

    static constexpr u32 opaqueBlack = C2D_Color32(0, 0, 0, 255);

    C2D_SceneBegin(screen);

    // Fast path when screen is 100% solid black
    if ((phase == TransitionPhase::FADE_OUT && p >= 1.0f) ||
        (phase == TransitionPhase::FADE_IN  && p <= 0.0f)) {
        C2D_DrawRectSolid(0.0f, 0.0f, 1.0f, screenW, screenH, opaqueBlack);
        return;
    }

    constexpr float GRAD_H       = 120.0f;
    constexpr float ALPHA_FACTOR = 255.0f / GRAD_H; // 2.125f per pixel
    const float totalDist        = screenH + GRAD_H;
    const float edgeY            = (p * totalDist) - GRAD_H;

    if (phase == TransitionPhase::FADE_OUT) {
        // Solid curtain body above leading edge
        if (edgeY > 0.0f) {
            C2D_DrawRectSolid(0.0f, 0.0f, 1.0f, screenW, std::min(edgeY, screenH), opaqueBlack);
        }

        // Clamped feathered gradient leading edge
        const float drawTop = std::max(0.0f, edgeY);
        const float drawBot = std::min(screenH, edgeY + GRAD_H);
        if (drawBot > drawTop) {
            const u8 aTop = static_cast<u8>(255.0f - (drawTop - edgeY) * ALPHA_FACTOR);
            const u8 aBot = static_cast<u8>(255.0f - (drawBot - edgeY) * ALPHA_FACTOR);

            const u32 cTop = C2D_Color32(0, 0, 0, aTop);
            const u32 cBot = C2D_Color32(0, 0, 0, aBot);

            C2D_DrawRectangle(0.0f, drawTop, 1.0f, screenW, drawBot - drawTop, cTop, cTop, cBot, cBot);
        }
    } else { // FADE_IN (receding top-to-bottom)
        // Clamped feathered gradient trailing edge (transparent on top, opaque on bottom)
        const float drawTop = std::max(0.0f, edgeY);
        const float drawBot = std::min(screenH, edgeY + GRAD_H);
        if (drawBot > drawTop) {
            const u8 aTop = static_cast<u8>((drawTop - edgeY) * ALPHA_FACTOR);
            const u8 aBot = static_cast<u8>((drawBot - edgeY) * ALPHA_FACTOR);

            const u32 cTop = C2D_Color32(0, 0, 0, aTop);
            const u32 cBot = C2D_Color32(0, 0, 0, aBot);

            C2D_DrawRectangle(0.0f, drawTop, 1.0f, screenW, drawBot - drawTop, cTop, cTop, cBot, cBot);
        }

        // Solid curtain body below trailing edge
        const float solidTop = std::max(0.0f, edgeY + GRAD_H);
        if (solidTop < screenH) {
            C2D_DrawRectSolid(0.0f, solidTop, 1.0f, screenW, screenH - solidTop, opaqueBlack);
        }
    }
}
