#pragma once
#include <string>
#include <vector>

class Paths {
public:
    static std::string getPath(const std::string& file, const std::string& type, const std::string& library = "");
    
    static std::string image(const std::string& key, const std::string& library = "");
    static std::string xml(const std::string& key, const std::string& library = "");
    static std::string json(const std::string& key, const std::string& library = "");
    static std::string audio(const std::string& folder, const std::string& filename);
    static std::string font(const std::string& key);
    static std::string txt(const std::string& key, const std::string& library = "");
    static std::string weeksDir();
    
    static std::string characterJson(const std::string& character);
    static std::string characterCache(const std::string& character);
    static std::string healthIcon(const std::string& icon);
    static std::string weekJson(const std::string& name);
    static std::string songJson(const std::string& song, const std::string& difficulty = "");
    static std::string stageJson(const std::string& stage);
    static std::string songDataDir(const std::string& song);  // romfs:/preload/data/<song>/
    static std::vector<std::string> songLuaScripts(const std::string& song); // all .lua files in song data dir
    static std::vector<std::string> globalLuaScripts(); // all global .lua files
    static std::vector<std::string> eventLuaScripts(const std::vector<std::string>& events); // specific custom event .lua files
    static std::string customNoteLuaScript(const std::string& noteType);

    static std::string resolve(const std::string& path);
    static bool fileExists(const std::string& path);
};

#include <citro2d.h>
C2D_SpriteSheet Paths_loadSpriteSheet(const char* path);
void Paths_freeSpriteSheet(C2D_SpriteSheet sheet);
#define C2D_SpriteSheetLoad(path) Paths_loadSpriteSheet(path)
#define C2D_SpriteSheetFree(sheet) Paths_freeSpriteSheet(sheet)
