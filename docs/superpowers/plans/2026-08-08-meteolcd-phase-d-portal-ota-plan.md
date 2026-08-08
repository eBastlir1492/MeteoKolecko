# MeteoLCD Phase D Portal and OTA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Nahradit strohý blokující WiFiManager portal moderním lokálním mobilním UI, přidat úplné validované nastavení a sjednotit bezpečné servisní přechody s OTA.

**Architecture:** Malý synchronní `WebServer` a `DNSServer` běží kooperativně v explicitním `Portal` stavu. HTML/CSS/JS je lokální a generované do `PortalAssets.h`; formuláře používají server-side validaci. ElegantOTA se připojí ke stejnému serveru a `AppController` zastaví weather staging před flash zápisem.

**Tech Stack:** ESP32 core `WebServer`, `DNSServer`, WiFi, ElegantOTA 3.1.6, C++ pure validation tests, static HTML/CSS/vanilla JS bez CDN.

## Global Constraints

- **Required branch: `feature/level1-phase-d`**.
- Before every task, verify the exact branch with `git branch --show-current`.
- Never commit a phase task directly to `main` or to another phase branch.
- If the branch is missing, create it only from the verified `main` start point
  stated in the master Branch Contract and publish it with `git push -u origin`.
- Fáze A–C musí být dokončené.
- Portal běží pouze během explicitního provisioning/service režimu a má timeout.
- Heslo Wi-Fi se nikdy nevrací v HTML, JSON, logu ani diagnostics.
- Všechny mutace jsou POST; restart/reset mají dvoustupňové potvrzení.
- OTA zachová současný partition layout a zhasnutí podsvitu během flash write.
- `WebServer::handleClient()` a DNS processing se volají krátce z hlavní smyčky.
- Každý task končí jedním commitem a čistým worktree.

---

### Task D1: Portal validation a settings form model

**Files:**
- Create: `src/PortalValidation.h`
- Create: `src/PortalValidation.cpp`
- Create: `tests/host/test_portal_validation.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/AppSettings.h`
- Modify: `src/AppSettings.cpp`
- Modify: `src/Settings.h`
- Modify: `src/Settings.cpp`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`

**Interfaces:**
- Consumes: Level 1 `AppSettings` schema.
- Produces:

```cpp
enum class FormError : uint8_t {
  None, Missing, InvalidNumber, OutOfRange, InvalidBoolean, ConfirmationMismatch
};
struct FieldResult { FormError error; double number; bool boolean; };
FieldResult ParseNumberField(const char* text, double minValue, double maxValue);
FieldResult ParseBoolField(const char* text);
bool ConstantTimeEquals(const char* actual, const char* expected);
size_t HtmlEscape(const char* source, char* destination, size_t capacity);
```

- [ ] **Step 1: Write failing validation/security tests**

Cover empty, whitespace, trailing junk, NaN/Inf, Czech comma rejection, min/max boundaries, boolean `0/1`, HTML `<>&\"'`, destination truncation with NUL termination and exact confirmation text. Example:

```cpp
CHECK_EQ(ParseNumberField("49.1951", -90, 90).error, FormError::None);
CHECK_EQ(ParseNumberField("49.1x", -90, 90).error, FormError::InvalidNumber);
char escaped[32];
HtmlEscape("<&\"", escaped, sizeof escaped);
CHECK(std::strcmp(escaped, "&lt;&amp;&quot;") == 0);
CHECK(ConstantTimeEquals("RESET WIFI", "RESET WIFI"));
CHECK(!ConstantTimeEquals("reset wifi", "RESET WIFI"));
```

- [ ] **Step 2: Run and observe missing validation failure**

- [ ] **Step 3: Implement parser and complete Level 1 settings accessors**

Add repository getters/setters for animation frames 2–6, frame period 250–2000ms, pause 0–15000ms, overlay 3000–15000ms, cities/status bool and coordinates. All setters validate through `ValidateAppSettings`, mark one debounced dirty state and never immediately perform multiple NVS writes.

- [ ] **Step 4: Verify and commit validation**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/PortalValidation.* src/AppSettings.* src/Settings.* tests/host/test_portal_validation.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md
git commit -m "feat: validate MeteoLCD portal settings"
git status --short
```

### Task D2: Moderní offline portal assets

**Files:**
- Create: `assets/portal/index.html`
- Create: `assets/portal/portal.css`
- Create: `assets/portal/portal.js`
- Create: `tools/generate_portal_assets.py`
- Create: `tests/test_portal_assets.py`
- Create: `src/PortalAssets.h`
- Modify: `tools/verify.ps1`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`

**Interfaces:**
- Consumes: server endpoints documented in D3.
- Produces: PROGMEM `kPortalHtml`, `kPortalCss`, `kPortalJs` and lengths; source assets remain human-reviewable.

- [ ] **Step 1: Write failing asset tests**

Assert no `http://`, `https://`, `<script src=`, external font or CDN token; exactly one viewport meta; Czech UTF-8 labels; focus-visible style; reduced-motion rule; color-scheme support; form labels tied by `for/id`; destructive buttons use `data-confirm`; touch controls have CSS min-height/width ≥48px.

- [ ] **Step 2: Run and observe missing assets failure**

```powershell
python -m unittest tests.test_portal_assets -v
```

- [ ] **Step 3: Build the responsive UI source**

Use four navigation sections: Přehled, Připojení a poloha, Radar a displej, Systém. Default layout is one column, cards use max width 52rem, desktop becomes two-column where useful. Provide visible inline error/success regions with `aria-live`, native form semantics and no JS-only required save path. JavaScript only enhances Wi-Fi scan, status refresh, confirmation and OTA progress.

- [ ] **Step 4: Generate deterministic PROGMEM assets**

The Python generator normalizes LF, escapes raw-string delimiters and emits byte-exact arrays. Verification regenerates to `build/PortalAssets.h` and fails on drift.

- [ ] **Step 5: Verify static accessibility and commit assets**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1 -SkipArduino
git add assets/portal tools/generate_portal_assets.py tests/test_portal_assets.py src/PortalAssets.h tools/verify.ps1 docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md
git commit -m "feat: add modern offline portal interface"
git status --short
```

### Task D3: Cooperative captive PortalServer and Wi-Fi provisioning

**Files:**
- Create: `src/PortalServer.h`
- Create: `src/PortalServer.cpp`
- Create: `src/PortalResponses.h`
- Create: `src/PortalResponses.cpp`
- Create: `tests/host/test_portal_responses.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/WiFiPortal.h`
- Modify: `src/WiFiPortal.cpp`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`

**Interfaces:**
- Consumes: `PortalAssets`, `Settings`, `WiFi`, `WebServer`, `DNSServer`.
- Produces:

```cpp
enum class PortalMode : uint8_t { Provisioning, Settings };
enum class PortalExit : uint8_t { None, UserExit, Connected, Timeout, RestartRequested };
class PortalServer {
 public:
  bool begin(PortalMode mode, uint32_t nowMs);
  PortalExit tick(uint32_t nowMs);
  void stop();
  bool active() const;
};
```

Required routes:

```text
GET  /                 dashboard shell
GET  /portal.css       local CSS
GET  /portal.js        local JS
GET  /api/status       connection/weather/system summary without secrets
GET  /api/settings     non-secret settings
POST /api/settings     validated settings update
POST /api/location/geoip  online GeoIP detection; preserve manual location on failure
GET  /api/wifi         scanned SSID/RSSI/security list
POST /api/wifi         new ssid/password; response never echoes password
GET  /api/diagnostics  downloadable sanitized diagnostics JSON
POST /api/exit         close service portal
POST /api/restart      requires confirm=RESTART
POST /api/reset-wifi   requires confirm=RESET WIFI
POST /api/factory      requires confirm=FACTORY RESET
```

- [ ] **Step 1: Write failing response/secret tests**

Pure response builders receive fake status/settings and a sentinel secret `NeverExposeThis`. Assert the sentinel is absent from every HTML/JSON/error response, all JSON strings are escaped, invalid fields return HTTP 422 field errors and destructive mismatch returns 409 without action.

- [ ] **Step 2: Run and observe missing response failure**

- [ ] **Step 3: Implement captive AP lifecycle**

Use fixed AP IP `192.168.4.1`, `WIFI_AP_STA`, `softAPConfig`, `softAP`, wildcard DNS port 53 and portal timeout from configuration. Redirect common captive probes (`/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, unknown GET) to `/`. `tick()` calls DNS and `handleClient()` once and returns promptly.

- [ ] **Step 4: Implement provisioning without erasing saved credentials**

On normal boot call `WiFi.begin()` to reuse stored ESP32 station credentials. On validated POST call `WiFi.persistent(true)` then `WiFi.begin(ssid,password)`. Do not log password or store it in `Settings`. `reset-wifi` uses the ESP32-supported credential erase path only after exact confirmation. Keep AP alive while station connection result is displayed.

- [ ] **Step 5: Implement settings/status routes and server-side forms**

Every POST validates all supplied fields before applying any. A failed form changes nothing. Successful multi-field save updates the in-memory settings once and schedules one debounced NVS commit. GeoIP is allowed only while online; on lookup failure it preserves the existing manual location and returns an explicit error. Show the current fixed `TZ_INFO` read-only rather than pretending Level 1 supports arbitrary timezone data. Status includes firmware version, RSSI/IP, live weather timestamp/generation/freshness, free/min/largest heap/PSRAM and last non-secret error. `/api/diagnostics` returns the sanitized fixed-ring export as an attachment.

- [ ] **Step 6: Verify captive/mobile behavior and commit server**

Run full verify. On phone test provisioning without internet, wrong password correction, saved-network reboot, explicit exit, three-minute timeout and all form errors. Record in `docs/verification/portal-matrix.md`.

```powershell
git add src/PortalServer.* src/PortalResponses.* src/WiFiPortal.* src/MeteoPlaneRadar.ino tests/host/test_portal_responses.cpp tests/host/CMakeLists.txt docs/verification/portal-matrix.md docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md
git commit -m "feat: serve cooperative MeteoLCD configuration portal"
git status --short
```

### Task D4: Explicit AppController service transitions

**Files:**
- Create: `src/AppController.h`
- Create: `src/AppController.cpp`
- Create: `tests/host/test_app_controller.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `src/ScreenSettings.cpp`
- Modify: `src/ScreenSettings.h`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`

**Interfaces:**
- Consumes: weather refresh cancel/status, settings flush readiness, PortalServer and OTA request.
- Produces:

```cpp
enum class AppState : uint8_t {
  Booting, Connecting, LoadingWeather, WeatherActive,
  Settings, Portal, Ota, Degraded
};
enum class AppCommand : uint8_t {
  None, OpenSettings, CloseSettings, OpenPortal, ClosePortal,
  StartOta, WeatherAvailable, WeatherFailed
};
class AppController {
 public:
  AppState state() const;
  bool dispatch(AppCommand command);
  bool canStartWeatherRefresh() const;
  bool canWriteSettings() const;
};
```

- [ ] **Step 1: Write failing transition tests**

Cover boot/connect/weather, no-data degraded, settings open/close, portal from settings, reject weather refresh in Portal/Ota, OTA only after staging cancel, return from failed OTA, and impossible direct `Booting→Ota` rejection.

- [ ] **Step 2: Implement pure transition table**

No hardware calls inside the state model. Invalid commands return false without state change. The `.ino` integration performs side effects only after accepted transitions.

- [ ] **Step 3: Replace blocking flags in the main loop**

Remove `ScreenSettings_WantsPortal/ClearPortal` and `WantsOTA/ClearOTA` polling flags. Menu emits commands; controller owns current mode. In Portal state, weather animation may remain visually static but touch/portal service/display watchdog continue ticking.

- [ ] **Step 4: Verify and commit application states**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/AppController.* src/MeteoPlaneRadar.ino src/ScreenSettings.* tests/host/test_app_controller.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md
git commit -m "refactor: make MeteoLCD service states explicit"
git status --short
```

### Task D5: Integrate OTA into the shared portal server

**Files:**
- Modify: `src/OTA.h`
- Modify: `src/OTA.cpp`
- Modify: `src/PortalServer.h`
- Modify: `src/PortalServer.cpp`
- Modify: `src/AppController.cpp`
- Modify: `src/MeteoPlaneRadar.ino`
- Create: `docs/verification/ota-matrix.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`

**Interfaces:**
- Consumes: shared `WebServer&`, AppState Ota, weather staging cancellation.
- Produces:

```cpp
void OTA_Attach(WebServer& server);
void OTA_Detach();
bool OTA_Busy();
uint8_t OTA_ProgressPct();
bool OTA_LastResultOk();
```

- [ ] **Step 1: Add OTA lifecycle assertions to AppController tests**

Assert StartOta is rejected while staging cannot confirm cancellation, accepted after cancellation, NVS write is not started during Ota, and OTA failure returns to Settings/Portal without corrupting live weather generation.

- [ ] **Step 2: Refactor `OTA_Run()` into callbacks attached to shared server**

Remove its private infinite loop and private server ownership. Keep `ElegantOTA.setAutoReboot(true)`. `onStart` sets Ota state, draws warning once, waits only the existing readable 900ms, then sets backlight 0. `onProgress` records fixed integer progress without drawing. `onEnd` restores brightness and result UI after flash writing ends.

- [ ] **Step 3: Preserve cancel/timeout and display behavior**

Before upload starts, portal can exit normally. Once `OTA_Busy()` is true, ignore exit/reset commands. Do not draw while flash writing. Display watchdog policy during flash must match measured safe behavior; do not repeatedly feed a suspended watchdog as if it were active.

- [ ] **Step 4: Execute OTA matrix**

Test valid bin, invalid file, interrupted browser before upload, portal idle exit, completed update/reboot, settings preservation and weather cache behavior after reboot. Record exact observed results and firmware versions.

- [ ] **Step 5: Verify and commit OTA integration**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/OTA.* src/PortalServer.* src/AppController.* src/MeteoPlaneRadar.ino docs/verification/ota-matrix.md docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md
git commit -m "refactor: integrate OTA with MeteoLCD service portal"
git status --short
```

### Task D6: Remove WiFiManager and complete portal design review

**Files:**
- Delete: `src/WiFiPortal.cpp`
- Delete: `src/WiFiPortal.h`
- Modify: `sketch.yaml`
- Modify: `src/Config.h`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `README.md`
- Modify: `tools/verify.ps1`
- Create: `docs/verification/portal-ui-review.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`

**Interfaces:**
- Consumes: PortalServer and AppController now cover all old WiFiPortal behavior.
- Produces: no WiFiManager code/dependency; reviewed mobile/desktop portal.

- [ ] **Step 1: Add forbidden-symbol guard and observe failure**

Fail on `WiFiManager`, `WiFi_ConnectOrPortal`, `WiFi_StartPortal` and inclusion of `WiFiPortal.h` in remaining source.

- [ ] **Step 2: Remove old wrapper and library pin**

Delete both files and remove only `WiFiManager (2.0.17)` from `sketch.yaml`. Keep AP IP, timeout and product settings in focused Portal configuration.

- [ ] **Step 3: Conduct portal UI review**

Capture mobile widths 320/375/430px and desktop 1024px for dashboard, every form, inline validation, scan progress, stale/offline, OTA and confirmations. Review keyboard focus, contrast, long Czech labels, no-JS save and round-LCD AP instructions. Record accepted screenshots and corrections.

- [ ] **Step 4: Run full verification and commit Phase D**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add -A src/WiFiPortal.cpp src/WiFiPortal.h sketch.yaml src README.md tools/verify.ps1 docs/verification/portal-ui-review.md docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md
git commit -m "refactor: complete modern MeteoLCD portal"
git status --short
```

Expected: empty status; provisioning, settings and OTA all work without WiFiManager. Continue to this plan's `## Phase Integration Gate`. Do not start Phase E until that gate's final fresh `origin/main` verification succeeds.

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
   git push origin feature/level1-phase-d
   if ($LASTEXITCODE -ne 0) { throw 'Could not publish phase branch' }
   git fetch --prune origin
   if ($LASTEXITCODE -ne 0) { throw 'Could not refresh origin/main before review' }
   $expectedOriginMain = git rev-parse origin/main
   if ($LASTEXITCODE -ne 0) { throw 'Could not pin reviewed origin/main' }
   Write-Output "Reviewed origin/main SHA: $expectedOriginMain"
   if ((git rev-parse origin/feature/level1-phase-d) -ne $expectedPhaseTip) {
     throw 'Remote phase branch does not match pinned phase tip'
   }
   Write-Output "Reviewed phase tip SHA: $expectedPhaseTip"
   ```

   Review the full `origin/main...feature/level1-phase-d` diff. Retain both printed SHA values with the review record.
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
   if ((git rev-parse origin/feature/level1-phase-d) -ne $expectedPhaseTip) {
     throw 'Remote phase branch does not match pinned phase tip'
   }
   git merge-base --is-ancestor main origin/main
   if ($LASTEXITCODE -ne 0) { throw 'main cannot fast-forward to origin/main' }
   git merge --ff-only origin/main
   if ($LASTEXITCODE -ne 0) { throw 'Could not fast-forward main to origin/main' }
   $localPhaseTip = git rev-parse feature/level1-phase-d
   if ($LASTEXITCODE -ne 0) { throw 'Could not resolve local phase branch before merge' }
   if ($localPhaseTip -ne $expectedPhaseTip) {
     throw 'Local phase branch changed after review'
   }
   git merge --no-ff feature/level1-phase-d
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
