#pragma once

#include <array>
#include <vector>

#include "math.h"

inline std::vector<float> build_cube_vertices()
{
    // 6 faces * 2 triangles * 3 vertices. Each vertex: position xyz, color rgb (white; tinted in shader).
    return {
        // Front (z+)
        -0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f, 0.5f, 1.f, 1.f, 1.f,
        -0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f, 0.5f, 1.f, 1.f, 1.f,
        -0.5f,  0.5f, 0.5f, 1.f, 1.f, 1.f,
        // Back (z-)
        -0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
        -0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
        -0.5f,  0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f, -0.5f, 1.f, 1.f, 1.f,
        // Left (x-)
        -0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
        -0.5f, -0.5f,  0.5f, 1.f, 1.f, 1.f,
        -0.5f,  0.5f,  0.5f, 1.f, 1.f, 1.f,
        -0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
        -0.5f,  0.5f,  0.5f, 1.f, 1.f, 1.f,
        -0.5f,  0.5f, -0.5f, 1.f, 1.f, 1.f,
        // Right (x+)
         0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f,  0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f,  0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f,  0.5f,  0.5f, 1.f, 1.f, 1.f,
        // Top (y+)
        -0.5f, 0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f, 0.5f,  0.5f, 1.f, 1.f, 1.f,
         0.5f, 0.5f, -0.5f, 1.f, 1.f, 1.f,
        -0.5f, 0.5f, -0.5f, 1.f, 1.f, 1.f,
        -0.5f, 0.5f,  0.5f, 1.f, 1.f, 1.f,
         0.5f, 0.5f,  0.5f, 1.f, 1.f, 1.f,
        // Bottom (y-)
        -0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f,  0.5f, 1.f, 1.f, 1.f,
        -0.5f, -0.5f, -0.5f, 1.f, 1.f, 1.f,
         0.5f, -0.5f,  0.5f, 1.f, 1.f, 1.f,
        -0.5f, -0.5f,  0.5f, 1.f, 1.f, 1.f,
    };
}

inline std::vector<float> build_floor_grid_lines(int width, int depth, float cell)
{
    std::vector<float> data;
    float color[3] = {0.0f, 1.0f, 0.0f}; // neon green grid like Blockout
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
    float color[3] = {0.0f, 1.0f, 0.0f}; // neon green walls like Blockout
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

inline std::vector<float> build_bottom_plane(int width, int depth, float cell)
{
    float min_x = -0.5f * width * cell;
    float min_z = -0.5f * depth * cell;
    float max_x = 0.5f * width * cell;
    float max_z = 0.5f * depth * cell;
    // Two triangles; keep black so the grid lines pop.
    return {
        min_x, 0.f, min_z, 0.f, 0.f, 0.f,
        max_x, 0.f, min_z, 0.f, 0.f, 0.f,
        max_x, 0.f, max_z, 0.f, 0.f, 0.f,

        min_x, 0.f, min_z, 0.f, 0.f, 0.f,
        max_x, 0.f, max_z, 0.f, 0.f, 0.f,
        min_x, 0.f, max_z, 0.f, 0.f, 0.f,
    };
}
