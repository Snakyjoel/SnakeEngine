#include "Achievements.hpp"
#include "ModHandler.hpp"
#include <jansson.h>
#include <fstream>
#include <3ds.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>

std::vector<AchievementInfo> Achievements::achievementsStuff = {
    {"I Said Funkin'",            "Start the game for the first time.",                                                                         "startgame",             false, false},
    {"That's How You Do It!",      "Beat Tutorial in Story Mode (on any difficulty).",                                                           "tutorial",              false, false},
    {"More Like Daddy Queerest",   "Beat Week 1 in Story Mode (on any difficulty).",                                                             "week1",                 false, false},
    {"IT IS THE SPOOKY MONTH",     "Beat Week 2 in Story Mode (on any difficulty).",                                                             "week2",                 false, false},
    {"Pico Funny",                 "Beat Week 3 in Story Mode (on any difficulty).",                                                             "week3",                 false, false},
    {"Mommy Must Murder",          "Beat Week 4 in Story Mode (on any difficulty).",                                                             "week4",                 false, false},
    {"Yule Tide Joy",              "Beat Week 5 in Story Mode (on any difficulty).",                                                             "week5",                 false, false},
    {"A Visual Novelty",           "Beat Week 6 in Story Mode (on any difficulty).",                                                             "week6",                 false, false},
    {"I <3 JohnnyUtah",            "Beat Week 7 in Story Mode (on any difficulty).",                                                             "week7",                 false, false},
    {"Yo, Really Think So?",       "Beat Weekend 1 in Story Mode (on any difficulty) and unlock Pico as a playable character in Freeplay.",      "weekend1",              false, true },
    {"Stay Funky",                 "Press SELECT in Freeplay and unlock your first character.",                                                 "charSelect",            false, true },
    {"A Challenger Appears",       "Beat any Pico remix in Freeplay (on any difficulty).",                                                       "picoMix",               false, true },
    {"De-Stressing",               "Beat Stress (Pico Mix) in Freeplay (on any difficulty).",                                                   "stressPico",            false, true },
    {"L",                          "Earn a Loss rating on any song (on any difficulty).",                                                        "loss",                  false, false},
    {"Getting Freaky",             "Earn a Perfect rating on any song on Hard difficulty.",                                                      "PerfectRatingHard",     false, false},
    {"Harder Than Hard",           "Beat any Erect remix in Freeplay on Erect or Nightmare difficulty.",                                         "beatErectOrNightmare",  false, false},
    {"You Should Drink More Water","Earn a Gold Perfect rating on any song on Hard difficulty.",                                                "perfectHard",           false, false},
    {"The Rap God",                "Earn a Gold Perfect rating on any song on Nightmare difficulty.",                                            "perfectNightmare",      false, false},
    {"Just like the game!",        "Get freaky on a Friday.",                                                                                    "justlikethegame",       false, false},
    {"Nice",                       "Earn a rating of EXACTLY 69% (good luck)",                                                                   "69",                    true,  false},
    {"Eat It Up!",                 "Beat Collab 1 in Story Mode (on any difficulty).",                                                           "collab1",               false, true }
};

std::map<std::string, bool> Achievements::achievementsMap;
std::vector<std::string> Achievements::sessionUnlocks;

void Achievements::loadAchievements() {
    std::string path = ModHandler::getWorkingBase() + "achievements.json";
    
    json_error_t error;
    json_t* root = json_load_file(path.c_str(), 0, &error);
    
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
}

void Achievements::resetAchievements() {
    achievementsMap.clear();
    std::string path = ModHandler::getWorkingBase() + "achievements.json";
    remove(path.c_str());
    loadAchievements();
}

void Achievements::saveAchievements() {
    std::string basePath = ModHandler::getWorkingBase();
    mkdir(basePath.c_str(), 0777);

    std::string path = basePath + "achievements.json";
    json_t* root = json_object();
    for (const auto& kv : achievementsMap) {
        json_object_set_new(root, kv.first.c_str(), json_boolean(kv.second));
    }
    
    json_dump_file(root, path.c_str(), JSON_INDENT(4));
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
