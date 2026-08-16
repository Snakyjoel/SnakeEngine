#pragma once
#include <string>
#include <vector>
#include <map>

struct ModMetadata {
    std::string folder;
    std::string name;
    std::string description;
    int color[3] = {40, 40, 45};
    bool hasPackJson = false;
    bool active = true;
    bool runsGlobally = false;
};

struct ModSong {
    std::string name;
    std::string modFolder;
    int color[3] = {40, 40, 45};
};

struct ModWeek {
    std::string key;
    std::string modFolder;
};

class ModHandler {
public:
    static ModHandler& get();
    
    void scanMods();
    void saveConfig();
    void loadConfig();
    
    std::string getModPath(const std::string& relativePath);
    std::string getModFolderOfFile(const std::string& relativePath);
    bool fileExists(const std::string& path);
    
    std::vector<ModMetadata>& getMods() { return mods; }
    void reorderMod(int from, int to);
    
    std::vector<ModSong> getAllSongs();
    std::vector<ModWeek> getAllWeeks();
    
    static std::string getWorkingBase();
    static std::string currentModFolder;
    std::string currentModBG = "";
    
    std::vector<ModSong> getSongsFromMod(const std::string& folder);
    std::vector<ModWeek> getWeeksFromMod(const std::string& folder);

private:
    ModHandler() {}
    std::vector<ModMetadata> mods;
    std::vector<std::string> priorityOrder;
    
    void parsePackJson(ModMetadata& mod, const std::string& jsonPath = "");
};
