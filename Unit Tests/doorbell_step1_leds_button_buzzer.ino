// ============================================================
//  Smart Doorbell – Group 15
//  STEP 1: LEDs + Button + Buzzer test
//  (No camera, no WiFi, no PIR yet)
//
//  Purpose: Verify all outputs before adding camera.
//  Upload via MB downloader board (USB).
// ============================================================

// ── Pin definitions ─────────────────────────────────────────
#define PIN_BUZZER   2    // Passive buzzer (+)
#define PIN_LED_GRN  12   // Green LED  – Idle state
#define PIN_LED_RED  13   // Red LED    – Capturing  (⚠ shared with PIR later — resolve before Step 5)
#define PIN_LED_YLW  14   // Yellow LED – Sending
#define PIN_BUTTON   16   // Push button (INPUT_PULLUP → LOW when pressed)

// ── Buzzer tones ────────────────────────────────────────────
#define TONE_PRESS   1200   // Hz – doorbell press confirmation
#define TONE_BOOT    800    // Hz – startup beep
#define TONE_ERROR   400    // Hz – error state beep

// ── Timing ──────────────────────────────────────────────────
#define DEBOUNCE_MS     50
#define BEEP_SHORT_MS  150
#define BEEP_LONG_MS   400
#define DEMO_STEP_MS   600

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Smart Doorbell – Step 1 Test ===");

  // ── Outputs
  // GPIO 12 must start LOW (AI-Thinker boot requirement)
  // Setting as OUTPUT with LOW default satisfies this.
  pinMode(PIN_LED_GRN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YLW, OUTPUT);

  digitalWrite(PIN_LED_GRN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_YLW, LOW);

  // ── Input with internal pull-up
  // Button reads HIGH when idle, LOW when pressed.
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // ── Boot sequence: quick LED sweep + beep to confirm wiring
  bootSequence();

  // ── Enter idle state (green on)
  setIdle();
  Serial.println("Ready. Press the doorbell button.");
}

// ────────────────────────────────────────────────────────────
void loop() {
  if (buttonPressed()) {
    Serial.println("Button pressed!");

    // Simulate capture state: red on
    setCapturing();
    delay(800);

    // Simulate sending state: yellow on
    setSending();
    beep(TONE_PRESS, BEEP_LONG_MS);
    delay(800);

    // Back to idle
    setIdle();
    Serial.println("Cycle complete. Waiting for next press.");
  }
}

// ════════════════════════════════════════════════════════════
//  State helpers
// ════════════════════════════════════════════════════════════

void setIdle() {
  digitalWrite(PIN_LED_GRN, HIGH);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_YLW, LOW);
  Serial.println("State: IDLE (green)");
}

void setCapturing() {
  digitalWrite(PIN_LED_GRN, LOW);
  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_YLW, LOW);
  Serial.println("State: CAPTURING (red)");
}

void setSending() {
  digitalWrite(PIN_LED_GRN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_YLW, HIGH);
  Serial.println("State: SENDING (yellow)");
}

void setError() {
  // All three LEDs flash = white flash = error
  Serial.println("State: ERROR (white flash)");
  for (int i = 0; i < 6; i++) {
    digitalWrite(PIN_LED_GRN, HIGH);
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_YLW, HIGH);
    beep(TONE_ERROR, 100);
    delay(100);
    allLedsOff();
    delay(100);
  }
  setIdle();
}

void allLedsOff() {
  digitalWrite(PIN_LED_GRN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_YLW, LOW);
}

// ════════════════════════════════════════════════════════════
//  Button – debounced read
//  Returns true once per press (on release, to avoid repeat)
// ════════════════════════════════════════════════════════════

bool buttonPressed() {
  static bool lastState = HIGH;   // HIGH = not pressed (pull-up)
  static unsigned long lastChange = 0;

  bool current = digitalRead(PIN_BUTTON);

  if (current != lastState) {
    lastChange = millis();
    lastState = current;
  }

  // Debounce: state must be stable for DEBOUNCE_MS
  if ((millis() - lastChange) > DEBOUNCE_MS) {
    if (current == LOW) {
      // Wait for release before returning true (single-fire)
      while (digitalRead(PIN_BUTTON) == LOW) delay(10);
      delay(DEBOUNCE_MS);  // debounce release
      lastState = HIGH;
      return true;
    }
  }
  return false;
}

// ════════════════════════════════════════════════════════════
//  Buzzer
//  tone() drives the passive buzzer at the given frequency.
// ════════════════════════════════════════════════════════════

void beep(int freq, int durationMs) {
  tone(PIN_BUZZER, freq, durationMs);
  delay(durationMs + 10);   // small gap so notes don't merge
  noTone(PIN_BUZZER);
}

// ════════════════════════════════════════════════════════════
//  Boot sequence
//  Cycles through each LED + plays a rising two-tone beep.
//  Confirms every component is wired correctly before loop().
// ════════════════════════════════════════════════════════════

void bootSequence() {
  Serial.println("Boot sequence...");

  // Green
  digitalWrite(PIN_LED_GRN, HIGH);
  beep(TONE_BOOT, BEEP_SHORT_MS);
  delay(DEMO_STEP_MS);
  digitalWrite(PIN_LED_GRN, LOW);

  // Red
  digitalWrite(PIN_LED_RED, HIGH);
  beep(TONE_BOOT + 200, BEEP_SHORT_MS);
  delay(DEMO_STEP_MS);
  digitalWrite(PIN_LED_RED, LOW);

  // Yellow
  digitalWrite(PIN_LED_YLW, HIGH);
  beep(TONE_BOOT + 400, BEEP_SHORT_MS);
  delay(DEMO_STEP_MS);
  digitalWrite(PIN_LED_YLW, LOW);

  // All on (white = error state visual test)
  Serial.println("All LEDs on – error state test");
  digitalWrite(PIN_LED_GRN, HIGH);
  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_YLW, HIGH);
  beep(TONE_PRESS, BEEP_LONG_MS);
  delay(DEMO_STEP_MS);
  allLedsOff();

  delay(300);
  Serial.println("Boot sequence complete.");
}

// ════════════════════════════════════════════════════════════
//  To trigger the error state from Serial Monitor:
//    Send the character 'e' at 115200 baud
// ════════════════════════════════════════════════════════════

// Add this inside loop() to enable serial-triggered error test:
//
//   if (Serial.available() && Serial.read() == 'e') {
//     setError();
//   }
//
// Uncomment when you want to test the error flash pattern.
