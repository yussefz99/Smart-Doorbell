# Smart Doorbell – Group 15
## Progress Log

---

## Project Overview

An IoT smart doorbell built on the ESP32-CAM. When a visitor presses the physical button, the system captures a photo and sends a Telegram notification to the homeowner. The homeowner can reply and the visitor receives audio feedback through the doorbell speaker.

**Hardware:** ESP32-CAM (AI Thinker), push button, RGB LEDs, passive buzzer, MB USB downloader  
**Stack:** Arduino C++ (firmware) · Python FastAPI (backend) · Vanilla JS (dashboard) · Telegram Bot API

---

## Session 1 — Wiring Diagram

### What was done
- Produced a full wiring diagram for the ESP32-CAM connected to all Step 1 components
- Identified a **GPIO 13 conflict**: both the red LED and the PIR sensor share GPIO 13
- Decision: reassign red LED to **GPIO 4** before adding PIR sensor

### GPIO Pin Assignment (Version 1)

| GPIO | Component | Direction | Notes |
|------|-----------|-----------|-------|
| 2 | Green LED | OUTPUT | Idle state |
| 4 | Red LED | OUTPUT | Capturing state (moved from GPIO 13) |
| 15 | Yellow LED | OUTPUT | Sending state |
| 12 | Buzzer | OUTPUT | Passive buzzer |
| 14 | Button | INPUT_PULLUP | Press = LOW |
| 0 | Built-in flash LED | OUTPUT | Illuminate scene |

### Files produced
- `doorbell_step1_diagram.json` — Wokwi wiring diagram

---

## Session 2 — Step 1 Firmware

### What was done
- Wrote and tested Step 1 firmware covering:
  - **Boot sequence** — cycles green → red → yellow with rising beeps, flashes all three
  - **Button press cycle** — idle (green) → capturing (red) → sending (yellow + beep) → idle
  - **Debounced button read** — fires once per press on release
  - **`setError()`** — all LEDs flash with low beep × 6, returns to idle
  - **Serial logging** at 115200 baud for every state transition
  - **Serial error test** — send `'e'` from Serial Monitor to trigger error state
- Tested on **Wokwi simulator** — boot sequence and button cycle confirmed working
- Decided not to proceed to Step 2 without physical hardware

### Files produced
- `ESP32/doorbell_step1_leds_button_buzzer.ino`

---

## Session 3 — Hardware & Upload Explanation

### What was covered
- How the MB downloader board connects ESP32-CAM to USB
- Flash mode vs run mode (GPIO 0 to GND for flashing)
- Arduino IDE board setup: **AI Thinker ESP32-CAM**
- How `INPUT_PULLUP` works for the button
- Full upload procedure step by step
- What changes between each build step

### Upload Procedure (reference)
1. Wire GPIO 0 to GND
2. Connect USB, select board + COM port in Arduino IDE
3. Click Upload
4. When `Connecting....____` appears → press RESET once
5. Wait for "Done uploading"
6. Disconnect GPIO 0 from GND
7. Press RESET again
8. Open Serial Monitor at 115200 baud

---

## Session 4 — WiFi + Telegram Sketch

### What was done
- Built Step 2 firmware: WiFi connection + Telegram text message test
- Covers:
  - WiFi connect with 15-second timeout
  - Green LED blinks while connecting, solid when connected
  - Sends one Telegram text on boot to confirm pipeline works
  - Yellow LED flashes 3× on success, red blinks 6× on failure
  - Full Serial Monitor logging with error codes
- **Status:** built and ready — waiting for physical hardware to test

### Telegram Setup (completed)
- Bot created via BotFather ✅
- Bot token obtained ✅
- Chat ID obtained via `/getUpdates` ✅
- Credentials filled into sketch ✅

### Files produced
- `ESP32/doorbell_step2_wifi_telegram.ino`

---

## Session 5 — Backend Server

### What was done
- Built a complete Python FastAPI backend server with SQLite database
- All endpoints matching the agreed API contract:

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/api/visits` | All visits, newest first |
| POST | `/api/visits` | ESP32 posts new visit + photo |
| PATCH | `/api/visits/:id/tag` | Set tag (Expected / Unknown / Suspicious) |
| GET | `/api/stats` | Visits per day + hourly breakdown |
| GET | `/api/device/status` | Uptime, WiFi signal, last sync |
| POST | `/api/device/heartbeat` | ESP32 reports WiFi signal periodically |
| WS | `/ws` | Push new visits to dashboard in real time |

- WebSocket manager — broadcasts new visits to all connected dashboard clients
- Photos saved to `backend/photos/` directory and served as static files
- 7 demo visits seeded automatically on first run
- CORS enabled for dashboard connection
- **Status:** fully tested, running on `localhost:8000`

### How to start
```
cd backend
uvicorn server:app --reload --port 8000
```

### Visit data shape
```json
{
  "id": 42,
  "trigger": "button",
  "timestamp": "2026-05-25T14:32:10Z",
  "photo_url": "/photos/visit_20260525_143210.jpg",
  "response": "On my way",
  "tag": "Expected",
  "silent": false
}
```

### Files produced
- `backend/server.py`
- `backend/requirements.txt`

---

## Session 6 — Web Dashboard

### What was done
- Built a complete single-file dashboard (`backend/dashboard.html`)
- Dark theme, fully connected to the backend — no mock data
- WebSocket connection for live visit push with toast notifications
- Polling fallback every 30 seconds

### Pages

**Visit History**
- 4 stat cards: today's visits, this week, button presses, motion triggers
- Table: photo thumbnail, date/time + relative time, trigger badge, response (with "via Telegram"), tag dropdown
- Filter bar: All / Button / Motion / Untagged
- Tag dropdown PATCHes to backend immediately on change
- Silent badge on motion rows during quiet hours
- Live toast notification when new visit arrives via WebSocket

**Statistics**
- 4 summary cards: total visits, button presses, motion triggers, response rate
- Bar chart: visits per day (last 7 days) with All / Button / Motion toggle
- Heatmap: hourly activity across 24h with All / Button / Motion toggle

**Diagnostics**
- Live uptime ticker (counts up in real time)
- WiFi signal with strength label
- Last sync timestamp
- Camera status indicator
- System log with timestamped colour-coded entries

**Settings**
- Quiet hours: enable toggle + start/end time pickers
- Motion detection: alerts toggle, sensitivity slider (1–5), cooldown dropdown
- Telegram: per-alert-type toggles, bot status indicator
- Device: offline buffer, LED indicators, buzzer toggles
- Note: settings are UI-only — will be wired to backend in Version 2

### Files produced
- `backend/dashboard.html`

---

## Session 7 — Telegram Webhook + ngrok

### What was done
- Created `backend/telegram_bot.py` — all Telegram logic in one place:
  - `send_visit_notification()` — sends photo + two inline reply buttons to homeowner
  - `answer_callback()` — removes loading spinner after button tap
  - `remove_buttons()` — edits message to show confirmed reply, removes buttons
  - `set_webhook()` / `get_webhook_info()` / `delete_webhook()` — webhook management
- Updated `server.py`:
  - Calls `send_visit_notification()` automatically on every new visit
  - Stores `telegram_message_id` to link callbacks back to visits
  - Added `POST /telegram/webhook` — receives homeowner button taps from Telegram
  - Added `POST /admin/set-webhook` — registers webhook URL with Telegram
  - Added `GET /admin/webhook-info` — check current webhook status
  - Added DB migration for `telegram_message_id` column
- Created `backend/.env` — stores BOT_TOKEN and CHAT_ID securely
- Updated dashboard to handle `visit_updated` WebSocket event — Response column updates live when homeowner replies
- Installed and configured **ngrok** to expose localhost to Telegram

### Full flow tested and confirmed working
```
POST /api/visits
  → saved to DB
  → Telegram notification sent with [✅ On my way] [🏠 Not home] buttons
  → Homeowner taps button
  → Telegram calls /telegram/webhook
  → Response saved to DB
  → WebSocket pushes update to dashboard
  → Response column shows "On my way · via Telegram" live
```

### Infrastructure decision
- Using **ngrok** for now (temporary public URL)
- Will decide between **Railway** and **Firebase** deployment after hardware arrives

### Files produced / updated
- `backend/telegram_bot.py` — new
- `backend/.env` — new (not committed to git)
- `backend/server.py` — updated (webhook endpoint, Telegram call, migration)
- `backend/dashboard.html` — updated (handles `visit_updated` WebSocket event)

---

---

## Session 8 — Hardware Wiring + GPIO 0 Fix

### What was done

#### GPIO 0 / Camera XCLK Conflict — Root Cause Found and Fixed
- **Problem:** Step 5 sketch was taking continuous photos on its own
- **Root cause:** GPIO 0 is used by BOTH the BOOT button AND the camera XCLK pin
  - After `esp_camera_init()`, the ESP32's LEDC peripheral takes over GPIO 0 and drives it as a 20 MHz clock signal to the OV2640 sensor
  - `digitalRead(0)` after camera init reads the clock oscillation, not a button — triggers continuously
- **Fix:** Changed `BUTTON_PIN` from `0` to `13`
  - GPIO 13 is free on the AI Thinker ESP32-CAM — not used by the camera
  - Rewrote `loop()` with proper state-change debounce logic
  - Added all missing forward declarations (`triggerDoorbell`, `sendTextNotification`)
  - Removed the broken `volatile bool doorbellTriggered` flag approach
- **Result:** `doorbell_step5_complete_v1.ino` updated and ready to upload

#### Hardware Identification
- Confirmed the CS-CAM carrier board by see-sys.co.il has:
  - The ESP32-CAM (AI Thinker) module seated on top
  - Built-in CH9102 USB-Serial chip (no separate programmer needed)
  - 3D-printed gray button attachment with BOOT (B) and RESET (R) buttons
  - Side pin headers with labels: `3V3, GND, D15, D2, D4, RX2, TX2, D5, D18, D19, D21, RX0, TX0, D22, D23` (left) and `VIN, GND, D13, D12, D4, D27, D26, D25, D33, D32, D35, D34, UN, UP, EN` (right)
- **D13** is on the right side of the CS-CAM carrier board — confirmed visible

#### Wiring Attempt — External Button to GPIO 13
- Button placed on right breadboard, rows 9–12
- Wires connected from loose spare board (wrong board — needs to be active CS-CAM board)
- Connector type issue discovered:
  - The active CS-CAM carrier board connection points are **not standard female sockets**
  - User's wires have female ends — won't connect directly
  - Male-to-male wires available as bridge adapters
- **Status: In progress** — need close-up photo of CS-CAM pin area to determine exact connector type and correct wiring method

### Files updated
- `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` — GPIO 0 fix applied, BUTTON_PIN changed to 13, loop rewritten with proper debounce

### Current blocker
The CS-CAM carrier board pin/pad type is unclear — need to see close-up to confirm whether they are:
- Through-hole pads (bare holes, no headers soldered) → need to solder or use friction-fit male pins
- Some other connector type

---

## Open Decisions

| # | Decision | Status |
|---|----------|--------|
| 1 | Audio response to visitor: buzzer tones vs voice playback (MAX98357A + speaker) | ⏳ Waiting for team decision |
| 2 | PIR motion sensor: include in V1 or defer to V4 | Deferred to V4 |
| 3 | Cloud deployment: Railway vs Firebase | ⏳ Decide after hardware arrives |

---

## Known Issues

| # | Issue | Resolution |
|---|-------|------------|
| 1 | GPIO 13 shared by red LED and PIR sensor | Red LED reassigned to GPIO 4 |
| 2 | Camera capture untested | Waiting for physical hardware |
| 3 | Settings toggles are UI only | Will be wired to backend in V2 |
| 4 | ngrok URL changes on every restart | Must re-register webhook each time — fixed by cloud deployment later |
| 5 | GPIO 0 (BOOT button) used as camera XCLK — continuous photo trigger | Fixed: BUTTON_PIN changed to GPIO 13, external button required |
| 6 | CS-CAM carrier board pin connector type unclear | Pending close-up photo — may require soldering |
