#pragma once
#include <string>
#include <vector>
#include <map>

struct SongInfo {
    std::string name;
    std::string icon;
    int color[3];
    std::string introVideo;
    std::string outroVideo;
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

    static std::map<std::string, WeekData> weeksLoaded;
    static std::vector<std::string> weeksList;

    static void reloadWeekFiles(bool includeMods = true);
    static SongInfo findSongInfo(const std::string& songName);
};
