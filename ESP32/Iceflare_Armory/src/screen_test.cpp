#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// E32R40T-compatible hardware test. No Wi-Fi, no user secrets.
constexpr int BACKLIGHT_PIN = 27;
constexpr int TOUCH_CS_PIN = 33;
constexpr int TOUCH_IRQ_PIN = 36;
TFT_eSPI display = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
bool touchReady = false;
uint32_t lastReport = 0;

void drawTest() {
  display.fillScreen(TFT_NAVY);
  display.fillRect(0, 0, 320, 54, TFT_ORANGE);
  display.setTextColor(TFT_BLACK, TFT_ORANGE);
  display.drawCentreString("ICEFLARE ARMORY", 160, 15, 4);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  display.drawCentreString("ESP32 DISPLAY TEST", 160, 80, 2);
  display.fillRect(25, 128, 85, 65, TFT_RED);
  display.fillRect(118, 128, 85, 65, TFT_GREEN);
  display.fillRect(211, 128, 85, 65, TFT_BLUE);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  display.drawCentreString("Touch anywhere", 160, 235, 2);
  display.drawCentreString("Raw touch values appear", 160, 270, 2);
  display.drawCentreString("here and in Serial Monitor", 160, 293, 2);
  display.drawCentreString("ST7796 / XPT2046 test", 160, 440, 2);
}
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("Iceflare screen test starting");
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  SPI.begin(14, 12, 13);
  display.init();
  display.setRotation(0);
  drawTest();
  touchReady = touch.begin(SPI);
  if (touchReady) touch.setRotation(0);
  Serial.printf("Touch initialized: %s\n", touchReady ? "yes" : "no");
}
void loop() {
  if (!touchReady || !touch.touched()) return;
  TS_Point p = touch.getPoint();
  if (millis() - lastReport < 120) return;
  lastReport = millis();
  Serial.printf("Touch raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
  display.fillRect(0, 335, 320, 42, TFT_NAVY);
  display.setTextColor(TFT_YELLOW, TFT_NAVY);
  display.drawCentreString(String("x=") + p.x + "  y=" + p.y + "  z=" + p.z, 160, 344, 2);
}
