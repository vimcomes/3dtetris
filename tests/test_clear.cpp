#include <catch2/catch_test_macros.hpp>

#include "game.h"

static void fill_plane(Game& game, int y)
{
    // Use the debug method to fill a plane.
    // Since debug_fill_plane is behind #ifdef DEBUG_TOOLS,
    // we construct pieces manually instead.
    Piece p;
    p.shape = 0;
    p.pos = Vec3i{0, y, 0};
    p.pos_y = static_cast<float>(y);
    p.color = Vec3{1, 1, 1};
    p.blocks.clear();
    for (int x = 0; x < 6; ++x)
        for (int z = 0; z < 6; ++z)
            p.blocks.push_back(Vec3i{x, 0, z});
    const auto& well = game.well();
    for (const auto& b : p.blocks)
    {
        Vec3i wp = b + p.pos;
        if (well.in_bounds(wp) && well.is_free(wp))
            const_cast<Well&>(well).set_cell(wp, true, Vec3{1, 1, 1});
    }
}

TEST_CASE("clear_full_planes removes a full plane", "[core][clear]")
{
    Game game(6, 6, 20);
    fill_plane(game, 0);
    int cleared = game.clear_full_planes();
    REQUIRE(cleared == 1);

    // After clearing, the cell should be free again.
    CHECK(game.well().is_free(Vec3i{0, 0, 0}));
}

TEST_CASE("clear_full_planes does not remove a partial plane", "[core][clear]")
{
    Game game(6, 6, 20);

    // Fill only half of plane y=0.
    Piece p;
    p.shape = 0;
    p.pos = Vec3i{0, 0, 0};
    p.pos_y = 0.0f;
    p.color = Vec3{1, 1, 1};
    p.blocks.clear();
    for (int x = 0; x < 3; ++x)
        for (int z = 0; z < 3; ++z)
            p.blocks.push_back(Vec3i{x, 0, z});
    const auto& well = game.well();
    for (const auto& b : p.blocks)
    {
        Vec3i wp = b + p.pos;
        if (well.in_bounds(wp) && well.is_free(wp))
            const_cast<Well&>(well).set_cell(wp, true, Vec3{1, 1, 1});
    }

    int cleared = game.clear_full_planes();
    REQUIRE(cleared == 0);
}

TEST_CASE("clear_full_planes_range only clears within range", "[core][clear]")
{
    Game game(6, 6, 20);
    fill_plane(game, 0);
    fill_plane(game, 2);
    int cleared = game.clear_full_planes_range(1, 5);
    // Only y=2 is in range [1,5].
    REQUIRE(cleared == 1);
    CHECK(game.well().is_free(Vec3i{0, 2, 0}));
    // y=0 should still be filled (outside range).
    CHECK_FALSE(game.well().is_free(Vec3i{0, 0, 0}));
}

TEST_CASE("Clearing a plane causes blocks above to settle", "[core][clear]")
{
    Game game(6, 6, 20);
    fill_plane(game, 0);

    // Place a block at y=1.
    Piece p;
    p.shape = 0;
    p.pos = Vec3i{0, 1, 0};
    p.pos_y = 1.0f;
    p.color = Vec3{1, 1, 1};
    p.blocks = {Vec3i{0, 0, 0}};
    const auto& well = game.well();
    for (const auto& b : p.blocks)
    {
        Vec3i wp = b + p.pos;
        if (well.in_bounds(wp) && well.is_free(wp))
            const_cast<Well&>(well).set_cell(wp, true, Vec3{1, 1, 1});
    }

    int cleared = game.clear_full_planes();
    REQUIRE(cleared == 1);

    // The block at y=1 should have settled to y=0 (layer shifted down).
    CHECK(game.well().is_free(Vec3i{0, 1, 0}));
    CHECK_FALSE(game.well().is_free(Vec3i{0, 0, 0}));
}
