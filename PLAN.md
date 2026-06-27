# Smart Doorbell – Group 15
## Project Plan & Checklist

---

## Version 1 — Working Prototype
**Goal:** Visitor presses button → ESP32-CAM captures photo → Telegram notification sent

### Firmware
- [x] Step 1: LEDs + button + buzzer (Wokwi tested)
- [x] Step 2: WiFi connection + Telegram text message (built, needs hardware test)
- [x] Step 3: Camera capture test — capture photo (OV2640 init OK)
- [x] Step 4: Send photo to Telegram (via backend; photo + reply buttons arrive)
- [x] Step 5: Full V1 loop — button press → capture → send (flash LED for status; external LEDs/buzzer not used on CS-CAM build)

### Hardware
- [x] GPIO pin assignment finalised (GPIO 13 conflict resolved)
- [x] Wiring diagram produced
- [x] Physical hardware assembled and wired (CS-CAM board, button on IO13+IO14)
- [x] Upload procedure tested on real ESP32-CAM (USB-B; hold BOOT during "Connecting...")

### Testing
- [x] WiFi connects and Serial Monitor shows IP address (10.0.0.60)
- [x] Telegram message arrives on phone (photo + reply buttons)
- [x] Camera initialises without error
- [x] Photo captured
- [x] Photo sent to Telegram on button press
- [x] Status shown via built-in flash LED (capturing / sending / done)
- [ ] ~~LED states green → red → yellow~~ — N/A on CS-CAM build (single flash LED only)
- [ ] ~~Buzzer confirmation beep~~ — N/A on CS-CAM build (not wired)

---

## Version 2 — Better Interaction & Logging
**Goal:** Homeowner can reply, visits are stored, dashboard shows real data

### Backend
- [x] FastAPI server with SQLite database
- [x] GET `/api/visits` — returns all visits newest first
- [x] POST `/api/visits` — receives photo + metadata from ESP32
- [x] PATCH `/api/visits/:id/tag` — sets tag from dashboard
- [x] GET `/api/stats` — visits per day + hourly breakdown
- [x] GET `/api/device/status` — uptime, WiFi signal, last sync
- [x] POST `/api/device/heartbeat` — ESP32 reports status
- [x] WebSocket `/ws` — pushes new visits to dashboard live
- [ ] GET/POST `/api/settings` — save and load settings
- [ ] Telegram webhook endpoint — receive homeowner replies

### Dashboard
- [x] Visit History page (live data, filter bar, tag dropdown)
- [x] Statistics page (bar chart, heatmap)
- [x] Diagnostics page (live uptime, WiFi, system log)
- [x] Settings page (UI complete)
- [ ] Settings wired to backend (read/write from `/api/settings`)

### Telegram Bot
- [x] Bot created via BotFather
- [x] Token and Chat ID obtained
- [x] Telegram webhook set up to receive replies (via ngrok)
- [x] Reply buttons sent with every notification ("On my way" / "Not home")
- [x] Homeowner reply stored in `visits.response` column
- [x] Dashboard Response column updates live via WebSocket
- [ ] Voice response to visitor (decision pending — see open decisions)

### ESP32 Firmware
- [ ] POST visit + photo to backend after each doorbell press
- [ ] POST heartbeat to `/api/device/heartbeat` every 60 seconds
- [ ] Read settings from backend on boot

---

## Version 3 — Visitor Recognition
**Goal:** Returning visitors are identified by name

- [ ] DeepFace installed on backend server
- [ ] Known visitor registration endpoint (`POST /api/visitors`)
- [ ] Face encoding stored in database on registration
- [ ] Incoming photo compared against known faces on each visit
- [ ] Telegram notification includes visitor name if recognised
- [ ] Dashboard shows visitor name in visit table
- [ ] Register known visitor via dashboard UI

---

## Version 4 — Advanced Features
**Goal:** Motion detection, offline resilience, fine-grained control

### Motion Detection (PIR sensor — optional)
- [ ] HC-SR501 PIR sensor wired to GPIO 13
- [ ] Motion trigger added to firmware
- [ ] Quiet hours enforced on ESP32 (no Telegram alert during quiet hours)
- [ ] Motion sensitivity adjustable via settings

### Audio Response (decision pending)
- **Option A — Buzzer tones** (no new hardware)
  - [ ] Define two distinct tone patterns (on my way / not home)
  - [ ] Play correct tone when backend command received
- **Option B — Voice playback** (MAX98357A + speaker required)
  - [ ] MAX98357A I2S DAC wired to ESP32 (GPIO 26/25/22)
  - [ ] Audio files (.wav) recorded and stored on SPIFFS
  - [ ] ESP32 plays correct audio clip on command from backend

### Offline Buffering
- [ ] Detect WiFi loss on ESP32
- [ ] Store visits locally in SPIFFS when offline
- [ ] Sync buffered visits to backend when WiFi reconnects

### Settings enforcement on ESP32
- [ ] Motion alerts toggle respected
- [ ] Quiet hours start/end times respected
- [ ] LED indicators toggle respected
- [ ] Buzzer toggle respected

---

## What Can Be Built Without Hardware

| Task | Version | Status |
|------|---------|--------|
| Backend server | V2 | ✅ Done |
| Dashboard | V2 | ✅ Done |
| Telegram webhook + reply buttons | V2 | ✅ Done |
| Settings backend endpoint | V2 | ⬜ Ready to build |
| Wire dashboard settings to backend | V2 | ⬜ Ready to build |

## What Requires Hardware

| Task | Version | Blocked on |
|------|---------|-----------|
| WiFi + Telegram test | V1 | ESP32-CAM |
| Camera capture | V1 | ESP32-CAM |
| Full V1 doorbell loop | V1 | ESP32-CAM |
| PIR motion sensor | V4 | Hardware + decision |
| Audio response | V4 | Hardware + team decision |

---

## Next Actions (in order)

1. **Team decision** — buzzer tones vs voice playback for visitor audio response
2. ~~Build Telegram webhook~~ ✅ Done
3. **Build settings API** — save/load settings from backend, wire to dashboard
4. **Hardware arrives** → test WiFi sketch → test camera → complete V1
5. **Connect ESP32 to backend** — POST visits + heartbeat
6. **Cloud deployment** — Railway or Firebase (decide after hardware testing)
7. **V3** — add DeepFace visitor recognition
