# ============================================================
#  Smart Doorbell – Backend Server
#  Stack : FastAPI + PostgreSQL (Supabase, plain psycopg2)
#  Run   : uvicorn server:app --reload --port 8001
# ============================================================

import os
import time
from datetime import datetime, timezone, date
from contextlib import asynccontextmanager
from typing import Optional

import psycopg2
from psycopg2.extras import RealDictCursor

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, UploadFile, File, Form, Request, Header, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import JSONResponse, FileResponse
from pydantic import BaseModel
from dotenv import load_dotenv

load_dotenv()

import telegram_bot as tg

# ── Paths ────────────────────────────────────────────────────
BASE_DIR    = os.path.dirname(os.path.abspath(__file__))
PHOTOS_DIR  = os.path.join(BASE_DIR, "photos")
os.makedirs(PHOTOS_DIR, exist_ok=True)

DATABASE_URL = os.environ["DATABASE_URL"]

# Supabase Storage — durable photo hosting (Railway's disk is ephemeral).
# When unset, photos fall back to the local photos/ directory.
SUPABASE_URL         = os.getenv("SUPABASE_URL", "").strip().rstrip("/")
SUPABASE_SERVICE_KEY = os.getenv("SUPABASE_SERVICE_KEY", "").strip()
STORAGE_BUCKET       = "photos"

# Dashboard access password. Protects the READ endpoints the dashboard
# uses; device endpoints (visit POST, heartbeat) and the Telegram
# webhook stay open so the doorbell keeps working.
# Empty value = gate disabled (e.g. local development).
DASHBOARD_PASSWORD = os.getenv("DASHBOARD_PASSWORD", "").strip()

# Face recognition toggle — "0" disables matching entirely
RECOGNITION_ENABLED = os.getenv("RECOGNITION_ENABLED", "1").strip() != "0"

def require_dashboard_key(x_dashboard_key: str = Header(default="")):
    if DASHBOARD_PASSWORD and x_dashboard_key != DASHBOARD_PASSWORD:
        raise HTTPException(401, "Invalid dashboard password")

SERVER_START_TIME = time.time()

# ── Database ─────────────────────────────────────────────────
def get_db():
    return psycopg2.connect(DATABASE_URL, cursor_factory=RealDictCursor)

def db_fetchall(sql, params=None):
    with get_db() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, params or ())
            return cur.fetchall()

def db_fetchone(sql, params=None):
    with get_db() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, params or ())
            return cur.fetchone()

def db_execute(sql, params=None):
    """Run a statement, return affected row count."""
    with get_db() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, params or ())
            return cur.rowcount

def init_db():
    with get_db() as conn:
        with conn.cursor() as cur:
            cur.execute("""
                CREATE TABLE IF NOT EXISTS visits (
                    id                  SERIAL PRIMARY KEY,
                    trigger             TEXT NOT NULL CHECK (trigger IN ('button','motion')),
                    timestamp           TIMESTAMPTZ NOT NULL DEFAULT now(),
                    photo_url           TEXT,
                    response            TEXT,
                    tag                 TEXT CHECK (tag IN ('Expected','Unknown','Suspicious')),
                    silent              BOOLEAN NOT NULL DEFAULT FALSE,
                    telegram_message_id BIGINT
                );

                CREATE TABLE IF NOT EXISTS device_status (
                    id          INTEGER PRIMARY KEY CHECK (id = 1),
                    wifi_signal INTEGER,
                    last_sync   TIMESTAMPTZ
                );

                INSERT INTO device_status (id, wifi_signal, last_sync)
                VALUES (1, NULL, NULL)
                ON CONFLICT (id) DO NOTHING;
            """)

# ── WebSocket connection manager ──────────────────────────────
class ConnectionManager:
    def __init__(self):
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket):
        self.active.remove(ws)

    async def broadcast(self, payload: dict):
        import json
        dead = []
        for ws in self.active:
            try:
                await ws.send_text(json.dumps(payload))
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.active.remove(ws)

manager = ConnectionManager()

# ── App setup ─────────────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    if RECOGNITION_ENABLED:
        # Warm the face model in the background so the first visit
        # after a deploy doesn't wait for the model download/load.
        import threading, recognition
        def _warm():
            try:
                recognition.get_model()
                print("[Recognition] model warmed up")
            except Exception as e:
                print(f"[Recognition] warmup failed: {e!r}")
        threading.Thread(target=_warm, daemon=True).start()
    yield

app = FastAPI(title="Smart Doorbell API", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],   # tighten this in production
    allow_methods=["*"],
    allow_headers=["*"],
)

# Serve photos as static files at /photos/<filename>
app.mount("/photos", StaticFiles(directory=PHOTOS_DIR), name="photos")

# ── Pydantic models ───────────────────────────────────────────
class TagUpdate(BaseModel):
    tag: str   # Expected | Unknown | Suspicious

class DeviceHeartbeat(BaseModel):
    wifi_signal: Optional[int] = None

# ── Helper ────────────────────────────────────────────────────
def iso(dt) -> Optional[str]:
    if dt is None:
        return None
    if isinstance(dt, (datetime, date)):
        return dt.isoformat()
    return str(dt)

def row_to_visit(row) -> dict:
    return {
        "id":                  row["id"],
        "trigger":             row["trigger"],
        "timestamp":           iso(row["timestamp"]),
        "photo_url":           row["photo_url"],
        "response":            row["response"],
        "tag":                 row["tag"],
        "silent":              bool(row["silent"]),
        "telegram_message_id": row["telegram_message_id"],
        "visitor_id":          row.get("visitor_id"),
        "visitor_name":        row.get("visitor_name"),
    }

# ── Routes ────────────────────────────────────────────────────

# GET / — the live dashboard
@app.get("/", include_in_schema=False)
def dashboard():
    return FileResponse(os.path.join(BASE_DIR, "dashboard.html"))


# GET /api/auth/check — dashboard verifies its stored password
@app.get("/api/auth/check", dependencies=[Depends(require_dashboard_key)])
def auth_check():
    return {"ok": True}


# GET /api/recognition/health — load the face model, report memory.
# Used to verify the hosting environment can run recognition at all.
@app.get("/api/recognition/health", dependencies=[Depends(require_dashboard_key)])
def recognition_health():
    import recognition
    try:
        return recognition.health()
    except Exception as e:
        raise HTTPException(500, f"recognition unavailable: {e!r}")


# GET /api/visits — all visits, newest first (with visitor names)
@app.get("/api/visits", dependencies=[Depends(require_dashboard_key)])
def get_visits():
    rows = db_fetchall(
        """SELECT visits.*, visitors.name AS visitor_name
           FROM visits LEFT JOIN visitors ON visitors.id = visits.visitor_id
           ORDER BY visits.timestamp DESC"""
    )
    return [row_to_visit(r) for r in rows]


# ── Visitors API ──────────────────────────────────────────────

class VisitorRename(BaseModel):
    name: str

@app.get("/api/visitors", dependencies=[Depends(require_dashboard_key)])
def get_visitors():
    rows = db_fetchall(
        "SELECT id, name, photo_url, first_seen, last_seen, visit_count "
        "FROM visitors ORDER BY last_seen DESC"
    )
    return [{
        "id":          r["id"],
        "name":        r["name"],
        "photo_url":   r["photo_url"],
        "first_seen":  iso(r["first_seen"]),
        "last_seen":   iso(r["last_seen"]),
        "visit_count": r["visit_count"],
    } for r in rows]


@app.patch("/api/visitors/{visitor_id}", dependencies=[Depends(require_dashboard_key)])
def rename_visitor(visitor_id: int, body: VisitorRename):
    name = body.name.strip()
    if not name:
        raise HTTPException(400, "name must not be empty")
    affected = db_execute("UPDATE visitors SET name=%s WHERE id=%s", (name, visitor_id))
    if affected == 0:
        raise HTTPException(404, "Visitor not found")
    return {"id": visitor_id, "name": name}


@app.delete("/api/visitors/{visitor_id}", dependencies=[Depends(require_dashboard_key)])
def delete_visitor(visitor_id: int):
    db_execute("UPDATE visits SET visitor_id=NULL WHERE visitor_id=%s", (visitor_id,))
    affected = db_execute("DELETE FROM visitors WHERE id=%s", (visitor_id,))
    if affected == 0:
        raise HTTPException(404, "Visitor not found")
    return {"deleted": visitor_id}


# POST /api/visits — ESP32 posts a new visit (multipart: photo + metadata)
@app.post("/api/visits", status_code=201)
async def create_visit(
    trigger:   str        = Form(...),
    timestamp: str        = Form(None),
    silent:    bool       = Form(False),
    photo:     UploadFile = File(None),
):
    if trigger not in ("button", "motion"):
        raise HTTPException(400, "trigger must be 'button' or 'motion'")

    ts = timestamp or datetime.now(timezone.utc).isoformat()

    photo_url = None
    if photo:
        filename = f"visit_{ts.replace(':','-').replace('+','_')}.jpg"
        content  = await photo.read()

        if SUPABASE_URL and SUPABASE_SERVICE_KEY:
            # Durable storage: upload to Supabase Storage bucket
            import httpx
            async with httpx.AsyncClient() as client:
                # New-style keys (sb_secret_...) go in the apikey header;
                # a Bearer header with them fails ("Invalid Compact JWS").
                r = await client.post(
                    f"{SUPABASE_URL}/storage/v1/object/{STORAGE_BUCKET}/{filename}",
                    headers={
                        "apikey":       SUPABASE_SERVICE_KEY,
                        "Content-Type": "image/jpeg",
                        "x-upsert":     "true",
                    },
                    content=content,
                    timeout=20,
                )
            if r.status_code in (200, 201):
                photo_url = f"{SUPABASE_URL}/storage/v1/object/public/{STORAGE_BUCKET}/{filename}"
            else:
                print(f"[Storage] Supabase upload failed ({r.status_code}): {r.text[:200]}")

        if photo_url is None:
            # Fallback: local disk (ephemeral on Railway)
            filepath = os.path.join(PHOTOS_DIR, filename)
            with open(filepath, "wb") as f:
                f.write(content)
            photo_url = f"/photos/{filename}"

    # Face recognition — never allowed to break visit creation
    visitor_id = None
    visitor_name = None
    visitor_visits = None
    visitor_is_new = False
    if RECOGNITION_ENABLED and photo and photo_url:
        try:
            import asyncio, recognition
            embedding, det = await asyncio.to_thread(
                recognition.extract_embedding, content
            )
            if embedding:
                visitor_id, visitor_name, visitor_visits, visitor_is_new = \
                    recognition.match_or_create_visitor(
                        db_fetchone, db_execute, embedding, photo_url, ts
                    )
                print(f"[Recognition] visitor={visitor_id} name={visitor_name} "
                      f"visits={visitor_visits} new={visitor_is_new} det={det:.2f}")
            else:
                print("[Recognition] no usable face in photo")
        except Exception as e:
            print(f"[Recognition] failed: {e!r}")

    row = db_fetchone(
        """INSERT INTO visits (trigger, timestamp, photo_url, silent, visitor_id)
           VALUES (%s, %s, %s, %s, %s)
           RETURNING *""",
        (trigger, ts, photo_url, bool(silent), visitor_id)
    )
    visit = row_to_visit(row)
    visit_id = visit["id"]
    visit["visitor_name"]   = visitor_name
    visit["visitor_visits"] = visitor_visits
    visit["visitor_is_new"] = visitor_is_new

    # Send Telegram notification with reply buttons.
    # A Telegram failure must not lose the visit — it is already saved.
    try:
        msg_id = await tg.send_visit_notification(visit)
    except Exception as e:
        print(f"[Telegram] notification failed: {e!r}")
        msg_id = None
    if msg_id:
        db_execute(
            "UPDATE visits SET telegram_message_id=%s WHERE id=%s",
            (msg_id, visit_id)
        )
        visit["telegram_message_id"] = msg_id

    # Push to all connected dashboard clients
    await manager.broadcast({"type": "new_visit", "data": visit})

    return visit


# PATCH /api/visits/:id/tag — dashboard sets a tag
@app.patch("/api/visits/{visit_id}/tag", dependencies=[Depends(require_dashboard_key)])
def update_tag(visit_id: int, body: TagUpdate):
    if body.tag not in ("Expected", "Unknown", "Suspicious"):
        raise HTTPException(400, "tag must be Expected, Unknown, or Suspicious")

    affected = db_execute(
        "UPDATE visits SET tag=%s WHERE id=%s", (body.tag, visit_id)
    )

    if affected == 0:
        raise HTTPException(404, "Visit not found")
    return {"id": visit_id, "tag": body.tag}


# GET /api/stats — visits per day + busiest hours
@app.get("/api/stats", dependencies=[Depends(require_dashboard_key)])
def get_stats():
    # Visits per day for the last 7 days
    per_day = db_fetchall("""
        SELECT DATE(timestamp) AS day,
               COUNT(*)        AS total,
               COUNT(*) FILTER (WHERE trigger = 'button') AS button,
               COUNT(*) FILTER (WHERE trigger = 'motion') AS motion
        FROM   visits
        WHERE  DATE(timestamp) >= CURRENT_DATE - 6
        GROUP  BY day
        ORDER  BY day
    """)

    # Hourly activity (0-23)
    hourly = db_fetchall("""
        SELECT EXTRACT(HOUR FROM timestamp)::int AS hour,
               COUNT(*) AS total,
               COUNT(*) FILTER (WHERE trigger = 'button') AS button,
               COUNT(*) FILTER (WHERE trigger = 'motion') AS motion
        FROM   visits
        GROUP  BY hour
        ORDER  BY hour
    """)

    # Summary figures
    summary = db_fetchone("""
        SELECT
            COUNT(*)                                   AS total_visits,
            COUNT(*) FILTER (WHERE trigger = 'button') AS button_presses,
            COUNT(*) FILTER (WHERE trigger = 'motion') AS motion_triggers,
            COUNT(*) FILTER (WHERE response IS NOT NULL AND response != '') AS responses
        FROM visits
    """)

    response_rate = 0
    if summary["total_visits"] > 0:
        response_rate = round(summary["responses"] / summary["total_visits"] * 100)

    return {
        "per_day": [{**dict(r), "day": iso(r["day"])} for r in per_day],
        "hourly":  [dict(r) for r in hourly],
        "summary": {
            "total_visits":    summary["total_visits"],
            "button_presses":  summary["button_presses"],
            "motion_triggers": summary["motion_triggers"],
            "response_rate":   response_rate,
        }
    }


# GET /api/device/status — uptime, WiFi signal, last sync
@app.get("/api/device/status", dependencies=[Depends(require_dashboard_key)])
def get_device_status():
    row = db_fetchone("SELECT * FROM device_status WHERE id=1")

    uptime_seconds = int(time.time() - SERVER_START_TIME)

    # Device counts as online if a heartbeat arrived recently
    # (heartbeat interval is 60 s; allow 2.5 missed-beat margin)
    online = False
    if row["last_sync"]:
        age = (datetime.now(timezone.utc) - row["last_sync"]).total_seconds()
        online = age < 150

    return {
        "uptime_seconds": uptime_seconds,
        "wifi_signal":    row["wifi_signal"],
        "last_sync":      iso(row["last_sync"]),
        "online":         online,
        "camera_ok":      online,   # unknown when offline; tied to liveness
    }


# POST /api/device/heartbeat — ESP32 reports its status periodically
@app.post("/api/device/heartbeat")
def device_heartbeat(body: DeviceHeartbeat):
    now = datetime.now(timezone.utc).isoformat()
    db_execute(
        "UPDATE device_status SET wifi_signal=%s, last_sync=%s WHERE id=1",
        (body.wifi_signal, now)
    )
    return {"ok": True}


# WebSocket /ws — dashboard connects here for live push
# Browsers can't set headers on WebSockets, so the key rides in ?key=
@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    if DASHBOARD_PASSWORD and ws.query_params.get("key", "") != DASHBOARD_PASSWORD:
        await ws.close(code=1008)
        return
    await manager.connect(ws)
    try:
        while True:
            await ws.receive_text()   # keep connection alive
    except WebSocketDisconnect:
        manager.disconnect(ws)


# ── Telegram webhook ─────────────────────────────────────────

@app.post("/telegram/webhook")
async def telegram_webhook(request: Request):
    """
    Telegram calls this endpoint when the homeowner taps a reply button.
    Parses the callback, saves the response, pushes update to dashboard.
    """
    body = await request.json()

    # Only handle callback queries (button taps)
    callback = body.get("callback_query")
    if not callback:
        return {"ok": True}

    callback_id = callback["id"]
    data        = callback.get("data", "")   # e.g. "reply:42:On my way"

    if not data.startswith("reply:"):
        await tg.answer_callback(callback_id, "Unknown action")
        return {"ok": True}

    # Parse  "reply:<visit_id>:<response_text>"
    parts      = data.split(":", 2)
    visit_id   = int(parts[1])
    response   = parts[2]

    # Save response to database
    db_execute(
        "UPDATE visits SET response=%s WHERE id=%s",
        (response, visit_id)
    )
    row = db_fetchone("SELECT * FROM visits WHERE id=%s", (visit_id,))

    if not row:
        await tg.answer_callback(callback_id, "Visit not found")
        return {"ok": True}

    visit = row_to_visit(row)

    # Answer the callback — removes loading spinner on the button
    await tg.answer_callback(callback_id, f"✅ Sent: {response}")

    # Edit the Telegram message to remove buttons and show the reply
    msg_id = visit.get("telegram_message_id")
    if msg_id:
        ts = visit["timestamp"][:16].replace("T", " ")
        trigger_label = "🔔 Doorbell" if visit["trigger"] == "button" else "👁 Motion"
        await tg.remove_buttons(
            msg_id,
            f"{trigger_label} · {ts}\n✅ You replied: {response}"
        )

    # Push response update to dashboard via WebSocket
    await manager.broadcast({
        "type": "visit_updated",
        "data": visit,
    })

    return {"ok": True}


# ── Register / check webhook ──────────────────────────────────

@app.post("/admin/set-webhook", dependencies=[Depends(require_dashboard_key)])
async def set_webhook(request: Request):
    """
    Call this once after deploying (or starting ngrok).
    Body: { "url": "https://your-server-url" }
    """
    body = await request.json()
    url  = body.get("url", "").rstrip("/")
    if not url:
        raise HTTPException(400, "url is required")
    result = await tg.set_webhook(url)
    return result


@app.get("/admin/webhook-info", dependencies=[Depends(require_dashboard_key)])
async def webhook_info():
    return await tg.get_webhook_info()
