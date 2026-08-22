#pragma once
#include <string>
#include <vector>
#include <map>

struct AchievementInfo {
    std::string name;
    std::string description;
    std::string saveTag;
    bool hidden;
    bool notAvailable;
};

class Achievements {
public:
    static std::vector<AchievementInfo> achievementsStuff;
    static std::map<std::string, bool> achievementsMap;
    
    // Tracks achievements unlocked during the current play session
    static std::vector<std::string> sessionUnlocks;

    // Load from SD card and evaluate "Potato 3DS"
    static void loadAchievements();
    static void resetAchievements();
    
    // Save to SD card
    static void saveAchievements();

    // Check if unlocked
    static bool isAchievementUnlocked(const std::string& name);

    // Unlocks an achievement. Returns true if it was newly unlocked (adds to sessionUnlocks).
    static bool unlockAchievement(const std::string& name);

    // Returns index in achievementsStuff by saveTag
    static int getAchievementIndex(const std::string& name);
};
