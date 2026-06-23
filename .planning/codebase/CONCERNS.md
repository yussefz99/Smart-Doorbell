# Codebase Concerns

**Analysis Date:** 2026-06-23

---

## Tech Debt

**Demo mode flag left enabled in production firmware:**
- Issue: `TRIGGER_ON_BOOT` is `#define TRIGGER_ON_BOOT true` at line 42 of the active sketch. Every power-on or RESET fires a real doorbell event, uploads a photo to the backend, and sends a Telegram message.
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` line 42
- Impact: Any accidental power cycle during deployment or demo setup will trigger a false alert. Must be set to `false` after soldering the physical button — documented but not yet done.
- Fix approach: After soldering GPIO 13 button, change to `#define TRIGGER_ON_BOOT false` and re-upload.

**Hardcoded Railway URL in firmware:**
- Issue: `BACKEND_URL` is hardcoded as a string literal at line 27 of the production sketch. If the Railway deployment URL ever changes (e.g., project rename, migration, re-deploy under new service), firmware requires recompile and reflash to reconnect.
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` line 27
- Impact: Low for now (Railway URL is stable), but would silently fail — heartbeats stop, visits stop uploading, firmware falls back to direct Telegram-only mode with `reply:0:` as the visit ID in callback data (callback response loop broken).
- Fix approach: Move `BACKEND_URL` into `secrets.h` alongside other credentials.

**Older step sketches have plaintext credential placeholders committed to git:**
- Issue: `ESP32/doorbell_step2_wifi_telegram/doorbell_step2_wifi_telegram.ino` lines 15–18, `ESP32/doorbell_step4_photo_telegram/doorbell_step4_photo_telegram.ino` lines 13–16, and `ESP32/doorbell_step2_wifi_telegram_test/doorbell_step2_wifi_telegram_test.ino` lines 11–14 all contain `"YOUR_WIFI_SSID"`, `"YOUR_BOT_TOKEN_HERE"` etc. as inline literals in committed `.ino` files. These sketches do not use the `secrets.h` pattern.
- Files: `ESP32/doorbell_step2_wifi_telegram/doorbell_step2_wifi_telegram.ino`, `ESP32/doorbell_step4_photo_telegram/doorbell_step4_photo_telegram.ino`, `ESP32/doorbell_step2_wifi_telegram_test/doorbell_step2_wifi_telegram_test.ino`
- Impact: If anyone copies a legacy sketch and pastes real credentials, they're at risk of committing secrets. Pattern mismatch across the repo creates confusion about how credentials are managed.
- Fix approach: Add `#include "secrets.h"` to all step sketches consistently, matching the pattern in the production sketch.

**`ESP32/SECRETS.h` is a stale Google Speech API template, not project-relevant:**
- Issue: `ESP32/SECRETS.h` at the repo root of the ESP32 folder is a Google Speech API credential template (contains `speech.googleapis.com`, `ApiKey`, `root_ca`). It is unrelated to the doorbell project and was not gitignored.
- Files: `ESP32/SECRETS.h`
- Impact: Confusing to anyone reading the repo; the file name matches the gitignored `secrets.h` exactly but differs by case (filesystem matters on Linux/Railway). On a case-sensitive OS this could be accidentally committed if real values are added.
- Fix approach: Delete `ESP32/SECRETS.h` — it is a leftover template from a different project.

**Synchronous psycopg2 database calls block the FastAPI async event loop:**
- Issue: All DB helpers (`db_fetchone`, `db_fetchall`, `db_execute`) are synchronous blocking functions using `psycopg2.connect()` on every call (no connection pool). They are called directly inside async FastAPI endpoint handlers (e.g., `create_visit`, `delete_visit`, `get_visits`, `get_stats`, `telegram_webhook`) without `asyncio.to_thread()`.
- Files: `backend/server.py` lines 58–79 (DB helpers), lines 262–268, 297–321, 380, 440–465, 603–650 (async endpoints calling sync functions)
- Impact: Under concurrent load (multiple simultaneous visits, WebSocket connections, dashboard polling), each DB call blocks the event loop thread, queuing all other requests. This is unlikely to matter at low traffic but is an architectural smell.
- Fix approach: Either wrap blocking DB calls in `await asyncio.to_thread(db_fetchone, ...)` or migrate to an async driver (`asyncpg` or `databases` with SQLAlchemy async).

**No connection pool — a new Supabase connection is opened per query:**
- Issue: `get_db()` calls `psycopg2.connect(DATABASE_URL)` on every invocation. The `create_visit` endpoint alone opens 3–4 separate connections per doorbell press.
- Files: `backend/server.py` line 58–59
- Impact: Under Supabase's transaction pooler (port 6543) this is handled, but connection overhead is unnecessary and may hit limits under face recognition workload.
- Fix approach: Use `psycopg2.pool.ThreadedConnectionPool` or switch to `asyncpg`.

**Face recognition `visitors` / `visitor_embeddings` tables are not created by `init_db()`:**
- Issue: `init_db()` creates `visits`, `device_status`, and `settings` tables but NOT `visitors` or `visitor_embeddings`. The Visitors API (`GET/PATCH/DELETE /api/visitors`) and `get_visits` JOIN query (`SELECT visits.*, visitors.name`) will raise a psycopg2 `UndefinedTable` error if recognition is enabled and the tables were never manually created in Supabase.
- Files: `backend/server.py` lines 80–115 (`init_db`), lines 262–268 (`get_visits` JOIN), lines 329–362 (Visitors API)
- Impact: Turning on `RECOGNITION_ENABLED=1` without manually running the Supabase SQL for `visitors`/`visitor_embeddings`/`pgvector` will crash visit creation silently (caught by the broad `except Exception`) but Visitors API endpoints will raise 500 errors.
- Fix approach: Add `CREATE TABLE IF NOT EXISTS visitors` and `visitor_embeddings` (with `CREATE EXTENSION IF NOT EXISTS vector`) to `init_db()`, with `IF NOT EXISTS` guards.

**`requirements.txt` pins face recognition deps with `>=` (unpinned upper bound):**
- Issue: `insightface>=1.0`, `onnxruntime>=1.20`, `opencv-python-headless>=4.11`, `psutil>=6` use minimum-only version constraints. Any breaking release of these packages will be pulled in automatically on next Railway deploy.
- Files: `backend/requirements.txt` lines 9–12
- Impact: Recognition module could silently break on Railway redeploy with no code change.
- Fix approach: Pin all recognition deps to exact versions (e.g., `insightface==0.7.3`) once validated.

**Settings page in dashboard previously fully UI-only; timezone is hardcoded:**
- Issue: The `timezone` field in the settings table defaults to `'Asia/Jerusalem'` (line 109, `init_db`). There is no UI control or API field to change the timezone — `SettingsUpdate` model (lines 184–188) has no `timezone` field, so `update_settings` never writes it.
- Files: `backend/server.py` lines 184–188 (`SettingsUpdate`), line 109 (`init_db`), line 289 (`update_settings` SQL does not update timezone column)
- Impact: Quiet hours will work correctly only for Israel timezone. Deploying for users in any other timezone requires manually updating the Supabase row.
- Fix approach: Add `timezone: str` to `SettingsUpdate`, add timezone validation (check against `zoneinfo.available_timezones()`), update the SQL in `update_settings`, and add a timezone selector to the dashboard settings page.

---

## Known Bugs

**Telegram `editMessageCaption` fails silently for text-only messages:**
- Symptoms: When the camera fails and a text-only Telegram message was sent (no photo), tapping a reply button will call `remove_buttons()` which uses `editMessageCaption`. This Telegram API call requires a photo message — it returns an error for text messages, which is currently ignored (no error check in `remove_buttons()`).
- Files: `backend/telegram_bot.py` lines 128–140 (`remove_buttons`), `backend/server.py` lines 636–642
- Trigger: Camera capture failure → `sendTextNotification` fallback used → homeowner taps reply button.
- Workaround: None currently. The response IS saved to the DB and pushed via WebSocket; only the Telegram message UI is not updated.

**Telegram webhook callback parser has no bounds check on split result:**
- Symptoms: If Telegram delivers a `callback_data` string starting with `"reply:"` but with fewer than 3 colon-separated parts, `parts[1]` or `parts[2]` will raise an `IndexError`, returning HTTP 500 to Telegram. Telegram will then retry the callback every few seconds.
- Files: `backend/server.py` lines 615–617
- Trigger: Malformed callback data (edge case, but Telegram retries can cause repeated 500s).
- Workaround: None. Add `if len(parts) < 3: ...` guard before accessing `parts[1]` and `parts[2]`.

**`sendTextNotification` uses URL-encoded GET with only space → `%20` substitution:**
- Symptoms: Messages containing characters other than spaces (e.g., `?`, `&`, `=`, `+`, `#`) will corrupt the GET URL. The `⚠️` emoji in the camera-failure message may also cause encoding issues on some network stacks.
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` lines 342–360 (`sendTextNotification`)
- Trigger: Any camera error that triggers the fallback text path.
- Workaround: Switch to POST `/sendMessage` with JSON body as used by the backend's Telegram module.

**`millis()` rollover at ~49.7 days not handled in WiFi connection loop:**
- Symptoms: `connectWiFi()` uses `if (millis() - start > 15000)` where `start` is captured at connection start. If `millis()` wraps (rolls over from ~4.29B to 0) between `start` capture and the check, the subtraction underflows (unsigned arithmetic) producing a very large number, instantly timing out the WiFi connection on the rollover tick. Device requires RESET.
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` lines 219–222
- Trigger: Device left powered on for ~49.7 days — low probability for demo context, but a real concern for production deployment.
- Workaround: Arduino's `unsigned long` subtraction is actually rollover-safe for `millis() - start` comparisons specifically (wraps correctly), so this is lower risk than it appears. However `timeout` patterns on lines 327–329 and 415–417 use the same pattern and are fine too. No action required unless device must run >49 days.

---

## Security Considerations

**`POST /api/visits` and `POST /api/device/heartbeat` are completely unauthenticated:**
- Risk: Any internet-visible attacker can flood the backend with fake visits (including fake photos), consuming Supabase Storage quota, triggering unlimited Telegram notifications, and filling the visits database. Heartbeat endpoint can be spammed to fake device-online status.
- Files: `backend/server.py` line 366 (`create_visit` — no `Depends(require_dashboard_key)`), line 563 (`device_heartbeat` — no auth)
- Current mitigation: None. Intentionally left open so the ESP32 (which has no secret header) can post.
- Recommendations: Add a shared secret (e.g., `X-Device-Key` header matching a `DEVICE_SECRET` env var) checked in `create_visit` and `device_heartbeat`. The ESP32 sends this header; the secret is stored in `secrets.h` + Railway Variables. This prevents unauthenticated abuse from the internet.

**`POST /telegram/webhook` is unauthenticated — no Telegram secret token:**
- Risk: Any internet party can POST to `/telegram/webhook` with crafted JSON, causing arbitrary `response` values to be written to the database for any visit ID, and triggering WebSocket broadcasts to all dashboard clients.
- Files: `backend/server.py` lines 594–651 (`telegram_webhook`)
- Current mitigation: Attacker must know the visit ID and craft valid JSON — not trivial but not a high bar.
- Recommendations: Register the webhook with a `secret_token` via Telegram's `setWebhook` API parameter. FastAPI then validates the `X-Telegram-Bot-Api-Secret-Token` header on every incoming request.

**Dashboard password stored in `localStorage` in plaintext:**
- Risk: The `DASHBOARD_PASSWORD` value is stored as-is in `localStorage` under the key `dash_key` (line 810 of `dashboard.html`). Any XSS vulnerability, browser extension, or physical access to browser DevTools exposes the password. Additionally, the password is transmitted as `?key=<password>` in the WebSocket URL (line 924), which appears in server access logs.
- Files: `backend/dashboard.html` lines 780, 810, 924
- Current mitigation: Dashboard is served over HTTPS (Railway). The password is a low-security gate, not protecting PII beyond visit history and photos.
- Recommendations: Use `sessionStorage` instead of `localStorage` to avoid persistence across browser sessions. Pass the WebSocket key via a short-lived token exchanged during auth rather than the raw password in the URL.

**TLS certificate verification disabled on all ESP32 HTTPS connections:**
- Risk: `client.setInsecure()` is used in every `WiFiClientSecure` connection in the production firmware — to Telegram API (lines 281, 344) and to the Railway backend (lines 367, 430). This disables certificate validation, making all HTTPS connections from the ESP32 vulnerable to man-in-the-middle attacks on the local network (router compromise, ARP spoofing).
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` lines 281, 344, 367, 430
- Current mitigation: Acceptable for a university lab prototype but a real risk in home deployment.
- Recommendations: Pin the Telegram and Railway root CA certificate using `client.setCACert()`. Update `secrets.h` to include the CA PEM.

**CORS policy allows all origins:**
- Risk: `allow_origins=["*"]` (line 169 of `server.py`) means any website can make credentialed cross-origin requests to the API. Because the `X-Dashboard-Key` header is required for protected endpoints, the real attack surface is limited — but `POST /api/visits` and `POST /api/device/heartbeat` (both unauthenticated) can be called from any origin.
- Files: `backend/server.py` line 169
- Current mitigation: Comment notes "tighten this in production."
- Recommendations: Restrict `allow_origins` to the Railway deployment URL (`https://smart-doorbell-production.up.railway.app`). Keep `allow_origins=["*"]` only for local dev via an env var flag.

**Old bot token exists in git history at commit `e212e4f`:**
- Risk: The original Telegram bot token was committed in the initial commit (`e212e4f`, file `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`). The token was revoked on 2026-06-10 (confirmed in `SESSION_HANDOFF.md` line 133), so the exposed value is now dead. However, the git history is public (GitHub: `yussefz99/Smart-Doorbell`) and the commit remains.
- Files: git commit `e212e4f` — `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` (historical)
- Current mitigation: Token revoked. The ESP32 flash may still carry the old token as the direct-Telegram fallback path credential (SESSION_HANDOFF notes this as harmless since the token is dead).
- Recommendations: On next firmware upload, ensure `secrets.h` carries the current token (already done per handoff). The git history itself poses no active risk since the token is dead, but for hygiene consider a `git filter-repo` rewrite before final submission.

**`SUPABASE_SERVICE_KEY` used as the storage upload credential — it is a service-role key:**
- Risk: The Supabase service key (stored in `SUPABASE_SERVICE_KEY` env var) bypasses Row Level Security and has full database + storage access. It is used solely for photo upload/delete operations. If the Railway environment is compromised or the key is accidentally logged, an attacker has full Supabase access.
- Files: `backend/server.py` lines 37, 396–409 (upload), 310–316 (delete)
- Current mitigation: Key is in Railway Variables (not in git). The `photos` bucket is public-read so the service key is only needed for write/delete.
- Recommendations: Create a restricted Supabase storage policy that allows insert/delete using the anon key with a bucket policy, avoiding service-role key exposure in the application layer.

---

## Performance Bottlenecks

**Face recognition model (~600 MB) loaded into same Railway container as the API:**
- Problem: When `RECOGNITION_ENABLED=1`, the InsightFace model (`buffalo_s` or `buffalo_l`) is loaded on first use and held in memory. `buffalo_l` may OOM the Railway container and cause crash-loops.
- Files: `backend/recognition.py` lines 24–35 (`get_model`), `backend/server.py` lines 152–163 (warmup thread)
- Cause: Railway free/hobby tier containers have limited RAM (~512 MB for free tier). The model warmup thread runs at startup, competing with the API server for memory.
- Improvement path: Keep `RECOGNITION_ENABLED=0` by default (current). If enabling for demo, use `FACE_MODEL=buffalo_s` and watch Railway memory metrics before demo day. Long-term: move recognition to a separate service or use Supabase Edge Functions.

**`in_quiet_hours()` opens a new DB connection on every `POST /api/visits`:**
- Problem: `in_quiet_hours()` calls `db_fetchone()` which calls `psycopg2.connect()` — a new connection on each visit. Combined with the 3–4 other DB calls in `create_visit`, this means 4–5 connection round-trips per doorbell press.
- Files: `backend/server.py` lines 194–209 (`in_quiet_hours`), lines 379–382 (called in `create_visit`)
- Cause: No connection pool; no caching of settings.
- Improvement path: Cache settings in a module-level dict with a 60-second TTL, or pass an existing connection into `in_quiet_hours()`.

---

## Fragile Areas

**`triggerDoorbell()` is a ~35-second blocking function in the main loop:**
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` lines 169–210
- Why fragile: The function calls `capturePhoto()` (blocking, 100 ms + frame time), `postVisitToBackend()` (blocking HTTP with up to 20 s timeout), and `blinkFlash()` (blocking). During this entire time, button presses, heartbeat timing, and all other logic are blocked. A second button press during upload is silently dropped (the 3 s MIN_PRESS_GAP debounce is only checked at the top of `loop()`; `loop()` does not run during `triggerDoorbell()`).
- Safe modification: The `MIN_PRESS_GAP` (3000 ms, line 67) provides some protection, but network timeouts of up to 20 s mean the gap is actually enforced by the blocking time, not by design. Do not shorten network timeouts without understanding this interaction.
- Test coverage: No automated test; tested manually on hardware.

**WebSocket `ConnectionManager` uses an in-process list — does not survive a redeploy:**
- Files: `backend/server.py` lines 118–144 (`ConnectionManager`)
- Why fragile: `manager.active` is a plain Python list. Railway auto-deploys on every `git push main`. All WebSocket connections are silently killed on deploy; clients must reconnect. The dashboard reconnects automatically (WebSocket `onclose` triggers reconnect), but the presence count resets to 0 briefly.
- Safe modification: Acceptable for a prototype; do not assume state persists across deploys.

**`postVisitToBackend()` parses only the HTTP status line — response body ignored:**
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` lines 418–422
- Why fragile: Success is determined by `statusLine.indexOf("201") > 0`. If the Railway server returns `HTTP/1.1 201 Created` but the actual visit INSERT failed (e.g., DB connectivity), the ESP32 reports success but no visit is saved. Similarly, a `4XX` response body with an error message is never read or logged.
- Safe modification: No immediate fix needed for prototype; for production, read and log the full response body.

**`remove_buttons()` response in `telegram_bot.py` is never checked:**
- Files: `backend/telegram_bot.py` lines 128–140 (`remove_buttons`)
- Why fragile: The `httpx` response from `editMessageCaption` is discarded. Failures (wrong message type, expired message, token error) are silent. If Telegram rejects the edit, the homeowner sees buttons they cannot retap (they already fired), creating UI confusion.
- Safe modification: Read `resp.json()` and log `data` when `data.get("ok")` is False.

---

## Scaling Limits

**Railway trial credit ($5 / 30 days):**
- Current capacity: $5 credit, started sometime in June 2026.
- Limit: When credit exhausts, the Railway service goes offline. All visits, Telegram notifications, and dashboard access stop.
- Scaling path: Upgrade to Railway hobby plan ($5/month) before demo day, or migrate to a free-tier alternative (Render, Fly.io). Per `SESSION_HANDOFF.md` line 137, this is flagged as "Watch."

**Supabase free tier PostgreSQL row limits:**
- Current capacity: Supabase free tier allows up to 500 MB database storage and 1 GB file storage.
- Limit: VGA JPEG photos (~15–30 KB each) in Supabase Storage will accumulate. At 30 KB/photo, 1 GB holds ~33,000 photos — not a near-term concern for a demo project.
- Scaling path: Implement photo pruning (delete visits older than N days) or compress to lower JPEG quality.

---

## Dependencies at Risk

**`insightface` has no pinned version — `>=1.0` pulls from PyPI:**
- Risk: `insightface` 1.x releases are infrequent and the package has a history of breaking API changes on minor versions.
- Impact: Recognition module could break silently on Railway redeploy.
- Migration plan: Pin to `insightface==0.7.3` (last verified working version per project testing).

---

## Missing Critical Features

**No offline buffering on ESP32:**
- Problem: If WiFi drops mid-press or Railway is temporarily unavailable, the visit is lost entirely. The ESP32 logs the error and continues — no retry, no local storage.
- Blocks: Reliable doorbell operation in environments with intermittent connectivity.
- Note: Documented in `PLAN.md` Version 4 as a planned feature. SPIFFS buffering is the intended approach.

**No ESP32 auth on device endpoints:**
- Problem: `POST /api/visits` and `POST /api/device/heartbeat` have no authentication. Any device on the internet can inject visits.
- Blocks: Safe public deployment of the Railway backend.
- Note: Described in detail in Security Considerations above.

**Settings timezone selector missing from dashboard:**
- Problem: Timezone is hardcoded to `'Asia/Jerusalem'` in the DB default and never exposed for editing.
- Blocks: Correct quiet-hours behavior for any user outside Israel.

---

## Test Coverage Gaps

**No integration tests for the full visit flow:**
- What's not tested: `POST /api/visits` → Supabase INSERT → Telegram notification → WebSocket broadcast chain.
- Files: `backend/server.py` (entire `create_visit` function, lines 367–469)
- Risk: A regression in photo upload, DB insert, or Telegram notification could go undetected until hardware test.
- Priority: High

**No firmware unit tests beyond sketch-level manual verification:**
- What's not tested: `connectWiFi()`, `postVisitToBackend()`, `sendHeartbeat()`, debounce logic.
- Files: `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino`
- Risk: Changes to timing constants or network logic cannot be regression-tested without flashing hardware. The `Unit Tests/` folder contains copies of step sketches, not automated test harnesses.
- Priority: Medium (acceptable for a university lab project)

**Telegram webhook endpoint has no test coverage:**
- What's not tested: Callback parsing at `POST /telegram/webhook` (malformed data, missing fields, unknown callback types).
- Files: `backend/server.py` lines 594–651
- Risk: Malformed Telegram callbacks trigger unhandled exceptions (IndexError on `parts[1]`/`parts[2]`) that Telegram retries indefinitely.
- Priority: High

**Recognition module has no test against actual doorbell photos:**
- What's not tested: `buffalo_s` accuracy on the specific QVGA/VGA photos the CS-CAM carrier produces. Session notes confirm `buffalo_s` returned similarity −0.01 on test photos (effectively no match).
- Files: `backend/recognition.py`
- Risk: Enabling `RECOGNITION_ENABLED=1` on demo day may produce "New visitor" for every press regardless of the person at the door.
- Priority: Medium

---

*Concerns audit: 2026-06-23*
