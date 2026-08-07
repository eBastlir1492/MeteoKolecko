# MeteoLCD Level 1 Master Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Převést funkční MeteoPlaneRadar na stabilní weather-only MeteoLCD s atomickou cache ČHMÚ, testovatelným rendererem, adaptivním LCD UI a moderním lokálním portálem.

**Architecture:** Jde o postupnou seam-first migraci. Ověřená ST7701/VSYNC/double-framebuffer/touch pipeline zůstává stabilní hranicí; čisté doménové moduly se nejprve pokryjí host testy a teprve potom se zapojí do firmware. Síť smí dostat jednu pracovní FreeRTOS úlohu jen po měření blokující latence.

**Tech Stack:** Arduino-ESP32 3.0.7, C++/Arduino, Arduino GFX 1.4.9, PNGdec 1.0.1, ArduinoJson 7.1.0, synchronous `WebServer`, ElegantOTA 3.1.6, CMake/CTest host testy, Python 3 map/golden nástroje, PowerShell ověřovací skripty.

## Global Constraints

- Autorita: fyzicky ověřené chování > aktuální kód > Git historie > externí dokumentace.
- Neměnit ST7701 init, RGB pinout, 8MHz PCLK, porch/sync, dva PSRAM framebuffery ani desetřádkové bounce buffery bez samostatného měření a schválení.
- Zachovat funkční `EXIO8` chování bez přejmenováním motivované hardwarové změny.
- Nezapínat `TOUCH_RECOVERY`; zachovat CST820 filtry a `TOUCH_RELEASE_MS = 60`.
- Zachovat `src/partitions.csv`; partition migrace není OTA-safe.
- Zachovat pinned platformu/knihovny během Level 1; dependency update je samostatná Next Level položka.
- Velké buffery musí explicitně požadovat PSRAM a nesmějí tiše fallbackovat do interní RAM.
- Renderer, touch a viditelný framebuffer vlastní hlavní aplikační úloha.
- Poslední platná `live` weather sada přežije jakékoli selhání další obnovy.
- Portal nemá CDN/runtime internetovou závislost a nikdy nevrací uložené Wi-Fi heslo.
- Každý implementační task končí relevantními testy a jedním samostatným commitem.
- Neprovádět destruktivní Git obnovu rozpracované práce; při nečistém stromu nejprve zjistit původ změn.

---

## Autoritativní dokumenty

1. [Level 1 design](../specs/2026-08-08-meteolcd-weather-only-design.md)
2. [Next Level roadmap](../specs/2026-08-08-meteolcd-next-level-roadmap.md)
3. [Git a GitHub workflow](../specs/2026-08-08-meteokolecko-git-workflow-design.md)
4. Tento master plán a pět fázových plánů níže.

Nový agent bez kontextu chatu musí nejprve přečíst všechny tři výše uvedené položky a poté právě prováděný fázový plán celý.

## Aktuální kód, který je nutné chápat před implementací

- `src/MeteoPlaneRadar.ino`: setup/loop, screen router, gesture FSM, portal/OTA přechody a display watchdog.
- `src/Display_ST7701.cpp/.h`: RGB hardware, dvojité framebuffery, VSYNC a backlight.
- `src/Canvas16.h`: zero-copy kreslicí buffer; dnes po `LCD_Flush()` slepě přepíná index.
- `src/Touch_CST820.cpp/.h`: ověřené filtrování single-touch dat.
- `src/TCA9554.cpp/.h`: expander a recovery invarianty.
- `src/CHMU.cpp/.h`: katalog, přenos a compressed slots.
- `src/ScreenWeather.cpp/.h`: crop, decode, animace a per-pixel rendering.
- `src/CzBorder.cpp/.h`: 50bodová hranice a 59 měst s hard-coded zkratkami.
- `src/Settings.cpp/.h`: namespace `planeradar`, legacy klíče a debounce.
- `src/WiFiPortal.cpp/.h`: blocking WiFiManager provisioning.
- `src/OTA.cpp/.h`: blocking AP OTA přes synchronní `WebServer`; zhasnutí LCD během flash.
- `src/ADSB*`, `src/ScreenPlanes*`, `src/EuBorder*`, `src/EuMapData.h`: aviation-only odstranění.

## Cílová struktura souborů

Názvy jsou závazné pro navazující plány; změna názvu vyžaduje aktualizaci všech pozdějších Interfaces bloků ve stejném commitu.

```text
src/
  AppController.{h,cpp}             stav aplikace a prioritizace režimů
  AppSettings.{h,cpp}               čistý settings model, defaults, validate, legacy mapping
  Settings.{h,cpp}                  Preferences repository a debounce
  ChmuCatalogParser.{h,cpp}         streaming katalog parser
  ChmuTransport.{h,cpp}             bounded HTTP download
  WeatherTypes.h                    frame/set/status value types
  WeatherFrameStore.{h,cpp}         live/staging ownership a commit/abort
  WeatherDecoder.{h,cpp}            PNGdec adaptér
  WeatherRepository.{h,cpp}         refresh state machine/backoff
  Viewport.{h,cpp}                  Web Mercator a screen/source mapping
  RenderSurface565.h                host/device framebuffer view
  WeatherRenderer.{h,cpp}           map/radar/UI compose
  MapAsset.h
  MapAsset.generated.h              generovaný LOD asset
  CityLabelLayout.{h,cpp}           priority/collision/candidate placement
  DisplayBackend.{h,cpp}            present ownership a metrics
  GestureRecognizer.{h,cpp}         čistý gesture FSM
  OverlayUi.{h,cpp}                 adaptivní overlay
  ConnectivityManager.{h,cpp}       Wi-Fi state/backoff
  PortalAssets.h                    generované lokální HTML/CSS/JS
  PortalServer.{h,cpp}              captive/settings/API/OTA server
  Diagnostics.{h,cpp}               fixed ring a snapshots
assets/maps/cz-level1.json           auditovatelný Level 1 map source
tests/host/                           CMake/CTest čistých C++ modulů
tests/golden/                         referenční PNG a manifest
tools/                                verify, asset generator, golden renderer
docs/verification/                    skutečné build/hardware/soak výsledky
```

## Přesné mezimodulové kontrakty

```cpp
// AppSettings.h
enum class AppView : uint8_t { Weather = 0, Settings = 1 };
struct AppSettings {
  uint16_t schemaVersion;
  double latitude;
  double longitude;
  bool hasLocation;
  uint8_t brightnessPct;
  uint8_t meteoRangeIndex;
  uint8_t animationFrameCount;
  uint16_t framePeriodMs;
  uint16_t endPauseMs;
  uint16_t overlayTimeoutMs;
  bool showCities;
  bool showStatus;
  AppView lastView;
};
AppSettings DefaultAppSettings();
void ValidateAppSettings(AppSettings* value);
AppView MigrateLegacyScreen(uint8_t legacy);
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
// WeatherTypes.h
constexpr size_t kWeatherMaxFrames = 6;
struct WeatherFrameMeta {
  char filename[48];
  int64_t utcEpochSeconds;
  uint16_t width;
  uint16_t height;
  size_t compressedBytes;
};
struct WeatherCoverage {
  double north;
  double south;
  double west;
  double east;
};
struct WeatherFrame {
  WeatherFrameMeta meta;
  uint8_t* compressed;
  uint16_t* pixels;
  WeatherCoverage coverage;
};
struct WeatherFrameSet {
  uint32_t generation;
  uint8_t count;
  WeatherFrame frames[kWeatherMaxFrames];
};
enum class WeatherFreshness : uint8_t { None, Current, Refreshing, Stale };
```

```cpp
// WeatherFrameStore.h
class WeatherFrameStore {
 public:
  bool beginStaging(uint8_t expectedCount);
  bool putStaging(uint8_t index, WeatherFrame* ownedFrame);
  bool commitStaging();
  void abortStaging();
  const WeatherFrameSet* live() const;
  bool refreshing() const;
};
```

```cpp
// Viewport.h
struct GeoPoint { double lat; double lon; };
struct PixelPoint { int16_t x; int16_t y; };
struct ViewportSpec { GeoPoint center; float rangeKm; int16_t width; int16_t height; };
class Viewport {
 public:
  explicit Viewport(const ViewportSpec& spec);
  PixelPoint project(GeoPoint point) const;
  GeoPoint unproject(PixelPoint point) const;
  void buildSourceMaps(uint16_t sourceWidth, uint16_t sourceHeight,
                       int16_t* sourceX, int16_t* sourceY) const;
};
```

```cpp
// GestureRecognizer.h
enum class GestureType : uint8_t { None, Tap, DoubleTap, DragStart, DragMove, DragEnd };
struct GestureEvent { GestureType type; int16_t x; int16_t y; int16_t dx; int16_t dy; };
class GestureRecognizer {
 public:
  bool update(bool touching, int16_t x, int16_t y, uint32_t nowMs,
              GestureEvent* event);
};
```

```cpp
// DisplayBackend.h
enum class PresentResult : uint8_t { Presented, Timeout, InvalidBuffer };
class DisplayBackend {
 public:
  uint16_t* acquireBackBuffer();
  PresentResult present(uint16_t* completedBuffer, uint32_t timeoutMs);
  uint16_t* visibleBuffer() const;
  uint32_t vsyncTimeoutCount() const;
};
```

## Protokol přerušení a pokračování

Na začátku každého pracovního sezení:

```powershell
git status --short
git branch --show-current
git log -8 --oneline
```

1. Otevřít tento master plán a fázový plán odpovídající poslednímu commitu.
2. První nezaškrtnutý task je kandidát na pokračování.
3. Pokud je worktree nečistý, přečíst `git diff` a určit, zda jde o rozpracovaný task nebo uživatelské změny. Nic nemažte ani neobnovujte bez souhlasu.
4. Zopakovat poslední ověřovací příkaz před pokračováním.
5. Každý task dokončit jediným commitem. Jednotlivé checkboxy jsou krátké pracovní kroky; commit je checkpoint celého tasku. Zaškrtnutí kroků patří do stejného commitu jako implementace.
6. Po commitu ověřit `git status --short`; očekává se prázdný výstup.
7. Nikdy nepoužít `git reset --hard` ani `git checkout --` pro řešení přerušeného tasku.

Commit messages jsou jedinečné a tvoří resume ledger. Pokud poslední commit odpovídá tasku, task je hotový; pokud ne, kontroluje se diff a testy.

## Pořadí provádění

1. [Fáze A — baseline, test harness a weather-only migrace](./2026-08-08-meteolcd-phase-a-foundation-plan.md)
2. [Fáze B — atomický datový tok ČHMÚ](./2026-08-08-meteolcd-phase-b-weather-data-plan.md)
3. [Fáze C — renderer, mapa, města a LCD UI](./2026-08-08-meteolcd-phase-c-renderer-ui-plan.md)
4. [Fáze D — moderní portal, nastavení a OTA](./2026-08-08-meteolcd-phase-d-portal-ota-plan.md)
5. [Fáze E — odezva, diagnostika a release hardening](./2026-08-08-meteolcd-phase-e-hardening-plan.md)

Fáze jsou sekvenční. Uvnitř fáze jsou tasky také prováděny v pořadí; pozdější Interfaces bloky předpokládají přesná jména z dřívějších tasků.

Každý `tests/host/test_*.cpp` je samostatný executable s vlastním `main()`, který na konci vrátí `TestFailures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE`; CMake jej registruje jako samostatný CTest. Testy se nespoléhají na globální registry ani externí test framework.

## Definition of Done celého Level 1

- Všechny checkboxy ve všech pěti fázových plánech jsou v committed stavu.
- `tools/verify.ps1` projde host testy, Python testy a Arduino compile.
- Golden manifest odpovídá schváleným 480×480 obrazům.
- `docs/verification/` obsahuje skutečnou baseline, final comparison, hardware matrix a oba soak reporty.
- `git status --short` je prázdný.
- Položky mimo Level 1 nejsou náhodně implementované; zůstávají v Next Level roadmapě.
