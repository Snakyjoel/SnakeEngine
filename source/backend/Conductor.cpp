#include "Conductor.hpp"
#include <cmath>

float Conductor::bpm = 100.0f;
float Conductor::crochet = ((60.0f / 100.0f) * 1000.0f);
float Conductor::stepCrochet = (((60.0f / 100.0f) * 1000.0f) / 4.0f);
double Conductor::songPosition = 0;
std::vector<BpmChangeEvent> Conductor::bpmChangeMap;

void Conductor::changeBPM(float newBpm) {
    bpm = newBpm;
    crochet = ((60.0f / bpm) * 1000.0f);
    stepCrochet = (crochet / 4.0f);
}

void Conductor::update(double newPos) {
    songPosition = newPos;
}

float Conductor::getStep() {
    // Use static stepCrochet as fallback
    BpmChangeEvent lastChange = {0, 0, bpm, stepCrochet};
    for (auto& ev : bpmChangeMap) {
        if (songPosition >= ev.songTime)
            lastChange = ev;
        else
            break; // Sorted by time
    }
    float steps = lastChange.stepTime + (float)(songPosition - lastChange.songTime) / lastChange.stepCrochet;
    return steps;
}

int Conductor::getBeat() {
    return (int)floor(getStep() / 4.0f);
}

