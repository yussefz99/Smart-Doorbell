---
phase: 02-security-reliability-hardening
verified: 2026-06-27T00:00:00Z
status: human_needed
score: 6/7 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Physical GPIO 13 button press end-to-end round-trip"
    expected: "Press → photo captured → Telegram notification with photo → homeowner replies → dashboard updates → OLED shows reply. TRIGGER_ON_BOOT is false in doorbell_step5_btn_io14."
    why_human: "Requires physical ESP32-CAM hardware, flashed firmware (commit 770a43e), live Telegram bot, and Railway + Supabase connectivity. Cannot be verified programmatically. Teammate-owned (Yussef). Backend contract is verified; hardware execution is not."
---

# Phase 2: Security + Reliability Hardening — Verification Report

**Phase Goal:** All three device endpoints (visit upload, heartbeat, reply poll) require a shared secret (X-Device-Key gated by DEVICE_SECRET), the Telegram webhook is authenticated (X-Telegram-Bot-Api-Secret-Token), the callback parser cannot crash on malformed input, and the database bootstraps every table the app queries — and (teammate-owned) the physical GPIO-13 button on the OLED demo build triggers a verified end-to-end round-trip.
**Verified:** 2026-06-27
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | POST /api/visits without correct X-Device-Key returns 401 when DEVICE_SECRET is set; with correct key returns 201 | VERIFIED | `@app.post("/api/visits", status_code=201, dependencies=[Depends(require_device_key)])` at server.py:407. `require_device_key` raises `HTTPException(401)` only when `DEVICE_SECRET` is truthy and header mismatches (server.py:67-69). |
| 2 | POST /api/device/heartbeat without correct X-Device-Key returns 401 when DEVICE_SECRET is set; with correct key returns 200 | VERIFIED | `@app.post("/api/device/heartbeat", dependencies=[Depends(require_device_key)])` at server.py:617. Same dependency as truth 1. |
| 3 | GET /api/visits/{id}/response without correct X-Device-Key returns 401 when DEVICE_SECRET is set; with correct key returns reply text | VERIFIED | `@app.get("/api/visits/{visit_id}/response", dependencies=[Depends(require_device_key)])` at server.py:518. |
| 4 | When DEVICE_SECRET is empty/unset all three endpoints stay open (no 401) | VERIFIED | `require_device_key` guard: `if DEVICE_SECRET and x_device_key != DEVICE_SECRET` (server.py:68). When DEVICE_SECRET is empty string, the condition is falsy; function returns None with no raise. Mirrors `require_dashboard_key` semantics exactly. |
| 5 | POST /telegram/webhook with wrong/missing X-Telegram-Bot-Api-Secret-Token returns 403 when TELEGRAM_WEBHOOK_SECRET is set; correct token or empty secret processes callbacks normally | VERIFIED | server.py:658-661: `if TELEGRAM_WEBHOOK_SECRET and request.headers.get("X-Telegram-Bot-Api-Secret-Token", "") != TELEGRAM_WEBHOOK_SECRET: raise HTTPException(403, ...)`. Guard is before body parse. Empty secret bypasses check. `set_webhook` in telegram_bot.py:156-157 conditionally includes `secret_token` in setWebhook payload. |
| 6 | Malformed reply callback_data (reply:, reply:abc:Hi, reply:42) returns HTTP 200 with no exception | VERIFIED | server.py:704-708: `parts = data.split(":", 2); if len(parts) < 3 or not parts[1].isdigit(): ... return {"ok": True}`. `reply:` splits to 1 part (caught by `< 3`). `reply:42` splits to 2 parts (caught by `< 3`). `reply:abc:Hi` splits to 3 parts but `"abc".isdigit()` is False (caught). All three return clean 200. |
| 7 | init_db() creates pgvector + visitors + visitor_embeddings (vector(512)) + visits.visitor_id, all idempotent; fresh-DB endpoints no longer 500 | VERIFIED | server.py:137-155: `CREATE EXTENSION IF NOT EXISTS vector`, `CREATE TABLE IF NOT EXISTS visitors (id, name, photo_url, first_seen, last_seen, visit_count)`, `CREATE TABLE IF NOT EXISTS visitor_embeddings (... embedding vector(512))`, `ALTER TABLE visits ADD COLUMN IF NOT EXISTS visitor_id`. All guarded with IF NOT EXISTS. Columns match `/api/visitors` SELECT and `recognition.py` inserts. |
| — | SC-6 (ROADMAP): Physical GPIO 13 button press end-to-end round-trip (DEMO-01) | HUMAN NEEDED | Teammate-owned hardware task (Yussef, commit 770a43e adds X-Device-Key to firmware). Backend side is verified (authenticated endpoints accept X-Device-Key requests). Physical press-to-OLED loop cannot be confirmed without hardware. |

**Score:** 7/7 backend truths verified (SC-6 / DEMO-01 routes to human verification as intended by project plan)

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `backend/server.py` | `require_device_key` + `Depends()` on 3 routes; `TELEGRAM_WEBHOOK_SECRET` check; bounds-checked callback parser; `init_db()` with 4 new SQL statements | VERIFIED | All present. Exactly 3 routes carry `Depends(require_device_key)` (lines 407, 518, 617). Webhook check at lines 658-661. Parser bounds-check at lines 704-708. init_db SQL at lines 137-155. |
| `backend/telegram_bot.py` | `set_webhook` passes `secret_token` when set; `WEBHOOK_SECRET` constant | VERIFIED | `WEBHOOK_SECRET = os.getenv("TELEGRAM_WEBHOOK_SECRET", "").strip()...` at line 20. Conditional `payload["secret_token"] = WEBHOOK_SECRET` at lines 156-157. |
| `backend/.env` | `DEVICE_SECRET=` and `TELEGRAM_WEBHOOK_SECRET=` documented | VERIFIED (LOCAL) | Documented in SUMMARY files; gitignored file — content confirmed by SUMMARY.md which states both keys added with empty default. |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `create_visit` (POST /api/visits) | `require_device_key` | `dependencies=[Depends(require_device_key)]` | WIRED | server.py:407 |
| `device_heartbeat` (POST /api/device/heartbeat) | `require_device_key` | `dependencies=[Depends(require_device_key)]` | WIRED | server.py:617 |
| `get_visit_response` (GET /api/visits/{id}/response) | `require_device_key` | `dependencies=[Depends(require_device_key)]` | WIRED | server.py:518 |
| `require_device_key` | `DEVICE_SECRET` env var | `os.getenv("DEVICE_SECRET", "").strip()` + empty-disables-gate | WIRED | server.py:50, 68 |
| `telegram_webhook` | `TELEGRAM_WEBHOOK_SECRET` env var | `request.headers.get("X-Telegram-Bot-Api-Secret-Token", "")` comparison | WIRED | server.py:56, 658-661 |
| `set_webhook` | Telegram setWebhook `secret_token` field | `if WEBHOOK_SECRET: payload["secret_token"] = WEBHOOK_SECRET` | WIRED | telegram_bot.py:156-157 |
| `init_db()` | `visitors` / `visitor_embeddings` tables | `CREATE TABLE IF NOT EXISTS` | WIRED | server.py:139-152 |
| `init_db()` | pgvector `vector(512)` embedding column | `CREATE EXTENSION IF NOT EXISTS vector` | WIRED | server.py:137, 151 |

---

### Data-Flow Trace (Level 4)

Not applicable — this phase implements security middleware and schema bootstrap, not data-rendering components. All wired artifacts are guards/DDL, not dynamic-data renderers.

---

### Behavioral Spot-Checks

| Behavior | Evidence | Status |
|----------|----------|--------|
| `require_device_key` empty-disables-gate logic | `if DEVICE_SECRET and x_device_key != DEVICE_SECRET` — falsy DEVICE_SECRET skips raise | PASS |
| `require_device_key` raises 401 on mismatch | `raise HTTPException(401, "Invalid device key")` inside the guard | PASS |
| Webhook 403 gate | `if TELEGRAM_WEBHOOK_SECRET and ... != TELEGRAM_WEBHOOK_SECRET: raise HTTPException(403, ...)` | PASS |
| `reply:` (1 part) parser safety | `"reply:".split(":", 2)` → `["reply:", ""]` length 2 → caught by `len(parts) < 3` | PASS |
| `reply:42` (2 parts) parser safety | `"reply:42".split(":", 2)` → 2 parts → caught by `len(parts) < 3` | PASS (confirmed via python3 check) |
| `reply:abc:Hi` (non-numeric id) parser safety | `len(parts) == 3` but `"abc".isdigit() == False` → caught | PASS |
| init_db idempotency guards | Every statement uses `IF NOT EXISTS` or `ADD COLUMN IF NOT EXISTS` | PASS |
| Webhook/WebSocket NOT carrying device key guard | `@app.post("/telegram/webhook")` (line 648) and `@app.websocket("/ws")` (line 629) have no `require_device_key` | PASS |

---

### Probe Execution

No probes declared in PLAN files. Step 7c: SKIPPED (no probe-*.sh files found for this phase).

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| SEC-01 | 02-01 | POST /api/visits requires X-Device-Key | SATISFIED | server.py:407 + `require_device_key` definition |
| SEC-02 | 02-01 | POST /api/device/heartbeat requires X-Device-Key | SATISFIED | server.py:617 |
| SEC-03 | 02-03 (teammate-owned) | Firmware sends X-Device-Key; demo round-trip works | NEEDS HUMAN | Backend contract verified (require_device_key reads X-Device-Key header, matches WORK-SPLIT.md handshake #1). Firmware commit 770a43e. Hardware execution by Yussef. |
| SEC-04 | 02-02 | Telegram webhook validates X-Telegram-Bot-Api-Secret-Token | SATISFIED | server.py:658-661; telegram_bot.py:156-157 |
| SEC-05 | 02-02 | Callback parser bounds-checks reply: data, no 500 retry | SATISFIED | server.py:704-708 |
| SEC-06 | 02-01 | GET /api/visits/{id}/response requires X-Device-Key | SATISFIED | server.py:518 |
| REL-01 | 02-03 | init_db() creates visitors + visitor_embeddings; fresh DB no 500 | SATISFIED | server.py:137-155 |
| DEMO-01 | 02-03 (teammate-owned) | TRIGGER_ON_BOOT false; GPIO 13 triggers full round-trip | NEEDS HUMAN | Teammate-owned hardware task. Backend endpoints accept firmware requests (verified). Physical button press not run here. |

No orphaned requirements: all 8 Phase 2 requirements accounted for across the 3 plans.

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None | — | No TBD/FIXME/XXX markers found in server.py or telegram_bot.py | — | — |

No debt markers, no placeholder implementations, no empty handlers, no hardcoded stubs in the security-critical paths.

---

### Human Verification Required

#### 1. Full End-to-End GPIO 13 Round-Trip (DEMO-01 / SEC-03)

**Test:** On Yussef's bench: ensure `doorbell_step5_btn_io14` firmware (commit 770a43e) is flashed with `TRIGGER_ON_BOOT false` and `SECRET_DEVICE_KEY` matching Railway's `DEVICE_SECRET`. Press the physical GPIO 13 button once.

**Expected:** Camera captures photo → backend receives POST /api/visits with X-Device-Key header and returns 201 → Telegram notification arrives with photo and reply buttons → homeowner taps a button or sends a typed reply → dashboard updates live (visit row shows response) → OLED display on the ESP32 shows the reply text.

**Why human:** Requires physical ESP32-CAM hardware, soldered button, flashed firmware, live Telegram bot token, Railway backend reachable, and Supabase connected. Cannot be verified programmatically. WORK-SPLIT.md assigns this to Yussef (firmware + hardware owner).

---

### Gaps Summary

No backend gaps identified. All backend must-haves from the 3 PLAN files are implemented and wired.

The single human-needed item (DEMO-01 / SC-6) is the physical hardware round-trip explicitly documented as teammate-owned in WORK-SPLIT.md and 02-CONTEXT.md. The backend side of the contract — authenticated endpoints accepting X-Device-Key requests — is fully implemented and verified. The remaining confirmation is hardware execution on Yussef's bench.

**Commits verified in git history:**
- `e9dfbbf` — require_device_key + DEVICE_SECRET
- `85a0b96` — Depends(require_device_key) on 3 routes
- `838febd` — webhook auth + callback parser hardening
- `b36cdb8` — init_db pgvector + visitors/visitor_embeddings

---

_Verified: 2026-06-27_
_Verifier: Claude (gsd-verifier)_
