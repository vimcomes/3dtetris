#include "input.h"

#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

namespace
{

double g_scroll_delta = 0.0;

} // namespace

int key_name_to_glfw(const std::string& name)
{
    if (name == "Left") return GLFW_KEY_LEFT;
    if (name == "Right") return GLFW_KEY_RIGHT;
    if (name == "Up") return GLFW_KEY_UP;
    if (name == "Down") return GLFW_KEY_DOWN;
    if (name == "Space") return GLFW_KEY_SPACE;
    if (name == "Escape") return GLFW_KEY_ESCAPE;
    if (name == "Enter") return GLFW_KEY_ENTER;
    if (name == "Tab") return GLFW_KEY_TAB;
    if (name == "Backspace") return GLFW_KEY_BACKSPACE;
    if (name.size() == 1)
    {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
    }
    return GLFW_KEY_UNKNOWN;
}

bool is_key_down(GLFWwindow* window, const std::string& name)
{
    int key = key_name_to_glfw(name);
    if (key == GLFW_KEY_UNKNOWN) return false;
    return glfwGetKey(window, key) == GLFW_PRESS;
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

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_scroll_delta += yoffset;
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}

double consume_scroll_delta()
{
    double v = g_scroll_delta;
    g_scroll_delta = 0.0;
    return v;
}
