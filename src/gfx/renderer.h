#pragma once

#include "math.h"
#include "render.h"
#include "gfx/mesh.h"

namespace gfx {

void setup_block_shader(const BlockShader& bs, float time,
                        float lx0, float ly0, float lz0, float li0,
                        float lx1, float ly1, float lz1, float li1);

void draw_block(const GlMesh& mesh, const BlockShader& bs, const Mat4& mvp, const Mat4& model,
                const Vec3& tint, float alpha, float emissive);

void draw_flat(const GlMesh& mesh, const RenderShader& shader, const Mat4& mvp,
               const Vec3& tint, float alpha);

} // namespace gfx
