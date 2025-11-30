#pragma once

#include <string>
#include <vector>

#include "math.h"
#include "render.h"

struct AppConfig
{
    RenderPalette palette{};
    std::vector<Vec3> shape_colors;
    int well_width = 10;
    int well_depth = 10;
    int well_height = 20;
    float fall_interval = 1.2f;
};

// Load configuration from TOML file. Falls back to built-in defaults on any error.
AppConfig load_config(const std::string& path);
