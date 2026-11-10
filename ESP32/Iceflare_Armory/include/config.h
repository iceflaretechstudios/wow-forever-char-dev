#pragma once
// Set Wi-Fi locally; do not commit real passwords. GPIOs below are for the tested E32R40T-compatible board.
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define WORKER_BASE "https://iceflaredashboard.lanceshipley42.workers.dev"
#define PREFERRED_CHARACTER "Theundying"
// Nine-point affine calibration, portrait rotation 0, 320x480.
// Calibrator RMS 2.39 px; max calibration-target error 3.45 px.
// Coefficients map raw XPT2046 values directly to screen pixels.
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_AFFINE_AX0 337.0592350311f
#define TOUCH_AFFINE_AX1 -0.0865459735f
#define TOUCH_AFFINE_AX2 0.0007963339f
#define TOUCH_AFFINE_AY0 508.5161895860f
#define TOUCH_AFFINE_AY1 0.0008178298f
#define TOUCH_AFFINE_AY2 -0.1299426473f
#define REFRESH_INTERVAL_MS 300000UL
