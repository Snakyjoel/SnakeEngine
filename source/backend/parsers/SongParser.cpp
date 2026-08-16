#include "SongParser.hpp"
#include "../backend/Conductor.hpp"
#include "../backend/ModHandler.hpp"
#include <jansson.h>

float SongParser::songSpeed = 1.0f;
std::string SongParser::player1 = "bf";
std::string SongParser::player2 = "dad";
std::string SongParser::gfVersion = "gf";
std::string SongParser::stage = "stage";
std::string SongParser::arrowSkin = "";
std::string SongParser::ratingSkin = "";

void parseEventsFromJson(json_t* songObj, std::vector<Event>& eventsList) {
    if (!songObj) return;
    json_t *eventsArr = json_object_get(songObj, "events");
    if (eventsArr && json_is_array(eventsArr)) {
        size_t size = json_array_size(eventsArr);
        for(size_t i = 0; i < size; i++) {
            json_t *eventNode = json_array_get(eventsArr, i);
            if (!json_is_array(eventNode) || json_array_size(eventNode) < 2) continue;
            float strumTime = (float)json_number_value(json_array_get(eventNode, 0));
            json_t *eventDetails = json_array_get(eventNode, 1);
            if (json_is_array(eventDetails)) {
                size_t numDetails = json_array_size(eventDetails);
                for(size_t j = 0; j < numDetails; j++) {
                    json_t *detail = json_array_get(eventDetails, j);
                    if (json_is_array(detail) && json_array_size(detail) >= 3) {
                        Event e;
                        e.strumTime = strumTime;
                        e.name = json_string_value(json_array_get(detail, 0));
                        e.value1 = json_string_value(json_array_get(detail, 1));
                        e.value2 = json_string_value(json_array_get(detail, 2));
                        eventsList.push_back(e);
                    }
                }
            }
        }
    }
}

SongData SongParser::loadJson(const std::string& path) {
    SongData data;
    Conductor::bpmChangeMap.clear();
    
    json_t *root;
    json_error_t error;
    
    root = json_load_file(path.c_str(), 0, &error);
    if(!root) return data;
    
    // Compatibility: Try to get "song" object, but fallback to root if not found
    json_t *songObj = json_object_get(root, "song");
    if(!songObj || !json_is_object(songObj)) {
        songObj = root; // Flat JSON format fallback
    }
    
    // Set default values
    player1 = "bf-pixel";
    player2 = "bf-pixel";
    gfVersion = "gf";
    stage = "stage";
    arrowSkin = "";
    ratingSkin = "";

    json_t *arrowS = json_object_get(songObj, "arrowSkin");
    if(arrowS && json_is_string(arrowS)) arrowSkin = json_string_value(arrowS);

    json_t *ratingS = json_object_get(songObj, "ratingSkin");
    if(ratingS && json_is_string(ratingS)) ratingSkin = json_string_value(ratingS);

    json_t *p1 = json_object_get(songObj, "player1");
    if(p1 && json_is_string(p1)) player1 = json_string_value(p1);
    
    json_t *p2 = json_object_get(songObj, "player2");
    if(p2 && json_is_string(p2)) player2 = json_string_value(p2);
    
    json_t *gfJson = json_object_get(songObj, "gfVersion");
    if(gfJson && json_is_string(gfJson)) gfVersion = json_string_value(gfJson);
    else {
        json_t *p3 = json_object_get(songObj, "player3");
        if(p3 && json_is_string(p3)) gfVersion = json_string_value(p3);
    }
    
    json_t *stg = json_object_get(songObj, "stage");
    if(stg && json_is_string(stg)) stage = json_string_value(stg);
    
    float rootBpm = 100.0f;
    json_t *bpmJson = json_object_get(songObj, "bpm");
    if(bpmJson && json_is_number(bpmJson)) rootBpm = (float)json_number_value(bpmJson);
    
    if (rootBpm <= 0) rootBpm = 100.0f; // Safety
    Conductor::changeBPM(rootBpm);
    
    BpmChangeEvent initial = {0, 0, rootBpm, (60.0f / rootBpm) * 1000.0f / 4.0f};
    Conductor::bpmChangeMap.push_back(initial);
    
    json_t *speed = json_object_get(songObj, "speed");
    if(speed && json_is_number(speed)) songSpeed = json_number_value(speed);
    
    json_t *format = json_object_get(songObj, "format");
    bool isLegacy = true;
    if (format && json_is_string(format)) {
        std::string fmt = json_string_value(format);
        if (fmt == "psych_v1_convert" || fmt == "psych_v1") {
            isLegacy = false; // Psych Engine 0.7+ / 1.0b+ uses absolute lanes
        }
    }

    json_t *sections = json_object_get(songObj, "notes");
    if (!sections || !json_is_array(sections)) {
        printf("WARN: Song 'notes' missing or not an array\n");
        json_decref(root);
        return data;
    }
    size_t secSize = json_array_size(sections);
    
    float curStep = 0;
    float curTime = 0;
    float lastBpm = rootBpm;

    for(size_t i = 0; i < secSize; i++) {
        json_t *section = json_array_get(sections, i);
        json_t *changeBPM = json_object_get(section, "changeBPM");
        json_t *secBpm = json_object_get(section, "bpm");
        if (changeBPM && json_is_true(changeBPM) && secBpm && json_is_number(secBpm)) {
            float nBpm = json_number_value(secBpm);
            if (nBpm != lastBpm) {
                BpmChangeEvent be = { curStep, curTime, nBpm, (60.0f / nBpm) * 1000.0f / 4.0f };
                Conductor::bpmChangeMap.push_back(be);
                lastBpm = nBpm;
            }
        }
        
        float beats = 4.0f;
        json_t *sBeats = json_object_get(section, "sectionBeats");
        if (sBeats && json_is_number(sBeats)) beats = json_number_value(sBeats);
        else {
            json_t *lSteps = json_object_get(section, "lengthInSteps");
            if (lSteps && json_is_number(lSteps)) beats = json_number_value(lSteps) / 4.0f;
        }

        curStep += beats * 4.0f;
        curTime += (60.0f / lastBpm) * 1000.0f * beats;

        json_t *mustHit = json_object_get(section, "mustHitSection");
        bool isMustHit = json_is_true(mustHit);

        json_t *gfSec = json_object_get(section, "gfSection");
        bool isGf = json_is_true(gfSec);

        SectionMetadata secMeta;
        secMeta.startTime = curTime - ((60.0f / lastBpm) * 1000.0f * beats);
        secMeta.endTime = curTime;
        secMeta.mustHitSection = isMustHit;
        secMeta.gfSection = isGf;
        data.sections.push_back(secMeta);
        
        json_t *sNotes = json_object_get(section, "sectionNotes");
        size_t noteSize = 0;
        if (sNotes && json_is_array(sNotes)) {
            noteSize = json_array_size(sNotes);
        }
        
        for(size_t j = 0; j < noteSize; j++) {
            json_t *nData = json_array_get(sNotes, j);
            Note n;
            n.strumTime = (float)json_number_value(json_array_get(nData, 0));
            int rawLane = json_integer_value(json_array_get(nData, 1));
            n.noteData = rawLane % 4;
            n.sustainLength = (float)json_number_value(json_array_get(nData, 2));
            
            // Determine if the note belongs to the player
            if (isLegacy) {
                if (isMustHit) n.isPlayer = (rawLane < 4);
                else           n.isPlayer = (rawLane >= 4);
            } else {
                n.isPlayer = (rawLane < 4); // Psych 1.0+ absolute lanes: 0-3 is Player, 4-7 is Opponent
            }

            json_t *noteType = json_array_get(nData, 3);
            if (noteType) {
                if (json_is_string(noteType)) {
                    std::string nt = json_string_value(noteType);
                    n.noteType = nt;
                    if (nt == "GF Sing") n.gfNote = true;
                } else if (json_is_number(noteType)) {
                    double val = json_number_value(noteType);
                    if (val == 1.0 || val == 4.0) n.gfNote = true;
                }
            }
            
            data.notes.push_back(n);
        }
    }
    
    parseEventsFromJson(songObj, data.events);
    
    std::string dirPath = path.substr(0, path.find_last_of("\\/"));
    std::string eventsPath = dirPath + "/events.json";
    json_error_t evtError;
    json_t* eventsRoot = json_load_file(eventsPath.c_str(), 0, &evtError);
    if (eventsRoot) {
        json_t* eventsSongObj = json_object_get(eventsRoot, "song");
        if (eventsSongObj) {
            parseEventsFromJson(eventsSongObj, data.events);
        } else {
            parseEventsFromJson(eventsRoot, data.events);
        }
        json_decref(eventsRoot);
    }
    std::sort(data.events.begin(), data.events.end());
    
    json_decref(root);
    return data;
}
