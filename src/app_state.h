#pragma once

#include "game.h"
#include "game_ai.h"
#include "gfx/mesh.h"

struct SpinState
{
    bool active = false;
    Axis axis = Axis::Y;
    int dir = 1;
    float t = 0.f;
    float duration = 0.15f;
};

struct KeyState
{
    bool rot_x_pos = false, rot_x_neg = false;
    bool rot_y_pos = false, rot_y_neg = false;
    bool rot_z_pos = false, rot_z_neg = false;
    bool space = false;
    bool f = false;
    bool p = false;
};

struct RepeatState
{
    float timer = 0.f;
    float delay = 0.15f;
};

struct RmbState
{
    bool down = false;
    bool dragged = false;
    double last_x = 0.0;
    double last_y = 0.0;
};

struct AutoPlayState
{
    bool enabled = false;
    float step_timer = 0.f;
    int steps = 0;
    std::vector<AiPlanStep> plan;
    size_t plan_idx = 0;
    float plan_piece_top_y = -1.f;
};
