// ============================================================
//  Smart Doorbell – Step 2: WiFi connection + Telegram text
//  Board : AI Thinker ESP32-CAM
//  Tests : WiFi connects → sends one Telegram text message
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>

// ── Credentials ─────────────────────────────────────────────
const char* WIFI_SSID     = "Yussef";   // your WiFi name
const char* WIFI_PASSWORD = "yussef7855";   // your WiFi password
const char* BOT_TOKEN   = "8826200691:AAHa7xik9owDwAcb06JDhBk6JNbwPgpZcwc";   // from BotFather  e.g. 7123456789:AAFxxx
const char* CHAT_ID       = "1078220538";   // numeric chat ID e.g. 123456789
// ────────────────────────────────────────────────────────────

#define LED_GREEN   2
#define LED_RED     4
#define LED_YELLOW  15

#define WIFI_TIMEOUT_MS 15000

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  allLEDsOff();

  Serial.println("\n=== Smart Doorbell – Step 2 ===");

  if (!connectWiFi()) {
    setLED(LED_RED, true);
    Serial.println("[ERROR] WiFi connection failed. Check SSID/password.");
    return;
  }

  setLED(LED_GREEN, true);
  Serial.println("[OK] WiFi connected. IP: " + WiFi.localIP().toString());

  bool ok = sendTelegramText("ESP32-CAM online. WiFi test passed.");
  if (ok) {
    Serial.println("[OK] Telegram message sent successfully.");
    blinkLED(LED_YELLOW, 3, 150, 150);
    setLED(LED_GREEN, true);
  } else {
    Serial.println("[ERROR] Telegram send failed. Check BOT_TOKEN and CHAT_ID.");
    setLED(LED_GREEN, false);
    blinkLED(LED_RED, 6, 200, 200);
  }
}

void loop() {}

// ── WiFi ─────────────────────────────────────────────────────
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

// ── Telegram ──────────────────────────────────────────────────
bool sendTelegramText(const String& message) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);

  Serial.println("[Telegram] Connecting to api.telegram.org ...");

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[Telegram] Connection failed.");
    return false;
  }

  Serial.println("[Telegram] Connected. Sending request...");

  String encodedMessage = message;
encodedMessage.replace(" ", "%20");

String url = "/bot" + String(BOT_TOKEN)
           + "/sendMessage?chat_id=" + String(CHAT_ID)
           + "&text=" + encodedMessage;

  client.println("GET " + url + " HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Connection: close");
  client.println();

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[Telegram] Timeout waiting for response.");
      client.stop();
      return false;
    }
  }

  // Read first line of response (contains status code)
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
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   LOW);
  digitalWrite(LED_YELLOW,LOW);
}
