# Phase 2: Security + Reliability Hardening - Context

**Gathered:** 2026-06-27
**Status:** Ready for planning
**Source:** Conversation decisions (Rami, backend owner)

<domain>
## Phase Boundary

This phase hardens the **backend** (`backend/server.py`, `backend/telegram_bot.py`) so every
device endpoint and the Telegram webhook are authenticated, the callback parser cannot crash,
and a fresh database bootstraps every table the app queries.

**Ownership split (see `.planning/WORK-SPLIT.md`):**
- **Rami (this execution): backend only** — SEC-01, SEC-02, SEC-04, SEC-05, SEC-06, REL-01.
- **Yussef (teammate, already done / his bench):** SEC-03 (firmware sends `X-Device-Key`,
  committed in `770a43e`) and DEMO-01 (live GPIO-13 round-trip on his hardware).

**Out of scope for backend execution here:** flashing or running the ESP32 (it stays on
Yussef's setup), dashboard frontend changes (Phase 1, Yussef-owned), face recognition logic
(Phase 3 — REL-01 only creates the tables, it does not enable recognition).
</domain>

<decisions>
## Implementation Decisions

### Device authentication (SEC-01, SEC-02, SEC-06)
- Add a `require_device_key` dependency that mirrors the existing `require_dashboard_key`
  pattern (`backend/server.py:51-53`): read the `X-Device-Key` request header and compare it
  to a new `DEVICE_SECRET` env var.
- **Gate-off-when-empty (CRITICAL):** when `DEVICE_SECRET` is empty/unset, the dependency is a
  no-op and the endpoint stays open — exactly like `DASHBOARD_PASSWORD` behaves today. This is
  the safety property that lets the backend deploy WITHOUT breaking Yussef's live device: auth
  only "turns on" once `DEVICE_SECRET` is set on Railway AND the same value is in Yussef's
  firmware `secrets.h`. The plan must NOT hard-require the secret.
- On mismatch when the secret IS set: raise `HTTPException(401, ...)` (matches the existing
  dashboard-key 401 convention; 401 satisfies the "401 or 403" success criteria).
- Attach the dependency to exactly three routes:
  - `POST /api/visits` → `create_visit` (`server.py:366`)
  - `POST /api/device/heartbeat` → `device_heartbeat` (`server.py:575`)
  - `GET /api/visits/{id}/response` → `get_visit_response` (`server.py:476`, currently
    explicitly unauthenticated). The firmware already sends `X-Device-Key` on this poll
    (`doorbell_step5_btn_io14.ino:338`).

### Telegram webhook auth (SEC-04)
- Validate the `X-Telegram-Bot-Api-Secret-Token` header in `telegram_webhook`
  (`server.py:607`) against a new `TELEGRAM_WEBHOOK_SECRET` env var; reject with 403 when the
  secret is set and the header is missing/wrong.
- **Gate-off-when-empty:** when `TELEGRAM_WEBHOOK_SECRET` is unset, skip the check — otherwise
  Telegram callbacks would die the instant the backend deploys, before `setWebhook` is
  re-registered with the token.
- Extend `tg.set_webhook` (`telegram_bot.py:144`) to pass `secret_token=TELEGRAM_WEBHOOK_SECRET`
  to the Telegram `setWebhook` API so Telegram sends the header on every callback. Re-running
  `POST /admin/set-webhook` after deploy registers it.

### Callback parser hardening (SEC-05)
- The parser at `server.py:627-629` does `parts = data.split(":", 2); int(parts[1]); parts[2]`
  — this `IndexError`s on `reply:` alone and `ValueError`s on a non-numeric id, producing a 500
  that Telegram retries in a loop.
- Bounds-check: verify there are 3 parts and `parts[1]` is a valid int BEFORE using them; on
  malformed input answer the callback cleanly and return `{"ok": true}` (HTTP 200) so Telegram
  does not retry. No raised exception escapes the handler.

### Database bootstrap (REL-01)
- Extend `init_db()` (`server.py:80`) to create the two recognition tables the app already
  queries (`/api/visitors` at `server.py:329` selects from `visitors`; visits carry a
  `visitor_id`) so a fresh database does not 500:
  - `CREATE EXTENSION IF NOT EXISTS vector;` (pgvector — DECIDED: init_db auto-creates it; do
    not rely on a manual Supabase dashboard step).
  - `CREATE TABLE IF NOT EXISTS visitors (...)` with columns matching the existing queries:
    `id`, `name`, `photo_url`, `first_seen`, `last_seen`, `visit_count`.
  - `CREATE TABLE IF NOT EXISTS visitor_embeddings (...)` with a pgvector `embedding` column.
  - Add a `visitor_id` column to `visits` (FK to `visitors`, nullable, `IF NOT EXISTS` via
    `ALTER TABLE ... ADD COLUMN IF NOT EXISTS`).
- All statements idempotent (`IF NOT EXISTS`) so existing prod data is untouched and a
  dropped-table fresh start re-bootstraps cleanly.

### Config / secrets (operator actions, not code)
- New env vars: `DEVICE_SECRET`, `TELEGRAM_WEBHOOK_SECRET`. Both default to empty (gate off).
- Local test values already chosen: `DEVICE_SECRET=34229b9b17baa8cd1b6b783a2c05ae91`,
  `TELEGRAM_WEBHOOK_SECRET=9880d89e188692c95383662e8185b009`. Rami sets these on Railway and
  shares `DEVICE_SECRET` with Yussef for `secrets.h`.
- Document both in `backend/.env` (template) so local runs can exercise the auth path.

### Verification approach
- Backend verification runs against a locally-booted `uvicorn server:app` pointed at the shared
  Supabase (`DATABASE_URL`). Auth-positive/negative cases tested with/without the header.
- SEC-03 and DEMO-01 are verified by Yussef on hardware — not re-implemented here. The plan
  should record them as teammate-owned coverage so phase requirement coverage stays complete.

### Claude's Discretion
- Exact column types/constraints for `visitors` / `visitor_embeddings` (follow Supabase +
  pgvector + `recognition.py` expectations; embedding dimension per the InsightFace model).
- Whether `require_device_key` and the webhook check share a small helper.
- Error message wording.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Backend (the files being changed)
- `backend/server.py` — all routes, `init_db`, `require_dashboard_key` pattern to mirror
- `backend/telegram_bot.py` — `set_webhook` (extend with `secret_token`)
- `backend/recognition.py` — column/embedding expectations for the new tables (REL-01)

### Project + planning
- `CLAUDE.md` — stack frozen (FastAPI + Supabase + Railway), lab-prototype security bar
- `.planning/REQUIREMENTS.md` — SEC-01..06, REL-01, DEMO-01 exact text
- `.planning/ROADMAP.md` — Phase 2 success criteria (7 items)
- `.planning/WORK-SPLIT.md` — Rami/Yussef ownership + handshakes

### Firmware (read-only context — already implemented by Yussef)
- `ESP32/doorbell_step5_btn_io14/doorbell_step5_btn_io14.ino` — sends `X-Device-Key` on visit
  (`:562`), heartbeat (`:647`), response poll (`:338`); `TRIGGER_ON_BOOT false` (`:71`)
- `ESP32/doorbell_step5_btn_io14/secrets.h.example` — `SECRET_DEVICE_KEY` handshake value
</canonical_refs>

<specifics>
## Specific Ideas

- Mirror `require_dashboard_key` (`server.py:51-53`) exactly for `require_device_key` — same
  empty-disables-gate semantics, same 401 raise, attach via `dependencies=[Depends(...)]`.
- Webhook secret: Telegram's `secret_token` allows only `A-Z a-z 0-9 _ -`, 1–256 chars — the
  generated hex value satisfies this.
- Keep the visit-creation path resilient: auth runs before the body is processed; recognition
  stays gated behind `RECOGNITION_ENABLED` (unchanged).
</specifics>

<deferred>
## Deferred Ideas

- Cert pinning / connection pooling — explicitly deferred per CLAUDE.md.
- Rate limiting, per-device key rotation — beyond the lab-prototype bar.
- Enabling face recognition — Phase 3 (REL-01 only provisions the tables here).
- SEC-03 firmware + DEMO-01 hardware round-trip — Yussef-owned, his bench.
</deferred>

---

*Phase: 02-security-reliability-hardening*
*Context gathered: 2026-06-27 from conversation decisions*
