#include "config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
AppConfig default_config()
{
    AppConfig cfg;
    cfg.palette.clear = Vec3{0.f, 0.f, 0.f};
    cfg.palette.grid = Vec3{0.f, 1.f, 0.f};
    cfg.palette.outline = Vec3{0.92f, 0.95f, 0.98f};
    cfg.shape_colors = {
        Vec3{0.0f, 1.0f, 0.0f},  // I: green
        Vec3{1.0f, 0.0f, 0.0f},  // O: red
        Vec3{0.0f, 0.9f, 1.0f},  // T: cyan
        Vec3{0.0f, 0.0f, 1.0f},  // L: blue
        Vec3{1.0f, 0.75f, 0.0f}, // J: orange/yellow
        Vec3{1.0f, 0.0f, 0.8f},  // S: magenta
        Vec3{0.0f, 1.0f, 0.6f},  // Z: aqua green
        Vec3{0.9f, 0.9f, 0.9f},  // Dot 1x1x1
        Vec3{0.5f, 0.7f, 1.0f},  // Bar2 1x1x2
    };
    cfg.well_width = 6;
    cfg.well_depth = 6;
    return cfg;
}

std::string trim(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool parse_vec3(const std::string& value, Vec3& out)
{
    std::string v = trim(value);
    if (v.size() < 5 || v.front() != '[' || v.back() != ']')
    {
        return false;
    }
    v = v.substr(1, v.size() - 2);
    std::stringstream ss(v);
    std::string token;
    float comps[3] = {0.f, 0.f, 0.f};
    int count = 0;
    while (std::getline(ss, token, ',') && count < 3)
    {
        try
        {
            comps[count++] = std::stof(trim(token));
        }
        catch (...)
        {
            return false;
        }
    }
    if (count != 3)
    {
        return false;
    }
    out = Vec3{comps[0], comps[1], comps[2]};
    return true;
}

bool parse_int(const std::string& value, int& out)
{
    try
    {
        out = std::stoi(trim(value));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parse_float(const std::string& value, float& out)
{
    try
    {
        out = std::stof(trim(value));
        return true;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace

AppConfig load_config(const std::string& path)
{
    AppConfig cfg = default_config();

    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Config: cannot open " << path << ", using defaults\n";
        return cfg;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line))
    {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
        {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (section == "render.palette")
        {
            Vec3 v{};
            if (!parse_vec3(value, v))
            {
                continue;
            }
            if (key == "clear") cfg.palette.clear = v;
            else if (key == "grid") cfg.palette.grid = v;
            else if (key == "outline") cfg.palette.outline = v;
        }
        else if (section == "shapes")
        {
            static const char* names[] = {"I", "O", "T", "L", "J", "S", "Z", "Dot", "Bar2"};
            int idx = -1;
            for (int i = 0; i < 9; ++i)
            {
                if (key == names[i])
                {
                    idx = i;
                    break;
                }
            }
            if (idx >= 0)
            {
                Vec3 v{};
                if (parse_vec3(value, v))
                {
                    cfg.shape_colors[idx] = v;
                }
            }
        }
        else if (section == "well")
        {
            int v = 0;
            if (!parse_int(value, v))
            {
                continue;
            }
            if (key == "width") cfg.well_width = std::max(1, v);
            else if (key == "depth") cfg.well_depth = std::max(1, v);
            else if (key == "height") cfg.well_height = std::max(1, v);
        }
        else if (section == "gameplay")
        {
            float v = 0.f;
            if (!parse_float(value, v))
            {
                continue;
            }
            if (key == "fall_interval" && v > 0.f) cfg.fall_interval = v;
        }
    }

    // Guard against truncated shape arrays.
    if (cfg.shape_colors.size() < 9)
    {
        cfg.shape_colors.resize(9, Vec3{1.f, 1.f, 1.f});
    }
    return cfg;
}
