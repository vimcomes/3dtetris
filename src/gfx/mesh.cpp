#include "gfx/mesh.h"

#include <array>
#include <set>

namespace
{

GlMesh make_empty_mesh_impl(GLenum mode)
{
    GlMesh mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    mesh.mode = mode;
    return mesh;
}

} // namespace

GlMesh make_empty_mesh(GLenum mode)
{
    return make_empty_mesh_impl(mode);
}

GlMesh make_mesh(const std::vector<float>& data, GLenum mode)
{
    GlMesh mesh = make_empty_mesh(mode);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    mesh.count = static_cast<GLsizei>(data.size() / 6);
    mesh.mode = mode;
    return mesh;
}

void update_mesh(GlMesh& mesh, const std::vector<float>& data)
{
    if (mesh.vao == 0 || mesh.vbo == 0)
    {
        mesh = make_empty_mesh(mesh.mode);
    }
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    mesh.count = static_cast<GLsizei>(data.size() / 6);
}

void destroy_mesh(GlMesh& m)
{
    if (m.vao != 0) glDeleteVertexArrays(1, &m.vao);
    if (m.vbo != 0) glDeleteBuffers(1, &m.vbo);
    m = {};
}

void append_face(std::vector<float>& out, float x0, float y0, float z0, float x1, float y1, float z1, int axis, bool positive)
{
    float nx = 0.f, ny = 0.f, nz = 0.f;
    if      (axis == 0) nx = positive ? 1.f : -1.f;
    else if (axis == 1) ny = positive ? 1.f : -1.f;
    else                nz = positive ? 1.f : -1.f;

    auto push_quad = [&](float xA, float yA, float zA,
                         float xB, float yB, float zB,
                         float xC, float yC, float zC,
                         float xD, float yD, float zD)
    {
        out.insert(out.end(), {
            xA, yA, zA, nx, ny, nz,
            xB, yB, zB, nx, ny, nz,
            xC, yC, zC, nx, ny, nz,
            xA, yA, zA, nx, ny, nz,
            xC, yC, zC, nx, ny, nz,
            xD, yD, zD, nx, ny, nz,
        });
    };

    if (axis == 0)
    {
        float x = positive ? x1 : x0;
        if (positive)
            push_quad(x, y0, z0, x, y1, z0, x, y1, z1, x, y0, z1);
        else
            push_quad(x, y0, z0, x, y0, z1, x, y1, z1, x, y1, z0);
    }
    else if (axis == 1)
    {
        float y = positive ? y1 : y0;
        if (positive)
            push_quad(x0, y, z0, x0, y, z1, x1, y, z1, x1, y, z0);
        else
            push_quad(x0, y, z0, x1, y, z0, x1, y, z1, x0, y, z1);
    }
    else
    {
        float z = positive ? z1 : z0;
        if (positive)
            push_quad(x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z);
        else
            push_quad(x0, y0, z, x0, y1, z, x1, y1, z, x1, y0, z);
    }
}

std::vector<float> build_piece_mesh(const Piece& p, const Well& well, float cell_size, float block_scale)
{
    std::vector<float> vertices;
    std::vector<Vec3i> abs_blocks;
    abs_blocks.reserve(p.blocks.size());
    for (auto b : p.blocks)
    {
        Vec3i rb = apply_rot(p.rot, b);
        abs_blocks.push_back(Vec3i{p.pos.x + rb.x, p.pos.y + rb.y, p.pos.z + rb.z});
    }
    auto has_block = [&](const Vec3i& q)
    {
        for (const auto& b : abs_blocks)
            if (b.x == q.x && b.y == q.y && b.z == q.z) return true;
        return false;
    };

    float half = cell_size * block_scale * 0.5f;
    for (const auto& c : abs_blocks)
    {
        Vec3 center = well.cell_center(c, cell_size);
        float x0 = center.x - half, x1 = center.x + half;
        float y0 = center.y - half, y1 = center.y + half;
        float z0 = center.z - half, z1 = center.z + half;

        if (!has_block(Vec3i{c.x + 1, c.y, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 0, true);
        if (!has_block(Vec3i{c.x - 1, c.y, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 0, false);
        if (!has_block(Vec3i{c.x, c.y + 1, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 1, true);
        if (!has_block(Vec3i{c.x, c.y - 1, c.z})) append_face(vertices, x0, y0, z0, x1, y1, z1, 1, false);
        if (!has_block(Vec3i{c.x, c.y, c.z + 1})) append_face(vertices, x0, y0, z0, x1, y1, z1, 2, true);
        if (!has_block(Vec3i{c.x, c.y, c.z - 1})) append_face(vertices, x0, y0, z0, x1, y1, z1, 2, false);
    }
    return vertices;
}

std::vector<float> build_piece_edges(const Piece& p, const Well& well, float cell_size, float block_scale)
{
    std::vector<Vec3i> abs_blocks;
    abs_blocks.reserve(p.blocks.size());
    for (auto b : p.blocks)
    {
        Vec3i rb = apply_rot(p.rot, b);
        abs_blocks.push_back(Vec3i{p.pos.x + rb.x, p.pos.y + rb.y, p.pos.z + rb.z});
    }
    auto has_block = [&](const Vec3i& q)
    {
        for (const auto& b : abs_blocks)
            if (b.x == q.x && b.y == q.y && b.z == q.z) return true;
        return false;
    };

    struct EdgeKey
    {
        std::array<float, 6> v{};
        bool operator<(const EdgeKey& other) const { return v < other.v; }
    };
    std::set<EdgeKey> edges;

    auto add_edge = [&](const Vec3& a, const Vec3& b)
    {
        EdgeKey k{};
        if (a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z <= b.z))))
            k.v = {a.x, a.y, a.z, b.x, b.y, b.z};
        else
            k.v = {b.x, b.y, b.z, a.x, a.y, a.z};
        edges.insert(k);
    };

    float half = cell_size * block_scale * 0.5f;
    for (const auto& c : abs_blocks)
    {
        Vec3 center = well.cell_center(c, cell_size);
        float x0 = center.x - half, x1 = center.x + half;
        float y0 = center.y - half, y1 = center.y + half;
        float z0 = center.z - half, z1 = center.z + half;

        auto add_face_edges = [&](int axis, bool positive)
        {
            if (axis == 0)
            {
                float x = positive ? x1 : x0;
                Vec3 a{x, y0, z0}, b{x, y1, z0}, c1{x, y1, z1}, d{x, y0, z1};
                add_edge(a, b); add_edge(b, c1); add_edge(c1, d); add_edge(d, a);
            }
            else if (axis == 1)
            {
                float y = positive ? y1 : y0;
                Vec3 a{x0, y, z0}, b{x1, y, z0}, c1{x1, y, z1}, d{x0, y, z1};
                add_edge(a, b); add_edge(b, c1); add_edge(c1, d); add_edge(d, a);
            }
            else
            {
                float z = positive ? z1 : z0;
                Vec3 a{x0, y0, z}, b{x1, y0, z}, c1{x1, y1, z}, d{x0, y1, z};
                add_edge(a, b); add_edge(b, c1); add_edge(c1, d); add_edge(d, a);
            }
        };

        if (!has_block(Vec3i{c.x + 1, c.y, c.z})) add_face_edges(0, true);
        if (!has_block(Vec3i{c.x - 1, c.y, c.z})) add_face_edges(0, false);
        if (!has_block(Vec3i{c.x, c.y + 1, c.z})) add_face_edges(1, true);
        if (!has_block(Vec3i{c.x, c.y - 1, c.z})) add_face_edges(1, false);
        if (!has_block(Vec3i{c.x, c.y, c.z + 1})) add_face_edges(2, true);
        if (!has_block(Vec3i{c.x, c.y, c.z - 1})) add_face_edges(2, false);
    }

    std::vector<float> out;
    out.reserve(edges.size() * 12);
    for (const auto& e : edges)
    {
        out.push_back(e.v[0]); out.push_back(e.v[1]); out.push_back(e.v[2]); out.push_back(1.f); out.push_back(1.f); out.push_back(1.f);
        out.push_back(e.v[3]); out.push_back(e.v[4]); out.push_back(e.v[5]); out.push_back(1.f); out.push_back(1.f); out.push_back(1.f);
    }
    return out;
}
