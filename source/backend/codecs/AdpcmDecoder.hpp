#pragma once
#include <stdint.h>
#include <algorithm>
#include "AdpcmEncoder.hpp" // For stepTable and indexTable

class AdpcmDecoder {
public:
    struct State {
        int32_t predictor = 0;
        int32_t stepIndex = 0;
    };

    static void decodeIMA(const uint8_t* adpcmData, uint32_t adpcmSize, int16_t* pcmOut, uint32_t& samplesDecoded, State& state) {
        samplesDecoded = 0;
        for (uint32_t i = 0; i < adpcmSize; i++) {
            uint8_t byte = adpcmData[i];
            
            // Low nibble
            decodeNibble(byte & 0x0F, pcmOut[samplesDecoded++], state);
            
            // High nibble
            decodeNibble((byte >> 4) & 0x0F, pcmOut[samplesDecoded++], state);
        }
    }

private:
    static inline void decodeNibble(uint8_t nibble, int16_t& sample, State& state) {
        static const int indexTable[8] = {
            -1, -1, -1, -1, 2, 4, 6, 8
        };
        static const int stepTable[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
            50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 
            253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 
            1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 
            3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 
            12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
        };

        int32_t step = stepTable[state.stepIndex];
        
        int32_t diff = step >> 3;
        if (nibble & 4) diff += step;
        if (nibble & 2) diff += (step >> 1);
        if (nibble & 1) diff += (step >> 2);
        
        if (nibble & 8) state.predictor -= diff;
        else state.predictor += diff;
        
        state.predictor = std::max((int32_t)-32768, std::min((int32_t)32767, state.predictor));
        
        state.stepIndex += indexTable[nibble & 7];
        state.stepIndex = std::max((int32_t)0, std::min((int32_t)88, state.stepIndex));
        
        sample = (int16_t)state.predictor;
    }

};
