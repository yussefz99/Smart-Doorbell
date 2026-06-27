---
phase: 02-security-reliability-hardening
plan: 03
requirements: [REL-01, SEC-03, DEMO-01]
status: complete
---

# Plan 02-03: DB Bootstrap + Device Contract — Summary

## What was built
Extended `init_db()` so the database bootstraps every table the app already queries — closing
the fresh-DB 500 gaps and provisioning the Phase 3 recognition schema.

## Changes
- `backend/server.py` — `init_db()` now also runs (all idempotent, appended after the settings insert):
  - `CREATE EXTENSION IF NOT EXISTS vector;` (pgvector, no manual dashboard step)
  - `CREATE TABLE IF NOT EXISTS visitors (id, name, photo_url, first_seen, last_seen, visit_count)`
    — columns matching the `/api/visitors` SELECT and `recognition.py` inserts exactly
  - `CREATE TABLE IF NOT EXISTS visitor_embeddings (id, visitor_id FK, embedding vector(512))`
  - `ALTER TABLE visits ADD COLUMN IF NOT EXISTS visitor_id` (FK, nullable)
- Recognition stays OFF (`RECOGNITION_ENABLED` unchanged); this only provisions schema.

## Verification (live, against the production Supabase)
- Source asserts: all four statements + `vector(512)` present.
- `init_db()` run twice → no error (idempotent).
- `SELECT ... FROM visitors`, `SELECT visitor_id FROM visits`, `SELECT embedding FROM visitor_embeddings`
  all execute without error.
- `GET /api/visitors` → 200, `GET /api/visits` → 200 (were 500 candidates on a missing table).

## Requirements
- **REL-01** — met (fresh-DB bootstrap, idempotent, endpoints no longer 500).
- **SEC-03** — teammate-owned (Yussef, firmware commit `770a43e`): the firmware sends
  `X-Device-Key` on visit/heartbeat/response-poll. Backend contract confirmed: `require_device_key`
  reads the wire header `X-Device-Key` and compares to `DEVICE_SECRET` — matches handshake #1 in
  WORK-SPLIT.md. NOT implemented here (no firmware edits).
- **DEMO-01** — teammate-owned hardware: the live GPIO-13 round-trip (press → photo → Telegram →
  reply → dashboard → OLED) runs on Yussef's bench with `TRIGGER_ON_BOOT false`. Backend
  contribution = the authenticated endpoints accept the firmware's `X-Device-Key` requests. NOT
  run here (no hardware assumed).

## Commit
- `b36cdb8` feat(02-03): bootstrap pgvector + visitors/visitor_embeddings in init_db (REL-01)

## Self-Check: PASSED
- All statements idempotent; existing prod data untouched.
- No firmware/dashboard edits; recognition disabled.
- pgvector `CREATE EXTENSION` succeeded against the live Supabase (the one privileged statement).
