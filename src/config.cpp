#include "config.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace
{
std::vector<ShapeDef> build_modern_shapes(const std::vector<Vec3>& colors)
{
    std::vector<ShapeDef> defs = {
        // I
        {{Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{2, 0, 0}}},
        // O (2x2x2 cube)
        {{
            Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{0, 0, 1}, Vec3i{1, 0, 1},
            Vec3i{0, 1, 0}, Vec3i{1, 1, 0}, Vec3i{0, 1, 1}, Vec3i{1, 1, 1},
        }},
        // T
        {{Vec3i{0, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{1, 0, 0}, Vec3i{0, 0, 1}}},
        // L
        {{Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{-1, 0, 1}}},
        // J
        {{Vec3i{0, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{1, 0, 0}, Vec3i{1, 0, 1}}},
        // S
        {{Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{0, 0, 1}, Vec3i{-1, 0, 1}}},
        // Z
        {{Vec3i{0, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{0, 0, 1}, Vec3i{1, 0, 1}}},
        // Dot 1x1x1
        {{Vec3i{0, 0, 0}}},
        // Bar2 1x1x2 (vertical)
        {{Vec3i{0, 0, 0}, Vec3i{0, 1, 0}}},
    };
    if (colors.size() == defs.size())
    {
        for (size_t i = 0; i < defs.size(); ++i)
        {
            defs[i].color = colors[i];
        }
    }
    else
    {
        Vec3 fallback{0.8f, 0.8f, 0.8f};
        for (auto& d : defs) d.color = fallback;
    }
    return defs;
}

struct ParsedForm
{
    int x = 0, y = 0, z = 0;
    std::vector<int> data;
    Vec3i center{0, 0, 0};
};

std::vector<ParsedForm> read_blockout_forms(const std::filesystem::path& path)
{
    std::vector<ParsedForm> forms;
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Config: cannot open forms file " << path << "\n";
        return forms;
    }
    auto read_int = [&](int& out) -> bool
    {
        std::string tok;
        while (f >> tok)
        {
            if (!tok.empty() && tok[0] == '#')
            {
                f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }
            try
            {
                out = std::stoi(tok);
                return true;
            }
            catch (...)
            {
                continue;
            }
        }
        return false;
    };

    int count = 0;
    if (!read_int(count))
    {
        return forms;
    }
    forms.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        ParsedForm pf;
        if (!read_int(pf.x) || !read_int(pf.y) || !read_int(pf.z))
        {
            break;
        }
        pf.data.resize(pf.x * pf.y * pf.z);
        for (int z = 0; z < pf.z; ++z)
        {
            for (int y = 0; y < pf.y; ++y)
            {
                for (int x = 0; x < pf.x; ++x)
                {
                    int v = 0;
                    if (!read_int(v))
                    {
                        v = 0;
                    }
                    pf.data[z * pf.y * pf.x + y * pf.x + x] = v;
                }
            }
        }
        read_int(pf.center.x);
        read_int(pf.center.y);
        read_int(pf.center.z);
        forms.push_back(std::move(pf));
    }
    return forms;
}

std::vector<ShapeDef> convert_blockout_forms(const std::vector<ParsedForm>& parsed, const std::vector<Vec3>& palette, const std::string& set)
{
    std::vector<ShapeDef> defs;
    if (parsed.empty()) return defs;
    defs.reserve(parsed.size());
    for (size_t i = 0; i < parsed.size(); ++i)
    {
        const auto& pf = parsed[i];
        int cube_count = 0;
        for (int v : pf.data) if (v != 0) ++cube_count;

        bool include = true;
        if (set == "basic")
        {
            // Easiest set: only flat pieces and at most 4 cubes.
            include = (pf.z == 1) && (cube_count <= 4);
        }
        else if (set == "advanced")
        {
            // Everything except single/double cubes.
            include = cube_count >= 3;
        }
        else
        {
            include = cube_count >= 1;
        }
        if (!include) continue;

        ShapeDef def;
        for (int z = 0; z < pf.z; ++z)
        {
            for (int y = 0; y < pf.y; ++y)
            {
                for (int x = 0; x < pf.x; ++x)
                {
                    int v = pf.data[z * pf.y * pf.x + y * pf.x + x];
                    if (v != 0)
                    {
                        def.blocks.push_back(Vec3i{x - pf.center.x, y - pf.center.y, z - pf.center.z});
                    }
                }
            }
        }
        def.color = palette.empty() ? Vec3{0.0f, 0.0f, 1.0f} : palette[defs.size() % palette.size()];
        defs.push_back(std::move(def));
    }
    return defs;
}

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
    cfg.preset = "blockout";
    cfg.blockout_set = "basic";
    cfg.shapes = build_modern_shapes(cfg.shape_colors);
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

    std::filesystem::path config_path = path;
    std::filesystem::path config_dir = config_path.parent_path();
    bool fall_interval_explicit = false;

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
            if (key == "fall_interval" && v > 0.f)
            {
                cfg.fall_interval = v;
                fall_interval_explicit = true;
            }
        }
        else if (section == "preset")
        {
            if (key == "name")
            {
                cfg.preset = value;
                cfg.preset = trim(cfg.preset);
                if (!cfg.preset.empty() && cfg.preset.front() == '"' && cfg.preset.back() == '"')
                {
                    cfg.preset = cfg.preset.substr(1, cfg.preset.size() - 2);
                }
            }
            else if (key == "forms_path")
            {
                cfg.forms_path = value;
                if (!cfg.forms_path.empty() && cfg.forms_path.front() == '"' && cfg.forms_path.back() == '"')
                {
                    cfg.forms_path = cfg.forms_path.substr(1, cfg.forms_path.size() - 2);
                }
            }
            else if (key == "blockout_set")
            {
                cfg.blockout_set = value;
                if (!cfg.blockout_set.empty() && cfg.blockout_set.front() == '"' && cfg.blockout_set.back() == '"')
                {
                    cfg.blockout_set = cfg.blockout_set.substr(1, cfg.blockout_set.size() - 2);
                }
            }
        }
    }

    // Guard against truncated shape arrays.
    if (cfg.shape_colors.size() < 9)
    {
        cfg.shape_colors.resize(9, Vec3{1.f, 1.f, 1.f});
    }

    // Build shape defs after colors are resolved.
    cfg.shapes = build_modern_shapes(cfg.shape_colors);

    // Apply preset overrides.
    if (cfg.preset == "blockout")
    {
        cfg.well_width = 3;
        cfg.well_depth = 3;
        cfg.well_height = 18;
        cfg.palette.grid = Vec3{0.8f, 0.8f, 0.0f};
        cfg.palette.outline = Vec3{0.92f, 0.95f, 0.98f};
        cfg.palette.clear = Vec3{0.f, 0.f, 0.f};
        if (!fall_interval_explicit)
        {
            cfg.fall_interval = 0.35f; // closer to their faster baseline
        }

        // Shape colors: reuse current palette to colorize imported forms.
        std::vector<Vec3> blockout_colors = {
            Vec3{0.0f, 0.0f, 1.0f},
            Vec3{1.0f, 0.0f, 0.0f},
            Vec3{0.0f, 1.0f, 0.0f},
            Vec3{1.0f, 1.0f, 0.0f},
            Vec3{1.0f, 0.0f, 1.0f},
            Vec3{0.0f, 1.0f, 1.0f},
        };

        std::filesystem::path forms_path = cfg.forms_path.empty() ? std::filesystem::path("data/forms_blockout.dat") : std::filesystem::path(cfg.forms_path);
        if (!forms_path.is_absolute() && !config_dir.empty())
        {
            std::filesystem::path candidate = config_dir / forms_path;
            if (std::filesystem::exists(candidate))
            {
                forms_path = candidate;
            }
        }
        auto parsed = read_blockout_forms(forms_path);
        auto defs = convert_blockout_forms(parsed, blockout_colors, cfg.blockout_set);
        if (!defs.empty())
        {
            cfg.shapes = std::move(defs);
        }
        else
        {
            std::cerr << "Config: blockout preset requested but failed to load forms; using modern shapes\n";
        }
    }

    return cfg;
}
