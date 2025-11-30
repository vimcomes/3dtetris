TODO / session notes (2025-11-29)

Current status
- Branch: iso-pane (default on origin); main also exists but not primary.
- Last session log: ~/.codex/sessions/2025/11/29/rollout-2025-11-29T17-12-06-019ad02b-dce7-7042-bca0-b5ae850db0f9.jsonl
- Build: iso-pane builds successfully; clang-tidy clean on our code when run with include paths.

Done recently
- UI: Right column split into Controls (top) and Iso View (bottom). Old inset removed from main viewport; iso viewport is square, centered, own glViewport/scissor.
- Cameras: Iso camera centered/raised so top of well is visible; main camera controls intact.
- Rendering: Added light outlines for locked blocks in both views; iso active piece opaque; labels for well size in ASCII.
- Pieces/logic: O piece is a 2x2x2 cube. Plane clearing implemented (clear_full_planes) with cache rebuild; spawn accounts for piece height. Debug fill/clear buttons removed after AI work.
- Auto play: Inline AI removed; app.cpp now uses game_ai.h/game_ai.cpp with compute_plan planner (rot/pos search, scoring with line bonus, hole/bumpiness/height penalties). Plan executes stepwise on checkbox toggle.
- Cleanup: Removed unused brightness/color boost controls and code; last_mouse_y unused var removed. CMake sets FETCHCONTENT_FULLY_DISCONNECTED to avoid network fetches.

Next steps toward a full game
- Gameplay: Proper game over (top-out), scoring/levels with speed ramp, 7-bag piece generator, next queue (3–5), hold, soft/sonic drop scoring, pause/restart.
- UX: Configurable keybinds and mouse sensitivity, vsync/FPS cap toggle, windowed/fullscreen switch, concise HUD (score/level/cleared), pause/game-over screens, optional iso window toggle for low-end GPUs.
- Audio: SFX for rotate/drop/clear, music, volume sliders.
- Visual polish: Clear/spawn animations, themes/palettes, optional transparency/outline tuning.
- Stability/tests: Unit tests for movement/rotation/clears/generator; simple AI sanity tests; CI build + clang-tidy/format.
- Packaging: Build presets for Win/macOS/Linux; consider vendoring deps instead of FetchContent for release.

2025-11-30 session notes
- Session log: ~/.codex/sessions/2025/11/30/rollout-2025-11-30T19-04-46-019ad5b9-5cb4-74a1-8c28-af4efc9981e0.jsonl (this session)
- Rendering: Shaders/palette moved to render.cpp/h; fragment shader outputs color directly (no pow), sRGB enabled. ImGui backgrounds transparent to keep GL clear black. Active piece wireframe draws in white; active piece alpha now 1.0.
- Config: Added config.toml loader; palette, shapes, well size, fall interval come from file (with defaults/fallback). Paths checked from build dir too.
- Pieces/logic: Added dot (1x1x1) and short bar (1x1x2) shapes; default well 6x6x20, width/depth clamped to even 6..10. Gravity interval configurable.
- Cameras/UI: Auto-framing for main/iso viewports on resize and size changes. Keyboard works on hover/focus without initial click. Iso view hides near walls to see inside.
