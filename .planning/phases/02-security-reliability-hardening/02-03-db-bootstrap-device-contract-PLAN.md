---
phase: 02-security-reliability-hardening
plan: 03
type: execute
wave: 3
depends_on: ["02-02"]
files_modified:
  - backend/server.py
autonomous: true
requirements: [REL-01, SEC-03, DEMO-01]
must_haves:
  truths:
    - "On a fresh database (visitors / visitor_embeddings dropped), init_db() runs without error and creates both tables plus the pgvector extension"
    - "After a fresh init_db(), GET /api/visitors returns 200 (empty list) instead of a 500 from a missing table"
    - "After a fresh init_db(), POST /api/visits succeeds (no 500 from a missing visitors/visitor_embeddings table) — recognition stays OFF"
    - "The visits table has a nullable visitor_id column after init_db()"
    - "The backend's X-Device-Key header contract matches the firmware (teammate-owned SEC-03 verification)"
  artifacts:
    - path: "backend/server.py"
      provides: "init_db() creating pgvector + visitors + visitor_embeddings + visits.visitor_id, all idempotent"
      contains: "CREATE TABLE IF NOT EXISTS visitor_embeddings"
  key_links:
    - from: "init_db()"
      to: "visitors / visitor_embeddings tables queried by /api/visitors and recognition.py"
      via: "CREATE TABLE IF NOT EXISTS with columns matching the existing queries"
      pattern: "CREATE TABLE IF NOT EXISTS visitors"
    - from: "init_db()"
      to: "pgvector embedding column"
      via: "CREATE EXTENSION IF NOT EXISTS vector + vector(512) column"
      pattern: "CREATE EXTENSION IF NOT EXISTS vector"
---

<objective>
Make `init_db()` create every table the app already queries so a fresh database does not 500,
and record the teammate-owned device/backend contract verification (SEC-03, DEMO-01).

Purpose: `/api/visitors` (server.py:329) selects from `visitors`, `get_visits` LEFT JOINs
`visitors` (server.py:264-266), and `create_visit` writes `visitor_id` into `visits`
(server.py:441) — but `init_db()` never creates `visitors` or `visitor_embeddings`, so a
fresh database 500s on those paths. This plan extends `init_db()` to bootstrap the two
recognition tables (with pgvector) and the `visits.visitor_id` column, all idempotent, so
existing prod data is untouched and a dropped-table cold start re-bootstraps cleanly.
Recognition itself stays OFF (Phase 3) — this only provisions the schema.

This plan also carries the requirement-coverage lines for the two teammate-owned items so
phase coverage stays complete: SEC-03 (firmware already sends X-Device-Key, commit 770a43e)
and DEMO-01 (live GPIO-13 round-trip on the teammate's bench). The BACKEND side of both is
"the authenticated endpoints accept the firmware's requests" — verified by confirming the
header name and env contract, NOT by flashing or running hardware here.
</objective>

<execution_context>
@~/.claude/get-shit-done/workflows/execute-plan.md
@~/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/02-security-reliability-hardening/02-CONTEXT.md
@.planning/WORK-SPLIT.md
@CLAUDE.md

<interfaces>
<!-- Current init_db (backend/server.py:80-115) creates visits, device_status, settings. Extend it. -->
<!-- The exact columns the existing queries + recognition.py require: -->

<!-- /api/visitors query (server.py:329-334) selects these columns from visitors: -->
```python
"SELECT id, name, photo_url, first_seen, last_seen, visit_count FROM visitors ORDER BY last_seen DESC"
```

<!-- get_visits LEFT JOIN (server.py:264-266) needs visitors.id and visitors.name and visits.visitor_id: -->
```python
"SELECT visits.*, visitors.name AS visitor_name FROM visits LEFT JOIN visitors ON visitors.id = visits.visitor_id"
```

<!-- recognition.py (match_or_create_visitor) inserts/queries: -->
```python
# visitors:        photo_url, first_seen, last_seen, visit_count, name, id
# visitor_embeddings:  visitor_id, embedding (pgvector); cosine via  e.embedding <=> %s::vector
# INSERT INTO visitors (photo_url, first_seen, last_seen, visit_count) VALUES (...) RETURNING id
# INSERT INTO visitor_embeddings (visitor_id, embedding) VALUES (%s, %s::vector)
```
Embedding dimension: InsightFace buffalo_s / buffalo_l both produce 512-float normed
embeddings → use `vector(512)`. `first_seen` / `last_seen` are written with an ISO timestamp
string and compared against `now`-style values → use TIMESTAMPTZ.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Extend init_db() to bootstrap pgvector + visitors + visitor_embeddings + visits.visitor_id</name>
  <files>backend/server.py</files>
  <read_first>
    - backend/server.py:80-115 (existing init_db block — the single cur.execute multi-statement string to extend)
    - backend/server.py:329-334 (the /api/visitors SELECT — column names that visitors MUST provide)
    - backend/server.py:264-266 (the get_visits LEFT JOIN — needs visits.visitor_id + visitors.name)
    - backend/recognition.py:75-126 (match_or_create_visitor — exact insert columns + pgvector usage + 512-dim embedding)
  </read_first>
  <action>
    Extend the multi-statement SQL inside init_db() (server.py:83-115) with idempotent statements, appended
    after the existing settings INSERT so existing tables/data are untouched. Add, in order:
    (1) `CREATE EXTENSION IF NOT EXISTS vector;` so pgvector is available without a manual Supabase dashboard step.
    (2) `CREATE TABLE IF NOT EXISTS visitors (...)` with columns `id SERIAL PRIMARY KEY`, `name TEXT`,
    `photo_url TEXT`, `first_seen TIMESTAMPTZ`, `last_seen TIMESTAMPTZ`, `visit_count INTEGER NOT NULL DEFAULT 0`
    — matching the /api/visitors SELECT and recognition.py inserts exactly.
    (3) `CREATE TABLE IF NOT EXISTS visitor_embeddings (...)` with `id SERIAL PRIMARY KEY`,
    `visitor_id INTEGER REFERENCES visitors(id) ON DELETE CASCADE`, and `embedding vector(512)` (the pgvector
    type matching recognition.py's `%s::vector` casts and the buffalo 512-float embedding).
    (4) `ALTER TABLE visits ADD COLUMN IF NOT EXISTS visitor_id INTEGER REFERENCES visitors(id) ON DELETE SET NULL;`
    so the get_visits JOIN and create_visit INSERT have the column on an already-existing visits table.
    Keep everything inside the existing single cur.execute string (or a second cur.execute in the same block);
    every statement must be guarded with IF NOT EXISTS / ADD COLUMN IF NOT EXISTS so re-running init_db() and
    a dropped-table cold start both succeed. Do not enable recognition and do not change create_visit logic.
  </action>
  <verify>
    <automated>cd backend && python -c "src=open('server.py').read(); assert 'CREATE EXTENSION IF NOT EXISTS vector' in src; assert 'CREATE TABLE IF NOT EXISTS visitors' in src; assert 'CREATE TABLE IF NOT EXISTS visitor_embeddings' in src; assert 'vector(512)' in src; assert 'ADD COLUMN IF NOT EXISTS visitor_id' in src" && python -c "import server; server.init_db(); server.init_db(); print('init_db idempotent OK'); rows=server.db_fetchall('SELECT id,name,photo_url,first_seen,last_seen,visit_count FROM visitors LIMIT 1'); print('visitors selectable'); server.db_fetchall('SELECT visitor_id FROM visits LIMIT 1'); print('visits.visitor_id present'); server.db_fetchall('SELECT embedding FROM visitor_embeddings LIMIT 1'); print('visitor_embeddings present')"</automated>
  </verify>
  <acceptance_criteria>
    - backend/server.py init_db() contains `CREATE EXTENSION IF NOT EXISTS vector`, `CREATE TABLE IF NOT EXISTS visitors`, `CREATE TABLE IF NOT EXISTS visitor_embeddings`, and `ALTER TABLE visits ADD COLUMN IF NOT EXISTS visitor_id`
    - The visitor_embeddings embedding column is `vector(512)`
    - The visitors table exposes exactly the columns /api/visitors selects: id, name, photo_url, first_seen, last_seen, visit_count
    - init_db() runs twice in a row against the shared Supabase with no error (idempotent)
    - After init_db(): `SELECT ... FROM visitors`, `SELECT visitor_id FROM visits`, and `SELECT embedding FROM visitor_embeddings` all execute without error
    - create_visit logic and RECOGNITION_ENABLED gating are unchanged
  </acceptance_criteria>
  <done>init_db() bootstraps pgvector and both recognition tables idempotently; fresh-DB queries on visitors/visits.visitor_id/visitor_embeddings no longer 500.</done>
</task>

<task type="auto">
  <name>Task 2: Verify fresh-DB endpoint health + record teammate-owned SEC-03/DEMO-01 contract</name>
  <files>backend/server.py</files>
  <read_first>
    - backend/server.py:261-268 (get_visits — the JOIN that 500s without visitors/visitor_id)
    - backend/server.py:329-342 (get_visitors — 500s without the visitors table)
    - .planning/WORK-SPLIT.md (handshake #1: X-Device-Key header name + DEVICE_SECRET env var; SEC-03/DEMO-01 ownership)
    - .planning/phases/02-security-reliability-hardening/02-CONTEXT.md (firmware sends X-Device-Key on visit :562, heartbeat :647, response poll :338; TRIGGER_ON_BOOT false :71)
  </read_first>
  <action>
    No code change in this task — it is a verification + documentation task. Boot `uvicorn server:app` against the
    shared Supabase and confirm the fresh-DB endpoint health: `GET /api/visitors` returns 200 (list, possibly empty)
    and `GET /api/visits` returns 200 (the JOIN resolves) — both formerly 500 candidates on a fresh DB. Then record,
    in the plan SUMMARY, the teammate-owned coverage so phase requirement coverage is complete:
    (a) SEC-03 — the BACKEND contract is that `require_device_key` reads the wire header `X-Device-Key` and compares
    to `DEVICE_SECRET`; confirm this header name and env-var name match handshake #1 in WORK-SPLIT.md and the firmware
    (commit 770a43e sends X-Device-Key on visit/heartbeat/response poll). Mark SEC-03 firmware implementation as
    teammate-owned (Yussef), already done — NOT implemented here.
    (b) DEMO-01 — the BACKEND contract is that the three authenticated endpoints accept the firmware's
    X-Device-Key-bearing requests; the live GPIO-13 round-trip (press → photo → Telegram → reply → dashboard → OLED)
    runs on Yussef's bench with TRIGGER_ON_BOOT false. Mark DEMO-01 as teammate-owned/hardware — NOT run here.
    The SUMMARY must state explicitly that SEC-03 and DEMO-01 are covered by the device/backend contract and are
    verified on hardware by the teammate, not by the backend executor.
  </action>
  <verify>
    <automated>cd backend && (uvicorn server:app --port 8095 & echo $! > /tmp/sd_p3t2.pid); sleep 4; echo "GET /api/visitors (gate off, expect 200):"; curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8095/api/visitors; echo "GET /api/visits (gate off, expect 200):"; curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8095/api/visits; kill "$(cat /tmp/sd_p3t2.pid)" 2>/dev/null</automated>
  </verify>
  <acceptance_criteria>
    - GET /api/visitors returns 200 against the freshly-bootstrapped DB (no 500 from a missing table)
    - GET /api/visits returns 200 (the visitors LEFT JOIN resolves)
    - The SUMMARY documents SEC-03 as teammate-owned firmware (Yussef, commit 770a43e), confirming the backend's X-Device-Key + DEVICE_SECRET contract matches handshake #1
    - The SUMMARY documents DEMO-01 as teammate-owned hardware (live GPIO-13 round-trip on Yussef's bench, TRIGGER_ON_BOOT false), with the backend contribution being authenticated endpoints that accept the firmware's requests
    - No firmware files were edited and no hardware was assumed present
  </acceptance_criteria>
  <done>Fresh-DB endpoint health confirmed (no 500s); SEC-03 and DEMO-01 recorded as teammate-owned coverage with the backend contract verified.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Application code → database schema | A fresh or partially-migrated database is an availability boundary: missing tables turn read/write paths into 500s. |
| Firmware (teammate) → authenticated backend endpoints | Covered by Plan 02-01's require_device_key; this plan confirms the header/env contract matches the firmware. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-02-11 | Denial of Service | get_visits / get_visitors / create_visit on a fresh DB | mitigate | init_db() creates visitors + visitor_embeddings + visits.visitor_id idempotently so endpoints stop 500ing |
| T-02-12 | Tampering | init_db() against existing prod data | mitigate | every statement uses IF NOT EXISTS / ADD COLUMN IF NOT EXISTS — existing tables and data are never dropped or altered destructively |
| T-02-13 | Elevation of Privilege | CREATE EXTENSION vector requires DB privilege | accept | Supabase grants the connection role extension-create rights; if denied the statement is the only failure point and pgvector can be enabled once via the dashboard (documented fallback) |
| T-02-SC | Tampering | npm/pip installs | accept | no new packages installed in this phase — pgvector ships with Supabase Postgres; recognition deps (insightface/onnxruntime) already pinned in requirements.txt and are not installed or imported here (RECOGNITION_ENABLED stays 0) |
</threat_model>

<verification>
Boot `uvicorn server:app` against the shared Supabase.
1. Run init_db() twice — both succeed (idempotent).
2. Drop-and-recreate scenario: with visitors/visitor_embeddings absent, init_db() recreates them and GET /api/visitors / GET /api/visits / POST /api/visits no longer 500.
3. Confirm visits.visitor_id exists and visitor_embeddings.embedding is vector(512).
4. Record SEC-03 + DEMO-01 as teammate-owned with the backend X-Device-Key/DEVICE_SECRET contract confirmed against handshake #1.
</verification>

<success_criteria>
- ROADMAP Phase 2 success criterion 5 (fresh-DB init_db + POST /api/visits succeeds, no missing-table 500) is met.
- SEC-03 (firmware) and DEMO-01 (hardware round-trip) are recorded as teammate-owned, with the backend contract verified — coverage complete.
- No firmware/dashboard edits; recognition stays disabled.
</success_criteria>

<output>
Create `.planning/phases/02-security-reliability-hardening/02-03-SUMMARY.md` when done.
</output>
