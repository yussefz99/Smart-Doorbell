# Coding Conventions

**Analysis Date:** 2026-06-23

---

This project spans three distinct components with separate conventions: **ESP32 firmware** (C/C++ Arduino sketches), a **Python backend** (FastAPI), and a **vanilla-JS dashboard** (single HTML file). Where conventions differ per component they are documented separately.

---

## ESP32 Firmware (Arduino C/C++)

### Files

- Sketches use `.ino` extension: `doorbell_step5_complete_v1.ino`
- Each sketch lives in a same-named subdirectory: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`
- Secret header lives alongside the sketch: `secrets.h` (gitignored), with a committed template `secrets.h.example`
- Shared constants (non-secret) live in a top-level header: `ESP32/parameters.h`

### Naming Patterns

**Macros / `#define` constants:**
- ALL_CAPS with underscores: `PIN_BUZZER`, `DEBOUNCE_MS`, `BEEP_SHORT_MS`, `HEARTBEAT_MS`
- Secret macros prefixed `SECRET_`: `SECRET_WIFI_SSID`, `SECRET_BOT_TOKEN`

**Global variables:**
- camelCase: `lastDebounceTime`, `lastButtonState`, `lastPressTime`, `lastHeartbeat`
- Type annotations aligned at declaration: `unsigned long lastDebounceTime = 0;` / `int lastButtonState = HIGH;`

**Functions:**
- camelCase verbs: `connectWiFi()`, `initCamera()`, `capturePhoto()`, `sendHeartbeat()`, `triggerDoorbell()`
- State-setting helpers prefixed with the action: `setIdle()`, `setCapturing()`, `setSending()`, `setFlash(bool on)`
- LED helpers: `blinkLED(pin, times, onMs, offMs)`, `allLEDsOff()`
- Boolean return for operations that can fail: `bool connectWiFi()`, `bool initCamera()`, `bool sendPhotoToTelegram()`

### File Headers

Every `.ino` file opens with a banner comment block:

```cpp
// ============================================================
//  Smart Doorbell – <Sketch Name>
//  Board  : <board>
//  <key info lines>
//  Flow   : <step description>
// ============================================================
```

### Section Dividers

Two styles are used consistently:
- Subsection header (thin): `// ── Section name ─────────────────────`
- Major section header (double-line): `// ════════════════════════... / //  Section Name / // ════════════════...`

### Serial Logging

Prefixed bracket tags on every log line:
- `[OK]` — success
- `[ERROR]` — failure
- `[WiFi]`, `[Camera]`, `[Telegram]`, `[Backend]`, `[Heartbeat]`, `[BUTTON]`, `[DEMO]`, `[INFO]` — subsystem prefix

```cpp
Serial.println("[OK] WiFi connected: " + WiFi.localIP().toString());
Serial.println("[ERROR] Camera init failed.");
Serial.println("[Backend] Posting visit to: " + host);
```

### Error Handling

- Functions that can fail return `bool` (true = success, false = failure)
- Callers check return value immediately: `if (!connectWiFi()) { ... return; }`
- On error in `setup()`: print `[ERROR]` message, blink flash/LED N times, then `return` to halt further init
- Camera frame buffer always returned: `esp_camera_fb_return(fb)` called after use
- Timeouts use `millis()` arithmetic with explicit timeout constants

### Forward Declarations

All non-trivial sketches declare functions at the top of the file before `setup()`:

```cpp
// ── Forward declarations ──────────────────────────────────
bool connectWiFi();
bool initCamera();
camera_fb_t* capturePhoto();
```

### Secrets Management

- Real credentials live in `secrets.h` (gitignored)
- Template provided as `secrets.h.example` (committed)
- Credentials are only assigned after `#include "secrets.h"`:
  ```cpp
  #include "secrets.h"
  const char* WIFI_SSID = SECRET_WIFI_SSID;
  ```
- Test sketches under `Unit Tests/` use plain-text placeholder strings (`"YOUR_WIFI_SSID"`) — they are reference copies, not production code

### Demo / Feature Flags

Toggle-able compile-time flags using `#define` booleans, controlled via `#if`:

```cpp
#define TRIGGER_ON_BOOT  true
// ...
#if TRIGGER_ON_BOOT
  triggerDoorbell();
#endif
```

---

## Python Backend (`backend/`)

### Files

- `server.py` — FastAPI application, all routes and database helpers
- `telegram_bot.py` — Telegram API async helper functions
- `recognition.py` — lazy-loaded InsightFace wrapper
- `dashboard.html` — single-file frontend served by the backend
- `requirements.txt` — pinned dependencies

### Naming Patterns

**Functions:**
- `snake_case`: `get_db()`, `db_fetchall()`, `db_fetchone()`, `db_execute()`, `init_db()`, `in_quiet_hours()`, `row_to_visit()`, `require_dashboard_key()`

**Pydantic models (classes):**
- `PascalCase`: `TagUpdate`, `DeviceHeartbeat`, `SettingsUpdate`, `VisitorRename`, `ConnectionManager`

**Route handlers:**
- `snake_case` verbs: `get_visits()`, `create_visit()`, `delete_visit()`, `update_tag()`, `get_settings()`, `update_settings()`, `device_heartbeat()`, `get_device_status()`

**Module-level constants:**
- ALL_CAPS: `DATABASE_URL`, `SUPABASE_URL`, `SUPABASE_SERVICE_KEY`, `STORAGE_BUCKET`, `DASHBOARD_PASSWORD`, `RECOGNITION_ENABLED`, `TELEGRAM_API`, `PUBLIC_BASE_URL`, `SERVER_START_TIME`

**Local variables:**
- `snake_case`: `photo_url`, `visit_id`, `callback_id`, `visitor_id`

### File Header

All Python files open with a banner comment block:

```python
# ============================================================
#  Smart Doorbell – <Module Name>
#  <description>
# ============================================================
```

### Section Dividers

```python
# ── Section name ─────────────────────────────────────────────
```

### Import Organization

1. Standard library imports (`os`, `time`, `datetime`, `threading`, etc.)
2. Third-party imports (`psycopg2`, `fastapi`, `pydantic`, `dotenv`, `httpx`)
3. Local module imports (`import telegram_bot as tg`, `import recognition`)
4. `load_dotenv()` called immediately after dotenv import

Lazy imports (inside functions) used for optional heavy dependencies:
```python
import asyncio, recognition   # inside async def, only when RECOGNITION_ENABLED
```

### Error Handling

- FastAPI routes raise `HTTPException` with explicit status codes and messages:
  ```python
  raise HTTPException(400, "trigger must be 'button' or 'motion'")
  raise HTTPException(404, "Visit not found")
  raise HTTPException(401, "Invalid dashboard password")
  ```
- Non-fatal failures (Telegram, face recognition, Storage) caught with broad `except Exception as e` and `print(f"[subsystem] failed: {e!r}")` — never allowed to break the visit creation response
- DB helpers (`db_fetchone`, `db_execute`) use context managers for connections

### Logging

- `print()` with bracketed subsystem prefix (no logging framework):
  ```python
  print(f"[Recognition] visitor={visitor_id} name={visitor_name}")
  print(f"[Storage] Supabase upload failed ({r.status_code}): {r.text[:200]}")
  print("[Telegram] WARNING: BOT_TOKEN is empty — notifications disabled")
  ```

### Env Var Hygiene

String env vars stripped of stray whitespace and quotes on read:

```python
BOT_TOKEN = os.getenv("BOT_TOKEN", "").strip().strip('"').strip("'")
SUPABASE_URL = os.getenv("SUPABASE_URL", "").strip().rstrip("/")
```

### Async Patterns

- Route functions that call external services (`httpx`) are `async def`
- CPU-bound work (face recognition) offloaded with `asyncio.to_thread()`
- WebSocket manager uses `async def` for `connect` and `broadcast`; `broadcast` collects dead connections and removes them after iteration

### Database Patterns

Three thin wrappers around psycopg2 used everywhere — no ORM:

```python
def db_fetchall(sql, params=None): ...   # SELECT multiple rows
def db_fetchone(sql, params=None): ...   # SELECT/INSERT RETURNING one row
def db_execute(sql, params=None):  ...   # INSERT/UPDATE/DELETE, returns rowcount
```

All SQL uses `%s` parameterized queries (never f-string interpolation into SQL).

---

## Dashboard Frontend (`backend/dashboard.html`)

### File Organization

Single `dashboard.html` file containing HTML, CSS (in `<style>`), and JavaScript (in `<script>`). Not split into separate files.

### Naming Patterns

**Functions:**
- camelCase: `connectWS()`, `loadVisits()`, `renderVisitsTable()`, `drawBarChart()`, `drawHeatmap()`, `showToast()`, `addLog()`, `fetchJSON()`, `patchTag()`, `fmtDate()`, `timeAgo()`

**Variables:**
- camelCase: `visits`, `statsData`, `activeFilter`, `barSet`, `heatSet`, `uptimeBase`
- Module-level constants: ALL_CAPS: `API`, `WS`, `SAME_ORIGIN`

**CSS variables:**
- Kebab-case custom properties with `--` prefix: `--bg`, `--surface`, `--accent`, `--text-dim`, `--sidebar-w`, `--radius`

### JS Style

- `const`/`let` used consistently; `var` not used
- `async/await` throughout for all fetch and WebSocket operations
- `fetchJSON()` utility wraps all GET calls with auth headers
- Template literals used for dynamic HTML generation

### Section Dividers (JS)

```js
// ── Section name ─────────────────────────────────────────────
```

---

## Comments

**When to comment:**
- Explain hardware constraints: `// GPIO 0 = camera XCLK — it CANNOT be read as a button`
- Explain non-obvious timing decisions: `// Discard first frame — first frame is sometimes dark`
- Warn about future work: `// tighten this in production`
- Document workarounds: `// setInsecure() skips certificate verification — acceptable for a local test sketch`

**What not to comment:**
- Obvious operations are not commented
- No `@param`/`@return` JSDoc in JS; no Python docstrings on route handlers (Pydantic types serve as documentation)
- Python helper functions have single-line `"""docstrings"""` only when purpose is non-obvious

---

## Module Design

- No barrel files or re-exports; each Python module is imported directly
- `telegram_bot` imported as `tg` alias: `import telegram_bot as tg`
- Face recognition module (`recognition`) imported lazily inside functions when `RECOGNITION_ENABLED` is `True`, so the 600 MB model doesn't load unless explicitly opted in
- No shared state between backend modules except through the database

---

*Convention analysis: 2026-06-23*
