#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

// Smoke test — Catch2 is working.
TEST_CASE("Smoke", "[core]")
{
    REQUIRE(1 + 1 == 2);
}
