#pragma once
// EDIT THESE VALUES. The GPIO pins in platformio.ini are EXAMPLES ONLY.
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define WORKER_BASE "https://iceflaredashboard.lanceshipley42.workers.dev"
#define PREFERRED_CHARACTER "Theundying"
// Example XPT2046 pins; NOT verified for the photographed board.
#define TOUCH_CS  5
#define TOUCH_IRQ 255
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3800
#define TOUCH_MIN_Y 200
#define TOUCH_MAX_Y 3800
#define TOUCH_SWAP_XY false
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y false
#define REFRESH_INTERVAL_MS 300000UL
