#include "Highscores.hpp"
#include "ModHandler.hpp"
#include <jansson.h>
#include <sys/stat.h>
#include <sys/types.h>

std::map<std::string, int> Highscores::songScores;
std::map<std::string, float> Highscores::songAccuracies;
std::map<std::string, int> Highscores::weekScores;
std::map<std::string, float> Highscores::weekAccuracies;
std::map<std::string, std::string> Highscores::songRatings;
std::map<std::string, std::string> Highscores::weekRatings;
std::map<std::string, bool> Highscores::weekCompleted;

std::string Highscores::formatKey(const std::string& name, const std::string& diff) {
    std::string key = name;
    if (!diff.empty()) {
        key += "-" + diff;
    }
    return key;
}

void Highscores::load() {
    songScores.clear();
    songAccuracies.clear();
    weekScores.clear();
    weekAccuracies.clear();
    songRatings.clear();
    weekRatings.clear();
    weekCompleted.clear();

    std::string path = ModHandler::getWorkingBase() + "highscores.json";
    json_error_t error;
    json_t *root = json_load_file(path.c_str(), 0, &error);
    if (!root) return;

    json_t *sScores = json_object_get(root, "songScores");
    if (sScores && json_is_object(sScores)) {
        const char *key;
        json_t *value;
        json_object_foreach(sScores, key, value) {
            if (json_is_integer(value)) {
                songScores[key] = (int)json_integer_value(value);
            }
        }
    }

    json_t *sAccs = json_object_get(root, "songAccuracies");
    if (sAccs && json_is_object(sAccs)) {
        const char *key;
        json_t *value;
        json_object_foreach(sAccs, key, value) {
            if (json_is_number(value)) {
                songAccuracies[key] = (float)json_number_value(value);
            }
        }
    }

    json_t *wScores = json_object_get(root, "weekScores");
    if (wScores && json_is_object(wScores)) {
        const char *key;
        json_t *value;
        json_object_foreach(wScores, key, value) {
            if (json_is_integer(value)) {
                weekScores[key] = (int)json_integer_value(value);
            }
        }
    }

    json_t *wAccs = json_object_get(root, "weekAccuracies");
    if (wAccs && json_is_object(wAccs)) {
        const char *key;
        json_t *value;
        json_object_foreach(wAccs, key, value) {
            if (json_is_number(value)) {
                weekAccuracies[key] = (float)json_number_value(value);
            }
        }
    }

    json_t *sRatings = json_object_get(root, "songRatings");
    if (sRatings && json_is_object(sRatings)) {
        const char *key;
        json_t *value;
        json_object_foreach(sRatings, key, value) {
            if (json_is_string(value)) {
                songRatings[key] = json_string_value(value);
            }
        }
    }

    json_t *wRatings = json_object_get(root, "weekRatings");
    if (wRatings && json_is_object(wRatings)) {
        const char *key;
        json_t *value;
        json_object_foreach(wRatings, key, value) {
            if (json_is_string(value)) {
                weekRatings[key] = json_string_value(value);
            }
        }
    }

    json_t *wComp = json_object_get(root, "weekCompleted");
    if (wComp && json_is_object(wComp)) {
        const char *key;
        json_t *value;
        json_object_foreach(wComp, key, value) {
            if (json_is_boolean(value) && json_is_true(value)) {
                weekCompleted[key] = true;
            }
        }
    }

    json_decref(root);
}

void Highscores::reset() {
    songScores.clear();
    songAccuracies.clear();
    weekScores.clear();
    weekAccuracies.clear();
    songRatings.clear();
    weekRatings.clear();
    weekCompleted.clear();
    std::string path = ModHandler::getWorkingBase() + "highscores.json";
    remove(path.c_str());
    save();
}

void Highscores::save() {
    std::string basePath = ModHandler::getWorkingBase();
    mkdir(basePath.c_str(), 0777);

    json_t *root = json_object();

    json_t *sScores = json_object();
    for (auto const& pair : songScores) {
        json_object_set_new(sScores, pair.first.c_str(), json_integer(pair.second));
    }
    json_object_set_new(root, "songScores", sScores);

    json_t *sAccs = json_object();
    for (auto const& pair : songAccuracies) {
        json_object_set_new(sAccs, pair.first.c_str(), json_real(pair.second));
    }
    json_object_set_new(root, "songAccuracies", sAccs);

    json_t *wScores = json_object();
    for (auto const& pair : weekScores) {
        json_object_set_new(wScores, pair.first.c_str(), json_integer(pair.second));
    }
    json_object_set_new(root, "weekScores", wScores);

    json_t *wAccs = json_object();
    for (auto const& pair : weekAccuracies) {
        json_object_set_new(wAccs, pair.first.c_str(), json_real(pair.second));
    }
    json_object_set_new(root, "weekAccuracies", wAccs);

    json_t *sRatings = json_object();
    for (auto const& pair : songRatings) {
        json_object_set_new(sRatings, pair.first.c_str(), json_string(pair.second.c_str()));
    }
    json_object_set_new(root, "songRatings", sRatings);

    json_t *wRatings = json_object();
    for (auto const& pair : weekRatings) {
        json_object_set_new(wRatings, pair.first.c_str(), json_string(pair.second.c_str()));
    }
    json_object_set_new(root, "weekRatings", wRatings);

    json_t *wComp = json_object();
    for (auto const& pair : weekCompleted) {
        if (pair.second) {
            json_object_set_new(wComp, pair.first.c_str(), json_true());
        }
    }
    json_object_set_new(root, "weekCompleted", wComp);

    std::string path = basePath + "highscores.json";
    json_dump_file(root, path.c_str(), 0);
    json_decref(root);
}

void Highscores::saveScore(const std::string& song, int score, const std::string& diff) {
    std::string key = formatKey(song, diff);
    if (songScores.find(key) == songScores.end() || score > songScores[key]) {
        songScores[key] = score;
        save();
    }
}

void Highscores::saveAccuracy(const std::string& song, float accuracy, const std::string& diff) {
    std::string key = formatKey(song, diff);
    if (songAccuracies.find(key) == songAccuracies.end() || accuracy > songAccuracies[key]) {
        songAccuracies[key] = accuracy;
        save();
    }
}

void Highscores::saveWeekScore(const std::string& week, int score, const std::string& diff) {
    weekCompleted[week] = true;
    std::string key = formatKey(week, diff);
    if (weekScores.find(key) == weekScores.end() || score > weekScores[key]) {
        weekScores[key] = score;
        save();
    } else {
        save();
    }
}

int Highscores::getScore(const std::string& song, const std::string& diff) {
    std::string key = formatKey(song, diff);
    if (songScores.find(key) != songScores.end()) {
        return songScores[key];
    }
    return 0;
}

float Highscores::getAccuracy(const std::string& song, const std::string& diff) {
    std::string key = formatKey(song, diff);
    if (songAccuracies.find(key) != songAccuracies.end()) {
        return songAccuracies[key];
    }
    return 0.0f;
}

int Highscores::getWeekScore(const std::string& week, const std::string& diff) {
    std::string key = formatKey(week, diff);
    if (weekScores.find(key) != weekScores.end()) {
        return weekScores[key];
    }
    return 0;
}

void Highscores::saveRating(const std::string& song, const std::string& rating, const std::string& diff, float newAccuracy) {
    std::string key = formatKey(song, diff);
    if (songRatings.find(key) == songRatings.end() || newAccuracy >= getAccuracy(song, diff)) {
        songRatings[key] = rating;
        save();
    }
}

std::string Highscores::getRating(const std::string& song, const std::string& diff) {
    std::string key = formatKey(song, diff);
    if (songRatings.find(key) != songRatings.end()) {
        return songRatings[key];
    }
    return "";
}
void Highscores::saveWeekAccuracy(const std::string& week, float accuracy, const std::string& diff) {
    std::string key = formatKey(week, diff);
    if (weekAccuracies.find(key) == weekAccuracies.end() || accuracy > weekAccuracies[key]) {
        weekAccuracies[key] = accuracy;
        save();
    }
}

float Highscores::getWeekAccuracy(const std::string& week, const std::string& diff) {
    std::string key = formatKey(week, diff);
    if (weekAccuracies.find(key) != weekAccuracies.end()) {
        return weekAccuracies[key];
    }
    if (songAccuracies.find(key) != songAccuracies.end()) {
        return songAccuracies[key];
    }
    return 0.0f;
}

void Highscores::saveWeekRating(const std::string& week, const std::string& rating, const std::string& diff, float newAccuracy) {
    std::string key = formatKey(week, diff);
    if (weekRatings.find(key) == weekRatings.end() || newAccuracy >= getWeekAccuracy(week, diff)) {
        weekRatings[key] = rating;
        save();
    }
}

std::string Highscores::getWeekRating(const std::string& week, const std::string& diff) {
    std::string key = formatKey(week, diff);
    if (weekRatings.find(key) != weekRatings.end()) {
        return weekRatings[key];
    }
    if (songRatings.find(key) != songRatings.end()) {
        return songRatings[key];
    }
    return "";
}


bool Highscores::isWeekCompleted(const std::string& week) {
    if (week.empty()) return true;

    auto toLower = [](const std::string& s) {
        std::string res = s;
        for (char& c : res) c = (char)tolower((unsigned char)c);
        return res;
    };

    std::string lowerWeek = toLower(week);

    for (const auto& pair : weekCompleted) {
        if (pair.second && toLower(pair.first) == lowerWeek) {
            return true;
        }
    }

    for (const auto& pair : weekScores) {
        if (pair.second > 0) {
            std::string lowerKey = toLower(pair.first);
            if (lowerKey == lowerWeek || lowerKey.rfind(lowerWeek + "-", 0) == 0) {
                return true;
            }
        }
    }
    return false;
}
