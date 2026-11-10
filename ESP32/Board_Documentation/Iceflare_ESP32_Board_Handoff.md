# ESP32-WROOM-32E 4-inch SPI Touchscreen: Hardware and Firmware Notes

A community-oriented reference for a **320 × 480, 4-inch ESP32-WROOM-32E resistive touchscreen board** with an **ST7796 display controller** and **XPT2046 touch controller**. The pin mapping below was tested on an **LCDWIKI E32R40T-compatible** board.

> **Scope:** These values describe one tested board variant. Visually similar ESP32 display modules may use different controllers, GPIO wiring, backlight polarity, or flash layouts. Confirm your board's schematic and markings before copying the settings.

## Hardware and pin mapping

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| LCD SPI clock (SCLK) | 14 | Shared SPI bus |
| LCD SPI MOSI | 13 | Shared SPI bus |
| LCD SPI MISO | 12 | Shared SPI bus |
| LCD chip select (CS) | 15 | TFT_eSPI |
| LCD data/command (DC) | 2 | TFT_eSPI |
| LCD reset | -1 | No dedicated reset pin; module reset via EN |
| LCD backlight | 27 | Set OUTPUT, HIGH to enable on tested board |
| Touch chip select | 33 | XPT2046 |
| Touch interrupt | 36 | Input-only GPIO, suitable for IRQ |

- LCD: **ST7796**, SPI, **320 × 480** pixels.
- Touch: **XPT2046**, resistive, SPI.
- Tested orientation: **portrait, rotation 0**.
- LCD and touch share the SPI bus.
- Tested TFT SPI clock: **27 MHz**.

**Backlight caveat:** A dark screen can simply mean the backlight GPIO was never enabled. On the tested board, `pinMode(27, OUTPUT); digitalWrite(27, HIGH);` turns it on. This polarity is not universal.

## PlatformIO / Arduino framework

A working setup used PlatformIO's `esp32dev` board with the Arduino framework, **TFT_eSPI**, **XPT2046_Touchscreen**, and (for optional JSON/networking) **ArduinoJson**. The display driver is configured through build flags rather than editing the TFT_eSPI library's global setup files.

Minimal example `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
  bodmer/TFT_eSPI@^2.5.43
  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git

build_flags =
  -D USER_SETUP_LOADED=1
  -D ST7796_DRIVER=1
  -D TFT_WIDTH=320
  -D TFT_HEIGHT=480
  -D TFT_MISO=12
  -D TFT_MOSI=13
  -D TFT_SCLK=14
  -D TFT_CS=15
  -D TFT_DC=2
  -D TFT_RST=-1
  -D SPI_FREQUENCY=27000000
  -D LOAD_GLCD=1
  -D LOAD_FONT2=1
  -D LOAD_FONT4=1
```

A registry dependency written as `paulstoffregen/XPT2046_Touchscreen @ ^1.4` failed to resolve in one tested PlatformIO environment (`UnknownPackageError`). The **direct GitHub URL** shown above installed successfully.

If using JPEG decoding, `bodmer/TJpg_Decoder@^1.1.0` was also used successfully. For JSON parsing, `bblanchon/ArduinoJson@^7.4.1` was used.

### Basic initialization example

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

constexpr int TOUCH_CS = 33;
constexpr int TOUCH_IRQ = 36;
constexpr int BACKLIGHT = 27;

TFT_eSPI tft;
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

void setup() {
  Serial.begin(115200);

  pinMode(BACKLIGHT, OUTPUT);
  digitalWrite(BACKLIGHT, HIGH);

  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);

  // SPI.begin(SCLK, MISO, MOSI)
  SPI.begin(14, 12, 13);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("ESP32 display test", 20, 20, 2);

  touch.begin(SPI);
  touch.setRotation(0);

  Serial.println("Display and touch initialized");
}

void loop() {
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    Serial.printf("Raw touch: x=%d y=%d z=%d\n", p.x, p.y, p.z);
    delay(150);
  }
}
```

This is a **minimal example**, not a complete calibrated UI. Test display and raw touch independently before adding network services or other application logic.

## Resistive touch calibration

Raw XPT2046 readings are **not pixel coordinates**. On the tested board, both raw axes decreased in the positive screen direction, and the touch sensor showed slight skew. A nine-point calibration with a two-dimensional affine transform provided a substantially better fit than simple min/max scaling.

For a portrait screen at rotation 0, the fitted transform was:

```text
screenX = 337.0592350311 - 0.0865459735 * rawX + 0.0007963339 * rawY
screenY = 508.5161895860 + 0.0008178298 * rawX - 0.1299426473 * rawY
```

Example implementation:

```cpp
TS_Point p = touch.getPoint();
float sx = 337.0592350311f
         - 0.0865459735f * p.x
         + 0.0007963339f * p.y;
float sy = 508.5161895860f
         + 0.0008178298f * p.x
         - 0.1299426473f * p.y;

int screenX = constrain((int)lroundf(sx), 0, 319);
int screenY = constrain((int)lroundf(sy), 0, 479);
```

**Do not additionally invert or swap axes** when using a transform that already accounts for orientation and skew. **Do not assume these coefficients are transferable to another unit**: calibrate your own panel, especially if rotation changes.

### Example nine-point measurements

| Target pixel (x, y) | Mean raw (x, y) | Fit error |
|---|---|---:|
| 32, 48 | 3540.4, 3573.0 | 1.7 px |
| 160, 48 | 2077.7, 3561.8 | 0.6 px |
| 288, 48 | 587.5, 3563.9 | 2.4 px |
| 32, 240 | 3577.9, 2079.5 | 3.2 px |
| 160, 240 | 2076.3, 2054.1 | 3.4 px |
| 288, 240 | 601.9, 2048.3 | 3.2 px |
| 32, 432 | 3512.2, 624.6 | 2.4 px |
| 160, 432 | 2042.6, 601.6 | 0.8 px |
| 288, 432 | 568.3, 607.9 | 2.0 px |

The least-squares affine fit produced **2.39 px RMS** and **3.45 px maximum** error *at its calibration targets*. These are fitting errors, not independent validation measurements.

For other panels, collect multiple raw readings per target and solve the affine model:

```text
screenX = a0 + a1 * rawX + a2 * rawY
screenY = b0 + b1 * rawX + b2 * rawY
```

Use at least three non-collinear points; nine well-spaced points help quantify and reduce measurement noise. Validate the resulting mapping with additional points not used for fitting.

## Flash partitions: firmware uploads but will not boot

One application image of **1,326,016 bytes** exceeded the default **1,310,720-byte** OTA app partition. The serial boot log reported:

```text
esp_image: Image length 1326016 doesn't fit in partition length 1310720
boot: OTA app partition slot 0 is not bootable
boot: No bootable app partitions in the partition table
```

In that case the ESP32 bootloader ran, but the application did not start—so even the backlight stayed off.

A working configuration for that firmware used:

```ini
board_build.partitions = min_spiffs.csv
```

This changes the flash partition layout. **Check the size and filesystem needs of your application** before choosing a partition scheme; `min_spiffs.csv` is not universally appropriate. A normal upload should flash the configured partition table, but stale builds or flashing a different environment can cause confusion. Full-chip erase is a last resort because it removes stored data.

## Optional HTTPS / ArduinoJson note

In one ESP32 HTTPS client, passing `HTTPClient::getStream()` directly into `deserializeJson()` resulted in `JSON: Incomplete`. Reading the response through HTTPClient first resolved the observed issue:

```cpp
int statusCode = http.GET();
if (statusCode == HTTP_CODE_OK) {
  String body = http.getString();  // HTTPClient handles transfer framing
  DeserializationError err = deserializeJson(doc, body);
  // Check err and handle failures.
}
http.end();
```

This is a **diagnostic example**, not proof that all incomplete-JSON errors are caused by chunked responses. Log HTTP status, declared content length, received bytes, and available heap. For production HTTPS, validate the server certificate rather than disabling TLS verification.

## Troubleshooting checklist

| Symptom | Likely area to investigate |
|---|---|
| Black screen and no backlight | Boot log first; verify firmware starts and GPIO27 is driven HIGH |
| Backlight on, no visible graphics | ST7796 driver selection, SPI pins, CS/DC, rotation, display initialization |
| Raw touch values never appear | Touch CS33, IRQ36, shared SPI initialization, touch library |
| Touch is offset or reversed | Calibrate raw coordinates; check rotation; avoid applying inversion twice |
| PlatformIO cannot find XPT2046 library | Use the direct GitHub dependency URL above |
| Upload succeeds but boot reports no app | Inspect partition table and image size |
| JSON parser reports `Incomplete` | Inspect HTTP status/body length and transfer decoding |
| Random resets or brownout errors | Inspect power supply, USB cable, and serial logs |

### Recommended bring-up order

1. Confirm the board variant and pin assignments.
2. Build a minimal **display-only** test with backlight enabled.
3. Read **raw touchscreen** coordinates over serial.
4. Calibrate touch and verify against screen targets.
5. Add UI drawing and touch navigation.
6. Add storage, Wi-Fi, HTTPS, or other libraries one at a time.
7. Check flash partition size and memory usage as firmware grows.

---

**Sharing note:** This document intentionally omits private credentials, local filesystem paths, repository/account references, and application-specific details. It is a record of one tested board configuration, not an official manufacturer pinout or a guarantee for every similarly labeled module.
