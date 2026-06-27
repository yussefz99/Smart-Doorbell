---
phase: 03-visitor-recognition
plan: 01
requirements: [REC-02]
status: complete
---

# Plan 03-01: Pin Recognition Deps — Summary

## What was built
Pinned the face-recognition stack in `backend/requirements.txt` to exact versions (was `>=`).

## Changes
- `backend/requirements.txt` — face block now:
  - `insightface==1.0.1`, `onnxruntime==1.27.0`, `opencv-python-headless==4.13.0.92`,
    `psutil==7.2.2`, `numpy==2.4.6`.
  - Non-face deps (fastapi, uvicorn, python-multipart, python-dotenv, httpx, psycopg2-binary) unchanged.

## numpy decision
Pinned `numpy==2.4.6` — it's already what resolved cleanly in the venv with insightface 1.0.1,
and the import check passes, so pinning it makes deploys fully reproducible. No resolver conflict.

## Verification
- requirements.txt pins present, no `>=` remains on the face stack, `fastapi==0.111.0` intact.
- `backend/.venv` reports exactly 1.0.1 / 1.27.0 / 4.13.0.92 / 7.2.2; `import recognition` succeeds
  (the insightface/onnxruntime/cv2 chain resolves). Model not loaded (kept fast/light).

## Requirements
- REC-02 finish (exact dep pins) — met. The recognition pipeline code was untouched.

## Note on Railway
These versions have cp313 wheels (Railway Python 3.13); insightface builds from source via the
apt packages already in railpack.json. The exact pins guarantee Railway resolves the validated set.

## Commit
- `ae8dbef` feat(03-01): pin face-recognition deps to exact versions (REC-02)

## Self-Check: PASSED
