#pragma once
#include <string>
#include <vector>
#include <map>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// SpritemapParser
// Parses the two JSON files produced by "Better TA Extension" for Adobe Animate:
//   • spritemap#.json  — atlas: maps piece names (numbers) to pixel rects in the PNG
//   • Animation.json   — symbol/timeline tree with affine-matrix keyframes
// ─────────────────────────────────────────────────────────────────────────────

// One piece in the spritemap atlas (e.g. "0", "5", "18")
struct SpritemapPiece {
    std::string name;   // the string key ("0", "18", ...)
    int x, y, w, h;
    bool rotated;
    float uvLeft = 0.0f;
    float uvTop = 0.0f;
    float uvRight = 0.0f;
    float uvBottom = 0.0f;
    bool hasUv = false;
};

// An affine transform matrix [a, b, c, d, tx, ty]
// Matches the Flash / Animate convention:
//   | a  b  0 |
//   | c  d  0 |
//   | tx ty 1 |
struct AffineMatrix {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
};

// Transform pivot – the local origin of the piece before applying MX
struct Pivot {
    float x = 0, y = 0;
};

// Instance of an atlas sprite placed in a keyframe (ASI node)
struct AtlasSpriteInstance {
    std::string pieceName;  // key into atlas ("0", "18", ...)
    AffineMatrix mx;
};

// Instance of another symbol placed in a keyframe (SI node)
struct SymbolInstance {
    std::string symbolName; // name of the referenced symbol
    std::string loopType;   // "LP" (loop), "PO" (play once), "SF" (single frame)
    int firstFrame;         // FF — which frame of the child to show / start at
    AffineMatrix mx;
    Pivot trp;              // transform reference point (pivot)
};

// A single element in a keyframe — either an ASI or an SI
struct SpritemapElement {
    bool isAtlasSprite;           // true → use asi; false → use si
    AtlasSpriteInstance asi;
    SymbolInstance si;
};

// One keyframe in a layer
struct SpritemapKeyframe {
    int index;      // I  — frame number this keyframe starts at
    int duration;   // DU — how many frames this keyframe lasts
    std::vector<SpritemapElement> elements;
};

// One layer inside a symbol's timeline
struct SpritemapLayer {
    std::string name;
    std::vector<SpritemapKeyframe> keyframes;
};

// A symbol (animation clip) — can be a top-level animation or a sub-symbol
struct SpritemapSymbol {
    std::string name;
    std::vector<SpritemapLayer> layers;
    mutable int cachedDuration = -1;

    // Total duration in frames (max of all layer durations)
    int duration() const;

    // Returns the keyframe active at logical frame f for a given layer
    const SpritemapKeyframe* getKeyframe(int layerIdx, int frame) const;
};

// Top-level animation entry (from AN.STI / TL.L[])
struct SpritemapAnimation {
    std::string name;          // label from the keyframe (e.g. "idle", "select")
    int startFrame;            // I
    int duration;              // DU
    std::string symbolName;    // which symbol this animation plays (AN.SN)
    std::string loopType;      // "LP", "PO"
};

// Full parsed data from both JSON files
struct SpritemapData {
    // Atlas: piece name → piece rect
    std::map<std::string, SpritemapPiece> atlas;

    // All symbols keyed by name
    std::map<std::string, SpritemapSymbol> symbols;

    // Named animations from the main timeline
    std::vector<SpritemapAnimation> animations;

    // Root symbol name (AN.SN)
    std::string rootSymbolName;

    // Canvas size from MD section
    float canvasW = 1280, canvasH = 720;
    int frameRate = 24;
};

class SpritemapParser {
public:
    // Parse both JSON files and return the combined data.
    // atlasJsonPath  = path to spritemap1.json (or spritemap#.json)
    // animJsonPath   = path to Animation.json
    static bool parse(const std::string& atlasJsonPath,
                      const std::string& animJsonPath,
                      SpritemapData& out);

private:
    static bool parseAtlas(const std::string& path, SpritemapData& out);
    static bool parseAnimation(const std::string& path, SpritemapData& out);
};
