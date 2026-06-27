# REC-01 Feasibility Spike — Result

**Decision:** ✅ **GO** — visitor recognition is feasible on the real ESP32-CAM.
**Date:** 2026-06-27
**Run by:** Rami (backend), photos captured with Yussef on the live device.

## Setup
- Script: `.planning/spikes/rec01_spike.py` (imports the production `backend/recognition.py`,
  so thresholds match the server exactly).
- Model: `buffalo_s` · `MIN_DET_SCORE = 0.50` · `MATCH_THRESHOLD = 0.40`.
- Data: 6 real doorbell photos, 3 people × 2 frontal shots each
  (A = visits 49/50, B = 55/56, C = 57/58), pulled from Supabase Storage.

## Results
- **Faces detected: 6/6** (det_score 0.774–0.854 — all clear of the 0.50 floor).
- **Same-person cosine:** min 0.638, mean 0.747, max 0.839 (n=3 pairs).
- **Different-person cosine:** min −0.086, mean 0.073, max 0.211 (n=12 pairs).
- **Separation gap (min_same − max_diff): +0.427.**
- At threshold 0.40: **3/3 same-pairs match, 0/12 different-pairs false-match.**

## Verdict
GO. Same-person similarity separates cleanly from different-person, and the current
production threshold `MATCH_THRESHOLD = 0.40` classifies every pair correctly with
healthy margin on both sides (0.40 − 0.211 = 0.19 below; 0.638 − 0.40 = 0.24 above).
No threshold change required for now; 0.40 is well-centred in the gap.

## Caveats / follow-ups
- Same-person shots were taken seconds apart (near-identical frames), so the same-person
  numbers are optimistic vs. real visits across days/angles/lighting. The weakest same-pair
  (C, 0.638) is the most realistic signal and still far above any different-pair.
- Validate robustness with genuinely varied captures (different sessions/angles) during
  **REC-05** before trusting in production.
- Faces in `.planning/spikes/photos/` and the staging dir are gitignored / deleted — not committed.

## Implication for the plan
- Phase 3 (Visitor Recognition) proceeds. Gate cleared.
- Prerequisite still standing: **Phase 2 / REL-01** must run first to create the
  `visitors` + `visitor_embeddings` tables + pgvector that the matching path needs.
