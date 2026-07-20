// ============================================================
//  Smart Doorbell – Step 5: Complete V1  (button + OLED display)
//  Board  : AI Thinker ESP32-CAM (CS-CAM carrier board)
//  Button : Tactile button — IO13 (INPUT_PULLUP) + real GND
//           (other leg goes to the power-tap GND rail).
//           (GPIO 0 = camera XCLK — cannot be used as button)
//  OLED   : 128x64 I2C display — SDA=IO14, SCL=IO15.
//           Powered from 5V; (!) use ~1k series resistors on SDA/SCL —
//           the panel's pull-ups idle at 5V and ESP32 pins are 3.3V max.
//  LED    : Built-in flash LED (GPIO 4) — capturing indicator
//  Flow   : Button press → capture photo → send to Telegram
//           → save to backend → appear on dashboard live; status on OLED
// ============================================================

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <U8g2lib.h>          // OLED (install "U8g2" via Library Manager)
#include <Wire.h>

// ── OLED 128x64 I2C display ───────────────────────────────────
// Wired to the CS-CAM yellow header: SDA=IO14, SCL=IO15.
// If the screen shows garbage or a shifted image, your panel is an
// SH1106 (common on 1.3"): comment the SSD1306 line, uncomment SH1106.
#define OLED_SDA 14
#define OLED_SCL 15
#define OLED_X    4   // horizontal text offset (px). Increase = shift text right.
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C oled(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_SW_I2C oled(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

// ── Credentials ──────────────────────────────────────────────
// Real values live in secrets.h (gitignored — never committed).
#include "secrets.h"
const char* WIFI_SSID     = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const char* BOT_TOKEN     = SECRET_BOT_TOKEN;
const char* CHAT_ID       = SECRET_CHAT_ID;

// SEC-03: shared secret sent to the backend on every device request.
// Agree the value with Rami (backend env var DEVICE_SECRET) and add it to
// secrets.h as SECRET_DEVICE_KEY. The fallback lets the sketch compile now;
// the header is harmless until the backend starts enforcing it.
#ifndef SECRET_DEVICE_KEY
#define SECRET_DEVICE_KEY "CHANGE_ME_AGREE_WITH_RAMI"
#endif
const char* DEVICE_KEY    = SECRET_DEVICE_KEY;

// ── Backend server URL ────────────────────────────────────────
// Permanent cloud deployment (Railway + Supabase) — no ngrok.
// Leave empty to skip backend posting (Telegram only).
// ─────────────────────────────────────────────────────────────
const char* BACKEND_URL   = "https://smart-doorbell-production.up.railway.app";

// ── Pin definitions ───────────────────────────────────────────
// IMPORTANT: GPIO 0 = camera XCLK — it is driven as a 20 MHz
// clock after esp_camera_init(). It CANNOT be read as a button.
// Use GPIO 13 for the external tactile button instead.
// Wiring:
//   button leg A -> IO13 (yellow header)
//   button leg B -> GND  (power-tap GND rail)
//   OLED SDA -> IO14, OLED SCL -> IO15 (yellow header)
#define BUTTON_PIN  13   // External tactile button (INPUT_PULLUP).
                         // Other leg -> real GND (power-tap rail).
                         // IO14 is now free for the OLED's SDA line.
#define FLASH_LED    4   // Built-in white flash LED

// Demo mode while the physical button is not yet wired:
// fire ONE doorbell event automatically after boot, so pressing
// the carrier's RESET button acts as the doorbell.
// Set to false once the real button is wired to IO13 / IO14.
#define TRIGGER_ON_BOOT  false

unsigned long lastDebounceTime  = 0;
int           lastButtonState   = HIGH;   // last raw reading
int           buttonState       = HIGH;   // debounced (stable) state

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

// ── Heartbeat ─────────────────────────────────────────────────
// Report WiFi signal to the backend every minute so the dashboard
// can show the device as online/offline.
#define HEARTBEAT_MS 60000UL
unsigned long lastHeartbeat = 0;

// ── Forward declarations ──────────────────────────────────────
bool connectWiFi();
bool initCamera();
camera_fb_t* capturePhoto();
bool sendPhotoToTelegram(camera_fb_t* fb);
void sendTextNotification(const String& message);
bool postVisitToBackend(camera_fb_t* fb);
void sendHeartbeat();
void triggerDoorbell();
void blinkFlash(int times);
void setFlash(bool on);
void showStatus(const String& line1, const String& line2);
void showReply(const String& text);
String fetchReply(int visitId);

unsigned int visitCount      = 0;   // delivered visits this session (shown on OLED)
int          lastVisitId     = -1;  // id of the most recent visit (for reply polling)
String       lastVisitorName = "";  // recognised name ("" = unknown / recognition off)

// Non-blocking watch for the homeowner's reply, handled in loop(). This lets a
// reply that arrives later — e.g. a typed message — still reach the OLED,
// instead of only catching it in a short blocking window after the press.
bool          awaitingReply  = false;
int           awaitVisitId   = -1;
unsigned long awaitDeadline  = 0;
unsigned long lastReplyPoll  = 0;
#define REPLY_WATCH_MS   180000UL   // keep watching up to 3 min after a visit
#define REPLY_POLL_MS      4000UL   // poll the backend this often

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(FLASH_LED,  OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setFlash(false);

  // OLED display
  oled.begin();
  showStatus("Doorbell", "Starting...");

  Serial.println("\n=== Smart Doorbell V1 — Ready ===");

  // WiFi
  if (!connectWiFi()) {
    Serial.println("[ERROR] WiFi failed — check credentials.");
    blinkFlash(10);
    return;
  }
  Serial.println("[OK] WiFi connected: " + WiFi.localIP().toString());
  showStatus("WiFi OK", WiFi.localIP().toString());

  // Camera
  if (!initCamera()) {
    Serial.println("[ERROR] Camera init failed.");
    blinkFlash(10);
    return;
  }
  Serial.println("[OK] Camera ready.");

  // Ready signal — 3 quick flashes
  blinkFlash(3);
  showStatus("Ready", WiFi.localIP().toString());

  // First heartbeat right away — dashboard shows Online immediately
  sendHeartbeat();
  lastHeartbeat = millis();

#if TRIGGER_ON_BOOT
  // Demo mode: RESET button = doorbell. One event per boot.
  Serial.println("[DEMO] TRIGGER_ON_BOOT active — firing doorbell now.");
  triggerDoorbell();
#endif

  Serial.println("[OK] Doorbell ready. Press the IO13/IO14 button to trigger.");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // Read button with debounce
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    // Reading has been stable for DEBOUNCE_MS — accept it as the real state.
    if (reading != buttonState) {
      buttonState = reading;
      // Falling edge = button just pressed (pulled LOW)
      if (buttonState == LOW) {
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
  }

  lastButtonState = reading;

  // Periodic heartbeat (takes ~1 s; a button press during it is
  // missed — acceptable, the next press works)
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    sendHeartbeat();
  }

  // Background: watch for the homeowner's reply after a visit, so a reply
  // that arrives later (e.g. a typed message) still reaches the OLED.
  if (awaitingReply) {
    if (millis() > awaitDeadline) {
      awaitingReply = false;
      Serial.println("[Reply] No reply within timeout.");
      showStatus("Ready", WiFi.localIP().toString());
    } else if (millis() - lastReplyPoll >= REPLY_POLL_MS) {
      lastReplyPoll = millis();
      String reply = fetchReply(awaitVisitId);
      if (reply.length() > 0) {
        awaitingReply = false;
        Serial.println("[Reply] Homeowner: " + reply);
        showReply(reply);
        delay(8000);   // hold the reply for the visitor to read
        showStatus("Ready", WiFi.localIP().toString());
      }
    }
  }

  delay(10);
}

// ── Full doorbell flow ────────────────────────────────────────
void triggerDoorbell() {

  // Step 1 — Flash LED ON (capturing)
  setFlash(true);
  showStatus("Ding dong!", "Capturing...");
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

  // Step 3 — Deliver the visit.
  // Backend configured → upload photo to backend; the BACKEND sends
  // the single Telegram notification (photo + working reply buttons).
  // No backend → fall back to sending the photo to Telegram directly.
  bool ok;
  showStatus("Ding dong!", "Sending...");
  if (strlen(BACKEND_URL) > 0) {
    Serial.println("[Backend] Uploading visit + photo...");
    ok = postVisitToBackend(fb);
  } else {
    Serial.println("[Telegram] Sending photo directly...");
    ok = sendPhotoToTelegram(fb);
  }
  esp_camera_fb_return(fb);

  if (ok) {
    visitCount++;
    Serial.println("[OK] Visit delivered.");
    showStatus("Sent!", "Visits: " + String(visitCount));
    blinkFlash(3);
    delay(800);
    // Greet the visitor: by name if recognised, else a generic welcome.
    if (lastVisitorName.length() > 0)
      showStatus("Welcome,", lastVisitorName);
    else
      showStatus("New visitor", "Hello!");
    delay(3000);
    // Start watching for the homeowner's reply in the background (loop()),
    // so a reply that arrives later — e.g. a typed message — still shows.
    showStatus("Please wait", "for a reply...");
    awaitingReply = true;
    awaitVisitId  = lastVisitId;
    awaitDeadline = millis() + REPLY_WATCH_MS;
    lastReplyPoll = millis();
  } else {
    Serial.println("[ERROR] Delivery failed.");
    showStatus("Failed", "Try again");
    blinkFlash(6);
    delay(3000);
    showStatus("Ready", WiFi.localIP().toString());
  }

  Serial.println("[OK] Done. Waiting for next press...\n");
}

// ── OLED status helper ────────────────────────────────────────
void showStatus(const String& line1, const String& line2) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_7x14B_tr);
  oled.drawStr(OLED_X, 16, line1.c_str());
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(OLED_X, 40, line2.c_str());
  oled.sendBuffer();
}

// ── Show a (possibly long) reply, word-wrapped over up to 4 lines ─
// Used for the homeowner's reply, which can now be free-typed text.
void showReply(const String& text) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tr);   // ~20 chars per 128px line
  const int   maxChars = 20;
  const int   lineH    = 14;
  int         y        = 14;
  String      line     = "";
  String      word     = "";
  String      t        = text + " ";

  for (int i = 0; i < (int)t.length() && y <= 60; i++) {
    char c = t[i];
    if (c == ' ') {
      if (word.length() == 0) continue;
      if (line.length() + 1 + word.length() > (unsigned)maxChars && line.length() > 0) {
        oled.drawStr(OLED_X, y, line.c_str());
        y += lineH;
        line = word;
      } else {
        line = line.length() ? line + " " + word : word;
      }
      word = "";
    } else {
      word += c;
    }
  }
  if (line.length() > 0 && y <= 60) oled.drawStr(OLED_X, y, line.c_str());
  oled.sendBuffer();
}

// ── Poll the backend once for the homeowner's reply ───────────
// Returns the reply text the homeowner tapped or typed in Telegram,
// or "" if there's none yet (or on error). Called repeatedly from loop()
// so a late/typed reply still reaches the OLED.
String fetchReply(int visitId) {
  if (visitId < 0 || strlen(BACKEND_URL) == 0) return "";

  String host = String(BACKEND_URL);
  host.replace("https://", "");
  host.replace("http://",  "");
  String path = "/api/visits/" + String(visitId) + "/response";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  if (!client.connect(host.c_str(), 443)) return "";

  client.println("GET " + path + " HTTP/1.1");
  client.println("Host: " + host);
  client.println("X-Device-Key: " + String(DEVICE_KEY));
  client.println("Connection: close");
  client.println();

  String resp;
  unsigned long t = millis();
  while (client.connected() && millis() - t < 8000) {
    while (client.available()) { resp += (char)client.read(); t = millis(); }
  }
  client.stop();

  // Body: {"id":N,"response":"On my way"} or {"id":N,"response":null}
  int c = resp.indexOf("\"response\":");
  if (c >= 0) {
    int p = c + 11;
    while (p < (int)resp.length() && resp[p] == ' ') p++;
    if (p < (int)resp.length() && resp[p] == '"') {   // a real reply, not null
      int q = resp.indexOf('"', p + 1);
      if (q > p) return resp.substring(p + 1, q);
    }
  }
  return "";
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
  config.frame_size   = FRAMESIZE_VGA;   // 640x480 — enough detail for face recognition
  config.jpeg_quality = 10;
  config.fb_count     = 1;

  if (esp_camera_init(&config) != ESP_OK) return false;

  // Camera module sits rotated 180° in the CS-CAM carrier —
  // flip the sensor output so photos arrive upright.
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  }
  return true;
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

// ── Post visit + photo to backend ─────────────────────────────
// The backend saves the photo, stores the visit in the database,
// and sends the single Telegram notification with reply buttons.
bool postVisitToBackend(camera_fb_t* fb) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  // Extract host from BACKEND_URL
  String url    = String(BACKEND_URL);
  String host   = url;
  host.replace("https://", "");
  host.replace("http://",  "");

  Serial.println("[Backend] Posting visit to: " + host);

  if (!client.connect(host.c_str(), 443)) {
    Serial.println("[Backend] Connection failed.");
    return false;
  }

  String boundary = "----SmartDoorbell";

  String bodyStart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"trigger\"\r\n\r\n"
    "button\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"doorbell.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";
  int totalLen   = bodyStart.length() + fb->len + bodyEnd.length();

  client.println("POST /api/visits HTTP/1.1");
  client.println("Host: " + host);
  client.println("X-Device-Key: " + String(DEVICE_KEY));
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
    if (millis() - timeout > 20000) { client.stop(); return false; }
  }

  // Read the whole response (status line + headers + JSON body).
  String resp;
  unsigned long t = millis();
  while (client.connected() && millis() - t < 10000) {
    while (client.available()) { resp += (char)client.read(); t = millis(); }
  }
  client.stop();

  bool ok = resp.indexOf(" 201") > 0;
  int nl = resp.indexOf('\n');
  Serial.println("[Backend] Response: " + (nl > 0 ? resp.substring(0, nl) : resp));

  // Grab the new visit's id from the JSON body: {"id":123,...}
  lastVisitId = -1;
  int idPos = resp.indexOf("\"id\":");
  if (idPos >= 0) {
    int p = idPos + 5;
    while (p < (int)resp.length() && resp[p] == ' ') p++;
    int start = p;
    while (p < (int)resp.length() && isDigit(resp[p])) p++;
    if (p > start) lastVisitId = resp.substring(start, p).toInt();
  }
  Serial.println("[Backend] Visit id: " + String(lastVisitId));

  // Recognised name from the backend: "visitor_name":"Mom" (null = unknown).
  lastVisitorName = "";
  int vn = resp.indexOf("\"visitor_name\":");
  if (vn >= 0) {
    int p = vn + 15;
    while (p < (int)resp.length() && resp[p] == ' ') p++;
    if (p < (int)resp.length() && resp[p] == '"') {   // a real name, not null
      int q = resp.indexOf('"', p + 1);
      if (q > p) lastVisitorName = resp.substring(p + 1, q);
    }
  }
  if (lastVisitorName.length() > 0)
    Serial.println("[Backend] Recognised: " + lastVisitorName);

  return ok;
}

// ── Heartbeat: report WiFi signal to backend ──────────────────
void sendHeartbeat() {
  if (strlen(BACKEND_URL) == 0 || WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  String host = String(BACKEND_URL);
  host.replace("https://", "");
  host.replace("http://",  "");

  if (!client.connect(host.c_str(), 443)) {
    Serial.println("[Heartbeat] Connection failed.");
    return;
  }

  String body = String("{\"wifi_signal\":") + WiFi.RSSI() + "}";

  client.println("POST /api/device/heartbeat HTTP/1.1");
  client.println("Host: " + host);
  client.println("X-Device-Key: " + String(DEVICE_KEY));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(body.length()));
  client.println("Connection: close");
  client.println();
  client.print(body);

  unsigned long t = millis();
  while (client.available() == 0 && millis() - t < 5000) delay(10);
  client.stop();

  Serial.println("[Heartbeat] Sent. RSSI: " + String(WiFi.RSSI()) + " dBm");
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
