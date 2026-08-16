#include "Alphabet.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <algorithm>
#include <ctype.h>

static const Frame* findAlphabetFrame(CachedSpritesheet* sheet, char c) {
    if (!sheet) return nullptr;
    
    // Step 1: Check new format first: e.g. "character-a0000", "character-zero0000", symbols
    std::string newPrefix;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        newPrefix = "character-" + std::string(1, tolower((unsigned char)c));
    } else if (c >= '0' && c <= '9') {
        switch (c) {
            case '0': newPrefix = "character-zero"; break;
            case '1': newPrefix = "character-one"; break;
            case '2': newPrefix = "character-two"; break;
            case '3': newPrefix = "character-three"; break;
            case '4': newPrefix = "character-four"; break;
            case '5': newPrefix = "character-five"; break;
            case '6': newPrefix = "character-six"; break;
            case '7': newPrefix = "character-seven"; break;
            case '8': newPrefix = "character-eight"; break;
            case '9': newPrefix = "character-nine"; break;
        }
    } else {
        switch (c) {
            case '&': newPrefix = "character-ampersand"; break;
            case '<': newPrefix = "character-anglebracket-left"; break;
            case '>': newPrefix = "character-anglebracket-right"; break;
            case '*': newPrefix = "character-asterisk"; break;
            case '@': newPrefix = "character-at"; break;
            case '\\': newPrefix = "character-backslash"; break;
            case '`': newPrefix = "character-backtick"; break;
            case '^': newPrefix = "character-caret"; break;
            case ':': newPrefix = "character-colon"; break;
            case ',': newPrefix = "character-comma"; break;
            case '{': newPrefix = "character-curlybracket-left"; break;
            case '}': newPrefix = "character-curlybracket-right"; break;
            case '$': newPrefix = "character-dollar"; break;
            case '"': newPrefix = "character-doublequote"; break;
            case '=': newPrefix = "character-equal"; break;
            case '!': newPrefix = "character-exclamationmark"; break;
            case '-': newPrefix = "character-hyphen"; break;
            case '%': newPrefix = "character-percent"; break;
            case '.': newPrefix = "character-period"; break;
            case '+': newPrefix = "character-plus"; break;
            case '#': newPrefix = "character-pound"; break;
            case '?': newPrefix = "character-questionmark-alt"; break;
            case '(': newPrefix = "character-roundbracket-left"; break;
            case ')': newPrefix = "character-roundbracket-right"; break;
            case ';': newPrefix = "character-semicolon"; break;
            case '\'': newPrefix = "character-singlequote"; break;
            case '/': newPrefix = "character-slash"; break;
            case '[': newPrefix = "character-squarebracket-left"; break;
            case ']': newPrefix = "character-squarebracket-right"; break;
            case '~': newPrefix = "character-tilde"; break;
            case '_': newPrefix = "character-underscore"; break;
            case '|': newPrefix = "character-verticalbar"; break;
            default: break;
        }
    }
    
    if (!newPrefix.empty()) {
        std::string targetName = newPrefix + "0000";
        for (const auto& f : sheet->frames) {
            if (f.name == targetName) return &f;
        }
    }
    
    // Step 2: Fallback to old format (original Psych / FNF) for custom skins/mods
    char uc = toupper((unsigned char)c);
    std::string oldPrefix;
    if (uc >= 'A' && uc <= 'Z') {
        oldPrefix = std::string(1, uc);
    } else {
        switch (uc) {
            case '_': oldPrefix = "_"; break;
            case '\'': oldPrefix = "-apostraphie-"; break;
            case ',': oldPrefix = "-comma-"; break;
            case '.': oldPrefix = "-comma-"; break;
            case '-': oldPrefix = "-dash-"; break;
            case '!': oldPrefix = "-exclamation point-"; break;
            case '?': oldPrefix = "-question mark-"; break;
            case '"': oldPrefix = "-start quote-"; break;
            case ':': oldPrefix = ":"; break;
            case '(': oldPrefix = "("; break;
            case ')': oldPrefix = ")"; break;
            case '#': oldPrefix = "#"; break;
            case '%': oldPrefix = "%"; break;
            case '$': oldPrefix = "$"; break;
            default: break;
        }
    }
    if (!oldPrefix.empty()) {
        std::string targetName = oldPrefix + "0000";
        for (const auto& f : sheet->frames) {
            if (f.name == targetName) return &f;
        }
    }
    
    return nullptr;
}

void Alphabet::draw(const std::string& text, float x, float y, float scale, float alpha, bool centered, u32 color, float depth) {
    CachedSpritesheet* alphabetSheet = SpritesheetCache::get().load("shared/images/Alphabet");
    if (!alphabetSheet) return;

    float screenScale = (240.0f / 720.0f) * 1.25f;
    float drawScale = scale * screenScale;

    // First compute width if centered is true
    float startX = x;
    if (centered) {
        float totalWidth = 0.0f;
        for (char c : text) {
            if (c == ' ') {
                totalWidth += 28.0f; // base space width in logical coords
                continue;
            }
            const Frame* foundFrame = findAlphabetFrame(alphabetSheet, c);
            if (foundFrame) {
                totalWidth += foundFrame->frameW + 2.0f; // base width + letter spacing
            }
        }
        startX -= (totalWidth * drawScale) / 2.0f;
    }

    C2D_ImageTint tint;
    C2D_ImageTint* tintPtr = nullptr;
    bool usingMult = false;
    if (color != 0xFFFFFFFF) {
        C2D_PlainImageTint(&tint, color, 1.0f); // 100% blend strength for multiplication
        if (alpha < 1.0f) {
            // Apply alpha to tint corners
            tint.corners[0].color = (tint.corners[0].color & 0x00FFFFFF) | (((u32)(alpha * 255)) << 24);
            tint.corners[1].color = (tint.corners[1].color & 0x00FFFFFF) | (((u32)(alpha * 255)) << 24);
            tint.corners[2].color = (tint.corners[2].color & 0x00FFFFFF) | (((u32)(alpha * 255)) << 24);
            tint.corners[3].color = (tint.corners[3].color & 0x00FFFFFF) | (((u32)(alpha * 255)) << 24);
        }
        tintPtr = &tint;
        usingMult = true;
    } else if (alpha < 1.0f) {
        C2D_AlphaImageTint(&tint, alpha);
        tintPtr = &tint;
    }

    float curX = startX;
    float curY = y;

    for (char c : text) {
        if (c == ' ') {
            curX += 28.0f * drawScale;
            continue;
        }
        const Frame* foundFrame = findAlphabetFrame(alphabetSheet, c);

        if (foundFrame) {
            C2D_Image img;
            img.tex = foundFrame->tex;
            img.subtex = &foundFrame->uv;

            float lx = curX - foundFrame->frameX * drawScale;
            float ly = curY - foundFrame->frameY * drawScale;

            if (usingMult) C2D_SetTintMode(C2D_TintMult);

            if (foundFrame->rotated) {
                float angleRad = -(3.14159265f / 2.0f);
                float cx = lx + foundFrame->uv.height * drawScale / 2.0f;
                float cy = ly + foundFrame->uv.width  * drawScale / 2.0f;
                C2D_DrawImageAtRotated(img, cx, cy, depth, angleRad, tintPtr, drawScale, drawScale);
            } else {
                C2D_DrawImageAt(img, lx, ly, depth, tintPtr, drawScale, drawScale);
            }

            if (usingMult) C2D_SetTintMode(C2D_TintSolid);

            curX += (foundFrame->frameW + 2.0f) * drawScale;
        }
    }
}

float Alphabet::getTextWidth(const std::string& text, float scale) {
    CachedSpritesheet* alphabetSheet = SpritesheetCache::get().load("shared/images/Alphabet");
    if (!alphabetSheet) return 0.0f;

    float screenScale = (240.0f / 720.0f) * 1.25f;
    float drawScale = scale * screenScale;

    float totalWidth = 0.0f;
    for (char c : text) {
        if (c == ' ') {
            totalWidth += 28.0f * drawScale;
            continue;
        }
        const Frame* foundFrame = findAlphabetFrame(alphabetSheet, c);
        if (foundFrame) {
            totalWidth += (foundFrame->frameW + 2.0f) * drawScale;
        }
    }
    return totalWidth;
}
