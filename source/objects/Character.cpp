#include "Character.hpp"
#include "../backend/AsyncAssetManager.hpp"
#include "../states/PlayState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/Conductor.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <jansson.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct CachedCharacterTexture {
    C3D_Tex* tex;
    Tex3DS_SubTexture* subtex;
    int refCount;
};
static std::map<std::string, CachedCharacterTexture> charTextureCache;


Character::Character() {
    animations.clear();
    frames.clear();
    sheet = nullptr;
    rawTex = nullptr;
    rawSub = nullptr;
    mainImage.tex = nullptr;
    mainImage.subtex = nullptr;
    currentAnimData = nullptr;
    fileBuffer = nullptr;
    charTexturePath = "";
    curFrame = 0;
    frameTimer = 0;
    animFinished = false;
    visible = true;
}

Character::~Character() {
    if (sheet) C2D_SpriteSheetFree(sheet);
    
    if (!charTexturePath.empty() && charTextureCache.count(charTexturePath)) {
        charTextureCache[charTexturePath].refCount--;
        if (charTextureCache[charTexturePath].refCount <= 0) {
            C3D_Tex* t = charTextureCache[charTexturePath].tex;
            Tex3DS_SubTexture* s = charTextureCache[charTexturePath].subtex;
            if (t) {
                C3D_TexDelete(t);
                delete t;
            }
            if (s) delete s;
            charTextureCache.erase(charTexturePath);
        }
    } else {
        if (rawTex) {
            C3D_TexDelete(rawTex);
            delete rawTex;
        }
        if (rawSub) delete rawSub;
    }
    if (fileBuffer) linearFree(fileBuffer);
}

void Character::loadSparrowXml(const std::string& xmlPath) {
    SparrowParser::parseXml(xmlPath, frames);
}

#include <sys/stat.h>
#include <string.h>

static void ensureDirExists(const std::string& path) {
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        std::string dir = path.substr(0, pos);
        struct stat st;
        if (stat(dir.c_str(), &st) != 0) {
            mkdir(dir.c_str(), 0777);
        }
    }
}

static bool readChunked(FILE* f, void* buffer, size_t size) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = size;
    const size_t CHUNK_SIZE = 128 * 1024;
    while (remaining > 0) {
        size_t toRead = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        if (fread(ptr, 1, toRead, f) != toRead) {
            return false;
        }
        ptr += toRead;
        remaining -= toRead;
        
        size_t loaded = size - remaining;
        AsyncAssetManager::get().loadingAssetPercent = (int)((loaded * 100) / size);

        if (remaining > 0) {
            svcSleepThread(1000000LL);
        }
    }
    return true;
}

bool Character::loadFromCache(const std::string& path, CharacterData* data) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    CharCacheHeader header;
    if (fread(&header, sizeof(CharCacheHeader), 1, f) != 1) {
        fclose(f);
        return false;
    }
    if (strncmp(header.magic, "CHAR", 4) != 0 || header.version != 2) {
        fclose(f);
        return false;
    }

    data->charScale = header.charScale;
    data->flipX = header.flipX;
    data->noAntialiasing = header.noAntialiasing;
    data->healthIcon = header.healthIcon;
    data->healthbarR = header.healthbarR;
    data->healthbarG = header.healthbarG;
    data->healthbarB = header.healthbarB;
    data->camOffsetX = header.camOffsetX;
    data->camOffsetY = header.camOffsetY;
    data->baseX = header.baseX;
    data->baseY = header.baseY;
    data->singDuration = header.singDuration;
    data->danceEveryNumBeats = header.danceEveryNumBeats;
    
    data->frames.resize(header.numFrames);
    for (uint32_t i = 0; i < header.numFrames; i++) {
        FrameBin fb;
        fread(&fb, sizeof(FrameBin), 1, f);
        Frame fr;
        fr.name = fb.name;
        fr.x = fb.x; fr.y = fb.y; fr.w = fb.w; fr.h = fb.h;
        fr.frameX = fb.frameX; fr.frameY = fb.frameY; fr.frameW = fb.frameW; fr.frameH = fb.frameH;
        fr.rotated = fb.rotated;
        fr.index = (int)i;
        data->frames[i] = fr;
    }

    for (uint32_t i = 0; i < header.numAnimations; i++) {
        AnimBin ab;
        fread(&ab, sizeof(AnimBin), 1, f);
        Animation anim;
        anim.name = ab.name;
        anim.prefix = ab.prefix;
        anim.fps = ab.fps;
        anim.loop = ab.loop;
        if (anim.name == "danceLeft" || anim.name == "danceRight" || anim.name == "idle") {
            anim.loop = false;
        }
        anim.offsetX = ab.offsetX;
        anim.offsetY = ab.offsetY;
        
        anim.indices.resize(ab.numIndices);
        if (ab.numIndices > 0) {
            fread(anim.indices.data(), sizeof(int), ab.numIndices, f);
        }
        data->animations[anim.name] = anim;
    }
    
    data->isRawTex = header.isRawTex;
    std::string fullPath = header.imagePath;

    std::string rawPath = ModHandler::get().getModPath("images/" + fullPath + ".rawtex");
    if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + fullPath + ".rawtex")) {
        rawPath = "romfs:/preload/images/" + fullPath + ".rawtex";
    }
    if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + fullPath + ".rawtex")) {
        rawPath = "romfs:/shared/images/" + fullPath + ".rawtex";
    }

    if (data->isRawTex && Paths::fileExists(rawPath)) {
        FILE* ft = fopen(rawPath.c_str(), "rb");
        if (ft) {
            struct RawTexHdr { char magic[4]; uint16_t w; uint16_t h; uint16_t ow; uint16_t oh; } rtex;
            fread(&rtex, sizeof(RawTexHdr), 1, ft);
            data->rawWidth = header.rawWidth;
            data->rawHeight = header.rawHeight;
            data->rawOrigW = header.rawOrigW;
            data->rawOrigH = header.rawOrigH;
            data->fileSize = header.fileSize;
            data->fileBuffer = linearAlloc(data->fileSize);
            if (data->fileBuffer) {
                readChunked(ft, data->fileBuffer, data->fileSize);
                data->rawTex = new C3D_Tex();
                memset(data->rawTex, 0, sizeof(C3D_Tex));
                if (C3D_TexInit(data->rawTex, data->rawWidth, data->rawHeight, GPU_RGBA8)) {
                    if (data->rawTex->data) linearFree(data->rawTex->data);
                    data->rawTex->data = data->fileBuffer;
                    GSPGPU_FlushDataCache(data->rawTex->data, data->fileSize);
                    data->fileBuffer = nullptr;
                    
                    data->rawSub = new Tex3DS_SubTexture();
                    data->rawSub->width = data->rawOrigW;
                    data->rawSub->height = data->rawOrigH;
                    data->rawSub->left = 0.0f;
                    data->rawSub->top = 1.0f;
                    data->rawSub->right = (float)data->rawOrigW / data->rawWidth;
                    data->rawSub->bottom = 1.0f - ((float)data->rawOrigH / data->rawHeight);
                    data->isRawTex = false;
                } else {
                    delete data->rawTex;
                    data->rawTex = nullptr;
                    linearFree(data->fileBuffer);
                    data->fileBuffer = nullptr;
                }
            }
            fclose(ft);
        }
    } else {
        std::string t3xPath = Paths::image(fullPath);
        if (Paths::fileExists(t3xPath)) {
            FILE* ft = fopen(t3xPath.c_str(), "rb");
            if (ft) {
                fseek(ft, 0, SEEK_END);
                data->fileSize = ftell(ft);
                fseek(ft, 0, SEEK_SET);
                data->fileBuffer = linearAlloc(data->fileSize);
                if (data->fileBuffer) readChunked(ft, data->fileBuffer, data->fileSize);
                data->isRawTex = false;
                fclose(ft);
                
                if (data->fileBuffer && data->fileSize > 0) {
                    data->rawTex = new C3D_Tex();
                    memset(data->rawTex, 0, sizeof(C3D_Tex));
                    Tex3DS_Texture t3x = Tex3DS_TextureImport(data->fileBuffer, data->fileSize, data->rawTex, nullptr, false);
                    if (t3x) {
                        const Tex3DS_SubTexture* sub = Tex3DS_GetSubTexture(t3x, 0);
                        data->rawSub = new Tex3DS_SubTexture();
                        if (sub) {
                            *data->rawSub = *sub;
                        } else {
                            data->rawSub->width = data->rawTex->width;
                            data->rawSub->height = data->rawTex->height;
                            data->rawSub->left = 0.0f;
                            data->rawSub->top = 1.0f;
                            data->rawSub->right = 1.0f;
                            data->rawSub->bottom = 0.0f;
                        }
                        Tex3DS_TextureFree(t3x);
                    } else {
                        delete data->rawTex;
                        data->rawTex = nullptr;
                    }
                    linearFree(data->fileBuffer);
                    data->fileBuffer = nullptr;
                    data->fileSize = 0;
                }
            }
        }
    }

    if (!data->fileBuffer && !data->rawTex) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

void Character::saveToCache(const std::string& path, CharacterData* data, const std::string& imagePath) {
    ensureDirExists(path);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;

    CharCacheHeader header;
    memset(&header, 0, sizeof(CharCacheHeader));
    memcpy(header.magic, "CHAR", 4);
    header.version = 2;
    header.charScale = data->charScale;
    header.flipX = data->flipX;
    header.noAntialiasing = data->noAntialiasing;
    strncpy(header.healthIcon, data->healthIcon.c_str(), 31);
    header.healthbarR = data->healthbarR;
    header.healthbarG = data->healthbarG;
    header.healthbarB = data->healthbarB;
    header.camOffsetX = data->camOffsetX;
    header.camOffsetY = data->camOffsetY;
    header.baseX = data->baseX;
    header.baseY = data->baseY;
    header.singDuration = data->singDuration;
    header.danceEveryNumBeats = data->danceEveryNumBeats;
    strncpy(header.imagePath, imagePath.c_str(), 127);
    
    header.numFrames = (uint32_t)data->frames.size();
    header.numAnimations = (uint32_t)data->animations.size();
    header.isRawTex = data->isRawTex;
    header.fileSize = data->fileSize;
    header.rawWidth = data->rawWidth;
    header.rawHeight = data->rawHeight;
    header.rawOrigW = data->rawOrigW;
    header.rawOrigH = data->rawOrigH;

    fwrite(&header, sizeof(CharCacheHeader), 1, f);

    for (const auto& fr : data->frames) {
        FrameBin fb;
        memset(&fb, 0, sizeof(FrameBin));
        strncpy(fb.name, fr.name.c_str(), 127);
        fb.x = fr.x; fb.y = fr.y; fb.w = fr.w; fb.h = fr.h;
        fb.frameX = fr.frameX; fb.frameY = fr.frameY; fb.frameW = fr.frameW; fb.frameH = fr.frameH;
        fb.rotated = fr.rotated;
        fwrite(&fb, sizeof(FrameBin), 1, f);
    }

    for (const auto& pair : data->animations) {
        const Animation& anim = pair.second;
        AnimBin ab;
        memset(&ab, 0, sizeof(AnimBin));
        strncpy(ab.name, anim.name.c_str(), 63);
        strncpy(ab.prefix, anim.prefix.c_str(), 63);
        ab.fps = anim.fps;
        ab.loop = anim.loop;
        ab.offsetX = anim.offsetX;
        ab.offsetY = anim.offsetY;
        ab.numIndices = (uint32_t)anim.indices.size();
        fwrite(&ab, sizeof(AnimBin), 1, f);
        
        if (ab.numIndices > 0) {
            fwrite(anim.indices.data(), sizeof(int), ab.numIndices, f);
        }
    }

    fclose(f);
}

CharacterData* Character::parseDataAsync(const std::string& charName) {
    CharacterData* data = new CharacterData();
    data->charName = charName;
    
    std::string cachePath = Paths::characterCache(charName);
    if (Paths::fileExists(cachePath)) {
        if (loadFromCache(cachePath, data)) {
            return data;
        }
    }
    
    std::string jsonPath = Paths::characterJson(charName);
    if (!Paths::fileExists(jsonPath)) {
        printf("\x1b[14;1HWARN: Character '%s' not found. Using Placeholder.\x1b[K\n", charName.c_str());
        data->isPlaceholder = true;
        jsonPath = Paths::characterJson("bf-pixel");
    }

    json_error_t error;
    json_t* root = json_load_file(jsonPath.c_str(), 0, &error);
    if (!root) {
        printf("\x1b[14;1HERROR JSON: %s\x1b[K\n", jsonPath.c_str());
        delete data;
        return nullptr;
    }

    json_t *jsonScale = json_object_get(root, "scale");
    if (json_is_number(jsonScale)) data->charScale = (float)json_number_value(jsonScale);

    json_t *jsonFlip = json_object_get(root, "flip_x");
    if (json_is_boolean(jsonFlip)) data->flipX = json_boolean_value(jsonFlip);

    json_t *jsonNoAA = json_object_get(root, "no_antialiasing");
    if (json_is_boolean(jsonNoAA)) data->noAntialiasing = json_boolean_value(jsonNoAA);

    json_t *jsonIcon = json_object_get(root, "healthicon");
    if (json_is_string(jsonIcon)) data->healthIcon = json_string_value(jsonIcon);

    json_t *jsonHBC = json_object_get(root, "healthbar_colors");
    if (json_is_array(jsonHBC) && json_array_size(jsonHBC) >= 3) {
        data->healthbarR = (float)json_number_value(json_array_get(jsonHBC, 0)) / 255.0f;
        data->healthbarG = (float)json_number_value(json_array_get(jsonHBC, 1)) / 255.0f;
        data->healthbarB = (float)json_number_value(json_array_get(jsonHBC, 2)) / 255.0f;
    }

    json_t *jsonCam = json_object_get(root, "camera_position");
    if (json_is_array(jsonCam) && json_array_size(jsonCam) >= 2) {
        data->camOffsetX = (float)json_number_value(json_array_get(jsonCam, 0));
        data->camOffsetY = (float)json_number_value(json_array_get(jsonCam, 1));
    }
    json_t *jsonPos = json_object_get(root, "position");
    if (json_is_array(jsonPos) && json_array_size(jsonPos) >= 2) {
        data->baseX = (float)json_number_value(json_array_get(jsonPos, 0));
        data->baseY = (float)json_number_value(json_array_get(jsonPos, 1));
    }

    std::string fullPath = "";
    json_t *jsonImage = json_object_get(root, "image");
    if (json_is_string(jsonImage)) {
        fullPath = json_string_value(jsonImage);
        SparrowParser::parseXml(Paths::xml(fullPath), data->frames);
        
        std::string t3xPath = Paths::image(fullPath);
        std::string rawPath = ModHandler::get().getModPath("images/" + fullPath + ".rawtex");
        if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + fullPath + ".rawtex")) {
            rawPath = "romfs:/preload/images/" + fullPath + ".rawtex";
        }
        if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + fullPath + ".rawtex")) {
            rawPath = "romfs:/shared/images/" + fullPath + ".rawtex";
        }

        if (Paths::fileExists(rawPath)) {
            FILE* f = fopen(rawPath.c_str(), "rb");
            if (f) {
                struct RawTexHeader { char magic[4]; uint16_t w; uint16_t h; uint16_t ow; uint16_t oh; } header;
                if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                    data->rawWidth = header.w;
                    data->rawHeight = header.h;
                    data->rawOrigW = header.ow;
                    data->rawOrigH = header.oh;
                    data->fileSize = (size_t)header.w * header.h * 4;
                    data->fileBuffer = linearAlloc(data->fileSize);
                    if (data->fileBuffer) {
                        readChunked(f, data->fileBuffer, data->fileSize);
                        data->rawTex = new C3D_Tex();
                        memset(data->rawTex, 0, sizeof(C3D_Tex));
                        if (C3D_TexInit(data->rawTex, header.w, header.h, GPU_RGBA8)) {
                            if (data->rawTex->data) linearFree(data->rawTex->data);
                            data->rawTex->data = data->fileBuffer;
                            GSPGPU_FlushDataCache(data->rawTex->data, data->fileSize);
                            data->fileBuffer = nullptr;
                            
                            data->rawSub = new Tex3DS_SubTexture();
                            data->rawSub->width = header.ow;
                            data->rawSub->height = header.oh;
                            data->rawSub->left = 0.0f;
                            data->rawSub->top = 1.0f;
                            data->rawSub->right = (float)header.ow / header.w;
                            data->rawSub->bottom = 1.0f - ((float)header.oh / header.h);
                            data->isRawTex = false;
                        } else {
                            delete data->rawTex;
                            data->rawTex = nullptr;
                            linearFree(data->fileBuffer);
                            data->fileBuffer = nullptr;
                        }
                    }
                }
                fclose(f);
            }
        } else if (Paths::fileExists(t3xPath)) {
            FILE* f = fopen(t3xPath.c_str(), "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                data->fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);
                void* tempBuf = linearAlloc(data->fileSize);
                if (tempBuf) {
                    readChunked(f, tempBuf, data->fileSize);
                    data->isRawTex = false;
                    data->fileBuffer = tempBuf;
                    
                    if (data->fileBuffer && data->fileSize > 0) {
                        data->rawTex = new C3D_Tex();
                        memset(data->rawTex, 0, sizeof(C3D_Tex));
                        Tex3DS_Texture t3x = Tex3DS_TextureImport(data->fileBuffer, data->fileSize, data->rawTex, nullptr, false);
                        if (t3x) {
                            const Tex3DS_SubTexture* sub = Tex3DS_GetSubTexture(t3x, 0);
                            data->rawSub = new Tex3DS_SubTexture();
                            if (sub) {
                                *data->rawSub = *sub;
                            } else {
                                data->rawSub->width = data->rawTex->width;
                                data->rawSub->height = data->rawTex->height;
                                data->rawSub->left = 0.0f;
                                data->rawSub->top = 1.0f;
                                data->rawSub->right = 1.0f;
                                data->rawSub->bottom = 0.0f;
                            }
                            Tex3DS_TextureFree(t3x);
                        } else {
                            delete data->rawTex;
                            data->rawTex = nullptr;
                        }
                        linearFree(data->fileBuffer);
                        data->fileBuffer = nullptr;
                        data->fileSize = 0;
                    }
                }
                fclose(f);
            }
        }
    }

    json_t *anims = json_object_get(root, "animations");
    if (json_is_array(anims)) {
        size_t index; json_t *value;
        json_array_foreach(anims, index, value) {
            Animation anim;
            anim.name = json_string_value(json_object_get(value, "anim"));
            anim.prefix = json_string_value(json_object_get(value, "name"));
            anim.fps = (int)json_integer_value(json_object_get(value, "fps"));
            anim.loop = json_boolean_value(json_object_get(value, "loop"));
            if (anim.name == "danceLeft" || anim.name == "danceRight" || anim.name == "idle") {
                anim.loop = false;
            }
            
            std::vector<int> prefixIndices;
            for (int i = 0; i < (int)data->frames.size(); i++) {
                std::string frameName = data->frames[i].name;
                if (frameName.find(anim.prefix) == 0) {
                    bool matches = false;
                    if (frameName.length() == anim.prefix.length()) matches = true;
                    else {
                        std::string rest = frameName.substr(anim.prefix.length());
                        size_t firstPos = rest.find_first_not_of(" \t");
                        if (firstPos != std::string::npos) {
                            std::string actualRest = rest.substr(firstPos);
                            if (isdigit(actualRest[0]) || actualRest.find("instancia") == 0) matches = true;
                        }
                    }
                    if (matches) prefixIndices.push_back(i);
                }
            }

            json_t *indices = json_object_get(value, "indices");
            if (json_is_array(indices) && json_array_size(indices) > 0) {
                size_t iIdx; json_t *iVal;
                json_array_foreach(indices, iIdx, iVal) {
                    int localIdx = (int)json_integer_value(iVal);
                    if (localIdx >= 0 && localIdx < (int)prefixIndices.size()) {
                        anim.indices.push_back(prefixIndices[localIdx]);
                    }
                }
            } else {
                anim.indices = prefixIndices;
            }

            json_t *offsets = json_object_get(value, "offsets");
            if (json_is_array(offsets) && json_array_size(offsets) >= 2) {
                anim.offsetX = (float)json_number_value(json_array_get(offsets, 0));
                anim.offsetY = (float)json_number_value(json_array_get(offsets, 1));
            } else { anim.offsetX = 0; anim.offsetY = 0; }
            data->animations[anim.name] = anim;
        }
    }

    json_t *jsonSingDur = json_object_get(root, "sing_duration");
    if (json_is_number(jsonSingDur)) data->singDuration = (float)json_number_value(jsonSingDur);

    if (data->animations.count("danceLeft") && data->animations.count("danceRight") && 
        !data->animations["danceLeft"].indices.empty() && !data->animations["danceRight"].indices.empty()) {
        data->danceEveryNumBeats = 1;
    } else {
        data->danceEveryNumBeats = 2;
    }

    json_decref(root);
    
    if (!data->isPlaceholder && !fullPath.empty()) {
        bool isPlaying = (PlayState::instance != nullptr && Conductor::songPosition >= 0.0f);
        if (!isPlaying) {
            saveToCache(cachePath, data, fullPath);
        }
    }
    
    return data;
}

void Character::instantiateFromData(CharacterData* data) {
    curCharacterName = data->charName;
    isPlaceholder = data->isPlaceholder;
    charScale = data->charScale;
    charScaleX = data->charScale;
    charScaleY = data->charScale;
    flipX = data->flipX;
    noAntialiasing = data->noAntialiasing;
    healthIcon = data->healthIcon;
    healthbarR = data->healthbarR;
    healthbarG = data->healthbarG;
    healthbarB = data->healthbarB;
    camOffsetX = data->camOffsetX;
    camOffsetY = data->camOffsetY;
    baseX = data->baseX;
    baseY = data->baseY;
    x = baseX;
    y = baseY;
    singDuration = data->singDuration;
    danceEveryNumBeats = data->danceEveryNumBeats;
    animations = std::move(data->animations);
    frames = std::move(data->frames);

    if (data->isRawTex && data->fileBuffer && data->fileSize > 0) {
        rawTex = new C3D_Tex();
        memset(rawTex, 0, sizeof(C3D_Tex));
        if (!C3D_TexInit(rawTex, data->rawWidth, data->rawHeight, GPU_RGBA8)) {
            delete rawTex;
            rawTex = nullptr;
            printf("\x1b[16;1HERROR: RAM Full for Async Char Load\x1b[K\n");
        } else {
            if (rawTex->data) linearFree(rawTex->data);
            rawTex->data = data->fileBuffer;
            data->fileBuffer = nullptr;
            
            rawSub = new Tex3DS_SubTexture();
            rawSub->width = data->rawOrigW;
            rawSub->height = data->rawOrigH;
            rawSub->left = 0.0f;
            rawSub->top = 1.0f;
            rawSub->right = (float)data->rawOrigW / data->rawWidth;
            rawSub->bottom = 1.0f - ((float)data->rawOrigH / data->rawHeight);
            
            mainImage.tex = rawTex;
            mainImage.subtex = rawSub;
        }
    } else if (data->sheet) {
        sheet = data->sheet;
        mainImage = C2D_SpriteSheetGetImage(sheet, 0);
        data->sheet = nullptr;
    } else if (!data->isRawTex && data->rawTex) {
        rawTex = data->rawTex;
        rawSub = data->rawSub;
        
        mainImage.tex = rawTex;
        mainImage.subtex = rawSub;
        
        data->rawTex = nullptr;
        data->rawSub = nullptr;
    }
    
    if (mainImage.tex) {
        GPU_TEXTURE_FILTER_PARAM filter = (!noAntialiasing && ClientPrefs::globalAntialiasing) ? GPU_LINEAR : GPU_NEAREST;
        C3D_TexSetFilter(mainImage.tex, filter, filter);
        float rw = mainImage.subtex->right - mainImage.subtex->left;
        float rh = mainImage.subtex->bottom - mainImage.subtex->top;
        for (auto& f : frames) {
            f.tex = mainImage.tex;
            f.uv.width  = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left   = mainImage.subtex->left + ((float)f.x       * rw / (float)mainImage.subtex->width);
            f.uv.top    = mainImage.subtex->top  + ((float)f.y       * rh / (float)mainImage.subtex->height);
            f.uv.right  = mainImage.subtex->left + ((float)(f.x+f.w) * rw / (float)mainImage.subtex->width);
            f.uv.bottom = mainImage.subtex->top  + ((float)(f.y+f.h) * rh / (float)mainImage.subtex->height);
        }
    }

    dance();
}

void Character::loadFromPsychJson(const std::string& jsonPath) {
    size_t slash = jsonPath.find_last_of("\\/");
    std::string filename = (slash == std::string::npos) ? jsonPath : jsonPath.substr(slash + 1);
    size_t dot = filename.find_last_of(".");
    curCharacterName = (dot == std::string::npos) ? filename : filename.substr(0, dot);

    json_t *root;
    json_error_t error;
    
    if (!Paths::fileExists(jsonPath)) {
        printf("\x1b[14;1HWARN: Character '%s' not found. Using Placeholder.\x1b[K\n", jsonPath.c_str());
        isPlaceholder = true;
        loadFromPsychJson(Paths::characterJson("bf-pixel"));
        return;
    }

    if (jsonPath.find("bf-pixel") == std::string::npos) isPlaceholder = false;

    root = json_load_file(jsonPath.c_str(), 0, &error);
    if (!root) {
        printf("\x1b[14;1HERROR JSON: %s\x1b[K\n", jsonPath.c_str());
        return;
    }


    json_t *jsonScale = json_object_get(root, "scale");
    if (json_is_number(jsonScale)) charScale = (float)json_number_value(jsonScale);
    else charScale = 6.0f;
    charScaleX = charScale;
    charScaleY = charScale;

    json_t *jsonFlip = json_object_get(root, "flip_x");
    if (json_is_boolean(jsonFlip)) flipX = json_boolean_value(jsonFlip);

    json_t *jsonNoAA = json_object_get(root, "no_antialiasing");
    if (json_is_boolean(jsonNoAA)) noAntialiasing = json_boolean_value(jsonNoAA);

    json_t *jsonIcon = json_object_get(root, "healthicon");
    if (json_is_string(jsonIcon)) healthIcon = json_string_value(jsonIcon);
    else healthIcon = "face";

    json_t *jsonHBC = json_object_get(root, "healthbar_colors");
    if (json_is_array(jsonHBC) && json_array_size(jsonHBC) >= 3) {
        healthbarR = (float)json_number_value(json_array_get(jsonHBC, 0)) / 255.0f;
        healthbarG = (float)json_number_value(json_array_get(jsonHBC, 1)) / 255.0f;
        healthbarB = (float)json_number_value(json_array_get(jsonHBC, 2)) / 255.0f;
    }

    json_t *jsonCam = json_object_get(root, "camera_position");
    if (json_is_array(jsonCam) && json_array_size(jsonCam) >= 2) {
        camOffsetX = (float)json_number_value(json_array_get(jsonCam, 0));
        camOffsetY = (float)json_number_value(json_array_get(jsonCam, 1));
    }
    json_t *jsonPos = json_object_get(root, "position");
    if (json_is_array(jsonPos) && json_array_size(jsonPos) >= 2) {
        x = (float)json_number_value(json_array_get(jsonPos, 0));
        y = (float)json_number_value(json_array_get(jsonPos, 1));
        baseX = x;
        baseY = y;
    }

    json_t *jsonImage = json_object_get(root, "image");
    if (json_is_string(jsonImage)) {
        std::string fullPath = json_string_value(jsonImage);
        loadSparrowXml(Paths::xml(fullPath));
        addSpriteSheet(Paths::image(fullPath));
    }


    json_t *anims = json_object_get(root, "animations");
    if (!json_is_array(anims)) { json_decref(root); return; }

    size_t index;
    json_t *value;
    json_array_foreach(anims, index, value) {
        Animation anim;
        anim.name = json_string_value(json_object_get(value, "anim"));
        anim.prefix = json_string_value(json_object_get(value, "name"));
        anim.fps = (int)json_integer_value(json_object_get(value, "fps"));
        anim.loop = json_boolean_value(json_object_get(value, "loop"));
        if (anim.name == "danceLeft" || anim.name == "danceRight" || anim.name == "idle") {
            anim.loop = false;
        }
        
        json_t *indices = json_object_get(value, "indices");
        
        std::vector<int> prefixIndices;
        for (int i = 0; i < (int)frames.size(); i++) {
            std::string frameName = frames[i].name;
            if (frameName.find(anim.prefix) == 0) {
                bool matches = false;
                if (frameName.length() == anim.prefix.length()) matches = true;
                else {
                    std::string rest = frameName.substr(anim.prefix.length());
                    size_t firstPos = rest.find_first_not_of(" \t");
                    if (firstPos != std::string::npos) {
                        std::string actualRest = rest.substr(firstPos);
                        if (isdigit(actualRest[0]) || actualRest.find("instancia") == 0) {
                            matches = true;
                        }
                    }
                }

                if (matches) {
                    prefixIndices.push_back(i);
                }
            }
        }

        if (json_is_array(indices) && json_array_size(indices) > 0) {
            size_t iIdx; json_t *iVal;
            json_array_foreach(indices, iIdx, iVal) {
                int localIdx = (int)json_integer_value(iVal);
                if (localIdx >= 0 && localIdx < (int)prefixIndices.size()) {
                    anim.indices.push_back(prefixIndices[localIdx]);
                }
            }
        } else {
            anim.indices = prefixIndices;
        }

        json_t *offsets = json_object_get(value, "offsets");
        if (json_is_array(offsets) && json_array_size(offsets) >= 2) {
            anim.offsetX = (float)json_number_value(json_array_get(offsets, 0));
            anim.offsetY = (float)json_number_value(json_array_get(offsets, 1));
        } else {
            anim.offsetX = 0; anim.offsetY = 0;
        }
        animations[anim.name] = anim;
    }

    json_t *jsonSingDur = json_object_get(root, "sing_duration");
    if (json_is_number(jsonSingDur)) singDuration = (float)json_number_value(jsonSingDur);
    else singDuration = 4.0f;

    if (animations.count("danceLeft") && animations.count("danceRight") && 
        !animations["danceLeft"].indices.empty() && !animations["danceRight"].indices.empty()) {
        danceEveryNumBeats = 1;
    } else {
        danceEveryNumBeats = 2;
    }

    json_decref(root);
    dance();
}

void Character::dance(bool forced) {
    if (!forced && specialAnim) return;
    if (!forced && isExternalAnim && !animFinished) return;

    float singThreshold = (Conductor::stepCrochet * 0.0011f) * singDuration;
    if (!forced && (curAnim.find("sing") != std::string::npos && curAnim.find("miss") == std::string::npos) && holdTimer < singThreshold) return;

    if (animations.count("danceLeft") && animations.count("danceRight") && 
        !animations["danceLeft"].indices.empty() && !animations["danceRight"].indices.empty()) {
        danced = !danced;
        if (danced) playAnim("danceRight", forced);
        else playAnim("danceLeft", forced);
    } else if (animations.count("idle") && !animations["idle"].indices.empty()) {
        playAnim("idle", forced);
    } else if (animations.count("dance") && !animations["dance"].indices.empty()) {
        playAnim("dance", forced);
    }
}

struct RawTexHeader {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t origW;
    uint16_t origH;
};
static inline bool addrIsVRAM(const void* addr) {
    u32 v = (u32)addr;
    return v >= 0x1F000000 && v < 0x1F600000;
}

void Character::addSpriteSheet(const std::string& t3xPath) {
    printf("CharSheet: %s\n", t3xPath.c_str());
    if (sheet) { C2D_SpriteSheetFree(sheet); sheet = nullptr; }
    
    if (!charTexturePath.empty() && charTextureCache.count(charTexturePath)) {
        charTextureCache[charTexturePath].refCount--;
        if (charTextureCache[charTexturePath].refCount <= 0) {
            C3D_Tex* t = charTextureCache[charTexturePath].tex;
            Tex3DS_SubTexture* s = charTextureCache[charTexturePath].subtex;
            if (t) {
                C3D_TexDelete(t);
                delete t;
            }
            if (s) delete s;
            charTextureCache.erase(charTexturePath);
        }
    } else {
        if (rawTex) { C3D_TexDelete(rawTex); delete rawTex; }
        if (rawSub) { delete rawSub; }
    }
    rawTex = nullptr;
    rawSub = nullptr;
    mainImage.tex = nullptr;
    mainImage.subtex = nullptr;
    charTexturePath = "";
    
    for (auto& f : frames) {
        f.tex = nullptr;
    }

    sheet = nullptr;
    
    bool loadedFromCache = false;
    if (charTextureCache.count(t3xPath)) {
        charTextureCache[t3xPath].refCount++;
        rawTex = charTextureCache[t3xPath].tex;
        rawSub = charTextureCache[t3xPath].subtex;
        mainImage.tex = rawTex;
        mainImage.subtex = rawSub;
        charTexturePath = t3xPath;
        loadedFromCache = true;
    }

    if (!loadedFromCache) {
        std::string actualT3xPath = t3xPath;
        if (actualT3xPath.find(".t3x") == std::string::npos) {
            actualT3xPath += ".t3x";
        }
        
        FILE* f = fopen(actualT3xPath.c_str(), "rb");
        if (f) {
            rawTex = new C3D_Tex();
            memset(rawTex, 0, sizeof(C3D_Tex));
            Tex3DS_Texture t3x = Tex3DS_TextureImportStdio(f, rawTex, nullptr, false);
            if (t3x) {
                const Tex3DS_SubTexture* sub = Tex3DS_GetSubTexture(t3x, 0);
                rawSub = new Tex3DS_SubTexture();
                if (sub) {
                    *rawSub = *sub;
                } else {
                    rawSub->width = rawTex->width;
                    rawSub->height = rawTex->height;
                    rawSub->left = 0.0f;
                    rawSub->top = 1.0f;
                    rawSub->right = 1.0f;
                    rawSub->bottom = 0.0f;
                }
                mainImage.tex = rawTex;
                mainImage.subtex = rawSub;
                Tex3DS_TextureFree(t3x);
            } else {
                delete rawTex;
                rawTex = nullptr;
            }
            fclose(f);
        }
        
        if (!mainImage.tex) {
            std::string rawPath = t3xPath;
            size_t lastDot = rawPath.find_last_of(".");
            if (lastDot != std::string::npos) rawPath = rawPath.substr(0, lastDot) + ".rawtex";
            
            std::string searchKey = "images/";
            size_t imgPos = rawPath.find(searchKey);
            if (imgPos != std::string::npos) {
                std::string logicalPath = rawPath.substr(imgPos);
                std::string modPath = ModHandler::get().getModPath(logicalPath);
                if (!modPath.empty()) rawPath = modPath;
            }

            if (Paths::fileExists(rawPath)) {
                FILE* f = fopen(rawPath.c_str(), "rb");
                if (f) {
                    RawTexHeader header;
                    if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                        rawTex = new C3D_Tex();
                        memset(rawTex, 0, sizeof(C3D_Tex));
                        if (!C3D_TexInit(rawTex, header.width, header.height, GPU_RGBA8)) {
                            delete rawTex;
                            rawTex = nullptr;
                            printf("\x1b[16;1HERROR: VRAM Full. Cannot load %s\x1b[K\n", rawPath.c_str());
                        } else {
                            size_t dataSize = (size_t)header.width * header.height * 4;
                            void* data = linearAlloc(dataSize);
                            if (data) {
                                fread(data, dataSize, 1, f);
                                C3D_TexUpload(rawTex, data);
                                C3D_TexFlush(rawTex);
                                linearFree(data);
                                
                                rawSub = new Tex3DS_SubTexture();
                                rawSub->width = header.origW;
                                rawSub->height = header.origH;
                                rawSub->left = 0.0f;
                                rawSub->top = 1.0f;
                                rawSub->right = (float)header.origW / header.width;
                                rawSub->bottom = 1.0f - ((float)header.origH / header.height);
                                
                                mainImage.tex = rawTex;
                                mainImage.subtex = rawSub;
                            } else {
                                delete rawTex;
                                rawTex = nullptr;
                            }
                        }
                    }
                    fclose(f);
                }
            }
        }

        if (mainImage.tex) {
            charTexturePath = t3xPath;
            CachedCharacterTexture cached;
            cached.tex = rawTex;
            cached.subtex = rawSub;
            cached.refCount = 1;
            charTextureCache[t3xPath] = cached;
        }
    }

    if (!mainImage.tex) {
        printf("\x1b[15;1HERROR TEXTURE: %s\x1b[K\n", t3xPath.c_str());
        return;
    }
    
    GPU_TEXTURE_FILTER_PARAM filter = (!noAntialiasing && ClientPrefs::globalAntialiasing) ? GPU_LINEAR : GPU_NEAREST;
    C3D_TexSetFilter(mainImage.tex, filter, filter);

    float rw = mainImage.subtex->right - mainImage.subtex->left;
    float rh = mainImage.subtex->bottom - mainImage.subtex->top;

    for (auto& f : frames) {
        f.tex = mainImage.tex;
        f.uv.width  = (u16)f.w;
        f.uv.height = (u16)f.h;
        f.uv.left   = mainImage.subtex->left + ((float)f.x       * rw / (float)mainImage.subtex->width);
        f.uv.top    = mainImage.subtex->top  + ((float)f.y       * rh / (float)mainImage.subtex->height);
        f.uv.right  = mainImage.subtex->left + ((float)(f.x+f.w) * rw / (float)mainImage.subtex->width);
        f.uv.bottom = mainImage.subtex->top  + ((float)(f.y+f.h) * rh / (float)mainImage.subtex->height);
    }
}

void Character::playAnim(const std::string& animName, bool forced) {
    if (!forced && curAnim == animName && !animFinished) return;
    if (animations.count(animName)) {
        specialAnim = false;
        isExternalAnim = false;
        curAnim = animName;
        currentAnimData = &animations[animName];
        curFrame = 0; frameTimer = 0; animFinished = false;

        if (curCharacterName.rfind("gf-", 0) == 0 || curCharacterName == "gf") {
            if (animName == "singLEFT")
                danced = true;
            else if (animName == "singRIGHT")
                danced = false;
            else if (animName == "singUP" || animName == "singDOWN")
                danced = !danced;
        }
    }
}

void Character::playAnimFES(const std::string& path, const std::string& animName, int fps, bool loop, float x, float y) {
    CachedSpritesheet* cs = SpritesheetCache::get().load(path);
    if (!cs) return;

    isExternalAnim = true;
    externalFrames = cs->frames;
    
    externalAnimData.name = "FES_" + animName;
    externalAnimData.prefix = animName;
    externalAnimData.fps = fps;
    externalAnimData.loop = loop;
    
    float baseOX = 0;
    float baseOY = 0;
    if (animations.count("idle")) {
        baseOX = animations["idle"].offsetX;
        baseOY = animations["idle"].offsetY;
    } else if (animations.count("danceLeft")) {
        baseOX = animations["danceLeft"].offsetX;
        baseOY = animations["danceLeft"].offsetY;
    }

    externalAnimData.offsetX = baseOX + x;
    externalAnimData.offsetY = baseOY + y;
    externalAnimData.indices.clear();

    for (int i = 0; i < (int)externalFrames.size(); i++) {
        if (externalFrames[i].name.find(animName) == 0) {
            externalAnimData.indices.push_back(i);
        }
    }

    if (externalAnimData.indices.empty()) {
        printf("\x1b[17;1HFES ERROR: Anim '%s' not found in external XML!\x1b[K\n", animName.c_str());
    } else {
        curAnim = externalAnimData.name;
        currentAnimData = &externalAnimData;
        curFrame = 0; frameTimer = 0; animFinished = false;
    }
}

void Character::update(float dt) {
    if (!currentAnimData || currentAnimData->indices.empty()) return;
    
    if (curAnim.find("sing") != std::string::npos) {
        holdTimer += dt;
    } else {
        holdTimer = 0;
    }

    if (!isPlayer) {
        if (curAnim.find("sing") != std::string::npos && curAnim.find("miss") == std::string::npos) {
            float singThreshold = (Conductor::stepCrochet * 0.0011f) * singDuration;
            if (holdTimer >= singThreshold) {
                dance();
                holdTimer = 0;
            }
        }
    }

    if (curAnim.find("miss") != std::string::npos && animFinished) {
        dance(true);
    }

    if (animFinished) return;
    
    frameTimer += dt * currentAnimData->fps;
    while (frameTimer >= 1.0f && !animFinished) {
        frameTimer -= 1.0f;
        curFrame++;
        if (curFrame >= (int)currentAnimData->indices.size()) {
            if (currentAnimData->loop) {
                curFrame = 0;
            } else { 
                curFrame = (int)currentAnimData->indices.size() - 1; 
                animFinished = true; 
                frameTimer = 0.0f;
            }
        }
    }

    if (specialAnim && animFinished) {
        specialAnim = false;
        dance();
    }
}

bool Character::hasAnimation(const std::string& animName) {
    return animations.count(animName) > 0;
}

void Character::draw(float stageX, float stageY, float depth, float zoom, float camX, float camY, float shakeX, float shakeY) {
    if (!visible) return;
    if (!currentAnimData || currentAnimData->indices.empty()) {
        const std::vector<Frame>& useFrames = isExternalAnim ? externalFrames : frames;
        if (!useFrames.empty()) {
             const Frame& f0 = useFrames[0];
             C2D_Image img = { f0.tex, &f0.uv };
             float screenScale = 240.0f / 720.0f;
             float baseX = stageX + x - camX;
             float baseY = stageY + y - camY;
             float drawX = (baseX * screenScale * zoom) + (ScreenWidthTop / 2.0f) + shakeX;
             float drawY = (baseY * screenScale * zoom) + (ScreenHeight / 2.0f) + shakeY;
             C2D_ImageTint tint;
             C2D_ImageTint* tintPtr = nullptr;
             
             if (!img.tex || isPlaceholder) {
                 C2D_PlainImageTint(&tint, C2D_Color32(100, 100, 100, (u8)(alpha * 255.0f)), 1.0f);
                 tintPtr = &tint;
                 if (!img.tex) {
                     float finalScale = charScale * screenScale * zoom;
                     C2D_DrawRectSolid(drawX, drawY, depth, 64.0f * finalScale, 64.0f * finalScale, C2D_Color32(100, 100, 100, (u8)(alpha * 255.0f)));
                     return;
                 }
             } else if (alpha < 1.0f) {
                 C2D_AlphaImageTint(&tint, alpha);
                 tintPtr = &tint;
             }
             
             float totalAngle = angle;
             float finalScale = charScale * screenScale * zoom;
             bool shouldFlip = (isPlayer != flipX);
             {
                 float angleRad = totalAngle * (3.14159265f / 180.0f);
                 if (f0.rotated) angleRad -= (3.14159265f / 2.0f);
                 float imgW = f0.rotated ? (float)f0.h : (float)f0.w;
                 float imgH = f0.rotated ? (float)f0.w : (float)f0.h;
                 float centerX = drawX + imgW * (shouldFlip ? -finalScale : finalScale) / 2.0f;
                 float centerY = drawY + imgH * finalScale / 2.0f;

                 float scaleX = shouldFlip ? -finalScale : finalScale;
                 float scaleY = finalScale;
                 if (f0.rotated) {
                     scaleX = finalScale;
                     scaleY = shouldFlip ? -finalScale : finalScale;
                 }
                 C2D_DrawImageAtRotated(img, centerX, centerY, depth, angleRad, tintPtr, scaleX, scaleY);
             }
        } return;
    }
    
    int frameIdx = currentAnimData->indices[curFrame];
    const std::vector<Frame>& useFrames = isExternalAnim ? externalFrames : frames;
    if (frameIdx < 0 || frameIdx >= (int)useFrames.size()) return;

    const Frame& f = useFrames[frameIdx];
    C2D_Image img = { f.tex, &f.uv };

    float screenScale = 240.0f / 720.0f;
    float finalScaleX = charScaleX * screenScale * zoom;
    float finalScaleY = charScaleY * screenScale * zoom;
    bool shouldFlip = (isPlayer != flipX);

    float baseX = stageX + x - camX;
    float baseY = stageY + y - camY;
    float drawX = (baseX * screenScale * zoom) + (ScreenWidthTop / 2.0f) + shakeX;
    float drawY = (baseY * screenScale * zoom) + (ScreenHeight / 2.0f) + shakeY;

    // HaxeFlixel scales from the origin (center of the frame)
    if (!PlayState::instance || !PlayState::instance->legacyPositioning) {
        float originX = f.frameW / 2.0f;
        float originY = f.frameH / 2.0f;
        drawX += originX * (1.0f - charScaleX) * screenScale * zoom;
        drawY += originY * (1.0f - charScaleY) * screenScale * zoom;
    }

    // frameX/frameY are negative offsets that indicate where the visible sprite sits
    // inside the full logical frame. For rotated frames the atlas w/h are swapped,
    // but frameX/frameY in the XML already refer to the logical (unrotated) frame.
    if (shouldFlip) {
        drawX += (f.frameW + f.frameX - currentAnimData->offsetX) * finalScaleX;
    } else {
        drawX -= (f.frameX + currentAnimData->offsetX) * finalScaleX;
    }
    drawY -= (f.frameY + currentAnimData->offsetY) * finalScaleY;

    C2D_ImageTint tint;
    C2D_ImageTint* tintPtr = nullptr;
    if (!img.tex || isPlaceholder) {
        C2D_PlainImageTint(&tint, C2D_Color32(100, 100, 100, (u8)(alpha * 255.0f)), 1.0f);
        tintPtr = &tint;
        if (!img.tex) {
            float finalWidth = f.frameW * finalScaleX;
            float finalHeight = f.frameH * finalScaleY;
            if (finalWidth <= 0) finalWidth = 100.0f * finalScaleX;
            if (finalHeight <= 0) finalHeight = 100.0f * finalScaleY;
            C2D_DrawRectSolid(drawX, drawY, depth, finalWidth, finalHeight, C2D_Color32(100, 100, 100, (u8)(alpha * 255.0f)));
            return;
        }
    } else if (alpha < 1.0f) {
        C2D_AlphaImageTint(&tint, alpha);
        tintPtr = &tint;
    }

    float totalAngle = angle;

    // For rotated frames we must always go through the rotated draw path so we can
    // add the compensating -90° turn (the sprite was stored CW in the atlas).
    float angleRad = totalAngle * (3.14159265f / 180.0f);
    if (f.rotated) angleRad -= (3.14159265f / 2.0f);

    if (totalAngle != 0.0f || f.rotated) {
        float imgW = f.rotated ? (float)f.h : (float)f.w;
        float imgH = f.rotated ? (float)f.w : (float)f.h;
        float centerX = drawX + imgW * (shouldFlip ? -finalScaleX : finalScaleX) / 2.0f;
        float centerY = drawY + imgH * finalScaleY / 2.0f;
        
        float scaleX = shouldFlip ? -finalScaleX : finalScaleX;
        float scaleY = finalScaleY;
        if (f.rotated) {
            scaleX = finalScaleX;
            scaleY = shouldFlip ? -finalScaleY : finalScaleY;
        }

        C2D_DrawImageAtRotated(img, centerX, centerY, depth, angleRad, tintPtr, scaleX, scaleY);
    } else {
        C2D_DrawImageAt(img, drawX, drawY, depth, tintPtr, (shouldFlip ? -finalScaleX : finalScaleX), finalScaleY);
    }

}

void Character::setAntialiasing(bool antialiased) {
    noAntialiasing = !antialiased;
    GPU_TEXTURE_FILTER_PARAM filter = antialiased ? GPU_LINEAR : GPU_NEAREST;
    if (mainImage.tex) {
        C3D_TexSetFilter(mainImage.tex, filter, filter);
    }
    if (rawTex) {
        C3D_TexSetFilter(rawTex, filter, filter);
    }
    for (auto& f : frames) {
        if (f.tex) C3D_TexSetFilter(f.tex, filter, filter);
    }
    for (auto& f : externalFrames) {
        if (f.tex) C3D_TexSetFilter(f.tex, filter, filter);
    }
}
