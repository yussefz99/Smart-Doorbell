// ============================================================
//  Smart Doorbell – Step 5: Complete V1
//  Board  : AI Thinker ESP32-CAM (CS-CAM carrier board)
//  Button : External tactile button on GPIO 13
//           (GPIO 0 = camera XCLK — cannot be used as button)
//  LED    : Built-in flash LED (GPIO 4) — capturing indicator
//  Flow   : Button press → capture photo → send to Telegram
//           → save to backend → appear on dashboard live
// ============================================================

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ── Credentials ──────────────────────────────────────────────
// Real values live in secrets.h (gitignored — never committed).
#include "secrets.h"
const char* WIFI_SSID     = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const char* BOT_TOKEN     = SECRET_BOT_TOKEN;
const char* CHAT_ID       = SECRET_CHAT_ID;

// ── Backend server URL ────────────────────────────────────────
// Permanent cloud deployment (Railway + Supabase) — no ngrok.
// Leave empty to skip backend posting (Telegram only).
// ─────────────────────────────────────────────────────────────
const char* BACKEND_URL   = "https://smart-doorbell-production.up.railway.app";

// ── Pin definitions ───────────────────────────────────────────
// IMPORTANT: GPIO 0 = camera XCLK — it is driven as a 20 MHz
// clock after esp_camera_init(). It CANNOT be read as a button.
// Use GPIO 13 for the external tactile button instead.
// Wiring: one button leg → GPIO 13 pin on CS-CAM
//         other button leg → GND pin on CS-CAM
#define BUTTON_PIN  13   // External tactile button (INPUT_PULLUP)
#define FLASH_LED    4   // Built-in white flash LED

// Demo mode while the physical button is not yet wired:
// fire ONE doorbell event automatically after boot, so pressing
// the carrier's RESET button acts as the doorbell.
// Set to false once the real button is soldered to GPIO 13.
#define TRIGGER_ON_BOOT  true

unsigned long lastDebounceTime  = 0;
int           lastButtonState   = HIGH;

// ── AI Thinker ESP32-CAM camera pins ─────────────────────────
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

// ── Debounce ──────────────────────────────────────────────────
#define DEBOUNCE_MS      50
#define MIN_PRESS_GAP  3000   // minimum ms between two doorbell triggers
unsigned long lastPressTime = 0;

// ── Forward declarations ──────────────────────────────────────
bool connectWiFi();
bool initCamera();
camera_fb_t* capturePhoto();
bool sendPhotoToTelegram(camera_fb_t* fb);
void sendTextNotification(const String& message);
void postVisitToBackend();
void triggerDoorbell();
void blinkFlash(int times);
void setFlash(bool on);

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(FLASH_LED,  OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setFlash(false);

  Serial.println("\n=== Smart Doorbell V1 — Ready ===");

  // WiFi
  if (!connectWiFi()) {
    Serial.println("[ERROR] WiFi failed — check credentials.");
    blinkFlash(10);
    return;
  }
  Serial.println("[OK] WiFi connected: " + WiFi.localIP().toString());

  // Camera
  if (!initCamera()) {
    Serial.println("[ERROR] Camera init failed.");
    blinkFlash(10);
    return;
  }
  Serial.println("[OK] Camera ready.");

  // Ready signal — 3 quick flashes
  blinkFlash(3);

#if TRIGGER_ON_BOOT
  // Demo mode: RESET button = doorbell. One event per boot.
  Serial.println("[DEMO] TRIGGER_ON_BOOT active — firing doorbell now.");
  triggerDoorbell();
#endif

  Serial.println("[OK] Doorbell ready. Press the GPIO 13 button to trigger.");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // Read button with debounce
  // GPIO 0 is shared with camera XCLK but we can still read it
  // briefly between frames — camera runs in bursts, not continuously
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    // Button just pressed (went LOW)
    if (reading == LOW && lastButtonState == HIGH) {
      unsigned long now = millis();
      if (now - lastPressTime >= MIN_PRESS_GAP) {
        lastPressTime = now;
        Serial.println("\n[BUTTON] Doorbell pressed!");
        triggerDoorbell();
      } else {
        Serial.println("[INFO] Too soon — ignoring.");
      }
    }
  }

  lastButtonState = reading;
  delay(10);
}

// ── Full doorbell flow ────────────────────────────────────────
void triggerDoorbell() {

  // Step 1 — Flash LED ON (capturing)
  setFlash(true);
  Serial.println("[Camera] Capturing photo...");
  delay(500);  // let flash illuminate the scene

  // Step 2 — Capture
  camera_fb_t* fb = capturePhoto();
  setFlash(false);

  if (!fb) {
    Serial.println("[ERROR] Capture failed — sending text notification.");
    sendTextNotification("⚠️ Doorbell pressed but camera failed!");
    return;
  }
  Serial.println("[OK] Photo captured: " + String(fb->len) + " bytes.");

  // Step 3 — Send photo to Telegram
  Serial.println("[Telegram] Sending photo...");
  bool ok = sendPhotoToTelegram(fb);
  esp_camera_fb_return(fb);

  if (ok) {
    Serial.println("[OK] Photo sent to Telegram.");
    blinkFlash(3);
  } else {
    Serial.println("[ERROR] Telegram send failed.");
    blinkFlash(6);
  }

  // Step 4 — Post visit to backend (if configured)
  if (strlen(BACKEND_URL) > 0) {
    postVisitToBackend();
  }

  Serial.println("[OK] Done. Waiting for next press...\n");
}

// ── WiFi ──────────────────────────────────────────────────────
bool connectWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 15000) return false;
    setFlash(!digitalRead(FLASH_LED));
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  setFlash(false);
  return true;
}

// ── Camera ────────────────────────────────────────────────────
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
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count     = 1;

  return esp_camera_init(&config) == ESP_OK;
}

camera_fb_t* capturePhoto() {
  // Discard first frame — first frame is sometimes dark
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  delay(100);
  return esp_camera_fb_get();
}

// ── Send photo to Telegram ────────────────────────────────────
bool sendPhotoToTelegram(camera_fb_t* fb) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[Telegram] Connection failed.");
    return false;
  }

  String boundary = "----SmartDoorbell";

  String bodyStart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
    + String(CHAT_ID) + "\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"caption\"\r\n\r\n"
    "🔔 Doorbell pressed!\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"reply_markup\"\r\n\r\n"
    "{\"inline_keyboard\":[[{\"text\":\"✅ On my way\",\"callback_data\":\"reply:0:On my way\"},{\"text\":\"🏠 Not home\",\"callback_data\":\"reply:0:Not home\"}]]}\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"doorbell.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";
  int totalLen   = bodyStart.length() + fb->len + bodyEnd.length();

  client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(totalLen));
  client.println("Connection: close");
  client.println();
  client.print(bodyStart);

  uint8_t* buf  = fb->buf;
  size_t   len  = fb->len;
  size_t   chunk = 1024;
  while (len > 0) {
    size_t toSend = min(len, chunk);
    client.write(buf, toSend);
    buf += toSend;
    len -= toSend;
  }
  client.print(bodyEnd);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 15000) {
      client.stop();
      return false;
    }
  }

  String statusLine = client.readStringUntil('\n');
  Serial.println("[Telegram] Response: " + statusLine);
  client.stop();
  return statusLine.indexOf("200") > 0;
}

// ── Send text notification (camera fallback) ──────────────────
void sendTextNotification(const String& message) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);

  if (!client.connect("api.telegram.org", 443)) return;

  String encoded = message;
  encoded.replace(" ", "%20");

  client.println("GET /bot" + String(BOT_TOKEN)
    + "/sendMessage?chat_id=" + String(CHAT_ID)
    + "&text=" + encoded + " HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Connection: close");
  client.println();
  delay(1000);
  client.stop();
}

// ── Post visit to backend ─────────────────────────────────────
void postVisitToBackend() {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  // Extract host from BACKEND_URL
  String url    = String(BACKEND_URL);
  String host   = url;
  host.replace("https://", "");
  host.replace("http://",  "");

  Serial.println("[Backend] Posting visit to: " + host);

  if (!client.connect(host.c_str(), 443)) {
    Serial.println("[Backend] Connection failed.");
    return;
  }

  String body = "trigger=button";
  client.println("POST /api/visits HTTP/1.1");
  client.println("Host: " + host);
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Content-Length: " + String(body.length()));
  client.println("Connection: close");
  client.println();
  client.print(body);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 8000) { client.stop(); return; }
  }
  String resp = client.readStringUntil('\n');
  Serial.println("[Backend] Response: " + resp);
  client.stop();
}

// ── Flash LED helpers ─────────────────────────────────────────
void setFlash(bool on) {
  digitalWrite(FLASH_LED, on ? HIGH : LOW);
}

void blinkFlash(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(FLASH_LED, HIGH); delay(150);
    digitalWrite(FLASH_LED, LOW);  delay(150);
  }
}
