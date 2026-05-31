# 3D Tetris

A fully 3D take on Blockout-style tetris written in C++20 with OpenGL 3.3. Pieces fall through a 3D well, rotate freely around all three axes, and you clear entire planes instead of lines. Built from scratch as a personal project to explore 3D game logic, real-time rendering, and AI search in an interactive context.

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)
![C++](https://img.shields.io/badge/C%2B%2B-20-informational)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-orange)

## What's new

Reworked the entire rendering pipeline into a **glassmorphism / synthwave** style:

- **Glass blocks** — solid faces replaced with translucent glass (alpha 0.80), rendered without depth writes so overlapping pieces blend correctly.
- **Phong shading with 3 dynamic coloured lights** — cyan, hot-pink, and purple point lights orbit the well at runtime, lighting each face according to its outward normal.
- **Emissive breathing pulse** — every block's emissive channel oscillates (`0.4 + sin(t×2) × 0.15`), giving the whole well a slow breathing glow.
- **Bright wireframe edges** — each block's 12 box edges are drawn as separate GL_LINES at high emissive (2.5–3×), creating a neon-wire outline effect independent of the glass faces.
- **7-colour neon palette** — active pieces cycle through cyan / hot-pink / purple / electric-blue / teal / orange / violet per config shape colours.
- **Visible floor plane** — a bright coloured quad closes the bottom of the well, with correct CCW winding so it survives backface culling when viewed from above.
- **Gradient background** — deep navy-to-purple vertical gradient replaces the flat clear colour.
- **BFS reachability + 1-piece lookahead AI** — the AI planner was upgraded to verify each candidate placement is reachable (BFS through all moves/rotations) and to score one piece ahead.
- Custom Dear ImGui neon-cyan theme to match the visual style.

## Features

- **True 3D rotation** — pieces use integer 3×3 rotation matrices (SO(3) discrete subgroup, 24 unique orientations). No gimbal lock, no drift.
- **Blockout presets** — supports data-driven shape sets loaded from `.dat` files matching the original Blockout format. Ships with basic, advanced, and expert sets.
- **Dual viewport** — main perspective view with orbit/zoom controls + a fixed isometric mini-view in the sidebar.
- **AI auto-play** — brute-force planner enumerates all 24 orientations × every board position, scores each placement with a heuristic (holes, aggregate height, bumpiness, plane clears), and executes the best plan step by step.
- **Glassmorphism visual style** — translucent glass blocks, Phong lighting with 3 orbiting coloured point lights, emissive breathing pulse, neon wireframe edges.
- **HUD + next-piece preview + held piece display** — score/level/lines overlay, isometric next-piece preview, held piece shown in sidebar.
- **7-bag randomiser** — pieces are drawn from a shuffled bag of all shapes, guaranteeing even distribution.
- **Game state machine** — Playing / Paused (P) / Game Over with restart, proper top-out detection.
- **Hold piece + soft drop** — hold current piece (C) for later use; soft drop (V) for gradual speed-up.
- **High scores** — best score persisted to `highscore.txt` next to the binary.
- **Configurable** — well dimensions (3–7 × 3–7 × 5–20), start level, fall speed, colour palette, shape set, key bindings, AI weights — all in `config.toml`.

## Tech stack

| Layer | Library / approach |
|---|---|
| Window & input | GLFW 3.3.9 |
| OpenGL loader | GLAD (core profile 3.3) |
| UI | Dear ImGui (docking branch) |
| Math | Hand-rolled `math.h` — Vec3, Vec3i, Mat4, perspective, look_at |
| Build | CMake 3.24+, FetchContent (offline/disconnected mode) |
| Language | C++20 |

No external math library, no engine, no ECS — just the essentials.

## Building

Dependencies are fetched via CMake FetchContent. On first clone you need internet access (or pre-populated `cmake-build-debug/_deps/`).

```bash
git clone https://github.com/vimcomes/3dtetris.git
cd 3dtetris
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
./cmake-build-debug/3dtetris
```

CLion users: open the repo root, select the `Debug` preset, build and run.

## Controls

| Key | Action |
|---|---|
| Left / Right / Up / Down | Move piece on X / Z axes |
| E / D | Rotate around X |
| W / S | Rotate around Z |
| Q / A | Rotate around Y |
| Space | Hard drop |
| V | Soft drop (hold) |
| C | Hold piece (swap with held) |
| P | Pause / resume |
| F | Toggle wireframe on active piece |
| LMB drag | Orbit main camera (yaw) |
| RMB drag | Tilt camera (pitch); RMB click resets view |
| Mouse wheel | Zoom |
| Esc | Quit |

## Project structure

```
src/
  app.cpp        — main loop, ownership, dispatch (~980 lines)
  app_state.h    — shared state structs (SpinState, AutoPlayState, etc.)
  game.cpp/h     — game logic: well, piece physics, rotation, line clears
  game_ai.cpp/h  — AI planner: orientation enumeration, heuristic search
  render.cpp/h   — GLSL shaders (geometry + gradient)
  palette.h      — RenderPalette struct (no GL dependency)
  config.cpp/h   — TOML config loader, shape definitions
  rotation.h     — rotation matrices + mul_rot/apply_rot (single source)
  geometry.h     — mesh builders: cubes, edges, floor grid, well walls
  math.h         — linear algebra (Vec3, Mat4, transforms)
  shader.h       — GLSL compile/link helpers
  input.cpp/h    — GLFW init, callbacks, key-name resolver
  scores.cpp/h   — high score persistence
  gfx/
    mesh.h/cpp       — GlMesh struct, mesh constructors, piece mesh/edges
    renderer.h/cpp   — draw_block, draw_flat, setup_block_shader
  ui/
    hud.h/cpp        — ImGui init, HUD, GameOver, Paused overlays
    panels.h/cpp     — dockspace layout, Controls panel
data/
  forms_blockout.dat  — Blockout shape definitions
  highscore.txt       — persisted best score
config.toml            — runtime configuration
```

## Configuration

`config.toml` controls everything without recompiling:

```toml
[render.palette]
clear   = [0.024, 0.012, 0.059]
grid    = [1.0, 1.0, 1.0]
outline = [0.92, 0.95, 0.98]

[shapes]
I    = [0.298, 0.788, 0.941]
O    = [0.969, 0.145, 0.522]
T    = [0.443, 0.035, 0.718]
L    = [0.263, 0.380, 0.933]
J    = [0.024, 0.839, 0.627]
S    = [0.969, 0.498, 0.000]
Z    = [0.659, 0.333, 0.969]
Dot  = [0.298, 0.788, 0.941]
Bar2 = [0.969, 0.145, 0.522]

[well]
width  = 6
depth  = 6
height = 20

[gameplay]
fall_interval = 1.2
start_level   = 0

[preset]
name = "blockout"       # "modern" | "blockout"
blockout_set = "basic"  # "basic" | "advanced" | "expert"

[controls]
move_left = "Left"
move_right = "Right"
move_forward = "Up"
move_back = "Down"
rot_x_pos = "E"
rot_x_neg = "D"
rot_z_pos = "W"
rot_z_neg = "S"
rot_y_pos = "Q"
rot_y_neg = "A"
hard_drop = "Space"
soft_drop = "V"
hold = "C"
wireframe = "F"
pause = "P"

[ai]
weight_max_height = 5.0
weight_agg_height = 0.5
weight_holes = 50.0
weight_bumpiness = 3.0
```

## AI planner

The auto-play AI (`game_ai.cpp`) works as a one-ply search:

1. Build a flat occupancy array from the current well state.
2. For each of the 24 cube-rotation-group orientations × every (x, z) spawn position: drop the piece, simulate the resulting board.
3. Score the board using heuristic weights from `config.toml` (`[ai]` section): `max_height × weight_max_height + agg_height × weight_agg_height + holes × weight_holes + bumpiness × weight_bumpiness`. 1-ply lookahead evaluates the next piece's best placement.
4. Pick the minimum-score candidate. Emit rotation steps (matching `rotation.h` matrices) then translation steps then a hard-drop command.

The rotation matrices in `rotation.h` are a single shared source for both the game and the AI, eliminating plan/execution mismatch.

## License

Personal project, no licence assigned. Feel free to read and learn from the code.
