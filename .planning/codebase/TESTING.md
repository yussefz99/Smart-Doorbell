# Testing Patterns

**Analysis Date:** 2026-06-23

---

This project uses **hardware-in-the-loop validation sketches** as its primary testing approach. There is no automated test runner, no unit test framework, and no mocking library anywhere in the repo. Testing is performed by flashing incremental `.ino` sketches to the physical ESP32-CAM board and observing Serial Monitor output and LED/flash behavior. The Python backend and dashboard have no test files.

---

## Test Framework

**Runner:**
- None. No Jest, pytest, Vitest, or equivalent.
- Tests are Arduino sketches that run on the physical ESP32-CAM hardware.

**Assertion Library:**
- None. Pass/fail is communicated via `Serial.println("[OK] ...")` and `Serial.println("[ERROR] ...")` output to the Arduino Serial Monitor at 115200 baud, and via LED flash patterns.

**Run Commands:**
```bash
# There are no CLI test commands.
# To run a test sketch:
# 1. Open the .ino file in Arduino IDE
# 2. Select Board: AI Thinker ESP32-CAM
# 3. Upload via the MB downloader board (USB)
# 4. Open Serial Monitor at 115200 baud and observe output
```

---

## Test File Organization

**Location:**
- Test sketches live in `Unit Tests/` (top-level directory, parallel to `ESP32/`)
- Production sketches live in `ESP32/`
- The `Unit Tests/` directory mirrors the `ESP32/` directory structure exactly

**Structure:**
```
Unit Tests/
├── README.md                              # Single line: "validation tests for sensors/hardware parts"
├── doorbell_led_test/
│   └── doorbell_led_test.ino             # GPIO LED finder — blinks each pin to identify board LEDs
├── doorbell_step1_leds_button_buzzer.ino  # Step 1 standalone test (not in subdirectory)
├── doorbell_step2_wifi_telegram/
│   └── doorbell_step2_wifi_telegram.ino  # WiFi + Telegram text message test
├── doorbell_step2_wifi_telegram_test/
│   └── doorbell_step2_wifi_telegram_test.ino  # Alternative step 2 test variant
├── doorbell_step3_camera_test/
│   └── doorbell_step3_camera_test.ino   # Camera init + one capture test
├── doorbell_step4_photo_telegram/
│   └── doorbell_step4_photo_telegram.ino # WiFi + camera + photo-to-Telegram test
└── doorbell_step5_complete_v1/
    ├── doorbell_step5_complete_v1.ino    # Full integration test (mirrors production sketch)
    └── secrets.h.example                 # Credentials template
```

**Naming:**
- Test sketch files match the sketch subdirectory name exactly (Arduino IDE requirement)
- Step-based naming: `doorbell_step<N>_<feature>.ino` corresponds to the development step being validated

---

## Test Sketch Structure

Each test sketch follows the same Arduino pattern:

```cpp
// ============================================================
//  Smart Doorbell – Step N: <what is being tested>
//  Board : AI Thinker ESP32-CAM
//  Tests : <explicit list of what is verified>
//  LEDs  : <LED meaning key>
// ============================================================

// Pin constants and credentials at top

void setup() {
  Serial.begin(115200);
  // Initialize hardware

  // Run test sequence — report pass/fail via Serial + LEDs
  if (!testSubsystem()) {
    Serial.println("[ERROR] <subsystem> failed.");
    blinkLED(LED_RED, 6, 200, 200);   // error pattern
    return;  // halt further init
  }
  Serial.println("[OK] <subsystem> passed.");
  blinkLED(LED_GREEN, 3, 200, 200);   // success pattern
}

void loop() {}   // empty — test is one-shot, runs in setup()
```

**Key structural points:**
- Test sketches are **one-shot**: all test logic runs in `setup()`, `loop()` is empty
- Each subsystem check halts the test immediately on failure via `return`
- LED patterns communicate result without a serial monitor: 3 green blinks = pass, 6 red blinks = error
- The board-facing `Serial.println()` output serves as the assertion log

---

## Test Types

### GPIO / LED Finder Test (`Unit Tests/doorbell_led_test/doorbell_led_test.ino`)

- **Purpose:** Identify which GPIO pins control which physical LEDs on the CS-CAM carrier board
- **Method:** Cycles through candidate GPIO pins `{2, 4, 12, 13, 15, 16, 33}`, blinks each 3 times, then reports via Serial
- **Pass criteria:** Operator observes and notes which LED lights for each GPIO printed
- **Loop:** Continuous cycling — does not terminate

### WiFi + Telegram Text Test (Steps 2, 2-test variant)

- **Files:** `Unit Tests/doorbell_step2_wifi_telegram/doorbell_step2_wifi_telegram_test.ino`
- **Purpose:** Verify WiFi connectivity and Telegram bot credentials
- **Method:**
  1. Connect to WiFi (15 s timeout)
  2. Send a known-good text message to Telegram chat
  3. Read HTTP 200 response status
- **Pass criteria:** `[OK] Telegram message sent successfully.` + 3 yellow LED blinks
- **Fail criteria:** `[ERROR] WiFi connection failed` (solid red LED) or `[ERROR] Telegram send failed` (6 red blinks)

### Camera Init + Capture Test (`Unit Tests/doorbell_step3_camera_test/doorbell_step3_camera_test.ino`)

- **Purpose:** Verify camera hardware is wired correctly and can capture a frame
- **Method:**
  1. Initialize camera with QVGA (320x240) resolution
  2. Wait 1 s for sensor to stabilise
  3. Call `esp_camera_fb_get()`, check for non-null
  4. Print frame dimensions and byte size to Serial
- **Pass criteria:** Non-null frame buffer + `[OK] Camera test passed. Ready for Step 4.` + 3 green blinks
- **Fail criteria:** `[ERROR] Camera init failed` or `[ERROR] Capture failed — frame buffer is null.` + 6 red blinks

### WiFi + Camera + Photo-to-Telegram Test (`Unit Tests/doorbell_step4_photo_telegram/doorbell_step4_photo_telegram.ino`)

- **Purpose:** End-to-end test of the full photo delivery path (excluding backend)
- **Method:**
  1. Connect WiFi
  2. Init camera
  3. Capture photo (QVGA)
  4. POST photo as multipart to Telegram `sendPhoto` API
  5. Verify HTTP 200 response
- **LED protocol:** green = idle/ok, red = capturing, yellow = sending
- **Pass criteria:** `[OK] Photo sent to Telegram successfully.` + 3 green blinks
- **Fail criteria:** subsystem-specific `[ERROR]` + 6 red blinks

### Full Integration Test (`Unit Tests/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`)

- **Purpose:** Complete system test matching the production sketch exactly
- **Method:** Identical to the production `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`
- **Includes:** WiFi connect, camera init, heartbeat, `TRIGGER_ON_BOOT` demo mode, backend posting
- **Note:** `Unit Tests/` version uses the same `secrets.h` pattern as production

---

## Credentials in Test Sketches

Earlier test sketches (steps 1–4) embed placeholder credentials directly as string literals:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* BOT_TOKEN     = "YOUR_BOT_TOKEN_HERE";
const char* CHAT_ID       = "YOUR_CHAT_ID";
```

These placeholders must be replaced manually before flashing. Step 5 (production-equivalent) uses the `secrets.h` pattern:

```cpp
#include "secrets.h"
const char* WIFI_SSID = SECRET_WIFI_SSID;
```

The `secrets.h.example` template is committed at `Unit Tests/doorbell_step5_complete_v1/secrets.h.example` and `ESP32/doorbell_step5_complete_v1/secrets.h.example`.

---

## Pass/Fail Communication Protocol

All test sketches use a consistent two-channel reporting protocol:

| Channel | Pass | Fail |
|---------|------|------|
| Serial Monitor | `[OK] <message>` | `[ERROR] <message>` |
| LED | 3 green blinks | 6 red blinks |
| LED (sending) | 3 yellow blinks (step 2) | — |

---

## Backend Testing

**No automated tests exist for the backend.** The following are untested at the code level:

- `backend/server.py` — all FastAPI routes
- `backend/telegram_bot.py` — Telegram API helper functions
- `backend/recognition.py` — InsightFace face matching logic

Backend testing is done manually by:
1. Running `uvicorn server:app --reload --port 8001`
2. Triggering the ESP32 to POST a visit
3. Observing the dashboard for the live WebSocket update
4. Checking Telegram for the notification

---

## Dashboard Testing

**No automated tests exist for `backend/dashboard.html`.** Manual browser testing only.

---

## Coverage

**Requirements:** None enforced — no coverage tooling configured.

**Untested areas (high priority):**
- All Python backend route logic (`server.py`)
- WebSocket broadcast and disconnect handling (`ConnectionManager` in `server.py`)
- Quiet-hours time-window logic (`in_quiet_hours()` in `server.py`)
- Telegram webhook callback parsing (`/telegram/webhook` route)
- Face recognition matching threshold logic (`recognition.py`)
- Dashboard JS fetch/render functions (`dashboard.html`)

---

## Adding New Tests

**For new ESP32 firmware behavior:**
1. Create `Unit Tests/<sketch_name>/<sketch_name>.ino`
2. Open with `setup()` containing the one-shot test sequence
3. Use `[OK]` / `[ERROR]` Serial prefixes and 3-green / 6-red LED patterns
4. Keep `loop()` empty for one-shot tests; use continuous cycling only for interactive hardware probes

**For backend routes (not yet established):**
- No framework is in place; pytest with httpx's `AsyncClient` would be the natural fit given FastAPI + async patterns
- `from fastapi.testclient import TestClient` is the first step toward adding coverage

---

*Testing analysis: 2026-06-23*
