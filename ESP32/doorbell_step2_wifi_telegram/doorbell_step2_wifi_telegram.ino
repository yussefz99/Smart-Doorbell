// ============================================================
//  Smart Doorbell – Step 2: WiFi connection + Telegram text
//  Board : AI Thinker ESP32-CAM
//  Tests : WiFi connects → sends one Telegram text message
//  LEDs  : green blinks while connecting, solid when done
//          yellow flashes 3× after message is sent
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ── Credentials ─────────────────────────────────────────────
// Fill in your own values. Do NOT commit real secrets to git.
const char* WIFI_SSID     = "YOUR_WIFI_SSID";   // your WiFi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   // your WiFi password
const char* BOT_TOKEN   = "YOUR_BOT_TOKEN_HERE";   // from BotFather  e.g. 7123456789:AAFxxx
const char* CHAT_ID       = "YOUR_CHAT_ID";   // numeric chat ID e.g. 123456789
// ────────────────────────────────────────────────────────────

#define LED_GREEN   2
#define LED_RED     4
#define LED_YELLOW  15

// Maximum time to wait for WiFi before giving up (ms)
#define WIFI_TIMEOUT_MS 15000

// ── Forward declarations ─────────────────────────────────────
bool connectWiFi();
bool sendTelegramText(const String& message);
void blinkLED(int pin, int times, int onMs, int offMs);
void setLED(int pin, bool state);
void allLEDsOff();

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);  // let serial settle

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  allLEDsOff();

  Serial.println("\n=== Smart Doorbell – Step 2 ===");

  if (!connectWiFi()) {
    // WiFi failed → solid red, halt
    setLED(LED_RED, true);
    Serial.println("[ERROR] WiFi connection failed. Check SSID/password.");
    return;
  }

  // WiFi OK
  setLED(LED_GREEN, true);
  Serial.println("[OK] WiFi connected. IP: " + WiFi.localIP().toString());

  bool ok = sendTelegramText("ESP32-CAM online. WiFi test passed.");
  if (ok) {
    Serial.println("[OK] Telegram message sent successfully.");
    blinkLED(LED_YELLOW, 3, 150, 150);
    setLED(LED_GREEN, true);   // return to idle (green on)
  } else {
    Serial.println("[ERROR] Telegram send failed. Check BOT_TOKEN and CHAT_ID.");
    setLED(LED_GREEN, false);
    blinkLED(LED_RED, 6, 200, 200);  // error flash
  }
}

void loop() {
  // Nothing to do — this is a one-shot test sketch
}

// ────────────────────────────────────────────────────────────
bool connectWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) return false;
    // Blink green while waiting
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  return true;
}

// ────────────────────────────────────────────────────────────
bool sendTelegramText(const String& message) {
  WiFiClientSecure client;
  // setInsecure() skips certificate verification — acceptable for
  // a local test sketch; add a proper CA cert for production.
  client.setInsecure();

  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN
             + "/sendMessage?chat_id=" + CHAT_ID
             + "&text=" + message;

  Serial.println("[Telegram] Sending to: " + url);
  http.begin(client, url);

  int httpCode = http.GET();
  String payload = http.getString();
  http.end();

  Serial.println("[Telegram] HTTP response code: " + String(httpCode));
  if (httpCode != 200) {
    Serial.println("[Telegram] Response body: " + payload);
  }

  return (httpCode == 200);
}

// ── LED helpers ───────────────────────────────────────────────
void blinkLED(int pin, int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH); delay(onMs);
    digitalWrite(pin, LOW);  delay(offMs);
  }
}

void setLED(int pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
}

void allLEDsOff() {
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_YELLOW, LOW);
}
