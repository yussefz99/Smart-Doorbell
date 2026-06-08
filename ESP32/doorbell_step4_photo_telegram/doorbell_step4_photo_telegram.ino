// ============================================================
//  Smart Doorbell – Step 4: Capture photo → send to Telegram
//  Board : AI Thinker ESP32-CAM
//  Tests : WiFi → camera capture → sendPhoto to Telegram
//  LEDs  : green=idle/ok, red=capturing, yellow=sending
// ============================================================

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ── Credentials ──────────────────────────────────────────────
const char* WIFI_SSID     = "Yussef";
const char* WIFI_PASSWORD = "yussef7855";
const char* BOT_TOKEN     = "8826200691:AAHa7xik9owDwAcb06JDhBk6JNbwPgpZcwc";
const char* CHAT_ID       = "1078220538";
// ─────────────────────────────────────────────────────────────

// ── AI Thinker ESP32-CAM pin definition ──────────────────────
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
#define LED_YELLOW  15

#define WIFI_TIMEOUT_MS 15000

// ── Forward declarations ──────────────────────────────────────
bool connectWiFi();
bool initCamera();
camera_fb_t* capturePhoto();
bool sendPhotoToTelegram(camera_fb_t* fb);
void blinkLED(int pin, int times, int onMs, int offMs);
void setLED(int pin, bool state);
void allLEDsOff();

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  allLEDsOff();

  Serial.println("\n=== Smart Doorbell – Step 4: Photo to Telegram ===");

  // Step 1 — WiFi
  if (!connectWiFi()) {
    Serial.println("[ERROR] WiFi failed.");
    blinkLED(LED_RED, 6, 200, 200);
    return;
  }
  setLED(LED_GREEN, true);
  Serial.println("[OK] WiFi connected: " + WiFi.localIP().toString());

  // Step 2 — Camera
  if (!initCamera()) {
    Serial.println("[ERROR] Camera init failed.");
    blinkLED(LED_RED, 6, 200, 200);
    return;
  }
  Serial.println("[OK] Camera initialised.");

  // Step 3 — Capture
  setLED(LED_GREEN, false);
  setLED(LED_RED, true);   // red = capturing
  Serial.println("[Camera] Capturing photo...");
  delay(1000);  // let camera stabilise

  camera_fb_t* fb = capturePhoto();
  if (!fb) {
    Serial.println("[ERROR] Capture failed.");
    blinkLED(LED_RED, 6, 200, 200);
    return;
  }
  Serial.println("[OK] Photo captured: " + String(fb->len) + " bytes.");

  // Step 4 — Send
  setLED(LED_RED, false);
  setLED(LED_YELLOW, true);  // yellow = sending
  Serial.println("[Telegram] Sending photo...");

  bool ok = sendPhotoToTelegram(fb);
  esp_camera_fb_return(fb);  // free memory

  if (ok) {
    Serial.println("[OK] Photo sent to Telegram successfully.");
    setLED(LED_YELLOW, false);
    blinkLED(LED_GREEN, 3, 200, 200);
    setLED(LED_GREEN, true);  // back to idle
  } else {
    Serial.println("[ERROR] Failed to send photo.");
    setLED(LED_YELLOW, false);
    blinkLED(LED_RED, 6, 200, 200);
  }
}

void loop() {}

// ── WiFi ──────────────────────────────────────────────────────
bool connectWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) return false;
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  return true;
}

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
  config.frame_size   = FRAMESIZE_QVGA;  // 320x240 — good balance of size/speed
  config.jpeg_quality = 10;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init error: 0x%x\n", err);
    return false;
  }
  return true;
}

// ── Capture ───────────────────────────────────────────────────
camera_fb_t* capturePhoto() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return nullptr;
  return fb;
}

// ── Send photo to Telegram ────────────────────────────────────
// Uses multipart/form-data to POST the JPEG bytes directly
bool sendPhotoToTelegram(camera_fb_t* fb) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[Telegram] Connection failed.");
    return false;
  }

  String boundary = "----SmartDoorbell";
  String caption  = "Smart%20Doorbell%20visitor%20photo";

  // Build the multipart body parts
  String bodyStart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
    + String(CHAT_ID) + "\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"caption\"\r\n\r\n"
    "Smart Doorbell visitor photo\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"doorbell.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  int totalLen = bodyStart.length() + fb->len + bodyEnd.length();

  // Send HTTP headers
  client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(totalLen));
  client.println("Connection: close");
  client.println();

  // Send body
  client.print(bodyStart);
  // Send photo bytes in chunks to avoid memory overflow
  uint8_t* buf = fb->buf;
  size_t   len = fb->len;
  size_t   chunkSize = 1024;
  while (len > 0) {
    size_t toSend = min(len, chunkSize);
    client.write(buf, toSend);
    buf += toSend;
    len -= toSend;
  }
  client.print(bodyEnd);

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 15000) {
      Serial.println("[Telegram] Response timeout.");
      client.stop();
      return false;
    }
  }

  // Read response status line
  String statusLine = client.readStringUntil('\n');
  Serial.println("[Telegram] Response: " + statusLine);
  client.stop();

  return statusLine.indexOf("200") > 0;
}

// ── LED helpers ───────────────────────────────────────────────
void blinkLED(int pin, int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH); delay(onMs);
    digitalWrite(pin, LOW);  delay(offMs);
  }
}
void setLED(int pin, bool state) { digitalWrite(pin, state ? HIGH : LOW); }
void allLEDsOff() {
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_YELLOW, LOW);
}
