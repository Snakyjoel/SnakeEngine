#pragma once
#include <citro2d.h>
#include <string>
#include <vector>
#include <map>
#include "SparrowParser.hpp"

struct CachedSpritesheet {
    C2D_SpriteSheet sheet;
    std::vector<C2D_SpriteSheet> subSheets;
    std::vector<Frame> frames;
};

class SpritesheetCache {
public:
    static SpritesheetCache& get() {
        static SpritesheetCache instance;
        return instance;
    }

    CachedSpritesheet* load(const std::string& path) {
        if (cache.count(path)) return &cache[path];

        std::string strippedPath = path;
        std::string library = "";
        if (strippedPath.find("shared/images/") == 0) {
            strippedPath = strippedPath.substr(14);
            library = "shared";
        } else if (strippedPath.find("preload/images/") == 0) {
            strippedPath = strippedPath.substr(15);
            library = "preload";
        } else if (strippedPath.find("images/") == 0) {
            strippedPath = strippedPath.substr(7);
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
                    FILE* fLog = fopen("sdmc:/SnakeEngine/cache_log.txt", "a");
                    if (fLog) {
                        fprintf(fLog, "[CACHE] FAILED to load sub-sheet: %s\n", subPngPath.c_str());
                        fclose(fLog);
                    }
                } else {
                    cs.subSheets.push_back(subSheet);
                }
            }

            if (cs.subSheets.empty()) {
                printf("[CACHE] FAILED to load any sub-sheets for: %s\n", pngPath.c_str());
                FILE* fLog = fopen("sdmc:/SnakeEngine/cache_log.txt", "a");
                if (fLog) {
                    fprintf(fLog, "[CACHE] FAILED to load any sub-sheets for: %s\n", pngPath.c_str());
                    fclose(fLog);
                }
                return nullptr;
            }

            printf("[CACHE] Loaded %d sub-sheets for split texture: %s\n", (int)cs.subSheets.size(), pngPath.c_str());
            fflush(stdout);
            FILE* fLog = fopen("sdmc:/SnakeEngine/cache_log.txt", "a");
            if (fLog) {
                fprintf(fLog, "[CACHE] Loaded %d sub-sheets for split texture: %s\n", (int)cs.subSheets.size(), pngPath.c_str());
                fclose(fLog);
            }

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
                    FILE* fLog = fopen("sdmc:/SnakeEngine/cache_log.txt", "a");
                    if (fLog) {
                        fprintf(fLog, "[CACHE] Frame %s has invalid sheetIdx: %d (sheets size: %d)\n", f.name.c_str(), f.sheetIdx, (int)cs.subSheets.size());
                        fclose(fLog);
                    }
                }
            }

            cache[path] = cs;
            return &cache[path];
        }

        C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(pngPath.c_str());
        if (!sheet) {
            printf("[CACHE] FAILED to load sheet: %s\n", pngPath.c_str());
            return nullptr;
        }

        printf("[CACHE] Loaded sheet: %s, xmlPath: %s (exists: %d)\n", 
               pngPath.c_str(), xmlPath.c_str(), (int)Paths::fileExists(xmlPath));

        cs.sheet = sheet;
        C2D_Image mainImg = C2D_SpriteSheetGetImage(sheet, 0);
        
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
                
                cache[path] = cs;
                return &cache[path];
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

        cache[path] = cs;
        return &cache[path];
    }

    void clear() {
        for (auto& pair : cache) {
            if (pair.second.sheet) {
                C2D_SpriteSheetFree(pair.second.sheet);
            }
            for (auto sub : pair.second.subSheets) {
                if (sub) C2D_SpriteSheetFree(sub);
            }
        }
        cache.clear();
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
                if (it->second.sheet) {
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
};
