# Iceflare ESP32 Touchscreen Board — Engineering Handoff

**Status:** Working hardware + Armory firmware baseline, verified by user on 2026-11-10.  
**Board:** 4-inch 320×480 resistive touchscreen ESP32-WROOM-32E, LCDWIKI E32R40T-compatible.  
**Display controller:** ST7796 (SPI). **Touch controller:** XPT2046 (SPI).  
**Primary repository:** https://github.com/iceflaretechstudios/wow-forever-char-dev  
**Armory project folder:** `ESP32/Iceflare_Armory`.

## 1. Confirmed hardware configuration

| Signal | ESP32 GPIO | Notes |
|---|---:|---|
| LCD SPI SCLK | 14 | Shared SPI bus |
| LCD SPI MOSI | 13 | Shared SPI bus |
| LCD SPI MISO | 12 | Shared SPI bus |
| LCD CS | 15 | TFT_eSPI |
| LCD DC | 2 | TFT_eSPI |
| LCD RESET | -1 | Board reset/EN; no separate software-controlled reset |
| LCD backlight | 27 | **Must set OUTPUT and HIGH**; otherwise screen may appear completely dead |
| Touch CS | 33 | XPT2046 |
| Touch IRQ | 36 | Input-only pin; valid for IRQ |

Display uses **portrait rotation 0**, screen dimensions **320×480**. LCD and touch share SPI, so explicitly initialize `SPI.begin(14, 12, 13)` and use `touch.begin(SPI)`. Tested TFT SPI clock setting: 27 MHz.

**Critical startup order** (from working Armory firmware):

```cpp
Serial.begin(115200);
delay(300);
pinMode(27, OUTPUT);
digitalWrite(27, HIGH);       // backlight ON
pinMode(TOUCH_CS, OUTPUT);
digitalWrite(TOUCH_CS, HIGH);
SPI.begin(14, 12, 13);        // SCLK, MISO, MOSI
tft.init();
tft.setRotation(0);
touch.begin(SPI);
touch.setRotation(0);
```

The working firmware also initializes LittleFS and its JPEG decoder and draws the interface after Wi-Fi/API setup. Do not replace the working pin map with generic ESP32 examples.

## 2. PlatformIO configuration and build

Use **VS Code + PlatformIO**, board `esp32dev`, framework `arduino`, Windows USB port **COM4** (verify port if it changes). PlatformIO executable:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
```

Important `platformio.ini` entries (verify against current GitHub source):

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
board_build.partitions = min_spiffs.csv
build_src_filter =
  +<main.cpp>
  -<screen_test.cpp>
lib_deps =
  bodmer/TFT_eSPI@^2.5.43
  bblanchon/ArduinoJson@^7.4.1
  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
  bodmer/TJpg_Decoder@^1.1.0
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

The actual repository may include additional flags and a `screen_test` environment. Preserve them. **Do not use** `paulstoffregen/XPT2046_Touchscreen @ ^1.4`: it produced `UnknownPackageError`. The direct GitHub library URL worked.

**Critical partition issue:** without `board_build.partitions = min_spiffs.csv`, a 1,326,016-byte firmware image did not fit the default 1,310,720-byte OTA application partition. Serial showed:

```
Image length 1326016 doesn't fit in partition length 1310720
No bootable app partitions in the partition table
```

This caused a black screen and no backlight because the application never booted. Correcting the partition configuration and reflashing fixed the boot. **A successful upload does not guarantee a successful boot.** If needed, inspect boot output at 115200 baud. Do not erase the whole flash unless necessary; erasure removes stored data.

## 3. Nine-point touchscreen calibration — measured and validated

A standalone **Iceflare Touch Calibrator** project was built to sample nine targets, solve a two-dimensional affine transform, report errors and export JSON. It ran on this board and produced:

- **RMS calibration-point error:** 2.39 pixels
- **Maximum calibration-point error:** 3.45 pixels
- **Orientation:** rotation 0, 320×480
- User subsequently confirmed touch is **much more accurate** in the Armory.

Calibration output:

```json
{"ax":[337.0592350311,-0.0865459735,0.0007963339],"ay":[508.5161895860,0.0008178298,-0.1299426473],"rms":2.39,"max":3.45}
```

Mapping from **raw XPT2046** readings to screen pixels:

```cpp
float screenX = 337.0592350311f - 0.0865459735f * p.x + 0.0007963339f * p.y;
float screenY = 508.5161895860f + 0.0008178298f * p.x - 0.1299426473f * p.y;
int x = constrain((int)lroundf(screenX), 0, 319);
int y = constrain((int)lroundf(screenY), 0, 479);
```

The coefficients account for reversed axes and slight skew. **Do not additionally swap/invert/map axes** after applying the affine transform. This calibration is for the specific tested board, orientation and assembly; recalibrate if hardware/orientation changes. RMS error above is measured on the same nine fitting targets, not independent holdout measurements.

| Target screen (x,y) | Raw (x,y) | Fit error |
|---|---|---:|
| 32,48 | 3540.4,3573.0 | 1.7 px |
| 160,48 | 2077.7,3561.8 | 0.6 px |
| 288,48 | 587.5,3563.9 | 2.4 px |
| 32,240 | 3577.9,2079.5 | 3.2 px |
| 160,240 | 2076.3,2054.1 | 3.4 px |
| 288,240 | 601.9,2048.3 | 3.2 px |
| 32,432 | 3512.2,624.6 | 2.4 px |
| 160,432 | 2042.6,601.6 | 0.8 px |
| 288,432 | 568.3,607.9 | 2.0 px |

The standalone calibrator was delivered as `Iceflare_Touch_Calibrator.zip` in the original conversation; it is separate from the main Armory project. It captures nine targets, prints the fitted transform and supports touch verification. Do not assume the calibrator is checked into the Armory repository.

## 4. Cloudflare connection — confirmed fix

The display booted successfully but initially showed `Connecting to Cloudflare... JSON: Incomplete`. Reviewing GitHub history showed that the API JSON-reading code had not changed during the recent hardware updates. The old implementation parsed directly from the underlying HTTP stream:

```cpp
DeserializationError err = deserializeJson(out, http.getStream());
```

The working fix uses HTTPClient to obtain the decoded response **before** passing it to ArduinoJson:

```cpp
int code = http.GET();
if (code != 200) { /* log status and handle error */ }
String payload = http.getString();
http.end();
DeserializationError err = deserializeJson(out, payload);
```

`HTTPClient::getString()` handles HTTP transfer framing (including chunked responses) whereas direct `getStream()` parsing may expose incomplete/framed transport data. The change also added serial diagnostics for HTTP status, declared length, bytes received, and free heap. **User confirmed this update worked perfectly.** The specific root cause of the original incomplete response was not independently proven; preserve the working approach and logging.

Connection is via Wi-Fi and a Cloudflare Worker configured in `include/config.h`. **Keep Wi-Fi credentials local; never commit actual passwords.** The current code uses `WiFiClientSecure::setInsecure()` as a prototype expedient; for production, validate the server certificate/CA rather than disabling TLS verification.

## 5. Source-control workflow — important lesson

There were multiple local copies of the Armory. One older copy lacked backlight/SPI initialization, the correct partition configuration, and new touch mapping even though GitHub had fixes. Running `git pull` outside a `.git` repository produced `fatal: not a git repository`. Running PlatformIO from the repo root produced `NotPlatformIOProjectError` because `platformio.ini` is nested.

**Working recommended local clone:**

```
C:\Users\lance\Pictures\boards\Iceflare_Armory_Latest
  └─ ESP32\Iceflare_Armory\platformio.ini
```

Pull from the **repository root**; build from the **PlatformIO project directory**:

```powershell
cd "C:\Users\lance\Pictures\boards\Iceflare_Armory_Latest"
git pull origin main
cd ESP32\Iceflare_Armory
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32dev
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32dev -t upload --upload-port COM4
```

Monitor (close it before uploading):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COM4 --baud 115200
```

If `git pull` reports local modifications or conflicts, **stop and review** before overwriting anything, especially `include/config.h` with local Wi-Fi settings. Recommended workflow: make and commit source changes in GitHub, then pull/build/upload/test on the device.

## 6. Troubleshooting decision tree

| Symptom | Check first | Known solution / next action |
|---|---|---|
| No backlight, black screen | Serial boot output | If `No bootable app partitions`, fix partition configuration and reflash |
| Bootloader hands off, still black | Serial startup diagnostics | Check whether `SETUP STARTED`, `BACKLIGHT GPIO27 SET HIGH`, `LCD INITIALIZED` appear; verify code actually flashed |
| Backlight on but no drawing | TFT driver and pin flags | Use ST7796, rotation 0, shared SPI wiring and TFT_eSPI setup |
| Touch dead or inaccurate | Touch init + calibration | `touch.begin(SPI)`, CS33, IRQ36, apply nine-point affine transform exactly once |
| `UnknownPackageError` for touch library | `lib_deps` | Use GitHub URL for PaulStoffregen XPT2046_Touchscreen |
| `JSON: Incomplete` | HTTP status and response byte count | Use `http.getString()` before `deserializeJson`; preserve diagnostics |
| `NotPlatformIOProjectError` | Working directory | `cd ESP32\Iceflare_Armory` where `platformio.ini` lives |
| `fatal: not a git repository` | Working directory | Run git from cloned repo root, not an arbitrary project folder |
| Upload fails on COM4 | Serial Monitor / port | Close monitor, verify Device Manager COM port, retry |

## 7. Confirmed working baseline / provenance

- Hardware startup logging added in GitHub commit `ad78765fe19ccd491fed6ce88094701b4fccdb3b`.
- Nine-point affine mapping committed in `c70733e59144663de45ccaf91f271eae76a40c96` (coefficients in preceding commit `d71cc5ec66e22ccffd912245195e0c7086326f1d`).
- Cloudflare response-handling fix committed in **`0f084d9e92ca98f45ba132a3d0cd7f22891c4a24`**.
- User tested after that fix: **Cloudflare connection worked perfectly and touch was much more accurate.** This is the last confirmed working firmware baseline described in the conversation.
- Main application has Armory, Stats, Talents and Prof navigation, with equipment scrolling. Preserve these features while iterating.

## 8. Guidance for the next engineer / AI assistant

1. **Read the current GitHub `ESP32/Iceflare_Armory` files before editing**; don't assume an old local snippet matches the latest firmware.
2. Preserve the tested ST7796 + XPT2046 pin mapping, GPIO27 backlight setup, shared SPI initialization, `min_spiffs.csv` partition setting, and nine-point affine transform.
3. Keep the Cloudflare HTTP response buffering fix; avoid reverting to direct JSON parsing from `http.getStream()` without validating framing.
4. Make small, reviewable GitHub commits. Give exact Windows PowerShell pull/build/upload commands with the correct working directory.
5. Keep Wi-Fi passwords out of GitHub. Do not overwrite local `config.h` without considering local credentials.
6. When troubleshooting, ask for **complete 115200-baud serial output after reset**, not just the on-screen error.
7. Treat the commit above as the **last user-confirmed functional baseline**, not as proof that future edits will work without physical testing.

---
*Prepared from the development conversation, actual calibration output, inspected GitHub source, and user-confirmed test results. Pin assignments and calibration values are specific to this tested board.*
