#include "game_ai.h"

#include <algorithm>
#include <cmath>

namespace
{
int idx3(int x, int y, int z, int w, int d)
{
    return y * d * w + z * w + x;
}
}

Vec3i GameAi::rotate_block_local(const Vec3i& v, Axis axis, int dir)
{
    int r = (dir >= 0) ? 1 : -1;
    switch (axis)
    {
    case Axis::X: return Vec3i{v.x, r * v.z * -1, r * v.y};
    case Axis::Y: return Vec3i{r * v.z, v.y, r * -v.x};
    case Axis::Z: return Vec3i{r * -v.y, r * v.x, v.z};
    }
    return v;
}

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
        {
            p = step;
        }
        else
        {
            break;
        }
    }
    return p;
}

std::vector<AiPlanStep> GameAi::compute_plan(const Game& game)
{
    std::vector<AiPlanStep> empty;
    if (!game.active_piece())
    {
        return empty;
    }
    const Piece& active = *game.active_piece();
    // Generate rotations.
    std::vector<std::array<int, 3>> rotations = {
        {0, 0, 0}, {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1},
    };

    struct Candidate
    {
        std::vector<Vec3i> blocks;
        Vec3i pos;
        int rx = 0, ry = 0, rz = 0;
        float score = 0.f;
    };
    std::vector<Candidate> candidates;

    for (auto rot : rotations)
    {
        std::vector<Vec3i> rb = active.blocks;
        for (auto& b : rb)
        {
            b = rotate_block_local(b, Axis::X, rot[0]);
            b = rotate_block_local(b, Axis::Y, rot[1]);
            b = rotate_block_local(b, Axis::Z, rot[2]);
        }
        int minx = 0, maxx = 0, minz = 0, maxz = 0, miny = 0, maxy = 0;
        for (auto& b : rb)
        {
            minx = std::min(minx, b.x); maxx = std::max(maxx, b.x);
            minz = std::min(minz, b.z); maxz = std::max(maxz, b.z);
            miny = std::min(miny, b.y); maxy = std::max(maxy, b.y);
        }
        for (int x = 0; x < game.well().width(); ++x)
        {
            for (int z = 0; z < game.well().depth(); ++z)
            {
                Piece p = active;
                p.blocks = rb;
                p.pos = Vec3i{x - minx, game.well().height() - 1 - maxy, z - minz};
                p = drop_piece(game, p);
                if (!can_place(game, p)) continue;

                // Build occupancy.
                int w = game.well().width();
                int d = game.well().depth();
                int h = game.well().height();
                std::vector<uint8_t> occ(w * d * h, 0);
                const auto& cells = game.well().cells();
                for (int yy = 0; yy < h; ++yy)
                {
                    for (int zz = 0; zz < d; ++zz)
                    {
                        for (int xx = 0; xx < w; ++xx)
                        {
                            if (!game.well().is_free(Vec3i{xx, yy, zz}))
                            {
                                occ[idx3(xx, yy, zz, w, d)] = 1;
                            }
                        }
                    }
                }
                for (auto b : p.blocks)
                {
                    int xx = p.pos.x + b.x;
                    int yy = p.pos.y + b.y;
                    int zz = p.pos.z + b.z;
                    if (xx >= 0 && xx < w && yy >= 0 && yy < h && zz >= 0 && zz < d)
                    {
                        occ[idx3(xx, yy, zz, w, d)] = 1;
                    }
                }
                // Heuristic terms.
                int full_planes = 0;
                for (int yy = 0; yy < h; ++yy)
                {
                    bool full = true;
                    for (int zz = 0; zz < d && full; ++zz)
                        for (int xx = 0; xx < w; ++xx)
                            if (!occ[idx3(xx, yy, zz, w, d)]) { full = false; break; }
                    if (full) full_planes++;
                }

                int max_height = 0;
                int holes = 0;
                int agg_height = 0;
                int bumpiness = 0;
                // Column heights.
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
                                holes++;
                            }
                        }
                        heights[zz * w + xx] = col_h;
                        agg_height += col_h;
                    }
                }
                // Bumpiness across x, z neighbors.
                for (int zz = 0; zz < d; ++zz)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        int h0 = heights[zz * w + xx];
                        if (xx + 1 < w)
                        {
                            bumpiness += std::abs(h0 - heights[zz * w + (xx + 1)]);
                        }
                        if (zz + 1 < d)
                        {
                            bumpiness += std::abs(h0 - heights[(zz + 1) * w + xx]);
                        }
                    }
                }

                float score = 0.f;
                score += max_height * 5.0f;
                score += agg_height * 0.5f;
                score += holes * 50.0f;
                score += bumpiness * 3.0f;
                score -= full_planes * 800.0f;

                candidates.push_back({rb, p.pos, rot[0], rot[1], rot[2], score});
            }
        }
    }

    if (!candidates.empty())
    {
        auto best = *std::min_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b){ return a.score < b.score; });
        std::vector<AiPlanStep> plan;
        auto push_rot = [&](Axis axis, int times)
        {
            int t = times;
            if (t > 0) for (int i = 0; i < t; ++i) plan.push_back({axis == Axis::X ? AiPlanStep::Type::RotX : axis == Axis::Y ? AiPlanStep::Type::RotY : AiPlanStep::Type::RotZ, 1});
            if (t < 0) for (int i = 0; i < -t; ++i) plan.push_back({axis == Axis::X ? AiPlanStep::Type::RotX : axis == Axis::Y ? AiPlanStep::Type::RotY : AiPlanStep::Type::RotZ, -1});
        };
        push_rot(Axis::X, best.rx);
        push_rot(Axis::Y, best.ry);
        push_rot(Axis::Z, best.rz);

        if (const auto& cur = game.active_piece())
        {
            int dx = best.pos.x - cur->pos.x;
            int dz = best.pos.z - cur->pos.z;
            int step_x = (dx > 0) ? 1 : -1;
            for (int i = 0; i < std::abs(dx); ++i) plan.push_back({AiPlanStep::Type::MoveX, step_x});
            int step_z = (dz > 0) ? 1 : -1;
            for (int i = 0; i < std::abs(dz); ++i) plan.push_back({AiPlanStep::Type::MoveZ, step_z});
        }
        plan.push_back({AiPlanStep::Type::Drop, 0});
        return plan;
    }
    return empty;
}
