// ============================================================
//  Smart Doorbell – Built-in LED finder
//  Blinks each GPIO one by one so we can identify
//  which GPIO controls LED1, LED2, LED3 on the CS-CAM board
// ============================================================

// GPIO pins to test — common ones used for LEDs on ESP32-CAM boards
int testPins[] = { 2, 4, 12, 13, 15, 16, 33 };
int numPins = 7;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== LED Finder Test ===");
  Serial.println("Watch the board — each LED will blink one at a time.");
  Serial.println("Note which LED lights up for each GPIO printed.");

  for (int i = 0; i < numPins; i++) {
    pinMode(testPins[i], OUTPUT);
    digitalWrite(testPins[i], LOW);
  }
}

void loop() {
  for (int i = 0; i < numPins; i++) {
    Serial.println("Testing GPIO " + String(testPins[i]) + " ...");

    // Blink 3 times
    for (int j = 0; j < 3; j++) {
      digitalWrite(testPins[i], HIGH);
      delay(300);
      digitalWrite(testPins[i], LOW);
      delay(300);
    }

    delay(1000); // pause between pins
  }

  Serial.println("--- Cycle complete, repeating ---\n");
  delay(2000);
}
