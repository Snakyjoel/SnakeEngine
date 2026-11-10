#include "WeekParser.hpp"
#include <jansson.h>
#include <stdio.h>
#include <algorithm>
#include <ctype.h>

WeekData WeekParser::loadJson(const std::string& path) {
    WeekData week;
    json_t *root;
    json_error_t error;

    root = json_load_file(path.c_str(), 0, &error);
    if (!root) {
        printf("\x1b[20;1HERROR WEEK JSON: %s\x1b[K\n", path.c_str());
        return week;
    }

    // Week Info
    json_t *name = json_object_get(root, "weekName");
    if (json_is_string(name)) week.weekName = json_string_value(name);

    json_t *story = json_object_get(root, "storyName");
    if (json_is_string(story)) week.storyName = json_string_value(story);

    json_t *bg = json_object_get(root, "weekBackground");
    if (json_is_string(bg)) week.weekBackground = json_string_value(bg);

    json_t *diffs = json_object_get(root, "difficulties");
    if (json_is_string(diffs)) week.difficulties = json_string_value(diffs);

    // Songs parsing
    json_t *songsArr = json_object_get(root, "songs");
    if (json_is_array(songsArr)) {
        size_t index; json_t *val;
        json_array_foreach(songsArr, index, val) {
            if (json_is_array(val) && json_array_size(val) >= 2) {
                SongInfo song;
                json_t* sName = json_array_get(val, 0);
                json_t* sIcon = json_array_get(val, 1);
                
                if (sName && json_is_string(sName)) song.name = json_string_value(sName);
                if (sIcon && json_is_string(sIcon)) song.icon = json_string_value(sIcon);
                
                json_t *colors = json_array_get(val, 2);
                if (colors && json_is_array(colors) && json_array_size(colors) >= 3) {
                    song.color[0] = (int)json_integer_value(json_array_get(colors, 0));
                    song.color[1] = (int)json_integer_value(json_array_get(colors, 1));
                    song.color[2] = (int)json_integer_value(json_array_get(colors, 2));
                } else {
                    song.color[0] = 100; song.color[1] = 100; song.color[2] = 100;
                }
                
                json_t* sIntro = json_array_get(val, 3);
                json_t* sOutro = json_array_get(val, 4);
                if (sIntro && json_is_string(sIntro)) song.introVideo = json_string_value(sIntro);
                if (sOutro && json_is_string(sOutro)) song.outroVideo = json_string_value(sOutro);
                
                json_t* sChar = json_array_get(val, 5);
                if (sChar && json_is_string(sChar)) song.freeplayCharacter = json_string_value(sChar);
                else song.freeplayCharacter = "bf";

                // Search for stars number, object or array in the elements from index 3 onwards
                size_t arrSize = json_array_size(val);
                for (size_t k = 3; k < arrSize; k++) {
                    json_t* item = json_array_get(val, k);
                    if (!item) continue;
                    if (json_is_integer(item) || json_is_number(item)) {
                        int s = json_is_integer(item) ? (int)json_integer_value(item) : (int)json_number_value(item);
                        if (s < 0) s = 0;
                        if (s > 30) s = 30;
                        song.stars = s;
                        break;
                    } else if (json_is_object(item)) {
                        json_t* alb = json_object_get(item, "album");
                        if (alb && json_is_string(alb)) {
                            song.album = json_string_value(alb);
                        }

                        const char* diffKey;
                        json_t* diffVal;
                        json_object_foreach(item, diffKey, diffVal) {
                            if (strcmp(diffKey, "album") == 0) continue;
                            int s = 0;
                            if (json_is_integer(diffVal)) s = (int)json_integer_value(diffVal);
                            else if (json_is_number(diffVal)) s = (int)json_number_value(diffVal);
                            else if (json_is_string(diffVal)) s = atoi(json_string_value(diffVal));
                            if (s < 0) s = 0;
                            if (s > 30) s = 30;
                            std::string dk(diffKey);
                            for (char& c : dk) c = (char)tolower((unsigned char)c);
                            song.difficultyStars[dk] = s;
                        }
                        if (song.difficultyStars.count("normal")) song.stars = song.difficultyStars["normal"];
                        else if (!song.difficultyStars.empty()) song.stars = song.difficultyStars.begin()->second;
                        break;
                    } else if (json_is_array(item)) {
                        size_t nStars = json_array_size(item);
                        for (size_t si = 0; si < nStars; si++) {
                            json_t* starItem = json_array_get(item, si);
                            int s = 0;
                            if (json_is_integer(starItem)) s = (int)json_integer_value(starItem);
                            else if (json_is_number(starItem)) s = (int)json_number_value(starItem);
                            if (s < 0) s = 0;
                            if (s > 30) s = 30;
                            song.difficultyStarsArray.push_back(s);
                        }
                        if (!song.difficultyStarsArray.empty()) {
                            song.stars = (song.difficultyStarsArray.size() > 1) ? song.difficultyStarsArray[1] : song.difficultyStarsArray[0];
                        }
                        break;
                    }
                }
                
                week.songs.push_back(song);
            }
        }
    }
    // Parsing weekBefore
    json_t *wBefore = json_object_get(root, "weekBefore");
    if (json_is_string(wBefore)) week.weekBefore = json_string_value(wBefore);

    // Parsing bools
    json_t *startUnlocked = json_object_get(root, "startUnlocked");
    if (json_is_boolean(startUnlocked)) week.startUnlocked = json_is_true(startUnlocked);

    json_t *hideStoryMode = json_object_get(root, "hideStoryMode");
    if (json_is_boolean(hideStoryMode)) week.hideStoryMode = json_is_true(hideStoryMode);

    json_t *hideFreeplay = json_object_get(root, "hideFreeplay");
    if (json_is_boolean(hideFreeplay)) week.hideFreeplay = json_is_true(hideFreeplay);

    json_t *hiddenUntilUnlocked = json_object_get(root, "hiddenUntilUnlocked");
    if (json_is_boolean(hiddenUntilUnlocked)) week.hiddenUntilUnlocked = json_is_true(hiddenUntilUnlocked);

    // Parsing weekCharacters
    json_t *charsArr = json_object_get(root, "weekCharacters");
    if (json_is_array(charsArr)) {
        size_t index; json_t *val;
        json_array_foreach(charsArr, index, val) {
            if (json_is_string(val)) {
                week.weekCharacters.push_back(json_string_value(val));
            }
        }
    }

    // Parse album field
    json_t *albumVal = json_object_get(root, "album");
    if (json_is_string(albumVal)) {
        week.album = json_string_value(albumVal);
    }

    // Parse ost field
    json_t *ostVal = json_object_get(root, "ost");
    if (json_is_string(ostVal)) {
        week.ost = json_string_value(ostVal);
    }

    // Parse stars (0 to 30), object or array
    json_t *starsVal = json_object_get(root, "stars");
    if (!starsVal) starsVal = json_object_get(root, "ratings");
    if (starsVal) {
        int s = 0;
        if (json_is_integer(starsVal)) s = (int)json_integer_value(starsVal);
        else if (json_is_number(starsVal)) s = (int)json_number_value(starsVal);
        else if (json_is_string(starsVal)) s = atoi(json_string_value(starsVal));
        else if (json_is_object(starsVal)) {
            const char* diffKey;
            json_t* diffVal;
            json_object_foreach(starsVal, diffKey, diffVal) {
                int ds = 0;
                if (json_is_integer(diffVal)) ds = (int)json_integer_value(diffVal);
                else if (json_is_number(diffVal)) ds = (int)json_number_value(diffVal);
                else if (json_is_string(diffVal)) ds = atoi(json_string_value(diffVal));
                if (ds < 0) ds = 0;
                if (ds > 30) ds = 30;
                std::string dk(diffKey);
                for (char& c : dk) c = (char)tolower((unsigned char)c);
                week.difficultyStars[dk] = ds;
            }
            if (week.difficultyStars.count("normal")) s = week.difficultyStars["normal"];
            else if (!week.difficultyStars.empty()) s = week.difficultyStars.begin()->second;
        } else if (json_is_array(starsVal)) {
            size_t nStars = json_array_size(starsVal);
            for (size_t si = 0; si < nStars; si++) {
                json_t* starItem = json_array_get(starsVal, si);
                int ds = 0;
                if (json_is_integer(starItem)) ds = (int)json_integer_value(starItem);
                else if (json_is_number(starItem)) ds = (int)json_number_value(starItem);
                if (ds < 0) ds = 0;
                if (ds > 30) ds = 30;
                week.difficultyStarsArray.push_back(ds);
            }
            if (!week.difficultyStarsArray.empty()) {
                s = (week.difficultyStarsArray.size() > 1) ? week.difficultyStarsArray[1] : week.difficultyStarsArray[0];
            }
        }
        if (s < 0) s = 0;
        if (s > 30) s = 30;
        week.stars = s;
    }

    // Fallback: apply week stars to songs that didn't define any
    for (auto& song : week.songs) {
        if (song.stars == 0 && song.difficultyStars.empty() && song.difficultyStarsArray.empty()) {
            song.stars = week.stars;
            song.difficultyStars = week.difficultyStars;
            song.difficultyStarsArray = week.difficultyStarsArray;
        }
    }

    // Parse songAlbum object mapping keys in lowercase
    json_t *songAlbumObj = json_object_get(root, "songAlbum");
    if (json_is_object(songAlbumObj)) {
        const char *key;
        json_t *val;
        json_object_foreach(songAlbumObj, key, val) {
            if (json_is_string(val)) {
                std::string keyLower = key;
                std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
                week.songAlbum[keyLower] = json_string_value(val);
            }
        }
    }

    json_decref(root);
    return week;
}
