#include "SpritemapParser.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <functional>
#include <algorithm>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Minimal hand-rolled JSON helpers
// ─────────────────────────────────────────────────────────────────────────────

static float fast_atof(const char* p, char** end = nullptr) {
    while (*p == ' ' || *p == '\t') p++;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }
    float res = 0.0f;
    while (*p >= '0' && *p <= '9') {
        res = res * 10.0f + (*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        float frac = 1.0f;
        while (*p >= '0' && *p <= '9') {
            frac *= 0.1f;
            res += (*p - '0') * frac;
            p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        int expSign = 1;
        if (*p == '-') { expSign = -1; p++; }
        else if (*p == '+') { p++; }
        int exp = 0;
        while (*p >= '0' && *p <= '9') {
            exp = exp * 10 + (*p - '0');
            p++;
        }
        res *= std::pow(10.0f, expSign * exp);
    }
    if (end) *end = (char*)p;
    return res * sign;
}

// Extract the raw string value of a key from a JSON substring.
// Handles both "key":"value" and "key":number forms.
static std::string jsonGetRaw(const std::string& src, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t p = src.find(needle);
    if (p == std::string::npos) return "";
    p += needle.size();
    // skip whitespace
    while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\r' || src[p] == '\t')) p++;
    if (p >= src.size()) return "";
    if (src[p] == '"') {
        // string value
        size_t s = p + 1;
        size_t e = src.find('"', s);
        if (e == std::string::npos) return "";
        return src.substr(s, e - s);
    } else {
        // numeric / boolean / array start
        size_t s = p;
        size_t e = s;
        while (e < src.size() && src[e] != ',' && src[e] != '}' && src[e] != ']' && src[e] != '\n') e++;
        std::string v = src.substr(s, e - s);
        // trim
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\n')) v.pop_back();
        return v;
    }
}

static float jsonGetFloat(const std::string& src, const std::string& key, float def = 0.f) {
    std::string v = jsonGetRaw(src, key);
    if (v.empty()) return def;
    return fast_atof(v.c_str());
}

static int jsonGetInt(const std::string& src, const std::string& key, int def = 0) {
    std::string v = jsonGetRaw(src, key);
    if (v.empty()) return def;
    return (int)fast_atof(v.c_str());
}

// Find the content of a JSON object value for a given key.
// Returns the substring from '{' to the matching '}'.
// Tolerates optional whitespace between the colon and the opening brace.
static std::string jsonGetObject(const std::string& src, const std::string& key, size_t startPos = 0) {
    std::string needle = "\"" + key + "\":";
    size_t p = src.find(needle, startPos);
    if (p == std::string::npos) return "";
    p += needle.size();
    // skip optional whitespace between ':' and '{'
    while (p < src.size() && (src[p] == ' ' || src[p] == '\t' || src[p] == '\r' || src[p] == '\n')) p++;
    if (p >= src.size() || src[p] != '{') return "";
    int depth = 0;
    size_t s = p;
    for (size_t i = p; i < src.size(); i++) {
        if (src[i] == '{') depth++;
        else if (src[i] == '}') { depth--; if (depth == 0) return src.substr(s, i - s + 1); }
    }
    return "";
}


// Find the raw content between '[' and matching ']' for a given key.
// Tolerates optional whitespace between the colon and the opening bracket.
static std::string jsonGetArray(const std::string& src, const std::string& key, size_t startPos = 0) {
    std::string needle = "\"" + key + "\":";
    size_t p = src.find(needle, startPos);
    if (p == std::string::npos) return "";
    p += needle.size();
    // skip optional whitespace between ':' and '['
    while (p < src.size() && (src[p] == ' ' || src[p] == '\t' || src[p] == '\r' || src[p] == '\n')) p++;
    if (p >= src.size() || src[p] != '[') return "";
    int depth = 0;
    size_t s = p;
    for (size_t i = p; i < src.size(); i++) {
        if (src[i] == '[') depth++;
        else if (src[i] == ']') { depth--; if (depth == 0) return src.substr(s, i - s + 1); }
    }
    return "";
}

// Parse a 6-element float array "[a,b,c,d,tx,ty]" into an AffineMatrix
static AffineMatrix parseMX(const std::string& src, size_t startPos = 0) {
    AffineMatrix m;
    std::string needle = "\"MX\":";
    size_t p = src.find(needle, startPos);
    if (p == std::string::npos) return m;
    p += needle.size();
    while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\r' || src[p] == '\t')) p++;
    if (p >= src.size() || src[p] != '[') return m;
    p++; // skip '['
    float vals[6] = {1,0,0,1,0,0};
    for (int i = 0; i < 6; i++) {
        while (p < src.size() && (src[p] == ' ' || src[p] == ',' || src[p] == '\n' || src[p] == '\r' || src[p] == '\t')) p++;
        if (p >= src.size() || src[p] == ']') break;
        char* end;
        vals[i] = fast_atof(src.c_str() + p, &end);
        p = (size_t)(end - src.c_str());
    }
    m.a = vals[0]; m.b = vals[1]; m.c = vals[2]; m.d = vals[3];
    m.tx = vals[4]; m.ty = vals[5];
    return m;
}

// Parse "TRP":{"x":...,"y":...}
static Pivot parseTRP(const std::string& src, size_t startPos = 0) {
    Pivot p2;
    std::string trpStr = jsonGetObject(src, "TRP");
    if (trpStr.empty()) return p2;
    p2.x = jsonGetFloat(trpStr, "x");
    p2.y = jsonGetFloat(trpStr, "y");
    return p2;
}

// Iterate over top-level JSON objects in an array string "[{...},{...},...]"
// Calls callback(obj_string) for each top-level object
static void forEachObject(const std::string& arr, std::function<void(const std::string&)> cb) {
    int depth = 0;
    size_t start = std::string::npos;
    for (size_t i = 0; i < arr.size(); i++) {
        char c = arr[i];
        if (c == '{') {
            if (depth == 0) start = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0 && start != std::string::npos) {
                cb(arr.substr(start, i - start + 1));
                start = std::string::npos;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SpritemapSymbol helpers
// ─────────────────────────────────────────────────────────────────────────────

int SpritemapSymbol::duration() const {
    if (cachedDuration >= 0) return cachedDuration;
    int maxDur = 0;
    for (const auto& layer : layers) {
        for (const auto& kf : layer.keyframes) {
            int end = kf.index + kf.duration;
            if (end > maxDur) maxDur = end;
        }
    }
    cachedDuration = maxDur;
    return maxDur;
}

const SpritemapKeyframe* SpritemapSymbol::getKeyframe(int layerIdx, int frame) const {
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return nullptr;
    const auto& layer = layers[layerIdx];
    for (const auto& kf : layer.keyframes) {
        if (frame >= kf.index && frame < kf.index + kf.duration) {
            return &kf;
        }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse spritemap#.json
// ─────────────────────────────────────────────────────────────────────────────

bool SpritemapParser::parseAtlas(const std::string& path, SpritemapData& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    // ATLAS.SPRITES array
    std::string atlasObj = jsonGetObject(src, "ATLAS");
    if (atlasObj.empty()) return false;
    std::string spritesArr = jsonGetArray(atlasObj, "SPRITES");
    if (spritesArr.empty()) return false;

    forEachObject(spritesArr, [&](const std::string& item) {
        std::string sprObj = jsonGetObject(item, "SPRITE");
        if (sprObj.empty()) return;
        SpritemapPiece piece;
        piece.name    = jsonGetRaw(sprObj, "name");
        piece.x       = jsonGetInt(sprObj, "x");
        piece.y       = jsonGetInt(sprObj, "y");
        piece.w       = jsonGetInt(sprObj, "w");
        piece.h       = jsonGetInt(sprObj, "h");
        std::string rot = jsonGetRaw(sprObj, "rotated");
        piece.rotated = (rot == "true" || rot == "1");
        out.atlas[piece.name] = piece;
    });

    return !out.atlas.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse a symbol's timeline from its JSON object string
// ─────────────────────────────────────────────────────────────────────────────

static SpritemapSymbol parseSymbol(const std::string& symStr) {
    SpritemapSymbol sym;
    sym.name = jsonGetRaw(symStr, "SN");

    std::string tlObj = jsonGetObject(symStr, "TL");
    if (tlObj.empty()) return sym;
    std::string layersArr = jsonGetArray(tlObj, "L");
    if (layersArr.empty()) return sym;

    forEachObject(layersArr, [&](const std::string& layerStr) {
        SpritemapLayer layer;
        layer.name = jsonGetRaw(layerStr, "LN");

        std::string frArr = jsonGetArray(layerStr, "FR");
        if (frArr.empty()) return;

        forEachObject(frArr, [&](const std::string& frStr) {
            SpritemapKeyframe kf;
            kf.index    = jsonGetInt(frStr, "I");
            kf.duration = jsonGetInt(frStr, "DU");

            std::string elemArr = jsonGetArray(frStr, "E");
            if (!elemArr.empty()) {
                forEachObject(elemArr, [&](const std::string& elemStr) {
                    SpritemapElement elem;

                    // Atlas Sprite Instance (ASI) — leaf node that directly refs atlas piece
                    std::string asiObj = jsonGetObject(elemStr, "ASI");
                    if (!asiObj.empty()) {
                        elem.isAtlasSprite = true;
                        elem.asi.pieceName = jsonGetRaw(asiObj, "N");
                        elem.asi.mx = parseMX(asiObj);
                        kf.elements.push_back(elem);
                        return;
                    }

                    // Symbol Instance (SI) — reference to another symbol
                    std::string siObj = jsonGetObject(elemStr, "SI");
                    if (!siObj.empty()) {
                        elem.isAtlasSprite = false;
                        elem.si.symbolName  = jsonGetRaw(siObj, "SN");
                        elem.si.loopType    = jsonGetRaw(siObj, "LP");
                        elem.si.firstFrame  = jsonGetInt(siObj, "FF");
                        elem.si.mx = parseMX(siObj);
                        elem.si.trp = parseTRP(siObj);
                        kf.elements.push_back(elem);
                    }
                });
            }

            layer.keyframes.push_back(kf);
        });

        sym.layers.push_back(layer);
    });

    return sym;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse Animation.json
// ─────────────────────────────────────────────────────────────────────────────

bool SpritemapParser::parseAnimation(const std::string& path, SpritemapData& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    // Metadata (MD section)
    std::string mdObj = jsonGetObject(src, "MD");
    if (!mdObj.empty()) {
        out.canvasW   = jsonGetFloat(mdObj, "W", 1280.f);
        out.canvasH   = jsonGetFloat(mdObj, "H", 720.f);
        out.frameRate = jsonGetInt(mdObj, "FRT", 24);
    }

    // Main animation (AN section)
    std::string anObj = jsonGetObject(src, "AN");
    out.rootSymbolName = jsonGetRaw(anObj, "SN");
    if (!anObj.empty() && !out.rootSymbolName.empty()) {
        SpritemapSymbol rootSym = parseSymbol(anObj);
        out.symbols[out.rootSymbolName] = std::move(rootSym);
    }

    // Parse named animations from the main timeline (Layer 4 carries labels)
    if (!anObj.empty()) {
        std::string tlObj = jsonGetObject(anObj, "TL");
        if (!tlObj.empty()) {
            std::string layersArr = jsonGetArray(tlObj, "L");
            forEachObject(layersArr, [&](const std::string& layerStr) {
                std::string frArr = jsonGetArray(layerStr, "FR");
                if (frArr.empty()) return;

                // Find root symbol name used in Layer 1 (SI frames)
                // Also collect named keyframes (they have "N" field) as animation labels
                forEachObject(frArr, [&](const std::string& frStr) {
                    std::string animName = jsonGetRaw(frStr, "N");
                    if (!animName.empty()) {
                        SpritemapAnimation anim;
                        anim.name       = animName;
                        anim.startFrame = jsonGetInt(frStr, "I");
                        anim.duration   = jsonGetInt(frStr, "DU");
                        anim.symbolName = out.rootSymbolName;

                        // Try to read loop type from the first SI element if present
                        std::string elemArr = jsonGetArray(frStr, "E");
                        if (!elemArr.empty()) {
                            std::string firstElem;
                            forEachObject(elemArr, [&](const std::string& elem) {
                                if (firstElem.empty()) firstElem = elem;
                            });
                            std::string siObj = jsonGetObject(firstElem, "SI");
                            if (!siObj.empty()) {
                                anim.loopType = jsonGetRaw(siObj, "LP");
                            }
                        }

                        out.animations.push_back(anim);
                    }
                });
            });
        }
    }

    // Symbol definitions (SD section)
    std::string sdObj = jsonGetObject(src, "SD");
    if (sdObj.empty()) return false;
    std::string symbolsArr = jsonGetArray(sdObj, "S");
    if (symbolsArr.empty()) return false;

    forEachObject(symbolsArr, [&](const std::string& symStr) {
        SpritemapSymbol sym = parseSymbol(symStr);
        if (!sym.name.empty()) {
            out.symbols[sym.name] = std::move(sym);
        }
    });

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

bool SpritemapParser::parse(const std::string& atlasJsonPath,
                            const std::string& animJsonPath,
                            SpritemapData& out) {
    out.atlas.clear();
    out.symbols.clear();
    out.animations.clear();

    printf("[JSON] Parsing atlas: %s\n", atlasJsonPath.c_str());
    bool atlasOk = parseAtlas(atlasJsonPath, out);
    if (!atlasOk) {
        printf("[JSON] parseAtlas failed!\n");
    } else {
        printf("[JSON] parseAtlas succeeded with %d pieces\n", (int)out.atlas.size());
    }

    printf("[JSON] Parsing anim: %s\n", animJsonPath.c_str());
    bool animOk  = parseAnimation(animJsonPath, out);
    if (!animOk) {
        printf("[JSON] parseAnimation failed!\n");
    } else {
        printf("[JSON] parseAnimation succeeded\n");
    }

    if (atlasOk && animOk) {
        for (auto& pair : out.symbols) {
            pair.second.duration();
        }
    }

    return atlasOk && animOk;
}
