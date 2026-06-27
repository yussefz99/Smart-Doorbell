---
phase: 03-visitor-recognition
plan: 02
type: execute
wave: 1
depends_on: []
files_modified:
  - backend/server.py
autonomous: true
requirements: [REC-06]
must_haves:
  truths:
    - "GET /api/visits/{id}/response returns JSON containing a visitor_name key"
    - "When the visit's visitor has a name, visitor_name is that name; when there is no visitor or no name, visitor_name is null"
    - "GET /api/visits/{id}/response still returns the existing id and response keys (no breaking change for the firmware's text-reply read)"
    - "GET /api/visits/{id}/response still requires X-Device-Key when DEVICE_SECRET is set (Phase 2 auth preserved)"
  artifacts:
    - path: "backend/server.py"
      provides: "get_visit_response returning {id, response, visitor_name}"
      contains: "visitor_name"
  key_links:
    - from: "get_visit_response (GET /api/visits/{id}/response)"
      to: "visits.visitor_id -> visitors.name"
      via: "LEFT JOIN visitors ON visitors.id = visits.visitor_id (mirrors get_visits at server.py:305)"
      pattern: "LEFT JOIN visitors"
    - from: "get_visit_response decorator"
      to: "require_device_key"
      via: "dependencies=[Depends(require_device_key)] kept from Phase 2"
      pattern: "Depends\\(require_device_key\\)"
---

<objective>
Add `visitor_name` to the `GET /api/visits/{id}/response` payload so the door OLED firmware
can greet a recognized visitor by name. This fulfils handshake #2 with Yussef: the agreed
response shape is `{ "id", "response", "visitor_name" }`. The endpoint currently returns only
`{id, response}` (`backend/server.py:519-523`).

Purpose: The firmware OLED already reads this poll endpoint for the homeowner's text reply.
REC-06's "Welcome, [Name]" greeting needs the recognized name delivered through this same
poll — adding `visitor_name` here is the backend half of the contract (the firmware half is
Yussef-owned, partially done in commit 1f1e94a).
Output: `get_visit_response` returns `{id, response, visitor_name}`, joining `visits.visitor_id`
to `visitors.name`, with `visitor_name` null when the visit has no recognized visitor.

DO NOT: change the existing `id`/`response` keys (firmware depends on them), remove the
`require_device_key` dependency added in Phase 2, or touch any other route. This is a single
additive field on one endpoint.
</objective>

<execution_context>
@~/.claude/get-shit-done/workflows/execute-plan.md
@~/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/03-visitor-recognition/03-CONTEXT.md
@.planning/WORK-SPLIT.md
@CLAUDE.md

<interfaces>
<!-- The endpoint to change, plus the exact JOIN analog to copy from get_visits. -->

Current endpoint (backend/server.py:518-523) — returns only id + response:
```python
@app.get("/api/visits/{visit_id}/response", dependencies=[Depends(require_device_key)])
def get_visit_response(visit_id: int):
    row = db_fetchone("SELECT id, response FROM visits WHERE id=%s", (visit_id,))
    if not row:
        raise HTTPException(404, "visit not found")
    return {"id": row["id"], "response": row["response"]}
```

The JOIN pattern already used by get_visits (backend/server.py:304-307) — mirror this to pull the name:
```python
"""SELECT visits.*, visitors.name AS visitor_name
   FROM visits LEFT JOIN visitors ON visitors.id = visits.visitor_id
   ORDER BY visits.timestamp DESC"""
```

db_fetchone returns a RealDictCursor row (dict-like). visitor_name will be None when the visit
has no visitor_id or the visitor row has a NULL name. require_device_key (server.py:67-69) enforces
X-Device-Key only when DEVICE_SECRET is set — keep the dependency exactly as-is.

Agreed contract (handshake #2, WORK-SPLIT.md): `{ "id", "response", "visitor_name" }`.
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Add visitor_name to GET /api/visits/{id}/response via a LEFT JOIN</name>
  <files>backend/server.py</files>
  <read_first>
    - backend/server.py:513-523 (get_visit_response — the endpoint to modify, including its inline comment and require_device_key dependency)
    - backend/server.py:304-307 (get_visits — the canonical `LEFT JOIN visitors ON visitors.id = visits.visitor_id` with `visitors.name AS visitor_name` to copy)
    - backend/server.py:67-69 (require_device_key — confirm the auth dependency to keep)
  </read_first>
  <behavior>
    - For a visit whose visitor has a name: GET /api/visits/{id}/response returns {"id": <id>, "response": <text-or-null>, "visitor_name": "<name>"}.
    - For a visit with no visitor_id (or visitor name NULL): the same call returns "visitor_name": null, with id and response unchanged.
    - For a non-existent visit id: still returns 404 "visit not found" (unchanged).
    - With DEVICE_SECRET set and no/incorrect X-Device-Key: still returns 401 (auth unchanged).
  </behavior>
  <action>
    In backend/server.py, modify only `get_visit_response` (server.py:519-523). Change its query from
    `SELECT id, response FROM visits WHERE id=%s` to a LEFT JOIN that also pulls the visitor name —
    mirror the exact join shape from get_visits (server.py:305): select `visits.id`, `visits.response`,
    and `visitors.name AS visitor_name` from `visits LEFT JOIN visitors ON visitors.id = visits.visitor_id`
    filtered by `visits.id = %s`. Keep the `if not row: raise HTTPException(404, "visit not found")` guard.
    Change the return dict to `{"id": row["id"], "response": row["response"], "visitor_name": row["visitor_name"]}`.
    Keep the `dependencies=[Depends(require_device_key)]` on the decorator untouched. Update the inline
    comment block above the route (server.py:513-517) to note it now also returns the recognized
    visitor_name for the OLED greeting. Do not change any other route, the SELECT alias spelling
    (`visitor_name`), or the auth dependency.
  </action>
  <verify>
    <automated>cd backend && uvicorn server:app --port 8097 & SVPID=$!; sleep 4; echo "--- response payload shape (expect keys id,response,visitor_name) ---"; VID=$(curl -s "http://127.0.0.1:8097/api/visits" -H "X-Dashboard-Key: $DASHBOARD_PASSWORD" | .venv/bin/python -c "import sys,json; d=json.load(sys.stdin); print(d[0]['id'] if d else '')" 2>/dev/null); if [ -n "$VID" ]; then curl -s "http://127.0.0.1:8097/api/visits/$VID/response" -H "X-Device-Key: $DEVICE_SECRET" | .venv/bin/python -c "import sys,json; d=json.load(sys.stdin); assert set(['id','response','visitor_name']).issubset(d.keys()), d; print('[OK] keys present:', sorted(d.keys()))"; else echo "[SKIP] no visits in DB — assert key presence on 404 path instead"; curl -s "http://127.0.0.1:8097/api/visits/0/response" -H "X-Device-Key: $DEVICE_SECRET" -o /dev/null -w "%{http_code}\n"; fi; kill $SVPID 2>/dev/null</automated>
  </verify>
  <acceptance_criteria>
    - backend/server.py get_visit_response query contains `LEFT JOIN visitors` and `visitors.name AS visitor_name`
    - The return dict contains all three keys: `id`, `response`, `visitor_name`
    - The 404 guard for a missing visit is preserved
    - `dependencies=[Depends(require_device_key)]` is still on the route decorator (Phase 2 auth intact)
    - No other route in server.py was modified
    - For a visit with a named visitor, the live call returns that name; for an unrecognized visit, visitor_name is null
  </acceptance_criteria>
  <done>GET /api/visits/{id}/response returns the agreed {id, response, visitor_name} shape with the recognized name (or null), keeps its 404 and X-Device-Key behavior, and no other endpoint is touched.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| ESP32 firmware → backend HTTP (reply poll) | The OLED device polls this endpoint over the network; the response now carries a person's name (PII) in addition to the reply text. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-03 | Information Disclosure | GET /api/visits/{id}/response now returns visitor_name (PII) | mitigate | Keep the `require_device_key` dependency from Phase 2 — the name is only readable with the correct X-Device-Key when DEVICE_SECRET is set; enumeration of visit ids cannot leak names anonymously |
| T-03-04 | Tampering | Response contract change breaking firmware | mitigate | Additive-only change: `id` and `response` keys are preserved byte-for-byte; firmware that ignores `visitor_name` is unaffected (handshake #2 agreed with Yussef) |
| T-03-05 | Information Disclosure | Face/name PII at rest in Supabase (visitors table) | accept | Lab-prototype bar (CLAUDE.md); embeddings + names stored unencrypted in pgvector is an accepted residual risk for this submission; no liveness/anti-spoofing in scope |
</threat_model>

<verification>
Boot `uvicorn server:app` locally against the shared Supabase `DATABASE_URL`.
1. GET a visit id that has a recognized, named visitor: confirm `{id, response, visitor_name}` with the correct name.
2. GET a visit id with no visitor: confirm `visitor_name` is null and `id`/`response` unchanged.
3. GET a non-existent visit id: confirm 404 "visit not found".
4. With DEVICE_SECRET set, GET with no/wrong X-Device-Key: confirm 401 (auth still enforced).
</verification>

<success_criteria>
- ROADMAP Phase 3 success criterion 6 (backend half) is satisfied: the recognized name is delivered to the firmware via /response for the OLED greeting.
- Handshake #2 contract `{id, response, visitor_name}` is honored; the existing keys are unchanged.
- Phase 2 device auth on this endpoint is preserved.
- REC-06 firmware (OLED "Welcome, [Name]") remains teammate-owned (Yussef) — only the backend contract is delivered here.
</success_criteria>

<output>
Create `.planning/phases/03-visitor-recognition/03-02-SUMMARY.md` when done.
Note in the SUMMARY that the firmware OLED side of REC-06 is Yussef-owned and that this plan
delivers only the backend contract (handshake #2).
</output>
