#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

class AdpcmEncoder {
public:
#pragma pack(push, 1)
    struct Header {
        char magic[4]; // "SADP"
        uint32_t sampleRate;
        uint32_t numSamples;
        uint8_t channels; // Always 1 for now
        uint8_t reserved[7];
    };
#pragma pack(pop)

    struct State {
        int32_t predictor = 0;
        int32_t stepIndex = 0;
        uint8_t pendingNibble = 0;
        bool hasPending = false;
    };

    static std::vector<uint8_t> encodeIMA(const int16_t* pcmData, uint32_t numSamples, State& state) {
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

        std::vector<uint8_t> adpcm;
        adpcm.reserve(numSamples / 2 + 1);

        for (uint32_t i = 0; i < numSamples; i++) {
            int32_t sample = pcmData[i];
            int32_t diff = sample - state.predictor;
            int32_t step = stepTable[state.stepIndex];

            uint8_t nibble = 0;
            if (diff < 0) {
                nibble = 8;
                diff = -diff;
            }

            int32_t mask = 4;
            int32_t tempDiff = step;
            for (int bit = 0; bit < 3; bit++) {
                if (diff >= tempDiff) {
                    nibble |= mask;
                    diff -= tempDiff;
                }
                tempDiff >>= 1;
                mask >>= 1;
            }

            // Update predictor
            int32_t predDiff = step >> 3;
            if (nibble & 4) predDiff += step;
            if (nibble & 2) predDiff += (step >> 1);
            if (nibble & 1) predDiff += (step >> 2);

            if (nibble & 8) state.predictor -= predDiff;
            else state.predictor += predDiff;

            state.predictor = std::max((int32_t)-32768, std::min((int32_t)32767, state.predictor));

            // Update step index
            state.stepIndex += indexTable[nibble & 7];
            state.stepIndex = std::max((int32_t)0, std::min((int32_t)88, state.stepIndex));

            // Pack nibbles
            if (!state.hasPending) {
                state.pendingNibble = nibble;
                state.hasPending = true;
            } else {
                adpcm.push_back(state.pendingNibble | (nibble << 4));
                state.hasPending = false;
            }
        }
        
        return adpcm;
    }

    // Call this at the very end of a file to flush the last nibble if needed
    static void flush(std::vector<uint8_t>& adpcm, State& state) {
        if (state.hasPending) {
            adpcm.push_back(state.pendingNibble);
            state.hasPending = false;
        }
    }
};
