#pragma once
#include <vector>

struct BpmChangeEvent {
    float stepTime;
    float songTime;
    float bpm;
    float stepCrochet;
};

class Conductor {
public:
    static float bpm;
    static float crochet;
    static float stepCrochet;
    static double songPosition;
    static std::vector<BpmChangeEvent> bpmChangeMap;

    static void changeBPM(float newBpm);
    static void update(double newPos);
    static float getStep();
    static int getBeat();
};
