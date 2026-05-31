#pragma once

#include "imgui.h"

#include "game.h"
#include "app_state.h"

struct AppConfig;

namespace ui {

void build_dockspace(bool& dock_built, ImGuiID dockspace_id, const ImVec2& display_size);

void controls_panel(Game& game, const AppConfig& config,
                    int& desired_width, int& desired_depth, int& desired_height,
                    int& desired_start_level,
                    int& well_width, int& well_depth, int& well_height,
                    float cell_size,
                    bool& wireframe_active, AutoPlayState& auto_play,
                    GlMesh& floor_mesh, GlMesh& walls_mesh, GlMesh& wall_grid_mesh,
                    GlMesh& bottom_mesh, GlMesh& iso_walls_mesh, GlMesh& iso_wall_grid_mesh,
                    float& yaw, float& pitch, float& distance,
                    bool& needs_reframe, bool& iso_needs_reframe, float& dist_iso,
                    SpinState& spin);
}
