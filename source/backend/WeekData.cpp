#include "WeekData.hpp"
#include "WeekParser.hpp"
#include "ModHandler.hpp"
#include <fstream>
#include <dirent.h>
#include <algorithm>

std::map<std::string, WeekData> WeekData::weeksLoaded;
std::vector<std::string> WeekData::weeksList;

void WeekData::reloadWeekFiles(bool includeMods) {
    weeksLoaded.clear();
    weeksList.clear();

    std::string base = ModHandler::getWorkingBase();
    if (base.empty()) {
        #ifdef _3DS
        base = "sdmc:/SnakeEngine/";
        #else
        base = "SnakeEngine/";
        #endif
    }
    if (!base.empty() && base.back() != '/') base += "/";

    struct DirInfo {
        std::string path;
        std::string modFolder;
        bool isMod;
    };

    std::vector<DirInfo> dirs;
    
    // 1. Direct mods
    if (includeMods) {
        dirs.push_back({base + "mods/", "mods", true});
    }

    // 2. Base game paths
    dirs.push_back({"romfs:/shared/", "", false});
    dirs.push_back({"romfs:/preload/", "", false});

    // 3. Mod folders in priority order
    if (includeMods) {
        for (const auto& mod : ModHandler::get().getMods()) {
            if (!mod.active) continue;
            if (mod.folder.empty() || mod.folder == "mods") continue;
            dirs.push_back({base + mod.folder + "/", mod.folder, true});
        }
    }

    // Pass 1: Load base game weeks (from the main weekList.txt)
    std::string baseWeekListPath = "romfs:/shared/weeks/weekList.txt";
    if (!Paths::fileExists(baseWeekListPath)) {
        baseWeekListPath = "romfs:/preload/weeks/weekList.txt";
    }

    std::ifstream listFile(baseWeekListPath);
    std::vector<std::string> baseWeekList;
    if (listFile.is_open()) {
        std::string line;
        while (std::getline(listFile, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (!line.empty()) {
                baseWeekList.push_back(line);
            }
        }
        listFile.close();
    }

    for (const auto& weekKey : baseWeekList) {
        for (const auto& dir : dirs) {
            std::string jsonPath = dir.path + "weeks/" + weekKey + ".json";
            if (Paths::fileExists(jsonPath)) {
                if (weeksLoaded.find(weekKey) == weeksLoaded.end()) {
                    WeekData data = WeekParser::loadJson(jsonPath);
                    if (!data.songs.empty()) {
                        data.fileName = weekKey;
                        data.isMod = dir.isMod;
                        data.modFolder = dir.modFolder;
                        weeksLoaded[weekKey] = data;
                        weeksList.push_back(weekKey);
                    }
                }
                break; // First match wins
            }
        }
    }

    // Pass 2: Load mod-specific weeks and scanned directories
    for (const auto& dir : dirs) {
        std::string dirWeeksPath = dir.path + "weeks/";
        
        // 2a. Load weeks from directory's weekList.txt if it exists
        std::string modWeekListPath = dirWeeksPath + "weekList.txt";
        if (Paths::fileExists(modWeekListPath)) {
            std::ifstream modListFile(modWeekListPath);
            if (modListFile.is_open()) {
                std::string line;
                while (std::getline(modListFile, line)) {
                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                        line.pop_back();
                    }
                    if (!line.empty()) {
                        std::string weekKey = line;
                        if (weeksLoaded.find(weekKey) == weeksLoaded.end()) {
                            std::string jsonPath = dirWeeksPath + weekKey + ".json";
                            if (Paths::fileExists(jsonPath)) {
                                WeekData data = WeekParser::loadJson(jsonPath);
                                if (!data.songs.empty()) {
                                    data.fileName = weekKey;
                                    data.isMod = dir.isMod;
                                    data.modFolder = dir.modFolder;
                                    weeksLoaded[weekKey] = data;
                                    weeksList.push_back(weekKey);
                                }
                            }
                        }
                    }
                }
                modListFile.close();
            }
        }

        // 2b. Scan weeks directory for any other .json files
        DIR* dp = opendir(dirWeeksPath.c_str());
        if (dp) {
            struct dirent* entry;
            std::vector<std::string> foundWeeks;
            while ((entry = readdir(dp)) != nullptr) {
                std::string name = entry->d_name;
                if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
                    foundWeeks.push_back(name.substr(0, name.size() - 5));
                }
            }
            closedir(dp);
            std::sort(foundWeeks.begin(), foundWeeks.end());
            for (const auto& weekKey : foundWeeks) {
                if (weeksLoaded.find(weekKey) == weeksLoaded.end()) {
                    std::string jsonPath = dirWeeksPath + weekKey + ".json";
                    WeekData data = WeekParser::loadJson(jsonPath);
                    if (!data.songs.empty()) {
                        data.fileName = weekKey;
                        data.isMod = dir.isMod;
                        data.modFolder = dir.modFolder;
                        weeksLoaded[weekKey] = data;
                        weeksList.push_back(weekKey);
                    }
                }
            }
        }
    }

}

SongInfo WeekData::findSongInfo(const std::string& songName) {
    if (weeksLoaded.empty()) {
        reloadWeekFiles(true);
    }
    std::string target = songName;
    std::transform(target.begin(), target.end(), target.begin(), ::tolower);
    for (auto const& pair : weeksLoaded) {
        for (auto const& song : pair.second.songs) {
            std::string name = song.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name == target) {
                return song;
            }
        }
    }
    return SongInfo{"", "", {100, 100, 100}, "", ""};
}
