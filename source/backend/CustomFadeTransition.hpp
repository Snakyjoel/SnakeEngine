#pragma once
#include <citro3d.h>

// Draws the wipe/curtain fade overlay on a single screen.
// Call this for both top and bottom screens during a transition.
void drawWipeOverlay(C3D_RenderTarget* screen, float screenW, float screenH);
