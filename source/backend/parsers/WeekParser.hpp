#pragma once
#include "WeekData.hpp"
#include <string>

class WeekParser {
public:
    static WeekData loadJson(const std::string& path);
};
