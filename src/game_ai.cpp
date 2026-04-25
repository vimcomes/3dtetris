#include "game_ai.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
int idx3(int x, int y, int z, int w, int d)
{
    return y * d * w + z * w + x;
}

// Mirror of game.cpp rotation matrices (must stay in sync).
const int ROT_X_POS[3][3] = {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}};
const int ROT_Y_POS[3][3] = {{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}};
const int ROT_Z_POS[3][3] = {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}};

// Multiply two 3x3 integer rotation matrices; binarises result to {-1,0,1}.
// A and B must NOT alias out.
void mul_rot(const int A[3][3], const int B[3][3], int out[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
        {
            int v = 0;
            for (int k = 0; k < 3; ++k)
                v += A[i][k] * B[k][j];
            out[i][j] = (v > 0) ? 1 : (v < 0) ? -1 : 0;
        }
}

// In-place: mat = mat * rot (using a temp to avoid aliasing).
void apply_rot_step(int mat[3][3], const int rot[3][3])
{
    int tmp[3][3];
    mul_rot(mat, rot, tmp);
    std::memcpy(mat, tmp, sizeof(tmp));
}

bool mat_equal(const int A[3][3], const int B[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (A[i][j] != B[i][j]) return false;
    return true;
}

struct OrientEntry
{
    int mat[3][3];
    std::vector<AiPlanStep> steps; // steps from identity to reach this orientation
};

// Enumerate all 24 distinct orientations of the cube rotation group.
// Generates them as (kx rotations of X+) * (ky of Y+) * (kz of Z+),
// k in {0,1,2,3}. Deduplicates by comparing matrices.
// The associated steps reproduce each orientation via the same
// rotate_active(Axis, dir=+1) calls the game uses.
std::vector<OrientEntry> build_orientations()
{
    const int IDENTITY[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    std::vector<OrientEntry> result;
    result.reserve(24);

    for (int kx = 0; kx < 4; ++kx)
    {
        for (int ky = 0; ky < 4; ++ky)
        {
            for (int kz = 0; kz < 4; ++kz)
            {
                int mat[3][3];
                std::memcpy(mat, IDENTITY, sizeof(IDENTITY));
                for (int n = 0; n < kx; ++n) apply_rot_step(mat, ROT_X_POS);
                for (int n = 0; n < ky; ++n) apply_rot_step(mat, ROT_Y_POS);
                for (int n = 0; n < kz; ++n) apply_rot_step(mat, ROT_Z_POS);

                // Skip duplicates.
                bool dup = false;
                for (const auto& e : result)
                    if (mat_equal(e.mat, mat)) { dup = true; break; }
                if (dup) continue;

                OrientEntry entry;
                std::memcpy(entry.mat, mat, sizeof(mat));
                for (int n = 0; n < kx; ++n) entry.steps.push_back({AiPlanStep::Type::RotX, 1});
                for (int n = 0; n < ky; ++n) entry.steps.push_back({AiPlanStep::Type::RotY, 1});
                for (int n = 0; n < kz; ++n) entry.steps.push_back({AiPlanStep::Type::RotZ, 1});
                result.push_back(std::move(entry));
            }
        }
    }
    return result;
}

const std::vector<OrientEntry>& get_orientations()
{
    static std::vector<OrientEntry> cache = build_orientations();
    return cache;
}

} // namespace

bool GameAi::can_place(const Game& game, const Piece& p)
{
    return game.can_place_public(p);
}

Piece GameAi::drop_piece(const Game& game, Piece p)
{
    while (true)
    {
        Piece step = p;
        step.pos.y -= 1;
        if (can_place(game, step))
            p = step;
        else
            break;
    }
    return p;
}

std::vector<AiPlanStep> GameAi::compute_plan(const Game& game)
{
    if (!game.active_piece()) return {};
    const Piece& active = *game.active_piece();

    const auto& orientations = get_orientations();

    int w = game.well().width();
    int d = game.well().depth();
    int h = game.well().height();

    // Pre-build well occupancy once per plan (shared across all candidates).
    std::vector<uint8_t> well_occ(w * d * h, 0);
    for (int yy = 0; yy < h; ++yy)
        for (int zz = 0; zz < d; ++zz)
            for (int xx = 0; xx < w; ++xx)
                if (!game.well().is_free(Vec3i{xx, yy, zz}))
                    well_occ[idx3(xx, yy, zz, w, d)] = 1;

    struct Candidate
    {
        int orient_idx;
        Vec3i pos;
        float score;
    };
    std::vector<Candidate> candidates;

    for (int oi = 0; oi < static_cast<int>(orientations.size()); ++oi)
    {
        const auto& orient = orientations[oi];

        // Compute axis-aligned bounds of the rotated shape.
        int minx = 0, maxx = 0, minz = 0, maxz = 0, miny = 0, maxy = 0;
        bool first = true;
        for (auto b : active.blocks)
        {
            Vec3i rb = apply_rot(orient.mat, b);
            if (first)
            {
                minx = maxx = rb.x;
                minz = maxz = rb.z;
                miny = maxy = rb.y;
                first = false;
            }
            else
            {
                minx = std::min(minx, rb.x); maxx = std::max(maxx, rb.x);
                minz = std::min(minz, rb.z); maxz = std::max(maxz, rb.z);
                miny = std::min(miny, rb.y); maxy = std::max(maxy, rb.y);
            }
        }
        if (first) continue; // empty piece

        for (int x = 0; x < w; ++x)
        {
            for (int z = 0; z < d; ++z)
            {
                // Build a candidate piece with this orientation and position.
                Piece p = active;
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        p.rot[i][j] = orient.mat[i][j];
                // p.blocks stays as active.blocks; can_place uses apply_rot(p.rot, b).
                p.pos = Vec3i{x - minx, h - 1 - maxy, z - minz};

                p = drop_piece(game, p);
                if (!can_place(game, p)) continue;

                // Build occupancy with the piece dropped in.
                std::vector<uint8_t> occ = well_occ;
                for (auto b : p.blocks)
                {
                    Vec3i rb = apply_rot(p.rot, b);
                    int xx = p.pos.x + rb.x;
                    int yy = p.pos.y + rb.y;
                    int zz = p.pos.z + rb.z;
                    if (xx >= 0 && xx < w && yy >= 0 && yy < h && zz >= 0 && zz < d)
                        occ[idx3(xx, yy, zz, w, d)] = 1;
                }

                // Heuristic evaluation.
                int full_planes = 0;
                for (int yy = 0; yy < h; ++yy)
                {
                    bool full = true;
                    for (int zz = 0; zz < d && full; ++zz)
                        for (int xx = 0; xx < w; ++xx)
                            if (!occ[idx3(xx, yy, zz, w, d)]) { full = false; break; }
                    if (full) ++full_planes;
                }

                int max_height = 0;
                int holes = 0;
                int agg_height = 0;
                int bumpiness = 0;
                std::vector<int> heights(w * d, 0);
                for (int zz = 0; zz < d; ++zz)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        int col_h = 0;
                        bool filled_seen = false;
                        for (int yy = h - 1; yy >= 0; --yy)
                        {
                            if (occ[idx3(xx, yy, zz, w, d)])
                            {
                                filled_seen = true;
                                col_h = std::max(col_h, yy + 1);
                                max_height = std::max(max_height, yy + 1);
                            }
                            else if (filled_seen)
                            {
                                ++holes;
                            }
                        }
                        heights[zz * w + xx] = col_h;
                        agg_height += col_h;
                    }
                }
                for (int zz = 0; zz < d; ++zz)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        int h0 = heights[zz * w + xx];
                        if (xx + 1 < w) bumpiness += std::abs(h0 - heights[zz * w + (xx + 1)]);
                        if (zz + 1 < d) bumpiness += std::abs(h0 - heights[(zz + 1) * w + xx]);
                    }
                }

                float score = max_height * 5.0f + agg_height * 0.5f
                            + holes * 50.0f + bumpiness * 3.0f
                            - full_planes * 800.0f;
                candidates.push_back({oi, p.pos, score});
            }
        }
    }

    if (candidates.empty()) return {};

    auto best = *std::min_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score < b.score; });

    std::vector<AiPlanStep> plan;

    // Rotation steps that reproduce the chosen orientation from identity.
    for (const auto& step : orientations[best.orient_idx].steps)
        plan.push_back(step);

    // Translation steps.
    if (const auto& cur = game.active_piece())
    {
        int dx = best.pos.x - cur->pos.x;
        int dz = best.pos.z - cur->pos.z;
        int sx = (dx > 0) ? 1 : -1;
        for (int i = 0; i < std::abs(dx); ++i) plan.push_back({AiPlanStep::Type::MoveX, sx});
        int sz = (dz > 0) ? 1 : -1;
        for (int i = 0; i < std::abs(dz); ++i) plan.push_back({AiPlanStep::Type::MoveZ, sz});
    }
    plan.push_back({AiPlanStep::Type::Drop, 0});
    return plan;
}
