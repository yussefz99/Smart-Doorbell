// ============================================================
//  I2C Scanner — diagnostic for the OLED on the CS-CAM board
//  SDA = IO14, SCL = IO15 (same pins as the doorbell sketch).
//  Open Serial Monitor @ 115200.
//   - "Found device at 0x3C" → OLED is wired/powered correctly.
//   - "No I2C devices found"  → power or SDA/SCL wiring problem.
// ============================================================
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(14, 15);   // SDA=IO14, SCL=IO15
  Serial.println("\nI2C scanner ready.");
}

void loop() {
  byte count = 0;
  Serial.println("Scanning...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      count++;
    }
  }
  if (count == 0) Serial.println("  No I2C devices found.");
  Serial.println();
  delay(3000);
}
