# Phase 3: Visitor Recognition - Context

**Gathered:** 2026-06-27
**Status:** Ready for planning
**Source:** Conversation + codebase audit (Rami, backend owner). REC-01 spike already run.

<domain>
## Phase Boundary

Turn on returning-visitor recognition end-to-end: each visit gets a face embedding matched
against known visitors (pgvector), recognized names appear in the dashboard, Telegram, and on
the door OLED. **Critical:** most of the backend pipeline is ALREADY IMPLEMENTED (audited below) —
this phase is mostly *finishing, enabling, and validating*, not building from scratch.

**Gate already cleared:** REC-01 spike = **GO** (see `.planning/spikes/REC-01-RESULT.md`). On real
ESP32-CAM photos: 6/6 faces detected, same-person min 0.638 vs different-person max 0.211 (gap +0.427),
threshold 0.40 classifies every pair correctly. Model `buffalo_s`. Recognition is feasible.

**Ownership (see WORK-SPLIT.md):**
- **Rami (this execution, backend):** finish REC-02 (pin deps), REC-06 backend (`visitor_name` in
  `/response`), REC-04 (memory check), REC-05 backend (multi-person enrollment test + document
  accuracy/threshold), and the enablement decision.
- **Yussef (teammate, frontend/firmware):** REC-03 frontend (dashboard register/name UI) and REC-06
  firmware (OLED "Welcome, [Name]" — already partially done in commit 1f1e94a "visitor greeting by name").
</domain>

<decisions>
## ALREADY IMPLEMENTED — DO NOT RE-PLAN (verified in code 2026-06-27)

- **REC-01:** spike done, GO recorded.
- **REC-02 core:** `create_visit` (`server.py:458-491`) already extracts the embedding via
  `asyncio.to_thread(recognition.extract_embedding, ...)`, calls `recognition.match_or_create_visitor`,
  attaches `visitor_id`/`visitor_name`, wraps it in try/except so a recognition failure NEVER blocks
  visit creation, and inserts `visitor_id` into `visits`. `recognition.py` is fully built
  (extract_embedding, pgvector cosine match, multi-embedding storage, health).
- **REC-03 backend (Telegram):** `tg.send_visit_notification` (`telegram_bot.py:47-58`) already renders
  `"🔔 {name} is at the door!"` when a visitor is recognized.
- **REC-03 backend (API):** `GET /api/visitors`, `PATCH /api/visitors/{id}` (rename), `DELETE
  /api/visitors/{id}` already exist (`server.py:386-403`).
- **REC-03 dashboard data:** `get_visits` LEFT JOINs `visitor_name` (`server.py:305`), `row_to_visit`
  includes `visitor_id`/`visitor_name` (`server.py:270-271`), broadcast carries them.
- **REC-04 tooling:** `GET /api/recognition/health` already exists (`server.py:290`) — loads the model
  and reports memory.
- **REL-01 tables:** `visitors` + `visitor_embeddings(vector(512))` + `visits.visitor_id` exist (Phase 2).

## REMAINING BACKEND WORK (Rami — what this phase actually does)

### REC-02 finish — pin recognition deps to exact versions
- `backend/requirements.txt` currently uses `>=` for the face stack. Pin to the exact versions that
  install cleanly (locally verified on py3.11; these have cp313 wheels for Railway too):
  `insightface==1.0.1`, `onnxruntime==1.27.0`, `opencv-python-headless==4.13.0.92`, `psutil==7.2.2`.
  Consider also pinning `numpy` (insightface is numpy-sensitive; 2.4.6 worked locally) — Claude's discretion.

### REC-06 backend — add visitor_name to the device reply poll (handshake #2)
- `GET /api/visits/{id}/response` (`server.py:519-523`) currently returns `{id, response}`. Add
  `visitor_name` so the firmware OLED can greet by name. Agreed shape: `{ "id", "response", "visitor_name" }`.
  Keep the `require_device_key` dependency added in Phase 2.

### REC-04 — confirm model fits the hosting memory budget (no OOM)
- DECIDED: `FACE_MODEL=buffalo_s` (smaller; spike passed with it). Do NOT use buffalo_l unless memory allows.
- Verify via `GET /api/recognition/health` with `RECOGNITION_ENABLED=1` — locally first, then on Railway
  after deploy — and record the resident memory. If Railway's plan can't hold it, document the limit and
  keep recognition OFF in prod (acceptable fallback, like the REC-01 gate).

### REC-05 — accuracy/threshold validation (Rami backend + Yussef hardware)
- Backend-testable now: run the real recognition path (extract → match_or_create_visitor) against the
  live DB over the spike photos / a few enrolled people, confirm same-person re-recognition and
  no false matches at `MATCH_THRESHOLD=0.40`. Document measured accuracy + the chosen threshold.
- Full end-to-end (multiple real visits across 2–3 people via the device) is run on Yussef's bench.

### Enablement
- `RECOGNITION_ENABLED=1` and `FACE_MODEL=buffalo_s` are set on Railway to activate (operator action).
  Locally, set them in `backend/.env` to test. Keeping them unset leaves recognition OFF (safe default).

### Tuning (decided, no change needed unless REC-05 says otherwise)
- `MATCH_THRESHOLD = 0.40` — validated by the spike (clean separation). Leave as-is unless REC-05 data
  argues for a value nearer the gap midpoint (~0.42).

## TEAMMATE-OWNED (cover for requirement completeness, do NOT implement here)
- **REC-03 frontend:** dashboard UI to register/name a visitor (calls the existing `PATCH /api/visitors/{id}`).
  Yussef. Backend contract already provided.
- **REC-06 firmware:** OLED "Welcome, [Name]" using the new `/response` `visitor_name`. Yussef
  (partially done, commit 1f1e94a). Backend contract = `visitor_name` in the `/response` payload.

## Claude's Discretion
- Whether to pin numpy and to what version.
- Exact wording/format of the accuracy/threshold doc (a short REC-05 result note in the phase dir).
- Whether REC-05's backend validation reuses the spike script or a new small harness.
</decisions>

<canonical_refs>
## Canonical References

### Backend (the files in play)
- `backend/server.py` — `create_visit` recognition block (458-491, already done), `get_visit_response`
  (519-523, ADD visitor_name), `/api/recognition/health` (290), `/api/visitors` API (386-403), `init_db` (Phase 2)
- `backend/recognition.py` — model, extract_embedding, match_or_create_visitor, health; `MATCH_THRESHOLD=0.40`,
  `MIN_DET_SCORE=0.50`, `MODEL_NAME` from `FACE_MODEL` (default buffalo_s)
- `backend/telegram_bot.py` — `send_visit_notification` already renders the recognized name
- `backend/requirements.txt` — pin the face stack to exact versions (REC-02)
- `railpack.json` — apt packages already present for OpenCV/InsightFace on Railway

### Planning / spike
- `.planning/spikes/REC-01-RESULT.md` — the GO decision + numbers + threshold validation
- `.planning/spikes/rec01_spike.py` — reusable for REC-05 backend validation
- `.planning/REQUIREMENTS.md` — REC-01…06 exact text
- `.planning/ROADMAP.md` — Phase 3 success criteria (incl. the REC-01 GATE criterion, already met)
- `.planning/WORK-SPLIT.md` — Rami/Yussef ownership + handshakes #2 (/response payload) and #3 (visitors API)
- `CLAUDE.md` — recognition lazy-loaded behind RECOGNITION_ENABLED; ~600 MB model / Railway credit caution
</canonical_refs>

<specifics>
## Specific Ideas
- Verification mostly runs locally with `RECOGNITION_ENABLED=1 FACE_MODEL=buffalo_s` against the live
  Supabase (now has the pgvector tables). Use the venv (`backend/.venv`) which already has the face stack.
- For REC-05 backend, enrolling = POST a photo through the recognition path (creates a visitor), then
  re-submit another photo of the same person and confirm it matches (visitor_id stable, name carried).
  Avoid polluting prod visits unnecessarily — prefer calling recognition functions directly or clean up
  test visitors after.
</specifics>

<deferred>
## Deferred Ideas
- buffalo_l / higher-accuracy model — only if buffalo_s memory headroom and accuracy demand it.
- Liveness / anti-spoofing, multi-face handling — beyond the lab-prototype bar.
- Connection pooling, cert pinning — deferred per CLAUDE.md (unchanged).
- If REC-04 shows Railway can't hold the model: recognition ships OFF in prod (documented), demo runs
  it locally — acceptable per the REC-01 gate philosophy.
</deferred>

---

*Phase: 03-visitor-recognition*
*Context gathered: 2026-06-27 from conversation + codebase audit (much of the pipeline pre-exists)*
