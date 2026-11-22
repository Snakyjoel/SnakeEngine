#include "Alphabet.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <algorithm>
#include <ctype.h>
#include <unordered_map>

struct CharOffset {
    float x;
    float y;
};

static const Frame* findAlphabetFrame(CachedSpritesheet* sheet, const std::string& key, CharOffset& outOffset) {
    if (!sheet) return nullptr;

    outOffset = {0.0f, 0.0f};

    static CachedSpritesheet* cachedSheet = nullptr;
    static std::unordered_map<std::string, const Frame*> frameCache;
    static std::unordered_map<std::string, CharOffset> offsetCache;

    if (cachedSheet != sheet) {
        cachedSheet = sheet;
        frameCache.clear();
        offsetCache.clear();
    }

    auto it = frameCache.find(key);
    if (it != frameCache.end()) {
        outOffset = offsetCache[key];
        return it->second;
    }

    std::string animPrefix;
    CharOffset customOffset = {0.0f, 0.0f};

    // Symbols and special animations (bold)
    if (key == "'") { animPrefix = "apostrophe"; customOffset = {0.0f, 40.0f}; }
    else if (key == "\"") { animPrefix = "quote"; customOffset = {0.0f, 40.0f}; }
    else if (key == "!") { animPrefix = "exclamation"; }
    else if (key == "?") { animPrefix = "question"; }
    else if (key == ".") { animPrefix = "period"; }
    else if (key == ",") { animPrefix = "comma"; customOffset = {0.0f, -4.0f}; }
    else if (key == "\\") { animPrefix = "back slash"; }
    else if (key == "/") { animPrefix = "forward slash"; }
    else if (key == "¡") { animPrefix = "inverted exclamation"; customOffset = {0.0f, -20.0f}; }
    else if (key == "¿") { animPrefix = "inverted question"; customOffset = {0.0f, -20.0f}; }
    else if (key == "•") { animPrefix = "bullet"; }
    else if (key == "*") { animPrefix = "*"; customOffset = {0.0f, 40.0f}; }
    else if (key == "+") { animPrefix = "+"; customOffset = {0.0f, 12.0f}; }
    else if (key == "-") { animPrefix = "-"; customOffset = {0.0f, 16.0f}; }
    else if (key == "<") { animPrefix = "<"; customOffset = {0.0f, -2.0f}; }
    else if (key == ">") { animPrefix = ">"; customOffset = {0.0f, -2.0f}; }
    else if (key == ":") { animPrefix = ":"; customOffset = {0.0f, 8.0f}; }
    else if (key == ";") { animPrefix = ";"; customOffset = {0.0f, 4.0f}; }
    else if (key == "^") { animPrefix = "^"; customOffset = {0.0f, 38.0f}; }
    else if (key == "~") { animPrefix = "~"; customOffset = {0.0f, 20.0f}; }
    else if (key == "ç") { animPrefix = "ç"; customOffset = {0.0f, -11.0f}; }
    else if (key == "&") { animPrefix = "&"; customOffset = {0.0f, 2.0f}; }
    else if (key == "]") { animPrefix = "]"; customOffset = {0.0f, -1.0f}; }
    else {
        // Direct character name (a-z, 0-9, á, é, í, ó, ú, ñ, etc.)
        animPrefix = key;
    }

    std::string targetPsychBold = animPrefix + " bold instance 10000";
    for (const auto& f : sheet->frames) {
        if (f.name == targetPsychBold) {
            frameCache[key] = &f;
            offsetCache[key] = customOffset;
            outOffset = customOffset;
            return &f;
        }
    }

    // Fallback
    std::string boldStart = animPrefix + " bold";
    for (const auto& f : sheet->frames) {
        if (f.name.find(boldStart) == 0) {
            frameCache[key] = &f;
            offsetCache[key] = customOffset;
            outOffset = customOffset;
            return &f;
        }
    }

    // Fallback for character-prefix format
    std::string legacyPrefix = "character-" + animPrefix + "0000";
    for (const auto& f : sheet->frames) {
        if (f.name == legacyPrefix) {
            frameCache[key] = &f;
            offsetCache[key] = {0.0f, 0.0f};
            outOffset = {0.0f, 0.0f};
            return &f;
        }
    }

    // Fallback to old uppercase
    std::string upperPrefix = animPrefix;
    std::transform(upperPrefix.begin(), upperPrefix.end(), upperPrefix.begin(), ::toupper);
    upperPrefix += "0000";
    for (const auto& f : sheet->frames) {
        if (f.name == upperPrefix) {
            frameCache[key] = &f;
            offsetCache[key] = {0.0f, 0.0f};
            outOffset = {0.0f, 0.0f};
            return &f;
        }
    }

    // Fallback to '?'
    if (key != "?") {
        return findAlphabetFrame(sheet, "?", outOffset);
    }

    frameCache[key] = nullptr;
    offsetCache[key] = {0.0f, 0.0f};
    return nullptr;
}

// Helper
static std::vector<std::string> utf8Split(const std::string& str) {
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.length();) {
        unsigned char c = (unsigned char)str[i];
        size_t len = 1;
        if ((c & 0x80) == 0) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;

        if (i + len <= str.length()) {
            chars.push_back(str.substr(i, len));
        } else {
            chars.push_back(str.substr(i, 1));
            len = 1;
        }
        i += len;
    }
    return chars;
}

static std::string toLowerUtf8(const std::string& ch) {
    if (ch.length() == 1) {
        char c = ch[0];
        if (c >= 'A' && c <= 'Z') return std::string(1, (char)tolower((unsigned char)c));
        return ch;
    }
    if (ch == "Á") return "á";
    if (ch == "É") return "é";
    if (ch == "Í") return "í";
    if (ch == "Ó") return "ó";
    if (ch == "Ú") return "ú";
    if (ch == "Ñ") return "ñ";
    if (ch == "Ü") return "ü";
    if (ch == "À") return "à";
    if (ch == "È") return "è";
    if (ch == "Ì") return "ì";
    if (ch == "Ò") return "ò";
    if (ch == "Ù") return "ù";
    if (ch == "Â") return "â";
    if (ch == "Ê") return "ê";
    if (ch == "Î") return "î";
    if (ch == "Ô") return "ô";
    if (ch == "Û") return "û";
    if (ch == "Ç") return "ç";
    return ch;
}

void Alphabet::draw(const std::string& text, float x, float y, float scale, float alpha, bool centered, u32 color, float depth) {
    CachedSpritesheet* alphabetSheet = SpritesheetCache::get().load("shared/images/Alphabet");
    if (!alphabetSheet) return;

    const float baseResFactor = 41.0f / 55.0f;
    float screenScale = (240.0f / 720.0f) * 1.25f * baseResFactor;
    float drawScale = scale * screenScale;

    std::vector<std::string> glyphs = utf8Split(text);

    // Compute width if centered is true
    float startX = x;
    if (centered) {
        float totalWidth = 0.0f;
        for (const auto& rawCh : glyphs) {
            if (rawCh == " ") {
                totalWidth += 38.0f; // space width in glyph units
                continue;
            }
            std::string ch = toLowerUtf8(rawCh);
            CharOffset off;
            const Frame* foundFrame = findAlphabetFrame(alphabetSheet, ch, off);
            if (foundFrame) {
                totalWidth += (foundFrame->frameW + 2.0f);
            }
        }
        startX -= (totalWidth * drawScale) / 2.0f;
    }

    C2D_ImageTint tint;
    C2D_ImageTint* tintPtr = nullptr;
    bool usingMult = false;
    if (color != 0xFFFFFFFF) {
        C2D_PlainImageTint(&tint, color, 1.0f);
        if (alpha < 1.0f) {
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

    for (const auto& rawCh : glyphs) {
        if (rawCh == " ") {
            curX += 38.0f * drawScale;
            continue;
        }

        std::string ch = toLowerUtf8(rawCh);
        CharOffset off;
        const Frame* foundFrame = findAlphabetFrame(alphabetSheet, ch, off);

        if (foundFrame) {
            C2D_Image img;
            img.tex = foundFrame->tex;
            img.subtex = &foundFrame->uv;

            float psychYOffset = (55.0f - foundFrame->frameH) - off.y;
            float lx = curX - (foundFrame->frameX - off.x) * drawScale;
            float ly = curY - (foundFrame->frameY - psychYOffset) * drawScale;

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

    const float baseResFactor = 41.0f / 55.0f;
    float screenScale = (240.0f / 720.0f) * 1.25f * baseResFactor;
    float drawScale = scale * screenScale;

    std::vector<std::string> glyphs = utf8Split(text);

    float totalWidth = 0.0f;
    for (const auto& rawCh : glyphs) {
        if (rawCh == " ") {
            totalWidth += 38.0f * drawScale;
            continue;
        }
        std::string ch = toLowerUtf8(rawCh);
        CharOffset off;
        const Frame* foundFrame = findAlphabetFrame(alphabetSheet, ch, off);
        if (foundFrame) {
            totalWidth += (foundFrame->frameW + 2.0f) * drawScale;
        }
    }
    return totalWidth;
}
