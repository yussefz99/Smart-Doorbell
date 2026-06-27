---
phase: 03-visitor-recognition
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - backend/requirements.txt
autonomous: true
requirements: [REC-02]
must_haves:
  truths:
    - "The face-recognition stack in requirements.txt is pinned to exact versions (== not >=)"
    - "A fresh install from requirements.txt resolves the face stack to the validated versions"
    - "The non-face dependencies (fastapi, uvicorn, httpx, psycopg2-binary, etc.) are left exactly as they were"
  artifacts:
    - path: "backend/requirements.txt"
      provides: "Exact version pins for insightface, onnxruntime, opencv-python-headless, psutil (and numpy at Claude's discretion)"
      contains: "insightface=="
  key_links:
    - from: "backend/requirements.txt"
      to: "Railway build (cp313 wheels) + backend/.venv (cp311 verified)"
      via: "pip install -r requirements.txt resolving to identical versions on every deploy"
      pattern: "insightface==1\\.0\\.1"
---

<objective>
Pin the face-recognition dependency stack in `backend/requirements.txt` to exact versions,
replacing the current `>=` ranges. This is the only remaining piece of REC-02 — the
recognition pipeline itself (embedding extraction, pgvector matching, fail-open visit
creation) is already fully implemented in `backend/recognition.py` and `backend/server.py`
and must NOT be touched here.

Purpose: Reproducible deploys. Unpinned `>=` ranges let Railway pull a different
InsightFace/ONNXRuntime/OpenCV build than the one validated in the REC-01 spike, which
could change accuracy or break the cp313 wheel resolution on Railway. Exact pins are also
a supply-chain mitigation (T-03-01) — they freeze the resolved package set against silent
upstream substitution.
Output: `backend/requirements.txt` with `insightface==1.0.1`, `onnxruntime==1.27.0`,
`opencv-python-headless==4.13.0.92`, `psutil==7.2.2` (and an optional `numpy` pin).

DO NOT: change any non-face dependency, edit `recognition.py`, `server.py`, or any
runtime code. This plan is requirements.txt only.
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
<!-- The file to edit, as it is today. Edit ONLY the face block below the comment. -->

Current backend/requirements.txt (the face block uses >= and must become ==):
```
fastapi==0.111.0
uvicorn[standard]==0.29.0
python-multipart==0.0.9
python-dotenv==1.0.0
httpx==0.27.0
psycopg2-binary==2.9.10

# Face recognition (V3)
insightface>=1.0
onnxruntime>=1.20
opencv-python-headless>=4.11
psutil>=6
```

Target exact versions (decided in 03-CONTEXT.md; cp313 wheels exist for Railway, cp311 verified locally):
  insightface==1.0.1
  onnxruntime==1.27.0
  opencv-python-headless==4.13.0.92
  psutil==7.2.2
numpy pin = Claude's discretion (insightface is numpy-sensitive; 2.4.6 worked locally — add `numpy==2.4.6`
only if it resolves cleanly in the venv check; otherwise leave numpy unpinned and note why in the SUMMARY).
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Pin the face-recognition stack to exact versions in requirements.txt</name>
  <files>backend/requirements.txt</files>
  <read_first>
    - backend/requirements.txt (the full current file — only the four `>=` lines under "# Face recognition (V3)" change)
    - .planning/phases/03-visitor-recognition/03-CONTEXT.md (the "REC-02 finish — pin recognition deps" section listing the exact versions)
    - .planning/spikes/REC-01-RESULT.md (confirms buffalo_s on this stack passed the gate — the versions being pinned are the validated ones)
  </read_first>
  <action>
    In backend/requirements.txt, replace the four lines under the "# Face recognition (V3)" comment
    so each uses an exact `==` pin instead of `>=`: insightface to 1.0.1, onnxruntime to 1.27.0,
    opencv-python-headless to 4.13.0.92, psutil to 7.2.2. Keep the "# Face recognition (V3)" comment.
    Leave every non-face line (fastapi, uvicorn[standard], python-multipart, python-dotenv, httpx,
    psycopg2-binary) byte-for-byte unchanged. For numpy (Claude's discretion): only add a `numpy==2.4.6`
    pin if Task 2's venv install confirms it resolves cleanly with insightface; if adding it causes a
    resolver conflict, omit it and record that decision in the SUMMARY. Do not edit any other file.
  </action>
  <verify>
    <automated>cd backend && grep -q '^insightface==1.0.1$' requirements.txt && grep -q '^onnxruntime==1.27.0$' requirements.txt && grep -q '^opencv-python-headless==4.13.0.92$' requirements.txt && grep -q '^psutil==7.2.2$' requirements.txt && ! grep -E '^(insightface|onnxruntime|opencv-python-headless|psutil)>=' requirements.txt && grep -q '^fastapi==0.111.0$' requirements.txt</automated>
  </verify>
  <acceptance_criteria>
    - requirements.txt contains the line `insightface==1.0.1`
    - requirements.txt contains the line `onnxruntime==1.27.0`
    - requirements.txt contains the line `opencv-python-headless==4.13.0.92`
    - requirements.txt contains the line `psutil==7.2.2`
    - No face-stack line still uses `>=` (grep for `insightface>=`, `onnxruntime>=`, `opencv-python-headless>=`, `psutil>=` returns nothing)
    - The six non-face dependency lines are unchanged (fastapi==0.111.0 still present)
  </acceptance_criteria>
  <done>The four face-stack dependencies are pinned to exact versions; all other requirements lines are untouched.</done>
</task>

<task type="auto">
  <name>Task 2: Verify the pinned stack installs and imports cleanly in the project venv</name>
  <files>backend/requirements.txt</files>
  <read_first>
    - backend/requirements.txt (the now-pinned file from Task 1)
    - backend/recognition.py:24-35 (get_model imports `from insightface.app import FaceAnalysis` — the import path that must resolve)
    - .planning/phases/03-visitor-recognition/03-CONTEXT.md (the venv `backend/.venv` already has the face stack installed)
  </read_first>
  <action>
    Using the existing project venv (`backend/.venv`), install the pinned requirements and confirm the
    versions actually resolve and import. Run `backend/.venv/bin/pip install -r backend/requirements.txt`
    and then import the recognition entry points: `insightface`, `onnxruntime`, `cv2`, `psutil`, and the
    project's own `recognition` module (which lazily imports `insightface.app.FaceAnalysis`). Capture the
    resolved version of each pinned package (`pip show` or `importlib.metadata.version`) and confirm they
    equal the pinned values. If numpy was pinned in Task 1, confirm pip did not downgrade/conflict; if it
    did, revert the numpy pin per Task 1's discretion note. Do NOT load the actual model here (no
    `get_model()` call) — only confirm imports succeed, so this stays fast and memory-light.
  </action>
  <verify>
    <automated>cd backend && .venv/bin/python -c "import importlib.metadata as m; assert m.version('insightface')=='1.0.1', m.version('insightface'); assert m.version('onnxruntime')=='1.27.0', m.version('onnxruntime'); assert m.version('opencv-python-headless')=='4.13.0.92', m.version('opencv-python-headless'); assert m.version('psutil')=='7.2.2', m.version('psutil'); import insightface, onnxruntime, cv2, psutil, recognition; print('[OK] pinned stack imports and versions match')"</automated>
  </verify>
  <acceptance_criteria>
    - `importlib.metadata.version('insightface')` returns `1.0.1`
    - `importlib.metadata.version('onnxruntime')` returns `1.27.0`
    - `importlib.metadata.version('opencv-python-headless')` returns `4.13.0.92`
    - `importlib.metadata.version('psutil')` returns `7.2.2`
    - `import recognition` succeeds (no ImportError from the insightface/onnxruntime/cv2 chain)
    - If numpy was pinned, pip reports no dependency conflict; otherwise the numpy pin is removed and the SUMMARY records why
  </acceptance_criteria>
  <done>The pinned face stack installs into backend/.venv with the exact versions and the recognition module imports without error.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| PyPI → Railway/local build | Dependency resolution crosses an untrusted supply chain; `>=` ranges allow a different build to be pulled on any future deploy. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-01 | Tampering | insightface / onnxruntime / opencv-python-headless / psutil install | mitigate | Pin to exact `==` versions validated in the REC-01 spike, freezing the resolved package set against silent upstream substitution between deploys |
| T-03-02 | Denial of Service | onnxruntime/opencv build mismatch on Railway | mitigate | Exact pins guarantee Railway resolves the same cp313 wheels validated locally, avoiding an unbuildable image or an OOM-prone larger build |
| T-03-SC | Tampering | npm/pip installs (package legitimacy) | accept | Four face packages are mainstream, well-known PyPI projects already in production use on this codebase (pre-existing `>=` lines, REC-01 spike ran on them). No new packages introduced — only version tightening of already-trusted deps; not [ASSUMED]/[SUS]/[SLOP] |
</threat_model>

<verification>
1. Inspect backend/requirements.txt — the face block uses `==` exact pins, non-face lines unchanged.
2. `backend/.venv/bin/pip install -r backend/requirements.txt` resolves without conflict.
3. The four pinned packages report exactly the pinned versions and `import recognition` succeeds.
</verification>

<success_criteria>
- REC-02's remaining work (exact dep pins) is complete; the recognition pipeline code is untouched.
- A fresh `pip install -r requirements.txt` resolves the face stack to the REC-01-validated versions on both the local venv (cp311) and Railway (cp313).
- No runtime code (recognition.py, server.py, telegram_bot.py) and no non-face dependency changed.
</success_criteria>

<output>
Create `.planning/phases/03-visitor-recognition/03-01-SUMMARY.md` when done.
Record the final numpy decision (pinned to which version, or left unpinned and why).
</output>
