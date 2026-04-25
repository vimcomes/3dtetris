#include "game.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
int index3(int x, int y, int z, int w, int d)
{
    return y * d * w + z * w + x;
}

Vec3i rotate_block(const Vec3i& v, Axis axis, int dir)
{
    // dir: +1 clockwise (looking from positive axis), -1 counter-clockwise.
    int r = (dir >= 0) ? 1 : -1;
    switch (axis)
    {
    case Axis::X: return Vec3i{v.x, r * v.z * -1, r * v.y};
    case Axis::Y: return Vec3i{r * v.z, v.y, r * -v.x};
    case Axis::Z: return Vec3i{r * -v.y, r * v.x, v.z};
    }
    return v;
}

const int ROT_X_POS[3][3] = {
    {1, 0, 0},
    {0, 0, -1},
    {0, 1, 0},
};
const int ROT_X_NEG[3][3] = {
    {1, 0, 0},
    {0, 0, 1},
    {0, -1, 0},
};
const int ROT_Y_POS[3][3] = {
    {0, 0, 1},
    {0, 1, 0},
    {-1, 0, 0},
};
const int ROT_Y_NEG[3][3] = {
    {0, 0, -1},
    {0, 1, 0},
    {1, 0, 0},
};
const int ROT_Z_POS[3][3] = {
    {0, -1, 0},
    {1, 0, 0},
    {0, 0, 1},
};
const int ROT_Z_NEG[3][3] = {
    {0, 1, 0},
    {-1, 0, 0},
    {0, 0, 1},
};

void mul_rot(const int A[3][3], const int B[3][3], int out[3][3])
{
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int v = 0;
            for (int k = 0; k < 3; ++k)
            {
                v += A[i][k] * B[k][j];
            }
            out[i][j] = v;
        }
    }
    // Binarize to {-1,0,1} to avoid drift.
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (out[i][j] > 0) out[i][j] = 1;
            else if (out[i][j] < 0) out[i][j] = -1;
            else out[i][j] = 0;
        }
    }
}

ShapeBounds rotated_bounds(const std::vector<Vec3i>& blocks, const int rot[3][3])
{
    ShapeBounds b{};
    if (blocks.empty())
    {
        return b;
    }
    Vec3i r0 = apply_rot(rot, blocks[0]);
    b.min_x = b.max_x = r0.x;
    b.min_y = b.max_y = r0.y;
    b.min_z = b.max_z = r0.z;
    for (size_t i = 1; i < blocks.size(); ++i)
    {
        Vec3i r = apply_rot(rot, blocks[i]);
        b.min_x = std::min(b.min_x, r.x);
        b.max_x = std::max(b.max_x, r.x);
        b.min_y = std::min(b.min_y, r.y);
        b.max_y = std::max(b.max_y, r.y);
        b.min_z = std::min(b.min_z, r.z);
        b.max_z = std::max(b.max_z, r.z);
    }
    return b;
}

int grid_y_from_float(float y)
{
    // Keep the block in the upper cell until it fully crosses into the next.
    return static_cast<int>(std::ceil(y - 1e-4f));
}
} // namespace

Well::Well(int w, int d, int h) : width_(w), depth_(d), height_(h), cells_(w * d * h)
{
}

bool Well::in_bounds(const Vec3i& c) const
{
    return c.x >= 0 && c.x < width_ &&
           c.y >= 0 && c.y < height_ &&
           c.z >= 0 && c.z < depth_;
}

bool Well::is_free(const Vec3i& c) const
{
    if (!in_bounds(c))
    {
        return false;
    }
    return !cells_[index3(c.x, c.y, c.z, width_, depth_)].filled;
}

void Well::set_cell(const Vec3i& c, bool filled, const Vec3& color)
{
    if (!in_bounds(c)) return;
    auto& cell = cells_[index3(c.x, c.y, c.z, width_, depth_)];
    cell.filled = filled;
    cell.color = color;
}

void Well::lock_piece(const Piece& p)
{
    for (auto b : p.blocks)
    {
        Vec3i rb = apply_rot(p.rot, b);
        Vec3i c{p.pos.x + rb.x, p.pos.y + rb.y, p.pos.z + rb.z};
        if (!in_bounds(c))
        {
            continue;
        }
        auto& cell = cells_[index3(c.x, c.y, c.z, width_, depth_)];
        cell.filled = true;
        cell.color = p.color;
    }
}

Vec3 Well::cell_center(const Vec3i& c, float cell_size) const
{
    float min_x = -0.5f * width_ * cell_size;
    float min_z = -0.5f * depth_ * cell_size;
    return Vec3{min_x + (c.x + 0.5f) * cell_size,
                (c.y + 0.5f) * cell_size,
                min_z + (c.z + 0.5f) * cell_size};
}

Game::Game(int w, int d, int h, std::vector<ShapeDef> shapes, float fall_interval, int start_level) : well_(w, d, h)
{
    if (shapes.empty())
    {
        // Fallback to built-in modern shapes if nothing was loaded.
        std::vector<Vec3> colors = {
            Vec3{0.0f, 1.0f, 0.0f},  Vec3{1.0f, 0.0f, 0.0f},  Vec3{0.0f, 0.9f, 1.0f},
            Vec3{0.0f, 0.0f, 1.0f},  Vec3{1.0f, 0.75f, 0.0f}, Vec3{1.0f, 0.0f, 0.8f},
            Vec3{0.0f, 1.0f, 0.6f},  Vec3{0.9f, 0.9f, 0.9f}, Vec3{0.5f, 0.7f, 1.0f}};
        shapes = {
            {{Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{2, 0, 0}}, colors[0]},
            {{
                 Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{0, 0, 1}, Vec3i{1, 0, 1},
                 Vec3i{0, 1, 0}, Vec3i{1, 1, 0}, Vec3i{0, 1, 1}, Vec3i{1, 1, 1},
             },
             colors[1]},
            {{Vec3i{0, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{1, 0, 0}, Vec3i{0, 0, 1}}, colors[2]},
            {{Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{-1, 0, 1}}, colors[3]},
            {{Vec3i{0, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{1, 0, 0}, Vec3i{1, 0, 1}}, colors[4]},
            {{Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{0, 0, 1}, Vec3i{-1, 0, 1}}, colors[5]},
            {{Vec3i{0, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{0, 0, 1}, Vec3i{1, 0, 1}}, colors[6]},
            {{Vec3i{0, 0, 0}}, colors[7]},
            {{Vec3i{0, 0, 0}, Vec3i{0, 1, 0}}, colors[8]},
        };
    }

    shapes_ = std::move(shapes);
    bounds_.resize(shapes_.size());
    for (size_t i = 0; i < shapes_.size(); ++i)
    {
        const auto& blocks = shapes_[i].blocks;
        if (blocks.empty()) continue;
        int min_x = blocks[0].x, max_x = blocks[0].x;
        int min_y = blocks[0].y, max_y = blocks[0].y;
        int min_z = blocks[0].z, max_z = blocks[0].z;
        for (const auto& b : blocks)
        {
            min_x = std::min(min_x, b.x);
            max_x = std::max(max_x, b.x);
            min_y = std::min(min_y, b.y);
            max_y = std::max(max_y, b.y);
            min_z = std::min(min_z, b.z);
            max_z = std::max(max_z, b.z);
        }
        bounds_[i] = ShapeBounds{min_x, max_x, min_y, max_y, min_z, max_z};
    }

    rng_.seed(std::random_device{}());
    if (fall_interval > 0.f)
    {
        fall_interval_ = fall_interval;
    }
    base_fall_interval_ = fall_interval_;
    start_level_ = std::clamp(start_level, 0, 9);
    reset_progress();
    active_ = spawn_piece();
}

Piece Game::draw_from_bag()
{
    if (shapes_.empty()) return {};
    if (bag_.empty())
    {
        bag_.resize(shapes_.size());
        std::iota(bag_.begin(), bag_.end(), 0);
        std::shuffle(bag_.begin(), bag_.end(), rng_);
    }
    int shape = bag_.back();
    bag_.pop_back();

    Piece p;
    p.shape = shape;
    p.blocks = shapes_[shape].blocks;
    p.rot[0][0] = 1; p.rot[0][1] = 0; p.rot[0][2] = 0;
    p.rot[1][0] = 0; p.rot[1][1] = 1; p.rot[1][2] = 0;
    p.rot[2][0] = 0; p.rot[2][1] = 0; p.rot[2][2] = 1;
    p.color = shapes_[shape].color;

    const auto& b = bounds_[shape];
    int shape_w = b.max_x - b.min_x + 1;
    int shape_d = b.max_z - b.min_z + 1;
    int shape_h = b.max_y - b.min_y + 1;

    int start_x = (well_.width() - shape_w) / 2 - b.min_x;
    int start_z = (well_.depth() - shape_d) / 2 - b.min_z;
    int start_y = well_.height() - shape_h - b.min_y;

    start_x = std::max(0, start_x);
    start_z = std::max(0, start_z);
    start_y = std::max(0, start_y);

    p.pos = Vec3i{start_x, start_y, start_z};
    p.pos_y = static_cast<float>(p.pos.y);
    return p;
}

Piece Game::spawn_piece()
{
    if (shapes_.empty()) return {};
    if (!next_piece_) next_piece_ = draw_from_bag();
    Piece current = *next_piece_;
    next_piece_ = draw_from_bag();
    return current;
}

bool Game::can_place(const Piece& p) const
{
    for (auto b : p.blocks)
    {
        Vec3i rb = apply_rot(p.rot, b);
        Vec3i c{p.pos.x + rb.x, p.pos.y + rb.y, p.pos.z + rb.z};
        if (!well_.in_bounds(c))
        {
            return false;
        }
        if (!well_.is_free(c))
        {
            return false;
        }
    }
    return true;
}

void Game::try_lock_and_spawn()
{
    if (!active_)
    {
        return;
    }
    const int piece_cubes = static_cast<int>(active_->blocks.size());
    int min_y = well_.height();
    int max_y = -1;
    for (auto b : active_->blocks)
    {
        Vec3i rb = apply_rot(active_->rot, b);
        int y = active_->pos.y + rb.y;
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
    min_y = std::clamp(min_y, 0, well_.height() - 1);
    max_y = std::clamp(max_y, 0, well_.height() - 1);
    well_.lock_piece(*active_);
    last_cleared_ = clear_full_planes_range(min_y, max_y);
    total_cleared_ += last_cleared_;
    rebuild_locked_cache();
    cubes_dropped_ += piece_cubes;
    update_level_and_speed();
    const int stack = stack_height();
    const int free_height = std::max(1, well_.height() - stack);
    const float time_now = piece_timer_;
    const float fall_speed = fall_interval_ > 0.0f ? 1.0f / fall_interval_ : 0.0f;
    const float clear_bonus = std::pow(2.0f, static_cast<float>(last_cleared_));
    const float move_score = (level_factor_ * time_now * 200.0f * clear_bonus * fall_speed) / free_height;
    score_ += static_cast<int>(move_score + 0.5f);
    active_ = spawn_piece();
    fall_timer_ = 0.0f;
    piece_timer_ = 0.0f;
    drop_target_.reset();
    if (!can_place(*active_))
    {
        state_ = GameState::GameOver;
        active_.reset();
    }
}

int Game::clear_full_planes()
{
    return clear_full_planes_range(0, well_.height() - 1);
}

int Game::clear_full_planes_range(int min_y, int max_y)
{
    if (well_.height() <= 0)
    {
        return 0;
    }
    min_y = std::clamp(min_y, 0, well_.height() - 1);
    max_y = std::clamp(max_y, 0, well_.height() - 1);
    if (min_y > max_y)
    {
        return 0;
    }
    int cleared = 0;
    for (int y = min_y; y <= max_y; ++y)
    {
        bool full = true;
        for (int z = 0; z < well_.depth() && full; ++z)
        {
            for (int x = 0; x < well_.width(); ++x)
            {
                if (well_.is_free(Vec3i{x, y, z}))
                {
                    full = false;
                    break;
                }
            }
        }
        if (!full)
        {
            continue;
        }
        // Shift layers above down.
        for (int yy = y; yy < well_.height() - 1; ++yy)
        {
            for (int z = 0; z < well_.depth(); ++z)
            {
                for (int x = 0; x < well_.width(); ++x)
                {
                    Vec3i from{x, yy + 1, z};
                    Vec3i to{x, yy, z};
                    if (well_.in_bounds(from))
                    {
                        const auto& src = well_.cells()[index3(from.x, from.y, from.z, well_.width(), well_.depth())];
                        well_.set_cell(to, src.filled, src.color);
                    }
                }
            }
        }
        // Clear top layer.
        int top = well_.height() - 1;
        for (int z = 0; z < well_.depth(); ++z)
        {
            for (int x = 0; x < well_.width(); ++x)
            {
                well_.set_cell(Vec3i{x, top, z}, false, Vec3{});
            }
        }
        ++cleared;
        --y; // recheck this y after collapsing.
    }
    return cleared;
}

void Game::rebuild_locked_cache()
{
    locked_positions_.clear();
    locked_colors_.clear();
    for (int y = 0; y < well_.height(); ++y)
    {
        for (int z = 0; z < well_.depth(); ++z)
        {
            for (int x = 0; x < well_.width(); ++x)
            {
                Vec3i c{x, y, z};
                if (well_.is_free(c))
                {
                    continue;
                }
                locked_positions_.push_back(c);
                locked_colors_.push_back(well_.cells()[index3(x, y, z, well_.width(), well_.depth())].color);
            }
        }
    }
}

void Game::debug_fill_plane(int y, const Vec3& color)
{
    if (y < 0 || y >= well_.height()) return;
    for (int z = 0; z < well_.depth(); ++z)
    {
        for (int x = 0; x < well_.width(); ++x)
        {
            well_.set_cell(Vec3i{x, y, z}, true, color);
        }
    }
    rebuild_locked_cache();
}

std::vector<int> Game::filled_planes() const
{
    std::vector<int> planes;
    for (int y = 0; y < well_.height(); ++y)
    {
        bool full = true;
        for (int z = 0; z < well_.depth() && full; ++z)
        {
            for (int x = 0; x < well_.width(); ++x)
            {
                if (well_.is_free(Vec3i{x, y, z}))
                {
                    full = false;
                    break;
                }
            }
        }
        if (full) planes.push_back(y);
    }
    return planes;
}

void Game::restart()
{
    well_ = Well(well_.width(), well_.depth(), well_.height());
    locked_positions_.clear();
    locked_colors_.clear();
    total_cleared_ = 0;
    state_ = GameState::Playing;
    bag_.clear();
    next_piece_.reset();
    reset_progress();
    active_ = spawn_piece();
    fall_timer_ = 0.0f;
    piece_timer_ = 0.0f;
    drop_target_.reset();
}

void Game::update(float dt)
{
    if (state_ != GameState::Playing) return;

    if (!active_)
    {
        active_ = spawn_piece();
        fall_timer_ = 0.0f;
        piece_timer_ = 0.0f;
    }
    piece_timer_ += dt;

    float fall_speed = fall_interval_ > 0.0f ? (1.0f / fall_interval_) : 0.0f;
    if (drop_target_)
    {
        fall_speed = std::max(fall_speed * 6.0f, fall_speed + 3.0f);
    }

    float new_y = active_->pos_y - fall_speed * dt;
    int new_grid_y = grid_y_from_float(new_y);
    Piece moved = *active_;
    moved.pos_y = new_y;
    moved.pos.y = new_grid_y;

    if (can_place(moved))
    {
        active_ = moved;
        if (drop_target_ && active_->pos.y <= *drop_target_)
        {
            active_->pos.y = *drop_target_;
            active_->pos_y = static_cast<float>(active_->pos.y);
            drop_target_.reset();
            try_lock_and_spawn();
        }
    }
    else
    {
        active_->pos_y = static_cast<float>(active_->pos.y);
        drop_target_.reset();
        try_lock_and_spawn();
    }
}

bool Game::rotate_active(Axis axis, int dir)
{
    if (!active_)
    {
        return false;
    }
    Piece rotated = *active_;
    int tmp[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    switch (axis)
    {
    case Axis::X:
        if (dir >= 0) mul_rot(rotated.rot, ROT_X_POS, tmp);
        else mul_rot(rotated.rot, ROT_X_NEG, tmp);
        break;
    case Axis::Y:
        if (dir >= 0) mul_rot(rotated.rot, ROT_Y_POS, tmp);
        else mul_rot(rotated.rot, ROT_Y_NEG, tmp);
        break;
    case Axis::Z:
        if (dir >= 0) mul_rot(rotated.rot, ROT_Z_POS, tmp);
        else mul_rot(rotated.rot, ROT_Z_NEG, tmp);
        break;
    }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            rotated.rot[i][j] = tmp[i][j];
        }
    }
    if (can_place(rotated))
    {
        active_ = rotated;
        return true;
    }

    // Blokout-style wall kick: try to nudge within bounds step-by-step.
    {
        Piece kicked = rotated;
        ShapeBounds rb = rotated_bounds(kicked.blocks, kicked.rot);
        bool ok = true;
        while (ok && kicked.pos.x + rb.min_x < 0)
        {
            Piece step = kicked;
            step.pos.x += 1;
            if (!can_place(step))
            {
                ok = false;
                break;
            }
            kicked = step;
        }
        while (ok && kicked.pos.x + rb.max_x >= well_.width())
        {
            Piece step = kicked;
            step.pos.x -= 1;
            if (!can_place(step))
            {
                ok = false;
                break;
            }
            kicked = step;
        }
        while (ok && kicked.pos.z + rb.min_z < 0)
        {
            Piece step = kicked;
            step.pos.z += 1;
            if (!can_place(step))
            {
                ok = false;
                break;
            }
            kicked = step;
        }
        while (ok && kicked.pos.z + rb.max_z >= well_.depth())
        {
            Piece step = kicked;
            step.pos.z -= 1;
            if (!can_place(step))
            {
                ok = false;
                break;
            }
            kicked = step;
        }
        if (ok && can_place(kicked))
        {
            active_ = kicked;
            return true;
        }
    }

    // Extra kick attempts near obstacles.
    {
        static const Vec3i kicks[] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
            {2, 0, 0}, {-2, 0, 0}, {0, 0, 2}, {0, 0, -2},
            {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
        };
        for (const auto& k : kicks)
        {
            Piece candidate = rotated;
            candidate.pos.x += k.x;
            candidate.pos.z += k.z;
            if (can_place(candidate))
            {
                active_ = candidate;
                return true;
            }
        }
    }

    return false;
}

void Game::move_active(int dx, int dz)
{
    if (!active_)
    {
        return;
    }
    Piece moved = *active_;
    moved.pos.x += dx;
    moved.pos.z += dz;
    if (can_place(moved))
    {
        active_ = moved;
    }
}

bool Game::hard_drop()
{
    if (!active_)
    {
        return false;
    }
    Piece dropped = *active_;
    while (true)
    {
        Piece step = dropped;
        step.pos.y -= 1;
        if (can_place(step))
        {
            dropped = step;
        }
        else
        {
            break;
        }
    }
    drop_target_ = dropped.pos.y;
    return true;
}

std::optional<Piece> Game::ghost_piece() const
{
    if (!active_)
    {
        return std::nullopt;
    }
    Piece ghost = *active_;
    while (true)
    {
        Piece step = ghost;
        step.pos.y -= 1;
        if (can_place(step))
        {
            ghost = step;
        }
        else
        {
            break;
        }
    }
    return ghost;
}

float Game::fall_progress() const
{
    if (!active_)
    {
        return 0.0f;
    }
    float t = static_cast<float>(active_->pos.y) - active_->pos_y;
    return std::clamp(t, 0.0f, 1.0f);
}

bool Game::active_can_fall() const
{
    if (!active_)
    {
        return false;
    }
    Piece step = *active_;
    step.pos.y -= 1;
    return can_place(step);
}

void Game::reset_progress()
{
    score_ = 0;
    cubes_dropped_ = 0;
    last_cleared_ = 0;
    level_ = start_level_;
    level_factor_ = (level_ < 5) ? (level_ / 5.0f) : (level_ - 5.0f);
    update_level_and_speed();
}

void Game::update_level_and_speed()
{
    const int new_level = cubes_dropped_ / 70;
    if (level_ < 10 && level_ < new_level)
    {
        level_ = new_level;
        level_factor_ = (level_ < 5) ? (level_ / 5.0f) : (level_ - 5.0f);
    }
    const float base_speed = base_fall_interval_ > 0.0f ? 1.0f / base_fall_interval_ : 0.0f;
    const float fall_speed = base_speed + level_ * 0.3f;
    fall_interval_ = fall_speed > 0.0f ? (1.0f / fall_speed) : base_fall_interval_;
}

int Game::stack_height() const
{
    int top = -1;
    for (int y = 0; y < well_.height(); ++y)
    {
        for (int z = 0; z < well_.depth(); ++z)
        {
            for (int x = 0; x < well_.width(); ++x)
            {
                if (!well_.is_free(Vec3i{x, y, z}))
                {
                    if (y > top) top = y;
                }
            }
        }
    }
    return top + 1;
}
