#pragma once
// Set Wi-Fi locally; do not commit real passwords. GPIOs below are for the tested E32R40T-compatible board.
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define WORKER_BASE "https://iceflaredashboard.lanceshipley42.workers.dev"
#define PREFERRED_CHARACTER "Theundying"
// First-pass calibration from five portrait-orientation touch samples.
// Raw X decreases left-to-right; raw Y decreases top-to-bottom.
// Corners show skew; fine-tune after checking on-screen buttons.
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MIN_X 604
#define TOUCH_MAX_X 3597
#define TOUCH_MIN_Y 796
#define TOUCH_MAX_Y 3287
#define TOUCH_SWAP_XY false
#define TOUCH_INVERT_X true
#define TOUCH_INVERT_Y true
#define REFRESH_INTERVAL_MS 300000UL
