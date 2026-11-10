#pragma once
#include <string>
#include <vector>
#include <map>
#include <initializer_list>

struct SongInfo {
    std::string name;
    std::string icon;
    int color[3];
    std::string introVideo;
    std::string outroVideo;
    std::string freeplayCharacter;
    int stars;
    std::string album;
    std::map<std::string, int> difficultyStars;
    std::vector<int> difficultyStarsArray;

    SongInfo() : stars(0) {
        color[0] = 100; color[1] = 100; color[2] = 100;
        freeplayCharacter = "bf";
    }

    SongInfo(const std::string& n, const std::string& ic, std::initializer_list<int> col, const std::string& in = "", const std::string& out = "", const std::string& fc = "bf", int st = 0)
        : name(n), icon(ic), introVideo(in), outroVideo(out), freeplayCharacter(fc), stars(st) {
        int i = 0;
        for (int v : col) {
            if (i < 3) color[i++] = v;
        }
        while (i < 3) color[i++] = 100;
    }

    int getStars(const std::string& diff = "", int diffIdx = -1) const {
        if (!diff.empty()) {
            std::string d = diff;
            for (char& c : d) c = (char)tolower((unsigned char)c);
            auto it = difficultyStars.find(d);
            if (it != difficultyStars.end()) return it->second;
        }
        if (diffIdx >= 0 && diffIdx < (int)difficultyStarsArray.size()) {
            return difficultyStarsArray[diffIdx];
        }
        return stars;
    }
};

class WeekData {
public:
    std::string weekName;
    std::string fileName;
    std::string storyName;
    std::string weekBefore;
    std::string weekBackground;
    std::vector<SongInfo> songs;
    std::vector<std::string> weekCharacters;
    std::string difficulties;
    bool startUnlocked = true;
    bool hideStoryMode = false;
    bool hideFreeplay = false;
    bool hiddenUntilUnlocked = false;
    bool isMod = false;
    std::string modFolder;
    
    // Album Art details
    std::string album;
    std::map<std::string, std::string> songAlbum;
    std::string ost;
    int stars = 0;
    std::map<std::string, int> difficultyStars;
    std::vector<int> difficultyStarsArray;

    static std::map<std::string, WeekData> weeksLoaded;
    static std::vector<std::string> weeksList;

    static void reloadWeekFiles(bool includeMods = true);
    static SongInfo findSongInfo(const std::string& songName);
};
