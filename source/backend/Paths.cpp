#include "ModHandler.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <algorithm>


std::string Paths::resolve(const std::string& path) {
    if (path.empty()) return path;

    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return path;
    }

    std::string prefix = "";
    std::string remaining = path;

    if (path.find("romfs:/") == 0) {
        prefix = "romfs:/";
        remaining = path.substr(7);
    } else if (path.find("sdmc:/") == 0) {
        prefix = "sdmc:/";
        remaining = path.substr(6);
    }

    std::vector<std::string> segments;
    size_t start = 0;
    while (true) {
        size_t nextSlash = remaining.find_first_of("\\/", start);
        if (nextSlash == std::string::npos) {
            segments.push_back(remaining.substr(start));
            break;
        }
        segments.push_back(remaining.substr(start, nextSlash - start));
        start = nextSlash + 1;
    }

    std::string currentPath = prefix;
    for (size_t i = 0; i < segments.size(); ++i) {
        const std::string& segment = segments[i];
        if (segment.empty() || segment == "." || segment == "..") {
            if (!currentPath.empty() && currentPath.back() != '/') currentPath += "/";
            currentPath += segment;
            continue;
        }

        std::string searchDir = currentPath.empty() ? "." : currentPath;
        DIR* dir = opendir(searchDir.c_str());
        bool found = false;
        if (dir) {
            struct dirent* entry;
            std::string segmentLower = segment;
            std::transform(segmentLower.begin(), segmentLower.end(), segmentLower.begin(), ::tolower);
            
            while ((entry = readdir(dir)) != nullptr) {
                std::string entryName = entry->d_name;
                std::string entryLower = entryName;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (segmentLower == entryLower) {
                    if (!currentPath.empty() && currentPath.back() != '/') currentPath += "/";
                    currentPath += entryName;
                    found = true;
                    break;
                }
            }
            closedir(dir);
        }

        if (!found) {
            if (!currentPath.empty() && currentPath.back() != '/') currentPath += "/";
            currentPath += segment;
        }
    }

    return currentPath;
}

bool Paths::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(resolve(path).c_str(), &buffer) == 0);
}

std::string Paths::getPath(const std::string& file, const std::string& type, const std::string& library) {
    std::string relPath = type + "/" + file;
    std::string modPath = ModHandler::get().getModPath(relPath);
    if (!modPath.empty()) return resolve(modPath);

    // Automatic .rawtex fallback for .t3x files in mods!
    if (file.find(".t3x") != std::string::npos) {
        std::string rawFile = file.substr(0, file.find_last_of(".")) + ".rawtex";
        std::string rawRel = type + "/" + rawFile;
        std::string rawMod = ModHandler::get().getModPath(rawRel);
        if (!rawMod.empty()) return resolve(rawMod);
    }

    if (!library.empty()) {
        std::string path = "romfs:/" + library + "/" + type + "/" + file;
        if (fileExists(path)) return resolve(path);
    }

    std::vector<std::string> libs = {"shared", "preload"};
    for (const auto& lib : libs) {
        std::string path = "romfs:/" + lib + "/" + type + "/" + file;
        if (fileExists(path)) return resolve(path);
    }
    
    // Automatic .rawtex fallback for .t3x files in romfs!
    if (file.find(".t3x") != std::string::npos) {
        std::string rawFile = file.substr(0, file.find_last_of(".")) + ".rawtex";
        if (!library.empty()) {
            std::string path = "romfs:/" + library + "/" + type + "/" + rawFile;
            if (fileExists(path)) return resolve(path);
        }
        for (const auto& lib : libs) {
            std::string path = "romfs:/" + lib + "/" + type + "/" + rawFile;
            if (fileExists(path)) return resolve(path);
        }
    }

    if (!library.empty()) {
        return resolve("romfs:/" + library + "/" + type + "/" + file);
    }
    return resolve("romfs:/" + type + "/" + file);
}

std::string Paths::image(const std::string& key, const std::string& library) {
    return getPath(key + ".t3x", "images", library);
}

std::string Paths::xml(const std::string& key, const std::string& library) {
    return getPath(key + ".xml", "images", library);
}

std::string Paths::json(const std::string& key, const std::string& library) {
    return getPath(key + ".json", "images", library);
}

std::string Paths::audio(const std::string& folder, const std::string& filename) {
    std::string normFolder = folder;
    size_t pos = normFolder.find("songs/");
    if (pos != std::string::npos) {
        std::string prefix = normFolder.substr(0, pos + 6);
        std::string song = normFolder.substr(pos + 6);
        std::transform(song.begin(), song.end(), song.begin(), ::tolower);
        std::replace(song.begin(), song.end(), ' ', '-');
        normFolder = prefix + song;
    }

    std::vector<std::string> extensions = {".ogg", ".adp"};
    std::vector<std::string> fileCases = {filename};
    
    std::string capped = filename;
    if (!capped.empty() && islower(capped[0])) {
        capped[0] = toupper(capped[0]);
        fileCases.push_back(capped);
    }

    for (const auto& ext : extensions) {
        for (auto& baseName : fileCases) {
            std::string finalName = baseName;
            if (finalName.find(".ogg") != std::string::npos && ext == ".adp") {
                finalName.replace(finalName.find(".ogg"), 4, ".adp");
            } else if (finalName.find(".adp") != std::string::npos && ext == ".ogg") {
                finalName.replace(finalName.find(".adp"), 4, ".ogg");
            }

            std::string modPath = ModHandler::get().getModPath(normFolder + "/" + finalName);
            if (!modPath.empty()) {
                return resolve(modPath);
            }
        }
    }

    std::string path1 = "romfs:/" + normFolder + "/" + filename;
    if (fileExists(path1)) return resolve(path1);

    if (capped != filename) {
        std::string path2 = "romfs:/" + normFolder + "/" + capped;
        if (fileExists(path2)) return resolve(path2);
    }

    auto toAdp = [](std::string s) {
        size_t p = s.find(".ogg");
        if (p != std::string::npos) s.replace(p, 4, ".adp");
        return s;
    };
    std::string adp1 = toAdp(path1);
    if (fileExists(adp1)) return resolve(adp1);
    if (capped != filename) {
        std::string adp2 = toAdp("romfs:/" + normFolder + "/" + capped);
        if (fileExists(adp2)) return resolve(adp2);
    }

    return resolve(path1);
}



std::string Paths::font(const std::string& key) {
    std::string relPath = "fonts/" + key + ".t3x";
    std::string modPath = ModHandler::get().getModPath(relPath);
    if (!modPath.empty()) return resolve(modPath);
    return resolve("romfs:/" + relPath);
}

std::string Paths::txt(const std::string& key, const std::string& library) {
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string type = key.substr(0, lastSlash);
        std::string file = key.substr(lastSlash + 1);
        return getPath(file + ".txt", type, library);
    }
    return getPath(key + ".txt", "", library);
}

std::string Paths::weeksDir() {
    return "romfs:/shared/weeks/";
}

std::string Paths::characterJson(const std::string& character) {
    return getPath(character + ".json", "characters", "preload");
}

std::string Paths::characterCache(const std::string& character) {
    std::string base = ModHandler::getWorkingBase();
    if (base.empty()) {
        #ifdef _3DS
        base = "sdmc:/SnakeEngine/";
        #else
        base = "SnakeEngine/";
        #endif
    }
    if (!base.empty() && base.back() != '/') base += "/";

    std::string modDir = ModHandler::get().currentModFolder;
    if (modDir.empty() || modDir == "preload") {
        return base + "cache/preload/characters/" + character + ".charcache";
    }

    return base + "mods/" + modDir + "/cache/characters/" + character + ".charcache";
}


std::string Paths::healthIcon(const std::string& icon) {
    std::string pathWithPrefix = getPath("icons/icon-" + icon + ".t3x", "images", "preload");
    if (fileExists(pathWithPrefix)) {
        return pathWithPrefix;
    }
    return getPath("icons/" + icon + ".t3x", "images", "preload");
}

std::string Paths::weekJson(const std::string& name) {
    return getPath(name + ".json", "weeks");
}

std::string Paths::songJson(const std::string& song, const std::string& difficulty) {
    std::string folder = song;
    std::transform(folder.begin(), folder.end(), folder.begin(), ::tolower);
    std::replace(folder.begin(), folder.end(), ' ', '-');

    std::string filename = folder;
    if (!difficulty.empty()) {
        std::string diffSuffix = difficulty;
        std::transform(diffSuffix.begin(), diffSuffix.end(), diffSuffix.begin(), ::tolower);
        filename += "-" + diffSuffix;
    }
    
    // Check multiple variations in mod folder (including songs/ and root folder for compatibility)
    auto checkVariations = [&](const std::string& targetFile) -> std::string {
        std::vector<std::string> variations = {
            "data/" + folder + "/" + targetFile + ".json",
            "data/" + song + "/" + targetFile + ".json",
            "songs/" + folder + "/" + targetFile + ".json",
            "songs/" + song + "/" + targetFile + ".json",
            folder + "/" + targetFile + ".json",
            song + "/" + targetFile + ".json"
        };

        for (const auto& relPath : variations) {
            std::string modPath = ModHandler::get().getModPath(relPath);
            if (!modPath.empty()) {
                return resolve(modPath);
            }
        }
        return "";
    };

    std::string result = checkVariations(filename);
    if (!result.empty()) return resolve(result);

    if (!difficulty.empty()) {
        result = checkVariations(folder);
        if (!result.empty()) return resolve(result);
    }

    result = checkVariations(folder + "-hard");
    if (!result.empty()) return resolve(result);

    result = checkVariations(folder + "-easy");
    if (!result.empty()) return resolve(result);

    return resolve("romfs:/preload/data/" + folder + "/" + filename + ".json");
}



std::string Paths::stageJson(const std::string& stage) {
    std::string editorPath = "sdmc:/SnakeEngine/editor/stages/" + stage + ".json";
    if (fileExists(editorPath)) return resolve(editorPath);
    
    return getPath(stage + ".json", "stages", "preload");
}

std::string Paths::songDataDir(const std::string& song) {
    std::string folder = song;
    std::transform(folder.begin(), folder.end(), folder.begin(), ::tolower);
    std::replace(folder.begin(), folder.end(), ' ', '-');
    return resolve("romfs:/preload/data/" + folder + "/");
}

std::vector<std::string> Paths::globalLuaScripts() {
    std::vector<std::string> results;
    std::vector<std::string> folders;
    
    folders.push_back("romfs:/preload/scripts/");

    std::string base = ModHandler::getWorkingBase();
    if (base.empty()) {
        #ifdef _3DS
        base = "sdmc:/SnakeEngine/";
        #else
        base = "SnakeEngine/";
        #endif
    }
    if (!base.empty() && base.back() != '/') base += "/";

    folders.push_back(base + "mods/scripts/");
    for (const auto& mod : ModHandler::get().getMods()) {
        if (!mod.folder.empty() && ((mod.active && mod.runsGlobally) || mod.folder == ModHandler::get().currentModFolder)) {
            folders.push_back(base + mod.folder + "/scripts/");
        }
    }

    for (const auto& folder : folders) {
        DIR* dp = opendir(folder.c_str());
        if (dp) {
            struct dirent* entry;
            while ((entry = readdir(dp)) != nullptr) {
                std::string name = entry->d_name;
                if (name.size() > 4 && name.substr(name.size() - 4) == ".lua") {
                    results.push_back(folder + name);
                }
            }
            closedir(dp);
        }
    }

    return results;
}

std::vector<std::string> Paths::songLuaScripts(const std::string& song) {
    std::vector<std::string> results;
    std::vector<std::string> folders;
    
    std::string songFolder = song;
    std::transform(songFolder.begin(), songFolder.end(), songFolder.begin(), ::tolower);
    std::replace(songFolder.begin(), songFolder.end(), ' ', '-');

    folders.push_back("romfs:/preload/data/" + songFolder + "/");

    std::string base = ModHandler::getWorkingBase();
    if (base.empty()) {
        #ifdef _3DS
        base = "sdmc:/SnakeEngine/";
        #else
        base = "SnakeEngine/";
        #endif
    }
    if (!base.empty() && base.back() != '/') base += "/";

    folders.push_back(base + "mods/data/" + songFolder + "/");
    for (const auto& mod : ModHandler::get().getMods()) {
        if (!mod.folder.empty() && ((mod.active && mod.runsGlobally) || mod.folder == ModHandler::get().currentModFolder)) {
            folders.push_back(base + mod.folder + "/data/" + songFolder + "/");
        }
    }

    for (const auto& folder : folders) {
        DIR* dp = opendir(folder.c_str());
        if (dp) {
            struct dirent* entry;
            while ((entry = readdir(dp)) != nullptr) {
                std::string name = entry->d_name;
                if (name.size() > 4 && name.substr(name.size() - 4) == ".lua") {
                    results.push_back(folder + name);
                }
            }
            closedir(dp);
        } else {
            std::string direct = folder + songFolder + ".lua";
            if (fileExists(direct)) {
                results.push_back(direct);
            }
        }
    }

    return results;
}

std::vector<std::string> Paths::eventLuaScripts(const std::vector<std::string>& events) {
    std::vector<std::string> results;
    for (const auto& eventName : events) {
        std::string path = getPath(eventName + ".lua", "custom_events", "preload");
        if (fileExists(path)) {
            results.push_back(path);
        }
    }
    return results;
}

std::string Paths::customNoteLuaScript(const std::string& noteType) {
    std::string path = getPath(noteType + ".lua", "custom_notetypes", "preload");
    if (fileExists(path)) return path;
    return "";
}

#undef C2D_SpriteSheetLoad

#include <unordered_set>
static std::unordered_set<void*> mockSheets;

C2D_SpriteSheet Paths_loadSpriteSheet(const char* path) {
    if (!path) return nullptr;

    std::string resolvedPath = Paths::resolve(path);
    if (resolvedPath.find(".rawtex") != std::string::npos) {
        FILE* f = fopen(resolvedPath.c_str(), "rb");
        if (f) {
            struct RawTexHeader {
                char magic[4];
                uint16_t width;
                uint16_t height;
                uint16_t origW;
                uint16_t origH;
            } header;

            if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 &&
                (strncmp(header.magic, "RWTX", 4) == 0 ||
                 strncmp(header.magic, "RWT4", 4) == 0 ||
                 strncmp(header.magic, "RWT5", 4) == 0)) {
                
                C3D_Tex* texArray = (C3D_Tex*)malloc(sizeof(C3D_Tex));
                if (texArray) {
                    memset(texArray, 0, sizeof(C3D_Tex));
                    
                    GPU_TEXCOLOR format = GPU_RGBA8;
                    if (strncmp(header.magic, "RWT4", 4) == 0) format = GPU_RGBA4;
                    else if (strncmp(header.magic, "RWT5", 4) == 0) format = GPU_RGB565;

                    if (C3D_TexInit(texArray, header.width, header.height, format)) {
                        size_t bpp = 4;
                        if (format == GPU_RGBA4 || format == GPU_RGB565) bpp = 2;
                        size_t dataSize = (size_t)header.width * header.height * bpp;
                        void* data = linearAlloc(dataSize);
                        if (data) {
                            fread(data, dataSize, 1, f);
                            C3D_TexUpload(texArray, data);
                            C3D_TexFlush(texArray);
                            linearFree(data);
                            struct Mock_Tex3DS_Texture_s {
                                u16 numSubTextures;
                                u16 width;
                                u16 height;
                                u8 format;
                                u8 mipmapLevels;
                                Tex3DS_SubTexture subTextures[1];
                            };

                            struct Mock_C2D_SpriteSheet_s {
                                Mock_Tex3DS_Texture_s* t3x;
                                C3D_Tex tex;
                            };

                            Mock_C2D_SpriteSheet_s* mock = (Mock_C2D_SpriteSheet_s*)malloc(sizeof(Mock_C2D_SpriteSheet_s));
                            Mock_Tex3DS_Texture_s* t3xMock = (Mock_Tex3DS_Texture_s*)malloc(sizeof(Mock_Tex3DS_Texture_s));
                            if (mock && t3xMock) {
                                t3xMock->numSubTextures = 1;
                                t3xMock->width = header.width;
                                t3xMock->height = header.height;
                                t3xMock->format = format;
                                t3xMock->mipmapLevels = 1;
                                t3xMock->subTextures[0].width = header.origW;
                                t3xMock->subTextures[0].height = header.origH;
                                t3xMock->subTextures[0].left = 0.0f;
                                t3xMock->subTextures[0].top = 1.0f;
                                t3xMock->subTextures[0].right = (float)header.origW / header.width;
                                t3xMock->subTextures[0].bottom = 1.0f - ((float)header.origH / header.height);

                                mock->t3x = t3xMock;
                                mock->tex = *texArray;
                                free(texArray);
                                
                                fclose(f);
                                mockSheets.insert(mock);
                                return (C2D_SpriteSheet)mock;
                            }
                            if (mock) free(mock);
                            if (t3xMock) free(t3xMock);
                        }
                        C3D_TexDelete(texArray);
                    }
                    free(texArray);
                }
            }
            fclose(f);
        }
        return nullptr;
    }

    return C2D_SpriteSheetLoad(resolvedPath.c_str());
}

void Paths_freeSpriteSheet(C2D_SpriteSheet sheet) {
    if (!sheet) return;
    if (mockSheets.count(sheet) > 0) {
        mockSheets.erase(sheet);
        struct Mock_Tex3DS_Texture_s {
            u16 numSubTextures;
            u16 width;
            u16 height;
            u8 format;
            u8 mipmapLevels;
            Tex3DS_SubTexture subTextures[1];
        };
        struct Mock_C2D_SpriteSheet_s {
            Mock_Tex3DS_Texture_s* t3x;
            C3D_Tex tex;
        };
        Mock_C2D_SpriteSheet_s* mock = (Mock_C2D_SpriteSheet_s*)sheet;
        if (mock->t3x) {
            free(mock->t3x);
        }
        if (mock->tex.data) {
            linearFree(mock->tex.data);
            mock->tex.data = nullptr;
        }
        C3D_TexDelete(&mock->tex);
        free(mock);
    } else {
        #undef C2D_SpriteSheetFree
        C2D_SpriteSheetFree(sheet);
        #define C2D_SpriteSheetFree(sheet) Paths_freeSpriteSheet(sheet)
    }
}
