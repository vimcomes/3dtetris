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

namespace
{
struct GlMesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
    GLenum mode = GL_TRIANGLES;
};

GlMesh make_empty_mesh(GLenum mode = GL_TRIANGLES)
{
    GlMesh mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    mesh.mode = mode;
    return mesh;
}

GlMesh make_mesh(const std::vector<float>& data, GLenum mode)
{
    GlMesh mesh = make_empty_mesh(mode);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    mesh.count = static_cast<GLsizei>(data.size() / 6);
    mesh.mode = mode;
    return mesh;
}

void update_mesh(GlMesh& mesh, const std::vector<float>& data)
{
    if (mesh.vao == 0 || mesh.vbo == 0)
    {
        mesh = make_empty_mesh(mesh.mode);
    }
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    mesh.count = static_cast<GLsizei>(data.size() / 6);
}

void destroy_mesh(GlMesh& m)
{
    if (m.vao != 0) glDeleteVertexArrays(1, &m.vao);
    if (m.vbo != 0) glDeleteBuffers(1, &m.vbo);
    m = {};
}

void append_face(std::vector<float>& out, float x0, float y0, float z0, float x1, float y1, float z1, int axis, bool positive)
{
    float c[3] = {1.f, 1.f, 1.f};
    auto push_quad = [&](float xA, float yA, float zA,
                         float xB, float yB, float zB,
                         float xC, float yC, float zC,
                         float xD, float yD, float zD)
    {
        out.insert(out.end(), {
            xA, yA, zA, c[0], c[1], c[2],
            xB, yB, zB, c[0], c[1], c[2],
            xC, yC, zC, c[0], c[1], c[2],
            xA, yA, zA, c[0], c[1], c[2],
            xC, yC, zC, c[0], c[1], c[2],
            xD, yD, zD, c[0], c[1], c[2],
        });
    };

    // axis: 0=x,1=y,2=z. Maintain CCW winding for outward normals.
    if (axis == 0)
    {
        float x = positive ? x1 : x0;
        if (positive)
        {
            // +X face
            push_quad(x, y0, z0, x, y1, z0, x, y1, z1, x, y0, z1);
        }
        else
        {
            // -X face
            push_quad(x, y0, z0, x, y0, z1, x, y1, z1, x, y1, z0);
        }
    }
    else if (axis == 1)
    {
        float y = positive ? y1 : y0;
        if (positive)
        {
            // +Y face (top)
            push_quad(x0, y, z0, x0, y, z1, x1, y, z1, x1, y, z0);
        }
        else
        {
            // -Y face (bottom)
            push_quad(x0, y, z0, x1, y, z0, x1, y, z1, x0, y, z1);
        }
    }
    else
    {
        float z = positive ? z1 : z0;
        if (positive)
        {
            // +Z face (front)
            push_quad(x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z);
        }
        else
        {
            // -Z face (back)
            push_quad(x0, y0, z, x0, y1, z, x1, y1, z, x1, y0, z);
        }
    }
}

std::vector<float> build_piece_mesh(const Piece& p, const Well& well, float cell_size)
{
    std::vector<float> vertices;
    // Build a quick adjacency check.
    std::vector<Vec3i> abs_blocks;
    abs_blocks.reserve(p.blocks.size());
    for (auto b : p.blocks)
    {
        abs_blocks.push_back(Vec3i{p.pos.x + b.x, p.pos.y + b.y, p.pos.z + b.z});
    }
    auto has_block = [&](const Vec3i& q)
    {
        for (const auto& b : abs_blocks)
        {
            if (b.x == q.x && b.y == q.y && b.z == q.z) return true;
        }
        return false;
    };

    float half = cell_size * 0.5f;
    for (const auto& c : abs_blocks)
    {
        Vec3 center = well.cell_center(c, cell_size);
        float x0 = center.x - half, x1 = center.x + half;
        float y0 = center.y - half, y1 = center.y + half;
        float z0 = center.z - half, z1 = center.z + half;

        if (!has_block(Vec3i{c.x + 1, c.y, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 0, true);
        if (!has_block(Vec3i{c.x - 1, c.y, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 0, false);
        if (!has_block(Vec3i{c.x, c.y + 1, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 1, true);
        if (!has_block(Vec3i{c.x, c.y - 1, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 1, false);
        if (!has_block(Vec3i{c.x, c.y, c.z + 1})) append_face(vertices, x0, y0, z0, x1, y1, z1, 2, true);
        if (!has_block(Vec3i{c.x, c.y, c.z - 1})) append_face(vertices, x0, y0, z0, x1, y1, z1, 2, false);
    }
    return vertices;
}

std::vector<float> build_piece_edges(const Piece& p, const Well& well, float cell_size)
{
    std::vector<Vec3i> abs_blocks;
    abs_blocks.reserve(p.blocks.size());
    for (auto b : p.blocks)
    {
        abs_blocks.push_back(Vec3i{p.pos.x + b.x, p.pos.y + b.y, p.pos.z + b.z});
    }
    auto has_block = [&](const Vec3i& q)
    {
        for (const auto& b : abs_blocks)
        {
            if (b.x == q.x && b.y == q.y && b.z == q.z) return true;
        }
        return false;
    };

    struct EdgeKey
    {
        std::array<float, 6> v{};
        bool operator<(const EdgeKey& other) const { return v < other.v; }
    };
    std::set<EdgeKey> edges;

    auto add_edge = [&](const Vec3& a, const Vec3& b)
    {
        EdgeKey k{};
        if (a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z <= b.z))))
        {
            k.v = {a.x, a.y, a.z, b.x, b.y, b.z};
        }
        else
        {
            k.v = {b.x, b.y, b.z, a.x, a.y, a.z};
        }
        edges.insert(k);
    };

    float half = cell_size * 0.5f;
    for (const auto& c : abs_blocks)
    {
        Vec3 center = well.cell_center(c, cell_size);
        float x0 = center.x - half, x1 = center.x + half;
        float y0 = center.y - half, y1 = center.y + half;
        float z0 = center.z - half, z1 = center.z + half;

        auto add_face_edges = [&](int axis, bool positive)
        {
            if (axis == 0)
            {
                float x = positive ? x1 : x0;
                Vec3 a{x, y0, z0}, b{x, y1, z0}, c1{x, y1, z1}, d{x, y0, z1};
                add_edge(a, b);
                add_edge(b, c1);
                add_edge(c1, d);
                add_edge(d, a);
            }
            else if (axis == 1)
            {
                float y = positive ? y1 : y0;
                Vec3 a{x0, y, z0}, b{x1, y, z0}, c1{x1, y, z1}, d{x0, y, z1};
                add_edge(a, b);
                add_edge(b, c1);
                add_edge(c1, d);
                add_edge(d, a);
            }
            else
            {
                float z = positive ? z1 : z0;
                Vec3 a{x0, y0, z}, b{x1, y0, z}, c1{x1, y1, z}, d{x0, y1, z};
                add_edge(a, b);
                add_edge(b, c1);
                add_edge(c1, d);
                add_edge(d, a);
            }
        };

        if (!has_block(Vec3i{c.x + 1, c.y, c.z})) add_face_edges(0, true);
        if (!has_block(Vec3i{c.x - 1, c.y, c.z})) add_face_edges(0, false);
        if (!has_block(Vec3i{c.x, c.y + 1, c.z})) add_face_edges(1, true);
        if (!has_block(Vec3i{c.x, c.y - 1, c.z})) add_face_edges(1, false);
        if (!has_block(Vec3i{c.x, c.y, c.z + 1})) add_face_edges(2, true);
        if (!has_block(Vec3i{c.x, c.y, c.z - 1})) add_face_edges(2, false);
    }

    std::vector<float> out;
    out.reserve(edges.size() * 12);
    for (const auto& e : edges)
    {
        out.push_back(e.v[0]); out.push_back(e.v[1]); out.push_back(e.v[2]); out.push_back(1.f); out.push_back(1.f); out.push_back(1.f);
        out.push_back(e.v[3]); out.push_back(e.v[4]); out.push_back(e.v[5]); out.push_back(1.f); out.push_back(1.f); out.push_back(1.f);
    }
    return out;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

static double g_scroll_delta = 0.0;
void scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
{
    g_scroll_delta += yoffset;
}

bool init_glfw()
{
    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    return true;
}

bool init_glad()
{
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
    {
        std::cerr << "Failed to load OpenGL functions via GLAD\n";
        return false;
    }
    return true;
}
} // namespace

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
    glfwSetScrollCallback(window, scroll_callback);

    if (!init_glad())
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    // Keep UI backgrounds transparent; rely on GL clear for black.
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

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
        int clamped = std::clamp(v, 3, 10);
        if (clamped < 6) return clamped; // allow narrow Blockout tube
        if (clamped % 2 != 0) clamped = (clamped < 10) ? (clamped + 1) : (clamped - 1);
        return clamped;
    };
    RenderShader shader = create_render_shader();
    RenderPalette palette = config.palette;

    auto cube_mesh = make_mesh(build_cube_vertices(), GL_TRIANGLES);

    int well_width = clamp_width_depth(config.well_width);
    int well_depth = clamp_width_depth(config.well_depth);
    int well_height = std::clamp(config.well_height, 12, 30);
    const float cell_size = 1.0f;

    auto floor_mesh = make_mesh(build_floor_grid_lines(well_width, well_depth, cell_size), GL_LINES);
    auto walls_mesh = make_mesh(build_well_outline_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
    auto iso_walls_mesh = make_mesh(build_well_outline_lines_culled(well_width, well_depth, well_height, cell_size), GL_LINES);
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

    Game game{well_width, well_depth, well_height, config.shapes, config.fall_interval};
    struct Spin
    {
        bool active = false;
        Axis axis = Axis::Y;
        int dir = 1;
        float t = 0.f;
        float duration = 0.15f;
    } spin;

    struct KeyState
    {
        bool rot_x_pos = false, rot_x_neg = false;
        bool rot_y_pos = false, rot_y_neg = false;
        bool rot_z_pos = false, rot_z_neg = false;
        bool space = false;
        bool f = false;
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

    struct AutoPlay
    {
        bool enabled = false;
        float step_timer = 0.f;
        int steps = 0;
        std::vector<AiPlanStep> plan;
        size_t plan_idx = 0;
    } auto_play;

    int desired_width = well_width;
    int desired_depth = well_depth;
    int desired_height = well_height;

    std::cout << "Controls:\n"
                 "  Mouse drag: orbit (RMB drag to tilt)\n"
                 "  Mouse wheel: zoom\n"
                 "  Arrows: move piece (X/Z)\n"
                 "  E/D: rotate around X\n"
                 "  W/S: rotate around Y\n"
                 "  Q/A: rotate around Z\n"
                 "  Space: hard drop\n"
                 "  F: toggle wireframe render for active piece\n"
                 "  ESC: quit\n";

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - prev_time);
        prev_time = now;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dock_flags);
        if (!dock_built)
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dock_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, io.DisplaySize);
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
        double scroll = g_scroll_delta;
        g_scroll_delta = 0.0;
        if (scroll != 0.0 && !io.WantCaptureMouse)
        {
            distance = std::clamp(distance - static_cast<float>(scroll) * zoom_speed, 4.0f, 40.0f);
        }

        // Piece input with edge detection for rotations/drop.
        auto is_down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };
        bool left_now = is_down(GLFW_KEY_LEFT);
        bool right_now = is_down(GLFW_KEY_RIGHT);
        bool up_now = is_down(GLFW_KEY_UP);
        bool down_now = is_down(GLFW_KEY_DOWN);
        bool q_now = is_down(GLFW_KEY_Q);
        bool a_now = is_down(GLFW_KEY_A);
        bool w_now = is_down(GLFW_KEY_W);
        bool s_now = is_down(GLFW_KEY_S);
        bool e_now = is_down(GLFW_KEY_E);
        bool d_now = is_down(GLFW_KEY_D);
        bool space_now = is_down(GLFW_KEY_SPACE);
        bool f_now = is_down(GLFW_KEY_F);

        // Rotations (Blockout-style)
        if (!io.WantCaptureKeyboard && viewport_hot && e_now && !prev_keys.rot_x_pos && game.rotate_active(Axis::X, 1))
        {
            spin = {true, Axis::X, 1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && d_now && !prev_keys.rot_x_neg && game.rotate_active(Axis::X, -1))
        {
            spin = {true, Axis::X, -1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && w_now && !prev_keys.rot_y_pos && game.rotate_active(Axis::Y, 1))
        {
            spin = {true, Axis::Y, 1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && s_now && !prev_keys.rot_y_neg && game.rotate_active(Axis::Y, -1))
        {
            spin = {true, Axis::Y, -1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && q_now && !prev_keys.rot_z_pos && game.rotate_active(Axis::Z, -1))
        {
            spin = {true, Axis::Z, -1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && a_now && !prev_keys.rot_z_neg && game.rotate_active(Axis::Z, 1))
        {
            spin = {true, Axis::Z, 1, 0.f, 0.15f};
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

        if (!io.WantCaptureKeyboard)
        {
            handle_repeat(left_now && viewport_hot, move_x_neg, [&] { game.move_active(-1, 0); });
            handle_repeat(right_now && viewport_hot, move_x_pos, [&] { game.move_active(1, 0); });
            handle_repeat(up_now && viewport_hot, move_z_neg, [&] { game.move_active(0, -1); });
            handle_repeat(down_now && viewport_hot, move_z_pos, [&] { game.move_active(0, 1); });
        }

        if (!io.WantCaptureKeyboard && viewport_hot && space_now && !prev_keys.space)
        {
            game.hard_drop();
            spin.active = false;
        }
        if (!io.WantCaptureKeyboard && viewport_hot && f_now && !prev_keys.f)
        {
            wireframe_active = !wireframe_active;
        }

        prev_keys = {e_now, d_now, w_now, s_now, q_now, a_now, space_now, f_now};

        game.update(dt);
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
        if (game.active_can_fall())
        {
            fall_offset = game.fall_progress() * cell_size;
        }

    if (auto_play.enabled && game.active_piece())
    {
        auto_play.step_timer += dt;
        if (auto_play.plan.empty())
        {
            GameAi ai;
            auto_play.plan = ai.compute_plan(game);
            auto_play.plan_idx = 0;
            auto_play.steps = 0;
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
                auto apply_tone = [&](const Vec3& c)
                {
                    return Vec3{
                        std::clamp(c.x, 0.0f, 1.0f),
                        std::clamp(c.y, 0.0f, 1.0f),
                        std::clamp(c.z, 0.0f, 1.0f)};
                };
                glClearColor(palette.clear.x, palette.clear.y, palette.clear.z, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

                glUseProgram(shader.program);

                auto draw_mesh_mvp = [&](const GlMesh& mesh, const Mat4& mvp, const Vec3& tint, float alpha)
                {
                    Vec3 t = apply_tone(tint);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, t.x, t.y, t.z);
                    glUniform1f(shader.u_alpha, alpha);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };
                auto draw_mesh = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha)
                {
                    Mat4 mvp = multiply(mvp_world, model);
                    draw_mesh_mvp(mesh, mvp, tint, alpha);
                };

                draw_mesh(bottom_mesh, identity(), Vec3{0.f, 0.f, 0.f}, 1.0f);
                draw_mesh(floor_mesh, identity(), palette.grid, 1.0f);
                draw_mesh(walls_mesh, identity(), palette.grid, 1.0f);

                // Active piece with simple spin animation (render on top).
                if (const auto& p = game.active_piece())
                {
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE); // allow both sides for translucent active piece
                    glDepthMask(GL_FALSE); // allow seeing locked blocks through translucent active piece.
                    Vec3 pivot{0.f, 0.f, 0.f};
                    for (const auto& b : p->blocks)
                    {
                        Vec3i c{p->pos.x + b.x, p->pos.y + b.y, p->pos.z + b.z};
                        Vec3 world = game.well().cell_center(c, cell_size);
                        pivot.x += world.x;
                        pivot.y += world.y;
                        pivot.z += world.z;
                    }
                    float inv = 1.0f / static_cast<float>(p->blocks.size());
                    pivot.x *= inv;
                    pivot.y *= inv;
                    pivot.z *= inv;

                    auto piece_vertices = build_piece_mesh(*p, game.well(), cell_size);
                    auto edge_vertices = build_piece_edges(*p, game.well(), cell_size);
                    update_mesh(active_mesh, piece_vertices);
                    update_mesh(active_edges, edge_vertices);

                    Mat4 model = translation(pivot);
                        if (spin_angle != 0.0f)
                        {
                            if (spin.axis == Axis::X) model = multiply(model, rotation_x(spin_angle));
                            else if (spin.axis == Axis::Y) model = multiply(model, rotation_y(spin_angle));
                            else model = multiply(model, rotation_z(spin_angle));
                        }
                    model = multiply(model, translation(Vec3{-pivot.x, -pivot.y, -pivot.z}));
                    if (fall_offset > 0.0f)
                    {
                        model = multiply(model, translation(Vec3{0.f, -fall_offset, 0.f}));
                    }

                    if (wireframe_active)
                    {
                        draw_mesh(active_edges, model, Vec3{1.0f, 1.0f, 1.0f}, 1.0f);
                    }
                    else
                    {
                        draw_mesh(active_mesh, model, p->color, 1.0f);
                    }
                    glDepthMask(GL_TRUE);
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_CULL_FACE);
                }

                // Ghost projection on landing spot (wireframe, low alpha).
                if (const auto ghost = game.ghost_piece())
                {
                    auto ghost_edges = build_piece_edges(*ghost, game.well(), cell_size);
                    update_mesh(active_edges, ghost_edges);
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    draw_mesh(active_edges, identity(), Vec3{1.0f, 1.0f, 1.0f}, 0.32f);
                    glDepthMask(GL_TRUE);
                    glEnable(GL_CULL_FACE);
                }

                // Locked cells.
                const auto& locked_positions = game.locked_cells();
                const auto& locked_colors = game.locked_colors();
                for (size_t i = 0; i < locked_positions.size(); ++i)
                {
                    Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                    Mat4 model = translation(world);
                    Mat4 mvp = multiply(mvp_world, model);
                    Vec3 t = apply_tone(locked_colors[i]);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, t.x, t.y, t.z);
                    glUniform1f(shader.u_alpha, 1.0f);
                    glBindVertexArray(cube_mesh.vao);
                    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.count);
                }
                // Outline pass to accentuate locked blocks.
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(1.3f);
                for (size_t i = 0; i < locked_positions.size(); ++i)
                {
                    Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                    Mat4 model = translation(world);
                    Mat4 mvp = multiply(mvp_world, model);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, palette.outline.x, palette.outline.y, palette.outline.z);
                    glUniform1f(shader.u_alpha, 1.0f);
                    glBindVertexArray(cube_mesh.vao);
                    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.count);
                }
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // Ghost projection on landing spot (wireframe, low alpha).
                if (const auto ghost = game.ghost_piece())
                {
                    auto ghost_edges = build_piece_edges(*ghost, game.well(), cell_size);
                    update_mesh(active_edges, ghost_edges);
                    glDisable(GL_CULL_FACE);
                    glDepthMask(GL_FALSE);
                    draw_mesh(active_edges, identity(), Vec3{1.0f, 1.0f, 1.0f}, 0.32f);
                    glDepthMask(GL_TRUE);
                    glEnable(GL_CULL_FACE);
                }

                // Active piece drawn last to avoid being covered by filled layers.
                if (const auto& p = game.active_piece())
                {
                    Vec3 pivot{0.f, 0.f, 0.f};
                    for (const auto& b : p->blocks)
                    {
                        Vec3i c{p->pos.x + b.x, p->pos.y + b.y, p->pos.z + b.z};
                        Vec3 world = game.well().cell_center(c, cell_size);
                        pivot.x += world.x;
                        pivot.y += world.y;
                        pivot.z += world.z;
                    }
                    float inv = 1.0f / static_cast<float>(p->blocks.size());
                    pivot.x *= inv;
                    pivot.y *= inv;
                    pivot.z *= inv;

                    auto piece_vertices = build_piece_mesh(*p, game.well(), cell_size);
                    auto edge_vertices = build_piece_edges(*p, game.well(), cell_size);
                    update_mesh(active_mesh, piece_vertices);
                    update_mesh(active_edges, edge_vertices);

                    Mat4 model = translation(pivot);
                    if (spin_angle != 0.0f)
                    {
                        if (spin.axis == Axis::X) model = multiply(model, rotation_x(spin_angle));
                        else if (spin.axis == Axis::Y) model = multiply(model, rotation_y(spin_angle));
                        else model = multiply(model, rotation_z(spin_angle));
                    }
                    model = multiply(model, translation(Vec3{-pivot.x, -pivot.y, -pivot.z}));
                    if (fall_offset > 0.0f)
                    {
                        model = multiply(model, translation(Vec3{0.f, -fall_offset, 0.f}));
                    }

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    if (wireframe_active)
                    {
                        draw_mesh(active_edges, model, Vec3{1.0f, 1.0f, 1.0f}, 1.0f);
                    }
                    else
                    {
                        draw_mesh(active_mesh, model, p->color, 1.0f);
                    }
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
                auto apply_tone = [&](const Vec3& c)
                {
                    return Vec3{
                        std::clamp(c.x, 0.0f, 1.0f),
                        std::clamp(c.y, 0.0f, 1.0f),
                        std::clamp(c.z, 0.0f, 1.0f)};
                };
                glClearColor(palette.clear.x, palette.clear.y, palette.clear.z, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
                    float target = 0.85f;
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

                glUseProgram(shader.program);
                auto draw_mesh_mvp = [&](const GlMesh& mesh, const Mat4& mvp, const Vec3& tint, float alpha)
                {
                    Vec3 t = apply_tone(tint);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, t.x, t.y, t.z);
                    glUniform1f(shader.u_alpha, alpha);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };
                auto draw_mesh = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha)
                {
                    Mat4 mvp = multiply(mvp_world, model);
                    draw_mesh_mvp(mesh, mvp, tint, alpha);
                };

                draw_mesh(bottom_mesh, identity(), Vec3{0.f, 0.f, 0.f}, 1.0f);
                draw_mesh(floor_mesh, identity(), palette.grid, 1.0f);
                draw_mesh(iso_walls_mesh, identity(), palette.grid, 1.0f);

                if (const auto& p = game.active_piece())
                {
                    auto piece_vertices = build_piece_mesh(*p, game.well(), cell_size);
                    auto edge_vertices = build_piece_edges(*p, game.well(), cell_size);
                    update_mesh(active_mesh, piece_vertices);
                    update_mesh(active_edges, edge_vertices);

                    Mat4 model = identity();
                    if (spin_angle != 0.0f)
                    {
                        if (spin.axis == Axis::X) model = multiply(model, rotation_x(spin_angle));
                        else if (spin.axis == Axis::Y) model = multiply(model, rotation_y(spin_angle));
                        else model = multiply(model, rotation_z(spin_angle));
                    }
                    if (fall_offset > 0.0f)
                    {
                        model = multiply(model, translation(Vec3{0.f, -fall_offset, 0.f}));
                    }

                    if (wireframe_active)
                    {
                        draw_mesh(active_edges, model, Vec3{1.0f, 1.0f, 1.0f}, 1.0f);
                    }
                    else
                    {
                        Vec3 iso_tint{std::min(p->color.x * 1.1f + 0.05f, 1.0f),
                                      std::min(p->color.y * 1.1f + 0.05f, 1.0f),
                                      std::min(p->color.z * 1.1f + 0.05f, 1.0f)};
                        draw_mesh(active_mesh, model, iso_tint, 1.0f);
                    }
                }
                // Locked cells.
                const auto& locked_positions = game.locked_cells();
                const auto& locked_colors = game.locked_colors();
                for (size_t i = 0; i < locked_positions.size(); ++i)
                {
                    Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                    Mat4 model = translation(world);
                    Mat4 mvp = multiply(mvp_world, model);
                    Vec3 t = apply_tone(locked_colors[i]);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, t.x, t.y, t.z);
                    glUniform1f(shader.u_alpha, 1.0f);
                    glBindVertexArray(cube_mesh.vao);
                    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.count);
                }
                // Edge overlay for locked cells to distinguish blocks.
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(1.2f);
                for (size_t i = 0; i < locked_positions.size(); ++i)
                {
                    Vec3 world = game.well().cell_center(locked_positions[i], cell_size);
                    Mat4 model = translation(world);
                    Mat4 mvp = multiply(mvp_world, model);
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, palette.outline.x, palette.outline.y, palette.outline.z);
                    glUniform1f(shader.u_alpha, 1.0f);
                    glBindVertexArray(cube_mesh.vao);
                    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.count);
                }
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                glBindVertexArray(0);
                glDisable(GL_SCISSOR_TEST);
            }
        }
        ImGui::End();

        if (ImGui::Begin("Controls"))
        {
            ImGui::TextUnformatted("Render");
            ImGui::Checkbox("Wireframe active piece (F)", &wireframe_active);
            ImGui::Separator();
            ImGui::TextUnformatted("Well size");
            ImGui::SliderInt("Width", &desired_width, 3, 10);
            desired_width = clamp_width_depth(desired_width);
            ImGui::SliderInt("Depth", &desired_depth, 3, 10);
            desired_depth = clamp_width_depth(desired_depth);
            ImGui::SliderInt("Height", &desired_height, 12, 30);
            bool size_changed = desired_width != well_width || desired_depth != well_depth || desired_height != well_height;
            if (ImGui::Button("Apply size") && size_changed)
            {
                well_width = desired_width;
                well_depth = desired_depth;
                well_height = desired_height;
                destroy_mesh(floor_mesh);
                destroy_mesh(walls_mesh);
                destroy_mesh(bottom_mesh);
                floor_mesh = make_mesh(build_floor_grid_lines(well_width, well_depth, cell_size), GL_LINES);
                walls_mesh = make_mesh(build_well_outline_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
                destroy_mesh(iso_walls_mesh);
                iso_walls_mesh = make_mesh(build_well_outline_lines_culled(well_width, well_depth, well_height, cell_size), GL_LINES);
                bottom_mesh = make_mesh(build_bottom_plane(well_width, well_depth, cell_size), GL_TRIANGLES);
                game = Game{well_width, well_depth, well_height, config.shapes, config.fall_interval};
                spin = {};
                auto_play = {};
                yaw = 0.0f;
                pitch = to_radians(89.0f);
                distance = std::max(12.0f, static_cast<float>(std::max(well_width, well_depth)) * 2.4f);
                needs_reframe = true;
                iso_needs_reframe = true;
                dist_iso = std::max(12.0f, static_cast<float>(std::max(well_width, well_depth)) * 3.0f);
            }
            ImGui::Separator();
            ImGui::TextUnformatted("Controls");
        ImGui::BulletText("Mouse drag: orbit (RMB drag to tilt)");
        ImGui::BulletText("Mouse wheel: zoom");
        ImGui::BulletText("Arrows: move");
        ImGui::BulletText("E/D: rotate X, W/S: rotate Y, Q/A: rotate Z");
        ImGui::BulletText("Space: hard drop");
        ImGui::BulletText("F: toggle wireframe");
        ImGui::BulletText("Esc: quit");
            ImGui::Separator();
            ImGui::Checkbox("Auto play", &auto_play.enabled);
            ImGui::Text("Auto steps: %d", auto_play.steps);
        }
        ImGui::End();

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
    destroy_mesh(floor_mesh);
    destroy_mesh(walls_mesh);
    destroy_mesh(bottom_mesh);
    destroy_render_shader(shader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
