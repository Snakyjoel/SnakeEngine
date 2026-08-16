#pragma once
#include <string>
#include <map>

class Highscores {
public:
    static std::map<std::string, int> songScores;
    static std::map<std::string, float> songAccuracies;
    static std::map<std::string, int> weekScores;
    static std::map<std::string, std::string> songRatings;

    static void load();
    static void reset();
    static void save();
    static void saveScore(const std::string& song, int score, const std::string& diff);
    static void saveAccuracy(const std::string& song, float accuracy, const std::string& diff);
    static void saveRating(const std::string& song, const std::string& rating, const std::string& diff, float newAccuracy);
    static void saveWeekScore(const std::string& week, int score, const std::string& diff);
    static int getScore(const std::string& song, const std::string& diff);
    static float getAccuracy(const std::string& song, const std::string& diff);
    static std::string getRating(const std::string& song, const std::string& diff);
    static int getWeekScore(const std::string& week, const std::string& diff);
    static std::string formatKey(const std::string& name, const std::string& diff);
};
