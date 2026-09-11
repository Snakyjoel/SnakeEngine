#pragma once
#include <cmath>
#include <string>
#include <algorithm>

#define PI 3.14159265358979323846

class Ease {
public:
    static float get(const std::string& type, float t) {
        std::string lowType = type;
        std::transform(lowType.begin(), lowType.end(), lowType.begin(), ::tolower);

        if (lowType == "linear") return t;
        
        // SINE
        if (lowType == "sinein") return 1.0f - cosf(t * 1.57079632679f);
        if (lowType == "sineout") return sinf(t * 1.57079632679f);
        if (lowType == "sineinout") return -0.5f * (cosf(PI * t) - 1.0f);

        // QUAD
        if (lowType == "quadin") return t * t;
        if (lowType == "quadout") return t * (2.0f - t);
        if (lowType == "quadinout") return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

        // CUBE
        if (lowType == "cubein") return t * t * t;
        if (lowType == "cubeout") { float t1 = t - 1.0f; return t1 * t1 * t1 + 1.0f; }
        if (lowType == "cubeinout") return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;

        // QUART
        if (lowType == "quartin") return t * t * t * t;
        if (lowType == "quartout") { float t1 = t - 1.0f; return 1.0f - t1 * t1 * t1 * t1; }
        if (lowType == "quartinout") return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - 8.0f * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) * (t - 1.0f);

        // QUINT
        if (lowType == "quintin") return t * t * t * t * t;
        if (lowType == "quintout") { float t1 = t - 1.0f; return t1 * t1 * t1 * t1 * t1 + 1.0f; }
        if (lowType == "quintinout") return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f + 16.0f * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) * (t - 1.0f);

        // EXPO
        if (lowType == "expoin") return t == 0 ? 0 : powf(2.0f, 10.0f * (t - 1.0f));
        if (lowType == "expoout") return t == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
        if (lowType == "expoinout") {
            if (t == 0) return 0;
            if (t == 1.0f) return 1.0f;
            if (t < 0.5f) return 0.5f * powf(2.0f, 20.0f * t - 10.0f);
            return -0.5f * powf(2.0f, -20.0f * t + 10.0f) + 1.0f;
        }

        // CIRC
        if (lowType == "circin") return 1.0f - sqrtf(1.0f - t * t);
        if (lowType == "circout") return sqrtf(1.0f - (t - 1.0f) * (t - 1.0f));
        if (lowType == "circinout") {
            if (t < 0.5f) return (1.0f - sqrtf(1.0f - 4.0f * t * t)) * 0.5f;
            float p = -2.0f * t + 2.0f;
            return (sqrtf(1.0f - p * p) + 1.0f) * 0.5f;
        }

        // BACK
        float c1 = 1.70158f;
        float c2 = c1 * 1.525f;
        float c3 = c1 + 1.0f;
        if (lowType == "backin") return c3 * t * t * t - c1 * t * t;
        if (lowType == "backout") {
            float tm1 = t - 1.0f;
            return 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
        }
        if (lowType == "backinout") {
            if (t < 0.5f) {
                float t2 = 2.0f * t;
                return (t2 * t2 * ((c2 + 1.0f) * t2 - c2)) * 0.5f;
            } else {
                float tm = 2.0f * t - 2.0f;
                return (tm * tm * ((c2 + 1.0f) * tm + c2) + 2.0f) * 0.5f;
            }
        }

        // BOUNCE (Simplified)
        if (lowType == "bounceout") {
            if (t < 1.0f / 2.75f) return 7.5625f * t * t;
            else if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
            else if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
            else { t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f; }
        }

        // Default to linear
        return t; 
    }
};
