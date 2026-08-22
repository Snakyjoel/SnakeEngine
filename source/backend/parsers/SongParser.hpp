#pragma once
#include <vector>
#include <string>
#include "Note.hpp"
#include <algorithm>

struct Event {
    float strumTime;
    std::string name;
    std::string value1;
    std::string value2;

    bool operator<(const Event& other) const {
        return strumTime < other.strumTime;
    }
};

struct SectionMetadata {
    float startTime;
    float endTime;
    bool mustHitSection;
    bool gfSection;
};

struct SongData {
    std::vector<Note> notes;
    std::vector<SectionMetadata> sections;
    std::vector<Event> events;
};

class SongParser {
public:
    static float songSpeed;
    static float originalSongSpeed;
    static std::string player1;
    static std::string player2;
    static std::string gfVersion;
    static std::string stage;
    static std::string arrowSkin;
    static std::string ratingSkin;
    static SongData loadJson(const std::string& path);
};
