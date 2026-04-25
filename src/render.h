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

// Phong shader with 3 orbiting point lights + emissive breathing for glass blocks.
struct BlockShader
{
    GLuint program = 0;
    GLint u_mvp = -1;
    GLint u_model = -1;
    GLint u_tint = -1;
    GLint u_alpha = -1;
    GLint u_emissive = -1;
    GLint u_time = -1;
    GLint u_light0_pos = -1, u_light0_color = -1, u_light0_intensity = -1;
    GLint u_light1_pos = -1, u_light1_color = -1, u_light1_intensity = -1;
    GLint u_light2_pos = -1, u_light2_color = -1, u_light2_intensity = -1;
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
    Vec3 clear{0.024f, 0.012f, 0.059f};   // #06030F near-black navy
    Vec3 grid{1.0f, 1.0f, 1.0f};          // white (alpha controlled at draw time)
    Vec3 outline{0.92f, 0.95f, 0.98f};
    Vec3 grad_bottom{0.06f, 0.02f, 0.14f};
    Vec3 grad_top{0.02f, 0.01f, 0.06f};
};

RenderShader create_render_shader();
void destroy_render_shader(RenderShader& shader);

BlockShader create_block_shader();
void destroy_block_shader(BlockShader& shader);

GradientShader create_gradient_shader();
void destroy_gradient_shader(GradientShader& shader);
void draw_gradient_bg(const GradientShader& gs, const Vec3& bottom, const Vec3& top);

RenderPalette default_render_palette();
inline void apply_clear_color(const RenderPalette& palette)
{
    glClearColor(palette.clear.x, palette.clear.y, palette.clear.z, 1.0f);
}
