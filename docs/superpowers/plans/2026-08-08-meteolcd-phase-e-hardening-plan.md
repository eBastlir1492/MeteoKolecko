# MeteoLCD Phase E Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Změřit a odstranit zbývající problémy odezvy, zavést diagnostiku, ověřit dlouhodobou stabilitu a vytvořit důkazní release checkpoint Level 1.

**Architecture:** Fixed-capacity diagnostics a connectivity state machine nejprve změří skutečné chování. Jedna síťová FreeRTOS úloha se přidá pouze při překročení schválené 100ms UI latence; rozhodnutí i nepřidání musí mít committed důkaz. Release je dokončen až po hardwarové matici a 24h/72h soak.

**Tech Stack:** ESP32 heap/task metrics, C++11 host tests, optional FreeRTOS queue/task, PowerShell verification, serial JSONL capture.

## Global Constraints

- **Required branch: `feature/level1-phase-e`**.
- Before every task, verify the exact branch with `git branch --show-current`.
- Never commit a phase task directly to `main` or to another phase branch.
- If the branch is missing, create it only from the verified `main` start point
  stated in the master Branch Contract and publish it with `git push -u origin`.
- Fáze A–D musí být dokončené.
- Diagnostics ani worker nesmí alokovat v render/touch hot path.
- Hlavní úloha vždy výhradně vlastní displej, touch a viditelný framebuffer.
- OTA musí před flash potvrdit zastavení workeru nebo žádný worker nesmí existovat.
- TLS politika Level 1 zůstává explicitně zdokumentovaná; neprovádět neověřenou certifikační migraci.
- Každý task končí jedním commitem a čistým worktree.

---

### Task E1: Fixed-capacity diagnostics a runtime metriky

**Files:**
- Create: `src/Diagnostics.h`
- Create: `src/Diagnostics.cpp`
- Create: `tests/host/test_diagnostics.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `src/WeatherRepository.cpp`
- Modify: `src/WeatherRenderer.cpp`
- Modify: `src/DisplayBackend.cpp`
- Modify: `src/PortalServer.cpp`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`

**Interfaces:**
- Consumes: monotonic milliseconds, duration values, error enums and heap snapshots.
- Produces:

```cpp
enum class DiagnosticEventType : uint8_t {
  Boot, WifiState, RefreshStart, RefreshSuccess, RefreshFailure,
  DecodeFailure, PresentTimeout, DisplayRecovery, OtaStart, OtaEnd
};
struct DiagnosticEvent { uint32_t atMs; DiagnosticEventType type; int32_t arg0; int32_t arg1; };
struct DurationStats { uint32_t count, minUs, maxUs; uint64_t sumUs; uint32_t overBudget; };
struct MemoryStats { uint32_t freeInternal, minInternal, largestInternal; uint32_t freePsram, minPsram, largestPsram; };
class Diagnostics {
 public:
  void record(DiagnosticEvent event);
  void observeDownload(uint32_t us);
  void observeDecode(uint32_t us);
  void observeCompose(uint32_t us);
  void observePresent(uint32_t us, bool timedOut);
  void sampleMemory();
  size_t writeJson(char* out, size_t capacity) const;
};
```

- [ ] **Step 1: Write failing ring/stats tests**

Assert 64-event wrap retains newest events in chronological export, zero-sample stats are valid, min/max/sum/overBudget update exactly, fixed output truncates with valid NUL and exported JSON never includes a supplied secret sentinel.

- [ ] **Step 2: Run and observe missing diagnostics failure**

- [ ] **Step 3: Implement fixed storage**

Use `DiagnosticEvent events_[64]`, head/count indices and no `String`. Durations are integer microseconds. `sampleMemory()` uses capability-specific ESP32 calls only in the device translation unit; host tests inject values through a pure update helper.

- [ ] **Step 4: Instrument boundaries, not inner loops**

Measure catalog/download, per-frame decode, compose and present around whole calls. Record boot reset reason, refresh errors, VSYNC timeout/recovery and OTA lifecycle. Portal `/api/status` includes aggregates and last 16 sanitized events.

- [ ] **Step 5: Verify overhead and commit diagnostics**

Run 1000-frame device animation and compare compose p95 with Phase C; diagnostics overhead must stay below 2% or instrumentation frequency must be reduced before completion.

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add src/Diagnostics.* src/MeteoPlaneRadar.ino src/WeatherRepository.cpp src/WeatherRenderer.cpp src/DisplayBackend.cpp src/PortalServer.cpp tests/host/test_diagnostics.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md
git commit -m "feat: add bounded MeteoLCD diagnostics"
git status --short
```

### Task E2: ConnectivityManager a omezený backoff

**Files:**
- Create: `src/ConnectivityManager.h`
- Create: `src/ConnectivityManager.cpp`
- Create: `src/ConnectivityPolicy.h`
- Create: `src/ConnectivityPolicy.cpp`
- Create: `tests/host/test_connectivity.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/MeteoPlaneRadar.ino`
- Modify: `src/PortalServer.cpp`
- Modify: `src/WeatherRepository.cpp`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`

**Interfaces:**
- Consumes: observed Wi-Fi link status and monotonic time.
- Produces:

```cpp
enum class ConnectivityState : uint8_t { Idle, Connecting, Connected, Backoff, Portal };
struct ConnectivitySnapshot { ConnectivityState state; int8_t rssi; uint32_t retryAtMs; uint8_t attempt; };
class ConnectivityManager {
 public:
  void begin(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void enterPortal();
  void leavePortal(uint32_t nowMs);
  bool online() const;
  ConnectivitySnapshot snapshot() const;
};
```

- [ ] **Step 1: Write failing policy tests**

Assert attempts at 5s, 10s, 20s, 40s, 80s and cap 120s; success resets attempt; portal suppresses reconnect; unsigned millis wrap is handled through signed deadline comparison; weather refresh is not requested while offline.

- [ ] **Step 2: Run and observe missing connectivity failure**

- [ ] **Step 3: Implement policy and Wi-Fi adapter**

`tick()` never loops or delays. It starts at most one `WiFi.begin()`/`WiFi.reconnect()` action at a deadline, observes status and emits diagnostics only on state change. Portal mode owns AP/STA transitions.

- [ ] **Step 4: Replace fixed 15-second reconnect path**

Remove remaining `WiFi_Loop` behavior. `WeatherRepository` consults `online()` before starting a transfer and transitions to stale/degraded without discarding live.

- [ ] **Step 5: Verify dropout matrix and commit**

Test AP power-off 1/5/30 minutes, restoration, credential change via portal and millis wrap fake. Run full verify.

```powershell
git add src/ConnectivityManager.* src/ConnectivityPolicy.* src/MeteoPlaneRadar.ino src/PortalServer.cpp src/WeatherRepository.cpp tests/host/test_connectivity.cpp tests/host/CMakeLists.txt docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md
git commit -m "feat: manage Wi-Fi with bounded reconnect backoff"
git status --short
```

### Task E3: Měřené rozhodnutí o síťové FreeRTOS úloze

**Files:**
- Create: `docs/verification/network-latency-decision.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`
- Conditional create in the same task: `src/NetworkWorker.h`, `src/NetworkWorker.cpp`, `tests/host/test_network_protocol.cpp`
- Conditional modify: `src/WeatherRepository.cpp`, `src/AppController.cpp`, `src/OTA.cpp`, `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: diagnostics latency distribution under real Wi-Fi/ČHMÚ conditions.
- Produces: either committed evidence that cooperative transport meets target, or exactly one bounded worker with this protocol:

```cpp
enum class NetworkJobType : uint8_t { FetchCatalog, DownloadPng, Stop };
struct NetworkJob {
  uint32_t id;
  NetworkJobType type;
  char url[160];
  uint8_t* destination;
  size_t capacity;
  uint32_t idleTimeoutMs;
  uint32_t totalTimeoutMs;
};
struct NetworkReply {
  uint32_t id;
  DownloadResult result;
  uint8_t catalogCount;
  ChmuFrameRef catalog[kWeatherMaxFrames];
};
class NetworkWorker {
 public:
  bool begin();
  bool submit(const NetworkJob& job);
  bool poll(NetworkReply* reply);
  bool requestStop(uint32_t nowMs);
  bool stopped() const;
  uint32_t stackHighWaterBytes() const;
};
```

- [ ] **Step 1: Measure before choosing**

Capture at least 20 catalog refreshes, 120 PNG downloads and simultaneous touch probes. Record median/p95/worst transport-call duration, touch-event latency, present timeouts and PSRAM minima for strong Wi-Fi, weak Wi-Fi and one induced packet-loss/disconnect case.

Decision rule:

```text
If any transport call prevents touch processing for >100 ms,
or measured touch p95 exceeds 100 ms during refresh:
  implement Worker branch below.
Else:
  document No-worker branch with raw summary and retain cooperative design.
```

- [ ] **Step 2A: Worker branch — write failing protocol tests**

Test queue full rejection, monotonically matched IDs, completion transfer, cancel during chunk read, Stop preventing new jobs, confirmed stopped state and OTA transition blocked until confirmation.

- [ ] **Step 3A: Worker branch — implement exactly one task**

Create one task with 12 288-byte stack, priority 1 and `tskNO_AFFINITY`; queues are statically allocated with job depth 2 and reply depth 4. Worker owns `ChmuTransport`; a catalog job returns at most six value-type `ChmuFrameRef` records, while a PNG job writes only caller-designated compressed staging memory. It never decodes, renders, touches NVS or accesses display/touch. Cancellation is checked between read chunks; transport timeouts bound connect/read stalls. Record stack high-water and PSRAM contention.

- [ ] **Step 4A: Worker branch — integrate OTA stop handshake**

`AppController` accepts StartOta only after `requestStop()` and `stopped()`. A timeout is reported as a visible OTA precondition failure; OTA must not force-delete a running task.

- [ ] **Step 2B: No-worker branch — prove the simpler architecture**

Document exact distributions and state: `NetworkWorker not created because all measured touch latencies were <=100 ms`. Add a regression threshold to diagnostics/hardware checklist. Do not create empty worker files or dormant task code.

- [ ] **Step 5: Verify the selected branch and commit the decision**

For Worker branch, rerun the same measurements and require touch p95 ≤100ms, no new VSYNC timeout, queue/race host tests pass and task stack retains ≥25% headroom. For No-worker branch, rerun full verify and attach raw serial summary.

Run verification first:

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

Commit the selected branch with one checkpoint command:

```powershell
$checkpointPaths = @(
  'docs/verification/network-latency-decision.md',
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md'
)
if (Test-Path 'src/NetworkWorker.h') {
  $checkpointPaths += @(
    'src/NetworkWorker.h', 'src/NetworkWorker.cpp',
    'src/WeatherRepository.cpp', 'src/AppController.cpp', 'src/OTA.cpp',
    'tests/host/test_network_protocol.cpp', 'tests/host/CMakeLists.txt'
  )
}
git add -- $checkpointPaths
git commit -m "perf: record measured weather refresh responsiveness"
git status --short
```

Mark the non-selected branch checkboxes complete as `N/A — <measured reason>` in the selected decision commit. Expected: exactly one decision commit and empty status.

### Task E4: Full regression, memory stability and failure matrix

**Files:**
- Create: `docs/verification/level1-hardware-matrix.md`
- Create: `docs/verification/level1-memory-report.md`
- Modify: `tools/verify.ps1`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`

**Interfaces:**
- Consumes: complete Level 1 firmware and all phase-specific verification documents.
- Produces: one repeatable release-candidate verification command and filled hardware evidence.

- [ ] **Step 1: Make verification cover every automated suite**

`tools/verify.ps1` must run CMake configure/build, all CTest tests, all Python unittests, map/portal generated-file drift checks, golden manifest integrity, forbidden-symbol scans and Arduino compile. Any nonzero exit stops the script.

- [ ] **Step 2: Execute the hardware matrix**

Record pass/fail and diagnostics snapshot for cold boot, 20 warm restarts, boot-reset hold, all touch gestures, four ranges, stale/no-data, Wi-Fi loss/recovery, portal pages/forms, credential replacement, NVS migration, forced allocation failure, truncated/corrupt PNG, VSYNC timeout hook, display recovery, OTA valid/invalid/interrupted and power recovery after ordinary reboot.

- [ ] **Step 3: Compare memory and binary against baseline**

Record final flash/RAM output, free/min/largest internal heap and PSRAM at boot, after maximum 200km live set, during staging, after ten failed refreshes, after portal and after OTA failure. Repeat 100 refresh cycles with a local fixture; largest free blocks must not show monotonic loss.

- [ ] **Step 4: Fix every failure in a separate task commit before returning**

Do not bury fixes in this evidence commit. If a regression is found, stop E4, add a specifically named remediation task to this plan, write a failing reproduction, implement, verify and commit it independently; then restart the complete E4 matrix.

- [ ] **Step 5: Commit verified release-candidate evidence**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add tools/verify.ps1 docs/verification/level1-hardware-matrix.md docs/verification/level1-memory-report.md docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md
git commit -m "test: verify MeteoLCD Level 1 release candidate"
git status --short
```

### Task E5: 24-hour soak checkpoint

**Files:**
- Create: `tools/capture_serial.ps1`
- Create: `docs/verification/soak-24h.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`

**Interfaces:**
- Consumes: periodic diagnostics JSONL emitted over 115200 Bd.
- Produces: timestamped serial capture and committed 24-hour stability checkpoint.

- [ ] **Step 1: Add deterministic serial capture helper**

`capture_serial.ps1` accepts required `-Port`, `-Hours`, `-Output`; opens 115200 Bd, prefixes host UTC timestamps and flushes each JSONL record. It exits nonzero on port loss and never overwrites an existing output file.

- [ ] **Step 2: Run the 24-hour checkpoint**

During 24h include normal five-minute refreshes, at least one 30-minute Wi-Fi outage and one portal open/exit. Report reset reasons, refresh success/failure, present timeouts, memory start/end/min/largest trend and UI responsiveness.

```powershell
git add tools/capture_serial.ps1 docs/verification/soak-24h.md docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md
git commit -m "test: pass MeteoLCD 24-hour soak"
git status --short
```

Expected: one task/one commit and empty status.

### Task E6: 72-hour soak and Level 1 release checkpoint

**Files:**
- Create: `docs/verification/soak-72h.md`
- Modify: `CHANGELOG.md`
- Modify: `README.md`
- Modify: `src/Version.h`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`

**Interfaces:**
- Consumes: committed 24-hour checkpoint, final verification command and serial capture helper.
- Produces: independent 72-hour evidence and final Level 1 version/documentation.

- [ ] **Step 1: Run the independent 72-hour soak**

Start from a cold boot and clean log. Require no unexpected reboot, persistent black screen, live-cache loss after failed refresh, growing memory fragmentation or stuck portal/OTA state. A weather-source outage is acceptable only if stale data/state remains correct.

- [ ] **Step 2: Finalize user documentation and version**

README documents weather-only controls, tap-to-overlay, gestures, portal connection, settings, OTA, stale behavior and data attribution. CHANGELOG lists measured changes and migration notes. Version change follows existing project convention and must not claim tests absent from evidence.

- [ ] **Step 3: Run final verification and commit Level 1 completion**

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
git add docs/verification/soak-72h.md README.md CHANGELOG.md src/Version.h docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md
git commit -m "release: complete MeteoLCD Level 1"
git status --short
git log -8 --oneline
```

Expected: full verification passes, status empty, latest commit is the Level 1 release checkpoint. Next work must begin from a separately brainstormed `NL-xx` roadmap item.

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
   git push origin feature/level1-phase-e
   if ($LASTEXITCODE -ne 0) { throw 'Could not publish phase branch' }
   git fetch --prune origin
   if ($LASTEXITCODE -ne 0) { throw 'Could not refresh origin/main before review' }
   $expectedOriginMain = git rev-parse origin/main
   if ($LASTEXITCODE -ne 0) { throw 'Could not pin reviewed origin/main' }
   Write-Output "Reviewed origin/main SHA: $expectedOriginMain"
   if ((git rev-parse origin/feature/level1-phase-e) -ne $expectedPhaseTip) {
     throw 'Remote phase branch does not match pinned phase tip'
   }
   Write-Output "Reviewed phase tip SHA: $expectedPhaseTip"
   ```

   Review the full `origin/main...feature/level1-phase-e` diff. Retain both printed SHA values with the review record.
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
   if ((git rev-parse origin/feature/level1-phase-e) -ne $expectedPhaseTip) {
     throw 'Remote phase branch does not match pinned phase tip'
   }
   git merge-base --is-ancestor main origin/main
   if ($LASTEXITCODE -ne 0) { throw 'main cannot fast-forward to origin/main' }
   git merge --ff-only origin/main
   if ($LASTEXITCODE -ne 0) { throw 'Could not fast-forward main to origin/main' }
   git merge --no-ff feature/level1-phase-e
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

   Level 1 is terminal after this final comparison. Do not create another phase branch.
   Hand off only to a separately brainstormed and approved Next Level plan
   and the [Next Level roadmap](../specs/2026-08-08-meteolcd-next-level-roadmap.md).

Do not delete the phase branch until the merge commit and remote `main` have
been verified. Never use force-push to repair a failed gate.
