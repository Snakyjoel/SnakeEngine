#include "Conductor.hpp"
#include <cmath>

float Conductor::bpm = 100.0f;
float Conductor::crochet = ((60.0f / 100.0f) * 1000.0f);
float Conductor::stepCrochet = (((60.0f / 100.0f) * 1000.0f) / 4.0f);
double Conductor::songPosition = 0;
double Conductor::rawPosition = 0;
double Conductor::integralError = 0;
std::vector<BpmChangeEvent> Conductor::bpmChangeMap;

void Conductor::changeBPM(float newBpm) {
    bpm = newBpm;
    crochet = ((60.0f / bpm) * 1000.0f);
    stepCrochet = (crochet / 4.0f);
}

void Conductor::update(double newPos) {
    songPosition = newPos;
    rawPosition = newPos;
}

void Conductor::resetSync() {
    integralError = 0.0;
    rawPosition = songPosition;
}

double Conductor::getSyncDrift() {
    return rawPosition - songPosition;
}

void Conductor::advanceTime(float dt, double hardwareAudioTime) {
    rawPosition = hardwareAudioTime;

    if (hardwareAudioTime <= 0.0 || songPosition < 0.0) {
        songPosition += dt * 1000.0f;
        integralError = 0.0;
        return;
    }

    songPosition += dt * 1000.0f;

    double error = hardwareAudioTime - songPosition;
    const double HARD_SNAP_THRESHOLD = 40.0;
    const double DEADBAND = 1.0;
    const double KP = 0.15;
    const double KI = 0.001;

    if (std::abs(error) > HARD_SNAP_THRESHOLD) {
        songPosition = hardwareAudioTime;
        integralError = 0.0;
    } else if (std::abs(error) > DEADBAND) {
        integralError += error * dt;
        if (integralError > 20.0) integralError = 20.0;
        else if (integralError < -20.0) integralError = -20.0;

        songPosition += (error * KP) + (integralError * KI);
    }
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

