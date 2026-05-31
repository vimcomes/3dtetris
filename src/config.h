#pragma once

#include <string>
#include <vector>

#include "math.h"
#include "palette.h"

struct ShapeDef
{
    std::vector<Vec3i> blocks;
    Vec3 color{};
};

struct ControlsConfig
{
    std::string move_left = "Left";
    std::string move_right = "Right";
    std::string move_forward = "Up";
    std::string move_back = "Down";
    std::string rot_x_pos = "E";
    std::string rot_x_neg = "D";
    std::string rot_z_pos = "W";
    std::string rot_z_neg = "S";
    std::string rot_y_pos = "Q";
    std::string rot_y_neg = "A";
    std::string hard_drop = "Space";
    std::string soft_drop = "V";
    std::string hold = "C";
    std::string wireframe = "F";
    std::string pause = "P";
};

struct AiConfig
{
    float weight_max_height = 5.0f;
    float weight_agg_height = 0.5f;
    float weight_holes = 50.0f;
    float weight_bumpiness = 3.0f;
};

struct AppConfig
{
    RenderPalette palette{};
    std::vector<Vec3> shape_colors;
    std::vector<ShapeDef> shapes;
    ControlsConfig controls;
    AiConfig ai;
    std::string preset = "modern";
    std::string blockout_set = "basic";
    std::string forms_path;
    int well_width = 10;
    int well_depth = 10;
    int well_height = 20;
    int start_level = 2;
    float fall_interval = 1.2f;
};

// Load configuration from TOML file. Falls back to built-in defaults on any error.
AppConfig load_config(const std::string& path);

// Built-in default config (modern shapes, blockout preset, default well).
AppConfig default_config();
