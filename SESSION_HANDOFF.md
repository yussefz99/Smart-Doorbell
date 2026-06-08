# Smart Doorbell – Group 15
## Session Handoff Document
**Date:** 2026-06-08  
**Purpose:** Full context for continuing in a new session

---

## Project Overview

An IoT smart doorbell built on the ESP32-CAM (AI Thinker). When a visitor presses a button:
1. ESP32-CAM captures a photo
2. Sends a Telegram notification with the photo and two reply buttons (✅ On my way / 🏠 Not home)
3. Visit is saved to backend database
4. Dashboard updates live via WebSocket

**Stack:** Arduino C++ (firmware) · Python FastAPI + SQLite (backend) · Vanilla JS (dashboard) · Telegram Bot API

---

## Hardware

| Component | Details |
|-----------|---------|
| ESP32-CAM | AI Thinker module with OV2640 camera |
| CS-CAM carrier board | by see-sys.co.il — has built-in CH9102 USB-Serial chip, B (Boot) + R (Reset) buttons |
| Tactile button | External push button on breadboard |
| Breadboard | Button placed rows 9–12 |
| USB cable | Mini USB → laptop for programming and power |

**CS-CAM carrier board pin layout (right side, top to bottom):**
```
VIN
GND   ← connect button GND wire here
D13   ← connect button signal wire here
D12
D4
D27
D26
D25
D33
D32
D35
D34
UN
UP
EN
```

---

## File Locations

| File | Path |
|------|------|
| Step 5 firmware (main) | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\ESP32\doorbell_step5_complete_v1\doorbell_step5_complete_v1.ino` |
| Step 2 (WiFi+Telegram text) | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\ESP32\doorbell_step2_wifi_telegram\doorbell_step2_wifi_telegram.ino` |
| Step 3 (camera test) | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\ESP32\doorbell_step3_camera_test\doorbell_step3_camera_test.ino` |
| Step 4 (photo to Telegram) | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\ESP32\doorbell_step4_photo_telegram\doorbell_step4_photo_telegram.ino` |
| Backend server | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\backend\server.py` |
| Telegram bot helper | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\backend\telegram_bot.py` |
| Dashboard | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\backend\dashboard.html` |
| Credentials (secret) | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\backend\.env` |
| Requirements | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\backend\requirements.txt` |
| Progress log | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\PROGRESS.md` |
| Plan checklist | `C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\PLAN.md` |

---

## Credentials (already filled in sketch and .env)

```
WiFi SSID:     YOUR_WIFI_SSID
WiFi Password: YOUR_WIFI_PASSWORD
BOT_TOKEN:     YOUR_BOT_TOKEN_HERE
CHAT_ID:       YOUR_CHAT_ID
```

---

## What Was Completed Before This Session

### Firmware steps (all tested on real hardware ✅)
- **Step 2** — WiFi connects, sends Telegram text message → working
- **Step 3** — Camera initializes, captures photo (5999 bytes QVGA JPEG) → working
- **Step 4** — Captures photo, sends to Telegram via multipart/form-data → working

### Backend (running, fully tested ✅)
- FastAPI + SQLite on port 8001
- Endpoints: GET/POST `/api/visits`, PATCH `/api/visits/:id/tag`, GET `/api/stats`, GET `/api/device/status`, POST `/api/device/heartbeat`, POST `/telegram/webhook`, POST `/admin/set-webhook`, WS `/ws`
- Telegram webhook working via ngrok
- Reply buttons (✅ On my way / 🏠 Not home) sent with every notification
- Dashboard Response column updates live when homeowner replies

### Dashboard (working ✅)
- 4 pages: Visit History, Statistics, Diagnostics, Settings
- Dark theme, WebSocket live updates, en-GB date formatting

---

## This Session — What Was Done

### 1. GPIO 0 / Camera XCLK Conflict — FIXED

**Problem:** Step 5 sketch was taking continuous photos by itself.

**Root cause:** GPIO 0 is used by both the BOOT button AND the camera's XCLK pin.
After `esp_camera_init()`, the ESP32's LEDC peripheral takes over GPIO 0 and drives it as a 20 MHz clock to the OV2640 sensor. `digitalRead(0)` after that point reads the clock oscillation, not a button press — this is why it triggered continuously.

**Fix applied to `doorbell_step5_complete_v1.ino`:**
- Changed `#define BUTTON_PIN` from `0` to `13`
- GPIO 13 is free on AI Thinker ESP32-CAM — not used by the camera at all
- Rewrote `loop()` with proper state-change debounce logic
- Added all missing forward declarations
- Removed the broken `volatile bool doorbellTriggered` approach
- Updated comments to explain the GPIO 0 constraint

**Current state of Step 5 sketch:** Code is correct and ready to upload. External button must be wired to GPIO 13.

### 2. Wiring Attempt — IN PROGRESS

**Goal:** Wire external tactile button to GPIO 13 on the CS-CAM carrier board.

**What was tried:**
- Button placed on breadboard rows 9–12
- Two wires (black = GND, orange/red = signal) connected
- Discovered the wires were connected to the spare/loose board, not the active CS-CAM board

**Connector type problem:**
- The active CS-CAM carrier board's pin connections are NOT standard female sockets that accept female jumper wire ends
- User has: male-to-male wires and female-to-female wires
- The correct connection method is still unclear — need a close-up photo of the CS-CAM pin area

---

## IMMEDIATE NEXT STEP (start here in new session)

**The wiring is not yet complete.** Here is exactly what needs to happen:

1. **Take a close-up photo of the CS-CAM carrier board pin area** (right side, where D13 and GND labels are) — need to see the exact connector type to know how to connect wires
2. **Wire the external button to GPIO 13 and GND** using whichever method fits the connector
3. **Upload `doorbell_step5_complete_v1.ino`** to the ESP32-CAM
4. **Test:** press button → photo should appear in Telegram
5. **If it works:** post a visit to backend too (set BACKEND_URL in sketch + run backend server + run ngrok)

---

## Step 5 Sketch — Key Settings

```cpp
// Credentials — already filled
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* BOT_TOKEN     = "YOUR_BOT_TOKEN_HERE";
const char* CHAT_ID       = "YOUR_CHAT_ID";
const char* BACKEND_URL   = "";  // fill with ngrok URL when ready

// Pin definitions
#define BUTTON_PIN  13   // External tactile button — wire to D13 on CS-CAM
#define FLASH_LED    4   // Built-in white flash LED

// Camera uses GPIO 0 as XCLK — DO NOT use GPIO 0 as button
```

---

## Arduino IDE Settings

| Setting | Value |
|---------|-------|
| Board | AI Thinker ESP32-CAM |
| Port | COM3 |
| Baud rate (Serial Monitor) | 115200 |

---

## How to Start the Backend

```bash
cd C:\Users\yusse\OneDrive\Desktop\Smart-Doorbell\backend
uvicorn server:app --reload --port 8001
```

## How to Start ngrok (for Telegram webhook)

```bash
# In a second terminal
.\ngrok http 8001

# Then register the webhook (replace URL with your ngrok URL)
Invoke-WebRequest -Uri "http://localhost:8001/admin/set-webhook" -Method POST -ContentType "application/json" -Body '{"url":"https://YOUR-NGROK-URL"}'
```

---

## Known Issues

| # | Issue | Status |
|---|-------|--------|
| 1 | GPIO 0 used as camera XCLK — cannot be used as button | ✅ Fixed — BUTTON_PIN changed to GPIO 13 |
| 2 | CS-CAM carrier board connector type unclear | ⏳ Need close-up photo to determine wiring method |
| 3 | Button not yet wired to active CS-CAM board | ⏳ In progress |
| 4 | Step 5 not yet uploaded or tested | ⏳ Blocked on wiring |
| 5 | Settings page UI only — not wired to backend | Deferred to V2 |
| 6 | ngrok URL changes on every restart | Deferred — fix with cloud deployment |

---

## Open Team Decisions

| Decision | Status |
|----------|--------|
| Audio response: buzzer tones vs voice (MAX98357A + speaker) | ⏳ Waiting for team |
| Cloud deployment: Railway vs Firebase | ⏳ Decide after hardware testing complete |

---

## What Comes After Step 5

Once button wiring is done and Step 5 tested:

1. **Connect ESP32 to backend** — fill `BACKEND_URL` in sketch, POST visits + heartbeat
2. **Settings API** — build GET/POST `/api/settings` endpoint, wire to dashboard
3. **Cloud deployment** — Railway or Firebase so ngrok is no longer needed
4. **V3** — DeepFace visitor recognition
