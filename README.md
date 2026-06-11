# Smart Doorbell — Group 15

## Project by:
Yussef Zarkawi and team (Group 15)

## Details about the project
An IoT smart doorbell built on the ESP32-CAM. When a visitor presses the button, the device captures a photo and uploads it to a cloud backend, which stores the visit, hosts the photo, and sends a Telegram notification to the homeowner with two reply buttons (✅ On my way / 🏠 Not home). The homeowner's reply is saved and shown live on a password-protected web dashboard. The device reports a heartbeat every minute so the dashboard shows real online/offline status.

### Architecture
```
ESP32-CAM ──(HTTPS)──▶ FastAPI on Railway ──▶ Supabase PostgreSQL
 button press                │                 Supabase Storage (photos)
 photo upload                ├──▶ Telegram Bot API (notification + reply webhook)
 heartbeat / RSSI            └──▶ Web dashboard (WebSocket live updates)
```

- **Live dashboard:** https://smart-doorbell-production.up.railway.app (password protected)
- **Backend:** Python FastAPI deployed on Railway (auto-deploys from this repo, root dir `/backend`)
- **Database:** Supabase PostgreSQL (visits, visitors, device status, settings) + Supabase Storage for photos
- **Notifications:** Telegram Bot API with inline reply buttons and a permanent webhook (no ngrok)

### Features
- Photo capture on button press → single Telegram notification with photo + reply buttons
- Homeowner reply round-trip: Telegram → backend → database → live dashboard update
- Dashboard: visit history with photo lightbox, statistics (daily/hourly charts), diagnostics with real device online/offline heartbeat, quiet-hours settings
- Quiet hours: visits during the configured window are recorded but delivered silently
- Password gate on all dashboard/API read endpoints
- Face recognition (visitor identification, "Rami is at the door") — implemented end-to-end but disabled by default (`RECOGNITION_ENABLED=1` to enable; see SESSION_HANDOFF.md)

## Folder description:
* **ESP32**: source code for the esp side (firmware). Main sketch: `doorbell_step5_complete_v1/` (credentials go in `secrets.h`, copy from `secrets.h.example`)
* **backend**: FastAPI server (`server.py`), Telegram helper (`telegram_bot.py`), face recognition module (`recognition.py`), dashboard (`dashboard.html`), deployment files (`Procfile`, `railpack.json`, `requirements.txt`)
* **Documentation**: wiring diagram + basic operating instructions
* **Unit Tests**: tests for individual hardware components (input / output devices) — step-by-step sketches: LEDs/button/buzzer, WiFi+Telegram, camera capture, photo upload, complete flow
* **flutter_app**: dart code for our Flutter app (not part of V1)
* **PLAN.md / PROGRESS.md / SESSION_HANDOFF.md**: planning, session-by-session progress log, and current-state handoff

## ESP32 SDK version used in this project:
ESP32 Arduino core (board: **AI Thinker ESP32-CAM**) — see Arduino IDE → Boards Manager for the installed version

## Arduino/ESP32 libraries used in this project:
* esp_camera — bundled with the ESP32 core
* WiFi — bundled with the ESP32 core
* WiFiClientSecure — bundled with the ESP32 core
* (no external Arduino libraries required)

Backend Python dependencies are listed in `backend/requirements.txt` (FastAPI, uvicorn, psycopg2, httpx, insightface, …).

## Connection diagram:
- ESP32-CAM mounted on the CS-CAM carrier board (see-sys.co.il) — USB programming via onboard CH9102
- External doorbell button: **GPIO 13 (IO13) → button → GND** (internal pull-up, no resistor)
- Built-in flash LED on GPIO 4 used as status indicator
- ⚠️ GPIO 0 is the camera XCLK — it cannot be used as a button input

## How to run
```bash
# Backend (local development — uses the cloud database via backend/.env)
cd backend
pip install -r requirements.txt
uvicorn server:app --reload --port 8001

# Deployment: push to main — Railway auto-deploys
```
Firmware: open `ESP32/doorbell_step5_complete_v1/` in Arduino IDE, create `secrets.h` from the example, select board *AI Thinker ESP32-CAM*, upload at 115200 baud.

## Project Poster:
(to be added)

This project is part of ICST - The Interdisciplinary Center for Smart Technologies, Taub Faculty of Computer Science, Technion
https://icst.cs.technion.ac.il/
