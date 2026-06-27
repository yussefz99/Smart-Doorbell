---
phase: 02-security-reliability-hardening
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - backend/server.py
  - backend/.env
autonomous: true
requirements: [SEC-01, SEC-02, SEC-06]
must_haves:
  truths:
    - "POST /api/visits without the correct X-Device-Key returns 401 when DEVICE_SECRET is set"
    - "POST /api/visits with the correct X-Device-Key returns 201 when DEVICE_SECRET is set"
    - "POST /api/device/heartbeat without the correct X-Device-Key returns 401 when DEVICE_SECRET is set; with it returns 200"
    - "GET /api/visits/{id}/response without the correct X-Device-Key returns 401 when DEVICE_SECRET is set; with it returns the reply text"
    - "When DEVICE_SECRET is empty/unset, all three endpoints stay open and behave exactly as before (no 401)"
  artifacts:
    - path: "backend/server.py"
      provides: "require_device_key dependency + Depends() on the three device routes"
      contains: "def require_device_key"
    - path: "backend/.env"
      provides: "DEVICE_SECRET documented, defaulting to empty"
      contains: "DEVICE_SECRET"
  key_links:
    - from: "create_visit (POST /api/visits)"
      to: "require_device_key"
      via: "dependencies=[Depends(require_device_key)]"
      pattern: "Depends\\(require_device_key\\)"
    - from: "require_device_key"
      to: "DEVICE_SECRET env var"
      via: "empty-disables-gate comparison mirroring require_dashboard_key"
      pattern: "DEVICE_SECRET"
---

<objective>
Authenticate the three device-facing endpoints with a shared `X-Device-Key` header,
gated by a new `DEVICE_SECRET` env var, mirroring the existing `require_dashboard_key`
pattern (`backend/server.py:51-53`).

Purpose: Close the highest-value, cheapest security gap (lab-prototype bar) — the
device upload, heartbeat, and reply-poll endpoints are currently fully unauthenticated.
Output: A `require_device_key` dependency attached to `create_visit`, `device_heartbeat`,
and `get_visit_response`; the `DEVICE_SECRET` env var documented in `backend/.env`.

CRITICAL SAFETY PROPERTY (gate-off-when-empty): When `DEVICE_SECRET` is empty/unset the
dependency is a no-op and every endpoint stays open — exactly like `DASHBOARD_PASSWORD`
behaves today. This is what lets the backend deploy WITHOUT breaking the teammate's live
device. The dependency MUST NOT hard-require the secret. Auth only "turns on" once
`DEVICE_SECRET` is set on Railway AND the same value is in the firmware secrets.
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
<!-- The analog to mirror EXACTLY. Use these directly — no codebase exploration needed. -->

From backend/server.py (the dashboard-key pattern to copy for device auth):
```python
DASHBOARD_PASSWORD = os.getenv("DASHBOARD_PASSWORD", "").strip()

def require_dashboard_key(x_dashboard_key: str = Header(default="")):
    if DASHBOARD_PASSWORD and x_dashboard_key != DASHBOARD_PASSWORD:
        raise HTTPException(401, "Invalid dashboard password")
```

How it is attached today (FastAPI dependency convention to follow):
```python
@app.get("/api/auth/check", dependencies=[Depends(require_dashboard_key)])
```

The three routes that need the dependency added (current signatures, all in backend/server.py):
```python
@app.post("/api/visits", status_code=201)
async def create_visit(trigger: str = Form(...), timestamp: str = Form(None),
                       silent: bool = Form(False), photo: UploadFile = File(None)):

@app.post("/api/device/heartbeat")
def device_heartbeat(body: DeviceHeartbeat):

@app.get("/api/visits/{visit_id}/response")
def get_visit_response(visit_id: int):
```
Note `Header` and `Depends` are already imported at backend/server.py:16.
The FastAPI dependency-injected header param uses snake_case `x_device_key`, which maps
to the wire header `X-Device-Key`.
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Add require_device_key dependency + DEVICE_SECRET env var</name>
  <files>backend/server.py, backend/.env</files>
  <read_first>
    - backend/server.py:36-55 (the DASHBOARD_PASSWORD constant + require_dashboard_key analog to mirror)
    - backend/server.py:16 (confirm Header + Depends already imported)
    - backend/.env (existing env-var documentation style; note the commented `# DEVICE_SECRET=` placeholder at the bottom)
  </read_first>
  <behavior>
    - When DEVICE_SECRET is empty: require_device_key returns None for any x_device_key value (no raise) — gate disabled.
    - When DEVICE_SECRET is set and x_device_key does not match: raises HTTPException(401, ...).
    - When DEVICE_SECRET is set and x_device_key matches: returns None (passes).
  </behavior>
  <action>
    In backend/server.py, near the DASHBOARD_PASSWORD definition (server.py:44), add a module-level
    constant `DEVICE_SECRET = os.getenv("DEVICE_SECRET", "").strip()`. Immediately below
    `require_dashboard_key` (server.py:51-53), define `def require_device_key(x_device_key: str = Header(default=""))`
    that raises `HTTPException(401, "Invalid device key")` ONLY when `DEVICE_SECRET` is truthy AND
    `x_device_key != DEVICE_SECRET`. The empty-secret branch must fall through with no raise — identical
    logic shape to require_dashboard_key. Do not introduce any new imports. In backend/.env, replace the
    commented `# DEVICE_SECRET=` placeholder with a documented `DEVICE_SECRET=` line (value empty by default),
    with a comment noting that empty = auth disabled and that the value must match the firmware secrets.h
    SECRET_DEVICE_KEY for the live device.
  </action>
  <verify>
    <automated>cd backend && python -c "import ast,sys; t=ast.parse(open('server.py').read()); fns=[n.name for n in ast.walk(t) if isinstance(n,ast.FunctionDef)]; sys.exit(0 if 'require_device_key' in fns else 1)" && grep -v '^#' server.py | grep -q 'DEVICE_SECRET' && grep -q '^DEVICE_SECRET=' .env</automated>
  </verify>
  <acceptance_criteria>
    - backend/server.py contains `def require_device_key(`
    - backend/server.py defines a `DEVICE_SECRET` module constant read from `os.getenv("DEVICE_SECRET", "")`
    - require_device_key raises only inside an `if DEVICE_SECRET and ...` guard (empty secret never raises)
    - The raise uses HTTPException with status 401
    - backend/.env contains an uncommented `DEVICE_SECRET=` line defaulting to empty
    - No new import statements were added to server.py
  </acceptance_criteria>
  <done>require_device_key exists, mirrors require_dashboard_key's empty-disables-gate semantics, and DEVICE_SECRET is documented in backend/.env.</done>
</task>

<task type="auto">
  <name>Task 2: Attach require_device_key to the three device routes + verify gate behavior</name>
  <files>backend/server.py</files>
  <read_first>
    - backend/server.py:366 (create_visit — POST /api/visits decorator)
    - backend/server.py:575-576 (device_heartbeat — POST /api/device/heartbeat decorator)
    - backend/server.py:476-481 (get_visit_response — GET /api/visits/{id}/response, currently explicitly unauthenticated; remove the "No dashboard key" comment rationale only as it becomes inaccurate)
    - backend/server.py:242 (example of dependencies=[Depends(...)] on an existing route)
  </read_first>
  <action>
    Add `dependencies=[Depends(require_device_key)]` to exactly three route decorators in backend/server.py:
    `@app.post("/api/visits", status_code=201)` (create_visit), `@app.post("/api/device/heartbeat")`
    (device_heartbeat), and `@app.get("/api/visits/{visit_id}/response")` (get_visit_response). Preserve the
    existing `status_code=201` kwarg on create_visit. Do NOT add the dependency to any other route, the
    WebSocket endpoint, or the Telegram webhook. Update the stale inline comment above get_visit_response
    that claims the endpoint has "No dashboard key" so it no longer misstates the auth posture (the firmware
    already sends X-Device-Key on this poll). Do not change any route body logic.
  </action>
  <verify>
    <automated>cd backend && DEVICE_SECRET=testkey123 uvicorn server:app --port 8099 & SVPID=$!; sleep 4; echo "--- no key (expect 401) ---"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8099/api/device/heartbeat -H "Content-Type: application/json" -d '{"wifi_signal":-50}'; echo "--- wrong key (expect 401) ---"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8099/api/device/heartbeat -H "Content-Type: application/json" -H "X-Device-Key: wrong" -d '{"wifi_signal":-50}'; echo "--- right key (expect 200) ---"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8099/api/device/heartbeat -H "Content-Type: application/json" -H "X-Device-Key: testkey123" -d '{"wifi_signal":-50}'; kill $SVPID 2>/dev/null; echo "--- gate OFF (DEVICE_SECRET empty) no key expect 200 ---"; uvicorn server:app --port 8098 & SVPID2=$!; sleep 4; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8098/api/device/heartbeat -H "Content-Type: application/json" -d '{"wifi_signal":-50}'; kill $SVPID2 2>/dev/null</automated>
  </verify>
  <acceptance_criteria>
    - With DEVICE_SECRET set: POST /api/device/heartbeat with no X-Device-Key returns 401; with wrong key returns 401; with correct key returns 200
    - With DEVICE_SECRET set: POST /api/visits with no/wrong X-Device-Key returns 401; with correct key proceeds past auth (201 on success)
    - With DEVICE_SECRET set: GET /api/visits/{id}/response with no/wrong X-Device-Key returns 401; with correct key returns the reply payload
    - With DEVICE_SECRET empty/unset: all three endpoints accept requests with NO X-Device-Key header (no 401) — gate off
    - Exactly three route decorators carry `dependencies=[Depends(require_device_key)]`; the webhook, /ws, and dashboard routes are untouched
  </acceptance_criteria>
  <done>The three device endpoints enforce X-Device-Key when DEVICE_SECRET is set and stay fully open when it is empty.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| ESP32 firmware → backend HTTP | Untrusted network input crosses here (visit upload, heartbeat, reply poll). Anyone on the internet can hit the Railway URL. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-02-01 | Spoofing | POST /api/visits (create_visit) | mitigate | require_device_key compares X-Device-Key to DEVICE_SECRET; reject 401 when set and mismatched |
| T-02-02 | Spoofing | POST /api/device/heartbeat (device_heartbeat) | mitigate | same require_device_key dependency; prevents forged liveness/wifi data |
| T-02-03 | Information Disclosure | GET /api/visits/{id}/response (get_visit_response) | mitigate | add require_device_key — previously unauthenticated, leaked reply text by enumerating visit ids |
| T-02-04 | Denial of Service | all device endpoints | accept | no rate limiting at the lab-prototype bar (per CLAUDE.md); shared secret raises the cost of abuse enough for a graded demo |
| T-02-05 | Tampering | DEVICE_SECRET misconfiguration | accept | gate-off-when-empty is intentional: empty secret = open, so a missed Railway var degrades to today's behavior, never a hard outage of the live device |
</threat_model>

<verification>
Boot `uvicorn server:app` locally against the shared Supabase `DATABASE_URL`.
1. With `DEVICE_SECRET=<value>`: curl each of the three endpoints with no header (expect 401), wrong header (expect 401), correct header (expect success: 201 for visits, 200 for heartbeat, 200 + reply body for response poll).
2. With `DEVICE_SECRET` empty: curl each endpoint with no header and confirm it behaves exactly as before (no 401) — the gate is off.
</verification>

<success_criteria>
- ROADMAP Phase 2 success criteria 1, 2, and 7 are met (visits, heartbeat, and response-poll auth).
- Gate-off-when-empty proven: empty DEVICE_SECRET leaves all three endpoints open.
- No firmware, dashboard, or webhook code touched.
</success_criteria>

<output>
Create `.planning/phases/02-security-reliability-hardening/02-01-SUMMARY.md` when done.
</output>
