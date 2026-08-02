// =============================================================================
//  MeteoPlaneRadar
//  Config.h - ALL user-tunable settings in one place.
//
//  This is the only file you normally need to touch when adapting the project:
//  time zone, default location, ranges, poll intervals, AP name, limits.
//  Everything here is a compile-time default; the location, brightness, units,
//  last screen and last range are also stored in NVS at runtime (Settings.*).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
//  Board pins / bus
// ---------------------------------------------------------------------------
#define I2C_SDA   15
#define I2C_SCL   7
#define BOOT_PIN  0        // hold at power-up (~3 s) = factory reset

// ---------------------------------------------------------------------------
//  Time zone (POSIX TZ string) + NTP
// ---------------------------------------------------------------------------
#define TZ_INFO   "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER "pool.ntp.org"

// ---------------------------------------------------------------------------
//  Default location (Prague). Overwritten on first boot by IP geolocation, or
//  manually in the WiFi portal; the stored value always wins.
// ---------------------------------------------------------------------------
#define DEFAULT_LAT 50.0755
#define DEFAULT_LON 14.4378

// ---------------------------------------------------------------------------
//  Configuration access point (WiFi portal and OTA share this name)
// ---------------------------------------------------------------------------
#define AP_SSID     "MeteoPlaneRadar"
#define AP_PASSWORD ""     // "" = open network

// ---------------------------------------------------------------------------
//  Aircraft radar (adsb.fi)
// ---------------------------------------------------------------------------
#define ADSB_MAX 100       // max aircraft held/drawn (airborne only)

// Selectable ranges in km. Keep them ascending; the count is derived.
#define PLANE_RANGES_KM { 10.0f, 25.0f, 50.0f, 100.0f }

// Poll interval by range - larger areas return more data and are less
// time-critical, so they are polled less often (easier on the free API).
// After a failed fetch the interval is doubled.
#define ADSB_PERIOD_NEAR_MS  5000    // up to  ADSB_NEAR_KM
#define ADSB_PERIOD_MID_MS  10000    // up to  ADSB_MID_KM
#define ADSB_PERIOD_FAR_MS  15000    // beyond ADSB_MID_KM
#define ADSB_NEAR_KM 25.0f
#define ADSB_MID_KM  50.0f

// ---------------------------------------------------------------------------
//  Weather radar (CHMU)
// ---------------------------------------------------------------------------
#define METEO_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f }

// ---------------------------------------------------------------------------
//  OTA (firmware update over WiFi)
// ---------------------------------------------------------------------------
#define OTA_IDLE_MS 300000UL   // leave OTA mode after this long with no upload

// ---------------------------------------------------------------------------
//  Map orientation
//  The user picks which compass bearing sits at the TOP of the aircraft radar,
//  i.e. the direction they are looking. The step must divide 90 evenly,
//  otherwise the exact cardinal directions (east / west) become unreachable.
// ---------------------------------------------------------------------------
#define MAP_ROT_STEP_DEG 45    // degrees per button press (45 -> 8 positions)

// ---------------------------------------------------------------------------
//  Aircraft detail
// ---------------------------------------------------------------------------
// adsb.fi occasionally drops an aircraft from a single poll and sends it again
// in the next one. Closing the detail panel on the first miss looks like the
// panel closes by itself, so tolerate this many consecutive misses first.
#define DETAIL_GRACE_POLLS 2

// ---------------------------------------------------------------------------
//  Diagnostics
//
//  The serial log comes out at 115200 Bd over the connector marked "USB" -
//  that is the ESP32-S3's native USB. Nothing shows up on the other USB-C
//  connector on the board.
// ---------------------------------------------------------------------------
// 1 = log touch gestures and every aircraft-selection change (with the reason
// why the detail closed) to the serial console at 115200 Bd.
#define TOUCH_DEBUG 0

// 1 = measure how long one full-screen flush takes and print min/last/max once
// per second. Use this to diagnose a flickering band: one frame lasts ~34 ms,
// so if the flush takes anywhere near that, the copy and the panel's scan-out
// run at the same speed and keep crossing each other. Set to 0 when done.
#define FLUSH_DEBUG 0

// ---------------------------------------------------------------------------
//  Watchdog
// ---------------------------------------------------------------------------
#define WDT_TIMEOUT_S 20       // reboot after this many seconds of being stuck
