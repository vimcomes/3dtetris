#pragma once

#include <glad/glad.h>

#include "math.h"

struct RenderShader
{
    GLuint program = 0;
    GLint u_mvp = -1;
    GLint u_tint = -1;
    GLint u_alpha = -1;
    GLint u_emissive = -1;
};

struct GradientShader
{
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint u_bottom = -1;
    GLint u_top = -1;
};

struct RenderPalette
{
    Vec3 clear{0.03f, 0.02f, 0.08f};
    Vec3 grid{0.10f, 0.28f, 0.45f};
    Vec3 outline{0.55f, 0.65f, 0.75f};
    Vec3 grad_bottom{0.04f, 0.02f, 0.12f};
    Vec3 grad_top{0.00f, 0.04f, 0.10f};
};

RenderShader create_render_shader();
void destroy_render_shader(RenderShader& shader);

GradientShader create_gradient_shader();
void destroy_gradient_shader(GradientShader& shader);
void draw_gradient_bg(const GradientShader& gs, const Vec3& bottom, const Vec3& top);

RenderPalette default_render_palette();
inline void apply_clear_color(const RenderPalette& palette)
{
    glClearColor(palette.clear.x, palette.clear.y, palette.clear.z, 1.0f);
}
