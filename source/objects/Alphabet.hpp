#pragma once
#include <string>
#include <citro2d.h>

class Alphabet {
public:
    static void draw(const std::string& text, float x, float y, float scale = 1.0f, float alpha = 1.0f, bool centered = false, u32 color = 0xFFFFFFFF, float depth = 0.95f);
    static float getTextWidth(const std::string& text, float scale = 1.0f);
};
