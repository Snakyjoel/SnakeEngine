#include "SpritemapParser.hpp"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <functional>
#include <algorithm>

struct StringSpan {
    const char* str = nullptr;
    size_t len = 0;

    StringSpan() = default;
    StringSpan(const char* s, size_t l) : str(s), len(l) {}
    StringSpan(const std::string& s) : str(s.data()), len(s.size()) {}

    bool empty() const { return len == 0 || str == nullptr; }
    char operator[](size_t i) const { return str[i]; }
    std::string toString() const { return empty() ? std::string() : std::string(str, len); }
};

static size_t findKey(StringSpan src, const char* key, size_t startPos = 0) {
    if (src.empty() || startPos >= src.len) return std::string::npos;
    size_t keyLen = strlen(key);
    const char* s = src.str;
    size_t n = src.len;

    for (size_t i = startPos; i + keyLen + 2 <= n; i++) {
        if (s[i] == '"' && memcmp(s + i + 1, key, keyLen) == 0 && s[i + 1 + keyLen] == '"') {
            size_t after = i + 1 + keyLen + 1;
            while (after < n && (s[after] == ' ' || s[after] == '\t' || s[after] == '\r' || s[after] == '\n')) after++;
            if (after < n && s[after] == ':') {
                after++;
                while (after < n && (s[after] == ' ' || s[after] == '\t' || s[after] == '\r' || s[after] == '\n')) after++;
                return after;
            }
            i = after;
        }
    }
    return std::string::npos;
}

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

static StringSpan jsonGetRawSpan(StringSpan src, const char* key) {
    size_t p = findKey(src, key, 0);
    if (p == std::string::npos || p >= src.len) return StringSpan();
    if (src.str[p] == '"') {
        size_t s = p + 1;
        const char* end = (const char*)memchr(src.str + s, '"', src.len - s);
        if (!end) return StringSpan();
        return StringSpan(src.str + s, (size_t)(end - (src.str + s)));
    } else {
        size_t s = p;
        size_t e = s;
        while (e < src.len && src.str[e] != ',' && src.str[e] != '}' && src.str[e] != ']' && src.str[e] != '\n') e++;
        while (e > s && (src.str[e - 1] == ' ' || src.str[e - 1] == '\r' || src.str[e - 1] == '\n' || src.str[e - 1] == '\t')) e--;
        return StringSpan(src.str + s, e - s);
    }
}

static std::string jsonGetRaw(StringSpan src, const char* key) {
    return jsonGetRawSpan(src, key).toString();
}

static float jsonGetFloat(StringSpan src, const char* key, float def = 0.f) {
    StringSpan v = jsonGetRawSpan(src, key);
    if (v.empty()) return def;
    return fast_atof(v.str);
}

static int jsonGetInt(StringSpan src, const char* key, int def = 0) {
    StringSpan v = jsonGetRawSpan(src, key);
    if (v.empty()) return def;
    return (int)fast_atof(v.str);
}

static StringSpan jsonGetObject(StringSpan src, const char* key, size_t startPos = 0) {
    size_t p = findKey(src, key, startPos);
    if (p == std::string::npos || p >= src.len || src.str[p] != '{') return StringSpan();
    int depth = 0;
    size_t s = p;
    for (size_t i = p; i < src.len; i++) {
        if (src.str[i] == '{') depth++;
        else if (src.str[i] == '}') {
            depth--;
            if (depth == 0) return StringSpan(src.str + s, i - s + 1);
        }
    }
    return StringSpan();
}

static StringSpan jsonGetArray(StringSpan src, const char* key, size_t startPos = 0) {
    size_t p = findKey(src, key, startPos);
    if (p == std::string::npos || p >= src.len || src.str[p] != '[') return StringSpan();
    int depth = 0;
    size_t s = p;
    for (size_t i = p; i < src.len; i++) {
        if (src.str[i] == '[') depth++;
        else if (src.str[i] == ']') {
            depth--;
            if (depth == 0) return StringSpan(src.str + s, i - s + 1);
        }
    }
    return StringSpan();
}

static void forEachObject(StringSpan arr, std::function<void(StringSpan)> cb) {
    if (arr.empty()) return;
    int depth = 0;
    size_t start = std::string::npos;
    for (size_t i = 0; i < arr.len; i++) {
        char c = arr.str[i];
        if (c == '{') {
            if (depth == 0) start = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0 && start != std::string::npos) {
                cb(StringSpan(arr.str + start, i - start + 1));
                start = std::string::npos;
            }
        }
    }
}

static AffineMatrix parseMX(StringSpan src, size_t startPos = 0) {
    AffineMatrix m;
    size_t p = findKey(src, "M3D", startPos);
    if (p != std::string::npos && p < src.len && src.str[p] == '[') {
        p++;
        float vals[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        for (int i = 0; i < 16; i++) {
            while (p < src.len && (src.str[p] == ' ' || src.str[p] == ',' || src.str[p] == '\n' || src.str[p] == '\r' || src.str[p] == '\t')) p++;
            if (p >= src.len || src.str[p] == ']') break;
            char* end;
            vals[i] = fast_atof(src.str + p, &end);
            p = (size_t)(end - src.str);
        }
        m.a  = vals[0];
        m.b  = vals[1];
        m.c  = vals[4];
        m.d  = vals[5];
        m.tx = vals[12];
        m.ty = vals[13];
        return m;
    }

    p = findKey(src, "MX", startPos);
    if (p == std::string::npos || p >= src.len || src.str[p] != '[') return m;
    p++;
    float vals[6] = {1,0,0,1,0,0};
    for (int i = 0; i < 6; i++) {
        while (p < src.len && (src.str[p] == ' ' || src.str[p] == ',' || src.str[p] == '\n' || src.str[p] == '\r' || src.str[p] == '\t')) p++;
        if (p >= src.len || src.str[p] == ']') break;
        char* end;
        vals[i] = fast_atof(src.str + p, &end);
        p = (size_t)(end - src.str);
    }
    m.a = vals[0]; m.b = vals[1]; m.c = vals[2]; m.d = vals[3];
    m.tx = vals[4]; m.ty = vals[5];
    return m;
}

static Pivot parseTRP(StringSpan src, size_t startPos = 0) {
    Pivot p2;
    StringSpan trpStr = jsonGetObject(src, "TRP", startPos);
    if (trpStr.empty()) return p2;
    p2.x = jsonGetFloat(trpStr, "x");
    p2.y = jsonGetFloat(trpStr, "y");
    return p2;
}

static float parseAlpha(StringSpan src, size_t startPos = 0) {
    StringSpan cObj = jsonGetObject(src, "C", startPos);
    if (cObj.empty()) return 1.0f;
    return jsonGetFloat(cObj, "AM", 1.0f);
}

static bool readFileString(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return false;
    }
    out.resize((size_t)sz);
    size_t readBytes = fread(&out[0], 1, (size_t)sz, f);
    fclose(f);
    if (readBytes != (size_t)sz) out.resize(readBytes);
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF) {
        out.erase(0, 3);
    }
    return !out.empty();
}

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

bool SpritemapParser::parseAtlas(const std::string& path, SpritemapData& out) {
    std::string src;
    if (!readFileString(path, src)) return false;
    StringSpan srcSpan(src);

    StringSpan atlasObj = jsonGetObject(srcSpan, "ATLAS");
    if (atlasObj.empty()) return false;
    StringSpan spritesArr = jsonGetArray(atlasObj, "SPRITES");
    if (spritesArr.empty()) return false;

    forEachObject(spritesArr, [&](StringSpan item) {
        StringSpan sprObj = jsonGetObject(item, "SPRITE");
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

static SpritemapSymbol parseSymbol(StringSpan symStr) {
    SpritemapSymbol sym;
    sym.name = jsonGetRaw(symStr, "SN");

    StringSpan tlObj = jsonGetObject(symStr, "TL");
    if (tlObj.empty()) return sym;
    StringSpan layersArr = jsonGetArray(tlObj, "L");
    if (layersArr.empty()) return sym;

    forEachObject(layersArr, [&](StringSpan layerStr) {
        SpritemapLayer layer;
        layer.name = jsonGetRaw(layerStr, "LN");
        layer.layerType = jsonGetRaw(layerStr, "LT");
        layer.clippedBy = jsonGetRaw(layerStr, "Clpb");

        StringSpan frArr = jsonGetArray(layerStr, "FR");
        if (frArr.empty()) return;

        forEachObject(frArr, [&](StringSpan frStr) {
            SpritemapKeyframe kf;
            kf.index    = jsonGetInt(frStr, "I");
            kf.duration = jsonGetInt(frStr, "DU");

            StringSpan elemArr = jsonGetArray(frStr, "E");
            if (!elemArr.empty()) {
                forEachObject(elemArr, [&](StringSpan elemStr) {
                    SpritemapElement elem;

                    StringSpan asiObj = jsonGetObject(elemStr, "ASI");
                    if (!asiObj.empty()) {
                        elem.isAtlasSprite = true;
                        elem.asi.pieceName = jsonGetRaw(asiObj, "N");
                        elem.asi.mx = parseMX(asiObj);
                        elem.asi.alpha = parseAlpha(asiObj);
                        kf.elements.push_back(elem);
                        return;
                    }

                    StringSpan siObj = jsonGetObject(elemStr, "SI");
                    if (!siObj.empty()) {
                        elem.isAtlasSprite = false;
                        elem.si.symbolName  = jsonGetRaw(siObj, "SN");
                        elem.si.loopType    = jsonGetRaw(siObj, "LP");
                        elem.si.firstFrame  = jsonGetInt(siObj, "FF");
                        elem.si.mx = parseMX(siObj);
                        elem.si.trp = parseTRP(siObj);
                        elem.si.alpha = parseAlpha(siObj);
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

bool SpritemapParser::parseAnimation(const std::string& path, SpritemapData& out) {
    std::string src;
    if (!readFileString(path, src)) return false;
    StringSpan srcSpan(src);

    StringSpan mdObj = jsonGetObject(srcSpan, "MD");
    if (!mdObj.empty()) {
        out.canvasW = jsonGetFloat(mdObj, "W", 0.f);
        if (out.canvasW == 0.f) out.canvasW = jsonGetFloat(mdObj, "w", 1280.f);
        out.canvasH = jsonGetFloat(mdObj, "H", 0.f);
        if (out.canvasH == 0.f) out.canvasH = jsonGetFloat(mdObj, "h", 720.f);
        out.frameRate = jsonGetInt(mdObj, "FRT", 24);
        if (out.frameRate <= 0) out.frameRate = 24;
    }

    StringSpan anObj = jsonGetObject(srcSpan, "AN");
    out.rootSymbolName = jsonGetRaw(anObj, "SN");
    if (!anObj.empty() && !out.rootSymbolName.empty()) {
        SpritemapSymbol rootSym = parseSymbol(anObj);
        out.symbols[out.rootSymbolName] = std::move(rootSym);
    }

    if (!anObj.empty()) {
        StringSpan tlObj = jsonGetObject(anObj, "TL");
        if (!tlObj.empty()) {
            StringSpan layersArr = jsonGetArray(tlObj, "L");
            forEachObject(layersArr, [&](StringSpan layerStr) {
                StringSpan frArr = jsonGetArray(layerStr, "FR");
                if (frArr.empty()) return;

                forEachObject(frArr, [&](StringSpan frStr) {
                    std::string animName = jsonGetRaw(frStr, "N");
                    if (!animName.empty()) {
                        SpritemapAnimation anim;
                        anim.name       = animName;
                        anim.startFrame = jsonGetInt(frStr, "I");
                        anim.duration   = jsonGetInt(frStr, "DU");
                        anim.symbolName = out.rootSymbolName;

                        StringSpan elemArr = jsonGetArray(frStr, "E");
                        if (!elemArr.empty()) {
                            StringSpan firstElem;
                            forEachObject(elemArr, [&](StringSpan elem) {
                                if (firstElem.empty()) firstElem = elem;
                            });
                            StringSpan siObj = jsonGetObject(firstElem, "SI");
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

    StringSpan sdObj = jsonGetObject(srcSpan, "SD");
    if (sdObj.empty()) return false;
    StringSpan symbolsArr = jsonGetArray(sdObj, "S");
    if (symbolsArr.empty()) return false;

    forEachObject(symbolsArr, [&](StringSpan symStr) {
        SpritemapSymbol sym = parseSymbol(symStr);
        if (!sym.name.empty()) {
            out.symbols[sym.name] = std::move(sym);
        }
    });

    return true;
}

static AffineMatrix composeMX(const AffineMatrix& p, const AffineMatrix& c) {
    AffineMatrix r;
    r.a  = c.a * p.a  + c.b * p.c;
    r.b  = c.a * p.b  + c.b * p.d;
    r.c  = c.c * p.a  + c.d * p.c;
    r.d  = c.c * p.b  + c.d * p.d;
    r.tx = c.tx * p.a + c.ty * p.c + p.tx;
    r.ty = c.tx * p.b + c.ty * p.d + p.ty;
    return r;
}

static void computeSymbolBounds(const SpritemapData& data,
                                const std::string& symName,
                                int frame,
                                const AffineMatrix& parentMX,
                                int depth,
                                float& outMinX, float& outMinY,
                                float& outMaxX, float& outMaxY,
                                bool& found) {
    if (depth > 6) return;
    auto sit = data.symbols.find(symName);
    if (sit == data.symbols.end()) return;
    const SpritemapSymbol& sym = sit->second;
    int totalDur = sym.duration();
    if (totalDur <= 0) return;
    int curFrame = frame % totalDur;

    for (size_t li = 0; li < sym.layers.size(); li++) {
        if (sym.layers[li].layerType == "Clp") continue;
        const SpritemapKeyframe* kf = sym.getKeyframe((int)li, curFrame);
        if (!kf) continue;
        for (const auto& elem : kf->elements) {
            if (elem.isAtlasSprite) {
                if (elem.asi.alpha <= 0.001f) continue;
                auto pit = data.atlas.find(elem.asi.pieceName);
                if (pit != data.atlas.end()) {
                    const SpritemapPiece& p = pit->second;
                    AffineMatrix m = composeMX(parentMX, elem.asi.mx);
                    float w = (p.rotated ? (float)p.h : (float)p.w);
                    float h = (p.rotated ? (float)p.w : (float)p.h);
                    float cornersX[4] = { 0, w, 0, w };
                    float cornersY[4] = { 0, 0, h, h };
                    for (int i = 0; i < 4; i++) {
                        float tx = m.a * cornersX[i] + m.c * cornersY[i] + m.tx;
                        float ty = m.b * cornersX[i] + m.d * cornersY[i] + m.ty;
                        if (!found) {
                            outMinX = outMaxX = tx;
                            outMinY = outMaxY = ty;
                            found = true;
                        } else {
                            if (tx < outMinX) outMinX = tx;
                            if (tx > outMaxX) outMaxX = tx;
                            if (ty < outMinY) outMinY = ty;
                            if (ty > outMaxY) outMaxY = ty;
                        }
                    }
                }
            } else {
                if (elem.si.alpha <= 0.001f) continue;
                AffineMatrix childMX = composeMX(parentMX, elem.si.mx);
                computeSymbolBounds(data, elem.si.symbolName, elem.si.firstFrame, childMX, depth + 1,
                                    outMinX, outMinY, outMaxX, outMaxY, found);
            }
        }
    }
}

bool SpritemapParser::parse(const std::string& atlasJsonPath,
                            const std::string& animJsonPath,
                            SpritemapData& out) {
    out.atlas.clear();
    out.symbols.clear();
    out.animations.clear();
    out.hasVisualBounds = false;

    bool atlasOk = parseAtlas(atlasJsonPath, out);
    bool animOk  = parseAnimation(animJsonPath, out);

    if (atlasOk && animOk) {
        for (auto& pair : out.symbols) {
            pair.second.duration();
        }

        auto rootIt = out.symbols.find(out.rootSymbolName);
        if (rootIt != out.symbols.end()) {
            float minX = 0, minY = 0, maxX = 0, maxY = 0;
            bool found = false;
            AffineMatrix id;
            int dur = rootIt->second.duration();
            int step = dur > 30 ? (dur / 15) : 1;
            if (step < 1) step = 1;
            for (int f = 0; f < dur; f += step) {
                computeSymbolBounds(out, out.rootSymbolName, f, id, 0, minX, minY, maxX, maxY, found);
            }
            if (found) {
                out.minX = minX;
                out.minY = minY;
                out.maxX = maxX;
                out.maxY = maxY;
                out.centerX = (minX + maxX) * 0.5f;
                out.centerY = (minY + maxY) * 0.5f;
                out.hasVisualBounds = true;
            }
        }
    }

    return atlasOk && animOk;
}
