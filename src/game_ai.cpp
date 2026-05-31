#include "game_ai.h"
#include "rotation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
#include <vector>

namespace
{

int idx3(int x, int y, int z, int w, int d)
{
    return y * d * w + z * w + x;
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
    std::vector<AiPlanStep> steps;
};

std::vector<OrientEntry> build_orientations()
{
    const int IDENTITY[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    std::vector<OrientEntry> result;
    result.reserve(24);
    for (int kx = 0; kx < 4; ++kx)
        for (int ky = 0; ky < 4; ++ky)
            for (int kz = 0; kz < 4; ++kz)
            {
                int mat[3][3];
                std::memcpy(mat, IDENTITY, sizeof(IDENTITY));
                for (int n = 0; n < kx; ++n) apply_rot_step(mat, ROT_X_POS);
                for (int n = 0; n < ky; ++n) apply_rot_step(mat, ROT_Y_POS);
                for (int n = 0; n < kz; ++n) apply_rot_step(mat, ROT_Z_POS);
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
    return result;
}

const std::vector<OrientEntry>& get_orientations()
{
    static std::vector<OrientEntry> cache = build_orientations();
    return cache;
}

// ---- occ-array helpers ----

bool occ_can_place(const std::vector<uint8_t>& occ, int w, int d, int h,
                   const std::vector<Vec3i>& blocks, const int rot[3][3], const Vec3i& pos)
{
    for (auto b : blocks)
    {
        Vec3i rb = apply_rot(rot, b);
        int x = pos.x + rb.x, y = pos.y + rb.y, z = pos.z + rb.z;
        if (x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d) return false;
        if (occ[idx3(x, y, z, w, d)]) return false;
    }
    return true;
}

void place_piece_occ(std::vector<uint8_t>& occ, int w, int d, int h,
                     const std::vector<Vec3i>& blocks, const int rot[3][3], const Vec3i& pos)
{
    for (auto b : blocks)
    {
        Vec3i rb = apply_rot(rot, b);
        int x = pos.x + rb.x, y = pos.y + rb.y, z = pos.z + rb.z;
        if (x >= 0 && x < w && y >= 0 && y < h && z >= 0 && z < d)
            occ[idx3(x, y, z, w, d)] = 1;
    }
}

// Clears full XZ planes and shifts layers down; returns count cleared.
int clear_planes_occ(std::vector<uint8_t>& occ, int w, int d, int h)
{
    int cleared = 0;
    for (int y = 0; y < h; )
    {
        bool full = true;
        for (int z = 0; z < d && full; ++z)
            for (int x = 0; x < w; ++x)
                if (!occ[idx3(x, y, z, w, d)]) { full = false; break; }
        if (!full) { ++y; continue; }
        for (int yy = y; yy < h - 1; ++yy)
            for (int z = 0; z < d; ++z)
                for (int x = 0; x < w; ++x)
                    occ[idx3(x, yy, z, w, d)] = occ[idx3(x, yy + 1, z, w, d)];
        for (int z = 0; z < d; ++z)
            for (int x = 0; x < w; ++x)
                occ[idx3(x, h - 1, z, w, d)] = 0;
        ++cleared;
    }
    return cleared;
}

// Heuristic on a post-clear occ (lower = better).
float eval_occ(const std::vector<uint8_t>& occ, int w, int d, int h, const AiConfig& cfg)
{
    int max_height = 0, holes = 0, agg_height = 0, bumpiness = 0;
    std::vector<int> heights(w * d, 0);
    for (int z = 0; z < d; ++z)
    {
        for (int x = 0; x < w; ++x)
        {
            int col_h = 0;
            bool seen = false;
            for (int y = h - 1; y >= 0; --y)
            {
                if (occ[idx3(x, y, z, w, d)])
                {
                    seen = true;
                    col_h = std::max(col_h, y + 1);
                    max_height = std::max(max_height, y + 1);
                }
                else if (seen)
                {
                    ++holes;
                }
            }
            heights[z * w + x] = col_h;
            agg_height += col_h;
        }
    }
    for (int z = 0; z < d; ++z)
        for (int x = 0; x < w; ++x)
        {
            int h0 = heights[z * w + x];
            if (x + 1 < w) bumpiness += std::abs(h0 - heights[z * w + (x + 1)]);
            if (z + 1 < d) bumpiness += std::abs(h0 - heights[(z + 1) * w + x]);
        }
    return max_height * cfg.weight_max_height + agg_height * cfg.weight_agg_height
         + holes * cfg.weight_holes + bumpiness * cfg.weight_bumpiness;
}

struct BlockBounds { int minx, maxx, miny, maxy, minz, maxz; };

BlockBounds compute_bounds(const std::vector<Vec3i>& blocks, const int rot[3][3])
{
    BlockBounds b{0, 0, 0, 0, 0, 0};
    bool first = true;
    for (auto blk : blocks)
    {
        Vec3i rb = apply_rot(rot, blk);
        if (first)
        {
            b.minx = b.maxx = rb.x;
            b.miny = b.maxy = rb.y;
            b.minz = b.maxz = rb.z;
            first = false;
        }
        else
        {
            b.minx = std::min(b.minx, rb.x); b.maxx = std::max(b.maxx, rb.x);
            b.miny = std::min(b.miny, rb.y); b.maxy = std::max(b.maxy, rb.y);
            b.minz = std::min(b.minz, rb.z); b.maxz = std::max(b.maxz, rb.z);
        }
    }
    return b;
}

// BFS: all positions reachable from the top-centre spawn via fall + lateral
// slide (no upward movement).  A position is a "landing" if the piece fits
// there but cannot fall one more cell.  This correctly discovers positions
// that require sliding under an overhang before or during the fall.
std::vector<Vec3i> find_landings_bfs(const std::vector<uint8_t>& occ, int w, int d, int h,
                                      const std::vector<Vec3i>& blocks, const int rot[3][3])
{
    const auto bd = compute_bounds(blocks, rot);
    Vec3i spawn{
        std::clamp(w / 2 - (bd.minx + bd.maxx) / 2, -bd.minx, w - 1 - bd.maxx),
        h - 1 - bd.maxy,
        std::clamp(d / 2 - (bd.minz + bd.maxz) / 2, -bd.minz, d - 1 - bd.maxz)
    };
    if (!occ_can_place(occ, w, d, h, blocks, rot, spawn)) return {};

    std::vector<bool> visited(w * d * h, false);
    std::queue<Vec3i> q;
    std::vector<Vec3i> landings;

    auto enc = [&](const Vec3i& p) { return idx3(p.x, p.y, p.z, w, d); };
    visited[enc(spawn)] = true;
    q.push(spawn);

    static constexpr int DX[] = {1, -1, 0, 0};
    static constexpr int DZ[] = {0, 0, 1, -1};

    while (!q.empty())
    {
        Vec3i cur = q.front(); q.pop();

        // Try to fall one cell.
        Vec3i below{cur.x, cur.y - 1, cur.z};
        bool can_fall = (below.y >= 0) && occ_can_place(occ, w, d, h, blocks, rot, below);
        if (can_fall)
        {
            int e = enc(below);
            if (!visited[e]) { visited[e] = true; q.push(below); }
        }
        else
        {
            landings.push_back(cur);
        }

        // Try lateral moves at current height.
        for (int i = 0; i < 4; ++i)
        {
            Vec3i next{cur.x + DX[i], cur.y, cur.z + DZ[i]};
            if (occ_can_place(occ, w, d, h, blocks, rot, next))
            {
                int e = enc(next);
                if (!visited[e]) { visited[e] = true; q.push(next); }
            }
        }
    }
    return landings;
}

// Faster drop-only landing finder used in the lookahead inner loop.
// Multiple (x,z) starting positions may produce the same landing; duplicates
// are removed before returning.
std::vector<Vec3i> find_landings_drop(const std::vector<uint8_t>& occ, int w, int d, int h,
                                       const std::vector<Vec3i>& blocks, const int rot[3][3])
{
    const auto bd = compute_bounds(blocks, rot);
    std::vector<Vec3i> landings;
    for (int x = 0; x < w; ++x)
    {
        for (int z = 0; z < d; ++z)
        {
            Vec3i pos{x - bd.minx, h - 1 - bd.maxy, z - bd.minz};
            if (!occ_can_place(occ, w, d, h, blocks, rot, pos)) continue;
            while (true)
            {
                Vec3i next{pos.x, pos.y - 1, pos.z};
                if (next.y < 0 || !occ_can_place(occ, w, d, h, blocks, rot, next)) break;
                pos = next;
            }
            landings.push_back(pos);
        }
    }
    std::sort(landings.begin(), landings.end(),
        [](const Vec3i& a, const Vec3i& b)
        {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        });
    landings.erase(
        std::unique(landings.begin(), landings.end(),
            [](const Vec3i& a, const Vec3i& b)
            { return a.x == b.x && a.y == b.y && a.z == b.z; }),
        landings.end());
    return landings;
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
        if (can_place(game, step)) p = step;
        else break;
    }
    return p;
}

std::vector<AiPlanStep> GameAi::compute_plan(const Game& game, const AiConfig& ai_cfg)
{
    if (!game.active_piece()) return {};
    const Piece& active  = *game.active_piece();
    const auto& next_opt =  game.next_piece();

    const int w = game.well().width();
    const int d = game.well().depth();
    const int h = game.well().height();

    // Build base occupancy once (all layers).
    std::vector<uint8_t> base_occ(w * d * h, 0);
    for (int yy = 0; yy < h; ++yy)
        for (int zz = 0; zz < d; ++zz)
            for (int xx = 0; xx < w; ++xx)
                if (!game.well().is_free(Vec3i{xx, yy, zz}))
                    base_occ[idx3(xx, yy, zz, w, d)] = 1;

    const auto& orientations = get_orientations();

    struct Candidate { int orient_idx; Vec3i pos; float score; };
    std::vector<Candidate> candidates;

    for (int oi = 0; oi < static_cast<int>(orientations.size()); ++oi)
    {
        const auto& orient = orientations[oi];

        // BFS gives all positions reachable from spawn — including lateral
        // insertions that a pure vertical drop would miss.
        auto landings = find_landings_bfs(base_occ, w, d, h, active.blocks, orient.mat);

        for (const auto& pos : landings)
        {
            // Simulate placing current piece and clearing any full planes.
            std::vector<uint8_t> occ1 = base_occ;
            place_piece_occ(occ1, w, d, h, active.blocks, orient.mat, pos);
            clear_planes_occ(occ1, w, d, h);

            float score;
            if (next_opt)
            {
                // 1-piece lookahead: score = best achievable state after next piece.
                // Uses drop-only for speed; BFS already applied to the current piece.
                float best_next = 1e9f;
                for (int oi2 = 0; oi2 < static_cast<int>(orientations.size()); ++oi2)
                {
                    const auto& orient2 = orientations[oi2];
                    auto landings2 = find_landings_drop(occ1, w, d, h,
                                                        next_opt->blocks, orient2.mat);
                    for (const auto& pos2 : landings2)
                    {
                        std::vector<uint8_t> occ2 = occ1;
                        place_piece_occ(occ2, w, d, h, next_opt->blocks, orient2.mat, pos2);
                        clear_planes_occ(occ2, w, d, h);
                        float s = eval_occ(occ2, w, d, h, ai_cfg);
                        if (s < best_next) best_next = s;
                    }
                }
                // No valid next placement = game-over state; heavily penalise.
                score = (best_next < 1e9f) ? best_next : 1e9f;
            }
            else
            {
                score = eval_occ(occ1, w, d, h, ai_cfg);
            }

            candidates.push_back({oi, pos, score});
        }
    }

    if (candidates.empty()) return {};

    const auto& best = *std::min_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score < b.score; });

    std::vector<AiPlanStep> plan;

    // Rotation steps that reproduce the chosen orientation from identity.
    for (const auto& step : orientations[best.orient_idx].steps)
        plan.push_back(step);

    // Translation steps relative to current piece position.
    const int dx = best.pos.x - active.pos.x;
    const int dz = best.pos.z - active.pos.z;
    const int sx = (dx > 0) ? 1 : -1;
    for (int i = 0; i < std::abs(dx); ++i) plan.push_back({AiPlanStep::Type::MoveX, sx});
    const int sz = (dz > 0) ? 1 : -1;
    for (int i = 0; i < std::abs(dz); ++i) plan.push_back({AiPlanStep::Type::MoveZ, sz});

    plan.push_back({AiPlanStep::Type::Drop, 0});
    return plan;
}
