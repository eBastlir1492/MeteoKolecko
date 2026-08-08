# MeteoLCD Phase C Renderer and LCD UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Zavést společný viewport, reprodukovatelný mapový asset, přímý RGB565 renderer, bezpečný frame present a adaptivní gesture-first LCD UI.

**Architecture:** Čisté mapové/renderovací moduly pracují nad `RenderSurface565` a testují se na hostu. Hardware `DisplayBackend` pouze získává/presentuje panely vlastněné buffery. Mapa, radar, labels a overlay jsou oddělené vrstvy sdílející jediný `Viewport`.

**Tech Stack:** C++11, Python 3 standard library asset/golden tools, Arduino GFX pouze pro zbývající text/primitiva, současný esp_lcd RGB driver.

## Global Constraints

- **Required branch: `feature/level1-phase-c`**.
- Before every task, verify the exact branch with `git branch --show-current`.
- Never commit a phase task directly to `main` or to another phase branch.
- If the branch is missing, create it only from the verified `main` start point
  stated in the master Branch Contract and publish it with `git push -u origin`.
- Fáze A a B musí být dokončené.
- Neměnit RGB piny, timing, framebuffer count, bounce height ani ST7701 init commands.
- Výstup je 480×480 RGB565; kruhová viditelná oblast musí zůstat bounds-safe.
- Žádná alokace při přehrávání animace.
- Labely nesmí měnit polohu mezi weather rámci pro stejný viewport/UI layout.
- Touch zůstává single-touch; žádný pinch.
- Každý task končí jedním commitem a čistým worktree.

---

### Task C1: Reprodukovatelný Level 1 mapový asset

**Files:**
- Create: `assets/maps/cz-level1.json`
- Create: `tools/generate_map_asset.py`
- Create: `tests/test_map_generator.py`
- Create: `src/MapAsset.h`
- Create: `src/MapAsset.generated.h`
- Modify: `tools/verify.ps1`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`

**Interfaces:**
- Consumes: všech 50 bodů `CZ_BORDER` a všech 59 položek `CZ_CITIES` z posledního committed `CzBorder.cpp`, přesunutých beze změny souřadnic/významu.
- Produces:

```cpp
struct MapPointQ { int16_t x; int16_t y; };
struct MapCity {
  int32_t latE5;
  int32_t lonE5;
  uint8_t priority;
  const char* fullName;
  const char* shortName;
};
extern const MapPointQ kCzBorderLod0[];
extern const MapPointQ kCzBorderLod1[];
extern const MapCity kCzCities[];
```

- [ ] **Step 1: Write failing generator tests**

The Python unittest loads JSON and asserts exactly 50 closed border points, 59 unique cities, valid lat/lon, priority 1 or 2, nonempty full/short names, and Prague/Brno/Ostrava coordinates matching the current code. Run generator twice to temporary files and assert byte-for-byte equality.

- [ ] **Step 2: Run and observe missing asset/generator failure**

```powershell
python -m unittest tests.test_map_generator -v
```

- [ ] **Step 3: Move current data into audit source and implement generator**

`cz-level1.json` records `source: "MeteoLCD CzBorder.cpp baseline"`, coordinate order and license note. Quantize border relative to fixed Web Mercator bounds using signed 16-bit values. Generate two Douglas-Peucker LODs with fixed tolerances and emit deterministic arrays plus string literals. Full Level 1 names stay ASCII; use natural short names such as `C. Budejovice`, never unexplained internal codes.

- [ ] **Step 4: Make generated-file drift fail verification**

`tools/verify.ps1` generates `build/MapAsset.generated.h` and compares it byte-for-byte with committed `src/MapAsset.generated.h`; mismatch throws with the exact regeneration command.

- [ ] **Step 5: Verify and commit map pipeline**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add assets/maps/cz-level1.json tools/generate_map_asset.py tests/test_map_generator.py src/MapAsset* tools/verify.ps1 docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md
git commit -m "feat: generate deterministic Level 1 map asset"
git status --short
```

### Task C2: RenderSurface565 a přímé radarové řádky

**Files:**
- Create: `src/RenderSurface565.h`
- Create: `src/WeatherRenderer.h`
- Create: `src/WeatherRenderer.cpp`
- Create: `tests/host/test_weather_renderer.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`

**Interfaces:**
- Consumes: immutable `WeatherFrame`, `Viewport`, precomputed source maps.
- Produces:

```cpp
struct RenderSurface565 {
  uint16_t* pixels;
  int16_t width;
  int16_t height;
  int16_t stride;
};
struct CircleClip { int16_t cx; int16_t cy; int16_t radius; };
void ClearSurface(RenderSurface565 dst, uint16_t color);
void DrawMapLayer(RenderSurface565 dst, CircleClip clip,
                  const Viewport& viewport);
void DrawRadarLayer(RenderSurface565 dst, CircleClip clip,
                    const WeatherFrame& frame,
                    const int16_t* sourceX, const int16_t* sourceY);
```

- [ ] **Step 1: Write failing pixel-exact tests**

Use a 4×4 source with unique RGB565 values and an 8×8 destination surrounded by canary words. Verify nearest-neighbor mapping, `-1` source indices preserve the existing map pixel, black/no-echo source pixels are transparent, circle corners remain black and canaries never change.

- [ ] **Step 2: Observe missing renderer failure**

- [ ] **Step 3: Implement bounds-safe row rendering**

For each destination row, derive the circle x interval once from `r²-dy²`, fetch `sourceY[y]` once, walk contiguous destination pixels and use `sourceX[x]`. Skip out-of-coverage and black/no-echo source pixels so the map base remains visible. Do not call Arduino GFX, divide, allocate or mutate the source frame inside the loop.

- [ ] **Step 4: Benchmark against current renderer on device**

Measure old/new compose time for 25/50/100/200km across at least 100 frames. Record median/p95/worst in `docs/verification/renderer-benchmark.md`. New path must not be slower at any range and target p95 below 25 ms; if not, retain evidence and optimize before completion.

- [ ] **Step 5: Verify and commit direct renderer**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/RenderSurface565.h src/WeatherRenderer.* tests/host/test_weather_renderer.cpp tests/host/CMakeLists.txt docs/verification/renderer-benchmark.md docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md
git commit -m "perf: render radar directly into RGB565 rows"
git status --short
```

### Task C3: Collision-aware city label layout

**Files:**
- Create: `src/CityLabelLayout.h`
- Create: `src/CityLabelLayout.cpp`
- Create: `tests/host/test_city_labels.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/WeatherRenderer.cpp`
- Modify: `src/ScreenWeather.cpp`
- Modify: `src/ScreenWeather.h`
- Delete: `src/CzBorder.cpp`
- Delete: `src/CzBorder.h`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`

**Interfaces:**
- Consumes: projected `MapCity`, viewport range, fixed text measurement callback and rectangles reserved by UI.
- Produces:

```cpp
struct RectI16 { int16_t x, y, width, height; };
struct CityLabelPlacement {
  uint16_t cityIndex;
  int16_t dotX, dotY;
  int16_t textX, textY;
  const char* text;
  RectI16 bounds;
};
uint8_t LayoutCityLabels(const MapCity* cities, uint16_t cityCount,
                         const Viewport& viewport, CircleClip clip,
                         const RectI16* reserved, uint8_t reservedCount,
                         CityLabelPlacement* out, uint8_t outCapacity);
```

- [ ] **Step 1: Write failing priority/collision tests**

Assert high priority wins, no output rectangles overlap, no label crosses circle/screen bounds, reserved overlay region is avoided, same inputs return byte-identical placements, 200km chooses fewer labels than 25km, and natural short/full name policy contains no `PHA`, `OVA`, `LBC`, `PCE` default labels.

- [ ] **Step 2: Run and observe missing layout failure**

- [ ] **Step 3: Implement deterministic candidates**

Sort by priority then stable source index. Try right, left, above-right, above-left, below-right, below-left. Use full name at ≤50km; beyond it prefer natural short name but omit a low-priority city instead of using an unexplained code. Reserve dot + two-pixel text halo. Use fixed output arrays only.

- [ ] **Step 4: Integrate the map/radar/label layer order and retire CzBorder**

`ScreenWeather` acquires the back surface, clears black, calls `DrawMapLayer`, overlays transparent-no-echo radar, then draws border emphasis, city dots/labels and UI. Draw a dark one-pixel halo before cyan/white ASCII text. Layout is computed only on viewport/UI geometry change, not per animation frame. Replace all `CzBorder_*` calls with generated `MapAsset` use, delete `CzBorder.cpp/.h`, and add a forbidden-symbol verification guard.

- [ ] **Step 5: Verify and commit labels**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add -A src/CityLabelLayout.* src/WeatherRenderer.cpp src/ScreenWeather.* src/CzBorder.cpp src/CzBorder.h tests/host/test_city_labels.cpp tests/host/CMakeLists.txt tools/verify.ps1 docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md
git commit -m "feat: place readable collision-free city labels"
git status --short
```

### Task C4: Potvrzené vlastnictví framebufferu a VSYNC timeout

**Files:**
- Create: `src/FrameBufferState.h`
- Create: `src/FrameBufferState.cpp`
- Create: `src/DisplayBackend.h`
- Create: `src/DisplayBackend.cpp`
- Create: `tests/host/test_framebuffer_state.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/Display_ST7701.h`
- Modify: `src/Display_ST7701.cpp`
- Modify: `src/Canvas16.h`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`

**Interfaces:**
- Consumes: two pointers from `LCD_FrameBuffer(0/1)` and VSYNC result.
- Produces: master-plan `DisplayBackend`; low-level `bool LCD_Flush(const uint16_t* fb, uint32_t timeoutMs)`.

- [ ] **Step 1: Write failing pure ownership tests**

Assert initial visible/drawing distinction, acquire only free buffer, successful present changes visible, timeout preserves previous visible and drawing ownership, invalid pointer is rejected, repeated acquire without present fails, and recovery reset returns to known state.

- [ ] **Step 2: Run and observe missing state machine failure**

- [ ] **Step 3: Make low-level flush report truth**

Change only return/result handling around existing draw-bitmap/VSYNC wait. Do not change panel creation, timings, bounce buffers or event callback. Drop stale VSYNC event before request as today; return false on timeout.

- [ ] **Step 4: Integrate DisplayBackend and Canvas16**

`Canvas16::flush()` switches `_fbIndex` only after successful confirmed present. On timeout it keeps drawing ownership, increments a counter and lets display watchdog act. All existing `gfx->flush()` call sites remain functional during migration.

- [ ] **Step 5: Verify hardware success and forced timeout**

Add a test-only hook that suppresses present acknowledgement without changing release build. Confirm timeout does not draw into visible buffer and normal VSYNC continues after hook removal. Run full verify.

- [ ] **Step 6: Commit framebuffer safety**

```powershell
git add src/FrameBufferState.* src/DisplayBackend.* src/Display_ST7701.* src/Canvas16.h src/MeteoPlaneRadar.ino tests/host/test_framebuffer_state.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md
git commit -m "fix: preserve framebuffer ownership on VSYNC timeout"
git status --short
```

### Task C5: GestureRecognizer a adaptivní overlay

**Files:**
- Create: `src/GestureRecognizer.h`
- Create: `src/GestureRecognizer.cpp`
- Create: `src/OverlayUi.h`
- Create: `src/OverlayUi.cpp`
- Create: `tests/host/test_gestures.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `src/ScreenWeather.cpp`
- Modify: `src/ScreenWeather.h`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`

**Interfaces:**
- Consumes: filtered single-touch samples and settings overlay timeout.
- Produces: master-plan `GestureEvent`; `OverlayUi::handle(event, nowMs)`, `visible(nowMs)`, `draw(...)`, and reserved label rectangles.

- [ ] **Step 1: Write failing gesture sequences**

Use timestamped samples for tap, double tap ≤350ms/32px, drag start ≥12px, drag moves, release only after 60ms silence, one missing sample inside drag, and no long-press hidden action. Verify a pending single tap is emitted only after the double-tap window expires.

- [ ] **Step 2: Run and observe missing gesture failure**

- [ ] **Step 3: Implement allocation-free gesture FSM**

Preserve physical filtering in `Touch_CST820`; move only interpretation out of `.ino`. Emit at most one event per update and retain queued event state internally.

- [ ] **Step 4: Implement round-safe overlay**

Hidden mode shows frame timestamp and only persistent stale/offline/error badge. Tap reveals top status, bottom timeline, play/pause, home, settings and zoom controls. Touch rectangles are ≥48 px and entirely inside the display circle. Auto-hide default is 6000ms, reset by any interaction; active drag prevents hiding.

- [ ] **Step 5: Connect pan/zoom/timeline behavior**

Drag outside overlay pans viewport immediately within the last decoded coverage; areas outside it show the map without stale stretched radar. On drag release call `WeatherRepository::requestViewportRebuild()` using retained live compressed PNG. Double tap zooms around tap and schedules the same atomic rebuild; timeline drag selects frame; explicit home restores settings location/range. There is no network request solely for pan/zoom, no global frame swipe and no pinch path.

- [ ] **Step 6: Verify and commit gesture UI**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/GestureRecognizer.* src/OverlayUi.* src/MeteoPlaneRadar.ino src/ScreenWeather.* tests/host/test_gestures.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md
git commit -m "feat: add gesture-driven adaptive LCD controls"
git status --short
```

### Task C6: LCD settings redesign, coach marks a 1:1 golden review

**Files:**
- Modify: `src/ScreenSettings.cpp`
- Modify: `src/ScreenSettings.h`
- Create: `src/SettingsMenuModel.h`
- Create: `src/SettingsMenuModel.cpp`
- Create: `tests/host/test_settings_menu.cpp`
- Create: `tools/rgb565_to_png.py`
- Create: `tests/test_rgb565_png.py`
- Create: `tests/golden/manifest.json`
- Create: `tests/golden/*.png`
- Create: `docs/verification/lcd-ui-review.md`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`

**Interfaces:**
- Consumes: AppSettings setters, GestureRecognizer and Overlay UI style.
- Produces: large-row scroll menu and deterministic 480×480 screenshots.

- [ ] **Step 1: Write failing menu geometry tests**

Assert every interactive rectangle is at least 48×48, lies in circle-safe bounds, scroll clamp is deterministic and rows exist for brightness, range, frame count, frame period, end pause, overlay timeout, city/status visibility, home reset, portal, OTA, diagnostics and gesture help.

- [ ] **Step 2: Implement menu model and screen**

Use large rows/cards, one value per row and drag scrolling. Do not implement an on-screen keyboard. Destructive/system operations are visually separated. First boot shows a one-time gesture coach mark; Settings can reopen it.

- [ ] **Step 3: Implement stdlib-only RGB565 PNG writer**

The script writes PNG signature, IHDR, zlib-compressed unfiltered RGB rows and IEND using `struct`, `zlib` and `binascii.crc32`; no Pillow dependency. Unit test a 2×2 known-color image by parsing/decompressing its chunks.

- [ ] **Step 4: Generate and review required 1:1 images**

Manifest cases: 25/50/100/200km; overlay hidden/shown; settings top/scrolled; help; loading; stale; offline; no-data; VSYNC error. Proveďte samostatný UI-design review průchod nad 1:1 obrázky (jiný reviewer než autor daného renderer tasku, pokud je dostupný). Record explicit approval or corrections in `lcd-ui-review.md`; do not approve by filename alone.

- [ ] **Step 5: Run full verification and hardware touch smoke**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

On device verify tap, double-tap, long drag with missing samples, timeline, home, settings scroll and six-second hide.

- [ ] **Step 6: Commit completed renderer/UI phase**

```powershell
git add src/ScreenSettings.* src/SettingsMenuModel.* tests/host/test_settings_menu.cpp tools/rgb565_to_png.py tests/test_rgb565_png.py tests/golden docs/verification/lcd-ui-review.md tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md
git commit -m "feat: complete round-display MeteoLCD UI"
git status --short
```

Expected: empty status and approved 1:1 images. Proceed to Phase D.

## Phase Integration Gate

1. Confirm every task checkbox in this phase is committed.
2. Run the phase's complete verification command and read its full output.
3. Confirm `git status --short` is empty and the current branch is the required
   phase branch.
4. Push the phase branch to `origin`, run `git fetch --prune origin`, record
   `$expectedOriginMain = git rev-parse origin/main`, and review the full
   `origin/main...branch` diff.
5. Switch to `main`, run `git fetch --prune origin` again, then run:

   ```powershell
   if ((git rev-parse origin/main) -ne $expectedOriginMain) {
     throw 'Unexpected remote main changed after review'
   }
   git merge-base --is-ancestor main origin/main
   if ($LASTEXITCODE -ne 0) { throw 'main cannot fast-forward to origin/main' }
   git merge --ff-only origin/main
   git merge --no-ff feature/level1-phase-c
   ```

   Stop without merging if either check fails.
6. Re-run the phase verification on the merge result.
7. Push `main` normally, verify local and `origin/main` object IDs match, and
   only then create the next phase branch from that `main`.

Do not delete the phase branch until the merge commit and remote `main` have
been verified. Never use force-push to repair a failed gate.
