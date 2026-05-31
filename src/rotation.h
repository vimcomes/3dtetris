#pragma once

#include <cstring>

#include "math.h"

// 90-degree rotation matrices (discrete SO(3) subgroup, 24 orientations).
// POS = positive (counter-clockwise), NEG = negative (clockwise).

static const int ROT_X_POS[3][3] = {
    {1, 0, 0},
    {0, 0, -1},
    {0, 1, 0},
};
static const int ROT_X_NEG[3][3] = {
    {1, 0, 0},
    {0, 0, 1},
    {0, -1, 0},
};
static const int ROT_Y_POS[3][3] = {
    {0, 0, 1},
    {0, 1, 0},
    {-1, 0, 0},
};
static const int ROT_Y_NEG[3][3] = {
    {0, 0, -1},
    {0, 1, 0},
    {1, 0, 0},
};
static const int ROT_Z_POS[3][3] = {
    {0, -1, 0},
    {1, 0, 0},
    {0, 0, 1},
};
static const int ROT_Z_NEG[3][3] = {
    {0, 1, 0},
    {-1, 0, 0},
    {0, 0, 1},
};

inline void mul_rot(const int A[3][3], const int B[3][3], int out[3][3])
{
    int tmp[3][3]{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                tmp[i][j] += A[i][k] * B[k][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
        {
            int v = tmp[i][j];
            out[i][j] = (v > 0) ? 1 : (v < 0) ? -1 : 0;
        }
}

inline Vec3i apply_rot(const int R[3][3], const Vec3i& v)
{
    return Vec3i{
        R[0][0] * v.x + R[0][1] * v.y + R[0][2] * v.z,
        R[1][0] * v.x + R[1][1] * v.y + R[1][2] * v.z,
        R[2][0] * v.x + R[2][1] * v.y + R[2][2] * v.z,
    };
}

inline void apply_rot_step(int mat[3][3], const int rot[3][3])
{
    int tmp[3][3];
    mul_rot(mat, rot, tmp);
    std::memcpy(mat, tmp, sizeof(tmp));
}
