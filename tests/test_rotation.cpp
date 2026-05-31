#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>
#include <cstring>

#include "math.h"
#include "game.h"
#include "rotation.h"

static bool is_identity(const int m[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (m[i][j] != (i == j ? 1 : 0))
                return false;
    return true;
}

static int mat_det(const int m[3][3])
{
    return
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

static bool is_orthogonal(const int m[3][3])
{
    int mt[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            mt[i][j] = m[j][i];
    int prod[3][3];
    mul_rot(m, mt, prod);
    return is_identity(prod);
}

static std::string mat_key(const int m[3][3])
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                  m[0][0], m[0][1], m[0][2],
                  m[1][0], m[1][1], m[1][2],
                  m[2][0], m[2][1], m[2][2]);
    return buf;
}

TEST_CASE("Rotation matrices generate exactly 24 unique orientations", "[core][rotation]")
{
    const int (*rots[6])[3] = {ROT_X_POS, ROT_X_NEG, ROT_Y_POS, ROT_Y_NEG, ROT_Z_POS, ROT_Z_NEG};

    std::set<std::string> found;
    int current[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    found.insert(mat_key(current));

    // BFS / closure: apply each rotation to every found orientation.
    std::set<std::string> prev;
    while (prev != found)
    {
        prev = found;
        std::set<std::string> next = found;
        for (const auto& key : prev)
        {
            int m[3][3];
            std::sscanf(key.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                        &m[0][0], &m[0][1], &m[0][2],
                        &m[1][0], &m[1][1], &m[1][2],
                        &m[2][0], &m[2][1], &m[2][2]);
            for (const auto& r : rots)
            {
                int prod[3][3];
                mul_rot(m, r, prod);
                next.insert(mat_key(prod));
            }
        }
        found = next;
    }

    REQUIRE(found.size() == 24);
}

TEST_CASE("Every orientation matrix is orthogonal with det = +1", "[core][rotation]")
{
    const int (*rots[6])[3] = {ROT_X_POS, ROT_X_NEG, ROT_Y_POS, ROT_Y_NEG, ROT_Z_POS, ROT_Z_NEG};

    std::set<std::string> found;
    int current[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    found.insert(mat_key(current));

    std::set<std::string> prev;
    while (prev != found)
    {
        prev = found;
        std::set<std::string> next = found;
        for (const auto& key : prev)
        {
            int m[3][3];
            std::sscanf(key.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                        &m[0][0], &m[0][1], &m[0][2],
                        &m[1][0], &m[1][1], &m[1][2],
                        &m[2][0], &m[2][1], &m[2][2]);
            for (const auto& r : rots)
            {
                int prod[3][3];
                mul_rot(m, r, prod);
                next.insert(mat_key(prod));
            }
        }
        found = next;
    }

    for (const auto& key : found)
    {
        int m[3][3];
        std::sscanf(key.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                    &m[0][0], &m[0][1], &m[0][2],
                    &m[1][0], &m[1][1], &m[1][2],
                    &m[2][0], &m[2][1], &m[2][2]);
        INFO("Matrix: " << mat_key(m));
        CHECK(is_orthogonal(m));
        CHECK(mat_det(m) == 1);
    }
}

TEST_CASE("apply_rot applies matrix to Vec3i", "[core][rotation]")
{
    const int id[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vec3i v{1, 2, 3};
    Vec3i r = apply_rot(id, v);
    REQUIRE(r.x == 1);
    REQUIRE(r.y == 2);
    REQUIRE(r.z == 3);

    Vec3i r2 = apply_rot(ROT_X_POS, v);
    REQUIRE(r2.x == 1);
    REQUIRE(r2.y == -3);
    REQUIRE(r2.z == 2);
}
