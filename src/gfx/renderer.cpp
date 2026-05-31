#include "gfx/renderer.h"

#include <glad/glad.h>

namespace gfx {

void setup_block_shader(const BlockShader& bs, float time,
                        float lx0, float ly0, float lz0, float li0,
                        float lx1, float ly1, float lz1, float li1)
{
    glUseProgram(bs.program);
    glUniform1f(bs.u_time, time);
    glUniform3f(bs.u_light0_pos,       lx0, ly0, lz0);
    glUniform3f(bs.u_light0_color,     0.298f, 0.788f, 0.941f);
    glUniform1f(bs.u_light0_intensity, li0);
    glUniform3f(bs.u_light1_pos,       lx1, ly1, lz1);
    glUniform3f(bs.u_light1_color,     0.969f, 0.145f, 0.522f);
    glUniform1f(bs.u_light1_intensity, li1);
    glUniform3f(bs.u_light2_pos,       0.0f, 1.0f, 3.0f);
    glUniform3f(bs.u_light2_color,     0.443f, 0.035f, 0.718f);
    glUniform1f(bs.u_light2_intensity, 2.0f);
}

void draw_block(const GlMesh& mesh, const BlockShader& bs, const Mat4& mvp, const Mat4& model,
                const Vec3& tint, float alpha, float emissive)
{
    glUniformMatrix4fv(bs.u_mvp,   1, GL_FALSE, mvp.m.data());
    glUniformMatrix4fv(bs.u_model, 1, GL_FALSE, model.m.data());
    glUniform3f(bs.u_tint,   tint.x, tint.y, tint.z);
    glUniform1f(bs.u_alpha,   alpha);
    glUniform1f(bs.u_emissive, emissive);
    glBindVertexArray(mesh.vao);
    glDrawArrays(mesh.mode, 0, mesh.count);
}

void draw_flat(const GlMesh& mesh, const RenderShader& shader, const Mat4& mvp,
               const Vec3& tint, float alpha)
{
    Vec3 t{std::clamp(tint.x, 0.f,1.f), std::clamp(tint.y, 0.f,1.f), std::clamp(tint.z, 0.f,1.f)};
    glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, mvp.m.data());
    glUniform3f(shader.u_tint, t.x, t.y, t.z);
    glUniform1f(shader.u_alpha, alpha);
    glBindVertexArray(mesh.vao);
    glDrawArrays(mesh.mode, 0, mesh.count);
}

} // namespace gfx
