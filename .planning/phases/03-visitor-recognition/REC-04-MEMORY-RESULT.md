# REC-04 — Memory Measurement (buffalo_s)

**Date:** 2026-06-27
**Model:** buffalo_s · `MIN_DET_SCORE=0.50` · `MATCH_THRESHOLD=0.40`

## Local measurement (via GET /api/recognition/health)
```json
{"enabled": true, "model": "buffalo_s", "loaded": true, "load_seconds": 0.4, "process_rss_mb": 672}
```
- **Resident memory with buffalo_s loaded: `process_rss_mb` = 672 MB.**
- `load_seconds` = 0.4 (model preloaded by the lifespan warmup thread when `RECOGNITION_ENABLED=1`,
  so the request path never pays the load cost; first cold load during the spike was longer).
- Measured on the project venv (Python 3.11) against the live Supabase.

## Railway implication
- CLAUDE.md flags the recognition model at ~600 MB and warns about Railway trial credit (~$5/30 days).
  672 MB matches that estimate. A Railway instance needs **>1 GB RAM headroom** to hold this process
  safely (FastAPI + uvicorn + the 672 MB model + request overhead).
- `buffalo_l` is NOT used (deferred) — it is ~4× the memory and would not fit the budget.
- Recognition is lazy-loaded behind `RECOGNITION_ENABLED`, so prod stays at the lightweight baseline
  until the operator explicitly enables it.

## Production go / keep-off decision
**PENDING — operator checkpoint (Task 3).** Set `RECOGNITION_ENABLED=1` + `FACE_MODEL=buffalo_s` on
Railway, redeploy, read `process_rss_mb` from the prod `/api/recognition/health`, then decide:
- **GO** if Railway holds 672 MB without OOM and within credit budget → recognition ON in prod.
- **KEEP-OFF** if it OOMs or burns credit → set `RECOGNITION_ENABLED=0` on Railway; prod ships
  recognition OFF and the demo runs it locally (acceptable per the REC-01 gate philosophy).

_Prod process_rss_mb: __________  ·  Decision: __________ (fill at checkpoint)_
