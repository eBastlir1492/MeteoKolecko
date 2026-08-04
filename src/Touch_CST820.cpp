// =============================================================================
//  MeteoPlaneRadar
//  CST820 capacitive touch controller.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "Touch_CST820.h"
#include "TCA9554.h"
#include "Display_ST7701.h"   // LCD_WIDTH / LCD_HEIGHT
#include "Config.h"           // TOUCH_DEBUG
#include <Wire.h>

static bool readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(CST820_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t got = Wire.requestFrom((int)CST820_ADDR, (int)len);
  if (got != len) { while (Wire.available()) Wire.read(); return false; }
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool Touch_Init() {
  // Reset via EXIO2.
  TCA9554_SetPin(EXIO_TOUCH_RST, false);
  delay(10);
  TCA9554_SetPin(EXIO_TOUCH_RST, true);
  delay(50);

  pinMode(CST820_INT_PIN, INPUT_PULLUP);

  // Try reading the chip ID (register 0xA7) as a communication test.
  uint8_t id = 0;
  if (readRegs(0xA7, &id, 1)) {
    Serial.printf("CST820 ID: 0x%02X\n", id);
    return true;
  }
  Serial.println("CST820 nereaguje na I2C");
  return false;
}

#if TOUCH_DEBUG
static uint32_t s_badReads = 0;      // rejected samples since the last report
static uint32_t s_badReported = 0;   // millis() of the last report
#endif

// Consecutive rejected samples. A handful is normal on a busy bus; a long run
// means the controller is wedged and needs a reset. Counted regardless of
// TOUCH_DEBUG, because the recovery below depends on it.
static uint16_t s_badRun = 0;

// Report a rejected sample at most once a second, so a noisy bus cannot flood
// the console (and slow everything down in the process).
static void noteBadSample(const char* why) {
  s_badRun++;
#if TOUCH_DEBUG
  s_badReads++;
  uint32_t now = millis();
  if (now - s_badReported >= 1000) {
    s_badReported = now;
    Serial.printf("TOUCH: zahozeno %lu vadnych cteni (%s)\n",
                  (unsigned long)s_badReads, why);
    s_badReads = 0;
  }
#else
  (void)why;
#endif
}

// An I2C glitch can leave the CST820 answering but talking nonsense, and it
// never recovers on its own - the touchscreen simply stops working until the
// board is power-cycled. A long run of rejected samples is the symptom, so
// reset the chip and start over.
static void recoverIfWedged() {
  if (s_badRun < TOUCH_REINIT_BAD) return;
  s_badRun = 0;
#if TOUCH_RECOVERY
  // Rate-limited on purpose - see the comment at TOUCH_RECOVERY in Config.h.
  static unsigned long lastTry = 0;
  static bool firstTry = true;
  if (!firstTry && millis() - lastTry < TOUCH_RECOVERY_MIN_MS) return;
  firstTry = false;
  lastTry = millis();
  Serial.println("TOUCH: prilis mnoho vadnych cteni, resetuji radic");
  Touch_Init();
  // The reset went through the expander that also holds the display's power
  // and reset lines, and the bus was already misbehaving. Make sure we did not
  // take the panel down with us.
  TCA9554_Verify();
#else
  // Kept even in a release build - this is the one line that would tell us the
  // wedged controller is a real thing and not just a theory. Rate-limited so it
  // cannot flood the console on a permanently noisy bus.
  static unsigned long lastMsg = 0;
  if (lastMsg == 0 || millis() - lastMsg >= TOUCH_RECOVERY_MIN_MS) {
    lastMsg = millis();
    Serial.println("TOUCH: mnoho vadnych cteni (obnova radice je vypnuta)");
  }
#endif
}

void Touch_Read(TouchData* out) {
  out->points = 0;
  uint8_t buf[6] = {};
  // Register 0x02 = number of points, followed by the coordinates.
  if (!readRegs(0x02, buf, 6)) { noteBadSample("I2C cteni selhalo"); recoverIfWedged(); return; }

  // --- Sanity checks on the data itself -------------------------------------
  // The I2C transfer can succeed (the chip ACKs) and still hand back garbage,
  // typically all 0xFF. Decoded naively that is "15 points at (4095, 4095)",
  // which the UI then treats as a real tap somewhere off the map - and that is
  // exactly what kept closing the aircraft detail panel on its own.
  if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF) {
    noteBadSample("same 0xFF");
    recoverIfWedged();
    return;
  }

  uint8_t points = buf[0] & 0x0F;
  if (points == 0) { s_badRun = 0; return; }   // no finger - the normal case
  if (points > 1) {                     // CST820 is a single-touch controller
    noteBadSample("nesmyslny pocet bodu");
    recoverIfWedged();
    return;
  }

  uint16_t x = ((buf[1] & 0x0F) << 8) | buf[2];
  uint16_t y = ((buf[3] & 0x0F) << 8) | buf[4];
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {   // outside the panel = not real
    noteBadSample("souradnice mimo displej");
    recoverIfWedged();
    return;
  }

  s_badRun = 0;          // a fully valid sample - the controller is healthy
  out->points = points;
  out->x = x;
  out->y = y;
}
