---
phase: 03-visitor-recognition
plan: 03
type: execute
wave: 2
depends_on: ["03-01"]
files_modified:
  - backend/.env
  - .planning/phases/03-visitor-recognition/REC-04-MEMORY-RESULT.md
  - .planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md
autonomous: false
requirements: [REC-04, REC-05, REC-01, REC-02, REC-03, REC-06]
user_setup:
  - service: railway
    why: "Activate recognition in production by setting env vars on the deployed service (operator-only; no CLI secret can be set from the plan)"
    env_vars:
      - name: RECOGNITION_ENABLED
        source: "Railway Dashboard -> smart-doorbell service -> Variables (set to 1)"
      - name: FACE_MODEL
        source: "Railway Dashboard -> smart-doorbell service -> Variables (set to buffalo_s)"
must_haves:
  truths:
    - "GET /api/recognition/health with RECOGNITION_ENABLED=1 loads buffalo_s and reports a resident memory number"
    - "The same person re-recognizes (stable visitor_id, name carried) at MATCH_THRESHOLD=0.40 over the validation photos"
    - "Different people do NOT false-match at MATCH_THRESHOLD=0.40 over the validation photos"
    - "A recognition failure (no face / model error) never blocks visit creation (verified, already implemented)"
    - "A go/keep-off decision for production is recorded based on whether Railway's plan holds the model without OOM"
  artifacts:
    - path: ".planning/phases/03-visitor-recognition/REC-04-MEMORY-RESULT.md"
      provides: "Measured resident memory (local + Railway) for buffalo_s and the prod go/keep-off decision"
      contains: "process_rss_mb"
    - path: ".planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md"
      provides: "Measured same-person / different-person separation and the confirmed MATCH_THRESHOLD"
      contains: "MATCH_THRESHOLD"
    - path: "backend/.env"
      provides: "RECOGNITION_ENABLED=1 + FACE_MODEL=buffalo_s for LOCAL recognition testing"
      contains: "RECOGNITION_ENABLED"
  key_links:
    - from: "GET /api/recognition/health"
      to: "recognition.health() -> psutil RSS"
      via: "model load under RECOGNITION_ENABLED=1, reporting process_rss_mb"
      pattern: "process_rss_mb"
    - from: "REC-05 validation harness"
      to: "recognition.extract_embedding + recognition.match_or_create_visitor"
      via: "running the production recognition path against the live Supabase pgvector tables"
      pattern: "match_or_create_visitor"
---

<objective>
Validate that visitor recognition is safe to turn on, then make the enablement decision.
This plan does NOT build recognition (already implemented) — it MEASURES and DECIDES:
(1) REC-04: confirm `buffalo_s` fits the hosting memory budget without OOM, via
`GET /api/recognition/health`, locally and on Railway; (2) REC-05: confirm same-person
re-recognition and no false matches at `MATCH_THRESHOLD=0.40` against the live DB; (3) set
`RECOGNITION_ENABLED=1`/`FACE_MODEL=buffalo_s` locally for testing and hand the Railway
variable change to the operator.

Purpose: REC-01's spike proved feasibility on near-identical frames; REC-04 and REC-05 close
the two open risks before production: out-of-memory on Railway's small plan, and accuracy on
real DB-resident embeddings. The go/keep-off decision mirrors the REC-01 gate philosophy —
if Railway can't hold the model, recognition ships OFF in prod (documented, acceptable) and
the demo runs it locally.
Output: two short result notes (memory + accuracy) in the phase dir, `backend/.env` set for
local recognition, and a recorded prod enablement decision.

DECIDED (do not revisit): FACE_MODEL=buffalo_s; MATCH_THRESHOLD=0.40. Only adjust the
threshold if REC-05 data argues for it (~0.42 gap midpoint) and record the rationale.
DO NOT: modify recognition.py, server.py, or telegram_bot.py — the pipeline is built and
verified. This plan is measurement, configuration, and documentation.
</objective>

<execution_context>
@~/.claude/get-shit-done/workflows/execute-plan.md
@~/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/03-visitor-recognition/03-CONTEXT.md
@.planning/spikes/REC-01-RESULT.md
@CLAUDE.md

<interfaces>
<!-- The already-built entry points this plan exercises. No code changes — only calls. -->

GET /api/recognition/health (backend/server.py:290-298) — loads the model, reports memory:
```python
@app.get("/api/recognition/health", dependencies=[Depends(require_dashboard_key)])
def recognition_health():
    if not RECOGNITION_ENABLED:
        return {"enabled": False, "note": "set RECOGNITION_ENABLED=1 to activate"}
    import recognition
    try:
        return {"enabled": True, **recognition.health()}
    except Exception as e:
        raise HTTPException(500, f"recognition unavailable: {e!r}")
```

recognition.health() (backend/recognition.py:129-141) returns:
  {"model": MODEL_NAME, "loaded": bool, "load_seconds": float, "process_rss_mb": int}

The production recognition path REC-05 must exercise (backend/recognition.py):
  extract_embedding(image_bytes) -> (embedding|None, det_score|None)   # MIN_DET_SCORE=0.50
  match_or_create_visitor(db_fetchone, db_execute, embedding, photo_url, ts)
      -> (visitor_id, visitor_name, visit_count, is_new)               # MATCH_THRESHOLD=0.40

Reusable harness: .planning/spikes/rec01_spike.py imports the production recognition module
and labels people by filename prefix (A_1.jpg, B_1.jpg). It is the recommended base for REC-05.

Local env (backend/.env): RECOGNITION_ENABLED currently 0, FACE_MODEL already buffalo_s.
Venv with the pinned face stack (from plan 03-01): backend/.venv.
Fail-open guarantee already in code: create_visit wraps recognition in try/except (server.py:458-491)
so a no-face/model error never blocks the visit — this plan only verifies it, does not add it.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: REC-04 — measure buffalo_s memory locally via /api/recognition/health</name>
  <files>backend/.env, .planning/phases/03-visitor-recognition/REC-04-MEMORY-RESULT.md</files>
  <read_first>
    - backend/server.py:288-298 (recognition_health route — what /api/recognition/health returns and that it needs RECOGNITION_ENABLED=1)
    - backend/recognition.py:24-35,129-141 (get_model + health — what loads and what process_rss_mb measures)
    - backend/.env:28-33 (the RECOGNITION_ENABLED / FACE_MODEL lines to set for local testing)
    - .planning/spikes/REC-01-RESULT.md (the spike's model/threshold context)
  </read_first>
  <action>
    Set `RECOGNITION_ENABLED=1` in backend/.env (FACE_MODEL is already buffalo_s — confirm it). Boot the
    backend from backend/.venv with these vars active and call `GET /api/recognition/health` (it requires
    the dashboard key — send X-Dashboard-Key matching DASHBOARD_PASSWORD, which may be empty locally).
    Capture the returned `model`, `loaded`, `load_seconds`, and `process_rss_mb`. Write
    `.planning/phases/03-visitor-recognition/REC-04-MEMORY-RESULT.md` recording: the local resident memory
    for buffalo_s, the load time, the date, and the implication for Railway (note CLAUDE.md's ~600 MB model
    caution and Railway trial-credit constraint). Leave the prod go/keep-off line as a placeholder to be
    filled by the Railway checkpoint in Task 3. Do NOT change recognition.py or server.py. Do not switch to
    buffalo_l (deferred unless memory headroom demands otherwise — it does not here).
  </action>
  <verify>
    <automated>cd backend && RECOGNITION_ENABLED=1 FACE_MODEL=buffalo_s uvicorn server:app --port 8096 & SVPID=$!; sleep 6; curl -s "http://127.0.0.1:8096/api/recognition/health" -H "X-Dashboard-Key: $DASHBOARD_PASSWORD" | .venv/bin/python -c "import sys,json; d=json.load(sys.stdin); assert d.get('enabled') is True, d; assert isinstance(d.get('process_rss_mb'),(int,float)) and d['process_rss_mb']>0, d; print('[OK] buffalo_s loaded, rss_mb=', d['process_rss_mb'], 'load_s=', d.get('load_seconds'))"; kill $SVPID 2>/dev/null; grep -q '^RECOGNITION_ENABLED=1$' .env && test -f ../.planning/phases/03-visitor-recognition/REC-04-MEMORY-RESULT.md && grep -q 'process_rss_mb' ../.planning/phases/03-visitor-recognition/REC-04-MEMORY-RESULT.md</automated>
  </verify>
  <acceptance_criteria>
    - backend/.env contains `RECOGNITION_ENABLED=1` and `FACE_MODEL=buffalo_s`
    - GET /api/recognition/health returns `enabled: true` and a positive numeric `process_rss_mb`
    - REC-04-MEMORY-RESULT.md exists and records the measured `process_rss_mb` and `load_seconds` for buffalo_s
    - The note references the Railway memory/credit constraint and leaves a prod go/keep-off placeholder
    - recognition.py and server.py are unchanged
  </acceptance_criteria>
  <done>buffalo_s resident memory is measured locally via the health endpoint and recorded; local .env has recognition enabled for testing.</done>
</task>

<task type="auto">
  <name>Task 2: REC-05 — validate accuracy + threshold against the live DB (and confirm fail-open)</name>
  <files>.planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md</files>
  <read_first>
    - .planning/spikes/rec01_spike.py (the reusable harness that imports production recognition and labels by filename prefix)
    - backend/recognition.py:38-126 (extract_embedding + match_or_create_visitor + MATCH_THRESHOLD/ADD_EMBEDDING_BELOW — the exact path to exercise)
    - backend/server.py:458-491 (the create_visit try/except block proving recognition failure never blocks visit creation — REC-05's fail-open assertion)
    - .planning/spikes/REC-01-RESULT.md (the spike numbers REC-05 extends with DB-resident matching)
  </read_first>
  <action>
    Using backend/.venv, run the PRODUCTION recognition path against the live Supabase pgvector tables to
    validate re-recognition. Prefer reusing/extending .planning/spikes/rec01_spike.py rather than posting
    real visits (avoid polluting prod visits — call recognition.extract_embedding then
    recognition.match_or_create_visitor with the real db_fetchone/db_execute helpers, and clean up any test
    visitor rows afterward). For 2-3 people with 2+ photos each: enroll the first photo (creates a visitor),
    then submit a second photo of the SAME person and confirm match_or_create_visitor returns the same
    visitor_id with is_new False (re-recognition); submit a DIFFERENT person's photo and confirm it does NOT
    match an existing visitor at MATCH_THRESHOLD=0.40 (creates a new visitor / no false match). Record the
    observed same-person and different-person cosine similarities and the pass/fail at 0.40. Also assert the
    fail-open property by feeding a no-face image to extract_embedding and confirming it returns (None, None)
    without raising (mirrors what protects create_visit). Write
    `.planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md` with the measured numbers, the
    confirmed MATCH_THRESHOLD (keep 0.40 unless the data clearly argues for ~0.42 — if so, note it but do
    NOT change recognition.py here; flag it as a follow-up), the people/photo count, and the fail-open
    confirmation. Clean up any test visitor/embedding rows you created in the live DB. Do NOT modify
    recognition.py.
  </action>
  <verify>
    <automated>test -f .planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md && grep -q 'MATCH_THRESHOLD' .planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md && grep -qiE 'same-person|same person' .planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md && grep -qiE 'different-person|different person|false match' .planning/phases/03-visitor-recognition/REC-05-ACCURACY-RESULT.md && cd backend && .venv/bin/python -c "import recognition; e,d = recognition.extract_embedding(b'not-an-image'); assert e is None and d is None; print('[OK] no-face/garbage input returns (None,None) without raising — fail-open confirmed')"</automated>
  </verify>
  <acceptance_criteria>
    - REC-05-ACCURACY-RESULT.md exists and records measured same-person and different-person similarities
    - The note states same-person re-recognition succeeded (stable visitor_id, is_new False) at MATCH_THRESHOLD=0.40
    - The note states different people did not false-match at MATCH_THRESHOLD=0.40
    - The note records the people count (2-3), photos per person (2+), and the confirmed/adjusted threshold with rationale
    - extract_embedding(garbage/no-face bytes) returns (None, None) without raising (fail-open verified)
    - Any test visitor/embedding rows created in the live DB were cleaned up; recognition.py unchanged
  </acceptance_criteria>
  <done>Same-person re-recognition and no-false-match are confirmed at MATCH_THRESHOLD=0.40 against the live DB, fail-open is verified, and the results are documented.</done>
</task>

<task type="checkpoint:human-action" gate="blocking">
  <name>Task 3: REC-04 prod confirmation + enablement — set Railway vars and measure prod memory</name>
  <action>
    Operator-only checkpoint — no automation can set a Railway service variable from this plan. Pause for the
    operator to set `RECOGNITION_ENABLED=1` and confirm `FACE_MODEL=buffalo_s` in the Railway service
    Variables, redeploy, then read prod memory back from `/api/recognition/health` and record the prod
    `process_rss_mb` plus the go/keep-off decision in REC-04-MEMORY-RESULT.md. Detailed steps are in
    how-to-verify below.
  </action>
  <what-built>
    Local REC-04 (buffalo_s memory measured via /api/recognition/health) and REC-05 (accuracy/threshold at
    0.40) are validated and documented. backend/.env has RECOGNITION_ENABLED=1 for local testing. The
    recognition pipeline, Telegram name rendering, visitors API, and /response visitor_name (plan 03-02) are
    all in place. What remains is operator-only: setting the production env vars on Railway (no secret/var
    can be written to the deployed service from this plan) and reading the prod memory number back to make
    the go/keep-off call.
  </what-built>
  <how-to-verify>
    1. In the Railway Dashboard for the smart-doorbell service -> Variables, set `RECOGNITION_ENABLED=1` and
       confirm `FACE_MODEL=buffalo_s`. Redeploy if Railway does not auto-redeploy on variable change.
    2. After the deploy is live, call the prod health endpoint:
       `curl -s "https://smart-doorbell-production.up.railway.app/api/recognition/health" -H "X-Dashboard-Key: <DASHBOARD_PASSWORD>"`
       and confirm it returns `"enabled": true` with a `process_rss_mb` value (not a 500 / OOM crash).
    3. Record the prod `process_rss_mb` in REC-04-MEMORY-RESULT.md and fill in the go/keep-off decision:
       - GO if Railway holds the model without OOM and within credit budget -> recognition ON in prod.
       - KEEP-OFF if it OOMs or burns credit too fast -> set RECOGNITION_ENABLED back to 0 on Railway,
         document that prod ships recognition OFF and the demo runs it locally (acceptable per the REC-01 gate).
    4. Optionally press the physical doorbell once (Yussef's bench) to confirm a real visit still creates and
       notifies with recognition active — but full multi-visit hardware testing for REC-05 is Yussef-owned.
  </how-to-verify>
  <resume-signal>Type "approved" once the Railway vars are set and the prod memory + go/keep-off decision are recorded in REC-04-MEMORY-RESULT.md, or describe the OOM/credit issue if keep-off.</resume-signal>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Visit photo → recognition model (in-process) | Untrusted image bytes are decoded and run through the ONNX model; a malformed image or absent face must not crash the request path. |
| Operator → Railway service config | Enabling recognition in prod loads a ~600 MB model into a memory-constrained instance. |
| Face embeddings + names → Supabase (pgvector, at rest) | Biometric-derived data of real people persisted in the database. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-06 | Denial of Service | buffalo_s model load on Railway (OOM) | mitigate | REC-04 measures resident memory before/after enablement; lazy-load keeps the model out of the request path until enabled; if prod can't hold it, RECOGNITION_ENABLED stays 0 (documented keep-off) — no crash loop |
| T-03-07 | Denial of Service | recognition failure inside create_visit | mitigate | Already-implemented try/except (server.py:458-491) makes recognition fail-open; this plan verifies a no-face/garbage input returns (None,None) and never blocks the visit |
| T-03-08 | Information Disclosure | face embeddings + names at rest in Supabase | accept | Lab-prototype bar (CLAUDE.md): embeddings stored unencrypted in pgvector; no liveness/anti-spoofing. Accepted residual risk for this graded submission |
| T-03-09 | Tampering | Railway env var misconfiguration enabling recognition before validation | mitigate | RECOGNITION_ENABLED defaults to 0 (safe-off); enablement is a deliberate operator checkpoint gated on the recorded memory measurement, not an automatic default |
| T-03-SC | Tampering | npm/pip installs (package legitimacy) | accept | No new packages introduced in this plan; the face stack was pinned and import-verified in plan 03-01 from pre-existing trusted PyPI deps |
</threat_model>

<verification>
1. Local: RECOGNITION_ENABLED=1 + buffalo_s -> /api/recognition/health returns enabled:true with a memory number; recorded in REC-04-MEMORY-RESULT.md.
2. Local: production recognition path re-recognizes the same person and rejects false matches at MATCH_THRESHOLD=0.40; recorded in REC-05-ACCURACY-RESULT.md.
3. Local: garbage/no-face bytes -> extract_embedding returns (None,None) without raising (fail-open).
4. Prod (operator checkpoint): Railway vars set, prod /api/recognition/health returns enabled:true without OOM; prod memory + go/keep-off decision recorded.
</verification>

<success_criteria>
- ROADMAP Phase 3 success criteria 2, 4, and 5 are satisfied (recognition active path, fail-open, model within memory budget with accuracy + threshold documented).
- REC-04 and REC-05 result notes exist with measured numbers and the confirmed MATCH_THRESHOLD.
- A clear prod go/keep-off enablement decision is recorded (recognition ON in prod, or OFF with the demo running it locally).
- No recognition.py / server.py / telegram_bot.py runtime code changed in this plan.
- Teammate-owned items remain teammate-owned: REC-03 frontend (dashboard register/name UI) and REC-06 firmware (OLED "Welcome, [Name]") are Yussef's; full multi-visit hardware REC-05 testing is Yussef's bench.
</success_criteria>

<output>
Create `.planning/phases/03-visitor-recognition/03-03-SUMMARY.md` when done.
In the SUMMARY, restate the final prod enablement decision and explicitly mark REC-03 frontend +
REC-06 firmware as teammate-owned (Yussef) so the phase requirement coverage is unambiguous.
</output>
