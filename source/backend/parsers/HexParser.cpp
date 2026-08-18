#include "HexParser.hpp"
#include <algorithm>
#include <cctype>

u32 HexParser::parseStringToC2D(const std::string& hexStr, u32 defaultColor) {
    std::string s = hexStr;
    
    // Trim spaces
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    size_t lastNonSpace = s.find_last_not_of(" \t\r\n");
    if (lastNonSpace != std::string::npos) {
        s.erase(lastNonSpace + 1);
    } else {
        s.clear();
    }
    
    // Convert to lowercase to check color names and handle hex chars consistently
    std::string lowerS = s;
    std::transform(lowerS.begin(), lowerS.end(), lowerS.begin(), ::tolower);
    
    // Check color names first
    if (lowerS == "black")       return C2D_Color32(0, 0, 0, 255);
    if (lowerS == "white")       return C2D_Color32(255, 255, 255, 255);
    if (lowerS == "red")         return C2D_Color32(255, 0, 0, 255);
    if (lowerS == "green")       return C2D_Color32(0, 255, 0, 255);
    if (lowerS == "blue")        return C2D_Color32(0, 0, 255, 255);
    if (lowerS == "transparent") return C2D_Color32(0, 0, 0, 0);
    if (lowerS == "gray" || lowerS == "grey") return C2D_Color32(128, 128, 128, 255);
    if (lowerS == "yellow")      return C2D_Color32(255, 255, 0, 255);
    if (lowerS == "orange")      return C2D_Color32(255, 165, 0, 255);
    if (lowerS == "purple")      return C2D_Color32(128, 0, 128, 255);
    if (lowerS == "pink")        return C2D_Color32(255, 192, 203, 255);
    if (lowerS == "cyan")        return C2D_Color32(0, 255, 255, 255);
    if (lowerS == "magenta")     return C2D_Color32(255, 0, 255, 255);
    if (lowerS == "brown")       return C2D_Color32(165, 42, 42, 255);
    if (lowerS == "silver")      return C2D_Color32(192, 192, 192, 255);
    if (lowerS == "gold")        return C2D_Color32(255, 215, 0, 255);
    if (lowerS == "lime")        return C2D_Color32(0, 255, 0, 255);
    if (lowerS == "maroon")      return C2D_Color32(128, 0, 0, 255);
    if (lowerS == "navy")        return C2D_Color32(0, 0, 128, 255);
    if (lowerS == "olive")       return C2D_Color32(128, 128, 0, 255);
    if (lowerS == "teal")        return C2D_Color32(0, 128, 128, 255);
    if (lowerS == "violet")      return C2D_Color32(238, 130, 238, 255);
    if (lowerS == "indigo")      return C2D_Color32(75, 0, 130, 255);
    if (lowerS == "turquoise")   return C2D_Color32(64, 224, 208, 255);
    if (lowerS == "lavender")    return C2D_Color32(230, 230, 250, 255);
    if (lowerS == "beige")       return C2D_Color32(245, 245, 220, 255);
    if (lowerS == "salmon")      return C2D_Color32(250, 128, 114, 255);
    if (lowerS == "crimson")     return C2D_Color32(220, 20, 60, 255);
    
    // Remove prefix
    if (lowerS.rfind("0x", 0) == 0) {
        lowerS = lowerS.substr(2);
    } else if (lowerS.rfind("#", 0) == 0) {
        lowerS = lowerS.substr(1);
    }
    
    if (lowerS.empty()) return defaultColor;
    
    // Check if it consists of valid hex digits
    for (char c : lowerS) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return defaultColor;
        }
    }
    
    u8 r = 255, g = 255, b = 255, a = 255;
    size_t len = lowerS.length();
    
    if (len == 3) {
        // RGB shorthand -> duplicate each character
        char rCh = lowerS[0];
        char gCh = lowerS[1];
        char bCh = lowerS[2];
        std::string rStr = {rCh, rCh};
        std::string gStr = {gCh, gCh};
        std::string bStr = {bCh, bCh};
        r = (u8)std::strtoul(rStr.c_str(), nullptr, 16);
        g = (u8)std::strtoul(gStr.c_str(), nullptr, 16);
        b = (u8)std::strtoul(bStr.c_str(), nullptr, 16);
        a = 255;
    }
    else if (len == 4) {
        // ARGB shorthand -> duplicate each character
        char aCh = lowerS[0];
        char rCh = lowerS[1];
        char gCh = lowerS[2];
        char bCh = lowerS[3];
        std::string aStr = {aCh, aCh};
        std::string rStr = {rCh, rCh};
        std::string gStr = {gCh, gCh};
        std::string bStr = {bCh, bCh};
        a = (u8)std::strtoul(aStr.c_str(), nullptr, 16);
        r = (u8)std::strtoul(rStr.c_str(), nullptr, 16);
        g = (u8)std::strtoul(gStr.c_str(), nullptr, 16);
        b = (u8)std::strtoul(bStr.c_str(), nullptr, 16);
    }
    else if (len == 6) {
        // RRGGBB
        r = (u8)std::strtoul(lowerS.substr(0, 2).c_str(), nullptr, 16);
        g = (u8)std::strtoul(lowerS.substr(2, 2).c_str(), nullptr, 16);
        b = (u8)std::strtoul(lowerS.substr(4, 2).c_str(), nullptr, 16);
        a = 255;
    }
    else if (len == 8) {
        // AARRGGBB
        a = (u8)std::strtoul(lowerS.substr(0, 2).c_str(), nullptr, 16);
        r = (u8)std::strtoul(lowerS.substr(2, 2).c_str(), nullptr, 16);
        g = (u8)std::strtoul(lowerS.substr(4, 2).c_str(), nullptr, 16);
        b = (u8)std::strtoul(lowerS.substr(6, 2).c_str(), nullptr, 16);
    }
    else {
        return defaultColor;
    }
    
    return C2D_Color32(r, g, b, a);
}
