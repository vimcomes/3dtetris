#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

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
#include "shader.h"

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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const char* vertex_shader = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        uniform mat4 uMVP;
        uniform vec3 uTint;
        out vec3 vColor;
        void main()
        {
            vColor = aColor * uTint;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* fragment_shader = R"(
        #version 330 core
        in vec3 vColor;
        uniform float uAlpha;
        out vec4 FragColor;
        void main()
        {
            vec3 srgb = pow(vColor, vec3(1.0/2.2));
            FragColor = vec4(srgb, uAlpha);
        }
    )";

    GLuint program = create_program(vertex_shader, fragment_shader);
    GLint u_mvp_loc = glGetUniformLocation(program, "uMVP");
    GLint u_tint_loc = glGetUniformLocation(program, "uTint");
    GLint u_alpha_loc = glGetUniformLocation(program, "uAlpha");

    auto cube_mesh = make_mesh(build_cube_vertices(), GL_TRIANGLES);

    constexpr int well_width = 10;
    constexpr int well_depth = 10;
    constexpr int well_height = 20;
    constexpr float cell_size = 1.0f;

    auto floor_mesh = make_mesh(build_floor_grid_lines(well_width, well_depth, cell_size), GL_LINES);
    auto walls_mesh = make_mesh(build_well_outline_lines(well_width, well_depth, well_height, cell_size), GL_LINES);
    auto bottom_mesh = make_mesh(build_bottom_plane(well_width, well_depth, cell_size), GL_TRIANGLES);
    GlMesh active_mesh = make_empty_mesh();
    GlMesh active_edges = make_empty_mesh(GL_LINES);

    float yaw = 0.0f;
    float pitch = to_radians(89.0f);
    float distance = 24.0f;
    double prev_time = glfwGetTime();

    Game game{well_width, well_depth, well_height};
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
        bool left = false, right = false, up = false, down = false;
        bool z = false, x = false;
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
    float brightness = 1.5f;
    float color_boost = 2.0f;
    bool rotating = false;
    struct RmbState
    {
        bool down = false;
        bool dragged = false;
        double last_x = 0.0;
        double last_y = 0.0;
    } rmb;
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;
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

    std::cout << "Controls:\n"
                 "  Mouse drag: rotate view around vertical axis\n"
                 "  W/S: tilt camera\n"
                 "  Q/E: zoom in/out\n"
                 "  Arrow Left/Right: rotate piece around Y\n"
                 "  Arrow Up/Down: rotate piece around X\n"
                 "  Z/X: rotate piece around Z\n"
                 "  I/K: move piece forward/backward (Z)\n"
                 "  J/L: move piece left/right (X)\n"
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

        // Mouse orbit around vertical axis (only inside viewport area).
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(window, &mx, &my);
        bool in_viewport = mx >= viewport_rect.x0 && mx <= viewport_rect.x1 && my >= viewport_rect.y0 && my <= viewport_rect.y1;
        if (in_viewport && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            if (!rotating)
            {
                rotating = true;
                last_mouse_x = mx;
                last_mouse_y = my;
            }
            double dx = mx - last_mouse_x;
            yaw += static_cast<float>(dx) * 0.005f;
            last_mouse_x = mx;
            last_mouse_y = my;
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

        // Camera anchored top-down with slight tilt; allow mild zoom and pitch adjust.
        const float zoom_speed = 6.0f;
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) distance = std::max(6.0f, distance - zoom_speed * dt);
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) distance = std::min(40.0f, distance + zoom_speed * dt);
        const float tilt_speed = 1.5f;
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) pitch = std::min(to_radians(88.0f), pitch + tilt_speed * dt);
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) pitch = std::max(to_radians(40.0f), pitch - tilt_speed * dt);

        // Piece input with edge detection for rotations/drop.
        auto is_down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };
        bool left_now = is_down(GLFW_KEY_LEFT);
        bool right_now = is_down(GLFW_KEY_RIGHT);
        bool up_now = is_down(GLFW_KEY_UP);
        bool down_now = is_down(GLFW_KEY_DOWN);
        bool z_now = is_down(GLFW_KEY_Z);
        bool x_now = is_down(GLFW_KEY_X);
        bool space_now = is_down(GLFW_KEY_SPACE);
        bool f_now = is_down(GLFW_KEY_F);
        bool a_now = is_down(GLFW_KEY_A);
        bool d_now = is_down(GLFW_KEY_D);

        if (!io.WantCaptureKeyboard && in_viewport && left_now && !prev_keys.left && game.rotate_active(Axis::Y, -1))
        {
            spin = {true, Axis::Y, -1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && in_viewport && right_now && !prev_keys.right && game.rotate_active(Axis::Y, 1))
        {
            spin = {true, Axis::Y, 1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && in_viewport && up_now && !prev_keys.up && game.rotate_active(Axis::X, -1))
        {
            spin = {true, Axis::X, -1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && in_viewport && down_now && !prev_keys.down && game.rotate_active(Axis::X, 1))
        {
            spin = {true, Axis::X, 1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && in_viewport && z_now && !prev_keys.z && game.rotate_active(Axis::Z, -1))
        {
            spin = {true, Axis::Z, -1, 0.f, 0.15f};
        }
        if (!io.WantCaptureKeyboard && in_viewport && x_now && !prev_keys.x && game.rotate_active(Axis::Z, 1))
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
            handle_repeat((is_down(GLFW_KEY_J) || a_now) && in_viewport, move_x_neg, [&] { game.move_active(-1, 0); });
            handle_repeat((is_down(GLFW_KEY_L) || d_now) && in_viewport, move_x_pos, [&] { game.move_active(1, 0); });
            handle_repeat(is_down(GLFW_KEY_I) && in_viewport, move_z_neg, [&] { game.move_active(0, -1); });
            handle_repeat(is_down(GLFW_KEY_K) && in_viewport, move_z_pos, [&] { game.move_active(0, 1); });
        }

    if (!io.WantCaptureKeyboard && space_now && !prev_keys.space)
    {
        game.hard_drop();
        spin.active = false;
    }
        if (!io.WantCaptureKeyboard && in_viewport && f_now && !prev_keys.f)
        {
            wireframe_active = !wireframe_active;
        }

    prev_keys = {left_now, right_now, up_now, down_now, z_now, x_now, space_now, f_now};

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
                    float gain = std::max(brightness * color_boost, 0.1f);
                    Vec3 v{c.x * gain, c.y * gain, c.z * gain};
                    float maxc = std::max(v.x, std::max(v.y, v.z));
                    if (maxc > 1.0f)
                    {
                        float inv = 1.0f / maxc;
                        v.x *= inv;
                        v.y *= inv;
                        v.z *= inv;
                    }
                    return v;
                };
                Vec3 clear{0.12f, 0.14f, 0.18f};
                glClearColor(clear.x, clear.y, clear.z, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                float cy = std::cos(yaw);
                float sy = std::sin(yaw);
                float cp = std::cos(pitch);
                float sp = std::sin(pitch);
                Vec3 eye{distance * sy * cp, distance * sp, distance * cy * cp};
                Mat4 view = look_at(eye, Vec3{0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f});
                float aspect = viewport_size.x / viewport_size.y;

                Mat4 proj = perspective(60.0f, aspect, 0.1f, 100.0f);
                Mat4 mvp_world = multiply(proj, view);

                glUseProgram(program);

                auto draw_mesh_mvp = [&](const GlMesh& mesh, const Mat4& mvp, const Vec3& tint, float alpha)
                {
                    Vec3 t = apply_tone(tint);
                    glUniformMatrix4fv(u_mvp_loc, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(u_tint_loc, t.x, t.y, t.z);
                    glUniform1f(u_alpha_loc, alpha);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };
                auto draw_mesh = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha)
                {
                    Mat4 mvp = multiply(mvp_world, model);
                    draw_mesh_mvp(mesh, mvp, tint, alpha);
                };

                draw_mesh(bottom_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 1.0f);
                draw_mesh(floor_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 1.0f);
                draw_mesh(walls_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 1.0f);

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
                        draw_mesh(active_edges, model, p->color, 0.95f);
                    }
                    else
                    {
                        draw_mesh(active_mesh, model, p->color, 0.6f);
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
                    draw_mesh(active_edges, identity(), Vec3{0.7f, 0.75f, 0.8f}, 0.3f);
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
                    glUniformMatrix4fv(u_mvp_loc, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(u_tint_loc, t.x, t.y, t.z);
                    glUniform1f(u_alpha_loc, 1.0f);
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
                    glUniformMatrix4fv(u_mvp_loc, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(u_tint_loc, 0.92f, 0.95f, 0.98f);
                    glUniform1f(u_alpha_loc, 1.0f);
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
                    draw_mesh(active_edges, identity(), Vec3{0.7f, 0.75f, 0.8f}, 0.3f);
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
                        draw_mesh(active_edges, model, p->color, 0.95f);
                    }
                    else
                    {
                        draw_mesh(active_mesh, model, p->color, 0.6f);
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
                    float gain = std::max(brightness * color_boost, 0.1f);
                    Vec3 v{c.x * gain, c.y * gain, c.z * gain};
                    float maxc = std::max(v.x, std::max(v.y, v.z));
                    if (maxc > 1.0f)
                    {
                        float inv = 1.0f / maxc;
                        v.x *= inv;
                        v.y *= inv;
                        v.z *= inv;
                    }
                    return v;
                };
                Vec3 clear{0.12f, 0.14f, 0.18f};
                glClearColor(clear.x, clear.y, clear.z, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                float yaw_iso = to_radians(45.0f);
                float pitch_iso = to_radians(65.0f);
                float dist_iso = 36.0f;
                float cy = std::cos(yaw_iso);
                float sy = std::sin(yaw_iso);
                float cp = std::cos(pitch_iso);
                float sp = std::sin(pitch_iso);
                float center_y = static_cast<float>(game.well().height()) * cell_size * 0.5f;
                Vec3 eye{dist_iso * sy * cp, dist_iso * sp, dist_iso * cy * cp};
                Mat4 view = look_at(eye, Vec3{0.f, center_y, 0.f}, Vec3{0.f, 1.f, 0.f});
                Mat4 proj = perspective(55.0f, 1.0f, 0.1f, 120.0f);
                Mat4 mvp_world = multiply(proj, view);

                glUseProgram(program);
                auto draw_mesh_mvp = [&](const GlMesh& mesh, const Mat4& mvp, const Vec3& tint, float alpha)
                {
                    Vec3 t = apply_tone(tint);
                    glUniformMatrix4fv(u_mvp_loc, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(u_tint_loc, t.x, t.y, t.z);
                    glUniform1f(u_alpha_loc, alpha);
                    glBindVertexArray(mesh.vao);
                    glDrawArrays(mesh.mode, 0, mesh.count);
                };
                auto draw_mesh = [&](const GlMesh& mesh, const Mat4& model, const Vec3& tint, float alpha)
                {
                    Mat4 mvp = multiply(mvp_world, model);
                    draw_mesh_mvp(mesh, mvp, tint, alpha);
                };

                draw_mesh(bottom_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 1.0f);
                draw_mesh(floor_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 1.0f);
                draw_mesh(walls_mesh, identity(), Vec3{1.f, 1.f, 1.f}, 1.0f);

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
                        draw_mesh(active_edges, model, p->color, 1.0f);
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
                    glUniformMatrix4fv(u_mvp_loc, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(u_tint_loc, t.x, t.y, t.z);
                    glUniform1f(u_alpha_loc, 1.0f);
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
                    glUniformMatrix4fv(u_mvp_loc, 1, GL_FALSE, mvp.m.data());
                    glUniform3f(u_tint_loc, 0.92f, 0.95f, 0.98f);
                    glUniform1f(u_alpha_loc, 1.0f);
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
            ImGui::SliderFloat("Brightness", &brightness, 0.8f, 3.5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderFloat("Color boost", &color_boost, 1.0f, 4.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Separator();
            ImGui::TextUnformatted("Controls");
            ImGui::BulletText("Mouse drag: orbit");
            ImGui::BulletText("W/S: tilt");
            ImGui::BulletText("Q/E: zoom");
            ImGui::BulletText("Arrows/Z/X: rotate");
            ImGui::BulletText("I/K/J/L: move");
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
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
