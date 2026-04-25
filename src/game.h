#pragma once

#include <optional>
#include <random>
#include <vector>

#include "config.h"
#include "math.h"

enum class GameState { Playing, Paused, GameOver };

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
    float pos_y = 0.0f;
    std::vector<Vec3i> blocks; // base offsets in local space
    int rot[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vec3 color{0.9f, 0.8f, 0.35f};
};

inline Vec3i apply_rot(const int R[3][3], const Vec3i& v)
{
    return Vec3i{
        R[0][0] * v.x + R[0][1] * v.y + R[0][2] * v.z,
        R[1][0] * v.x + R[1][1] * v.y + R[1][2] * v.z,
        R[2][0] * v.x + R[2][1] * v.y + R[2][2] * v.z,
    };
}

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
    Game(int w, int d, int h, std::vector<ShapeDef> shapes = {}, float fall_interval = 1.2f, int start_level = 2);

    void update(float dt);
    bool rotate_active(Axis axis, int dir);
    void move_active(int dx, int dz);
    bool hard_drop();
    [[nodiscard]] std::optional<Piece> ghost_piece() const;
    [[nodiscard]] float fall_progress() const;
    [[nodiscard]] bool active_can_fall() const;
    int clear_full_planes();
    int clear_full_planes_range(int min_y, int max_y);
    void debug_fill_plane(int y, const Vec3& color);
    void rebuild_locked_cache();
    [[nodiscard]] std::vector<int> filled_planes() const;
    bool can_place_public(const Piece& p) const { return can_place(p); }
    int score() const { return score_; }
    int level() const { return level_; }
    int cubes_dropped() const { return cubes_dropped_; }
    int last_cleared() const { return last_cleared_; }
    int total_cleared() const { return total_cleared_; }
    float fall_interval() const { return fall_interval_; }
    float fall_speed() const { return fall_interval_ > 0.0f ? 1.0f / fall_interval_ : 0.0f; }
    float piece_time() const { return piece_timer_; }
    GameState state() const { return state_; }
    void set_state(GameState s) { state_ = s; }
    void restart();

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
    int score_ = 0;
    int level_ = 2;
    int start_level_ = 2;
    int cubes_dropped_ = 0;
    int last_cleared_ = 0;
    int total_cleared_ = 0;
    GameState state_ = GameState::Playing;
    float level_factor_ = 0.4f;
    float base_fall_interval_ = 1.2f;
    float fall_timer_ = 0.0f;
    float fall_interval_ = 1.2f;
    float piece_timer_ = 0.0f;
    std::optional<int> drop_target_;
    std::mt19937 rng_;

    Piece spawn_piece();
    bool can_place(const Piece& p) const;
    void try_lock_and_spawn();
    void reset_progress();
    void update_level_and_speed();
    int stack_height() const;
};
