#pragma once

#include <array>
#include <vector>

#include "math.h"

inline std::vector<float> build_cube_vertices()
{
    // 6 faces * 2 triangles * 3 vertices. Each vertex: position xyz, normal xyz.
    // Scale 0.88 (half = 0.44) leaves a visible gap between adjacent locked cells.
    constexpr float h = 0.44f;
    return {
        // Front (z+), normal (0,0,1)
        -h,-h, h,  0,0,1,   h,-h, h,  0,0,1,   h, h, h,  0,0,1,
        -h,-h, h,  0,0,1,   h, h, h,  0,0,1,  -h, h, h,  0,0,1,
        // Back (z-), normal (0,0,-1)
        -h,-h,-h,  0,0,-1,  h, h,-h,  0,0,-1,  h,-h,-h,  0,0,-1,
        -h,-h,-h,  0,0,-1, -h, h,-h,  0,0,-1,  h, h,-h,  0,0,-1,
        // Left (x-), normal (-1,0,0)
        -h,-h,-h, -1,0,0,  -h,-h, h, -1,0,0,  -h, h, h, -1,0,0,
        -h,-h,-h, -1,0,0,  -h, h, h, -1,0,0,  -h, h,-h, -1,0,0,
        // Right (x+), normal (1,0,0)
         h,-h,-h,  1,0,0,   h, h, h,  1,0,0,   h,-h, h,  1,0,0,
         h,-h,-h,  1,0,0,   h, h,-h,  1,0,0,   h, h, h,  1,0,0,
        // Top (y+), normal (0,1,0)
        -h, h,-h,  0,1,0,   h, h, h,  0,1,0,   h, h,-h,  0,1,0,
        -h, h,-h,  0,1,0,  -h, h, h,  0,1,0,   h, h, h,  0,1,0,
        // Bottom (y-), normal (0,-1,0)
        -h,-h,-h,  0,-1,0,  h,-h,-h,  0,-1,0,  h,-h, h,  0,-1,0,
        -h,-h,-h,  0,-1,0,  h,-h, h,  0,-1,0, -h,-h, h,  0,-1,0,
    };
}

// 12 box edges of a 0.90-scale cube, white color vertices (for flat line shader).
inline std::vector<float> build_cube_edge_lines()
{
    constexpr float h = 0.45f;
    std::vector<float> d;
    d.reserve(12 * 2 * 6);
    auto e = [&](float x0,float y0,float z0, float x1,float y1,float z1){
        d.insert(d.end(), {x0,y0,z0, 1,1,1, x1,y1,z1, 1,1,1});
    };
    // bottom ring
    e(-h,-h,-h,  h,-h,-h); e( h,-h,-h,  h,-h, h);
    e( h,-h, h, -h,-h, h); e(-h,-h, h, -h,-h,-h);
    // top ring
    e(-h, h,-h,  h, h,-h); e( h, h,-h,  h, h, h);
    e( h, h, h, -h, h, h); e(-h, h, h, -h, h,-h);
    // verticals
    e(-h,-h,-h, -h, h,-h); e( h,-h,-h,  h, h,-h);
    e( h,-h, h,  h, h, h); e(-h,-h, h, -h, h, h);
    return d;
}

inline std::vector<float> build_floor_grid_lines(int width, int depth, float cell)
{
    std::vector<float> data;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;

    for (int i = 0; i <= width; ++i)
    {
        float x = min_x + i * cell;
        data.insert(data.end(), {x, 0.f, min_z, color[0], color[1], color[2],
                                 x, 0.f, max_z, color[0], color[1], color[2]});
    }
    for (int k = 0; k <= depth; ++k)
    {
        float z = min_z + k * cell;
        data.insert(data.end(), {min_x, 0.f, z, color[0], color[1], color[2],
                                 max_x, 0.f, z, color[0], color[1], color[2]});
    }
    return data;
}

inline std::vector<float> build_well_outline_lines(int width, int depth, int height, float cell)
{
    std::vector<float> data;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;
    float h = height * cell;

    // Corners verticals.
    std::array<Vec3, 4> corners = {{
        {min_x, 0.f, min_z},
        {max_x, 0.f, min_z},
        {max_x, 0.f, max_z},
        {min_x, 0.f, max_z},
    }};
    for (const auto& c : corners)
    {
        data.insert(data.end(), {c.x, 0.f, c.z, color[0], color[1], color[2],
                                 c.x, h, c.z, color[0], color[1], color[2]});
    }

    // Horizontal rings every few levels to hint depth.
    const int ring_step = 2;
    for (int level = ring_step; level <= height; level += ring_step)
    {
        float y = level * cell;
        data.insert(data.end(), {min_x, y, min_z, color[0], color[1], color[2],
                                 max_x, y, min_z, color[0], color[1], color[2]});
        data.insert(data.end(), {max_x, y, min_z, color[0], color[1], color[2],
                                 max_x, y, max_z, color[0], color[1], color[2]});
        data.insert(data.end(), {max_x, y, max_z, color[0], color[1], color[2],
                                 min_x, y, max_z, color[0], color[1], color[2]});
        data.insert(data.end(), {min_x, y, max_z, color[0], color[1], color[2],
                                 min_x, y, min_z, color[0], color[1], color[2]});
    }

    return data;
}

inline std::vector<float> build_well_outline_lines_culled(int width, int depth, int height, float cell)
{
    // Same as build_well_outline_lines, but drop the two faces closest to the isometric camera (+X and +Z).
    std::vector<float> data;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;
    float h = height * cell;

    // Keep only corners on far faces (x = min_x or z = min_z).
    std::array<Vec3, 4> corners = {{
        {min_x, 0.f, min_z},
        {max_x, 0.f, min_z},
        {min_x, 0.f, max_z},
        {max_x, 0.f, max_z},
    }};
    for (const auto& c : corners)
    {
        if (c.x == max_x && c.z == max_z) continue; // drop near-most corner
        if (c.x == max_x && c.z == min_z) continue; // drop +X edge
        if (c.x == min_x && c.z == max_z) continue; // drop +Z edge
        data.insert(data.end(), {c.x, 0.f, c.z, color[0], color[1], color[2],
                                 c.x, h, c.z, color[0], color[1], color[2]});
    }

    // Horizontal rings along far edges only (x = min_x or z = min_z).
    const int ring_step = 2;
    for (int level = ring_step; level <= height; level += ring_step)
    {
        float y = level * cell;
        // Back edge (-Z)
        data.insert(data.end(), {min_x, y, min_z, color[0], color[1], color[2],
                                 max_x, y, min_z, color[0], color[1], color[2]});
        // Left edge (-X)
        data.insert(data.end(), {min_x, y, min_z, color[0], color[1], color[2],
                                 min_x, y, max_z, color[0], color[1], color[2]});
    }

    return data;
}

inline std::vector<float> build_well_wall_grid_lines(int width, int depth, int height, float cell)
{
    std::vector<float> data;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;
    float h = height * cell;

    // Vertical lines on front/back faces for each x.
    for (int i = 0; i <= width; ++i)
    {
        float x = min_x + i * cell;
        data.insert(data.end(), {x, 0.f, min_z, color[0], color[1], color[2],
                                 x, h,   min_z, color[0], color[1], color[2]});
        data.insert(data.end(), {x, 0.f, max_z, color[0], color[1], color[2],
                                 x, h,   max_z, color[0], color[1], color[2]});
    }

    // Vertical lines on left/right faces for each z.
    for (int k = 0; k <= depth; ++k)
    {
        float z = min_z + k * cell;
        data.insert(data.end(), {min_x, 0.f, z, color[0], color[1], color[2],
                                 min_x, h,   z, color[0], color[1], color[2]});
        data.insert(data.end(), {max_x, 0.f, z, color[0], color[1], color[2],
                                 max_x, h,   z, color[0], color[1], color[2]});
    }

    return data;
}

inline std::vector<float> build_well_wall_grid_lines_culled(int width, int depth, int height, float cell)
{
    std::vector<float> data;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;
    float h = height * cell;

    // Only far faces: x = min_x and z = min_z.
    for (int i = 0; i <= width; ++i)
    {
        float x = min_x + i * cell;
        data.insert(data.end(), {x, 0.f, min_z, color[0], color[1], color[2],
                                 x, h,   min_z, color[0], color[1], color[2]});
    }
    for (int k = 0; k <= depth; ++k)
    {
        float z = min_z + k * cell;
        data.insert(data.end(), {min_x, 0.f, z, color[0], color[1], color[2],
                                 min_x, h,   z, color[0], color[1], color[2]});
    }

    return data;
}

inline std::vector<float> build_bottom_plane(int width, int depth, float cell)
{
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;
    // Two triangles, normal pointing up (block shader reads location 1 as normal).
    return {
        min_x, 0.f, min_z, 0.f, 1.f, 0.f,
        max_x, 0.f, min_z, 0.f, 1.f, 0.f,
        max_x, 0.f, max_z, 0.f, 1.f, 0.f,

        min_x, 0.f, min_z, 0.f, 1.f, 0.f,
        max_x, 0.f, max_z, 0.f, 1.f, 0.f,
        min_x, 0.f, max_z, 0.f, 1.f, 0.f,
    };
}
