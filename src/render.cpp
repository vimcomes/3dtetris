#include "render.h"

#include "shader.h"

RenderShader create_render_shader()
{
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
            FragColor = vec4(vColor, uAlpha);
        }
    )";

    RenderShader shader{};
    shader.program = create_program(vertex_shader, fragment_shader);
    shader.u_mvp = glGetUniformLocation(shader.program, "uMVP");
    shader.u_tint = glGetUniformLocation(shader.program, "uTint");
    shader.u_alpha = glGetUniformLocation(shader.program, "uAlpha");
    return shader;
}

void destroy_render_shader(RenderShader& shader)
{
    if (shader.program != 0)
    {
        glDeleteProgram(shader.program);
    }
    shader = {};
}

RenderPalette default_render_palette()
{
    return {};
}
