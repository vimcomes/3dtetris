#pragma once

#include <vector>

#include "config.h"
#include "game.h"

struct AiPlanStep
{
    enum class Type { MoveX, MoveZ, RotX, RotY, RotZ, Drop };
    Type type;
    int value = 0; // +1 / -1 for moves/rots, ignored for Drop
};

class GameAi
{
public:
    std::vector<AiPlanStep> compute_plan(const Game& game, const AiConfig& ai_cfg = {});

private:
    static bool can_place(const Game& game, const Piece& p);
    static Piece drop_piece(const Game& game, Piece p);
};
