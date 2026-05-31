#include "ui/panels.h"

#include <algorithm>
#include "imgui_internal.h"

#include "config.h"
#include "geometry.h"
#include "gfx/mesh.h"
#include "math.h"

namespace ui {

void build_dockspace(bool& dock_built, ImGuiID dockspace_id, const ImVec2& display_size)
{
    ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    if (!dock_built)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, dock_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, display_size);
        ImGuiID right_id = 0;
        ImGuiID left_id = dockspace_id;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.32f, &right_id, &left_id);
        ImGuiID right_bottom = 0;
        ImGuiID right_top = right_id;
        ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.5f, &right_bottom, &right_top);
        ImGui::DockBuilderDockWindow("Controls", right_top);
        ImGui::DockBuilderDockWindow("Iso View", right_bottom);
        ImGui::DockBuilderDockWindow("Viewport", left_id);
        ImGui::DockBuilderFinish(dockspace_id);
        dock_built = true;
    }
}

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
                    SpinState& spin)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.04f, 0.10f, 0.92f));
    if (ImGui::Begin("Controls"))
    {
        if (const auto& np = game.next_piece())
        {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "NEXT");
            ImVec2 canvas = ImGui::GetCursorScreenPos();
            float step = 13.f;
            float cx = canvas.x + 55.f;
            float cy = canvas.y + 40.f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (const auto& b : np->blocks)
            {
                Vec3i rb = apply_rot(np->rot, b);
                float ix = static_cast<float>(rb.x - rb.z) * step;
                float iy = (static_cast<float>(rb.x + rb.z) * 0.5f - static_cast<float>(rb.y)) * step;
                float x0 = cx + ix - step * 0.45f;
                float y0 = cy + iy - step * 0.45f;
                float x1 = cx + ix + step * 0.45f;
                float y1 = cy + iy + step * 0.45f;
                ImU32 col = IM_COL32(
                    static_cast<int>(np->color.x * 255),
                    static_cast<int>(np->color.y * 255),
                    static_cast<int>(np->color.z * 255), 230);
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col, 2.f);
                dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
                            IM_COL32(200, 230, 255, 120), 2.f, 0, 1.2f);
            }
            ImGui::Dummy(ImVec2(110.f, 80.f));
        }

        if (const auto& hp = game.held_piece())
        {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "HELD%s", game.can_hold() ? "" : " (used)");
            ImVec2 canvas = ImGui::GetCursorScreenPos();
            float step = 13.f;
            float cx = canvas.x + 55.f;
            float cy = canvas.y + 40.f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (const auto& b : hp->blocks)
            {
                Vec3i rb = apply_rot(hp->rot, b);
                float ix = static_cast<float>(rb.x - rb.z) * step;
                float iy = (static_cast<float>(rb.x + rb.z) * 0.5f - static_cast<float>(rb.y)) * step;
                float x0 = cx + ix - step * 0.45f;
                float y0 = cy + iy - step * 0.45f;
                float x1 = cx + ix + step * 0.45f;
                float y1 = cy + iy + step * 0.45f;
                ImU32 col = IM_COL32(
                    static_cast<int>(hp->color.x * 255),
                    static_cast<int>(hp->color.y * 255),
                    static_cast<int>(hp->color.z * 255), 160);
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col, 2.f);
                dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
                            IM_COL32(200, 230, 255, 80), 2.f, 0, 1.2f);
            }
            ImGui::Dummy(ImVec2(110.f, 80.f));
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Render");
        ImGui::Checkbox("Wireframe active piece (F)", &wireframe_active);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "SCORE  %d", game.score());
        ImGui::Text("Level:  %d", game.level());
        ImGui::Text("Lines:  %d", game.total_cleared());
        ImGui::Text("Last clear: %d", game.last_cleared());
        ImGui::Text("Fall speed: %.2f /s", game.fall_speed());
        ImGui::Separator();
        ImGui::TextUnformatted("Well size");
        ImGui::SliderInt("Width", &desired_width, 3, 7);
        desired_width = std::clamp(desired_width, 3, 7);
        ImGui::SliderInt("Depth", &desired_depth, 3, 7);
        desired_depth = std::clamp(desired_depth, 3, 7);
        ImGui::SliderInt("Height", &desired_height, 5, 20);
        bool size_changed = desired_width != well_width || desired_depth != well_depth || desired_height != well_height;
        bool level_changed = desired_start_level != config.start_level;
        ImGui::SliderInt("Start level", &desired_start_level, 0, 9);
        if (ImGui::Button("Apply size") && size_changed)
        {
            well_width = desired_width;
            well_depth = desired_depth;
            well_height = desired_height;
            destroy_mesh(floor_mesh);
            destroy_mesh(walls_mesh);
            destroy_mesh(wall_grid_mesh);
            destroy_mesh(bottom_mesh);
            floor_mesh = make_mesh(build_floor_grid_lines(well_width, well_depth, cell_size), GL_LINES);
            walls_mesh = make_mesh(build_well_outline_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
            wall_grid_mesh = make_mesh(build_well_wall_grid_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
            destroy_mesh(iso_walls_mesh);
            iso_walls_mesh = make_mesh(build_well_outline_lines_culled(well_width, well_depth, well_height, cell_size), GL_LINES);
            destroy_mesh(iso_wall_grid_mesh);
            iso_wall_grid_mesh = make_mesh(build_well_wall_grid_lines_culled(well_width, well_depth, well_height, cell_size), GL_LINES);
            bottom_mesh = make_mesh(build_bottom_plane(well_width, well_depth, cell_size), GL_TRIANGLES);
            game = Game{well_width, well_depth, well_height, config.shapes, config.fall_interval, config.start_level};
            spin = {};
            auto_play = {};
            yaw = 0.0f;
            pitch = to_radians(89.0f);
            distance = std::max(12.0f, static_cast<float>(std::max(well_width, well_depth)) * 2.4f);
            needs_reframe = true;
            iso_needs_reframe = true;
            dist_iso = std::max(12.0f, static_cast<float>(std::max(well_width, well_depth)) * 3.0f);
        }
        if (ImGui::Button("Apply start level") && level_changed)
        {
            game = Game{well_width, well_depth, well_height, config.shapes, config.fall_interval, desired_start_level};
            spin = {};
            auto_play = {};
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Controls");
        ImGui::BulletText("Mouse drag: orbit (RMB drag to tilt)");
        ImGui::BulletText("Mouse wheel: zoom");
        ImGui::BulletText("Arrows: move");
        ImGui::BulletText("E/D: rotate X, W/S: rotate Z, Q/A: rotate Y");
        ImGui::BulletText("Space: hard drop");
        ImGui::BulletText("V: soft drop (hold)");
        ImGui::BulletText("F: toggle wireframe");
        ImGui::BulletText("C: hold piece");
        ImGui::BulletText("P: pause / resume");
        ImGui::BulletText("Esc: quit");
        ImGui::Separator();
        ImGui::Checkbox("Auto play", &auto_play.enabled);
        ImGui::Text("Auto steps: %d", auto_play.steps);
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace ui
