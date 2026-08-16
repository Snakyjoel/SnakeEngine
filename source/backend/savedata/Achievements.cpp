#include "Achievements.hpp"
#include "ModHandler.hpp"
#include <jansson.h>
#include <fstream>
#include <3ds.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>

std::vector<AchievementInfo> Achievements::achievementsStuff = {
    {"Freaky on a Friday Night",  "Play on a Friday... Night.",                       "friday_night_play",   true},
    {"She Calls Me Daddy Too",    "Beat Week 1 on Hard with no Misses.",              "week1_nomiss",        false},
    {"No More Tricks",            "Beat Week 2 on Hard with no Misses.",              "week2_nomiss",        false},
    {"Call Me The Hitman",        "Beat Week 3 on Hard with no Misses.",              "week3_nomiss",        false},
    {"Lady Killer",               "Beat Week 4 on Hard with no Misses.",              "week4_nomiss",        false},
    {"Missless Christmas",        "Beat Week 5 on Hard with no Misses.",              "week5_nomiss",        false},
    {"Highscore!!",               "Beat Week 6 on Hard with no Misses.",              "week6_nomiss",        false},
    {"God Effing Damn It!",       "Beat Week 7 on Hard with no Misses.",              "week7_nomiss",        false},
    {"What a Funkin' Disaster!",  "Complete a Song with a rating lower than 20%.",    "ur_bad",              false},
    {"Perfectionist",             "Complete a Song with a rating of 100%.",           "ur_good",             false},
    {"Oversinging Much...?",      "Hold down a note for 10 seconds.",                 "oversinging",         false},
    {"Hyperactive",               "Finish a Song without going Idle.",                "hype",                false},
    {"Just the Two of Us",        "Finish a Song pressing only two keys.",            "two_keys",            false},
    {"Potato 3DS",                "Have you tried to run the game on an Old 3DS?",    "toastie",             false}
};

std::map<std::string, bool> Achievements::achievementsMap;
std::vector<std::string> Achievements::sessionUnlocks;

void Achievements::loadAchievements() {
    std::string basePath = ModHandler::getWorkingBase();
    std::string savesPath = basePath + "saves/";
    mkdir(basePath.c_str(), 0777);
    mkdir(savesPath.c_str(), 0777);

    json_error_t error;
    json_t* root = json_load_file((savesPath + "achievements.json").c_str(), 0, &error);
    
    if (root) {
        const char* key;
        json_t* value;
        json_object_foreach(root, key, value) {
            if (json_is_boolean(value)) {
                achievementsMap[std::string(key)] = json_boolean_value(value);
            }
        }
        json_decref(root);
    }

    // Evaluate Potato 3DS (Old 3DS models)
    bool isNew = false;
    APT_CheckNew3DS(&isNew);
    if (!isNew) {
        if (!isAchievementUnlocked("toastie")) {
            achievementsMap["toastie"] = true;
            saveAchievements(); // Silent save, no session unlock since it's on boot
        }
    }
}

void Achievements::resetAchievements() {
    achievementsMap.clear();
    std::string savesPath = ModHandler::getWorkingBase() + "saves/achievements.json";
    remove(savesPath.c_str());
    loadAchievements();
}

void Achievements::saveAchievements() {
    std::string basePath = ModHandler::getWorkingBase();
    std::string savesPath = basePath + "saves/";
    mkdir(basePath.c_str(), 0777);
    mkdir(savesPath.c_str(), 0777);

    json_t* root = json_object();
    for (const auto& kv : achievementsMap) {
        json_object_set_new(root, kv.first.c_str(), json_boolean(kv.second));
    }
    
    json_dump_file(root, (savesPath + "achievements.json").c_str(), JSON_INDENT(4));
    json_decref(root);
}

bool Achievements::isAchievementUnlocked(const std::string& name) {
    auto it = achievementsMap.find(name);
    if (it != achievementsMap.end()) {
        return it->second;
    }
    return false;
}

bool Achievements::unlockAchievement(const std::string& name) {
    if (!isAchievementUnlocked(name)) {
        achievementsMap[name] = true;
        
        // Ensure it's only in the session list once
        if (std::find(sessionUnlocks.begin(), sessionUnlocks.end(), name) == sessionUnlocks.end()) {
            sessionUnlocks.push_back(name);
        }
        
        saveAchievements();
        return true;
    }
    return false;
}

int Achievements::getAchievementIndex(const std::string& name) {
    for (size_t i = 0; i < achievementsStuff.size(); i++) {
        if (achievementsStuff[i].saveTag == name) {
            return (int)i;
        }
    }
    return -1;
}
