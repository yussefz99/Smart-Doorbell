<!-- refreshed: 2026-06-23 -->
# Architecture

**Analysis Date:** 2026-06-23

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────┐
│                    HARDWARE LAYER (ESP32-CAM)                        │
│   `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`  │
│   Button press (GPIO 13) → Camera capture → WiFi HTTPS POST         │
└───────────────────────────┬─────────────────────────────────────────┘
                            │  HTTPS POST /api/visits  (multipart)
                            │  HTTPS POST /api/device/heartbeat (JSON)
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│              BACKEND LAYER (FastAPI on Railway)                      │
│   `backend/server.py`                                                │
│                                                                      │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐ │
│   │ REST API     │  │ WebSocket    │  │ Telegram Webhook          │ │
│   │ /api/*       │  │ /ws          │  │ /telegram/webhook         │ │
│   └──────┬───────┘  └──────┬───────┘  └────────────┬─────────────┘ │
│          │                 │                         │               │
│   ┌──────▼─────────────────▼─────────────────────┐  │               │
│   │     ConnectionManager (WebSocket broadcast)  │  │               │
│   └─────────────────────────────────────────────┘  │               │
│                                                      │               │
│   `backend/telegram_bot.py`  ◄───────────────────────┘               │
│   `backend/recognition.py`  (optional, RECOGNITION_ENABLED=1)       │
└────────┬──────────────────────────────┬────────────────────────────┘
         │                              │
         ▼                              ▼
┌────────────────────┐    ┌────────────────────────────────────────┐
│  Supabase          │    │  Telegram Bot API                      │
│  · PostgreSQL DB   │    │  api.telegram.org                      │
│  · Storage bucket  │    │  sendPhoto / editMessageCaption        │
│    "photos"        │    │  answerCallbackQuery / setWebhook      │
└────────────────────┘    └────────────────────────────────────────┘
         ▲
         │  WebSocket /ws   (live push)
         │  REST /api/*     (HTTP fetch)
┌────────┴───────────────────────────────────────────────────────────┐
│                    DASHBOARD (Vanilla JS SPA)                       │
│   `backend/dashboard.html`   (served by FastAPI at GET /)           │
└────────────────────────────────────────────────────────────────────┘
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

**Overall:** Event-driven IoT pipeline with push notifications and real-time dashboard

**Key Characteristics:**
- Hardware event (button press) is the sole trigger for the primary data flow
- Backend is the single notification authority — ESP32 never posts directly to Telegram when `BACKEND_URL` is set
- Dashboard receives all updates via WebSocket push; HTTP polling is a 30-second fallback only
- Recognition and visitors features are fully implemented but gated behind an env flag (`RECOGNITION_ENABLED=1`)
- Quiet hours are enforced server-side, not on the ESP32

## Layers

**Firmware Layer:**
- Purpose: Detect button press, capture JPEG photo, deliver to backend or Telegram
- Location: `ESP32/doorbell_step5_complete_v1/`
- Contains: Arduino `.ino` sketch, `secrets.h` (gitignored), `secrets.h.example`
- Depends on: WiFi, `esp_camera.h`, `WiFiClientSecure` (TLS, insecure mode — no cert pinning)
- Used by: Nothing (hardware push-only, no inbound connections)

**Backend API Layer:**
- Purpose: Receive visits, store records, push to Telegram, broadcast to dashboards, expose REST endpoints
- Location: `backend/server.py`
- Contains: FastAPI app, WebSocket `ConnectionManager`, DB helpers, route handlers, quiet-hours logic
- Depends on: Supabase PostgreSQL (`DATABASE_URL`), Supabase Storage (`SUPABASE_URL`, `SUPABASE_SERVICE_KEY`), `telegram_bot.py`, optionally `recognition.py`
- Used by: ESP32 (POST /api/visits, POST /api/device/heartbeat), dashboard (all /api/* + /ws), Telegram (POST /telegram/webhook)

**Telegram Integration Layer:**
- Purpose: Async HTTP calls to Telegram Bot API
- Location: `backend/telegram_bot.py`
- Contains: `send_visit_notification`, `answer_callback`, `remove_buttons`, `set_webhook`, `get_webhook_info`
- Depends on: `BOT_TOKEN`, `CHAT_ID`, `PUBLIC_BASE_URL` env vars; `httpx` async client
- Used by: `backend/server.py` (called from `create_visit` and `telegram_webhook` route handlers)

**Face Recognition Layer (optional):**
- Purpose: Extract 512-float face embeddings, match against known visitors via cosine similarity (pgvector)
- Location: `backend/recognition.py`
- Contains: InsightFace ONNX model wrapper (lazy-loaded, thread-safe), `extract_embedding`, `match_or_create_visitor`, `health`
- Depends on: `insightface`, `onnxruntime`, `opencv-python-headless`, pgvector extension on Supabase
- Used by: `backend/server.py` inside `create_visit` when `RECOGNITION_ENABLED=1`

**Dashboard Layer:**
- Purpose: Browser UI for viewing visits, tagging, statistics, diagnostics, settings
- Location: `backend/dashboard.html`
- Contains: Single-file HTML/CSS/JS SPA; no build step, no framework
- Depends on: Backend REST API (`/api/*`) and WebSocket (`/ws`)
- Used by: Homeowner via any browser

## Data Flow

### Primary Request Path (Button Press → Dashboard)

1. Visitor presses tactile button wired to GPIO 13 on ESP32-CAM (`ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:149`)
2. `loop()` detects LOW on GPIO 13 with 50 ms debounce + 3 s min-gap guard (`doorbell_step5_complete_v1.ino:138-153`)
3. `triggerDoorbell()` activates flash LED, calls `capturePhoto()` — discards first frame, returns second JPEG (`doorbell_step5_complete_v1.ino:169-210`)
4. `postVisitToBackend()` sends multipart HTTPS POST to `https://smart-doorbell-production.up.railway.app/api/visits` with `trigger=button` and JPEG binary (`doorbell_step5_complete_v1.ino:365-423`)
5. `create_visit()` in `backend/server.py` receives upload, checks quiet hours, uploads JPEG to Supabase Storage, inserts visit row into PostgreSQL (`backend/server.py:366-469`)
6. If `RECOGNITION_ENABLED`, `recognition.extract_embedding()` runs in a thread; `match_or_create_visitor()` queries pgvector for nearest face (`backend/server.py:422-438`)
7. `tg.send_visit_notification(visit)` posts photo + inline reply buttons to Telegram via `api.telegram.org/sendPhoto` (`backend/telegram_bot.py:33-113`)
8. `manager.broadcast({"type": "new_visit", "data": visit})` pushes the visit JSON to every connected dashboard WebSocket (`backend/server.py:467`)
9. Dashboard JS receives `new_visit` event, prepends row to table, shows toast, plays ding sound (`backend/dashboard.html:930-936`)

### Homeowner Reply Path (Telegram → Dashboard)

1. Homeowner taps "On my way" or "Not home" inline button in Telegram
2. Telegram POSTs callback to `https://<railway>/telegram/webhook` (`backend/server.py:594`)
3. `telegram_webhook()` parses `callback_data` format `reply:<visit_id>:<response>`, saves response to DB (`backend/server.py:608-634`)
4. `tg.answer_callback()` removes spinner; `tg.remove_buttons()` edits Telegram message caption to show reply (`backend/server.py:633-642`)
5. `manager.broadcast({"type": "visit_updated", "data": visit})` updates the dashboard live (`backend/server.py:645-649`)

### Heartbeat Path

1. ESP32 `sendHeartbeat()` POSTs `{"wifi_signal": <RSSI>}` to `/api/device/heartbeat` every 60 seconds (`doorbell_step5_complete_v1.ino:426-457`)
2. `device_heartbeat()` updates `device_status` row with signal + timestamp (`backend/server.py:563-570`)
3. Dashboard `/api/device/status` computes `online = last_sync age < 150s` (`backend/server.py:548-559`)

### Demo Mode Path (TRIGGER_ON_BOOT)

1. `TRIGGER_ON_BOOT true` (current default while button is unsoldered) — `setup()` calls `triggerDoorbell()` once at boot (`doorbell_step5_complete_v1.ino:122-126`)
2. Pressing the carrier RESET button triggers the full flow without a wired button
3. Set `TRIGGER_ON_BOOT false` after soldering GPIO 13 button

**State Management:**
- All persistent state lives in Supabase PostgreSQL (visits, device_status, settings, visitors, visitor_embeddings tables)
- Dashboard in-memory state: `visits[]` array, `statsData`, `DASH_KEY` (localStorage), WebSocket connection
- Backend in-memory state: `manager.active` list of WebSocket connections, `SERVER_START_TIME`, `_model` singleton in `recognition.py`

## Key Abstractions

**`ConnectionManager` (WebSocket broadcast hub):**
- Purpose: Track all open dashboard WebSocket connections; broadcast JSON events
- Location: `backend/server.py:118-144`
- Pattern: Singleton `manager` instance; dead connections cleaned up on broadcast failure

**`triggerDoorbell()` (full event pipeline):**
- Purpose: Encapsulates the complete button-press-to-notification flow on firmware
- Location: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:169`
- Pattern: Sequential steps with error fallbacks; returns to polling loop regardless of outcome

**`postVisitToBackend()` (firmware HTTP client):**
- Purpose: Raw HTTP/1.1 multipart POST over `WiFiClientSecure` (no Arduino HTTPClient library)
- Location: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino:365`
- Pattern: Manual header construction, chunked write (1024-byte chunks), checks for HTTP 201 in status line

**`row_to_visit()` (DB row serializer):**
- Purpose: Convert psycopg2 `RealDictCursor` row to JSON-safe dict; single canonical shape
- Location: `backend/server.py:219-231`
- Pattern: Called on every route that returns visit data; adds `visitor_id` / `visitor_name` when recognition is active

## Entry Points

**ESP32 Firmware:**
- Location: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`
- Triggers: Arduino runtime calls `setup()` once on boot, then `loop()` indefinitely
- Responsibilities: WiFi connect, camera init, button poll, heartbeat timer

**Backend Server:**
- Location: `backend/server.py`
- Triggers: `uvicorn server:app --host 0.0.0.0 --port $PORT` (via `backend/Procfile`)
- Responsibilities: `lifespan()` context manager calls `init_db()` on startup, optionally warms face model in background thread

**Dashboard:**
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

**What happens:** All `WiFiClientSecure` calls use `setInsecure()`, disabling TLS certificate validation entirely.
**Why it's wrong:** A network attacker on the same LAN can MITM the connection and capture photos or inject fake visits.
**Do this instead:** Load the Railway/Supabase root CA cert and call `client.setCACert(root_ca)` before `connect()`. See `ESP32/SECRETS.h.example` pattern.

### Connection-Per-Query Database Access

**What happens:** `get_db()` in `backend/server.py:58-59` opens a new psycopg2 connection for every helper call (`db_fetchall`, `db_fetchone`, `db_execute`).
**Why it's wrong:** Each connection takes ~20-50 ms to establish; under concurrent load this adds latency and can exhaust Supabase's connection limit (transaction pooler port 6543 mitigates but does not eliminate this).
**Do this instead:** Use a connection pool (e.g. `psycopg2.pool.ThreadedConnectionPool` or switch to `asyncpg` with a pool).

### Single-File Dashboard with Inline Logic

**What happens:** All HTML, CSS, and ~600 lines of JS live in `backend/dashboard.html` with no build step.
**Why it's wrong:** As features grow, the file becomes hard to maintain and test; no module boundaries between data-fetching, rendering, and WebSocket layers.
**Do this instead:** Extract JS into separate files and load via `<script src="...">` modules, or move to a lightweight framework if the app grows.

## Error Handling

**Strategy:** Best-effort with graceful degradation; critical paths log and continue rather than crash.

**Patterns:**
- Telegram send failure in `create_visit` is caught; visit is already saved before notification attempt (`backend/server.py:452-464`)
- Face recognition failure is caught with `try/except`; `visitor_id` stays `None` and visit is created regardless (`backend/server.py:422-438`)
- Photo upload to Supabase Storage falls back to local disk if upload fails (`backend/server.py:410-415`)
- ESP32 `triggerDoorbell()` falls back to direct Telegram photo send if `BACKEND_URL` is empty; falls back to text notification if camera fails
- WebSocket dead connections are pruned during `broadcast()` by catching send exceptions (`backend/server.py:131-138`)

## Cross-Cutting Concerns

**Logging:** `print()` statements throughout `backend/server.py`, `telegram_bot.py`, and `recognition.py`; prefixed with `[Component]` (e.g. `[Telegram]`, `[Storage]`, `[Recognition]`). ESP32 uses `Serial.println()` at 115200 baud with `[Label]` prefixes.

**Validation:** FastAPI Pydantic models (`TagUpdate`, `DeviceHeartbeat`, `SettingsUpdate`, `VisitorRename`) validate request bodies. Trigger values validated inline (`backend/server.py:373`). Time format validated with regex `_TIME_RE` (`backend/server.py:192`).

**Authentication:** Dashboard endpoints protected by `X-Dashboard-Key` header check via `require_dashboard_key` FastAPI `Depends` (`backend/server.py:51-54`). WebSocket auth via `?key=` query param (`backend/server.py:577`). Device endpoints (`POST /api/visits`, `POST /api/device/heartbeat`) are intentionally open — no auth required from ESP32. `DASHBOARD_PASSWORD` empty = gate disabled (local dev).

---

*Architecture analysis: 2026-06-23*
