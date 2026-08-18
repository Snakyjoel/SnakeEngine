#pragma once
#include <string>
#include <citro2d.h>

class HexParser {
public:
    /**
     * Parses a hex color string or color name and returns a Citro2D compatible u32 color.
     * Supports formats:
     * - "#RRGGBBAA", "#RRGGBB", "#RGBA", "#RGB"
     * - "0xRRGGBBAA", "0xRRGGBB", "0xRGBA", "0xRGB"
     * - "RRGGBBAA", "RRGGBB", "RGBA", "RGB"
     * And color names: "white", "black", "red", "green", "blue", "transparent".
     *
     * @param hexStr The input color string.
     * @param defaultColor Fallback color if parsing fails (defaults to white).
     * @return Citro2D u32 color value.
     */
    static u32 parseStringToC2D(const std::string& hexStr, u32 defaultColor = 0xFFFFFFFF);
};
