#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../backend/parsers/SparrowParser.hpp"
#include "../backend/parsers/SpritemapParser.hpp"
#include "../backend/SpritesheetCache.hpp"
#include "../backend/Macros.hpp"

// CppAnimate - a knock off of FlxAnimate lmao
// Uses SpritesheetCache to load the sheet + XML automatically.
// Not intended for gameplay (yet)
class CppAnimate {
public:
    CppAnimate() = default;
    ~CppAnimate();

    // Load spritesheet + XML via SpritesheetCache.
    // Path format is the same as SpritesheetCache::load() (e.g. "preload/images/gfDanceTitle")
    void loadSheet(const std::string& path);

    // ── Spritemap (Adobe Animate) mode ────────────────────────────────────────
    // Load a spritemap export: the compiled .t3x texture, the atlas JSON
    // (spritemap#.json) and the animation JSON (Animation.json).
    // After this call you can add animations via addSpritemapAnim().
    bool loadSpritemap(const std::string& t3xPath,
                       const std::string& atlasJsonPath,
                       const std::string& animJsonPath);

    // Add a named animation that plays the named symbol from Animation.json.
    // fps / loop override the canvas frameRate and loopType from the JSON.
    // Pass fps <= 0 to use the canvas frameRate from the JSON.
    void addSpritemapAnim(const std::string& name,
                          const std::string& symbolName,
                          const std::vector<int>& indices = {},
                          float fps = -1.f,
                          bool loop = true);

    // Returns all animation names found in the Animation.json (idle, select, …)
    std::vector<std::string> getSpritemapAnimNames() const;

    // Add an animation by frame name prefix (Sparrow/XML mode).
    // All frames whose name starts with `prefix` will be collected in order.
    // If `indices` is non-empty, only those positions within the matched frames are used.
    void addAnim(const std::string& name, const std::string& prefix,
                 float fps = 24.0f, bool loop = true,
                 float offX = 0.0f, float offY = 0.0f,
                 const std::vector<int>& indices = {});

    // Playback
    void play(const std::string& name, bool forceRestart = false, int startFrame = 0);
    void setLoop(const std::string& name, bool loop);
    void stop();
    void pause();
    void resume();

    // Update frame timer. Call every frame from your state's update().
    void update(float dt);

    // Draw the current frame at (x, y).
    // x/y is the top-left corner taking frameX/frameY offsets into account.
    void draw(float x, float y, float depth,
              float sx = 1.0f, float sy = 1.0f,
              C2D_ImageTint* tint = nullptr);

    // Draw centered at (cx, cy).
    void drawCentered(float cx, float cy, float depth,
                      float sx = 1.0f, float sy = 1.0f,
                      C2D_ImageTint* tint = nullptr);

    // Fired at the end of a non-looping animation.
    std::function<void(const std::string&)> onAnimFinished;
    std::function<void(const std::string&, int)> onFrameChange;

    bool hasAnim(const std::string& name) const;

    // Logical size of the current frame (respects frameW/frameH if set)
    float width()  const;
    float height() const;

    // State
    std::string curAnim    = "";
    bool animFinished      = false;
    bool visible           = true;
    float alpha            = 1.0f;
    float angle            = 0.0f; // Rotation angle in degrees
    bool flipX             = false;
    bool flipY             = false;
    float scaleX           = 1.0f;
    float scaleY           = 1.0f;
    bool antialiasing      = true;

    // Offset applied on top of the animation's own offset
    float extraOffsetX     = 0.0f;
    float extraOffsetY     = 0.0f;

    // Visual center in Flash coordinates for drawCentered
    float visualCenterX    = 0.0f;
    float visualCenterY    = 0.0f;
    bool hasVisualCenter   = false;
    void setVisualCenter(float cx, float cy) {
        visualCenterX = cx;
        visualCenterY = cy;
        hasVisualCenter = true;
    }

    // Set this to true to ignore frame offsets (frameX/frameY) from the XML
    bool ignoreFrameOffsets = false;

    // Scale applied to the whole spritemap composition (auto-computed or manual)
    // Only used in spritemap mode. Set before calling play().
    float spritemapScale   = 1.0f;
    float spritemapTextureScale = 1.0f; // Scale multiplier for downscaled atlas textures

    // ── Spritemap runtime state (public for debug/display) ────────────────────
    SpritemapData   smData;                    // parsed JSON data
    C2D_SpriteSheet smSheet  = nullptr;        // loaded .t3x sheet
    C3D_Tex*        smTex    = nullptr;        // ptr into smSheet (not owned)
    const Tex3DS_SubTexture* smSubtex = nullptr; // ptr to subtex from smSheet
    int             smStartFrame   = 0;        // starting frame of current clip
    int             smLogicalFrame = 0;        // current frame in playing symbol
    int             smTotalFrames  = 0;        // total frames of playing symbol
    float           smFPS          = 24.f;    // effective fps of current anim

    // ── Sparrow / XML mode (private) ──────────────────────────────────────────
private:
    bool isSpritemapMode   = false;

    CachedSpritesheet* sheet = nullptr;

    // Built animation data: indices into sheet->frames
    struct AnimData {
        std::string prefix;
        std::vector<int> frameIndices; // indices into sheet->frames
        float fps;
        bool loop;
        float offX, offY;
    };

    std::map<std::string, AnimData> anims;
    const AnimData* getCurAnimData() const;
    int curFrameIdx        = 0;  // index into curAnimData->frameIndices
    float frameTimer       = 0.0f;
    bool paused            = false;

    const Frame* currentFrame() const;

    struct SmAnimData {
        std::string symbolName;
        float fps;
        bool loop;
        std::vector<int> indices;
    };
    std::map<std::string, SmAnimData> smAnims;

    // Runtime playback state (internal only)
    Tex3DS_SubTexture smFullSub;          // covers the whole texture
    float   smFrameTimer   = 0.0f;
    bool    smLoop         = true;


    // Draw helpers for spritemap mode
    // Draws one symbol recursively, applying parent transform mx.
    void drawSymbol(const std::string& symbolName,
                    int logicalFrame,
                    const AffineMatrix& parentMX,
                    float depth,
                    int recursionDepth = 0,
                    const C2D_ImageTint* tint = nullptr,
                    float parentAlpha = 1.0f) const;

    // Draw one atlas piece with a composed affine matrix.
    void drawPiece(const std::string& pieceName,
                   const AffineMatrix& mx,
                   float depth,
                   const C2D_ImageTint* tint = nullptr,
                   float pieceAlpha = 1.0f) const;

    // Compose two affine matrices: result = child applied in parent space
    static AffineMatrix compose(const AffineMatrix& parent, const AffineMatrix& child);

    // Convert pivot + matrix to a translation-adjusted matrix
    static AffineMatrix applyPivot(const AffineMatrix& mx, const Pivot& trp);
};
