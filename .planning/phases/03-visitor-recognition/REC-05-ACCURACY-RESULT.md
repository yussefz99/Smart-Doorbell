# REC-05 — Accuracy & Threshold Validation

**Date:** 2026-06-27
**Model:** buffalo_s · **MATCH_THRESHOLD = 0.40** (confirmed, unchanged) · `MIN_DET_SCORE=0.50`
**Method:** production recognition path (`recognition.extract_embedding` → `match_or_create_visitor`)
run against the **live Supabase pgvector tables**. Data: 3 people × 2 real ESP32-CAM photos
(A=visits 49/50, B=55/56, C=57/58). Test visitor rows were created in the live DB and deleted
afterward (DB returned to 0 visitor rows).

## Separation (recomputed via the live path)
- photos: 6 · faces detected: 6/6
- **same-person cosine:** min 0.638, mean 0.747, max 0.839
- **different-person cosine:** min −0.086, mean 0.073, max 0.211
- gap (min_same − max_diff) = **+0.427** — matches the REC-01 spike.

## Re-recognition against the live DB (match_or_create_visitor)
| Photo | Result | Expected | Pass |
|-------|--------|----------|------|
| A_1 | new → visitor 3 | new | ✓ |
| A_2 | **matched → visitor 3, is_new=False** | match A_1 (same id) | ✓ |
| B_1 | new → visitor 4 | new, ≠ A | ✓ |
| C_1 | new → visitor 5 | new, ≠ A,B | ✓ |

- **Same-person re-recognition: PASS** — A_2 returned A_1's `visitor_id` with `is_new=False`.
- **No false match: PASS** — B and C created distinct visitors; neither matched A at threshold 0.40.
- **Fail-open: PASS** — `extract_embedding(b"not-an-image")` → `(None, None)` with no exception
  (this is what protects `create_visit`'s try/except so recognition never blocks a visit).

## Threshold decision
**Keep MATCH_THRESHOLD = 0.40.** It sits well inside the gap (0.21 → 0.64): every same-pair
matches, zero different-pairs false-match. No change to `recognition.py`. (Midpoint ~0.42 would
also work; 0.40 already has healthy margin both ways, so no change is warranted.)

## Caveat
Same-person photos were taken seconds apart (high similarity). Genuinely varied captures across
different visits/days are part of the teammate-owned multi-visit hardware test (Yussef's bench).
The clean separation here + the spike give high confidence the gate holds.

## Requirement
REC-05 (backend) — met: accuracy validated, threshold documented, fail-open verified.
