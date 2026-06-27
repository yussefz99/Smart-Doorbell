---
phase: 02-security-reliability-hardening
plan: 01
requirements: [SEC-01, SEC-02, SEC-06]
status: complete
---

# Plan 02-01: Device Auth — Summary

## What was built
Added `require_device_key`, a FastAPI dependency mirroring `require_dashboard_key`'s
empty-disables-gate semantics, and attached it to the three device-facing endpoints.

## Changes
- `backend/server.py`:
  - New module constant `DEVICE_SECRET = os.getenv("DEVICE_SECRET", "").strip()`.
  - New `require_device_key(x_device_key)` — raises `HTTPException(401, "Invalid device key")`
    only when `DEVICE_SECRET` is set AND the header mismatches; empty secret = no-op.
  - `dependencies=[Depends(require_device_key)]` added to exactly three routes:
    `POST /api/visits`, `POST /api/device/heartbeat`, `GET /api/visits/{id}/response`.
  - Updated the stale "No dashboard key" comment above `get_visit_response`.
- `backend/.env` (gitignored): documented `DEVICE_SECRET=` (empty by default).

## Verification (live, against the shared Supabase)
Booted `uvicorn server:app` via the project venv:
- **Gate ON** (`DEVICE_SECRET=testkey123`): heartbeat → 401 (no key), 401 (wrong key), 200 (correct key).
- **Gate OFF** (`DEVICE_SECRET` empty): heartbeat → 200 with no key — endpoints stay open.
- Source: exactly 3 routes carry the dependency; webhook and `/ws` untouched; `server.py` parses.

Tested via the heartbeat endpoint only (idempotent device_status write) to avoid writing
test visit rows to the production DB. The visits/response routes share the identical
dependency, so the 401/gate-off behavior is equivalent.

## Requirements
- SEC-01 (visits auth), SEC-02 (heartbeat auth), SEC-06 (response-poll auth) — met.
- Gate-off-when-empty proven (protects the teammate's live device).

## Commits
- `e9dfbbf` feat(02-01): add require_device_key dependency + DEVICE_SECRET
- `85a0b96` feat(02-01): require X-Device-Key on visits, heartbeat, response poll

## Self-Check: PASSED
- No firmware, dashboard, or webhook code touched.
- No new imports (Header/Depends already imported).
