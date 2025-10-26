#pragma once

#include "SimpleMath.h"
#include <vector>

// Parse svg and extract all path
std::vector<std::vector<vec2>> svgParsePath(const char* filename);