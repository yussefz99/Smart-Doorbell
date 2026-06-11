# ============================================================
#  Smart Doorbell – Face Recognition
#  Lazy-loaded InsightFace (ONNX) wrapper.
#  The model loads on first use, never at import time, so a
#  memory problem can't take down the doorbell endpoints.
# ============================================================

import os
import threading
import time

# buffalo_s = small models (fits small cloud instances)
# buffalo_l = large models (better accuracy, ~4x the memory)
MODEL_NAME = os.getenv("FACE_MODEL", "buffalo_s").strip() or "buffalo_s"

# Minimum face-detector confidence to accept a face at all
MIN_DET_SCORE = 0.50

_model = None
_lock = threading.Lock()
_load_seconds = None


def get_model():
    """Load the face model once, thread-safe."""
    global _model, _load_seconds
    with _lock:
        if _model is None:
            from insightface.app import FaceAnalysis
            t0 = time.time()
            m = FaceAnalysis(name=MODEL_NAME, providers=["CPUExecutionProvider"])
            m.prepare(ctx_id=-1, det_size=(640, 640))
            _load_seconds = round(time.time() - t0, 1)
            _model = m
    return _model


def extract_embedding(image_bytes: bytes):
    """
    Returns (embedding, det_score) for the most confident face in the
    image, or (None, None) when no usable face is found.
    The embedding is a normalized 512-float list (cosine-ready).
    """
    import cv2
    import numpy as np

    img = cv2.imdecode(np.frombuffer(image_bytes, np.uint8), cv2.IMREAD_COLOR)
    if img is None:
        return None, None

    faces = get_model().get(img)
    if not faces:
        return None, None

    best = max(faces, key=lambda f: f.det_score)
    if float(best.det_score) < MIN_DET_SCORE:
        return None, None

    return best.normed_embedding.tolist(), float(best.det_score)


# ── Visitor matching ──────────────────────────────────────────
# Cosine similarity above this = same person
MATCH_THRESHOLD = 0.40
# Below this (but matched) we also store the new embedding, so each
# visitor accumulates a variety of angles/lighting (max per visitor):
ADD_EMBEDDING_BELOW = 0.60
MAX_EMBEDDINGS_PER_VISITOR = 5


def to_pgvector(embedding) -> str:
    return "[" + ",".join(f"{x:.6f}" for x in embedding) + "]"


def match_or_create_visitor(db_fetchone, db_execute, embedding, photo_url, ts):
    """
    Find the nearest known visitor by cosine similarity, or create a
    new one. Returns (visitor_id, visitor_name, visit_count, is_new).
    """
    vec = to_pgvector(embedding)

    nearest = db_fetchone(
        """SELECT v.id, v.name, v.visit_count,
                  1 - (e.embedding <=> %s::vector) AS similarity
           FROM   visitor_embeddings e
           JOIN   visitors v ON v.id = e.visitor_id
           ORDER  BY e.embedding <=> %s::vector
           LIMIT  1""",
        (vec, vec)
    )

    if nearest and float(nearest["similarity"]) >= MATCH_THRESHOLD:
        visitor_id = nearest["id"]
        row = db_fetchone(
            """UPDATE visitors
               SET visit_count = visit_count + 1, last_seen = %s
               WHERE id = %s
               RETURNING visit_count""",
            (ts, visitor_id)
        )
        # Store an extra embedding when this angle looks new
        if float(nearest["similarity"]) < ADD_EMBEDDING_BELOW:
            n = db_fetchone(
                "SELECT COUNT(*) AS n FROM visitor_embeddings WHERE visitor_id=%s",
                (visitor_id,)
            )["n"]
            if n < MAX_EMBEDDINGS_PER_VISITOR:
                db_execute(
                    "INSERT INTO visitor_embeddings (visitor_id, embedding) VALUES (%s, %s::vector)",
                    (visitor_id, vec)
                )
        return visitor_id, nearest["name"], row["visit_count"], False

    # New visitor
    row = db_fetchone(
        """INSERT INTO visitors (photo_url, first_seen, last_seen, visit_count)
           VALUES (%s, %s, %s, 1)
           RETURNING id""",
        (photo_url, ts, ts)
    )
    visitor_id = row["id"]
    db_execute(
        "INSERT INTO visitor_embeddings (visitor_id, embedding) VALUES (%s, %s::vector)",
        (visitor_id, vec)
    )
    return visitor_id, None, 1, True


def health() -> dict:
    """Load the model and report memory + timing — used to verify the
    deployment environment can actually run recognition."""
    import psutil

    get_model()
    rss_mb = psutil.Process().memory_info().rss / (1024 * 1024)
    return {
        "model":           MODEL_NAME,
        "loaded":          _model is not None,
        "load_seconds":    _load_seconds,
        "process_rss_mb":  round(rss_mb),
    }
