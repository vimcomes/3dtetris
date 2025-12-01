#pragma once

#include <optional>
#include <random>
#include <vector>

#include "config.h"
#include "math.h"

enum class Axis
{
    X,
    Y,
    Z,
};

struct CellState
{
    bool filled = false;
    Vec3 color{};
};

struct Piece
{
    int shape = 0;
    Vec3i pos{0, 0, 0};
    std::vector<Vec3i> blocks;
    Vec3 color{0.9f, 0.8f, 0.35f};
};

struct ShapeBounds
{
    int min_x = 0, max_x = 0;
    int min_y = 0, max_y = 0;
    int min_z = 0, max_z = 0;
};

class Well
{
public:
    Well(int w, int d, int h);

    [[nodiscard]] bool in_bounds(const Vec3i& c) const;
    [[nodiscard]] bool is_free(const Vec3i& c) const;
    void set_cell(const Vec3i& c, bool filled, const Vec3& color);
    void lock_piece(const Piece& p);
    [[nodiscard]] Vec3 cell_center(const Vec3i& c, float cell_size) const;

    int width() const { return width_; }
    int depth() const { return depth_; }
    int height() const { return height_; }
    const std::vector<CellState>& cells() const { return cells_; }

private:
    int width_;
    int depth_;
    int height_;
    std::vector<CellState> cells_;
};

class Game
{
public:
    Game(int w, int d, int h, std::vector<ShapeDef> shapes = {}, float fall_interval = 1.2f);

    void update(float dt);
    bool rotate_active(Axis axis, int dir);
    void move_active(int dx, int dz);
    bool hard_drop();
    [[nodiscard]] std::optional<Piece> ghost_piece() const;
    [[nodiscard]] float fall_progress() const;
    [[nodiscard]] bool active_can_fall() const;
    int clear_full_planes();
    void debug_fill_plane(int y, const Vec3& color);
    void rebuild_locked_cache();
    [[nodiscard]] std::vector<int> filled_planes() const;
    bool can_place_public(const Piece& p) const { return can_place(p); }

    const Well& well() const { return well_; }
    const std::optional<Piece>& active_piece() const { return active_; }
    const std::vector<Vec3i>& locked_cells() const { return locked_positions_; }
    const std::vector<Vec3>& locked_colors() const { return locked_colors_; }

private:
    Well well_;
    std::optional<Piece> active_;
    std::vector<ShapeDef> shapes_;
    std::vector<struct ShapeBounds> bounds_;
    std::vector<Vec3i> locked_positions_;
    std::vector<Vec3> locked_colors_;
    float fall_timer_ = 0.0f;
    float fall_interval_ = 1.2f;
    std::mt19937 rng_;

    Piece spawn_piece();
    bool can_place(const Piece& p) const;
    void try_lock_and_spawn();
};
