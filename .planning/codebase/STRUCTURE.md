# Codebase Structure

**Analysis Date:** 2026-06-23

## Directory Layout

```
Smart-Doorbell/
├── ESP32/                        # All ESP32-CAM Arduino firmware
│   ├── doorbell_step1_leds_button_buzzer.ino   # Step 1: LED/button/buzzer test (no WiFi)
│   ├── doorbell_step2_wifi_telegram/           # Step 2: WiFi + Telegram text
│   │   └── doorbell_step2_wifi_telegram.ino
│   ├── doorbell_step2_wifi_telegram_test/      # Step 2 variant test
│   ├── doorbell_step3_camera_test/             # Step 3: Camera capture to SPIFFS
│   │   └── doorbell_step3_camera_test.ino
│   ├── doorbell_step4_photo_telegram/          # Step 4: Send photo to Telegram
│   │   └── doorbell_step4_photo_telegram.ino
│   ├── doorbell_step5_complete_v1/             # Step 5: ACTIVE FIRMWARE (production)
│   │   ├── doorbell_step5_complete_v1.ino      # Main sketch — full doorbell flow
│   │   └── secrets.h.example                  # Credentials template (gitignored actual)
│   ├── doorbell_led_test/                      # LED test sketch directory
│   │   └── doorbell_led_test.ino
│   ├── parameters.h                            # GPIO pin constants for peripherals
│   ├── SECRETS.h                               # Legacy secrets template (example values only)
│   ├── compiled_program.bin                    # Pre-compiled binary (empty placeholder)
│   └── how_to_export_compiled_program.txt      # Arduino export instructions
│
├── backend/                      # Python FastAPI server + dashboard (deployed to Railway)
│   ├── server.py                 # FastAPI app — all routes, WebSocket, DB helpers
│   ├── telegram_bot.py           # Telegram Bot API async helpers
│   ├── recognition.py            # InsightFace face recognition (off by default)
│   ├── dashboard.html            # Single-file Vanilla JS SPA (served at GET /)
│   ├── requirements.txt          # Python dependencies
│   ├── Procfile                  # Railway start command: uvicorn server:app
│   └── railpack.json             # Railway build config (apt packages for OpenCV)
│
├── Documentation/                # Hardware documentation
│   └── connection diagram/       # Fritzing wiring diagram files
│
├── Unit Tests/                   # Test sketches mirroring ESP32/ step structure
│   ├── doorbell_led_test/
│   ├── doorbell_step2_wifi_telegram/
│   ├── doorbell_step2_wifi_telegram_test/
│   ├── doorbell_step3_camera_test/
│   ├── doorbell_step4_photo_telegram/
│   └── doorbell_step5_complete_v1/
│
├── PLAN.md                       # Feature checklist across V1–V4
├── PROGRESS.md                   # Build progress and session notes
├── SESSION_HANDOFF.md            # Full context for resuming in new sessions
├── README.md                     # Project overview
└── .gitignore                    # Ignores .env, secrets.h, backend/photos/, *.jpg
```

## Directory Purposes

**`ESP32/`:**
- Purpose: All Arduino firmware, organized as numbered build steps (incremental feature additions)
- Contains: `.ino` sketch files, one per feature step; shared header files
- Key files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` (active production firmware), `ESP32/parameters.h` (peripheral GPIO pin constants)

**`ESP32/doorbell_step5_complete_v1/`:**
- Purpose: The only sketch that runs on the deployed hardware
- Contains: Complete V1 firmware + secrets template
- Key files: `doorbell_step5_complete_v1.ino`, `secrets.h` (gitignored — fill from `secrets.h.example`)

**`backend/`:**
- Purpose: Python backend deployed to Railway; serves the dashboard and all API endpoints
- Contains: FastAPI server, Telegram helper, face recognition module, single-file dashboard HTML, deployment config
- Key files: `server.py` (all logic), `dashboard.html` (full UI)

**`Documentation/`:**
- Purpose: Hardware documentation and wiring diagrams
- Contains: Fritzing connection diagram files
- Key files: `Documentation/connection diagram/` (Fritzing .fzz)

**`Unit Tests/`:**
- Purpose: Test sketches submitted with hardware unit test report; mirrors `ESP32/` step structure
- Contains: One subdirectory per firmware step with associated test code
- Note: Populated to match the submitted HW unit test report (2026-06-09)

## Key File Locations

**Entry Points:**
- `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`: Firmware `setup()` + `loop()` — start here for ESP32 changes
- `backend/server.py`: FastAPI `app` instance, all REST/WebSocket routes, DB schema init via `lifespan` context
- `backend/dashboard.html`: Full dashboard SPA — `startApp()` at line ~1344 is the JS entry point

**Configuration:**
- `ESP32/doorbell_step5_complete_v1/secrets.h`: WiFi SSID/password, Telegram BOT_TOKEN, CHAT_ID — gitignored; copy from `secrets.h.example`
- `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:27`: `BACKEND_URL` constant — set to Railway production URL
- `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:42`: `TRIGGER_ON_BOOT` flag — `true` = demo mode (RESET = doorbell); `false` = real button
- `backend/.env`: Server secrets (`BOT_TOKEN`, `CHAT_ID`, `DATABASE_URL`, `SUPABASE_URL`, `SUPABASE_SERVICE_KEY`, `DASHBOARD_PASSWORD`) — gitignored
- `backend/Procfile`: Railway start command — `web: uvicorn server:app --host 0.0.0.0 --port $PORT`
- `backend/railpack.json`: Railway build — apt packages needed for OpenCV headless

**Core Logic:**
- `backend/server.py:366` (`create_visit`): Main POST /api/visits handler — photo upload, recognition, DB insert, Telegram notify, WebSocket broadcast
- `backend/server.py:594` (`telegram_webhook`): Incoming Telegram reply callback handler
- `backend/server.py:118` (`ConnectionManager`): WebSocket broadcast hub
- `backend/telegram_bot.py:33` (`send_visit_notification`): Formats and sends Telegram photo with reply buttons
- `backend/recognition.py:75` (`match_or_create_visitor`): pgvector cosine similarity visitor matching

**Database Schema (auto-created by `init_db()` in `backend/server.py:80`):**
- `visits`: id, trigger, timestamp, photo_url, response, tag, silent, telegram_message_id, visitor_id
- `device_status`: id (always 1), wifi_signal, last_sync
- `settings`: id (always 1), quiet_enabled, quiet_start, quiet_end, timezone
- `visitors` (recognition only): id, name, photo_url, first_seen, last_seen, visit_count
- `visitor_embeddings` (recognition only): id, visitor_id, embedding (pgvector)

**Testing:**
- `Unit Tests/`: Hardware test sketches for each firmware step (Arduino IDE only, no automated test runner)

## Naming Conventions

**Files:**
- Firmware sketches: `doorbell_step<N>_<description>.ino` — lowercase with underscores, prefixed by step number
- Python modules: lowercase snake_case (`server.py`, `telegram_bot.py`, `recognition.py`)
- Header files: lowercase or UPPERCASE for legacy files (`secrets.h`, `parameters.h`, `SECRETS.h`)
- Secrets template: `secrets.h.example` (same name as gitignored file + `.example` suffix)

**Directories:**
- Firmware steps: `doorbell_step<N>_<description>/` matching their `.ino` filename
- Backend: flat — all Python files in `backend/` root, no subdirectories

**Python identifiers:**
- Routes / DB helpers: snake_case (`create_visit`, `get_visits`, `db_fetchall`)
- Pydantic models: PascalCase (`TagUpdate`, `DeviceHeartbeat`, `SettingsUpdate`)
- Constants: UPPER_SNAKE_CASE (`DATABASE_URL`, `BACKEND_URL`, `RECOGNITION_ENABLED`)

**C++ (Arduino) identifiers:**
- Functions: camelCase (`triggerDoorbell`, `sendHeartbeat`, `initCamera`)
- Constants / pin defines: UPPER_SNAKE_CASE (`BUTTON_PIN`, `FLASH_LED`, `DEBOUNCE_MS`)
- Credential variables: camelCase with type prefix (`WIFI_SSID`, `BOT_TOKEN`, `CHAT_ID`)

## Where to Add New Code

**New firmware feature (ESP32):**
- Primary code: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` — add helper functions below the `loop()` block following the existing `// ── Section name ──` comment dividers
- New GPIO pins: add `#define` near line 35 and initialize in `setup()`
- New secrets: add a `#define SECRET_*` line in `ESP32/doorbell_step5_complete_v1/secrets.h.example` and reference via `const char*` in the main sketch

**New backend API endpoint:**
- Implementation: `backend/server.py` — add FastAPI route function below the existing route group it belongs to (visits, settings, visitors, device, admin sections)
- Auth: add `dependencies=[Depends(require_dashboard_key)]` for dashboard-facing endpoints; omit for device endpoints (ESP32 calls)
- DB schema change: add `ALTER TABLE` / `CREATE TABLE` inside `init_db()` at `backend/server.py:80`

**New dashboard page/feature:**
- Implementation: `backend/dashboard.html` — add HTML section in the `<!-- PAGE -->` block area (around line 700), add nav item in `#sidebar nav`, add entry to the `pages` object (around line 901), wire data loading in the nav `click` handler

**New Telegram action:**
- Implementation: `backend/telegram_bot.py` — add an async function using `httpx.AsyncClient`; call it from `backend/server.py` in the relevant route

**New recognition feature:**
- Implementation: `backend/recognition.py` — all recognition logic stays in this module; functions accept `db_fetchone`/`db_execute` as injected dependencies so they stay testable

**New step sketch (firmware prototype):**
- Implementation: Create `ESP32/doorbell_step<N>_<description>/doorbell_step<N>_<description>.ino`
- Mirror in: `Unit Tests/doorbell_step<N>_<description>/` for the test version

## Special Directories

**`backend/photos/` (runtime-generated, not committed):**
- Purpose: Local fallback photo storage when Supabase Storage is unavailable
- Generated: Yes, at server startup (`os.makedirs(PHOTOS_DIR, exist_ok=True)`)
- Committed: No — listed in `.gitignore`

**`ESP32/doorbell_step5_complete_v1/secrets.h` (runtime-filled, not committed):**
- Purpose: WiFi and Telegram credentials injected at firmware compile time
- Generated: No — must be manually created by copying `secrets.h.example` and filling in values
- Committed: No — listed in `.gitignore`

**`.planning/codebase/` (GSD planning output):**
- Purpose: Codebase maps generated by `/gsd:map-codebase` for use by planning and execution agents
- Generated: Yes, by GSD tooling
- Committed: Yes (planning artifacts)

---

*Structure analysis: 2026-06-23*
