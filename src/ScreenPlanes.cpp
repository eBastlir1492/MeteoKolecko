// =============================================================================
//  MeteoPlaneRadar
//  Screen 1: aircraft radar (adsb.fi) + aircraft detail.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenPlanes.h"
#include "ADSB.h"
#include "Settings.h"
#include "Config.h"
#include "EuBorder.h"
#include "UI.h"
#include "Display_ST7701.h"

#include <WiFi.h>
#include <math.h>
#include <string.h>   // strncpy / strcmp for the hex-based selection

// Round panel - radar centred on the middle of the screen.
// R_RADIUS = 230 px maps the selected range onto the radar circle.
#define R_CX (LCD_WIDTH / 2)
#define R_CY (LCD_HEIGHT / 2)
#define R_RADIUS 230

// Ranges (radius in km).
static const float RANGES_KM[] = PLANE_RANGES_KM;
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;   // 25 km by default

static float currentRange() { return RANGES_KM[s_rangeIdx]; }

// Base poll interval by range. Larger areas return more data and are less
// time-critical, so they are polled less often - keeps us a light user of the
// free adsb.fi API.
static unsigned long basePeriodMs() {
  float r = currentRange();
  if (r <= ADSB_NEAR_KM) return ADSB_PERIOD_NEAR_MS;
  if (r <= ADSB_MID_KM)  return ADSB_PERIOD_MID_MS;
  return ADSB_PERIOD_FAR_MS;
}

static unsigned long s_nextFetch = 0;
static bool  s_dataOk = false;
static String s_status = "Starting...";

// Aircraft detail - the selected aircraft is remembered by its ICAO hex address,
// NOT by its index in the list.
//
// The list is rebuilt from scratch on every fetch (every 5 s) and adsb.fi does
// not guarantee a stable order: one aircraft leaving the area shifts everything
// after it. Holding an index across a fetch therefore silently repoints the
// detail at a different aircraft. Keying on the hex means the panel either
// follows the aircraft you actually picked, or closes when it genuinely leaves.
//
// Empty string = nothing selected / detail closed.
static char s_selectedHex[8] = "";

// Grace period for the detail panel. adsb.fi sometimes omits an aircraft from a
// single poll and sends it again in the next one; closing the panel on the
// first miss looks like it closes by itself. We therefore keep the last known
// data and only give up after DETAIL_GRACE_POLLS consecutive misses.
static int      s_selMiss = 0;          // consecutive polls without the aircraft
static Aircraft s_selCache;             // last known data of the selected one
static bool     s_selCacheOk = false;

// Screen positions from the last draw, for tap-to-select. Parallel to the
// aircraft list, so s_planeHex[i] records which aircraft the point belongs to
// and the tap resolves to an identity rather than to a slot number.
static int  s_planeX[ADSB_MAX];
static int  s_planeY[ADSB_MAX];
static char s_planeHex[ADSB_MAX][8];
static int  s_planeN = 0;

bool ScreenPlanes_DetailOpen() { return s_selectedHex[0] != '\0'; }

// reason = short text for the serial log, so we can tell WHY the panel closed
// (user tap vs. the aircraft disappearing from the data).
static void selectNone(const char* reason) {
#if TOUCH_DEBUG
  if (s_selectedHex[0]) Serial.printf("SEL: zavreno (%s) hex=%s\n", reason, s_selectedHex);
#else
  (void)reason;
#endif
  s_selectedHex[0] = '\0';
  s_selMiss = 0;
  s_selCacheOk = false;
}
static void selectHex(const char* hex) {
  strncpy(s_selectedHex, hex, sizeof(s_selectedHex) - 1);
  s_selectedHex[sizeof(s_selectedHex) - 1] = '\0';
  s_selMiss = 0;
  s_selCacheOk = false;
#if TOUCH_DEBUG
  Serial.printf("SEL: vybrano hex=%s\n", s_selectedHex);
#endif
}

// Convert an aircraft's lat/lon into screen coordinates (north up).
// --- Map orientation -------------------------------------------------------
// The user picks WHICH COMPASS BEARING IS AT THE TOP of the screen, i.e. the
// direction they are looking out of the window. Everything then follows:
//
//     screen angle of bearing b  =  b - topBearing      (0 = up, clockwise)
//
// So looking east (top = 90) puts north on the left, which is exactly where it
// is in reality. Setting an "amount to turn by" instead used to need the
// opposite value (360 - bearing) and felt like the map was mirrored.
//
// We do NOT rotate the display (the canvas only supports rotation 0 and turning
// the framebuffer would break touch mapping) - the PROJECTION is rotated.
// Borders and cities go through the same project(), so they follow along for
// free; only the aircraft icons need their heading corrected separately.
static float s_rotSin = 0.0f, s_rotCos = 1.0f;   // cached sin/cos of the angle
static uint16_t s_topDeg = 0;                    // bearing shown at the top

static void refreshRotation() {
  uint16_t deg = Settings_TopBearing();
  if (deg == s_topDeg) return;
  s_topDeg = deg;
  float r = (float)deg * 0.0174532925f;
  s_rotSin = sinf(r);
  s_rotCos = cosf(r);
}

static void project(float lat, float lon, double clat, double clon,
                    float rangeKm, int* sx, int* sy) {
  float latr = clat * 0.0174532925f;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);
  float dyKm = (lat - clat) * 111.0f;
  // Rotate the local east/north vector so that the chosen bearing ends up at
  // the top: with top = 90 (looking east) a target due east appears up.
  float rx = dxKm * s_rotCos - dyKm * s_rotSin;
  float ry = dxKm * s_rotSin + dyKm * s_rotCos;
  float scale = (float)R_RADIUS / rangeKm;   // px per km
  *sx = R_CX + (int)(rx * scale);
  *sy = R_CY - (int)(ry * scale);
}

// Wrapper matching the ProjectFn signature (used to draw the map via EuBorder).
// Uses the current user location and the selected range.
static void cityProject(float lat, float lon, int* sx, int* sy) {
  project(lat, lon, Settings_Lat(), Settings_Lon(), currentRange(), sx, sy);
}

// --- Altitude colour bands ---
// The colour of an aircraft encodes its barometric altitude, so a glance tells
// you whether something is on approach overhead or just transiting at cruise.
// The bands are contiguous - every altitude falls into exactly one.
//
//   < 2 km   red     approach/departure, helicopters, light aircraft
//   2-6 km   orange  climb/descent, regional traffic
//   6-10 km  yellow  lower cruise levels
//   >= 10 km blue    long-haul cruise
//
// An aircraft reporting no altitude at all (altFt == 0 and not on the ground)
// is drawn grey rather than being forced into the "low" band, which would
// otherwise paint every unknown target an alarming red.
static uint16_t altColor(float altFt, bool known) {
  if (!known) return C_GRAY;
  float km = altFt * 0.0003048f;      // feet -> kilometres
  if (km <  2.0f) return C_RED;
  if (km <  6.0f) return C_ORANGE;
  if (km < 10.0f) return C_YELLOW;
  return C_BLUE;
}

// Aircraft icon - a winged arrow, rotated to match the ground track.
// Filled polygon; the nose points "forward" (locally up, fwd positive).
// When the track is unknown (hasTrack=false), a circle is drawn instead.
static void drawPlane(int x, int y, float trackDeg, bool hasTrack, uint16_t col) {
  if (!hasTrack) {
    // Track unknown - circle with a dot (orientation cannot be determined).
    gfx->drawCircle(x, y, 7, col);
    gfx->fillCircle(x, y, 2, col);
    return;
  }
  float a = trackDeg * 0.0174532925f;
  float ca = cosf(a), sa = sinf(a);
  // Local coordinates: right = to the right of the nose, fwd = forward (towards it).
  // Compass rotation: track 0 = up, 90 = right (east).
  //   screen_x = x + right*cos(a) + fwd*sin(a)
  //   screen_y = y - right*... ; derived so that fwd follows the track:
  //   nose (right=0, fwd=L) -> (x + L*sin(a), y - L*cos(a))
  auto rot = [&](float right, float fwd, int* ox, int* oy) {
    *ox = x + (int)(right * ca + fwd * sa);
    *oy = y + (int)(right * sa - fwd * ca);
  };
  // Arrow vertices (right, fwd). Nose ahead (fwd=+12), tail behind (fwd=-12).
  // Outline order: nose, right side, right wingtip, fuselage, right tailplane,
  //                rear centre, left tailplane, fuselage, left wingtip, left side
  const float P[10][2] = {
    { 0,  12}, { 3,  1}, { 13, -8}, { 3, -5}, { 3, -7},
    { 0, -12}, {-3, -7}, {-3, -5}, {-13, -8}, {-3,  1}
  };
  int px[10], py[10];
  for (int i = 0; i < 10; i++) rot(P[i][0], P[i][1], &px[i], &py[i]);

  // Fill as a triangle fan from the centre of the icon.
  for (int i = 0; i < 10; i++) {
    int j = (i + 1) % 10;
    gfx->fillTriangle(x, y, px[i], py[i], px[j], py[j], col);
  }
}

void ScreenPlanes_Enter() {
  s_rangeIdx = Settings_PlaneRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;   // guard against a stale value
  s_nextFetch = 0;
}

bool ScreenPlanes_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = "Cekam na WiFi"; return false; }

  if (millis() >= s_nextFetch) {
    s_status = "Stahuji...";
    s_dataOk = ADSB_Fetch(Settings_Lat(), Settings_Lon(), currentRange());
    s_status = s_dataOk ? "OK" : "Chyba";
    // Normal cadence depends on range; after a failure back off to double the
    // interval rather than hammering the API at the normal rate.
    unsigned long period = basePeriodMs();
    s_nextFetch = millis() + (s_dataOk ? period : period * 2);

    // Selection bookkeeping happens HERE (once per poll), not while drawing -
    // drawing runs many times per second and would burn the grace period in an
    // instant. The selection is an ICAO hex, so a reordered list cannot move it
    // onto a different aircraft.
    if (s_dataOk && ScreenPlanes_DetailOpen()) {
      if (ADSB_FindByHex(s_selectedHex) >= 0) {
        s_selMiss = 0;                       // still there
      } else if (++s_selMiss > DETAIL_GRACE_POLLS) {
        selectNone("letadlo zmizelo z dat");
      }
#if TOUCH_DEBUG
      else Serial.printf("SEL: chybi v datech (%d/%d) hex=%s\n",
                         s_selMiss, DETAIL_GRACE_POLLS, s_selectedHex);
#endif
    }
    return true;   // new data -> redraw
  }
  return false;    // otherwise skip the redraw (keeps swiping responsive)
}

// Short tap: if the detail is open -> either toggle the units (button at the bottom)
// or close it. Otherwise select the nearest aircraft.
bool ScreenPlanes_HandleTap(int x, int y) {
  if (ScreenPlanes_DetailOpen()) {   // detail is open
    // Units button zone (bottom of the panel).
    const int pw = 320, ph = 260;
    int px = R_CX - pw / 2, py = R_CY - ph / 2;
    int btnY = py + ph - 42;
    if (x >= px + 18 && x <= px + pw - 18 && y >= btnY && y <= btnY + 32) {
      Settings_SetMetricUnits(!Settings_MetricUnits());   // toggle + persist
      return true;
    }
    selectNone("tap mimo panel");   // tapped elsewhere -> close
    return true;
  }
  // Find the aircraft nearest the tap (within 30 px), then remember *which
  // aircraft* it was, not where it happened to sit in the list.
  int best = -1;
  long bestD = 30L * 30L;
  for (int i = 0; i < s_planeN; i++) {
    long dx = s_planeX[i] - x, dy = s_planeY[i] - y;
    long d = dx * dx + dy * dy;
    if (d < bestD) { bestD = d; best = i; }
  }
  if (best >= 0 && s_planeHex[best][0]) {
    selectHex(s_planeHex[best]);
    return true;
  }
  return false;
}

// Swipe: change the range (both directions). Re-fetch immediately at the new
// radius. Ignored while the detail panel is open (close it first).
void ScreenPlanes_ChangeRange(int dir) {
  if (ScreenPlanes_DetailOpen()) return;
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  Settings_SetPlaneRange(s_rangeIdx);   // remember across restarts (debounced)
  Serial.printf("ADSB range: %.0f km\n", currentRange());
  s_nextFetch = 0;
}

// Close the detail panel (called on the long-press screen switch).
void ScreenPlanes_CloseDetail() { selectNone("dlouhy stisk / prepnuti obrazovky"); }

void ScreenPlanes_Draw() {
  gfx->fillScreen(C_BLACK);
  refreshRotation();           // pick up a changed rotation setting
  float range = currentRange();

  // --- Map underlay ---
  // The map data spans the whole of Europe, so work out which slice of it can
  // actually be on screen and hand that window to EuBorder, which throws away
  // everything else before drawing. The radar circle has a radius of `range`
  // km, so the visible span is `range` in every direction; a 20% margin keeps
  // lines that only clip the edge of the view from being dropped.
  const double clat = Settings_Lat();
  const double clon = Settings_Lon();
  const float  marginKm = range * 1.2f;
  const float  dLat = marginKm / 111.0f;
  const float  dLon = marginKm / (111.0f * cosf(clat * 0.0174532925f));
  const float  lat0 = clat - dLat, lat1 = clat + dLat;
  const float  lon0 = clon - dLon, lon1 = clon + dLon;

  EuBorder_Draw(cityProject, C_GRAY, lat0, lat1, lon0, lon1);

  // Cities as an underlay (below the aircraft, so it is clear where they are).
  // Fewer of them survive as the range grows, otherwise a dense region such as
  // the Ruhr would bury the traffic under a wall of labels.
  {
    int rad = LCD_WIDTH / 2 - 4;
    bool showFull = (range <= 25.0f);    // full names only at close range
    uint8_t maxTier;
    if      (range <= 25.0f) maxTier = 3;   // everything down to 50k
    else if (range <= 50.0f) maxTier = 2;   // 150k and above
    else                     maxTier = 1;   // 300k and above
    EuBorder_DrawCities(cityProject, R_CX, R_CY, rad, C_DKGRAY, C_GRAY,
                        showFull, maxTier, lat0, lat1, lon0, lon1);
  }

  // Range rings and centre marker (2 circles, white cross).
  gfx->drawCircle(R_CX, R_CY, LCD_WIDTH / 2 - 2, C_DKGRAY);
  gfx->drawCircle(R_CX, R_CY, LCD_WIDTH / 4, C_DKGRAY);
  gfx->drawFastHLine(R_CX - 8, R_CY, 16, C_WHITE);
  gfx->drawFastVLine(R_CX, R_CY - 8, 16, C_WHITE);

  // Aircraft.
  int shown = 0;
  const Aircraft* list = ADSB_List();
  int n = ADSB_Count();
  s_planeN = n;

  // Resolve the selection once per frame. If the aircraft has left the area it
  // is simply not in the new data, so the detail closes by itself rather than
  // silently latching onto whoever now sits at that index.
  // Do NOT close the panel here - that is decided once per poll in Tick(), so
  // the grace period actually lasts. While the aircraft is missing we keep
  // drawing the last known values from the cache.
  int selIdx = ADSB_FindByHex(s_selectedHex);

  for (int i = 0; i < n; i++) {
    s_planeX[i] = -9999; s_planeY[i] = -9999;   // default: off-screen
    s_planeHex[i][0] = '\0';
    if (list[i].onGround) continue;
    int sx, sy;
    project(list[i].lat, list[i].lon, Settings_Lat(), Settings_Lon(), range, &sx, &sy);
    int dx = sx - R_CX, dy = sy - R_CY;
    if (dx * dx + dy * dy > R_RADIUS * R_RADIUS) continue;   // outside the circle
    // Store position *and* identity, so a tap resolves to an aircraft and not
    // to a list slot that may mean something else by the time it is used.
    s_planeX[i] = sx; s_planeY[i] = sy;
    strncpy(s_planeHex[i], list[i].hex, sizeof(s_planeHex[i]) - 1);
    s_planeHex[i][sizeof(s_planeHex[i]) - 1] = '\0';
    // Highlight the selected aircraft with a ring. It is white, not cyan: the
    // icon colour now carries the altitude, and cyan sat too close to the blue
    // of the cruise band to read as "selected".
    if (i == selIdx) gfx->drawCircle(sx, sy, 16, C_WHITE);
    // Altitude is only meaningful when the aircraft actually reports it.
    bool altKnown = (list[i].altFt > 0.0f);
    // The map is turned, so the icon heading has to be corrected the same way -
    // otherwise the aircraft would point the wrong direction.
    float screenTrack = list[i].track - (float)s_topDeg;
    while (screenTrack < 0.0f) screenTrack += 360.0f;
    drawPlane(sx, sy, screenTrack, list[i].hasTrack,
              altColor(list[i].altFt, altKnown));
    if (list[i].callsign[0]) {
      // Callsign below the icon, centred (the icon has a radius of ~14 px).
      gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
      int len = strlen(list[i].callsign);
      int tw = len * 12;   // font size 2: ~12 px per character
      gfx->setCursor(sx - tw / 2, sy + 22);
      gfx->print(list[i].callsign);
    }
    shown++;
  }

  // Aircraft count at the top (the title was dropped to save space).
  char sub[32];
  if (WiFi.status() != WL_CONNECTED || !s_dataOk) {
    UI_TextCentered(s_status.c_str(), 48, C_YELLOW, 2);
  } else {
    snprintf(sub, sizeof(sub), "Letadel: %d", shown);
    UI_TextCentered(sub, 48, C_CYAN, 2);
  }

  // --- Altitude legend ---
  // The icon colour means nothing without a key. Four swatches in a continuous
  // bar, with the band boundaries (2 / 6 / 10 km) marked underneath. It sits
  // below the aircraft count, where the round panel still has room - the bottom
  // of the screen is already taken by the range readout.
  {
    const uint16_t cols[4] = {C_RED, C_ORANGE, C_YELLOW, C_BLUE};
    const char*    bounds[3] = {"2", "6", "10"};   // km, at the band edges
    const int sw = 26;                 // swatch width
    const int barW = 4 * sw;
    const int lx = R_CX - barW / 2;
    const int ly = 74;

    // Continuous bar - the bands abut, matching the fact that the altitude
    // ranges themselves are contiguous with no gaps between them.
    for (int i = 0; i < 4; i++) {
      gfx->fillRect(lx + i * sw, ly, sw, 6, cols[i]);
    }

    // Boundary numbers, centred on each internal edge of the bar.
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY);
    for (int i = 0; i < 3; i++) {
      int edge = lx + (i + 1) * sw;              // border between band i and i+1
      int tw = strlen(bounds[i]) * 6;
      gfx->setCursor(edge - tw / 2, ly + 10);
      gfx->print(bounds[i]);
    }
    // Unit, just past the right end of the bar.
    gfx->setCursor(lx + barW + 4, ly + 10);
    gfx->print("km");
  }

  // --- Compass marks ---
  // Bearing b appears on screen at angle (b - rotation), 0 = up, clockwise.
  // Small size-1 letters at r = 205 slot in between the screen dots (y=18),
  // the aircraft count (y=48) and the range row at the bottom.
  {
    const int   cr = 205;
    const char* lbl[4] = { "S", "V", "J", "Z" };   // sever, vychod, jih, zapad
    const int   brg[4] = { 0, 90, 180, 270 };
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY);
    for (int i = 0; i < 4; i++) {
      // Bearing b shows up at screen angle (b - topBearing).
      float a = (brg[i] - (int)s_topDeg) * 0.0174532925f;
      int cxp = R_CX + (int)(cr * sinf(a)) - 3;   // -3/-4 centres the glyph
      int cyp = R_CY - (int)(cr * cosf(a)) - 4;
      gfx->setCursor(cxp, cyp);
      gfx->print(lbl[i]);
    }
  }

  // Range indicator at the bottom (moved up a little to leave room for the
  // southern compass mark).
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%.0f km", range);
  UI_TextCentered(rbuf, LCD_HEIGHT - 76, C_YELLOW, 2);

  int dotGap = 24;
  int totalW = (RANGE_COUNT - 1) * dotGap;
  int startX = R_CX - totalW / 2;
  int dotY = LCD_HEIGHT - 50;
  for (int i = 0; i < RANGE_COUNT; i++) {
    int x = startX + i * dotGap;
    if (i == s_rangeIdx) gfx->fillCircle(x, dotY, 5, C_YELLOW);
    else                 gfx->drawCircle(x, dotY, 5, C_GRAY);
  }

  // --- Detail of the selected aircraft (overlay) ---
  // selIdx was resolved from the hex at the top of the frame, so this always
  // shows the aircraft that was actually tapped, with values refreshed from the
  // latest fetch (altitude, speed and climb rate update live while it is open).
  // Keep a copy of the selected aircraft so the panel can still be drawn while
  // the aircraft is temporarily missing from the data (grace period).
  if (selIdx >= 0 && selIdx < n) { s_selCache = list[selIdx]; s_selCacheOk = true; }
  const bool signalLost = (selIdx < 0) && s_selCacheOk;

  if (ScreenPlanes_DetailOpen() && s_selCacheOk) {
    const Aircraft& ac = s_selCache;
    const bool metric = Settings_MetricUnits();

    // Panel centred so it fits inside the round display.
    const int pw = 320, ph = 260;
    const int px = R_CX - pw / 2;
    const int py = R_CY - ph / 2;
    gfx->fillRoundRect(px, py, pw, ph, 14, C_DKGRAY);
    gfx->drawRoundRect(px, py, pw, ph, 14, C_CYAN);

    // Close button in the top-right corner.
    int cxx = px + pw - 24, cyy = py + 22;
    gfx->fillCircle(cxx, cyy, 15, C_RED);
    gfx->drawLine(cxx - 6, cyy - 6, cxx + 6, cyy + 6, C_WHITE);
    gfx->drawLine(cxx - 6, cyy + 6, cxx + 6, cyy - 6, C_WHITE);

    // Callsign (heading).
    gfx->setTextSize(3); gfx->setTextColor(C_YELLOW);
    gfx->setCursor(px + 18, py + 16);
    gfx->print(ac.callsign[0] ? ac.callsign : "?");

    // Data rows (font 2). Units converted according to the setting.
    char line[44];
    int ty = py + 56;
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);

    // Altitude: ft or m.
    if (metric) snprintf(line, sizeof(line), "Vyska: %.0f m", ac.altFt * 0.3048f);
    else        snprintf(line, sizeof(line), "Vyska: %.0f ft", ac.altFt);
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Speed: kt or km/h.
    if (metric) snprintf(line, sizeof(line), "Rychlost: %.0f km/h", ac.gsKt * 1.852f);
    else        snprintf(line, sizeof(line), "Rychlost: %.0f kt", ac.gsKt);
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Ground track.
    if (ac.hasTrack) snprintf(line, sizeof(line), "Kurz: %.0f deg", ac.track);
    else             snprintf(line, sizeof(line), "Kurz: neznamy");
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Climb/descent - short label + units so the line does not overflow.
    // An arrow instead of the words "climbing/descending".
    const char* ar = ac.baroRate > 100 ? "^" : (ac.baroRate < -100 ? "v" : "-");
    if (metric) snprintf(line, sizeof(line), "Stoupani: %.1f m/s %s", ac.baroRate * 0.00508f, ar);
    else        snprintf(line, sizeof(line), "Stoupani: %.0f ft/m %s", ac.baroRate, ar);
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Type (if known).
    if (ac.type[0]) {
      snprintf(line, sizeof(line), "Typ: %s", ac.type);
      gfx->setCursor(px + 18, ty); gfx->print(line);
    }

    // While the aircraft is missing from the data (grace period) say so, so the
    // frozen values are not mistaken for live ones.
    if (signalLost) {
      gfx->setTextSize(1); gfx->setTextColor(C_YELLOW);
      const char* lost = "signal ztracen";
      gfx->setCursor(px + pw - 18 - (int)strlen(lost) * 6, py + ph - 56);
      gfx->print(lost);
    }

    // Units toggle button at the bottom.
    int btnY = py + ph - 42;
    gfx->fillRoundRect(px + 18, btnY, pw - 36, 32, 8, C_CYAN);
    gfx->setTextSize(1); gfx->setTextColor(C_BLACK);
    const char* btnLabel = metric ? "Jednotky: metricke (klik = letecke)"
                                   : "Jednotky: letecke (klik = metricke)";
    int tw = strlen(btnLabel) * 6;
    gfx->setCursor(px + 18 + (pw - 36 - tw) / 2, btnY + 12);
    gfx->print(btnLabel);
  }
}
