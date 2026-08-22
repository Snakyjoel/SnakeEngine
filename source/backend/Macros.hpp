#pragma once
#include <citro2d.h>
#include <citro3d.h>
#include "SparrowParser.hpp"

// Screen
#define ScreenWidthTop 400
#define ScreenWidthBot 320
#define ScreenHeight 240
#define ScreenWidth 400 //For compatibility idk but im gonna keep it

#define topScreen 0
#define bottomScreen 1

// Text
#define DrawTextBorderCardinal(textObj, x, y, depth, scaleX, scaleY, size, color) \
    C2D_DrawText(textObj, C2D_WithColor, (x) - (size), (y), depth, scaleX, scaleY, color); \
    C2D_DrawText(textObj, C2D_WithColor, (x) + (size), (y), depth, scaleX, scaleY, color); \
    C2D_DrawText(textObj, C2D_WithColor, (x), (y) - (size), depth, scaleX, scaleY, color); \
    C2D_DrawText(textObj, C2D_WithColor, (x), (y) + (size), depth, scaleX, scaleY, color)

#define DrawTextBorderDiagonal(textObj, x, y, depth, scaleX, scaleY, size, color) \
    C2D_DrawText(textObj, C2D_WithColor, (x) - (size), (y) - (size), depth, scaleX, scaleY, color); \
    C2D_DrawText(textObj, C2D_WithColor, (x) + (size), (y) - (size), depth, scaleX, scaleY, color); \
    C2D_DrawText(textObj, C2D_WithColor, (x) - (size), (y) + (size), depth, scaleX, scaleY, color); \
    C2D_DrawText(textObj, C2D_WithColor, (x) + (size), (y) + (size), depth, scaleX, scaleY, color)

#define DrawTextBorderFull(textObj, x, y, depth, scaleX, scaleY, size, color) \
    DrawTextBorderCardinal(textObj, x, y, depth, scaleX, scaleY, size, color); \
    DrawTextBorderDiagonal(textObj, x, y, depth, scaleX, scaleY, size, color)

#define AddTextDepth(text, x, y, scale, centered, border, color, maxWidth, depth) \
    do { \
        ClearTextBuf(); \
        C2D_Text gText; \
        C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, std::string((text)).c_str()); \
        C2D_TextOptimize(&gText); \
        float tw, th; \
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th); \
        float actualScale = (scale); \
        if ((maxWidth) > 0.0f && tw > (maxWidth)) { \
            actualScale *= (float)(maxWidth) / tw; \
            C2D_TextGetDimensions(&gText, actualScale, actualScale, &tw, &th); \
        } \
        float dx = (centered) ? (float)(x) - (tw / 2.0f) : (float)(x); \
        float dy = (centered) ? (float)(y) - (th / 2.0f) : (float)(y); \
        dx = std::round(dx); dy = std::round(dy); \
        if ((border) > 0.0f) { DrawTextBorderFull(&gText, dx, dy, (depth) - 0.01f, actualScale, actualScale, border, C2D_Color32(0, 0, 0, ((color) >> 24) & 0xFF)); } \
        C2D_DrawText(&gText, C2D_WithColor, dx, dy, (depth), actualScale, actualScale, color); \
        C2D_Flush(); \
    } while(0)

#define AddText(text, x, y, scale, centered, border, color, maxWidth) \
    AddTextDepth(text, x, y, scale, centered, border, color, maxWidth, 0.85f)

#define AddTextCenteredDepth(text, x, y, scale, border, color, maxWidth, depth) \
    do { \
        ClearTextBuf(); \
        C2D_Text gText; \
        C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, std::string((text)).c_str()); \
        C2D_TextOptimize(&gText); \
        float tw, th; \
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th); \
        float actualScale = (scale); \
        if ((maxWidth) > 0.0f && tw > (maxWidth)) { \
            actualScale *= (float)(maxWidth) / tw; \
            C2D_TextGetDimensions(&gText, actualScale, actualScale, &tw, &th); \
        } \
        float dx = (float)(x); \
        float dy = (float)(y) - (th / 2.0f); \
        dx = std::round(dx); dy = std::round(dy); \
        u32 flags = C2D_WithColor | C2D_AlignCenter; \
        if ((border) > 0.0f) { \
            u32 borderColor = C2D_Color32(0, 0, 0, ((color) >> 24) & 0xFF); \
            C2D_DrawText(&gText, flags, dx - (border), dy, (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx + (border), dy, (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx, dy - (border), (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx, dy + (border), (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx - (border), dy - (border), (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx + (border), dy - (border), (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx - (border), dy + (border), (depth) - 0.01f, actualScale, actualScale, borderColor); \
            C2D_DrawText(&gText, flags, dx + (border), dy + (border), (depth) - 0.01f, actualScale, actualScale, borderColor); \
        } \
        C2D_DrawText(&gText, flags, dx, dy, (depth), actualScale, actualScale, color); \
        C2D_Flush(); \
    } while(0)

#define AddTextCentered(text, x, y, scale, border, color, maxWidth) \
    AddTextCenteredDepth(text, x, y, scale, border, color, maxWidth, 0.85f)

#define DrawBoldText(font, buf, text, x, y, scale, centered, borderSize, textColor, borderColor, boldOffset, maxWidth) \
do { \
    C2D_Text gText; \
    C2D_TextFontParse(&gText, font, buf, std::string((text)).c_str()); \
    C2D_TextOptimize(&gText); \
    float tw, th; \
    C2D_TextGetDimensions(&gText, scale, scale, &tw, &th); \
    float actualScale = scale; \
    if ((maxWidth) > 0.0f && tw > (maxWidth)) { \
        actualScale *= (maxWidth) / tw; \
        C2D_TextGetDimensions(&gText, actualScale, actualScale, &tw, &th); \
    } \
    float dx = (centered) ? ((x) - (tw / 2.0f)) : (x); \
    float dy = (centered) ? ((y) - (th / 2.0f)) : (y); \
    dx = std::round(dx); \
    dy = std::round(dy); \
    if ((borderSize) > 0.0f) { \
        DrawTextBorderFull(&gText, dx, dy, 0.84f, actualScale, actualScale, (borderSize) + (boldOffset), borderColor); \
    } \
    C2D_DrawText(&gText, C2D_WithColor, dx - (boldOffset), dy, 0.85f, actualScale, actualScale, textColor); \
    C2D_DrawText(&gText, C2D_WithColor, dx + (boldOffset), dy, 0.85f, actualScale, actualScale, textColor); \
    C2D_DrawText(&gText, C2D_WithColor, dx, dy - (boldOffset), 0.85f, actualScale, actualScale, textColor); \
    C2D_DrawText(&gText, C2D_WithColor, dx, dy + (boldOffset), 0.85f, actualScale, actualScale, textColor); \
    C2D_DrawText(&gText, C2D_WithColor, dx, dy, 0.85f, actualScale, actualScale, textColor); \
    C2D_Flush(); \
} while(0)


extern C2D_Font globalVCRFont;

#define ClearTextBuf() \
    if (vcrFontBuf) C2D_TextBufClear(vcrFontBuf)

#define VCRFontFix() \
    vcrFont = globalVCRFont; \
    vcrFontBuf = C2D_TextBufNew(4096);

// State

// Input
extern bool g_inTransition;
#undef hidKeysDown
#undef hidKeysHeld
#undef hidKeysUp
#undef hidTouchRead
#define hidKeysDown() (!g_inTransition ? (hidKeysDown)() : 0)
#define hidKeysHeld() (!g_inTransition ? (hidKeysHeld)() : 0)
#define hidKeysUp() (!g_inTransition ? (hidKeysUp)() : 0)
#define hidTouchRead(pos) \
    do { \
        if (!g_inTransition) { \
            (hidTouchRead)(pos); \
        } else { \
            (pos)->px = 0; (pos)->py = 0; \
        } \
    } while(0)

#define keyPressed(key) (hidKeysHeld() & (key))
#define keyJustPressed(key) (hidKeysDown() & (key))
#define keyJustReleased(key) (hidKeysUp() & (key))

// Colors
#define CWhite C2D_Color32(255, 255, 255, 255)
#define CBlack C2D_Color32(0, 0, 0, 255)
#define CGray C2D_Color32(180, 180, 180, 255)
#define CRed C2D_Color32(255, 50, 50, 255)
#define CGreen C2D_Color32(50, 255, 50, 255)
#define CBlue C2D_Color32(50, 50, 255, 255)
#define CYellow C2D_Color32(255, 255, 50, 255)

// Simple drawing
#define drawImage(img, x, y, z) C2D_DrawImageAt(img, x, y, z)
#define drawImageScaled(img, x, y, z, sx, sy) C2D_DrawImageAt(img, x, y, z, nullptr, sx, sy)
#define drawImageTinted(img, x, y, z, tint) C2D_DrawImageAt(img, x, y, z, tint)
#define drawImageScaledTinted(img, x, y, z, sx, sy, tint) C2D_DrawImageAt(img, x, y, z, tint, sx, sy)

static inline void drawCenteredBG(C2D_Image img, float targetW, float targetH, float depth, C2D_ImageTint* tint = nullptr) {
    if (!img.tex) return;
    float minScaleX = targetW / img.subtex->width;
    float minScaleY = targetH / img.subtex->height;
    float scale = std::max(0.95f, std::max(minScaleX, minScaleY));
    float drawW = img.subtex->width * scale;
    float drawH = img.subtex->height * scale;
    float drawX = (targetW - drawW) / 2.0f;
    float drawY = (targetH - drawH) / 2.0f;
    C2D_DrawImageAt(img, drawX, drawY, depth, tint, scale, scale);
}

#define SetTextureAntialiasing(tex) \
    C3D_TexSetFilter(tex, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST, ClientPrefs::globalAntialiasing ? GPU_LINEAR : GPU_NEAREST)
#define EnableAntialiasing(tex) \
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR)
#define DisableAntialiasing(tex) \
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST)

static inline float frameLogicalW(const Frame& f) {
    if (f.frameW > 0) return (float)f.frameW;
    return f.rotated ? (float)f.h : (float)f.w;
}

static inline float frameLogicalH(const Frame& f) {
    if (f.frameH > 0) return (float)f.frameH;
    return f.rotated ? (float)f.w : (float)f.h;
}

static inline void drawFrameAt(const Frame& f, float x, float y, float depth, C2D_ImageTint* tint = nullptr, float sx = 1.0f, float sy = 1.0f) {
    if (!f.tex) return;
    C2D_Image img = { f.tex, &f.uv };
    float drawX = x - (float)f.frameX * sx;
    float drawY = y - (float)f.frameY * sy;

    if (f.rotated) {
        float cx = drawX + (float)f.h * sx * 0.5f;
        float cy = drawY + (float)f.w * sy * 0.5f;
        C2D_DrawImageAtRotated(img, cx, cy, depth, -(3.14159265f / 2.0f), tint, sx, sy);
    } else {
        C2D_DrawImageAt(img, drawX, drawY, depth, tint, sx, sy);
    }
}

static inline void drawFrameCentered(const Frame& f, float centerX, float centerY, float depth, C2D_ImageTint* tint = nullptr, float sx = 1.0f, float sy = 1.0f) {
    float x = centerX - frameLogicalW(f) * sx * 0.5f;
    float y = centerY - frameLogicalH(f) * sy * 0.5f;
    drawFrameAt(f, x, y, depth, tint, sx, sy);
}

#define DrawFrameCentered(sheet, frameIdx, centerX, centerY, depth, scale) \
do { \
    if ((sheet) && !(sheet)->frames.empty()) { \
        const Frame& f = (sheet)->frames[(frameIdx)]; \
        drawFrameCentered(f, centerX, centerY, depth, nullptr, scale, scale); \
    } \
} while(0)

// drawFlash — triggers a self-fading flash overlay on the given screen.
// Calling it with duration > 0.0f starts the flash (only if not already active).
// Call every frame from draw() — the function advances its own internal timer.
// Example: drawFlash(topScreen, 1.0f, CWhite);  -> 1-second white flash
void flash(int screen, float duration, u32 color);
void drawFlash(int screen);

// Render
#define beginScreen(target) \
do { \
    C2D_SceneBegin(target); \
    C2D_TargetClear(target, CBlack); \
} while(0)

struct NoteSprite {
    C3D_Tex* tex = nullptr;
    Tex3DS_SubTexture sub;
    bool rotated = false;
    float w = 0.0f; // Logical width
    float h = 0.0f; // Logical height
    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
};

static inline NoteSprite cropNoteSprite(const NoteSprite& ns, float cropTop, float cropBottom) {
    NoteSprite cropped = ns;
    if (cropTop <= 0.0f && cropBottom <= 0.0f) return cropped;

    float originalHeight = ns.h;
    if (originalHeight <= 0.0f) return cropped;

    float nw = ns.sub.right - ns.sub.left;
    float nh = ns.sub.bottom - ns.sub.top;

    if (cropTop > 0.0f) {
        if (cropTop >= originalHeight) {
            cropped.sub.width = 0;
            cropped.sub.height = 0;
            cropped.h = 0.0f;
            return cropped;
        }
        cropped.h = originalHeight - cropTop;
        cropped.frameY += cropTop;
        if (ns.rotated) {
            // Rotated 90 degrees counter-clockwise in sheet.
            // Physical left corresponds to logical top.
            cropped.sub.left += cropTop * nw / ns.sub.width;
            cropped.sub.width = (u16)(cropped.sub.width - cropTop);
        } else {
            // Non-rotated: logical top maps to physical top.
            cropped.sub.top += cropTop * nh / ns.sub.height;
            cropped.sub.height = (u16)(cropped.sub.height - cropTop);
        }
    }

    if (cropBottom > 0.0f) {
        float currentHeight = cropped.h;
        if (cropBottom >= currentHeight) {
            cropped.sub.width = 0;
            cropped.sub.height = 0;
            cropped.h = 0.0f;
            return cropped;
        }
        cropped.h = currentHeight - cropBottom;
        if (ns.rotated) {
            // Cropping from logical bottom means cropping from the physical right.
            cropped.sub.right -= cropBottom * nw / ns.sub.width;
            cropped.sub.width = (u16)(cropped.sub.width - cropBottom);
        } else {
            // Non-rotated: logical bottom maps to physical bottom.
            cropped.sub.bottom -= cropBottom * nh / ns.sub.height;
            cropped.sub.height = (u16)(cropped.sub.height - cropBottom);
        }
    }

    return cropped;
}

static inline void renderNoteSprite(const NoteSprite& ns, float x, float y, float depth, const C2D_ImageTint* tint = nullptr, float sx = 1.0f, float sy = 1.0f, float angle = 0.0f, bool flipX = false, bool flipY = false, bool antialiasing = true) {
    if (!ns.sub.width || !ns.sub.height || !ns.tex) return;
    
    GPU_TEXTURE_FILTER_PARAM f = antialiasing ? GPU_LINEAR : GPU_NEAREST;
    C3D_TexSetFilter(ns.tex, f, f);

    C2D_Image img = { ns.tex, &ns.sub };

    float scaleX_final = sx * (flipX ? -1.0f : 1.0f);
    float scaleY_final = sy * (flipY ? -1.0f : 1.0f);

    if (ns.rotated) {
        float cx = x + ns.h * sy * 0.5f;
        float cy = y + ns.w * sx * 0.5f;
        C2D_DrawImageAtRotated(img, cx, cy, depth, angle + (3.14159265f / 2.0f), tint, scaleY_final, scaleX_final);
    } else {
        if (angle != 0.0f || flipX || flipY) {
            float cx = x + ns.w * sx * 0.5f;
            float cy = y + ns.h * sy * 0.5f;
            C2D_DrawImageAtRotated(img, cx, cy, depth, angle, tint, scaleX_final, scaleY_final);
        } else {
            C2D_DrawImageAt(img, x, y, depth, tint, scaleX_final, scaleY_final);
        }
    }
}

static inline void renderRatingSprite(C3D_Tex* tex, const Tex3DS_SubTexture* sub, bool rotated, float frameWidth, float frameHeight, float x, float y, float depth, const C2D_ImageTint* tint, float scale, float angle = 0.0f) {
    if (!sub->width || !sub->height || !tex) return;
    C2D_Image img = { tex, sub };

    if (rotated) {
        float cx = x + sub->height * scale * 0.5f;
        float cy = y + sub->width * scale * 0.5f;
        C2D_DrawImageAtRotated(img, cx, cy, depth, angle - (3.14159265f / 2.0f), tint, scale, scale);
    } else {
        if (angle != 0.0f) {
            float cx = x + sub->width * scale * 0.5f;
            float cy = y + sub->height * scale * 0.5f;
            C2D_DrawImageAtRotated(img, cx, cy, depth, angle, tint, scale, scale);
        } else {
            C2D_DrawImageAt(img, x, y, depth, tint, scale, scale);
        }
    }
}



/*
Hope dies slowly.
Humanity is fading away...

The end is drawing ever closer, but ignorance makes it imperceptible.

Clear your mind, and follow the man who wears blue and black.
Where you have all the options, wait until a unknow door appears.

@{3P7 H1S D3@L.....
*/
