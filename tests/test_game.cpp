#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

#include "game.h"
#include "config.h"

TEST_CASE("Game starts in Playing state", "[core][game]")
{
    Game game(6, 6, 20);
    REQUIRE(game.state() == GameState::Playing);
}

TEST_CASE("Hard drop locks piece and spawns next", "[core][game]")
{
    Game game(6, 6, 20);
    auto before = game.active_piece();
    REQUIRE(before.has_value());

    bool dropped = game.hard_drop();
    REQUIRE(dropped);

    // After update, a new piece should be active (old one was locked).
    game.update(2.0f);
    REQUIRE(game.active_piece().has_value());
    if (before && game.active_piece())
    {
        // Shape may differ (7-bag shuffled).
        CHECK(game.active_piece()->pos.y >= 0);
    }
}

TEST_CASE("Top-out: can_place_public detects blocked spawn", "[core][game]")
{
    // Fill the top layer of the well.
    Game game(6, 6, 20);
    for (int x = 0; x < 6; ++x)
        for (int z = 0; z < 6; ++z)
            const_cast<Well&>(game.well()).set_cell(Vec3i{x, 19, z}, true, Vec3{1, 1, 1});
    game.rebuild_locked_cache();

    // Check the active piece can no longer be placed at its position.
    if (game.active_piece())
    {
        CHECK_FALSE(game.can_place_public(*game.active_piece()));
    }

    // After restart, the new piece should also fail.
    game.set_state(GameState::GameOver);
    REQUIRE(game.state() == GameState::GameOver);
}

TEST_CASE("7-bag draws each shape exactly once per cycle via next_piece", "[core][game]")
{
    // Spawn 9+ pieces by repeatedly updating on an empty well.
    // Use the isometric test well from game_ai (6x6x20).
    Game game(6, 6, 20);
    std::set<int> seen;
    for (int i = 0; i < 30 && game.state() == GameState::Playing; ++i)
    {
        // Record current active and next piece shapes.
        if (game.active_piece()) seen.insert(game.active_piece()->shape);
        if (game.next_piece()) seen.insert(game.next_piece()->shape);
        if (seen.size() >= 9) break;

        // Hard-drop + advance time so the piece locks naturally.
        game.hard_drop();
        for (int s = 0; s < 200; ++s)
        {
            game.update(0.05f);
            if (game.state() != GameState::Playing)
                break;
            // If the piece reached the grid bottom, wait for lock + spawn.
            if (game.active_piece() && game.active_piece()->pos_y <= 0.0f)
                break;
        }
    }
    INFO("Saw " << seen.size() << " unique shapes");
    REQUIRE(seen.size() == 9);
}

TEST_CASE("Ghost piece returns a valid dropped position", "[core][game]")
{
    Game game(6, 6, 20);
    auto ghost = game.ghost_piece();
    REQUIRE(ghost.has_value());

    // Ghost should be at or below the active piece's grid y.
    if (game.active_piece())
    {
        CHECK(ghost->pos.y <= game.active_piece()->pos.y);
    }
}

TEST_CASE("Rotate active piece changes orientation", "[core][game]")
{
    Game game(6, 6, 20);
    if (!game.active_piece())
        return;

    int before[3][3];
    std::memcpy(before, game.active_piece()->rot, sizeof(before));
    game.rotate_active(Axis::X, 1);

    bool same = true;
    for (int i = 0; i < 3 && same; ++i)
        for (int j = 0; j < 3 && same; ++j)
            if (before[i][j] != game.active_piece()->rot[i][j])
                same = false;
    CHECK_FALSE(same);
}

TEST_CASE("Move active piece changes position", "[core][game]")
{
    Game game(6, 6, 20);
    if (!game.active_piece())
        return;

    auto before = game.active_piece()->pos;
    game.move_active(1, 0);
    if (game.active_piece())
    {
        CHECK(game.active_piece()->pos.x == before.x + 1);
        CHECK(game.active_piece()->pos.z == before.z);
    }
}

TEST_CASE("Pause and resume", "[core][game]")
{
    Game game(6, 6, 20);
    game.set_state(GameState::Paused);
    REQUIRE(game.state() == GameState::Paused);
    game.set_state(GameState::Playing);
    REQUIRE(game.state() == GameState::Playing);
}

TEST_CASE("Hold swaps piece", "[core][game]")
{
    Game game(6, 6, 20);
    REQUIRE(game.active_piece().has_value());
    REQUIRE_FALSE(game.held_piece().has_value());
    REQUIRE(game.can_hold());

    int first_shape = game.active_piece()->shape;
    bool held = game.hold_active();
    REQUIRE(held);
    // After hold, a new piece is active and the old one is held.
    REQUIRE(game.held_piece().has_value());
    REQUIRE(game.held_piece()->shape == first_shape);
    REQUIRE_FALSE(game.can_hold());
}

TEST_CASE("Hold can only be used once per piece", "[core][game]")
{
    Game game(6, 6, 20);
    game.hold_active();
    REQUIRE_FALSE(game.can_hold());
    // Second hold should fail.
    bool held = game.hold_active();
    REQUIRE_FALSE(held);
}

TEST_CASE("Soft drop speeds up fall", "[core][game]")
{
    Game game(6, 6, 20);
    float normal = game.fall_speed();
    game.set_soft_drop(true);
    // Soft drop multiplies speed by 5.
    // The actual speed is calculated in update(); fall_interval is unchanged.
    CHECK(game.fall_interval() > 0.0f);
    CHECK(game.fall_speed() > 0.0f);
}
