#pragma once

#include "game.h"

struct GLFWwindow;

namespace ui {

void init_imgui(GLFWwindow* window);

void hud_overlay(const Game& game, float viewport_x0, float viewport_y0);
void game_over_overlay(Game& game, int& high_score, bool& auto_play_reset, bool& spin_reset, bool& score_saved);
void paused_overlay();

} // namespace ui
