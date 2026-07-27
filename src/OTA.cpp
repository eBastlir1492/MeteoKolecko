// =============================================================================
//  MeteoPlaneRadar
//  OTA firmware update over WiFi via ElegantOTA (web upload in AP mode).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "OTA.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Watchdog.h"
#include "Touch_CST820.h"
#include "WiFiPortal.h"   // AP_SSID (shared AP name)
#include "Version.h"
#include "Config.h"   // AP_SSID, OTA_IDLE_MS
#include "Settings.h"  // Settings_Backlight()

#include <WiFi.h>
#include <WebServer.h>      // part of the ESP32 core - no extra library needed
#include <ElegantOTA.h>

// ElegantOTA's DEFAULT mode is the synchronous WebServer, so no library file has
// to be patched and the async stack (ESPAsyncWebServer + AsyncTCP) is not
// needed at all. That also avoids the HTTP_* enum clash between
// ESPAsyncWebServer.h and the core's http_parser.h.
static WebServer s_server(80);
static volatile bool  s_busy = false;        // an upload is in progress

// --- Screens (drawn onto the shared PSRAM canvas, one flush each) ---
static void drawInfo() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Aktualizace firmware", 34, C_CYAN, 2);
  { char v[28]; snprintf(v, sizeof(v), "chiptron.cz  |  nyni v%s", FW_VERSION);
    UI_TextCentered(v, 58, C_GRAY, 1); }
  UI_TextCentered("1) Pripoj se na WiFi:", 80, C_GRAY, 1);

  const int qr = 176;
  UI_DrawWifiQR(AP_SSID, "", /*open=*/true, (LCD_WIDTH - qr) / 2, 98, qr);

  UI_TextCentered("MeteoPlaneRadar  (bez hesla)", 290, C_WHITE, 1);
  UI_TextCentered("2) Otevri  192.168.4.1/update", 312, C_YELLOW, 1);
  UI_TextCentered("3) Nahraj  MeteoPlaneRadar.ino.bin", 332, C_GRAY, 1);
  UI_TextCentered("Klepnutim se vratis (restart)", 356, C_GRAY, 1);
  gfx->flush();
}

// Drawn ONCE when the upload starts. It is shown for a moment and then the
// BACKLIGHT IS TURNED OFF for the duration of the transfer.
//
// Why: on the ESP32-S3 the RGB panel streams the framebuffer out of PSRAM
// continuously. Writing to flash (which OTA does non-stop) has to suspend the
// cache, the DMA misses its data and the picture drifts/jumps. That is a
// hardware property, not something the drawing code can avoid - so instead of
// showing a jumping image we simply blank the display while flashing.
//
// Note: the built-in GFX font is ASCII only, so the text is without diacritics
// (same as the other on-screen strings in this project).
static void drawUpdating() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Probiha aktualizace...", LCD_HEIGHT / 2 - 10, C_WHITE, 2);
  UI_TextCentered("Neodpojuj napajeni", LCD_HEIGHT / 2 + 20, C_GRAY, 1);
  gfx->flush();
}

// --- ElegantOTA callbacks ---
static void onStart() {
  s_busy = true;
  drawUpdating();
  delay(900);          // long enough to read the message
  Set_Backlight(0);    // display off - see the note above
}

static void onProgress(size_t cur, size_t total) {
  (void)cur; (void)total;
  // Deliberately NO drawing here - see drawUpdating(). The browser shows the
  // real progress bar; the device only needs to stay alive.
  Watchdog_Feed();
}

static void onEnd(bool ok) {
  // Flash writing is over, the RGB DMA can keep sync again -> light it back up.
  Set_Backlight(Settings_Backlight());
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(ok ? "Hotovo, restartuji..." : "Aktualizace selhala",
                  LCD_HEIGHT / 2, ok ? C_GREEN : C_RED, 2);
  gfx->flush();
  // On success ElegantOTA reboots the board by itself (setAutoReboot).
}

void OTA_Run() {
  // Open access point with a fixed, known address (192.168.4.1).
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  drawInfo();

  ElegantOTA.begin(&s_server);
  ElegantOTA.setAutoReboot(true);
  ElegantOTA.onStart(onStart);
  ElegantOTA.onProgress(onProgress);
  ElegantOTA.onEnd(onEnd);
  s_server.begin();

  // Stay here until an update reboots us, or the user taps to leave / times out.
  bool armed = false;                 // only accept a tap after the finger lifted
  unsigned long start = millis();
  for (;;) {
    s_server.handleClient();   // sync server: pump incoming requests
    ElegantOTA.loop();
    Watchdog_Feed();

    if (!s_busy) {
      TouchData t; Touch_Read(&t);
      if (t.points == 0) armed = true;
      else if (armed)    break;       // tap (after release) = exit
      if (millis() - start > OTA_IDLE_MS) break;
    }
    delay(5);
  }

  // Clean exit: drop the AP and reboot, which reconnects to the home WiFi and
  // restores the last screen/range (persisted in Settings).
  s_server.stop();
  WiFi.softAPdisconnect(true);
  delay(100);
  ESP.restart();
}
