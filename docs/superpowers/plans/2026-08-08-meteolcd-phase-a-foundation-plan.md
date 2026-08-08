# MeteoLCD Phase A Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vytvořit reprodukovatelnou testovací baseline, verzovat nastavení a odstranit aviation část bez změny ověřené LCD/touch pipeline.

**Architecture:** Nejprve se přidá host test runner a jednotný verify příkaz. Čistý `AppSettings` model ochrání migraci legacy NVS; teprve potom se přepne navigace na weather-only a odstraní mrtvé aviation soubory a dependency.

**Tech Stack:** C++11, CMake/CTest, PowerShell, Arduino CLI profil `default`, Preferences, současné pinned Arduino knihovny.

## Global Constraints

- **Required branch: `feature/level1-phase-a`**.
- Before every task, verify the exact branch with `git branch --show-current`.
- Never commit a phase task directly to `main` or to another phase branch.
- If the branch is missing, create it only from the verified `main` start point
  stated in the master Branch Contract and publish it with `git push -u origin`.
- Přečíst master plán a Level 1 design před změnou.
- Neměnit `Display_ST7701*`, `Canvas16.h`, `Touch_CST820*`, `TCA9554*` ani `partitions.csv` v této fázi.
- Namespace NVS zůstává přesně `planeradar`.
- Každý task končí jedním commitem a čistým worktree.
- Pokud nezměněný projekt nelze sestavit, zastavit se s přesným logem; neopravovat současně toolchain a firmware naslepo.

---

### Task A1: Host test harness, verify script a měřená baseline

**Files:**
- Create: `.gitignore`
- Create: `tests/host/CMakeLists.txt`
- Create: `tests/host/TestHarness.h`
- Create: `tests/host/test_smoke.cpp`
- Create: `tools/verify.ps1`
- Create: `docs/verification/level1-baseline.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`

**Interfaces:**
- Consumes: `arduino-cli compile --profile default src` declared by `sketch.yaml`.
- Produces: one command `powershell -ExecutionPolicy Bypass -File tools/verify.ps1`; `CHECK` and `CHECK_EQ` macros for later host tests.

- [ ] **Step 1: Verify prerequisites before editing**

```powershell
arduino-cli version
cmake --version
python --version
git status --short
```

Expected: all commands succeed and Git output is empty. If a tool is absent, install it outside the repository, rerun these commands, and do not edit firmware first.

- [ ] **Step 2: Compile the untouched firmware and capture actual output**

```powershell
New-Item -ItemType Directory -Force build\baseline | Out-Null
arduino-cli compile --profile default --output-dir build\baseline src 2>&1 | Tee-Object build\baseline\compile.txt
```

Expected: exit 0. Copy the reported sketch/flash/RAM numbers and compiler version into `docs/verification/level1-baseline.md`; do not estimate values.

- [ ] **Step 3: Add a dependency-free host harness**

`tests/host/TestHarness.h` must define failure-counting macros with file/line output:

```cpp
#pragma once
#include <cstdio>
#include <cstdlib>

inline int& TestFailures() { static int value = 0; return value; }
#define CHECK(expr) do { if (!(expr)) { \
  std::fprintf(stderr, "%s:%d CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
  ++TestFailures(); } } while (0)
#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))
```

`tests/host/test_smoke.cpp`:

```cpp
#include "TestHarness.h"
int main() {
  CHECK_EQ(480 * 480 * 2, 460800);
  return TestFailures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Každý další `test_*.cpp` má stejný vlastní `main()`/return pattern a je samostatným CTest executable; nepřidávejte skrytý globální test registry mechanismus.

`tests/host/CMakeLists.txt` starts with C++11 and one CTest target:

```cmake
cmake_minimum_required(VERSION 3.16)
project(meteolcd_host_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_executable(test_smoke test_smoke.cpp)
add_test(NAME smoke COMMAND test_smoke)
```

- [ ] **Step 4: Add the single verification entry point**

`tools/verify.ps1` must stop on failure and keep generated output under ignored `build/`:

```powershell
param([switch]$SkipArduino)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
cmake -S tests/host -B build/host
cmake --build build/host --config Release
ctest --test-dir build/host -C Release --output-on-failure
python -m unittest discover -s tests -p 'test_*.py'
if (-not $SkipArduino) {
  arduino-cli compile --profile default --output-dir build/arduino src
}
```

`.gitignore` contains `/build/`, Python cache and editor-local output, not source assets or golden images.

- [ ] **Step 5: Run the new baseline verification**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Expected: smoke PASS, Python reports zero discovered tests without error, Arduino compile exit 0.

- [ ] **Step 6: Commit the baseline checkpoint**

Mark A1 checkboxes complete, then:

```powershell
git add .gitignore tests/host tools/verify.ps1 docs/verification/level1-baseline.md docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md
git commit -m "test: establish MeteoLCD build baseline"
git status --short
```

Expected: commit succeeds; final output empty.

### Task A2: Čistý verzovaný AppSettings model

**Files:**
- Create: `src/AppSettings.h`
- Create: `src/AppSettings.cpp`
- Create: `tests/host/test_app_settings.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`

**Interfaces:**
- Consumes: no Arduino API.
- Produces: `AppView`, `AppSettings`, `DefaultAppSettings()`, `ValidateAppSettings()`, `MigrateLegacyScreen()` exactly as master plan.

- [ ] **Step 1: Write failing settings tests**

Tests must assert at least:

```cpp
CHECK_EQ(static_cast<int>(MigrateLegacyScreen(0)), static_cast<int>(AppView::Weather));
CHECK_EQ(static_cast<int>(MigrateLegacyScreen(1)), static_cast<int>(AppView::Weather));
CHECK_EQ(static_cast<int>(MigrateLegacyScreen(2)), static_cast<int>(AppView::Settings));
CHECK_EQ(static_cast<int>(MigrateLegacyScreen(255)), static_cast<int>(AppView::Weather));

AppSettings s = DefaultAppSettings();
s.latitude = 120.0; s.longitude = -250.0; s.brightnessPct = 200;
s.meteoRangeIndex = 99; s.animationFrameCount = 0;
s.framePeriodMs = 1; s.endPauseMs = 65000; s.overlayTimeoutMs = 1;
ValidateAppSettings(&s);
CHECK_EQ(s.latitude, 50.0755);
CHECK_EQ(s.longitude, 14.4378);
CHECK_EQ(s.brightnessPct, 100);
CHECK_EQ(s.meteoRangeIndex, 1);
CHECK_EQ(s.animationFrameCount, 2);
CHECK_EQ(s.framePeriodMs, 250);
CHECK_EQ(s.endPauseMs, 15000);
CHECK_EQ(s.overlayTimeoutMs, 3000);
```

Register `test_app_settings` with `../../src/AppSettings.cpp` and include `../../src`.

- [ ] **Step 2: Run tests and verify the expected failure**

```powershell
cmake -S tests/host -B build/host
cmake --build build/host --config Release
ctest --test-dir build/host -C Release --output-on-failure
```

Expected: compilation fails because `AppSettings.h` or its symbols do not exist.

- [ ] **Step 3: Implement defaults and validation**

Use schema version `1` and these exact limits:

```cpp
static const uint16_t kAppSettingsSchema = 1;
static const uint8_t kRangeCount = 4;
// brightness 0..100; range invalid -> 1; frames 2..6;
// frame period 250..2000 ms; pause 0..15000 ms; overlay 3000..15000 ms.
```

Defaults: současné `Config.h` coordinates `50.0755, 14.4378`, `hasLocation=false`, brightness `80`, range index `1`, six frames, `500 ms`, `5000 ms`, `6000 ms`, cities/status enabled and `AppView::Weather`. Invalid coordinates restore default coordinates and clear `hasLocation`.

- [ ] **Step 4: Run focused and full verification**

```powershell
cmake --build build/host --config Release
ctest --test-dir build/host -C Release --output-on-failure
arduino-cli compile --profile default --output-dir build/arduino src
```

Expected: all tests and firmware build pass.

- [ ] **Step 5: Commit AppSettings**

```powershell
git add src/AppSettings.* tests/host/test_app_settings.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md
git commit -m "feat: add versioned MeteoLCD settings model"
git status --short
```

Expected: empty status.

### Task A3: Preferences repository a legacy migration

**Files:**
- Modify: `src/Settings.h`
- Modify: `src/Settings.cpp`
- Create: `docs/verification/settings-migration-matrix.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`

**Interfaces:**
- Consumes: `AppSettings` validation and `MigrateLegacyScreen()` from A2; existing namespace `planeradar` and keys `lat`, `lon`, `hasLoc`, `bl`, `rngM`, `scr`.
- Produces: `const AppSettings& Settings_Current()`, setters for Level 1 fields, debounced `Settings_Tick()`, and compatibility wrappers used by existing screens until later tasks.

- [ ] **Step 1: Add pure migration cases before Preferences integration**

Extend `AppSettings.h` and `test_app_settings.cpp` with this exact pure legacy input and `MigrateLegacySettings()` case:

```cpp
struct LegacySettings {
  double lat;
  double lon;
  bool hasLocation;
  uint8_t brightnessPct;
  uint8_t meteoRangeIndex;
  uint8_t screen;
};
AppSettings MigrateLegacySettings(const LegacySettings& legacy);
```

```cpp
LegacySettings legacy = {};
legacy.lat = 50.0755; legacy.lon = 14.4378; legacy.hasLocation = true;
legacy.brightnessPct = 67; legacy.meteoRangeIndex = 3; legacy.screen = 2;
AppSettings migrated = MigrateLegacySettings(legacy);
CHECK_EQ(migrated.hasLocation, true);
CHECK_EQ(migrated.brightnessPct, 67);
CHECK_EQ(migrated.meteoRangeIndex, 3);
CHECK_EQ(static_cast<int>(migrated.lastView), static_cast<int>(AppView::Settings));
```

- [ ] **Step 2: Verify the migration test fails**

Run CTest; expected compilation failure for undefined `LegacySettings`/`MigrateLegacySettings`.

- [ ] **Step 3: Implement schema-aware Settings loading**

Required behavior:

```text
if key "schema" is absent:
  read legacy keys
  migrate and validate in RAM
  persist all new common keys plus schema=1 once
else:
  read schema-1 keys and validate
never call prefs.clear() during migration
never touch Wi-Fi driver/WiFiManager storage
```

Use new keys no longer than Preferences limits: `schema`, `lat`, `lon`, `hasLoc`, `bl`, `range`, `frames`, `frameMs`, `pauseMs`, `overlayMs`, `cities`, `status`, `view`. Keep existing public getters as wrappers until callers migrate.

- [ ] **Step 4: Document and run the migration matrix**

`settings-migration-matrix.md` records actual results for fresh NVS, legacy plane screen, legacy meteo screen, legacy settings screen, invalid screen, invalid coordinates and existing schema 1. Test on hardware or a Preferences test double before marking complete.

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Expected: host + Arduino pass; migration matrix has no unfilled cells.

- [ ] **Step 5: Commit repository migration**

```powershell
git add src/AppSettings.* src/Settings.* tests/host/test_app_settings.cpp docs/verification/settings-migration-matrix.md docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md
git commit -m "feat: migrate legacy settings without data loss"
git status --short
```

### Task A4: Weather-only routing before deletion

**Files:**
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `src/ScreenSettings.cpp`
- Modify: `src/ScreenSettings.h`
- Modify: `src/Config.h`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`

**Interfaces:**
- Consumes: `AppView::Weather`, `AppView::Settings`; current `ScreenWeather_*`, `ScreenSettings_*`.
- Produces: two-view navigation with weather as default; no runtime references to `ScreenPlanes` or ADS-B.

- [ ] **Step 1: Add a compile-time guard test for forbidden runtime references**

Create a PowerShell check inside `tools/verify.ps1` after host tests:

```powershell
$mainText = Get-Content -Raw src/MeteoPlaneRadar.ino
if ($mainText -match 'ADSB_|ScreenPlanes_|SCREEN_PLANES') {
  throw 'weather-only router still references aviation runtime symbols'
}
```

Run it now; expected failure.

- [ ] **Step 2: Replace the screen router**

Use exactly two views mapped to `AppView`. Remove aviation includes, polling callback, modal/detail routing, plane range handling and directional screen cycling. During this transitional task:

- weather is full-screen default;
- a small-movement press lasting at least 500ms anywhere toggles weather/settings until Phase C replaces it with the visible overlay settings button;
- stored `AppView` is restored through `Settings_Current().lastView`;
- screen dots represent two views or are removed if the adaptive overlay task will replace them.

- [ ] **Step 3: Remove aviation controls from the current settings screen**

Delete metric/aviation unit and top-bearing rows and their tap branches. Keep brightness, meteo range, Wi-Fi portal and OTA operational so the commit is independently usable.

- [ ] **Step 4: Verify host, static guard and Arduino build**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Expected: all pass. On hardware verify boot → weather, open/exit settings, portal request, OTA request and restart persistence.

- [ ] **Step 5: Commit weather-only routing**

```powershell
git add src/MeteoPlaneRadar.ino src/ScreenSettings.* src/Config.h tools/verify.ps1 docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md
git commit -m "feat: route MeteoLCD to weather-only views"
git status --short
```

### Task A5: Delete ADS-B and aircraft screen

**Files:**
- Delete: `src/ADSB.cpp`
- Delete: `src/ADSB.h`
- Delete: `src/ScreenPlanes.cpp`
- Delete: `src/ScreenPlanes.h`
- Modify: `src/Settings.h`
- Modify: `src/Settings.cpp`
- Modify: `src/Config.h`
- Modify: `tools/verify.ps1`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`

**Interfaces:**
- Consumes: weather-only router from A4.
- Produces: no aircraft source, types, config, Preferences API or body buffer in build.

- [ ] **Step 1: Strengthen the aviation static guard**

Add recursive checks that fail for `#include "ADSB.h"`, `#include "ScreenPlanes.h"`, `Settings_PlaneRange`, `Settings_MetricUnits`, `Settings_TopBearing`, `ADSB_URL`, `PLANE_` and `MAP_ROT_` in remaining `src` text. Run and observe failure before removal.

- [ ] **Step 2: Delete aviation source and APIs**

Remove the four files and all now-unused settings getters/setters/config values. Do not delete ArduinoJson because `GeoIP.cpp` still consumes it. Legacy NVS keys remain ignored, not cleared.

- [ ] **Step 3: Run full verification and record size delta**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Append actual compiled flash/RAM values and delta from A1 to `docs/verification/level1-baseline.md` under `After aircraft removal`.

- [ ] **Step 4: Commit aircraft deletion**

```powershell
git add -A src/ADSB.cpp src/ADSB.h src/ScreenPlanes.cpp src/ScreenPlanes.h src/Settings.* src/Config.h tools/verify.ps1 docs/verification/level1-baseline.md docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md
git commit -m "refactor: remove ADS-B and aircraft screen"
git status --short
```

### Task A6: Delete European aviation map, remove unused async dependencies a rebrand

**Files:**
- Delete: `src/EuBorder.cpp`
- Delete: `src/EuBorder.h`
- Delete: `src/EuMapData.h`
- Modify: `sketch.yaml`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `src/Version.h`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `src/WiFiPortal.cpp`
- Modify: `src/OTA.cpp`
- Modify: `tools/verify.ps1`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`

**Interfaces:**
- Consumes: no remaining aviation runtime references.
- Produces: weather-only branding; build profile without ESP Async WebServer/Async TCP; retained internal namespace migration.

- [ ] **Step 1: Add final forbidden-source checks**

Fail verification if any remaining source contains `EuBorder`, `EuMapData`, `adsb.fi`, `Aircraft radar` or includes deleted files. Run and observe failure.

- [ ] **Step 2: Remove European aviation map and async libraries**

Delete the three files. Remove only `ESP Async WebServer (3.3.12)` and `Async TCP (3.2.10)` from `sketch.yaml`; synchronous OTA already uses core `WebServer`. Keep all other pinned versions unchanged.

- [ ] **Step 3: Rebrand user-facing text without breaking migration**

Use `MeteoLCD` for boot, AP/portal/OTA labels, README and new docs. Keep NVS namespace `planeradar`. If AP SSID changes, document that users must select the new SSID during portal/OTA, but do not erase stored station credentials.

- [ ] **Step 4: Verify and perform hardware smoke**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Hardware expected: black-frame-first boot, weather display, touch works, portal connects, OTA page loads, display recovers after exiting service mode.

- [ ] **Step 5: Commit completed foundation phase**

```powershell
git add -A src/EuBorder.cpp src/EuBorder.h src/EuMapData.h sketch.yaml src README.md CHANGELOG.md tools/verify.ps1 docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md
git commit -m "refactor: complete MeteoLCD weather-only foundation"
git status --short
```

Expected: empty status; all A tasks are committed and hardware smoke is recorded. Continue to this plan's `## Phase Integration Gate`. Do not start Phase B until that gate's final fresh `origin/main` verification succeeds.

## Phase Integration Gate

1. Confirm every task checkbox in this phase is committed.
2. Run the phase's complete verification command and read its full output.
3. Confirm `git status --short` is empty and the current branch is the required
   phase branch.
4. Pin the exact phase tip, push that phase branch to `origin`, and bind the
   reviewed `origin/main`:

   ```powershell
   $expectedPhaseTip = git rev-parse HEAD
   if ($LASTEXITCODE -ne 0) { throw 'Could not pin reviewed phase tip' }
   git push origin feature/level1-phase-a
   if ($LASTEXITCODE -ne 0) { throw 'Could not publish phase branch' }
   git fetch --prune origin
   if ($LASTEXITCODE -ne 0) { throw 'Could not refresh origin/main before review' }
   $expectedOriginMain = git rev-parse origin/main
   if ($LASTEXITCODE -ne 0) { throw 'Could not pin reviewed origin/main' }
   Write-Output "Reviewed origin/main SHA: $expectedOriginMain"
   if ((git rev-parse origin/feature/level1-phase-a) -ne $expectedPhaseTip) {
     throw 'Remote phase branch does not match pinned phase tip'
   }
   Write-Output "Reviewed phase tip SHA: $expectedPhaseTip"
   ```

   Review the full `origin/main...feature/level1-phase-a` diff. Retain both printed SHA values with the review record.
   If either reviewed SHA is missing when resuming, restart with this review fetch and re-review the full diff.
   Do not recalculate either expected SHA during integration.
5. Rebind the retained review values literally and integrate:

   ```powershell
   # Replace both placeholders only with the retained review-record values before running any integration command.
   $expectedOriginMain = '<reviewed-origin-main-sha>'
   $expectedPhaseTip = '<reviewed-phase-tip-sha>'
   if ($expectedOriginMain -eq '<reviewed-origin-main-sha>' -or
       $expectedPhaseTip -eq '<reviewed-phase-tip-sha>') {
     throw 'Reviewed SHAs must be rebound from the retained review record'
   }
   git switch main
   if ($LASTEXITCODE -ne 0) { throw 'Could not switch to main' }
   git fetch --prune origin
   if ($LASTEXITCODE -ne 0) { throw 'Could not refresh origin/main before integration' }
   if ((git rev-parse origin/main) -ne $expectedOriginMain) {
     throw 'Unexpected remote main changed after review'
   }
   if ((git rev-parse origin/feature/level1-phase-a) -ne $expectedPhaseTip) {
     throw 'Remote phase branch does not match pinned phase tip'
   }
   git merge-base --is-ancestor main origin/main
   if ($LASTEXITCODE -ne 0) { throw 'main cannot fast-forward to origin/main' }
   git merge --ff-only origin/main
   if ($LASTEXITCODE -ne 0) { throw 'Could not fast-forward main to origin/main' }
   $localPhaseTip = git rev-parse feature/level1-phase-a
   if ($LASTEXITCODE -ne 0) { throw 'Could not resolve local phase branch before merge' }
   if ($localPhaseTip -ne $expectedPhaseTip) {
     throw 'Local phase branch changed after review'
   }
   git merge --no-ff feature/level1-phase-a
   if ($LASTEXITCODE -ne 0) { throw 'Could not merge reviewed phase branch' }
   ```

   Stop before the dependent command if any guard fails.
6. Re-run the phase verification on the merge result.
7. Push `main` normally and verify it against a fresh remote-tracking ref:

   ```powershell
   git push origin main
   if ($LASTEXITCODE -ne 0) { throw 'Could not publish integrated main' }
   git fetch --prune origin
   if ($LASTEXITCODE -ne 0) { throw 'Could not refresh origin after main push' }
   if ((git rev-parse main) -ne (git rev-parse origin/main)) {
     throw 'Local main does not match fresh origin/main'
   }
   ```

   Only after this final comparison succeeds may the next phase begin and its
   branch be created from the verified `main`.

Do not delete the phase branch until the merge commit and remote `main` have
been verified. Never use force-push to repair a failed gate.
