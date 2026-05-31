#include <catch2/catch_test_macros.hpp>

#include "game.h"
#include "game_ai.h"

TEST_CASE("AI produces a valid plan on an empty well", "[core][ai]")
{
    Game game(6, 6, 20);
    GameAi ai;
    auto plan = ai.compute_plan(game);

    // The plan should contain at least one step (usually hard drop).
    REQUIRE_FALSE(plan.empty());

    // Check that all steps have valid types.
    for (const auto& step : plan)
    {
        CHECK((step.type == AiPlanStep::Type::MoveX ||
               step.type == AiPlanStep::Type::MoveZ ||
               step.type == AiPlanStep::Type::RotX ||
               step.type == AiPlanStep::Type::RotY ||
               step.type == AiPlanStep::Type::RotZ ||
               step.type == AiPlanStep::Type::Drop));
    }

    // The last step should be a Drop.
    REQUIRE(plan.back().type == AiPlanStep::Type::Drop);
}

TEST_CASE("AI plan is deterministic with same seed", "[core][ai]")
{
    Game game(6, 6, 20);
    GameAi ai;
    auto plan1 = ai.compute_plan(game);
    auto plan2 = ai.compute_plan(game);

    REQUIRE(plan1.size() == plan2.size());
    for (size_t i = 0; i < plan1.size(); ++i)
    {
        REQUIRE(plan1[i].type == plan2[i].type);
        REQUIRE(plan1[i].value == plan2[i].value);
    }
}
