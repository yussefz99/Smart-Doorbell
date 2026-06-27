<!-- GSD:project-start source:PROJECT.md -->
## Project

**Smart Doorbell — Group 15**

An IoT smart doorbell built on the ESP32-CAM. A visitor presses the physical
button, the device captures a photo and uploads it to a FastAPI backend, which
sends the homeowner a Telegram notification with inline reply buttons. The
homeowner's reply flows back through a Telegram webhook, and every visit —
including the reply — appears live on a web dashboard. This is a university
capstone project (Group 15), graded on **both a live demo and a written
report/poster**.

**Core Value:** When a visitor presses the doorbell, the homeowner reliably receives a photo
notification on Telegram and can respond — and that full round-trip is visible
live on the dashboard. If everything else fails, this press → photo → notify →
reply → dashboard loop must work.

### Constraints

- **Tech stack**: Frozen — FastAPI + Supabase + Railway (backend), Arduino/ESP32-CAM (firmware), vanilla JS (dashboard). No rewrites; keep tested code.
- **Deliverable**: Must produce a working **live demo** AND a written **report/poster** — both are graded.
- **Hardware**: Single AI Thinker ESP32-CAM on a CS-CAM carrier; GPIO 13 button now soldered and working.
- **Budget**: Railway trial credit (~$5/30 days) and Supabase free tier — keep the service online through demo day.
- **Security**: Lab-prototype bar — close the cheap, high-value gaps (endpoint auth, webhook token); cert pinning and connection pooling are explicitly deferred.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

## Languages
- C/C++ (Arduino dialect) — ESP32 firmware (`ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` and all earlier step sketches)
- Python 3 — Backend server (`backend/server.py`, `backend/telegram_bot.py`, `backend/recognition.py`)
- JavaScript (Vanilla ES2020+) — Browser dashboard embedded in `backend/dashboard.html`
- HTML/CSS — Dashboard UI, single-file (`backend/dashboard.html`, ~1400 lines)
- Flutter app directory (`flutter_app/`) exists but contains only a placeholder file — no Dart code implemented yet
## Runtime
- Python 3.13 (inferred: `psycopg2-binary` ≥2.9.10 required for Python 3.13 wheels)
- ASGI runtime via `uvicorn[standard]`
- Arduino framework on ESP-IDF
- Target board: AI Thinker ESP32-CAM (dual-core Xtensa LX6, 240 MHz)
- Flash: 4 MB
- pip with `requirements.txt`
- Lockfile: Not present (no `pip.lock` or `poetry.lock`)
## Frameworks
- FastAPI 0.111.0 — REST API + WebSocket server (`backend/server.py`)
- Uvicorn 0.29.0 (standard extras) — ASGI server; started via `backend/Procfile` as `uvicorn server:app --host 0.0.0.0 --port $PORT`
- Pydantic (bundled with FastAPI) — request body validation via `BaseModel`
- No framework — Vanilla JS + native browser `fetch`, `WebSocket`, and CSS variables
- Chart rendering: pure canvas/DOM drawing, no charting library
- Arduino core for ESP32 (Arduino IDE, board: AI Thinker ESP32-CAM)
- No PlatformIO — projects use the standard `.ino` Arduino sketch format
- InsightFace ≥1.0 — ONNX-based face analysis (`backend/recognition.py`)
- ONNXRuntime ≥1.20 — inference backend (CPU only, `CPUExecutionProvider`)
- OpenCV (headless) ≥4.11 — image decode/preprocess
## Key Dependencies
- `fastapi==0.111.0` — main API framework
- `uvicorn[standard]==0.29.0` — production ASGI server
- `psycopg2-binary==2.9.10` — PostgreSQL driver (Supabase); must be ≥2.9.10 for Python 3.13 wheels
- `httpx==0.27.0` — async HTTP client for Telegram Bot API calls and Supabase Storage REST API
- `python-multipart==0.0.9` — multipart/form-data parsing (ESP32 photo uploads)
- `python-dotenv==1.0.0` — `.env` loading for local development
- `insightface>=1.0` — face detection + embedding extraction (model: `buffalo_s` or `buffalo_l`)
- `onnxruntime>=1.20` — ONNX model inference
- `opencv-python-headless>=4.11` — image decode without display dependencies
- `psutil>=6` — process memory reporting for `/api/recognition/health`
- `railpack.json` specifies apt packages for Railway: `libgl1`, `libglib2.0-0`, `libxcb1`, `libx11-6`, `libxext6`, `libsm6` (required by OpenCV/InsightFace even in headless mode)
- `esp_camera.h` — camera capture (ESP-IDF bundled, board-specific)
- `WiFi.h` — WiFi STA mode connection
- `WiFiClientSecure.h` — TLS over TCP for HTTPS calls (certificate check disabled via `setInsecure()`)
- `HTTPClient.h` — used in earlier steps (step2), replaced by raw `WiFiClientSecure` in step5
## Configuration
- Loaded from `backend/.env` locally (gitignored) and Railway Variables in production
- Required env vars:
- Optional env vars:
- Secrets in `ESP32/doorbell_step5_complete_v1/secrets.h` (gitignored; template at `ESP32/doorbell_step5_complete_v1/secrets.h.example`)
- Defines: `SECRET_WIFI_SSID`, `SECRET_WIFI_PASSWORD`, `SECRET_BOT_TOKEN`, `SECRET_CHAT_ID`
- Hardcoded backend URL in sketch: `BACKEND_URL = "https://smart-doorbell-production.up.railway.app"` (`doorbell_step5_complete_v1.ino` line 27)
- Demo-mode flag: `TRIGGER_ON_BOOT true` (line 42) — fires one event on RESET; set to `false` after button is soldered
- Backend: no build step — Python source deployed directly
- Firmware: Arduino IDE (board: AI Thinker ESP32-CAM, port COM3, baud 115200); pre-compiled binary at `ESP32/compiled_program.bin`
## Platform Requirements
- Python 3.13+
- Arduino IDE with ESP32 board support package (AI Thinker ESP32-CAM profile)
- Supabase project with pgvector extension enabled (required for face recognition table `visitor_embeddings`)
- Backend deployed on **Railway** (PaaS, auto-deploys on push to `main` branch of GitHub repo `yussefz99/Smart-Doorbell`)
- Root Directory set to `/backend` in Railway service config
- Database on **Supabase** (`aws-1-ap-northeast-1` region, Tokyo; RLS enabled)
- Photo storage on **Supabase Storage** (public bucket named `photos`)
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

## ESP32 Firmware (Arduino C/C++)
### Files
- Sketches use `.ino` extension: `doorbell_step5_complete_v1.ino`
- Each sketch lives in a same-named subdirectory: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`
- Secret header lives alongside the sketch: `secrets.h` (gitignored), with a committed template `secrets.h.example`
- Shared constants (non-secret) live in a top-level header: `ESP32/parameters.h`
### Naming Patterns
- ALL_CAPS with underscores: `PIN_BUZZER`, `DEBOUNCE_MS`, `BEEP_SHORT_MS`, `HEARTBEAT_MS`
- Secret macros prefixed `SECRET_`: `SECRET_WIFI_SSID`, `SECRET_BOT_TOKEN`
- camelCase: `lastDebounceTime`, `lastButtonState`, `lastPressTime`, `lastHeartbeat`
- Type annotations aligned at declaration: `unsigned long lastDebounceTime = 0;` / `int lastButtonState = HIGH;`
- camelCase verbs: `connectWiFi()`, `initCamera()`, `capturePhoto()`, `sendHeartbeat()`, `triggerDoorbell()`
- State-setting helpers prefixed with the action: `setIdle()`, `setCapturing()`, `setSending()`, `setFlash(bool on)`
- LED helpers: `blinkLED(pin, times, onMs, offMs)`, `allLEDsOff()`
- Boolean return for operations that can fail: `bool connectWiFi()`, `bool initCamera()`, `bool sendPhotoToTelegram()`
### File Headers
### Section Dividers
- Subsection header (thin): `// ── Section name ─────────────────────`
- Major section header (double-line): `// ════════════════════════... / //  Section Name / // ════════════════...`
### Serial Logging
- `[OK]` — success
- `[ERROR]` — failure
- `[WiFi]`, `[Camera]`, `[Telegram]`, `[Backend]`, `[Heartbeat]`, `[BUTTON]`, `[DEMO]`, `[INFO]` — subsystem prefix
### Error Handling
- Functions that can fail return `bool` (true = success, false = failure)
- Callers check return value immediately: `if (!connectWiFi()) { ... return; }`
- On error in `setup()`: print `[ERROR]` message, blink flash/LED N times, then `return` to halt further init
- Camera frame buffer always returned: `esp_camera_fb_return(fb)` called after use
- Timeouts use `millis()` arithmetic with explicit timeout constants
### Forward Declarations
### Secrets Management
- Real credentials live in `secrets.h` (gitignored)
- Template provided as `secrets.h.example` (committed)
- Credentials are only assigned after `#include "secrets.h"`:
- Test sketches under `Unit Tests/` use plain-text placeholder strings (`"YOUR_WIFI_SSID"`) — they are reference copies, not production code
### Demo / Feature Flags
#define TRIGGER_ON_BOOT  true
#if TRIGGER_ON_BOOT
#endif
## Python Backend (`backend/`)
### Files
- `server.py` — FastAPI application, all routes and database helpers
- `telegram_bot.py` — Telegram API async helper functions
- `recognition.py` — lazy-loaded InsightFace wrapper
- `dashboard.html` — single-file frontend served by the backend
- `requirements.txt` — pinned dependencies
### Naming Patterns
- `snake_case`: `get_db()`, `db_fetchall()`, `db_fetchone()`, `db_execute()`, `init_db()`, `in_quiet_hours()`, `row_to_visit()`, `require_dashboard_key()`
- `PascalCase`: `TagUpdate`, `DeviceHeartbeat`, `SettingsUpdate`, `VisitorRename`, `ConnectionManager`
- `snake_case` verbs: `get_visits()`, `create_visit()`, `delete_visit()`, `update_tag()`, `get_settings()`, `update_settings()`, `device_heartbeat()`, `get_device_status()`
- ALL_CAPS: `DATABASE_URL`, `SUPABASE_URL`, `SUPABASE_SERVICE_KEY`, `STORAGE_BUCKET`, `DASHBOARD_PASSWORD`, `RECOGNITION_ENABLED`, `TELEGRAM_API`, `PUBLIC_BASE_URL`, `SERVER_START_TIME`
- `snake_case`: `photo_url`, `visit_id`, `callback_id`, `visitor_id`
### File Header
### Section Dividers
### Import Organization
### Error Handling
- FastAPI routes raise `HTTPException` with explicit status codes and messages:
- Non-fatal failures (Telegram, face recognition, Storage) caught with broad `except Exception as e` and `print(f"[subsystem] failed: {e!r}")` — never allowed to break the visit creation response
- DB helpers (`db_fetchone`, `db_execute`) use context managers for connections
### Logging
- `print()` with bracketed subsystem prefix (no logging framework):
### Env Var Hygiene
### Async Patterns
- Route functions that call external services (`httpx`) are `async def`
- CPU-bound work (face recognition) offloaded with `asyncio.to_thread()`
- WebSocket manager uses `async def` for `connect` and `broadcast`; `broadcast` collects dead connections and removes them after iteration
### Database Patterns
## Dashboard Frontend (`backend/dashboard.html`)
### File Organization
### Naming Patterns
- camelCase: `connectWS()`, `loadVisits()`, `renderVisitsTable()`, `drawBarChart()`, `drawHeatmap()`, `showToast()`, `addLog()`, `fetchJSON()`, `patchTag()`, `fmtDate()`, `timeAgo()`
- camelCase: `visits`, `statsData`, `activeFilter`, `barSet`, `heatSet`, `uptimeBase`
- Module-level constants: ALL_CAPS: `API`, `WS`, `SAME_ORIGIN`
- Kebab-case custom properties with `--` prefix: `--bg`, `--surface`, `--accent`, `--text-dim`, `--sidebar-w`, `--radius`
### JS Style
- `const`/`let` used consistently; `var` not used
- `async/await` throughout for all fetch and WebSocket operations
- `fetchJSON()` utility wraps all GET calls with auth headers
- Template literals used for dynamic HTML generation
### Section Dividers (JS)
## Comments
- Explain hardware constraints: `// GPIO 0 = camera XCLK — it CANNOT be read as a button`
- Explain non-obvious timing decisions: `// Discard first frame — first frame is sometimes dark`
- Warn about future work: `// tighten this in production`
- Document workarounds: `// setInsecure() skips certificate verification — acceptable for a local test sketch`
- Obvious operations are not commented
- No `@param`/`@return` JSDoc in JS; no Python docstrings on route handlers (Pydantic types serve as documentation)
- Python helper functions have single-line `"""docstrings"""` only when purpose is non-obvious
## Module Design
- No barrel files or re-exports; each Python module is imported directly
- `telegram_bot` imported as `tg` alias: `import telegram_bot as tg`
- Face recognition module (`recognition`) imported lazily inside functions when `RECOGNITION_ENABLED` is `True`, so the 600 MB model doesn't load unless explicitly opted in
- No shared state between backend modules except through the database
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

## System Overview
```text
```
## Component Responsibilities
| Component | Responsibility | File |
|-----------|----------------|------|
| ESP32 Firmware | Button detection, camera capture, HTTPS upload, heartbeat | `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` |
| FastAPI Server | REST API, WebSocket manager, visit persistence, quiet hours | `backend/server.py` |
| Telegram Bot Helper | Send photo notifications, handle reply callbacks, manage webhook | `backend/telegram_bot.py` |
| Face Recognition | Extract embeddings, match/create visitor records (off by default) | `backend/recognition.py` |
| Dashboard SPA | Live visit display, statistics, diagnostics, settings UI | `backend/dashboard.html` |
| Supabase PostgreSQL | Persistent storage for visits, device_status, settings, visitors | External (Supabase, Tokyo region) |
| Supabase Storage | Durable photo hosting (public `photos` bucket) | External (Supabase) |
## Pattern Overview
- Hardware event (button press) is the sole trigger for the primary data flow
- Backend is the single notification authority — ESP32 never posts directly to Telegram when `BACKEND_URL` is set
- Dashboard receives all updates via WebSocket push; HTTP polling is a 30-second fallback only
- Recognition and visitors features are fully implemented but gated behind an env flag (`RECOGNITION_ENABLED=1`)
- Quiet hours are enforced server-side, not on the ESP32
## Layers
- Purpose: Detect button press, capture JPEG photo, deliver to backend or Telegram
- Location: `ESP32/doorbell_step5_complete_v1/`
- Contains: Arduino `.ino` sketch, `secrets.h` (gitignored), `secrets.h.example`
- Depends on: WiFi, `esp_camera.h`, `WiFiClientSecure` (TLS, insecure mode — no cert pinning)
- Used by: Nothing (hardware push-only, no inbound connections)
- Purpose: Receive visits, store records, push to Telegram, broadcast to dashboards, expose REST endpoints
- Location: `backend/server.py`
- Contains: FastAPI app, WebSocket `ConnectionManager`, DB helpers, route handlers, quiet-hours logic
- Depends on: Supabase PostgreSQL (`DATABASE_URL`), Supabase Storage (`SUPABASE_URL`, `SUPABASE_SERVICE_KEY`), `telegram_bot.py`, optionally `recognition.py`
- Used by: ESP32 (POST /api/visits, POST /api/device/heartbeat), dashboard (all /api/* + /ws), Telegram (POST /telegram/webhook)
- Purpose: Async HTTP calls to Telegram Bot API
- Location: `backend/telegram_bot.py`
- Contains: `send_visit_notification`, `answer_callback`, `remove_buttons`, `set_webhook`, `get_webhook_info`
- Depends on: `BOT_TOKEN`, `CHAT_ID`, `PUBLIC_BASE_URL` env vars; `httpx` async client
- Used by: `backend/server.py` (called from `create_visit` and `telegram_webhook` route handlers)
- Purpose: Extract 512-float face embeddings, match against known visitors via cosine similarity (pgvector)
- Location: `backend/recognition.py`
- Contains: InsightFace ONNX model wrapper (lazy-loaded, thread-safe), `extract_embedding`, `match_or_create_visitor`, `health`
- Depends on: `insightface`, `onnxruntime`, `opencv-python-headless`, pgvector extension on Supabase
- Used by: `backend/server.py` inside `create_visit` when `RECOGNITION_ENABLED=1`
- Purpose: Browser UI for viewing visits, tagging, statistics, diagnostics, settings
- Location: `backend/dashboard.html`
- Contains: Single-file HTML/CSS/JS SPA; no build step, no framework
- Depends on: Backend REST API (`/api/*`) and WebSocket (`/ws`)
- Used by: Homeowner via any browser
## Data Flow
### Primary Request Path (Button Press → Dashboard)
### Homeowner Reply Path (Telegram → Dashboard)
### Heartbeat Path
### Demo Mode Path (TRIGGER_ON_BOOT)
- All persistent state lives in Supabase PostgreSQL (visits, device_status, settings, visitors, visitor_embeddings tables)
- Dashboard in-memory state: `visits[]` array, `statsData`, `DASH_KEY` (localStorage), WebSocket connection
- Backend in-memory state: `manager.active` list of WebSocket connections, `SERVER_START_TIME`, `_model` singleton in `recognition.py`
## Key Abstractions
- Purpose: Track all open dashboard WebSocket connections; broadcast JSON events
- Location: `backend/server.py:118-144`
- Pattern: Singleton `manager` instance; dead connections cleaned up on broadcast failure
- Purpose: Encapsulates the complete button-press-to-notification flow on firmware
- Location: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:169`
- Pattern: Sequential steps with error fallbacks; returns to polling loop regardless of outcome
- Purpose: Raw HTTP/1.1 multipart POST over `WiFiClientSecure` (no Arduino HTTPClient library)
- Location: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:365`
- Pattern: Manual header construction, chunked write (1024-byte chunks), checks for HTTP 201 in status line
- Purpose: Convert psycopg2 `RealDictCursor` row to JSON-safe dict; single canonical shape
- Location: `backend/server.py:219-231`
- Pattern: Called on every route that returns visit data; adds `visitor_id` / `visitor_name` when recognition is active
## Entry Points
- Location: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`
- Triggers: Arduino runtime calls `setup()` once on boot, then `loop()` indefinitely
- Responsibilities: WiFi connect, camera init, button poll, heartbeat timer
- Location: `backend/server.py`
- Triggers: `uvicorn server:app --host 0.0.0.0 --port $PORT` (via `backend/Procfile`)
- Responsibilities: `lifespan()` context manager calls `init_db()` on startup, optionally warms face model in background thread
- Location: `backend/dashboard.html` (served by `GET /` in `backend/server.py:236-238`)
- Triggers: Browser navigation to the Railway URL
- Responsibilities: Immediately checks stored password via `/api/auth/check`; if valid, calls `startApp()` which opens WebSocket and loads visits
## Architectural Constraints
- **Threading:** FastAPI runs on uvicorn async event loop (single-threaded async); face model loading uses `threading.Thread` + `asyncio.to_thread` to avoid blocking the loop; WebSocket `ConnectionManager` is not thread-safe beyond the event loop
- **Global state:** `manager` (WebSocket connections), `SERVER_START_TIME` in `backend/server.py`; `_model`, `_lock`, `_load_seconds` in `backend/recognition.py`; `BOT_TOKEN`/`CHAT_ID` module-level constants in `backend/telegram_bot.py`
- **Circular imports:** None detected
- **TLS / cert pinning:** ESP32 uses `client.setInsecure()` on all `WiFiClientSecure` connections — no certificate validation
- **Connection-per-query DB:** `get_db()` opens a new psycopg2 connection on every call; no connection pool — acceptable under demo load, would not scale
- **Railway ephemeral disk:** Photos fall back to local `backend/photos/` directory when Supabase Storage env vars are absent, but Railway's disk is ephemeral; always configure `SUPABASE_URL` + `SUPABASE_SERVICE_KEY` in production
## Anti-Patterns
### Raw HTTPS in Firmware Without Certificate Pinning
### Connection-Per-Query Database Access
### Single-File Dashboard with Inline Logic
## Error Handling
- Telegram send failure in `create_visit` is caught; visit is already saved before notification attempt (`backend/server.py:452-464`)
- Face recognition failure is caught with `try/except`; `visitor_id` stays `None` and visit is created regardless (`backend/server.py:422-438`)
- Photo upload to Supabase Storage falls back to local disk if upload fails (`backend/server.py:410-415`)
- ESP32 `triggerDoorbell()` falls back to direct Telegram photo send if `BACKEND_URL` is empty; falls back to text notification if camera fails
- WebSocket dead connections are pruned during `broadcast()` by catching send exceptions (`backend/server.py:131-138`)
## Cross-Cutting Concerns
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->
## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
