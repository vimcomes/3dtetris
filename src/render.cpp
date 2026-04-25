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
        uniform float uEmissive;
        out vec4 FragColor;
        void main()
        {
            FragColor = vec4(vColor * uEmissive, uAlpha);
        }
    )";

    RenderShader shader{};
    shader.program = create_program(vertex_shader, fragment_shader);
    shader.u_mvp      = glGetUniformLocation(shader.program, "uMVP");
    shader.u_tint     = glGetUniformLocation(shader.program, "uTint");
    shader.u_alpha    = glGetUniformLocation(shader.program, "uAlpha");
    shader.u_emissive = glGetUniformLocation(shader.program, "uEmissive");
    return shader;
}

void destroy_render_shader(RenderShader& shader)
{
    if (shader.program != 0) glDeleteProgram(shader.program);
    shader = {};
}

GradientShader create_gradient_shader()
{
    const char* vs = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        out float vY;
        void main()
        {
            vY = aPos.y * 0.5 + 0.5;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* fs = R"(
        #version 330 core
        in float vY;
        uniform vec3 uBottom;
        uniform vec3 uTop;
        out vec4 FragColor;
        void main()
        {
            FragColor = vec4(mix(uBottom, uTop, vY), 1.0);
        }
    )";

    GradientShader gs{};
    gs.program  = create_program(vs, fs);
    gs.u_bottom = glGetUniformLocation(gs.program, "uBottom");
    gs.u_top    = glGetUniformLocation(gs.program, "uTop");

    float verts[] = {-1.f, -1.f,  1.f, -1.f,  1.f,  1.f,
                     -1.f, -1.f,  1.f,  1.f, -1.f,  1.f};
    glGenVertexArrays(1, &gs.vao);
    glGenBuffers(1, &gs.vbo);
    glBindVertexArray(gs.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gs.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    return gs;
}

void destroy_gradient_shader(GradientShader& shader)
{
    if (shader.program) glDeleteProgram(shader.program);
    if (shader.vao)     glDeleteVertexArrays(1, &shader.vao);
    if (shader.vbo)     glDeleteBuffers(1, &shader.vbo);
    shader = {};
}

void draw_gradient_bg(const GradientShader& gs, const Vec3& bottom, const Vec3& top)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glUseProgram(gs.program);
    glUniform3f(gs.u_bottom, bottom.x, bottom.y, bottom.z);
    glUniform3f(gs.u_top,    top.x,    top.y,    top.z);
    glBindVertexArray(gs.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

RenderPalette default_render_palette()
{
    return {};
}
