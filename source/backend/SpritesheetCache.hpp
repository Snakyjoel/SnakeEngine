#pragma once
#include <citro2d.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "SparrowParser.hpp"
#include "TextureLoader.hpp"

struct CachedSpritesheet {
    C2D_SpriteSheet sheet = nullptr;
    std::vector<C2D_SpriteSheet> subSheets;
    std::vector<Frame> frames;
    C3D_Tex* rawTex = nullptr;
    Tex3DS_SubTexture* rawSub = nullptr;
    void* vramData = nullptr;
};

class SpritesheetCache {
public:
    static SpritesheetCache& get() {
        static SpritesheetCache instance;
        return instance;
    }

    static std::string normalizeKey(const std::string& path) {
        std::string stripped = path;
        if (stripped.find("shared/images/") != std::string::npos) {
            size_t idx = stripped.find("shared/images/");
            stripped = stripped.substr(idx + 14);
        } else if (stripped.find("preload/images/") != std::string::npos) {
            size_t idx = stripped.find("preload/images/");
            stripped = stripped.substr(idx + 15);
        } else if (stripped.find("images/") == 0) {
            stripped = stripped.substr(7);
        }
        return stripped;
    }

    CachedSpritesheet* load(const std::string& path) {
        std::string cleanKey = normalizeKey(path);
        if (cache.count(cleanKey)) return &cache[cleanKey];

        std::string strippedPath = cleanKey;
        std::string library = "";
        if (path.find("shared/images/") != std::string::npos) {
            library = "shared";
        } else if (path.find("preload/images/") != std::string::npos) {
            library = "preload";
        }

        std::string pngPath = Paths::image(strippedPath, library);
        std::string xmlPath = Paths::xml(strippedPath, library);

        if (pngPath.empty() || !Paths::fileExists(pngPath)) pngPath = "romfs:/" + path + ".t3x";
        if (xmlPath.empty() || !Paths::fileExists(xmlPath)) xmlPath = "romfs:/" + path + ".xml";

        CachedSpritesheet cs;
        bool isSplit = false;
        int atlasW = 0, atlasH = 0;
        
        int cols = 1;
        int rows = 1;
        if (Paths::fileExists(xmlPath)) {
            SparrowParser::parseXml(xmlPath, cs.frames, &atlasW, &atlasH, &isSplit, &cols, &rows);
        }

        if (isSplit) {
            cs.sheet = nullptr;
            int numSheets = cols * rows;

            for (int i = 0; i < numSheets; ++i) {
                std::string subPngPath = Paths::image(strippedPath + "_" + std::to_string(i), library);
                if (subPngPath.empty() || !Paths::fileExists(subPngPath)) {
                    subPngPath = "romfs:/" + path + "_" + std::to_string(i) + ".t3x";
                    if (!Paths::fileExists(subPngPath)) {
                        subPngPath = "romfs:/" + path + "_" + std::to_string(i) + ".rawtex";
                    }
                }

                C2D_SpriteSheet subSheet = C2D_SpriteSheetLoad(subPngPath.c_str());
                if (!subSheet) {
                    printf("[CACHE] FAILED to load sub-sheet: %s\n", subPngPath.c_str());
                } else {
                    cs.subSheets.push_back(subSheet);
                }
            }

            if (cs.subSheets.empty()) {
                printf("[CACHE] FAILED to load any sub-sheets for: %s\n", pngPath.c_str());
                return nullptr;
            }

            printf("[CACHE] Loaded %d sub-sheets for split texture: %s\n", (int)cs.subSheets.size(), pngPath.c_str());
            fflush(stdout);

            for (auto& f : cs.frames) {
                if (f.sheetIdx >= 0 && f.sheetIdx < (int)cs.subSheets.size()) {
                    C2D_Image subImg = C2D_SpriteSheetGetImage(cs.subSheets[f.sheetIdx], 0);
                    f.tex = subImg.tex;
                    if (!f.tex) {
                        printf("[CACHE] Frame %s resolved to NULL texture (sheetIdx: %d)\n", f.name.c_str(), f.sheetIdx);
                        fflush(stdout);
                    }
                    
                    static Tex3DS_SubTexture defaultSubtex;
                    defaultSubtex.width = subImg.tex ? subImg.tex->width : 0;
                    defaultSubtex.height = subImg.tex ? subImg.tex->height : 0;
                    defaultSubtex.left = 0.0f;
                    defaultSubtex.top = 0.0f;
                    defaultSubtex.right = 1.0f;
                    defaultSubtex.bottom = 1.0f;
                    
                    const Tex3DS_SubTexture* sub = subImg.subtex ? subImg.subtex : &defaultSubtex;
                    
                    float rw = sub->right - sub->left;
                    float rh = sub->bottom - sub->top;
                    
                    float refW = (float)sub->width;
                    float refH = (float)sub->height;
                    
                    f.uv.width  = (u16)f.w;
                    f.uv.height = (u16)f.h;
                    f.uv.left   = sub->left + ((float)f.x       * rw / refW);
                    f.uv.top    = sub->top  + ((float)f.y       * rh / refH);
                    f.uv.right  = sub->left + ((float)(f.x+f.w) * rw / refW);
                    f.uv.bottom = sub->top  + ((float)(f.y+f.h) * rh / refH);
                } else {
                    f.tex = nullptr;
                    printf("[CACHE] Frame %s has invalid sheetIdx: %d (sheets size: %d)\n", f.name.c_str(), f.sheetIdx, (int)cs.subSheets.size());
                    fflush(stdout);
                }
            }

            cache[cleanKey] = cs;
            return &cache[cleanKey];
        }

        C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(pngPath.c_str());
        if (!sheet && pngPath.find(".png") == std::string::npos) {
            std::string resolvedPng = pngPath;
            size_t dot = resolvedPng.find_last_of('.');
            if (dot != std::string::npos) resolvedPng = resolvedPng.substr(0, dot) + ".png";
            sheet = C2D_SpriteSheetLoad(resolvedPng.c_str());
        }

        C2D_Image mainImg;
        mainImg.tex = nullptr;
        mainImg.subtex = nullptr;

        if (sheet) {
            cs.sheet = sheet;
            mainImg = C2D_SpriteSheetGetImage(sheet, 0);
        } else {
            // Direct PNG fallback: try loading via TextureLoader
            std::string resolvedPng = pngPath;
            if (resolvedPng.find(".png") == std::string::npos) {
                size_t dot = resolvedPng.find_last_of('.');
                if (dot != std::string::npos) resolvedPng = resolvedPng.substr(0, dot) + ".png";
            }
            if (TextureLoader::loadPng(resolvedPng, &cs.rawTex, &cs.rawSub)) {
                mainImg.tex = cs.rawTex;
                mainImg.subtex = cs.rawSub;
            } else {
                printf("[CACHE] FAILED to load sheet or PNG: %s\n", pngPath.c_str());
                return nullptr;
            }
        }

        printf("[CACHE] Loaded asset: %s, xmlPath: %s (exists: %d)\n", 
               pngPath.c_str(), xmlPath.c_str(), (int)Paths::fileExists(xmlPath));
        
        static Tex3DS_SubTexture defaultSubtex;
        defaultSubtex.width = mainImg.tex ? mainImg.tex->width : 0;
        defaultSubtex.height = mainImg.tex ? mainImg.tex->height : 0;
        defaultSubtex.left = 0.0f;
        defaultSubtex.top = 0.0f;
        defaultSubtex.right = 1.0f;
        defaultSubtex.bottom = 1.0f;
        
        const Tex3DS_SubTexture* sub = mainImg.subtex ? mainImg.subtex : &defaultSubtex;

        if (cs.frames.empty()) {
            if (!Paths::fileExists(xmlPath)) {
                Frame f;
                f.name = "default";
                f.x = 0;
                f.y = 0;
                f.w = sub->width;
                f.h = sub->height;
                f.frameX = 0;
                f.frameY = 0;
                f.frameW = sub->width;
                f.frameH = sub->height;
                f.rotated = false;
                f.tex = (C3D_Tex*)mainImg.tex;
                f.uv = *sub;
                cs.frames.push_back(f);
                
                cache[cleanKey] = cs;
                return &cache[cleanKey];
            } else {
                SparrowParser::parseXml(xmlPath, cs.frames, &atlasW, &atlasH, &isSplit);
            }
        }
        
        if (cs.frames.empty()) {
            printf("\x1b[16;1HFES ERROR: No frames in XML: %s\x1b[K\n", xmlPath.c_str());
            C2D_SpriteSheetFree(sheet);
            return nullptr;
        }
        
        float refW = (atlasW > 0) ? (float)atlasW : (float)sub->width;
        float refH = (atlasH > 0) ? (float)atlasH : (float)sub->height;

        float rw = sub->right - sub->left;
        float rh = sub->bottom - sub->top;

        for (auto& f : cs.frames) {
            f.tex = mainImg.tex;
            f.uv.width  = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left   = sub->left + ((float)f.x       * rw / refW);
            f.uv.top    = sub->top  + ((float)f.y       * rh / refH);
            f.uv.right  = sub->left + ((float)(f.x+f.w) * rw / refW);
            f.uv.bottom = sub->top  + ((float)(f.y+f.h) * rh / refH);
        }

        printf("[CACHE] Sparrow XML parsed. Frame count: %d\n", (int)cs.frames.size());

        cache[cleanKey] = cs;
        return &cache[cleanKey];
    }
    const std::map<std::string, CachedSpritesheet>& getCache() const { return cache; }

    void pin(const std::string& path) {
        pinnedKeys.insert(normalizeKey(path));
    }

    bool isPinned(C2D_SpriteSheet sheet) const {
        if (!sheet) return false;
        for (const auto& pair : cache) {
            if (pair.second.sheet == sheet && pinnedKeys.count(pair.first)) {
                return true;
            }
        }
        return false;
    }

    bool isPinnedKey(const std::string& path) const {
        return pinnedKeys.count(normalizeKey(path)) > 0;
    }

    void clear() {
        for (auto& pair : cache) {
            if (pair.second.sheet) {
                if (pair.second.vramData) {
                    C3D_Tex* tex = C2D_SpriteSheetGetImage(pair.second.sheet, 0).tex;
                    if (tex) tex->data = nullptr;
                    vramFree(pair.second.vramData);
                    pair.second.vramData = nullptr;
                }
                C2D_SpriteSheetFree(pair.second.sheet);
            }
            for (auto sub : pair.second.subSheets) {
                if (sub) C2D_SpriteSheetFree(sub);
            }
            if (pair.second.rawTex) {
                if (pair.second.vramData) {
                    pair.second.rawTex->data = nullptr;
                    vramFree(pair.second.vramData);
                    pair.second.vramData = nullptr;
                }
                C3D_TexDelete(pair.second.rawTex);
                delete pair.second.rawTex;
                pair.second.rawTex = nullptr;
            }
            if (pair.second.rawSub) {
                delete pair.second.rawSub;
                pair.second.rawSub = nullptr;
            }
        }
        cache.clear();
        pinnedKeys.clear();
    }

    void updateVram(C2D_SpriteSheet sheet, C3D_Tex* tex, void* vramData) {
        for (auto& pair : cache) {
            if ((sheet && pair.second.sheet == sheet) || (tex && pair.second.rawTex == tex)) {
                pair.second.vramData = vramData;
                for (auto& f : pair.second.frames) {
                    if (f.tex) {
                        f.tex->data = vramData;
                    }
                }
            }
        }
    }

    bool contains(C2D_SpriteSheet sheet) {
        for (auto& pair : cache) {
            if (pair.second.sheet == sheet) return true;
        }
        return false;
    }

    bool unloadBySheet(C2D_SpriteSheet sheet) {
        if (!sheet) return false;
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.sheet == sheet) {
                if (pinnedKeys.count(it->first)) {
                    return false;
                }
                if (it->second.sheet) {
                    if (it->second.vramData) {
                        C3D_Tex* tex = C2D_SpriteSheetGetImage(it->second.sheet, 0).tex;
                        if (tex) tex->data = nullptr;
                        vramFree(it->second.vramData);
                        it->second.vramData = nullptr;
                    }
                    C2D_SpriteSheetFree(it->second.sheet);
                }
                for (auto sub : it->second.subSheets) {
                    if (sub) C2D_SpriteSheetFree(sub);
                }
                cache.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    SpritesheetCache() = default;
    std::map<std::string, CachedSpritesheet> cache;
    std::set<std::string> pinnedKeys;
};
