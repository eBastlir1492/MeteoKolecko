// =============================================================================
//  MeteoPlaneRadar
//  OTA firmware update over WiFi (ElegantOTA web upload) - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
//
//  Requires a DUAL-APP (OTA) partition scheme - see partitions.csv.
//  Library: ElegantOTA (ayushsharma82) in its default synchronous mode -
//  uses the core's WebServer, so nothing else needs installing or patching.
// =============================================================================
#pragma once
#include <Arduino.h>

// Blocking OTA mode: brings up an open access point "MeteoPlaneRadar",
// serves the ElegantOTA upload page at http://192.168.4.1/update and draws the
// join instructions (QR) on the display. Returns only after a successful update
// (device reboots) or when the user taps to exit / an idle timeout elapses
// (device reboots to resume normal operation).
void OTA_Run();
