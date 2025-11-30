#pragma once

#include <glad/glad.h>

#include "math.h"

struct RenderShader
{
    GLuint program = 0;
    GLint u_mvp = -1;
    GLint u_tint = -1;
    GLint u_alpha = -1;
};

struct RenderPalette
{
    Vec3 clear{0.f, 0.f, 0.f};
    Vec3 grid{0.f, 1.f, 0.f};
    Vec3 outline{0.92f, 0.95f, 0.98f};
};

RenderShader create_render_shader();
void destroy_render_shader(RenderShader& shader);
RenderPalette default_render_palette();
inline void apply_clear_color(const RenderPalette& palette)
{
    glClearColor(palette.clear.x, palette.clear.y, palette.clear.z, 1.0f);
}
