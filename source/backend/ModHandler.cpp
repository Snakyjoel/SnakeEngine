// Trying to find some code of LMODS?? too bad, i already killed it
#include "ModHandler.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <jansson.h>
#include <algorithm>
#include <fstream>
#include <iostream>

#ifdef _3DS
#include <3ds.h>
#define MOD_BASE_PATH "sdmc:/SnakeEngine/"
#else
#define MOD_BASE_PATH "SnakeEngine/"
#endif

ModHandler& ModHandler::get() {
    static ModHandler instance;
    return instance;
}

std::string ModHandler::currentModFolder = "";

static std::string workingBase = "sdmc:/SnakeEngine/";

static void checkWorkingBase() {}
std::string ModHandler::getWorkingBase() { checkWorkingBase(); return workingBase; }

void ModHandler::scanMods() {
    mods.clear();
    
    const char* roots[] = {"sdmc:/SnakeEngine/", "/SnakeEngine/", "SnakeEngine/"};
    std::string base = "";

    for (const char* r : roots) {
        DIR* d = opendir(r);
        if (d) {
            base = r;
            workingBase = r; // Update global base
            closedir(d);
            break;
        }
    }

    if (base.empty()) {
        #ifdef _3DS
        base = "sdmc:/SnakeEngine/";
        #else
        base = "SnakeEngine/";
        #endif
        workingBase = base;
    }

    // Try creating directories if they don't exist
    #ifdef _3DS
    mkdir("sdmc:/SnakeEngine", 0777);
    mkdir("sdmc:/SnakeEngine/mods", 0777);
    #else
    mkdir("SnakeEngine", 0777);
    mkdir("SnakeEngine/mods", 0777);
    #endif

    printf("\x1b[15;1H[MODS] Scanning: %s\x1b[K\n", base.c_str());

    std::string path = base + "mods/";
    DIR* d = opendir(path.c_str());
    if (d) {
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            std::string folderName = entry->d_name;
            std::string fullPath = path + folderName;
            
            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                // Ignore standard game asset folders placed directly
                if (folderName == "characters" || folderName == "songs" || 
                    folderName == "data" || folderName == "images" || 
                    folderName == "weeks" || folderName == "scripts" || 
                    folderName == "sounds" || folderName == "music" || 
                    folderName == "stages") {
                    continue;
                }

                ModMetadata mod;
                mod.folder = "mods/" + folderName;
                mod.name = folderName;
                
                std::string packPath = fullPath + "/pack.json";
                if (!fileExists(packPath)) packPath = fullPath + "/Pack.json";
                
                if (fileExists(packPath)) {
                    mod.hasPackJson = true;
                    parsePackJson(mod, packPath);
                } else {
                    mod.hasPackJson = false;
                    mod.runsGlobally = true; // Normal mods run globally by default
                }
                
                mods.push_back(mod);
            }
        }
        closedir(d);
    }

    // Check for root assets placed directly inside mods/
    auto checkRootContent = [&]() -> bool {
        DIR* d = opendir(path.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                if (entry->d_name[0] == '.') continue;
                std::string name = entry->d_name;
                if (name == "characters" || name == "songs" || name == "data" || 
                    name == "images" || name == "weeks" || name == "scripts" || 
                    name == "sounds" || name == "music" || name == "stages") {
                    closedir(d);
                    return true;
                }
            }
            closedir(d);
        }
        return false;
    };

    if (checkRootContent()) {
        ModMetadata rootMods;
        rootMods.folder = "mods";
        rootMods.name = "Direct mods Content";
        rootMods.description = "Files placed directly in the mods folder.";
        rootMods.hasPackJson = false;
        mods.push_back(rootMods);
    }

    printf("\x1b[16;1H[MODS] Found %d mods\x1b[K\n", (int)mods.size());
    loadConfig(); // Apply priority
}

void ModHandler::parsePackJson(ModMetadata& mod, const std::string& jsonPath) {
    std::string path = jsonPath;
    if (path.empty()) {
        path = getWorkingBase() + mod.folder + "/pack.json";
        if (!fileExists(path)) path = getWorkingBase() + mod.folder + "/Pack.json";
    }

    json_t* root;
    json_error_t error;
    root = json_load_file(path.c_str(), 0, &error);
    if (!root) return;

    json_t* name = json_object_get(root, "name");
    if (name && json_is_string(name)) mod.name = json_string_value(name);

    json_t* desc = json_object_get(root, "description");
    if (desc && json_is_string(desc)) mod.description = json_string_value(desc);

    json_t* color = json_object_get(root, "color");
    if (color && json_is_array(color) && json_array_size(color) >= 3) {
        mod.color[0] = (int)json_integer_value(json_array_get(color, 0));
        mod.color[1] = (int)json_integer_value(json_array_get(color, 1));
        mod.color[2] = (int)json_integer_value(json_array_get(color, 2));
    }

    json_t* global = json_object_get(root, "runsGlobally");
    if (global && json_is_boolean(global)) {
        mod.runsGlobally = json_is_true(global);
    } else {
        mod.runsGlobally = false;
    }

    json_decref(root);
}

void ModHandler::saveConfig() {
    std::string path = getWorkingBase() + "modsList.txt";
    std::ofstream file(path);
    if (!file.is_open()) return;
    
    for (const auto& mod : mods) {
        if (mod.folder.empty()) continue; // Skip root mod
        file << mod.folder << "|" << (mod.active ? "1" : "0") << "\n";
    }
    file.close();
}

void ModHandler::loadConfig() {
    std::string path = getWorkingBase() + "modsList.txt";
    std::ifstream file(path);
    if (!file.is_open()) return;
    
    std::vector<std::pair<std::string, bool>> list;
    std::string line;
    while (std::getline(file, line)) {
        // Strip trailing CR/LF
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (line.empty()) continue;
        
        size_t split = line.find('|');
        if (split != std::string::npos) {
            std::string folder = line.substr(0, split);
            bool active = (line.substr(split + 1) == "1");
            list.push_back({folder, active});
        }
    }
    
    std::vector<ModMetadata> sortedMods;
    
    // Always put root mod first if it exists
    for (const auto& mod : mods) {
        if (mod.folder.empty()) {
            sortedMods.push_back(mod);
            break;
        }
    }
    
    // Then add mods according to modsList.txt
    for (const auto& item : list) {
        for (auto it = mods.begin(); it != mods.end(); ++it) {
            if (it->folder == item.first) {
                it->active = item.second;
                sortedMods.push_back(*it);
                mods.erase(it);
                break;
            }
        }
    }
    
    // Add any remaining mods that weren't in modsList.txt (new mods)
    for (const auto& mod : mods) {
        if (!mod.folder.empty()) {
            sortedMods.push_back(mod);
        }
    }
    
    mods = sortedMods;
}

void ModHandler::reorderMod(int from, int to) {
    if (from < 0 || from >= (int)mods.size() || to < 0 || to >= (int)mods.size()) return;
    // Don't allow reordering above direct mods if we want to keep them at top?
    // User said direct mods are ALWAYS at the top.
    
    ModMetadata mod = mods[from];
    mods.erase(mods.begin() + from);
    mods.insert(mods.begin() + to, mod);
    saveConfig();
}

std::string ModHandler::getModPath(const std::string& relativePath) {
    checkWorkingBase();



    // Global Mode: Search active global mods
    for (const auto& mod : mods) {
        if (!mod.active && mod.folder != currentModFolder) continue;
        
        // Metadata (weeks, song charts, song directories) are always searched
        // across all active mods regardless of runsGlobally.
        bool isMetadata = (relativePath.find("weeks/") == 0 || 
                            relativePath.find("data/") == 0 || 
                            relativePath.find("songs/") == 0);

        if (!isMetadata && !mod.runsGlobally && mod.folder != currentModFolder) continue;
        
        std::string fullPath = workingBase;
        if (!mod.folder.empty()) fullPath += mod.folder + "/";
        fullPath += relativePath;
        
        if (fileExists(fullPath)) {
            return fullPath;
        }
    }
    return "";
}

std::string ModHandler::getModFolderOfFile(const std::string& relativePath) {
    checkWorkingBase();
    for (const auto& mod : mods) {
        if (!mod.active && mod.folder != currentModFolder) continue;
        
        std::string fullPath = workingBase;
        if (!mod.folder.empty()) fullPath += mod.folder + "/";
        fullPath += relativePath;
        if (fileExists(fullPath)) return mod.folder;
    }

    return "";
}

std::vector<ModSong> ModHandler::getAllSongs() {
    std::vector<ModSong> allSongs;
    checkWorkingBase();

    for (const auto& mod : mods) {
        if (!mod.active) continue; // Skip inactive mods
        std::string songsPath = workingBase;
        if (!mod.folder.empty()) songsPath += mod.folder + "/";
        songsPath += "songs/";
        
        DIR* dir = opendir(songsPath.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] == '.') continue;
                
                std::string songFolder = entry->d_name;
                std::string fullSongPath = songsPath + songFolder;
                
                struct stat st;
                if (stat(fullSongPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    ModSong s;
                    s.name = songFolder;
                    s.modFolder = mod.folder;
                    s.color[0] = mod.color[0];
                    s.color[1] = mod.color[1];
                    s.color[2] = mod.color[2];
                    
                    // Check if song already exists in list (higher priority mod version should be kept)
                    bool exists = false;
                    for (const auto& existing : allSongs) {
                        if (existing.name == s.name) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) allSongs.push_back(s);
                }
            }
            closedir(dir);
        }
    }
    return allSongs;
}

std::vector<ModWeek> ModHandler::getAllWeeks() {
    std::vector<ModWeek> allWeeks;
    for(const auto& m : mods) {
        if (!m.active) continue; // Skip inactive mods
        auto w = getWeeksFromMod(m.folder);
        allWeeks.insert(allWeeks.end(), w.begin(), w.end());
    }
    return allWeeks;
}

std::vector<ModSong> ModHandler::getSongsFromMod(const std::string& folder) {
    std::vector<ModSong> songs;
    checkWorkingBase();
    
    // Find the mod metadata to get the color
    ModMetadata* targetMod = nullptr;
    for (auto& m : mods) {
        if (m.folder == folder) { targetMod = &m; break; }
    }

    std::string songsPath = workingBase;
    if (!folder.empty()) songsPath += folder + "/";
    songsPath += "songs/";
    
    DIR* dir = opendir(songsPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string fullSongPath = songsPath + entry->d_name;
            struct stat st;
            if (stat(fullSongPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                ModSong s;
                s.name = entry->d_name;
                s.modFolder = folder;
                if (targetMod) {
                    s.color[0] = targetMod->color[0];
                    s.color[1] = targetMod->color[1];
                    s.color[2] = targetMod->color[2];
                }
                songs.push_back(s);
            }
        }
        closedir(dir);
    }
    return songs;
}

std::vector<ModWeek> ModHandler::getWeeksFromMod(const std::string& folder) {
    std::vector<ModWeek> weeks;
    checkWorkingBase();

    std::string weeksPath = workingBase;
    if (!folder.empty()) weeksPath += folder + "/";
    weeksPath += "weeks/";
    
    DIR* dir = opendir(weeksPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string name = entry->d_name;
            if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
                ModWeek w;
                w.key = name.substr(0, name.size() - 5);
                w.modFolder = folder;
                weeks.push_back(w);
            }
        }
        closedir(dir);
    }
    return weeks;
}

bool ModHandler::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}
