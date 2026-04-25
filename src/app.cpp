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
    // Write per-face normal (block shader reads location 1 as aNormal).
    float nx = 0.f, ny = 0.f, nz = 0.f;
    if      (axis == 0) nx = positive ? 1.f : -1.f;
    else if (axis == 1) ny = positive ? 1.f : -1.f;
    else                nz = positive ? 1.f : -1.f;

    auto push_quad = [&](float xA, float yA, float zA,
                         float xB, float yB, float zB,
                         float xC, float yC, float zC,
                         float xD, float yD, float zD)
    {
        out.insert(out.end(), {
            xA, yA, zA, nx, ny, nz,
            xB, yB, zB, nx, ny, nz,
            xC, yC, zC, nx, ny, nz,
            xA, yA, zA, nx, ny, nz,
            xC, yC, zC, nx, ny, nz,
            xD, yD, zD, nx, ny, nz,
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

std::vector<float> build_piece_mesh(const Piece& p, const Well& well, float cell_size, float block_scale = 0.88f)
{
    std::vector<float> vertices;
    std::vector<Vec3i> abs_blocks;
    abs_blocks.reserve(p.blocks.size());
    for (auto b : p.blocks)
    {
        Vec3i rb = apply_rot(p.rot, b);
        abs_blocks.push_back(Vec3i{p.pos.x + rb.x, p.pos.y + rb.y, p.pos.z + rb.z});
    }
    auto has_block = [&](const Vec3i& q)
    {
        for (const auto& b : abs_blocks)
        {
            if (b.x == q.x && b.y == q.y && b.z == q.z) return true;
        }
        return false;
    };

    float half = cell_size * block_scale * 0.5f;
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

std::vector<float> build_piece_edges(const Piece& p, const Well& well, float cell_size, float block_scale = 0.90f)
{
    std::vector<Vec3i> abs_blocks;
    abs_blocks.reserve(p.blocks.size());
    for (auto b : p.blocks)
    {
        Vec3i rb = apply_rot(p.rot, b);
        abs_blocks.push_back(Vec3i{p.pos.x + rb.x, p.pos.y + rb.y, p.pos.z + rb.z});
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

    float half = cell_size * block_scale * 0.5f;
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
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_scroll_delta += yoffset;
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
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
    style.WindowRounding   = 8.0f;
    style.FrameRounding    = 4.0f;
    style.GrabRounding     = 4.0f;
    style.PopupRounding    = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding      = 4.0f;
    style.WindowPadding    = ImVec2(12, 12);
    style.FramePadding     = ImVec2(8, 5);
    style.ItemSpacing      = ImVec2(10, 7);
    style.WindowBorderSize = 0.0f;
    // Transparent BG for GL viewport and iso windows; Controls gets its own push.
    style.Colors[ImGuiCol_WindowBg]        = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_ChildBg]         = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_DockingEmptyBg]  = ImVec4(0.f, 0.f, 0.f, 0.f);
    // Neon cyan accent
    style.Colors[ImGuiCol_Text]            = ImVec4(0.88f, 0.95f, 1.00f, 1.0f);
    style.Colors[ImGuiCol_Button]          = ImVec4(0.08f, 0.28f, 0.42f, 0.90f);
    style.Colors[ImGuiCol_ButtonHovered]   = ImVec4(0.12f, 0.52f, 0.72f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]    = ImVec4(0.05f, 0.70f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]      = ImVec4(0.00f, 0.75f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]= ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]       = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]         = ImVec4(0.06f, 0.10f, 0.18f, 0.90f);
    style.Colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.08f, 0.18f, 0.32f, 0.90f);
    style.Colors[ImGuiCol_Header]          = ImVec4(0.00f, 0.50f, 0.72f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered]   = ImVec4(0.00f, 0.68f, 0.88f, 0.60f);
    style.Colors[ImGuiCol_Separator]       = ImVec4(0.00f, 0.45f, 0.65f, 0.50f);
    style.Colors[ImGuiCol_TitleBg]         = ImVec4(0.04f, 0.04f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]   = ImVec4(0.04f, 0.10f, 0.18f, 1.00f);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    // Replace ImGui-installed scroll callback with our chaining version.
    glfwSetScrollCallback(window, scroll_callback);

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

    struct AutoPlay
    {
        bool enabled = false;
        float step_timer = 0.f;
        int steps = 0;
        std::vector<AiPlanStep> plan;
        size_t plan_idx = 0;
        float plan_piece_top_y = -1.f; // pos_y when plan was built (pieces only fall, so higher = new piece)
    } auto_play;

    int prev_total_cleared = 0;
    float clear_flash_t = 0.f;
    static constexpr float clear_flash_dur = 0.35f;

    int desired_width = well_width;
    int desired_depth = well_depth;
    int desired_height = well_height;
    int desired_start_level = config.start_level;

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
        if (scroll != 0.0)
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
        bool p_now = is_down(GLFW_KEY_P);

        // Rotations (Blockout-style) — only when game is active
        if (!io.WantCaptureKeyboard && viewport_hot && e_now && !prev_keys.rot_x_pos && game.state() == GameState::Playing && game.rotate_active(Axis::X, 1))
        {
            spin = {true, Axis::X, 1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && d_now && !prev_keys.rot_x_neg && game.state() == GameState::Playing && game.rotate_active(Axis::X, -1))
        {
            spin = {true, Axis::X, -1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && w_now && !prev_keys.rot_y_pos && game.state() == GameState::Playing && game.rotate_active(Axis::Z, 1))
        {
            spin = {true, Axis::Z, 1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && s_now && !prev_keys.rot_y_neg && game.state() == GameState::Playing && game.rotate_active(Axis::Z, -1))
        {
            spin = {true, Axis::Z, -1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && q_now && !prev_keys.rot_z_pos && game.state() == GameState::Playing && game.rotate_active(Axis::Y, -1))
        {
            spin = {true, Axis::Y, -1, 0.f, 1.0f / 3.0f};
        }
        if (!io.WantCaptureKeyboard && viewport_hot && a_now && !prev_keys.rot_z_neg && game.state() == GameState::Playing && game.rotate_active(Axis::Y, 1))
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

        prev_keys = {e_now, d_now, w_now, s_now, q_now, a_now, space_now, f_now, p_now};

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
            auto_play.plan = ai.compute_plan(game);
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

                // Block shader setup (called before any block draw).
                auto setup_block_shader = [&]() {
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

                auto draw_block = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha, float emissive) {
                    Mat4 mvp = multiply(mvp_world, model);
                    glUniformMatrix4fv(block_shader.u_mvp,   1, GL_FALSE, mvp.m.data());
                    glUniformMatrix4fv(block_shader.u_model, 1, GL_FALSE, model.m.data());
                    glUniform3f(block_shader.u_tint,   tint.x, tint.y, tint.z);
                    glUniform1f(block_shader.u_alpha,   alpha);
                    glUniform1f(block_shader.u_emissive, emissive);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };

                auto draw_flat = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha) {
                    Mat4 mvp = multiply(mvp_world, model);
                    Vec3 t{std::clamp(tint.x, 0.f,1.f), std::clamp(tint.y, 0.f,1.f), std::clamp(tint.z, 0.f,1.f)};
                    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(shader.u_tint, t.x, t.y, t.z);
                    glUniform1f(shader.u_alpha, alpha);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };

                // --- Lines: floor grid + well walls (flat shader) ---
                glUseProgram(shader.program);
                glUniform1f(shader.u_emissive, 1.0f);
                draw_flat(floor_mesh,     identity(), palette.grid, 0.35f);
                draw_flat(walls_mesh,     identity(), palette.grid, 0.30f);
                draw_flat(wall_grid_mesh, identity(), palette.grid, 0.12f);

                // --- Floor plane (block shader, dark purple emissive) ---
                setup_block_shader();
                draw_block(bottom_mesh, identity(), Vec3{0.133f, 0.039f, 0.251f}, 0.92f, 1.0f);

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
                iso_draw_flat(floor_mesh,         identity(), palette.grid, 0.35f);
                iso_draw_flat(iso_walls_mesh,     identity(), palette.grid, 0.30f);
                iso_draw_flat(iso_wall_grid_mesh, identity(), palette.grid, 0.12f);

                iso_setup_block();
                iso_draw_block(bottom_mesh, identity(), Vec3{0.133f, 0.039f, 0.251f}, 0.92f, 1.0f);

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

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.04f, 0.10f, 0.92f));
        if (ImGui::Begin("Controls"))
        {
            // Next piece preview (2D isometric projection).
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
            desired_width = clamp_width_depth(desired_width);
            ImGui::SliderInt("Depth", &desired_depth, 3, 7);
            desired_depth = clamp_width_depth(desired_depth);
            ImGui::SliderInt("Height", &desired_height, 5, 20);
            bool size_changed = desired_width != well_width || desired_depth != well_depth || desired_height != well_height;
            bool level_changed = desired_start_level != config.start_level;
            ImGui::SliderInt("Start level", &desired_start_level, 0, 9);
            if (ImGui::Button("Apply size") && size_changed)
            {
                well_width = desired_width;
                well_depth = desired_depth;
                well_height = desired_height;
                config.start_level = desired_start_level;
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
                config.start_level = desired_start_level;
                game = Game{well_width, well_depth, well_height, config.shapes, config.fall_interval, config.start_level};
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
        ImGui::BulletText("F: toggle wireframe");
        ImGui::BulletText("P: pause / resume");
        ImGui::BulletText("Esc: quit");
            ImGui::Separator();
            ImGui::Checkbox("Auto play", &auto_play.enabled);
            ImGui::Text("Auto steps: %d", auto_play.steps);
        }
        ImGui::End();
        ImGui::PopStyleColor();

        // HUD overlay on main viewport (B6).
        if (game.state() == GameState::Playing || game.state() == GameState::Paused)
        {
            ImGui::SetNextWindowPos(ImVec2(viewport_rect.x0 + 14.f, viewport_rect.y0 + 14.f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGuiWindowFlags hud_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
                                         ImGuiWindowFlags_NoFocusOnAppearing;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.85f, 1.0f, 0.90f));
            if (ImGui::Begin("##hud", nullptr, hud_flags))
            {
                ImGui::Text("SCORE  %d", game.score());
                ImGui::Text("LEVEL  %d", game.level());
                ImGui::Text("LINES  %d", game.total_cleared());
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }

        // Game Over overlay
        if (game.state() == GameState::GameOver)
        {
            ImGuiIO& overlay_io = ImGui::GetIO();
            ImVec2 center{overlay_io.DisplaySize.x * 0.5f * 0.68f,
                          overlay_io.DisplaySize.y * 0.5f};
            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.88f);
            ImGuiWindowFlags ov_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
            if (ImGui::Begin("##gameover", nullptr, ov_flags))
            {
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("GAME OVER").x) * 0.5f);
                ImGui::TextColored(ImVec4(1.f, 0.25f, 0.25f, 1.f), "GAME OVER");
                ImGui::Separator();
                ImGui::Text("Score:  %d", game.score());
                ImGui::Text("Level:  %d", game.level());
                ImGui::Text("Lines:  %d", game.total_cleared());
                ImGui::Spacing();
                float btn_w = 120.f;
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btn_w) * 0.5f);
                if (ImGui::Button("Restart", ImVec2(btn_w, 0)))
                {
                    game.restart();
                    auto_play = {};
                    spin = {};
                }
            }
            ImGui::End();
        }

        // Paused overlay
        if (game.state() == GameState::Paused)
        {
            ImGuiIO& overlay_io = ImGui::GetIO();
            ImVec2 center{overlay_io.DisplaySize.x * 0.5f * 0.68f,
                          overlay_io.DisplaySize.y * 0.5f};
            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(240, 100), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.80f);
            ImGuiWindowFlags ov_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
            if (ImGui::Begin("##paused", nullptr, ov_flags))
            {
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("PAUSED").x) * 0.5f);
                ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f), "PAUSED");
                ImGui::Spacing();
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Press P to resume").x) * 0.5f);
                ImGui::TextDisabled("Press P to resume");
            }
            ImGui::End();
        }

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
