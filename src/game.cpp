#include "game.h"

#include <algorithm>

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
        Vec3i c{p.pos.x + b.x, p.pos.y + b.y, p.pos.z + b.z};
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

Game::Game(int w, int d, int h, std::vector<ShapeDef> shapes, float fall_interval) : well_(w, d, h)
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
    active_ = spawn_piece();
}

Piece Game::spawn_piece()
{
    if (shapes_.empty())
    {
        return {};
    }
    std::uniform_int_distribution<int> dist(0, static_cast<int>(shapes_.size() - 1));
    int shape = dist(rng_);
    Piece p;
    p.shape = shape;
    p.blocks = shapes_[shape].blocks;
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
    return p;
}

bool Game::can_place(const Piece& p) const
{
    for (auto b : p.blocks)
    {
        Vec3i c{p.pos.x + b.x, p.pos.y + b.y, p.pos.z + b.z};
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
    well_.lock_piece(*active_);
    clear_full_planes();
    rebuild_locked_cache();
    active_ = spawn_piece();
    if (!can_place(*active_))
    {
        // Simple reset on top collision.
        well_ = Well(well_.width(), well_.depth(), well_.height());
        locked_positions_.clear();
        locked_colors_.clear();
        active_ = spawn_piece();
    }
}

int Game::clear_full_planes()
{
    int cleared = 0;
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

void Game::update(float dt)
{
    if (!active_)
    {
        active_ = spawn_piece();
    }
    fall_timer_ += dt;
    if (fall_timer_ >= fall_interval_)
    {
        fall_timer_ = 0.0f;
        Piece moved = *active_;
        moved.pos.y -= 1;
        if (can_place(moved))
        {
            active_ = moved;
        }
        else
        {
            try_lock_and_spawn();
        }
    }
}

bool Game::rotate_active(Axis axis, int dir)
{
    if (!active_)
    {
        return false;
    }
    Piece rotated = *active_;
    for (auto& b : rotated.blocks)
    {
        b = rotate_block(b, axis, dir);
    }
    if (can_place(rotated))
    {
        active_ = rotated;
        return true;
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
    active_ = dropped;
    try_lock_and_spawn();
    fall_timer_ = 0.0f;
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
    if (fall_interval_ <= 0.0f)
    {
        return 0.0f;
    }
    float t = fall_timer_ / fall_interval_;
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
