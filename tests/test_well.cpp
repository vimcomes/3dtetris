#include <catch2/catch_test_macros.hpp>

#include "game.h"

TEST_CASE("Well::in_bounds rejects out-of-range coordinates", "[core][well]")
{
    Well w(4, 5, 6);
    CHECK(w.in_bounds(Vec3i{0, 0, 0}));
    CHECK(w.in_bounds(Vec3i{3, 5, 4}));
    CHECK_FALSE(w.in_bounds(Vec3i{-1, 0, 0}));
    CHECK_FALSE(w.in_bounds(Vec3i{4, 0, 0}));
    CHECK_FALSE(w.in_bounds(Vec3i{0, -1, 0}));
    CHECK_FALSE(w.in_bounds(Vec3i{0, 6, 0}));
    CHECK_FALSE(w.in_bounds(Vec3i{0, 0, -1}));
    CHECK_FALSE(w.in_bounds(Vec3i{0, 0, 5}));
}

TEST_CASE("Well::is_free after set_cell", "[core][well]")
{
    Well w(3, 3, 3);
    CHECK(w.is_free(Vec3i{1, 1, 1}));
    w.set_cell(Vec3i{1, 1, 1}, true, Vec3{1, 0, 0});
    CHECK_FALSE(w.is_free(Vec3i{1, 1, 1}));
    CHECK(w.is_free(Vec3i{0, 0, 0}));
}

TEST_CASE("Well::lock_piece places cells correctly", "[core][well]")
{
    Well w(5, 5, 5);
    Piece p;
    p.shape = 0;
    p.pos = Vec3i{2, 0, 2};
    p.pos_y = 0.0f;
    p.blocks = {Vec3i{0, 0, 0}, Vec3i{1, 0, 0}};
    p.color = Vec3{0, 1, 0};

    w.lock_piece(p);
    CHECK_FALSE(w.is_free(Vec3i{2, 0, 2}));
    CHECK_FALSE(w.is_free(Vec3i{3, 0, 2}));
    CHECK(w.is_free(Vec3i{2, 1, 2}));
}
