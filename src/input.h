#pragma once

#include <string>

struct GLFWwindow;

bool init_glfw();
bool init_glad();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
double consume_scroll_delta();

int key_name_to_glfw(const std::string& name);
bool is_key_down(GLFWwindow* window, const std::string& name);
