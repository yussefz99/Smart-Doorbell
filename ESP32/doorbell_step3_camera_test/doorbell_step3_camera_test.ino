// ============================================================
//  Smart Doorbell – Step 3: Camera capture test
//  Board : AI Thinker ESP32-CAM
//  Tests : Camera initialises → captures one photo → reports size
//  LEDs  : green = OK, red = error
// ============================================================

#include "esp_camera.h"

// ── AI Thinker ESP32-CAM pin definition ─────────────────────
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_GREEN   2
#define LED_RED     4

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   LOW);

  Serial.println("\n=== Smart Doorbell – Step 3: Camera Test ===");

  if (!initCamera()) {
    Serial.println("[ERROR] Camera init failed. Check connections.");
    blinkLED(LED_RED, 6, 200, 200);
    return;
  }

  Serial.println("[OK] Camera initialised.");
  digitalWrite(LED_GREEN, HIGH);
  delay(500);

  captureAndReport();
}

void loop() {}

// ── Camera init ───────────────────────────────────────────────
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Start with lower quality to ensure stable capture
  config.frame_size   = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 10;              // 0-63, lower = better quality
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init error: 0x%x\n", err);
    return false;
  }

  // Adjust sensor settings for better image
  sensor_t* s = esp_camera_sensor_get();
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, 0);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_exposure_ctrl(s, 1);

  return true;
}

// ── Capture photo and report ──────────────────────────────────
void captureAndReport() {
  Serial.println("[Camera] Capturing photo...");

  // Small delay to let camera stabilise
  delay(1000);

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("[ERROR] Capture failed — frame buffer is null.");
    blinkLED(LED_RED, 6, 200, 200);
    return;
  }

  Serial.println("[OK] Photo captured successfully!");
  Serial.println("──────────────────────────────");
  Serial.println("  Format : JPEG");
  Serial.println("  Width  : " + String(fb->width)  + " px");
  Serial.println("  Height : " + String(fb->height) + " px");
  Serial.println("  Size   : " + String(fb->len)    + " bytes ("
                               + String(fb->len / 1024) + " KB)");
  Serial.println("──────────────────────────────");
  Serial.println("[OK] Camera test passed. Ready for Step 4.");

  // Return frame buffer to free memory
  esp_camera_fb_return(fb);

  // Blink green 3x to signal success
  blinkLED(LED_GREEN, 3, 200, 200);
  digitalWrite(LED_GREEN, HIGH);
}

// ── LED helpers ───────────────────────────────────────────────
void blinkLED(int pin, int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH); delay(onMs);
    digitalWrite(pin, LOW);  delay(offMs);
  }
}
