# MeteoLCD Phase B Weather Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Nahradit destruktivní blokující ČHMÚ obnovu validovanou `live/staging` transakcí, která při jakékoli chybě zachová poslední platnou animaci.

**Architecture:** Čistý streaming parser vytváří fixed-capacity seznam kandidátů. Transport pouze přenáší a validuje bajty; decoder vytváří oříznuté RGB565 rámce; `WeatherFrameStore` je jediným vlastníkem zveřejněných pixelů. `WeatherRepository` řídí transakci, freshness a backoff.

**Tech Stack:** C++11 host testy, Arduino `HTTPClient`/`WiFiClientSecure`, PNGdec 1.0.1, explicitní ESP32 PSRAM alokace.

## Global Constraints

- **Required branch: `feature/level1-phase-b`**.
- Before every task, verify the exact branch with `git branch --show-current`.
- Never commit a phase task directly to `main` or to another phase branch.
- If the branch is missing, create it only from the verified `main` start point
  stated in the master Branch Contract and publish it with `git push -u origin`.
- Fáze A musí být dokončená a committed.
- `CHMU_INDEX_URL`, geografické hranice a maska dat zůstávají podle funkčního kódu.
- Maximum je šest rámců a maximálně 131 072 compressed bajtů na PNG.
- `live` sada se mění jen úspěšným `commitStaging()`.
- Nezavádět FreeRTOS task v této fázi; nejprve měřit synchronní state machine.
- Každý task končí jedním commitem a čistým worktree.

---

### Task B1: Streaming parser ČHMÚ katalogu

**Files:**
- Create: `src/ChmuCatalogParser.h`
- Create: `src/ChmuCatalogParser.cpp`
- Create: `tests/host/test_chmu_catalog.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: raw HTML bytes in arbitrary chunk boundaries.
- Produces:

```cpp
struct ChmuFrameRef { char filename[48]; int64_t utcEpochSeconds; };
class ChmuCatalogParser {
 public:
  ChmuCatalogParser();
  void reset();
  void push(const uint8_t* data, size_t size);
  uint8_t count() const;
  const ChmuFrameRef& frame(uint8_t oldestFirstIndex) const;
};
bool ParseChmuFilename(const char* name, ChmuFrameRef* out);
```

- [ ] **Step 1: Write failing filename and streaming tests**

Cover valid leap-day/time, invalid prefix, invalid month/day/hour/minute, duplicate link, out-of-order HTML, more than six files and a filename split at every possible byte boundary. Core assertions:

```cpp
ChmuFrameRef ref = {};
CHECK(ParseChmuFilename("pacz2gmaps3.z_max3d.20260808.1430.0.png", &ref));
CHECK_EQ(ref.utcEpochSeconds, INT64_C(1786199400));
CHECK(!ParseChmuFilename("pacz2gmaps3.z_max3d.20261308.1430.0.png", &ref));

ChmuCatalogParser parser;
const char html[] = "x pacz2gmaps3.z_max3d.20260808.1425.0.png y "
                    "pacz2gmaps3.z_max3d.20260808.1430.0.png";
for (size_t i = 0; i < sizeof(html) - 1; ++i) parser.push(
    reinterpret_cast<const uint8_t*>(html + i), 1);
CHECK_EQ(parser.count(), 2);
CHECK(parser.frame(0).utcEpochSeconds < parser.frame(1).utcEpochSeconds);
```

- [ ] **Step 2: Run CTest and observe missing-symbol failure**

```powershell
cmake -S tests/host -B build/host
cmake --build build/host --config Release
ctest --test-dir build/host -C Release --output-on-failure
```

- [ ] **Step 3: Implement fixed-capacity parser**

Use a 48-byte rolling candidate buffer, validate the exact 44-character filename form, Gregorian date including leap years, and convert UTC with a timezone-independent civil-date algorithm. Keep only six newest unique frames and expose them oldest-first. Do not use `String`, regex, locale time conversion or heap allocation.

- [ ] **Step 4: Run host and Arduino verification**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Expected: all pass.

- [ ] **Step 5: Commit the parser**

```powershell
git add src/ChmuCatalogParser.* tests/host/test_chmu_catalog.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "feat: add bounded CHMU catalog parser"
git status --short
```

### Task B2: Atomic WeatherFrameStore ownership

**Files:**
- Create: `src/WeatherTypes.h`
- Create: `src/WeatherFrameStore.h`
- Create: `src/WeatherFrameStore.cpp`
- Create: `tests/host/test_weather_frame_store.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: complete externally allocated `WeatherFrame` with compressed PNG, decoded RGB565 pixels, coverage and release callback.
- Produces: master-plan `WeatherFrameSet`/`WeatherFrameStore` plus:

```cpp
using ReleaseFrameFn = void (*)(WeatherFrame* frame, void* context);
explicit WeatherFrameStore(ReleaseFrameFn releaseFrame, void* context);
uint32_t liveGeneration() const;
```

- [ ] **Step 1: Write ownership/failure tests**

Use fake integer-backed compressed/pixel pointers and a release recorder. Test empty live, commit generation 1, begin second staging, abort without changing live, duplicate slot rejection, incomplete commit rejection, replacement releasing both blocks of old live exactly once, and destructor/explicit clear releasing every owned frame exactly once.

```cpp
CHECK(store.beginStaging(2));
CHECK(store.putStaging(0, &frame0));
CHECK(frame0.compressed == nullptr && frame0.pixels == nullptr);
CHECK(!store.commitStaging());
CHECK(store.putStaging(1, &frame1));
CHECK(store.commitStaging());
const WeatherFrameSet* first = store.live();
CHECK_EQ(first->count, 2);
CHECK_EQ(first->generation, 1u);
```

- [ ] **Step 2: Run and observe missing store failure**

Run focused CTest target; expected compile failure.

- [ ] **Step 3: Implement transactional ownership**

Keep `live_` and `staging_` as value members with six fixed slots and occupied bits. `beginStaging()` first aborts only an existing staging set; it never touches live. `putStaging()` moves the entire frame and zeros both caller pointers only on success. On abort, release both compressed and decoded staging blocks. `commitStaging()` succeeds only when all expected indices are occupied, increments generation, swaps sets and releases the former live after the swap. Zero every released or moved frame.

- [ ] **Step 4: Run full verification including sanitizer where available**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1 -SkipArduino
arduino-cli compile --profile default --output-dir build/arduino src
```

Expected: no double release and all builds pass.

- [ ] **Step 5: Commit the store**

```powershell
git add src/WeatherTypes.h src/WeatherFrameStore.* tests/host/test_weather_frame_store.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "feat: add atomic weather frame store"
git status --short
```

### Task B3: Bounded HTTP transport and PNG envelope validation

**Files:**
- Create: `src/ChmuTransport.h`
- Create: `src/ChmuTransport.cpp`
- Create: `tests/host/test_download_validation.cpp`
- Create: `src/DownloadValidation.h`
- Create: `src/DownloadValidation.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: catalog URL or PNG URL, caller-owned fixed buffer for PNG, parser reference for catalog and optional poll callback.
- Produces:

```cpp
enum class DownloadError : uint8_t {
  None, NotConnected, HttpStatus, TooLarge, LengthMismatch,
  Timeout, ReadFailed, InvalidPng
};
struct DownloadResult { DownloadError error; int httpStatus; size_t bytes; };
bool ValidatePngEnvelope(const uint8_t* data, size_t bytes);
class ChmuTransport {
 public:
  void setPollFn(void (*fn)());
  DownloadResult fetchCatalog(const char* url, ChmuCatalogParser* parser,
                              uint32_t idleTimeoutMs, uint32_t totalTimeoutMs);
  DownloadResult downloadPng(const char* url, uint8_t* out, size_t capacity,
                             uint32_t idleTimeoutMs, uint32_t totalTimeoutMs);
};
```

- [ ] **Step 1: Write failing pure validation tests**

Test exact eight-byte PNG signature, minimum IHDR envelope, zero bytes, one-byte truncations, oversize declaration, known length mismatch and unknown/chunked length with successful termination. The pure helper must treat declared length as an optional signed value and require equality when non-negative.

- [ ] **Step 2: Run tests and observe failure**

Expected: missing validation symbols.

- [ ] **Step 3: Implement pure validation then Arduino transport**

Both methods in `ChmuTransport.cpp` must:

- use HTTPS and preserve current `setInsecure()` for Level 1 compatibility;
- require HTTP 200;
- reject positive `Content-Length > capacity` before reading;
- read chunks until EOF with separate no-progress and total timeout;
- never write past capacity;
- require actual bytes equal declared length when present;
- call poll callback between reads but never use it to extend the total deadline;
- stream catalog chunks directly into `ChmuCatalogParser::push()` without building a growing `String`;
- validate the PNG envelope only in `downloadPng()`;
- call `http.end()` on every exit path.

- [ ] **Step 4: Verify with a local failure-injection HTTP fixture and Arduino build**

Add fixture cases to the host validation target; hardware integration later supplies actual HTTPS. Run full verify. Expected all pass.

- [ ] **Step 5: Commit transport validation**

```powershell
git add src/ChmuTransport.* src/DownloadValidation.* tests/host/test_download_validation.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "feat: validate bounded CHMU downloads"
git status --short
```

### Task B4: Čistý Web Mercator Viewport pro crop i renderer

**Files:**
- Create: `src/Viewport.h`
- Create: `src/Viewport.cpp`
- Create: `tests/host/test_viewport.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: geographic center, range and screen/source dimensions.
- Produces: `GeoPoint`, `PixelPoint`, `ViewportSpec`, `Viewport` contract from the master plan plus `panPixels(dx,dy)` and `zoomAt(point,factor)` returning a new `ViewportSpec`.

- [ ] **Step 1: Write failing projection tests**

Test center maps to `(240,240)`, project/unproject round-trip error `< 1e-5°`, north has smaller y, latitude clamps to Web Mercator ±85.05112878°, pan direction and zoom preserving the geographic point under the tapped pixel.

```cpp
Viewport v({{49.1951, 16.6068}, 50.0f, 480, 480});
PixelPoint center = v.project({49.1951, 16.6068});
CHECK_EQ(center.x, 240); CHECK_EQ(center.y, 240);
GeoPoint roundTrip = v.unproject(v.project({50.0755, 14.4378}));
CHECK(std::fabs(roundTrip.lat - 50.0755) < 1e-5);
CHECK(std::fabs(roundTrip.lon - 14.4378) < 1e-5);
```

- [ ] **Step 2: Run CTest and observe missing Viewport failure**

- [ ] **Step 3: Implement the single projection authority**

Use Earth radius 6 378 137 m and standard spherical Web Mercator. Define `rangeKm` as center-to-top visible distance. `buildSourceMaps()` converts screen columns/rows to source pixel indices once per viewport/source geometry and uses `-1` for out-of-source coordinates. Do not depend on Arduino APIs.

- [ ] **Step 4: Run full verification and commit**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/Viewport.* tests/host/test_viewport.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "feat: add shared Web Mercator viewport"
git status --short
```

### Task B5: WeatherDecoder with explicit PSRAM ownership

**Files:**
- Create: `src/WeatherDecoder.h`
- Create: `src/WeatherDecoder.cpp`
- Create: `tests/host/test_weather_crop.cpp`
- Create: `src/WeatherCrop.h`
- Create: `src/WeatherCrop.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: validated compressed PNG, target `ViewportSpec`, ČHMÚ image/data bounds.
- Produces:

```cpp
struct WeatherCrop { uint16_t x, y, width, height; };
bool ComputeWeatherCrop(const ViewportSpec& viewport, uint16_t pngWidth,
                        uint16_t pngHeight, WeatherCrop* out);
enum class DecodeError : uint8_t { None, InvalidInput, OutOfCoverage, NoPsram, PngError };
struct DecodedCrop {
  DecodeError error;
  uint16_t* pixels;
  uint16_t width;
  uint16_t height;
  WeatherCoverage coverage;
};
DecodedCrop DecodeWeatherPng(const uint8_t* png, size_t bytes,
                             const ViewportSpec& viewport);
void ReleaseWeatherFrame(WeatherFrame* frame, void* context);
```

- [ ] **Step 1: Write pure crop tests**

Cover Prague/Brno and 25/50/100/200km viewports, crop within source bounds, out-of-coverage, non-zero dimensions and monotonic growth with range. Use expected coordinates calculated from the same documented Web Mercator equations, not copied output from production.

- [ ] **Step 2: Observe failure, then implement `WeatherCrop` pure math**

Use double precision for geographic math and clamp only after detecting completely out-of-coverage views. Run CTest until pass.

- [ ] **Step 3: Move PNGdec callbacks behind `WeatherDecoder`**

Allocate exactly `crop.width * crop.height * sizeof(uint16_t)` with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`. Return `NoPsram` on failure; do not call ordinary `malloc`. Mask title/right legend regions using existing CHMU data bounds and return exact geographic coverage of the decoded crop. On every PNGdec failure free the pixel allocation before return. The repository, not the decoder, owns and pairs the already validated compressed block with this result.

- [ ] **Step 4: Verify corrupt/truncated fixture and real-device decode**

The repository test fixture must include a small legally generated PNG, its one-byte truncation and corrupted signature. Expected: valid decode succeeds on device test harness, both corrupt cases fail and leave PSRAM unchanged after release. Run full verify.

- [ ] **Step 5: Commit decoder boundary**

```powershell
git add src/WeatherDecoder.* src/WeatherCrop.* tests/host/test_weather_crop.cpp tests/fixtures tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "feat: isolate CHMU crop and PSRAM decoding"
git status --short
```

### Task B6: WeatherRepository transaction, freshness and backoff

**Files:**
- Create: `src/WeatherRepository.h`
- Create: `src/WeatherRepository.cpp`
- Create: `tests/host/test_weather_refresh.cpp`
- Create: `src/RefreshPolicy.h`
- Create: `src/RefreshPolicy.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: parser, transport, decoder and store.
- Produces:

```cpp
enum class RefreshState : uint8_t { Idle, FetchCatalog, FetchFrames, DecodeFrames, Publish, Backoff };
struct WeatherStatus {
  RefreshState state;
  WeatherFreshness freshness;
  DownloadError lastDownloadError;
  DecodeError lastDecodeError;
  int64_t newestFrameUtc;
  uint32_t liveGeneration;
};
class WeatherRepository {
 public:
  void requestRefresh(uint32_t nowMs);
  bool requestViewportRebuild(const ViewportSpec& viewport);
  bool tick(uint32_t nowMs);
  const WeatherFrameSet* live() const;
  WeatherStatus status(uint32_t nowMs) const;
};
```

- [ ] **Step 1: Write fake-driven transaction tests**

Fakes must script catalog success/failure, per-frame download/decode failure and allocation failure. Assert that generation changes only when all required new frames are valid, old live pointer remains stable on failure, unchanged filenames copy validated compressed live bytes instead of downloading, viewport rebuild performs zero network calls, rebuild failure preserves old coverage, freshness becomes `Stale` without deleting data, and backoff sequence is 15s, 30s, 60s, 120s, capped at 300s.

- [ ] **Step 2: Run and observe missing repository failure**

Run focused host target.

- [ ] **Step 3: Implement one-short-step-per-tick state machine**

Each `tick()` advances at most one catalog request result, one frame transfer result, one decode or one publish transition. A synchronous TLS call may still block and will be measured in Phase E, but repository state and ownership remain deterministic. Refresh cadence is 300 000 ms after success. Prefer newest catalog set over retrying an obsolete missing file. Use one reusable 131 072-byte PSRAM download scratch; after validation allocate the frame's actual compressed byte length, copy into it and decode from that owned block. Before refresh or viewport rebuild, calculate the worst staging budget; on failure remain on live without partially consuming it.

- [ ] **Step 4: Run all failure cases and Arduino compile**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Expected: all pass, including store release counts.

- [ ] **Step 5: Commit repository state machine**

```powershell
git add src/WeatherRepository.* src/RefreshPolicy.* tests/host/test_weather_refresh.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "feat: add transactional weather refresh state machine"
git status --short
```

### Task B7: Integrate repository and retire legacy CHMU ownership

**Files:**
- Modify: `src/ScreenWeather.cpp`
- Modify: `src/ScreenWeather.h`
- Modify: `src/MeteoPlaneRadar.ino`
- Delete or reduce to constants/compatibility: `src/CHMU.cpp`
- Modify: `src/CHMU.h`
- Create: `docs/verification/weather-failure-matrix.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`

**Interfaces:**
- Consumes: `WeatherRepository::tick/live/status`.
- Produces: screen animation reads only immutable live set; no `CHMU_FetchLatest`, global six-slot owner or destructive `loadAndBuild()` path.

- [ ] **Step 1: Add static guards for legacy APIs**

Make `tools/verify.ps1` fail on `CHMU_FetchLatest`, `CHMU_FetchAnim`, `CHMU_AnimData`, `CHMU_AnimSize`, `s_frame565` or `loadAndBuild` after integration. Run before changes and observe failure.

- [ ] **Step 2: Connect repository to setup/tick/draw**

Initialize one application-owned repository after settings. `ScreenWeather_Tick()` requests refresh by cadence and advances animation independently. `ScreenWeather_Draw()` snapshots the live set pointer/generation once per draw and never reads staging. Status text distinguishes `Loading`, `Refreshing`, `Stale`, `Offline` and `No data`.

- [ ] **Step 3: Remove legacy storage and dead single-frame API**

Keep CHMU geographic constants in a focused header if still used; delete old compressed global slots and APIs. Do not combine renderer optimization from Phase C.

- [ ] **Step 4: Execute failure matrix on hardware**

Record actual generation/time/UI behavior for: catalog offline, failure at PNG 1/3/6, truncated response, corrupt PNG, Wi-Fi loss, PSRAM allocation failure hook and recovery. Every row must say old generation preserved `yes`.

- [ ] **Step 5: Run verification and commit Phase B**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add -A src tests tools/verify.ps1 docs/verification/weather-failure-matrix.md docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md
git commit -m "refactor: adopt atomic CHMU weather repository"
git status --short
```

Expected: empty status and functioning old renderer fed by new immutable live data. Proceed to Phase C.

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
   git merge --no-ff feature/level1-phase-b
   ```

   Stop without merging if either check fails.
6. Re-run the phase verification on the merge result.
7. Push `main` normally, verify local and `origin/main` object IDs match, and
   only then create the next phase branch from that `main`.

Do not delete the phase branch until the merge commit and remote `main` have
been verified. Never use force-push to repair a failed gate.
