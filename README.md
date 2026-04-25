# 3D Tetris

A fully 3D take on Blockout-style tetris written in C++20 with OpenGL 3.3. Pieces fall through a 3D well, rotate freely around all three axes, and you clear entire planes instead of lines. Built from scratch as a personal project to explore 3D game logic, real-time rendering, and AI search in an interactive context.

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)
![C++](https://img.shields.io/badge/C%2B%2B-20-informational)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-orange)

## Features

- **True 3D rotation** — pieces use integer 3×3 rotation matrices (SO(3) discrete subgroup, 24 unique orientations). No gimbal lock, no drift.
- **Blockout presets** — supports data-driven shape sets loaded from `.dat` files matching the original Blockout format. Ships with basic, advanced, and expert sets.
- **Dual viewport** — main perspective view with orbit/zoom controls + a fixed isometric mini-view in the sidebar.
- **AI auto-play** — brute-force planner enumerates all 24 orientations × every board position, scores each placement with a heuristic (holes, aggregate height, bumpiness, plane clears), and executes the best plan step by step.
- **Neon visual style** — gradient background, emissive glow on pieces (sRGB framebuffer saturation), line-clear flash animation, ghost piece wireframe.
- **HUD + next-piece preview** — score/level/lines overlay on the viewport, isometric next-piece preview in the sidebar.
- **7-bag randomiser** — pieces are drawn from a shuffled bag of all shapes, guaranteeing even distribution.
- **Game state machine** — Playing / Paused (P) / Game Over with restart, proper top-out detection.
- **Configurable** — well dimensions (3–7 × 3–7 × 5–20), start level, fall speed, colour palette, shape set — all in `config.toml`.

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
| P | Pause / resume |
| F | Toggle wireframe on active piece |
| LMB drag | Orbit main camera (yaw) |
| RMB drag | Tilt camera (pitch); RMB click resets view |
| Mouse wheel | Zoom |
| Esc | Quit |

## Project structure

```
src/
  app.cpp        — main loop, rendering, input, ImGui layout (~1500 lines)
  game.cpp/h     — game logic: well, piece physics, rotation, line clears
  game_ai.cpp/h  — AI planner: orientation enumeration, heuristic search
  render.cpp/h   — GLSL shaders (geometry + gradient), palette
  config.cpp/h   — TOML config loader, shape definitions
  geometry.h     — mesh builders: cubes, edges, floor grid, well walls
  math.h         — linear algebra (Vec3, Mat4, transforms)
  shader.h       — GLSL compile/link helpers
data/
  forms_blockout.dat  — Blockout shape definitions (centres + bitmaps)
config.toml            — runtime configuration
```

## Configuration

`config.toml` controls everything without recompiling:

```toml
[preset]
name = "blockout"       # "modern" | "blockout"
blockout_set = "basic"  # "basic" | "advanced" | "expert"

[well]
width  = 6
depth  = 6
height = 20

[gameplay]
fall_interval = 6.2     # seconds per cell at level 0
start_level   = 2

[render.palette]
clear   = [0.03, 0.02, 0.08]
grid    = [0.10, 0.28, 0.45]
outline = [0.55, 0.65, 0.75]
```

## AI planner

The auto-play AI (`game_ai.cpp`) works as a one-ply search:

1. Build a flat occupancy array from the current well state.
2. For each of the 24 cube-rotation-group orientations × every (x, z) spawn position: drop the piece, simulate the resulting board.
3. Score the board: `max_height × 5 + agg_height × 0.5 + holes × 50 + bumpiness × 3 − full_planes × 800`.
4. Pick the minimum-score candidate. Emit rotation steps (matching the game's own rotation matrices) then translation steps then a hard-drop command.

The rotation matrices in the AI mirror those in `game.cpp` exactly — same `ROT_X_POS / ROT_Y_POS / ROT_Z_POS` constants — so there is no plan/execution mismatch.

## License

Personal project, no licence assigned. Feel free to read and learn from the code.
