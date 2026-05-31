#pragma once

#include <vector>

#include <glad/glad.h>

#include "game.h"

struct GlMesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
    GLenum mode = GL_TRIANGLES;
};

GlMesh make_empty_mesh(GLenum mode = GL_TRIANGLES);
GlMesh make_mesh(const std::vector<float>& data, GLenum mode);
void update_mesh(GlMesh& mesh, const std::vector<float>& data);
void destroy_mesh(GlMesh& m);

void append_face(std::vector<float>& out, float x0, float y0, float z0, float x1, float y1, float z1, int axis, bool positive);

std::vector<float> build_piece_mesh(const Piece& p, const Well& well, float cell_size, float block_scale = 0.88f);
std::vector<float> build_piece_edges(const Piece& p, const Well& well, float cell_size, float block_scale = 0.90f);
