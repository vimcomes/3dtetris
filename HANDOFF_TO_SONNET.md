# Передача задачи Sonnet 4.6 — 3D Tetris

Этот файл — полная спецификация того, что должна сделать следующая сессия.  
Контекст сформирован сессией Opus 4.7 (2026-04-24). Полные находки — в `WORKLOG.md` (раздел "Сессия 2026-04-24 (append, Opus 4.7)").

---

## Общий контекст

- **Проект:** CLion C++20, OpenGL 3.3 + GLFW + ImGui (docking branch) + GLAD. Своя math.h.
- **Сборка:** CMake FetchContent в режиме `DISCONNECTED`. Зависимости уже в `cmake-build-debug/_deps/`.
- **Ветка:** `iso-pane`.
- **НЕ трогать:** `.idea/`, `CMakePresets.json`, `cmake-build-debug/_deps/`.
- **После существенных правок:** запустить `graphify update .` (CLAUDE.md требует, без API-стоимости).
- **Билд:** `cmake --build cmake-build-debug` — после каждого шага проверять что собирается и бинарь `./cmake-build-debug/3dtetris` запускается.
- **Язык ответов:** русский.

---

## Две итерации работы

### Итерация А — "Игра работает как надо" (~2 часа)

Цель: убрать критические баги в геймплее и рендере. Только после этого — красота.

#### A1. Починить AI-планировщик [game_ai.cpp]

**Проблема (подробности в WORKLOG):**
- `rotate_block_local(v, axis, 0)` **не обрабатывает dir==0** — условие `(dir >= 0) ? 1 : -1` превращает 0 в +1, т.е. ноль тоже крутит фигуру.
- Из-за этого 4 из 7 "кандидатов-ориентаций" в `compute_plan()` идентичны, а identity-ориентация (текущая) вообще не рассматривается.
- План-рассинхрон: AI оценивает позицию с 3 поворотами, но плашит 0 шагов поворота → игра роняет фигуру в identity-ориентации, совершенно в другом положении чем оценил AI.

**Задача:**
1. В `rotate_block_local` добавить `if (dir == 0) return v;` в начало.
2. **Полностью переписать** генерацию кандидатов так, чтобы она использовала ту же `mul_rot()` и матрицы `ROT_X_POS/NEG, ROT_Y_POS/NEG, ROT_Z_POS/NEG` что и `Game::rotate_active`. Например:
   - Перебирать (kx, ky, kz) где каждый ∈ {0,1,2,3} — количество поворотов на 90° по оси. Это 64 комбинации, но многие совпадают.
   - Отбрасывать дубликаты через канонизацию матрицы вращения (hash или прямое сравнение).
   - Должно получиться ровно 24 различные ориентации (группа вращений куба).
3. В `compute_plan`, когда пушится план:
   - Вместо `push_rot(Axis::X, best.rx)` пушить реальную последовательность `RotX/RotY/RotZ` шагов, соответствующую выбранной (kx, ky, kz).
4. Проверить: Auto play должен **видимо** выбирать места с дырами/линиями вместо случайных.

**Файлы:** `src/game_ai.cpp`, `src/game_ai.h` (возможно обновить структуру Candidate).

#### A2. Убрать дублирующий рендер [app.cpp]

**Проблема:** Активная фигура и ghost piece рисуются **дважды** в main viewport (строки ~862–924 и ~970–1018). Эффекты: двойное альфа-смешивание (фигура визуально более непрозрачная), лишняя GPU-работа.

**Задача:**
1. Оставить **только второй** (post-locked) render-pass активной фигуры и ghost — чтобы фигура всегда рисовалась поверх locked-блоков.
2. Удалить первый блок (до рендера locked-cells).
3. Убедиться что `glDisable(GL_DEPTH_TEST)` + `glDepthMask(GL_FALSE)` корректно обёрнуты `glEnable` обратно.

**Файл:** `src/app.cpp` (строки ~862–924).

#### A3. Починить spin-анимацию в iso-виде [app.cpp]

**Проблема:** В iso viewport (`~1174-1185`) spin вращает вокруг **world origin**, а не вокруг пивота фигуры. В main viewport это сделано правильно (`translation(pivot) × rotation × translation(-pivot)`), нужно скопировать в iso.

**Задача:** Вычислить пивот (центр masses блоков фигуры) так же как в main, обернуть rotation в translate-back-translate.

**Файл:** `src/app.cpp` (блок `// Active piece` в `Iso View` окне).

#### A4. Game-over state [game.cpp/h, app.cpp]

**Проблема:** При top-out (`!can_place(*active_)` после лока) сейчас молча пересоздаётся well и сбрасывается прогресс — игрок не понимает что проиграл.

**Задача:**
1. В `game.h` добавить:
   ```cpp
   enum class GameState { Playing, Paused, GameOver };
   ```
   и поле `GameState state_ = GameState::Playing;` + метод `GameState state() const`.
2. В `Game::update()` — если `state_ != Playing`, выйти сразу (не обновлять таймеры/позицию).
3. В `try_lock_and_spawn()` — заменить молчаливый reset на `state_ = GameState::GameOver;` (оставить поле, без сброса score).
4. Добавить `Game::restart()` — ресет всего состояния к стартовому.
5. В `app.cpp`:
   - Если `game.state() == GameOver` — отрисовать overlay "GAME OVER" поверх viewport'а (центрированный ImGui window с крупным текстом и кнопкой "Restart").
   - Кнопка Restart вызывает `game.restart()`.
6. Пауза по клавише **P**: toggle между `Playing` и `Paused`. В Paused — отрисовать overlay "PAUSED" но продолжать рендерить сцену.

**Файлы:** `src/game.h`, `src/game.cpp`, `src/app.cpp`.

---

### Итерация Б — "Игра выглядит красиво" (~3-4 часа)

Цель: проект выглядит как законченная неоновая Blockout-демка, UI аккуратный.

Начинать **только** после завершения всех A1–A4 и проверки билда.

#### B1. Визуальная палитра и фон

1. **Gradient-фон** вместо чистого чёрного clear:
   - Большой full-screen quad как первый render-pass в каждом viewport
   - Вертикальный градиент в fragment shader (`uBottom`, `uTop` — два Vec3)
   - Рекомендуемые цвета: низ `#0a0520` (тёмный пурпур), верх `#001028` (тёмный индиго)
2. **Уточнить clear color** в config.toml (`[render.palette] clear = [0.02, 0.01, 0.08]`) и сделать palette настраиваемой через UI.

#### B2. Emissive / glow эффект на активной фигуре

Вариант минимум (без FBO):
- В шейдере добавить uniform `uEmissive` (float 0..2)
- Для активной фигуры `uEmissive=1.3`, для locked `uEmissive=1.0`, для ghost `uEmissive=0.5`
- В fragment: `FragColor = vec4(vColor * uEmissive, uAlpha)` — сверх-яркие значения из-за sRGB выглядят как glow

Вариант максимум (post-process bloom):
- Создать FBO с двумя attachments (color + bright)
- Render-to-texture, два pass gaussian blur на bright, additive композит
- Займёт примерно 150 строк, оправданно если хотим "ах".

**Начать с минимума**, при наличии времени — перейти к bloom.

#### B3. Flash-анимация очистки линий

В `Game::clear_full_planes_range` — вместо мгновенного shift'а:
1. Добавить состояние `std::vector<int> clearing_planes_y_; float clear_anim_t_;`
2. При обнаружении полной плоскости — добавить в clearing_planes_y_, НЕ удалять сразу
3. В `update()` — увеличивать `clear_anim_t_`, когда достигнет 0.25s — выполнить shift и очистить список
4. В рендере: если `y` в clearing_planes_y_ — тинтовать все блоки этой плоскости в белый с альфа пульсацией (`sin(clear_anim_t_ * 30) * 0.5 + 0.5`)

#### B4. Ease-out gravity

В `Game::update()` сейчас `pos_y -= fall_speed * dt` (линейное).  
Заменить на небольшое easing: ближе к целевой клетке скорость чуть замедляется. Альтернатива — плавное "pop" на лок (quick squash-stretch на 80мс при приземлении).

#### B5. Кастомная ImGui тема

В `main()` после `StyleColorsDark()`:
```cpp
ImGuiStyle& style = ImGui::GetStyle();
style.WindowRounding = 8.0f;
style.FrameRounding = 4.0f;
style.GrabRounding = 4.0f;
style.WindowPadding = ImVec2(12, 12);
style.FramePadding = ImVec2(8, 6);
style.ItemSpacing = ImVec2(10, 8);
style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.08f, 0.88f);
style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.35f, 0.45f, 0.9f);
style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.55f, 0.75f, 1.0f);
style.Colors[ImGuiCol_Header] = ImVec4(0.0f, 0.7f, 0.9f, 0.4f);
// ... и т.д., акцент cyan/neon-green
```

**Шрифт:** подгрузить TTF (например Inter, Roboto, или JetBrains Mono) через `io.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Regular.ttf", 18.0f);`. Второй шрифт — крупный для HUD (`size=32`). Файлы положить в `assets/fonts/`.

#### B6. HUD-оверлей поверх viewport

Сейчас Score/Level/Cubes dropped в боковой панели Controls — теряются.  
Добавить отдельное прозрачное ImGui-окно `HUD` поверх main viewport:
- Без рамки (`NoDecoration | NoBackground | NoInputs`)
- В левом верхнем углу viewport'а
- Крупный шрифт, формат:
  ```
  SCORE    12,340
  LEVEL    3
  LINES    12
  ```
- Прогресс-бар под LEVEL: `cubes_dropped_ % 70 / 70`

#### B7. Next-piece preview

В боковой панели Controls:
- Маленький ImGui child window 200×200
- Отдельный GL-рендер (scissor/viewport) с фиксированной 3D-камерой
- Показывает следующую фигуру из очереди (см. B9)

Потребует добавить в `Game` поле `std::optional<Piece> next_piece_` и заполнять его при spawn.

#### B8. Подписи осей в iso-виде

Мелкие цветные тики на полу iso-pane: красный X, синий Z (Y уходит вверх — можно подписать стрелку на задней стенке).

#### B9. 7-bag рандомизатор (бонус)

Вместо `uniform_int_distribution`: хранить `std::vector<int> bag_`. Когда пустой — заполнить перестановкой всех shape-индексов, shuffle. Pop front при спавне. Гарантирует равномерность.

---

## Протокол работы для Sonnet

1. **Перед стартом** — прочитать `WORKLOG.md` целиком.
2. **Делать правки атомарно:** один шаг (A1, A2, ...) — билд — запуск — фикс регрессий — дальше.
3. **Билд:** `cmake --build cmake-build-debug 2>&1 | tail -30`. Если ошибки — **не двигаться дальше** пока не починено.
4. **Визуальная проверка:** user может запустить `./cmake-build-debug/3dtetris` — Sonnet должен запросить скриншот/фидбек после визуальных изменений (B1, B2, B5, B6).
5. **После каждой итерации (А целиком и Б целиком)** — написать краткий лог в `WORKLOG.md` ("что сделано, что не влезло, регрессии").
6. **После B**: запустить `graphify update .` (чтобы граф оставался актуальным).
7. **Не** создавать дополнительных `.md` файлов без запроса.
8. Коммиты — только если user явно попросит. Использовать format с `Co-Authored-By: Claude Sonnet 4.6`.

## Критерии приёмки

**Итерация А готова если:**
- [ ] Билд чистый
- [ ] Auto play перестал дёргать фигуру в случайные позиции, видимо избегает дыр
- [ ] Активная фигура рендерится один раз, прозрачность 0.7 корректная
- [ ] Iso spin крутится вокруг центра фигуры
- [ ] Top-out показывает Game Over, кнопка Restart работает
- [ ] Пауза по P работает, сцена продолжает рендериться

**Итерация Б готова если:**
- [ ] Фон не чёрный, есть градиент
- [ ] Активная фигура заметно ярче locked (emissive)
- [ ] Очистка линий показывает flash-анимацию (не моментальная)
- [ ] ImGui имеет скругления, акцентный цвет, читаемый шрифт
- [ ] HUD Score/Level/Lines отображается крупно поверх viewport'а
- [ ] Превью следующей фигуры работает
- [ ] 7-bag активен (если хватит времени)
