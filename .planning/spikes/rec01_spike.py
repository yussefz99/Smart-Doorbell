#!/usr/bin/env python3
# ============================================================
#  REC-01 Feasibility Spike — Visitor Recognition go/no-go
#
#  Runs the PRODUCTION recognition path (backend/recognition.py)
#  over a folder of real ESP32-CAM doorbell photos and answers:
#    1. Is a face actually detected on this camera? (det_score)
#    2. Does the SAME person score clearly higher similarity than
#       DIFFERENT people? (the separation that makes matching work)
#
#  Labelling is by filename prefix before the first '_' or '-':
#      A_1.jpg  A_2.jpg  A_3.jpg   -> person "A"
#      B_1.jpg  B_2.jpg            -> person "B"
#
#  Usage:
#      cd backend
#      .venv/bin/python ../.planning/spikes/rec01_spike.py <photo_dir>
#
#  Faithful to production: imports recognition.extract_embedding,
#  so MIN_DET_SCORE / MATCH_THRESHOLD match what the server uses.
# ============================================================

import os
import sys
import glob
import itertools
import warnings

warnings.filterwarnings("ignore")

# Import the real backend module so the spike mirrors production exactly.
HERE = os.path.dirname(os.path.abspath(__file__))
BACKEND = os.path.normpath(os.path.join(HERE, "..", "..", "backend"))
sys.path.insert(0, BACKEND)

import numpy as np  # noqa: E402
import recognition  # noqa: E402


def label_of(filename: str) -> str:
    base = os.path.basename(filename)
    name = os.path.splitext(base)[0]
    for sep in ("_", "-"):
        if sep in name:
            return name.split(sep)[0]
    return name


def cosine(a, b) -> float:
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    # recognition returns L2-normalized embeddings, so dot == cosine.
    return float(np.dot(a, b))


def main():
    if len(sys.argv) < 2:
        print("usage: rec01_spike.py <photo_dir>")
        sys.exit(2)

    photo_dir = sys.argv[1]
    paths = []
    for ext in ("*.jpg", "*.jpeg", "*.png", "*.JPG", "*.JPEG", "*.PNG"):
        paths.extend(glob.glob(os.path.join(photo_dir, ext)))
    paths = sorted(set(paths))

    if not paths:
        print(f"[ERROR] no images found in {photo_dir}")
        sys.exit(1)

    print(f"model        : {recognition.MODEL_NAME}")
    print(f"MIN_DET_SCORE: {recognition.MIN_DET_SCORE}")
    print(f"MATCH_THRESH : {recognition.MATCH_THRESHOLD}")
    print(f"photos       : {len(paths)} in {photo_dir}")
    print("=" * 64)

    # ── 1. Detection + embedding per photo ────────────────────
    items = []  # (filename, label, embedding, det_score)
    print(f"{'photo':<24}{'person':<8}{'face?':<7}{'det_score'}")
    print("-" * 64)
    for p in paths:
        with open(p, "rb") as f:
            data = f.read()
        emb, det = recognition.extract_embedding(data)
        lab = label_of(p)
        ok = "YES" if emb is not None else "no"
        det_s = f"{det:.3f}" if det is not None else "  -"
        print(f"{os.path.basename(p):<24}{lab:<8}{ok:<7}{det_s}")
        if emb is not None:
            items.append((os.path.basename(p), lab, emb, det))

    detected = len(items)
    print("-" * 64)
    print(f"faces detected: {detected}/{len(paths)}")
    if detected < 2:
        print("\n[NO-GO] Need >=2 detected faces to compare. "
              "If faces aren't detected at all, the camera framing/quality "
              "is the blocker — recapture closer / better lit.")
        sys.exit(0)

    # ── 2. Pairwise similarity, split same vs different person ─
    same, diff = [], []
    print("\nPairwise cosine similarity:")
    print(f"{'pair':<36}{'sim':>8}  kind")
    print("-" * 64)
    for (fa, la, ea, _), (fb, lb, eb, _) in itertools.combinations(items, 2):
        s = cosine(ea, eb)
        kind = "SAME" if la == lb else "diff"
        (same if la == lb else diff).append(s)
        print(f"{fa+' vs '+fb:<36}{s:>8.3f}  {kind}")

    print("=" * 64)

    def stats(xs):
        if not xs:
            return "  (none)"
        return f"n={len(xs):<3} min={min(xs):.3f}  mean={sum(xs)/len(xs):.3f}  max={max(xs):.3f}"

    print(f"SAME person : {stats(same)}")
    print(f"DIFF person : {stats(diff)}")

    # ── 3. Verdict ────────────────────────────────────────────
    print("=" * 64)
    if not same:
        print("[INCONCLUSIVE] No same-person pairs. Capture >=2 photos of "
              "at least one person (filename prefix must match, e.g. A_1, A_2).")
        sys.exit(0)
    if not diff:
        print("[INCONCLUSIVE] No different-person pairs. Capture a 2nd person.")
        sys.exit(0)

    min_same = min(same)
    max_diff = max(diff)
    thr = recognition.MATCH_THRESHOLD
    gap = min_same - max_diff

    # How would the production threshold actually classify these pairs?
    same_ok = sum(1 for s in same if s >= thr)
    diff_bad = sum(1 for s in diff if s >= thr)  # false matches

    print(f"separation gap (min_same - max_diff): {gap:+.3f}")
    print(f"at threshold {thr}: {same_ok}/{len(same)} same-pairs match, "
          f"{diff_bad}/{len(diff)} diff-pairs FALSE-match")
    print("=" * 64)

    clean_separation = min_same > max_diff
    threshold_works = same_ok == len(same) and diff_bad == 0

    if clean_separation and threshold_works:
        print("[GO] Same-person clearly separates from different-person, and "
              f"threshold {thr} classifies every pair correctly. Recognition "
              "is feasible on these photos. (Consider tuning threshold to sit "
              "in the gap.)")
    elif clean_separation:
        suggested = round((min_same + max_diff) / 2, 2)
        print(f"[GO*] Clean separation (gap {gap:+.3f}) but current threshold "
              f"{thr} mis-classifies some pairs. Recognition is feasible — "
              f"retune MATCH_THRESHOLD to ~{suggested} (midpoint of the gap).")
    else:
        print(f"[NO-GO] Same-person ({min_same:.3f} min) does NOT clearly beat "
              f"different-person ({max_diff:.3f} max). No threshold separates "
              "them cleanly on these photos. Recognition stays OFF unless "
              "framing/lighting/model (try buffalo_l) improves the signal.")


if __name__ == "__main__":
    main()
