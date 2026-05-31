#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "game.h"
#include "game_ai.h"
#include "geometry.h"
#include "math.h"
#include "config.h"
#include "render.h"
#include "gfx/mesh.h"
#include "input.h"
#include "app_state.h"
#include "gfx/renderer.h"
#include "scores.h"
#include "ui/hud.h"
#include "ui/panels.h"

int main()
{
    if (!init_glfw())
    {
        return EXIT_FAILURE;
    }

    constexpr int start_width = 1280;
    constexpr int start_height = 720;
    GLFWwindow* window = glfwCreateWindow(start_width, start_height, "3D Tetris", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwFocusWindow(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!init_glad())
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    ui::init_imgui(window);
    ImGuiIO& io = ImGui::GetIO();

    glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);

    auto pick_config_path = []()
    {
        namespace fs = std::filesystem;
        std::vector<std::string> candidates = {"config.toml", "../config.toml"};
        for (const auto& p : candidates)
        {
            if (fs::exists(p)) return p;
        }
        return candidates.front();
    };

    AppConfig config = load_config(pick_config_path());
    auto clamp_width_depth = [](int v)
    {
        return std::clamp(v, 3, 7);
    };
    RenderShader shader = create_render_shader();
    BlockShader block_shader = create_block_shader();
    GradientShader grad_shader = create_gradient_shader();
    RenderPalette palette = config.palette;

    auto cube_mesh = make_mesh(build_cube_vertices(), GL_TRIANGLES);
    auto cube_edges_mesh = make_mesh(build_cube_edge_lines(), GL_LINES);

    int well_width = clamp_width_depth(config.well_width);
    int well_depth = clamp_width_depth(config.well_depth);
    int well_height = std::clamp(config.well_height, 5, 20);
    const float cell_size = 1.0f;

    auto floor_mesh = make_mesh(build_floor_grid_lines(well_width, well_depth, cell_size), GL_LINES);
    auto walls_mesh = make_mesh(build_well_outline_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
    auto wall_grid_mesh = make_mesh(build_well_wall_grid_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
    auto iso_walls_mesh = make_mesh(build_well_outline_lines_culled(well_width, well_depth, well_height, cell_size), GL_LINES);
    auto iso_wall_grid_mesh = make_mesh(build_well_wall_grid_lines_culled(well_width, well_depth, well_height, cell_size), GL_LINES);
    auto bottom_mesh = make_mesh(build_bottom_plane(well_width, well_depth, cell_size), GL_TRIANGLES);
    GlMesh active_mesh = make_empty_mesh();
    GlMesh active_edges = make_empty_mesh(GL_LINES);

    float yaw = 0.0f;
    float pitch = to_radians(89.0f);
    float distance = std::max(24.0f, static_cast<float>(std::max(well_width, well_depth)) * 2.4f);
    bool needs_reframe = true;
    ImVec2 prev_viewport_size{-1.f, -1.f};
    float dist_iso = std::max(24.0f, static_cast<float>(std::max(well_width, well_depth)) * 3.0f);
    bool iso_needs_reframe = true;
    ImVec2 iso_prev_size{-1.f, -1.f};
    double prev_time = glfwGetTime();
    float app_time = 0.0f;

    Game game{well_width, well_depth, well_height, config.shapes, config.fall_interval, config.start_level};
    SpinState spin;

    struct KeyState
    {
        bool rot_x_pos = false, rot_x_neg = false;
        bool rot_y_pos = false, rot_y_neg = false;
        bool rot_z_pos = false, rot_z_neg = false;
        bool space = false;
        bool f = false;
        bool p = false;
    } prev_keys;

    struct RepeatState
    {
        float timer = 0.f;
        float delay = 0.15f;
    };
    RepeatState move_x_neg, move_x_pos, move_z_neg, move_z_pos;

    bool wireframe_active = false;
    bool rotating = false;
    struct RmbState
    {
        bool down = false;
        bool dragged = false;
        double last_x = 0.0;
        double last_y = 0.0;
    } rmb;
    double last_mouse_x = 0.0;
    bool dock_built = false;
    struct ViewportRect { float x0 = 0.f, y0 = 0.f, x1 = static_cast<float>(start_width), y1 = static_cast<float>(start_height); } viewport_rect;

    AutoPlayState auto_play;

    int prev_total_cleared = 0;
    float clear_flash_t = 0.f;
    static constexpr float clear_flash_dur = 0.35f;

    int desired_width = well_width;
    int desired_depth = well_depth;
    int desired_height = well_height;
    int desired_start_level = config.start_level;

    int high_score = load_high_score("highscore.txt");

    std::cout << "Controls:\n"
                 "  Mouse drag: orbit (RMB drag to tilt)\n"
                 "  Mouse wheel: zoom\n"
                 "  Arrows: move piece (X/Z)\n"
                 "  E/D: rotate around X\n"
                 "  W/S: rotate around Z\n"
                 "  Q/A: rotate around Y\n"
                 "  Space: hard drop\n"
                 "  F: toggle wireframe render for active piece\n"
                 "  ESC: quit\n";

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - prev_time);
        prev_time = now;
        app_time += dt;

        // Orbiting light positions (shared by main view and iso view).
        const float lx0 = std::sin(app_time * 0.3f) * 5.0f;
        const float lz0 = std::cos(app_time * 0.3f) * 5.0f;
        const float ly0 = static_cast<float>(well_height) * cell_size * 0.8f;
        const float li0 = 4.0f + std::sin(app_time * 1.2f) * 0.6f;
        const float lx1 = -std::sin(app_time * 0.4f) * 5.0f;
        const float lz1 = -std::cos(app_time * 0.4f) * 5.0f;
        const float ly1 = static_cast<float>(well_height) * cell_size * 0.5f;
        const float li1 = 3.5f + std::sin(app_time * 0.9f + 1.0f) * 0.5f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        ui::build_dockspace(dock_built, dockspace_id, io.DisplaySize);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Clear whole framebuffer; ImGui windows are transparent and per-view clears are scissored.
        {
            int fb_width = 1;
            int fb_height = 1;
            glfwGetFramebufferSize(window, &fb_width, &fb_height);
            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, fb_width, fb_height);
            glClearColor(palette.clear.x, palette.clear.y, palette.clear.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // Mouse orbit around vertical axis (only inside viewport area).
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(window, &mx, &my);
        bool in_viewport = mx >= viewport_rect.x0 && mx <= viewport_rect.x1 && my >= viewport_rect.y0 && my <= viewport_rect.y1;
        bool viewport_hot = in_viewport || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow);
        if (in_viewport && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            if (!rotating)
            {
                rotating = true;
                last_mouse_x = mx;
            }
            double dx = mx - last_mouse_x;
            yaw += static_cast<float>(dx) * 0.005f;
            last_mouse_x = mx;
        }
        else
        {
            rotating = false;
        }

        if (in_viewport && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            if (!rmb.down)
            {
                rmb.down = true;
                rmb.dragged = false;
                rmb.last_x = mx;
                rmb.last_y = my;
            }
            double dx = mx - rmb.last_x;
            double dy = my - rmb.last_y;
            if (std::abs(dx) > 2.0 || std::abs(dy) > 2.0)
            {
                rmb.dragged = true;
            }
            pitch = std::clamp(pitch + static_cast<float>(dy) * 0.004f, to_radians(40.0f), to_radians(88.0f));
            rmb.last_x = mx;
            rmb.last_y = my;
        }
        else if (rmb.down)
        {
            if (!rmb.dragged)
            {
                yaw = 0.0f;
                pitch = to_radians(89.0f);
                distance = 24.0f;
            }
            rmb.down = false;
            rmb.dragged = false;
        }

        // Camera zoom via mouse wheel; pitch adjusted via RMB drag.
        const float zoom_speed = 3.0f;
        double scroll = consume_scroll_delta();
        if (scroll != 0.0)
        {
            distance = std::clamp(distance - static_cast<float>(scroll) * zoom_speed, 4.0f, 40.0f);
        }

        // Piece input with edge detection for rotations/drop.
        auto is_down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };
        auto key_down = [&](const std::string& name) { return is_down(key_name_to_glfw(name)); };
        bool left_now = key_down(config.controls.move_left);
        bool right_now = key_down(config.controls.move_right);
        bool up_now = key_down(config.controls.move_forward);
        bool down_now = key_down(config.controls.move_back);
        bool rot_xp_now = key_down(config.controls.rot_x_pos);
        bool rot_xn_now = key_down(config.controls.rot_x_neg);
        bool rot_zp_now = key_down(config.controls.rot_z_pos);
        bool rot_zn_now = key_down(config.controls.rot_z_neg);
        bool rot_yp_now = key_down(config.controls.rot_y_pos);
        bool rot_yn_now = key_down(config.controls.rot_y_neg);
        bool space_now = key_down(config.controls.hard_drop);
        bool v_now = key_down(config.controls.soft_drop);
        bool c_now = key_down(config.controls.hold);
        bool f_now = key_down(config.controls.wireframe);
        bool p_now = key_down(config.controls.pause);

        // Rotations (Blockout-style) — only when game is active
        if (!io.WantCaptureKeyboard && viewport_hot && rot_xp_now && !prev_keys.rot_x_pos && game.state() == GameState::Playing && game.rotate_active(Axis::X, 1))
        {
            spin = {true, Axis::X, 1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && rot_xn_now && !prev_keys.rot_x_neg && game.state() == GameState::Playing && game.rotate_active(Axis::X, -1))
        {
            spin = {true, Axis::X, -1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && rot_zp_now && !prev_keys.rot_z_pos && game.state() == GameState::Playing && game.rotate_active(Axis::Z, 1))
        {
            spin = {true, Axis::Z, 1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && rot_zn_now && !prev_keys.rot_z_neg && game.state() == GameState::Playing && game.rotate_active(Axis::Z, -1))
        {
            spin = {true, Axis::Z, -1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && rot_yp_now && !prev_keys.rot_y_pos && game.state() == GameState::Playing && game.rotate_active(Axis::Y, -1))
        {
            spin = {true, Axis::Y, -1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && rot_yn_now && !prev_keys.rot_y_neg && game.state() == GameState::Playing && game.rotate_active(Axis::Y, 1))
        {
            spin = {true, Axis::Y, 1, 0.f, 1.0f / 3.0f};
        }

        auto handle_repeat = [&](bool pressed, RepeatState& rep, auto action)
        {
            if (pressed)
            {
                rep.timer += dt;
                if (rep.timer >= rep.delay)
                {
                    rep.timer = 0.f;
                    action();
                }
            }
            else
            {
                rep.timer = rep.delay; // allow immediate on next press
            }
        };

        if (!io.WantCaptureKeyboard && game.state() == GameState::Playing)
        {
            handle_repeat(left_now && viewport_hot, move_x_neg, [&] { game.move_active(-1, 0); });
            handle_repeat(right_now && viewport_hot, move_x_pos, [&] { game.move_active(1, 0); });
            handle_repeat(up_now && viewport_hot, move_z_neg, [&] { game.move_active(0, -1); });
            handle_repeat(down_now && viewport_hot, move_z_pos, [&] { game.move_active(0, 1); });
        }

        if (!io.WantCaptureKeyboard && viewport_hot && space_now && !prev_keys.space && game.state() == GameState::Playing)
        {
            game.hard_drop();
            spin.active = false;
        }
        if (!io.WantCaptureKeyboard && viewport_hot && f_now && !prev_keys.f)
        {
            wireframe_active = !wireframe_active;
        }
        if (!io.WantCaptureKeyboard && p_now && !prev_keys.p)
        {
            if (game.state() == GameState::Playing)
                game.set_state(GameState::Paused);
            else if (game.state() == GameState::Paused)
                game.set_state(GameState::Playing);
        }

        static bool prev_c = false;
        if (!io.WantCaptureKeyboard && viewport_hot && c_now && !prev_c && game.state() == GameState::Playing)
        {
            game.hold_active();
        }
        prev_c = c_now;
        game.set_soft_drop(!io.WantCaptureKeyboard && v_now && game.state() == GameState::Playing);

        prev_keys = {rot_xp_now, rot_xn_now, rot_zp_now, rot_zn_now, rot_yp_now, rot_yn_now, space_now, f_now, p_now};

        game.update(dt);
        {
            int tc = game.total_cleared();
            if (tc > prev_total_cleared) { clear_flash_t = clear_flash_dur; }
            prev_total_cleared = tc;
            clear_flash_t = std::max(0.f, clear_flash_t - dt);
        }
        if (spin.active)
        {
            spin.t += dt;
            if (spin.t >= spin.duration)
            {
                spin.active = false;
            }
        }

        float spin_angle = 0.0f;
        if (spin.active)
        {
            float remaining = 1.0f - std::min(spin.t / spin.duration, 1.0f);
            float sign = static_cast<float>(spin.dir >= 0 ? 1 : -1);
            spin_angle = remaining * -sign * to_radians(90.0f);
        }
        float fall_offset = 0.0f;

    if (auto_play.enabled && game.active_piece())
    {
        auto_play.step_timer += dt;
        // Invalidate stale plan if a new piece spawned (natural lock before Drop step).
        // Pieces only fall, so pos_y higher than when plan was built means a new piece appeared.
        if (!auto_play.plan.empty() && game.active_piece()->pos_y > auto_play.plan_piece_top_y + 0.5f)
        {
            auto_play.plan.clear();
            auto_play.plan_idx = 0;
        }
        if (auto_play.plan.empty())
        {
            GameAi ai;
            auto_play.plan = ai.compute_plan(game, config.ai);
            auto_play.plan_idx = 0;
            auto_play.steps = 0;
            auto_play.plan_piece_top_y = game.active_piece()->pos_y;
        }
        if (!auto_play.plan.empty() && auto_play.plan_idx < auto_play.plan.size() && auto_play.step_timer >= 0.18f)
        {
            auto_play.step_timer = 0.f;
            const auto& act = auto_play.plan[auto_play.plan_idx++];
            switch (act.type)
            {
            case AiPlanStep::Type::RotX: game.rotate_active(Axis::X, act.value); break;
            case AiPlanStep::Type::RotY: game.rotate_active(Axis::Y, act.value); break;
            case AiPlanStep::Type::RotZ: game.rotate_active(Axis::Z, act.value); break;
            case AiPlanStep::Type::MoveX: game.move_active(act.value, 0); break;
            case AiPlanStep::Type::MoveZ: game.move_active(0, act.value); break;
            case AiPlanStep::Type::Drop:
                game.hard_drop();
                spin.active = false;
                spin_angle = 0.0f;
                auto_play.plan.clear();
                auto_play.plan_idx = 0;
                break;
            }
            auto_play.steps++;
        }
    }

ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGuiWindowFlags viewport_flags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoScrollbar |
                                          ImGuiWindowFlags_NoScrollWithMouse |
                                          ImGuiWindowFlags_NoBackground |
                                          ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("Viewport", nullptr, viewport_flags))
        {
            ImVec2 content_min = ImGui::GetWindowContentRegionMin();
            ImVec2 content_max = ImGui::GetWindowContentRegionMax();
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 viewport_pos{window_pos.x + content_min.x, window_pos.y + content_min.y};
            ImVec2 viewport_size{content_max.x - content_min.x, content_max.y - content_min.y};
            viewport_rect = {viewport_pos.x, viewport_pos.y, viewport_pos.x + viewport_size.x, viewport_pos.y + viewport_size.y};

            if (viewport_size.x != prev_viewport_size.x || viewport_size.y != prev_viewport_size.y)
            {
                needs_reframe = true;
                prev_viewport_size = viewport_size;
            }

            if (viewport_size.x > 0.f && viewport_size.y > 0.f)
            {
                int fb_width = 1;
                int fb_height = 1;
                glfwGetFramebufferSize(window, &fb_width, &fb_height);
                ImVec2 fb_scale = io.DisplayFramebufferScale;
                int vx = static_cast<int>(viewport_pos.x * fb_scale.x);
                int vy = static_cast<int>((io.DisplaySize.y - viewport_pos.y - viewport_size.y) * fb_scale.y);
                int vw = static_cast<int>(viewport_size.x * fb_scale.x);
                int vh = static_cast<int>(viewport_size.y * fb_scale.y);

                glViewport(vx, vy, vw, vh);
                glEnable(GL_SCISSOR_TEST);
                glScissor(vx, vy, vw, vh);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                draw_gradient_bg(grad_shader, palette.grad_bottom, palette.grad_top);

                float center_y_view = static_cast<float>(well_height) * cell_size * 0.5f;
                float cy = std::cos(yaw);
                float sy = std::sin(yaw);
                float cp = std::cos(pitch);
                float sp = std::sin(pitch);
                if (needs_reframe)
                {
                    auto ndc_max_for_distance = [&](float test_distance)
                    {
                        float cy_t = std::cos(yaw);
                        float sy_t = std::sin(yaw);
                        float cp_t = std::cos(pitch);
                        float sp_t = std::sin(pitch);
                        Vec3 eye_t{test_distance * sy_t * cp_t, center_y_view + test_distance * sp_t, test_distance * cy_t * cp_t};
                        Mat4 view_t = look_at(eye_t, Vec3{0.f, center_y_view, 0.f}, Vec3{0.f, 1.f, 0.f});
                        float aspect = viewport_size.x / viewport_size.y;
                        Mat4 proj_t = perspective(60.0f, aspect, 0.1f, 200.0f);
                        Mat4 mvp_t = multiply(proj_t, view_t);
                        auto clip_ndc = [&](const Vec3& p)
                        {
                            float x = mvp_t.m[0] * p.x + mvp_t.m[4] * p.y + mvp_t.m[8] * p.z + mvp_t.m[12];
                            float y = mvp_t.m[1] * p.x + mvp_t.m[5] * p.y + mvp_t.m[9] * p.z + mvp_t.m[13];
                            float w = mvp_t.m[3] * p.x + mvp_t.m[7] * p.y + mvp_t.m[11] * p.z + mvp_t.m[15];
                            if (std::abs(w) < 1e-4f) w = 1e-4f;
                            return std::max(std::abs(x / w), std::abs(y / w));
                        };
                        float half_w = 0.5f * static_cast<float>(well_width) * cell_size;
                        float half_d = 0.5f * static_cast<float>(well_depth) * cell_size;
                        float top_y = static_cast<float>(well_height) * cell_size;
                        std::array<Vec3, 8> corners = {{
                            {-half_w, 0.f, -half_d},
                            { half_w, 0.f, -half_d},
                            { half_w, 0.f,  half_d},
                            {-half_w, 0.f,  half_d},
                            {-half_w, top_y, -half_d},
                            { half_w, top_y, -half_d},
                            { half_w, top_y,  half_d},
                            {-half_w, top_y,  half_d},
                        }};
                        float max_ndc = 0.f;
                        for (const auto& c : corners)
                        {
                            max_ndc = std::max(max_ndc, clip_ndc(c));
                        }
                        return max_ndc;
                    };

                    float target = 0.95f;
                    float lo = 1.0f;
                    float hi = 200.0f;
                    for (int i = 0; i < 24; ++i)
                    {
                        float mid = 0.5f * (lo + hi);
                        float ndc = ndc_max_for_distance(mid);
                        if (ndc > target)
                        {
                            lo = mid; // too big on screen, pull back
                        }
                        else
                        {
                            hi = mid; // too small, push closer
                        }
                    }
                    distance = hi;
                    needs_reframe = false;
                }
                Vec3 eye{distance * sy * cp, center_y_view + distance * sp, distance * cy * cp};
                Mat4 view = look_at(eye, Vec3{0.f, center_y_view, 0.f}, Vec3{0.f, 1.f, 0.f});
                float aspect = viewport_size.x / viewport_size.y;

                Mat4 proj = perspective(60.0f, aspect, 0.1f, 100.0f);
                Mat4 mvp_world = multiply(proj, view);

                auto setup_block_shader = [&]() {
                    gfx::setup_block_shader(block_shader, app_time, lx0, ly0, lz0, li0, lx1, ly1, lz1, li1);
                };

                auto draw_block = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha, float emissive) {
                    Mat4 mvp = multiply(mvp_world, model);
                    gfx::draw_block(mesh, block_shader, mvp, model, tint, alpha, emissive);
                };

                auto draw_flat = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha) {
                    Mat4 mvp = multiply(mvp_world, model);
                    gfx::draw_flat(mesh, shader, mvp, tint, alpha);
                };

                // --- Floor plane first (depth set here; grid lines at y=0 draw on top) ---
                setup_block_shader();
                glDisable(GL_CULL_FACE);
                draw_block(bottom_mesh, translation(Vec3{0.f, -0.02f, 0.f}), Vec3{0.55f, 0.05f, 0.95f}, 1.0f, 3.0f);
                glEnable(GL_CULL_FACE);

                // --- Lines: floor grid + well walls (flat shader) ---
                glUseProgram(shader.program);
                glUniform1f(shader.u_emissive, 1.0f);
                draw_flat(floor_mesh,     identity(), palette.grid, 0.35f);
                draw_flat(walls_mesh,     identity(), palette.grid, 0.30f);
                draw_flat(wall_grid_mesh, identity(), palette.grid, 0.12f);

                // --- Locked cells: glass faces then bright edges ---
                const auto& locked_positions = game.locked_cells();
                const auto& locked_colors = game.locked_colors();
                {
                    float flash = clear_flash_t > 0.f ? (1.0f + clear_flash_t / clear_flash_dur * 3.5f) : 1.0f;
                    // Transparent faces (block shader, depth write off)
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    setup_block_shader();
                    for (size_t i = 0; i < locked_positions.size(); ++i)
                    {
                        Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                        draw_block(cube_mesh, translation(world), locked_colors[i], 0.80f, flash);
                    }
                    glDepthMask(GL_TRUE);
                    // Bright box edges (flat shader)
                    glUseProgram(shader.program);
                    glUniform1f(shader.u_emissive, 2.5f);
                    glLineWidth(1.5f);
                    for (size_t i = 0; i < locked_positions.size(); ++i)
                    {
                        Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                        draw_flat(cube_edges_mesh, translation(world), locked_colors[i], 1.0f);
                    }
                    glUniform1f(shader.u_emissive, 1.0f);
                    glEnable(GL_CULL_FACE);
                }

                // --- Ghost piece: faint faces + dim edges ---
                if (const auto ghost = game.ghost_piece())
                {
                    auto ghost_verts = build_piece_mesh(*ghost, game.well(), cell_size);
                    auto ghost_edges = build_piece_edges(*ghost, game.well(), cell_size);
                    update_mesh(active_mesh, ghost_verts);
                    update_mesh(active_edges, ghost_edges);
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    setup_block_shader();
                    draw_block(active_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 0.08f, 0.3f);
                    glUseProgram(shader.program);
                    glUniform1f(shader.u_emissive, 1.0f);
                    draw_flat(active_edges, identity(), Vec3{1.f, 1.f, 1.f}, 0.20f);
                    glDepthMask(GL_TRUE);
                    glEnable(GL_CULL_FACE);
                }

                // --- Active piece: glass faces + bright edges ---
                if (const auto& p = game.active_piece())
                {
                    Vec3 pivot{0.f, 0.f, 0.f};
                    for (const auto& b : p->blocks)
                    {
                        Vec3i rb = apply_rot(p->rot, b);
                        Vec3i c{p->pos.x + rb.x, p->pos.y + rb.y, p->pos.z + rb.z};
                        Vec3 world = game.well().cell_center(c, cell_size);
                        pivot.x += world.x;
                        pivot.y += world.y;
                        pivot.z += world.z;
                    }
                    float inv = 1.0f / static_cast<float>(p->blocks.size());
                    pivot.x *= inv; pivot.y *= inv; pivot.z *= inv;

                    auto piece_vertices = build_piece_mesh(*p, game.well(), cell_size);
                    auto edge_vertices  = build_piece_edges(*p, game.well(), cell_size);
                    update_mesh(active_mesh, piece_vertices);
                    update_mesh(active_edges, edge_vertices);

                    Mat4 model = translation(pivot);
                    if (spin_angle != 0.0f)
                    {
                        if      (spin.axis == Axis::X) model = multiply(model, rotation_x(spin_angle));
                        else if (spin.axis == Axis::Y) model = multiply(model, rotation_y(spin_angle));
                        else                           model = multiply(model, rotation_z(spin_angle));
                    }
                    model = multiply(model, translation(Vec3{-pivot.x, -pivot.y, -pivot.z}));
                    fall_offset = (p->pos_y - static_cast<float>(p->pos.y)) * cell_size;
                    if (fall_offset != 0.0f)
                        model = multiply(model, translation(Vec3{0.f, fall_offset, 0.f}));

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    if (wireframe_active)
                    {
                        glUseProgram(shader.program);
                        glUniform1f(shader.u_emissive, 3.0f);
                        draw_flat(active_edges, model, p->color, 1.0f);
                        glUniform1f(shader.u_emissive, 1.0f);
                    }
                    else
                    {
                        // Faces
                        setup_block_shader();
                        draw_block(active_mesh, model, p->color, 0.80f, 1.0f);
                        // Bright edges
                        glUseProgram(shader.program);
                        glUniform1f(shader.u_emissive, 3.0f);
                        draw_flat(active_edges, model, p->color, 1.0f);
                        glUniform1f(shader.u_emissive, 1.0f);
                    }
                    glDepthMask(GL_TRUE);
                    glEnable(GL_CULL_FACE);
                    glEnable(GL_DEPTH_TEST);
                }

                glBindVertexArray(0);
                glDisable(GL_SCISSOR_TEST);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // Separate isometric view under the controls column.
        ImGuiWindowFlags iso_flags = ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoBackground;
        if (ImGui::Begin("Iso View", nullptr, iso_flags))
        {
            ImVec2 content_min = ImGui::GetWindowContentRegionMin();
            ImVec2 content_max = ImGui::GetWindowContentRegionMax();
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 iso_pos{window_pos.x + content_min.x, window_pos.y + content_min.y};
            ImVec2 iso_size{content_max.x - content_min.x, content_max.y - content_min.y};
            if (iso_size.x > 0.f && iso_size.y > 0.f)
            {
                if (iso_size.x != iso_prev_size.x || iso_size.y != iso_prev_size.y)
                {
                    iso_needs_reframe = true;
                    iso_prev_size = iso_size;
                }
                int fb_width = 1;
                int fb_height = 1;
                glfwGetFramebufferSize(window, &fb_width, &fb_height);
                ImVec2 fb_scale = io.DisplayFramebufferScale;
                int vx_full = static_cast<int>(iso_pos.x * fb_scale.x);
                int vy_full = static_cast<int>((io.DisplaySize.y - iso_pos.y - iso_size.y) * fb_scale.y);
                int vw_full = static_cast<int>(iso_size.x * fb_scale.x);
                int vh_full = static_cast<int>(iso_size.y * fb_scale.y);
                int side = std::min(vw_full, vh_full);
                int vx = vx_full + (vw_full - side) / 2;
                int vy = vy_full + (vh_full - side) / 2;
                int vw = side;
                int vh = side;

                glViewport(vx, vy, vw, vh);
                glEnable(GL_SCISSOR_TEST);
                glScissor(vx, vy, vw, vh);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                draw_gradient_bg(grad_shader, palette.grad_bottom, palette.grad_top);

                float yaw_iso = to_radians(45.0f);
                float pitch_iso = to_radians(65.0f);
                if (iso_needs_reframe)
                {
                    auto ndc_max_for_distance = [&](float test_distance)
                    {
                        float cy_t = std::cos(yaw_iso);
                        float sy_t = std::sin(yaw_iso);
                        float cp_t = std::cos(pitch_iso);
                        float sp_t = std::sin(pitch_iso);
                        Vec3 eye_t{test_distance * sy_t * cp_t, test_distance * sp_t, test_distance * cy_t * cp_t};
                        float center_y_t = static_cast<float>(game.well().height()) * cell_size * 0.5f;
                        Mat4 view_t = look_at(eye_t, Vec3{0.f, center_y_t, 0.f}, Vec3{0.f, 1.f, 0.f});
                        Mat4 proj_t = perspective(55.0f, 1.0f, 0.1f, 200.0f);
                        Mat4 mvp_t = multiply(proj_t, view_t);
                        auto clip_ndc = [&](const Vec3& p)
                        {
                            float x = mvp_t.m[0] * p.x + mvp_t.m[4] * p.y + mvp_t.m[8] * p.z + mvp_t.m[12];
                            float y = mvp_t.m[1] * p.x + mvp_t.m[5] * p.y + mvp_t.m[9] * p.z + mvp_t.m[13];
                            float w = mvp_t.m[3] * p.x + mvp_t.m[7] * p.y + mvp_t.m[11] * p.z + mvp_t.m[15];
                            if (std::abs(w) < 1e-4f) w = 1e-4f;
                            return std::max(std::abs(x / w), std::abs(y / w));
                        };
                        float half_w = 0.5f * static_cast<float>(well_width) * cell_size;
                        float half_d = 0.5f * static_cast<float>(well_depth) * cell_size;
                        float top_y = static_cast<float>(well_height) * cell_size;
                        std::array<Vec3, 8> corners = {{
                            {-half_w, 0.f, -half_d},
                            { half_w, 0.f, -half_d},
                            { half_w, 0.f,  half_d},
                            {-half_w, 0.f,  half_d},
                            {-half_w, top_y, -half_d},
                            { half_w, top_y, -half_d},
                            { half_w, top_y,  half_d},
                            {-half_w, top_y,  half_d},
                        }};
                        float max_ndc = 0.f;
                        for (const auto& c : corners)
                        {
                            max_ndc = std::max(max_ndc, clip_ndc(c));
                        }
                        return max_ndc;
                    };
                    float target = 0.92f;
                    float lo = 1.0f;
                    float hi = 200.0f;
                    for (int i = 0; i < 24; ++i)
                    {
                        float mid = 0.5f * (lo + hi);
                        float ndc = ndc_max_for_distance(mid);
                        if (ndc > target)
                        {
                            lo = mid;
                        }
                        else
                        {
                            hi = mid;
                        }
                    }
                    dist_iso = hi;
                    iso_needs_reframe = false;
                }
                float cy = std::cos(yaw_iso);
                float sy = std::sin(yaw_iso);
                float cp = std::cos(pitch_iso);
                float sp = std::sin(pitch_iso);
                float center_y = static_cast<float>(game.well().height()) * cell_size * 0.5f;
                Vec3 eye{dist_iso * sy * cp, dist_iso * sp, dist_iso * cy * cp};
                Mat4 view = look_at(eye, Vec3{0.f, center_y, 0.f}, Vec3{0.f, 1.f, 0.f});
                Mat4 proj = perspective(55.0f, 1.0f, 0.1f, 120.0f);
                Mat4 mvp_world = multiply(proj, view);

                auto iso_draw_block = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha, float emissive) {
                    Mat4 mvp = multiply(mvp_world, model);
                    glUseProgram(block_shader.program);
                    glUniformMatrix4fv(block_shader.u_mvp,   1, GL_FALSE, mvp.m.data());
                    glUniformMatrix4fv(block_shader.u_model, 1, GL_FALSE, model.m.data());
                    glUniform3f(block_shader.u_tint,    tint.x, tint.y, tint.z);
                    glUniform1f(block_shader.u_alpha,   alpha);
                    glUniform1f(block_shader.u_emissive, emissive);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };
                auto iso_draw_flat = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha) {
                    Mat4 mvp = multiply(mvp_world, model);
                    Vec3 t{std::clamp(tint.x,0.f,1.f), std::clamp(tint.y,0.f,1.f), std::clamp(tint.z,0.f,1.f)};
                    glUseProgram(shader.program);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, t.x, t.y, t.z);
                    glUniform1f(shader.u_alpha, alpha);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };
                auto iso_setup_block = [&]() {
                    glUseProgram(block_shader.program);
                    glUniform1f(block_shader.u_time, app_time);
                    glUniform3f(block_shader.u_light0_pos,       lx0, ly0, lz0);
                    glUniform3f(block_shader.u_light0_color,     0.298f, 0.788f, 0.941f);
                    glUniform1f(block_shader.u_light0_intensity, li0);
                    glUniform3f(block_shader.u_light1_pos,       lx1, ly1, lz1);
                    glUniform3f(block_shader.u_light1_color,     0.969f, 0.145f, 0.522f);
                    glUniform1f(block_shader.u_light1_intensity, li1);
                    glUniform3f(block_shader.u_light2_pos,       0.0f, 1.0f, 3.0f);
                    glUniform3f(block_shader.u_light2_color,     0.443f, 0.035f, 0.718f);
                    glUniform1f(block_shader.u_light2_intensity, 2.0f);
                };

                glUseProgram(shader.program);
                glUniform1f(shader.u_emissive, 1.0f);
                iso_setup_block();
                iso_draw_block(bottom_mesh, translation(Vec3{0.f, -0.02f, 0.f}), Vec3{0.55f, 0.05f, 0.95f}, 0.90f, 2.0f);

                iso_draw_flat(floor_mesh,         identity(), palette.grid, 0.35f);
                iso_draw_flat(iso_walls_mesh,     identity(), palette.grid, 0.30f);
                iso_draw_flat(iso_wall_grid_mesh, identity(), palette.grid, 0.12f);

                if (const auto& p = game.active_piece())
                {
                    auto piece_vertices = build_piece_mesh(*p, game.well(), cell_size);
                    auto edge_vertices = build_piece_edges(*p, game.well(), cell_size);
                    update_mesh(active_mesh, piece_vertices);
                    update_mesh(active_edges, edge_vertices);

                    Vec3 iso_pivot{0.f, 0.f, 0.f};
                    for (const auto& b : p->blocks)
                    {
                        Vec3i rb = apply_rot(p->rot, b);
                        Vec3i c{p->pos.x + rb.x, p->pos.y + rb.y, p->pos.z + rb.z};
                        Vec3 world = game.well().cell_center(c, cell_size);
                        iso_pivot.x += world.x;
                        iso_pivot.y += world.y;
                        iso_pivot.z += world.z;
                    }
                    float iso_inv = 1.0f / static_cast<float>(p->blocks.size());
                    iso_pivot.x *= iso_inv;
                    iso_pivot.y *= iso_inv;
                    iso_pivot.z *= iso_inv;

                    Mat4 model = translation(iso_pivot);
                    if (spin_angle != 0.0f)
                    {
                        if (spin.axis == Axis::X) model = multiply(model, rotation_x(spin_angle));
                        else if (spin.axis == Axis::Y) model = multiply(model, rotation_y(spin_angle));
                        else model = multiply(model, rotation_z(spin_angle));
                    }
                    model = multiply(model, translation(Vec3{-iso_pivot.x, -iso_pivot.y, -iso_pivot.z}));
                    fall_offset = (p->pos_y - static_cast<float>(p->pos.y)) * cell_size;
                    if (fall_offset != 0.0f)
                    {
                        model = multiply(model, translation(Vec3{0.f, fall_offset, 0.f}));
                    }

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    if (wireframe_active)
                    {
                        glUseProgram(shader.program);
                        glUniform1f(shader.u_emissive, 3.0f);
                        iso_draw_flat(active_edges, model, p->color, 1.0f);
                        glUniform1f(shader.u_emissive, 1.0f);
                    }
                    else
                    {
                        iso_setup_block();
                        iso_draw_block(active_mesh, model, p->color, 0.80f, 1.0f);
                        glUseProgram(shader.program);
                        glUniform1f(shader.u_emissive, 3.0f);
                        iso_draw_flat(active_edges, model, p->color, 1.0f);
                        glUniform1f(shader.u_emissive, 1.0f);
                    }
                    glDepthMask(GL_TRUE);
                    glEnable(GL_CULL_FACE);
                    glEnable(GL_DEPTH_TEST);
                }
                // Locked cells (iso view).
                {
                    float flash = clear_flash_t > 0.f ? (1.0f + clear_flash_t / clear_flash_dur * 3.5f) : 1.0f;
                    const auto& locked_positions = game.locked_cells();
                    const auto& locked_colors = game.locked_colors();
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    iso_setup_block();
                    for (size_t i = 0; i < locked_positions.size(); ++i)
                    {
                        Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                        iso_draw_block(cube_mesh, translation(world), locked_colors[i], 0.80f, flash);
                    }
                    glDepthMask(GL_TRUE);
                    glUseProgram(shader.program);
                    glUniform1f(shader.u_emissive, 2.5f);
                    glLineWidth(1.2f);
                    for (size_t i = 0; i < locked_positions.size(); ++i)
                    {
                        Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                        iso_draw_flat(cube_edges_mesh, translation(world), locked_colors[i], 1.0f);
                    }
                    glUniform1f(shader.u_emissive, 1.0f);
                    glEnable(GL_CULL_FACE);
                }

                glBindVertexArray(0);
                glDisable(GL_SCISSOR_TEST);
            }
        }
        ImGui::End();

        ui::controls_panel(
            game, config,
            desired_width, desired_depth, desired_height, desired_start_level,
            well_width, well_depth, well_height, cell_size,
            wireframe_active, auto_play,
            floor_mesh, walls_mesh, wall_grid_mesh, bottom_mesh,
            iso_walls_mesh, iso_wall_grid_mesh,
            yaw, pitch, distance, needs_reframe, iso_needs_reframe, dist_iso, spin);

        if (game.state() == GameState::Playing || game.state() == GameState::Paused)
            ui::hud_overlay(game, viewport_rect.x0, viewport_rect.y0);

        if (game.state() == GameState::GameOver)
        {
            bool auto_play_reset = false;
            bool palm_reset = false;
            bool score_saved = true;
            ui::game_over_overlay(game, high_score, auto_play_reset, palm_reset, score_saved);
            if (!score_saved && high_score > 0)
            {
                save_high_score("highscore.txt", high_score);
                score_saved = true;
            }
            if (auto_play_reset) auto_play = {};
            if (palm_reset) spin = {};
        }

        if (game.state() == GameState::Paused)
            ui::paused_overlay();

        ImGui::Render();
        int fb_width = 1;
        int fb_height = 1;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    destroy_mesh(cube_mesh);
    destroy_mesh(cube_edges_mesh);
    destroy_mesh(floor_mesh);
    destroy_mesh(walls_mesh);
    destroy_mesh(wall_grid_mesh);
    destroy_mesh(bottom_mesh);
    destroy_mesh(iso_walls_mesh);
    destroy_mesh(iso_wall_grid_mesh);
    destroy_render_shader(shader);
    destroy_block_shader(block_shader);
    destroy_gradient_shader(grad_shader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
