# Iceflare Armory — ESP32 touchscreen

Dedicated ESP32 firmware companion for the Iceflare Cloudflare WoW armory dashboard.

Hardware target: ESP32-32E, 4-inch 320×480 resistive touchscreen.

The original Cloudflare Worker and D1 deployment are unchanged.

**Status:** initial PlatformIO firmware source exists as a downloadable ZIP in the ChatGPT project conversation. Display controller and touch pin mapping have not been hardware-verified; do not flash with example GPIO values.

**Files in the source ZIP:** `platformio.ini`, `src/main.cpp`, `include/config.h`, `tools/prepare_portrait.py`, `README.md`.

**Security:** keep Wi-Fi credentials out of GitHub. Use local configuration for real passwords.
