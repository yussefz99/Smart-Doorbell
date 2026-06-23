# External Integrations

**Analysis Date:** 2026-06-23

## APIs & External Services

**Telegram Bot API:**
- Service: Telegram Bot API (`https://api.telegram.org`)
- What it's used for: Sending photo notifications to the homeowner on every doorbell press; delivering inline reply buttons ("On my way" / "Not home"); editing messages after homeowner reply; registering and querying the webhook
- SDK/Client: Raw `httpx.AsyncClient` (no official Python SDK) in `backend/telegram_bot.py`
- Auth: `BOT_TOKEN` env var (prefixed into URL path: `/bot{BOT_TOKEN}/...`)
- Methods used: `sendPhoto`, `sendMessage`, `answerCallbackQuery`, `editMessageCaption`, `setWebhook`, `getWebhookInfo`, `deleteWebhook`
- Direct ESP32 path (fallback): firmware (`doorbell_step5_complete_v1.ino`) connects raw TLS via `WiFiClientSecure` to `api.telegram.org:443` and posts `sendPhoto` or `sendMessage` multipart manually when `BACKEND_URL` is empty

**Railway (Hosting Platform):**
- What it's used for: Production hosting of the FastAPI backend; auto-deploy on push to `main`
- Public URL: `https://smart-doorbell-production.up.railway.app`
- Configuration: `backend/Procfile` (start command), `backend/railpack.json` (apt package list for OpenCV/InsightFace system deps)
- Env vars injected via Railway Variables dashboard (no config file committed)

## Data Storage

**Databases:**
- Type/Provider: PostgreSQL on Supabase (`aws-1-ap-northeast-1` region, Tokyo)
- Connection: `DATABASE_URL` env var (transaction pooler connection string, port 6543)
- Client: `psycopg2-binary==2.9.10` with `RealDictCursor` — raw SQL, no ORM
- Connection pattern: new connection per query via `get_db()` in `backend/server.py` (lines 58–59); no connection pooling beyond Supabase's transaction pooler
- Schema (auto-created on startup via `init_db()` in `backend/server.py`):
  - `visits` — doorbell events (trigger, timestamp, photo_url, response, tag, silent, telegram_message_id, visitor_id)
  - `device_status` — single row with wifi_signal + last_sync (heartbeat data)
  - `settings` — single row with quiet_hours config and timezone
  - `visitors` — face recognition: recognized visitor records (name, photo_url, visit counts) — created only if pgvector extension is enabled
  - `visitor_embeddings` — 512-float face embeddings stored as `vector` type (pgvector); cosine similarity queries via `<=>` operator
- RLS: enabled on Supabase project

**File Storage:**
- Provider: Supabase Storage (public bucket named `photos`)
- What it's used for: Durable JPEG photo storage; Railway's disk is ephemeral so photos must be offloaded
- Auth: `SUPABASE_SERVICE_KEY` env var (`sb_secret_...` format) passed as `apikey` header (not Bearer — Supabase rejects Bearer for `sb_secret_` keys)
- Client: raw `httpx.AsyncClient` REST calls to `{SUPABASE_URL}/storage/v1/object/{bucket}/{filename}` in `backend/server.py` (lines 396–408)
- Fallback: if Supabase URL/key not set, photos saved to local `backend/photos/` directory and served as static files via `StaticFiles` mount (ephemeral on Railway)
- Photo URL format (durable): `{SUPABASE_URL}/storage/v1/object/public/photos/{filename}`
- Delete: `DELETE /storage/v1/object/photos/{filename}` called on visit deletion (best-effort, `backend/server.py` lines 306–318)

**Caching:**
- None

## Authentication & Identity

**Dashboard Auth:**
- Custom: password gate using `DASHBOARD_PASSWORD` env var
- Implementation: all dashboard read endpoints use `Depends(require_dashboard_key)` which checks the `X-Dashboard-Key` request header (`backend/server.py` lines 51–53)
- WebSocket auth: password passed as `?key=` query param (browsers cannot set headers on WebSocket upgrades), checked in `backend/server.py` lines 577–579
- Client-side: password stored in `localStorage` under `dash_key` key (`backend/dashboard.html` line 780)
- Empty `DASHBOARD_PASSWORD` = gate disabled (local development mode)

**Device Auth:**
- None — ESP32 device endpoints (`POST /api/visits`, `POST /api/device/heartbeat`) are open (no auth required). Device identity is assumed from network access.

**Face Recognition (optional):**
- No external auth provider — InsightFace model runs locally in the backend process
- Model files downloaded from InsightFace CDN on first load (`buffalo_s` or `buffalo_l` model pack)

## Monitoring & Observability

**Error Tracking:**
- None (no Sentry, Rollbar, or similar service)

**Logs:**
- Python `print()` statements throughout `backend/server.py`, `backend/telegram_bot.py`, and `backend/recognition.py` — visible in Railway's log viewer
- Prefixes: `[Telegram]`, `[Storage]`, `[Recognition]`, `[Heartbeat]`, `[Backend]`, `[Settings]`

## CI/CD & Deployment

**Hosting:**
- Railway PaaS — service configured with Root Directory `/backend`; deploys automatically on every push to the `main` branch of GitHub repo `yussefz99/Smart-Doorbell`

**CI Pipeline:**
- None — no automated tests run in CI; deploys on push without test gate

**Firmware Deployment:**
- Manual: Arduino IDE used to compile and upload via USB (COM3, 115200 baud)
- Pre-compiled binary present at `ESP32/compiled_program.bin` for direct flashing

## Environment Configuration

**Required env vars (production):**
- `DATABASE_URL` — Supabase PostgreSQL connection string (transaction pooler, port 6543)
- `BOT_TOKEN` — Telegram Bot token (from @BotFather)
- `CHAT_ID` — Telegram numeric chat ID of the homeowner

**Optional env vars:**
- `SUPABASE_URL` — Supabase project base URL (without trailing slash)
- `SUPABASE_SERVICE_KEY` — Supabase service key for Storage API
- `DASHBOARD_PASSWORD` — dashboard protection password (empty = disabled)
- `RECOGNITION_ENABLED` — set `1` to load face recognition model (default: `0`)
- `FACE_MODEL` — `buffalo_s` (default) or `buffalo_l`
- `PUBLIC_BASE_URL` — override for the public server URL used in Telegram photo links

**Secrets location:**
- Production: Railway Variables dashboard (never committed to git)
- Local backend: `backend/.env` (gitignored via `.gitignore` line 2)
- Firmware: `ESP32/doorbell_step5_complete_v1/secrets.h` (gitignored via `.gitignore` line 3; template: `ESP32/doorbell_step5_complete_v1/secrets.h.example`)

**Security note:** An old `BOT_TOKEN` was committed in git history (commit `e212e4f`); it was revoked on 2026-06-10. New token is in `.env` and Railway Variables.

## Webhooks & Callbacks

**Incoming:**
- `POST /telegram/webhook` — Telegram calls this endpoint when the homeowner taps an inline reply button (callback query). Parses `callback_data` in format `reply:<visit_id>:<response_text>`, saves response to DB, edits the Telegram message to remove buttons, and pushes `visit_updated` event to all connected WebSocket dashboard clients. Implemented in `backend/server.py` lines 594–651.
- Registration: `POST /admin/set-webhook` endpoint calls Telegram's `setWebhook` API with the Railway public URL. Webhook is permanently registered to Railway URL (no ngrok needed).

**Outgoing:**
- Telegram Bot API: `sendPhoto` / `sendMessage` — on every new visit via `backend/telegram_bot.py:send_visit_notification()`
- Telegram Bot API: `answerCallbackQuery` — acknowledge button tap
- Telegram Bot API: `editMessageCaption` — replace inline buttons with confirmation text after homeowner reply
- Supabase Storage REST API: `POST /storage/v1/object/photos/{filename}` — photo upload on each visit
- Supabase Storage REST API: `DELETE /storage/v1/object/photos/{filename}` — photo cleanup on visit deletion

## WebSocket

**Internal Push (Dashboard Live Updates):**
- Endpoint: `GET /ws` (WebSocket upgrade)
- Implementation: `ConnectionManager` class in `backend/server.py` (lines 118–145); in-memory list of active connections, no external broker
- Events broadcast: `new_visit`, `visit_updated`, `visit_deleted`, `presence`
- Auth: `?key=` query param checked against `DASHBOARD_PASSWORD`
- Dashboard connects at: `wss://{host}/ws?key={DASH_KEY}` (or `ws://` for local HTTP)

---

*Integration audit: 2026-06-23*
