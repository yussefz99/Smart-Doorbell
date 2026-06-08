# ============================================================
#  Smart Doorbell – Backend Server
#  Stack : FastAPI + SQLite (no ORM, plain sqlite3)
#  Run   : uvicorn server:app --reload --port 8000
# ============================================================

import sqlite3
import os
import time
from datetime import datetime, timezone
from contextlib import asynccontextmanager
from typing import Optional

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, UploadFile, File, Form, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from dotenv import load_dotenv

load_dotenv()

import telegram_bot as tg

# ── Paths ────────────────────────────────────────────────────
BASE_DIR    = os.path.dirname(os.path.abspath(__file__))
DB_PATH     = os.path.join(BASE_DIR, "doorbell.db")
PHOTOS_DIR  = os.path.join(BASE_DIR, "photos")
os.makedirs(PHOTOS_DIR, exist_ok=True)

SERVER_START_TIME = time.time()

# ── Database ─────────────────────────────────────────────────
def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row   # rows behave like dicts
    return conn

def init_db():
    conn = get_db()
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS visits (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            trigger             TEXT    NOT NULL CHECK(trigger IN ('button','motion')),
            timestamp           TEXT    NOT NULL,
            photo_url           TEXT,
            response            TEXT,
            tag                 TEXT    CHECK(tag IN ('Expected','Unknown','Suspicious')),
            silent              INTEGER NOT NULL DEFAULT 0,
            telegram_message_id INTEGER
        );

        CREATE TABLE IF NOT EXISTS device_status (
            id          INTEGER PRIMARY KEY CHECK(id = 1),
            wifi_signal INTEGER,
            last_sync   TEXT
        );

        INSERT OR IGNORE INTO device_status (id, wifi_signal, last_sync)
        VALUES (1, NULL, NULL);
    """)
    conn.commit()

    # Migration: add telegram_message_id column if it doesn't exist yet
    cols = [r[1] for r in conn.execute("PRAGMA table_info(visits)").fetchall()]
    if "telegram_message_id" not in cols:
        conn.execute("ALTER TABLE visits ADD COLUMN telegram_message_id INTEGER")
        conn.commit()

    conn.close()

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
    seed_demo_data()
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
def row_to_visit(row) -> dict:
    return {
        "id":                  row["id"],
        "trigger":             row["trigger"],
        "timestamp":           row["timestamp"],
        "photo_url":           row["photo_url"],
        "response":            row["response"],
        "tag":                 row["tag"],
        "silent":              bool(row["silent"]),
        "telegram_message_id": row["telegram_message_id"],
    }

# ── Routes ────────────────────────────────────────────────────

# GET /api/visits — all visits, newest first
@app.get("/api/visits")
def get_visits():
    conn = get_db()
    rows = conn.execute(
        "SELECT * FROM visits ORDER BY timestamp DESC"
    ).fetchall()
    conn.close()
    return [row_to_visit(r) for r in rows]


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
        filename  = f"visit_{ts.replace(':','-').replace('+','_')}.jpg"
        filepath  = os.path.join(PHOTOS_DIR, filename)
        content   = await photo.read()
        with open(filepath, "wb") as f:
            f.write(content)
        photo_url = f"/photos/{filename}"

    conn = get_db()
    cur  = conn.execute(
        "INSERT INTO visits (trigger, timestamp, photo_url, silent) VALUES (?,?,?,?)",
        (trigger, ts, photo_url, int(silent))
    )
    conn.commit()
    visit_id = cur.lastrowid

    row   = conn.execute("SELECT * FROM visits WHERE id=?", (visit_id,)).fetchone()
    conn.close()
    visit = row_to_visit(row)

    # Send Telegram notification with reply buttons
    msg_id = await tg.send_visit_notification(visit)
    if msg_id:
        conn = get_db()
        conn.execute("UPDATE visits SET telegram_message_id=? WHERE id=?", (msg_id, visit_id))
        conn.commit()
        conn.close()
        visit["telegram_message_id"] = msg_id

    # Push to all connected dashboard clients
    await manager.broadcast({"type": "new_visit", "data": visit})

    return visit


# PATCH /api/visits/:id/tag — dashboard sets a tag
@app.patch("/api/visits/{visit_id}/tag")
def update_tag(visit_id: int, body: TagUpdate):
    if body.tag not in ("Expected", "Unknown", "Suspicious"):
        raise HTTPException(400, "tag must be Expected, Unknown, or Suspicious")

    conn = get_db()
    affected = conn.execute(
        "UPDATE visits SET tag=? WHERE id=?", (body.tag, visit_id)
    ).rowcount
    conn.commit()
    conn.close()

    if affected == 0:
        raise HTTPException(404, "Visit not found")
    return {"id": visit_id, "tag": body.tag}


# GET /api/stats — visits per day + busiest hours
@app.get("/api/stats")
def get_stats():
    conn = get_db()

    # Visits per day for the last 7 days
    per_day = conn.execute("""
        SELECT DATE(timestamp) AS day,
               COUNT(*)        AS total,
               SUM(trigger='button') AS button,
               SUM(trigger='motion') AS motion
        FROM   visits
        WHERE  DATE(timestamp) >= DATE('now','-6 days')
        GROUP  BY day
        ORDER  BY day
    """).fetchall()

    # Hourly activity (0-23)
    hourly = conn.execute("""
        SELECT CAST(strftime('%H', timestamp) AS INTEGER) AS hour,
               COUNT(*) AS total,
               SUM(trigger='button') AS button,
               SUM(trigger='motion') AS motion
        FROM   visits
        GROUP  BY hour
        ORDER  BY hour
    """).fetchall()

    # Summary figures
    summary = conn.execute("""
        SELECT
            COUNT(*)                                        AS total_visits,
            SUM(trigger='button')                           AS button_presses,
            SUM(trigger='motion')                           AS motion_triggers,
            SUM(response IS NOT NULL AND response != '')    AS responses,
            COUNT(*)                                        AS total_for_rate
        FROM visits
    """).fetchone()

    conn.close()

    response_rate = 0
    if summary["total_visits"] > 0:
        response_rate = round(summary["responses"] / summary["total_visits"] * 100)

    return {
        "per_day": [dict(r) for r in per_day],
        "hourly":  [dict(r) for r in hourly],
        "summary": {
            "total_visits":    summary["total_visits"],
            "button_presses":  summary["button_presses"],
            "motion_triggers": summary["motion_triggers"],
            "response_rate":   response_rate,
        }
    }


# GET /api/device/status — uptime, WiFi signal, last sync
@app.get("/api/device/status")
def get_device_status():
    conn = get_db()
    row  = conn.execute("SELECT * FROM device_status WHERE id=1").fetchone()
    conn.close()

    uptime_seconds = int(time.time() - SERVER_START_TIME)

    return {
        "uptime_seconds": uptime_seconds,
        "wifi_signal":    row["wifi_signal"],
        "last_sync":      row["last_sync"],
        "camera_ok":      True,   # ESP32 will update this via heartbeat
    }


# POST /api/device/heartbeat — ESP32 reports its status periodically
@app.post("/api/device/heartbeat")
def device_heartbeat(body: DeviceHeartbeat):
    now = datetime.now(timezone.utc).isoformat()
    conn = get_db()
    conn.execute(
        "UPDATE device_status SET wifi_signal=?, last_sync=? WHERE id=1",
        (body.wifi_signal, now)
    )
    conn.commit()
    conn.close()
    return {"ok": True}


# WebSocket /ws — dashboard connects here for live push
@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
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
    conn = get_db()
    conn.execute(
        "UPDATE visits SET response=? WHERE id=?",
        (response, visit_id)
    )
    conn.commit()
    row = conn.execute("SELECT * FROM visits WHERE id=?", (visit_id,)).fetchone()
    conn.close()

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

@app.post("/admin/set-webhook")
async def set_webhook(request: Request):
    """
    Call this once after starting ngrok.
    Body: { "url": "https://abc123.ngrok.io" }
    """
    body = await request.json()
    url  = body.get("url", "").rstrip("/")
    if not url:
        raise HTTPException(400, "url is required")
    result = await tg.set_webhook(url)
    return result


@app.get("/admin/webhook-info")
async def webhook_info():
    return await tg.get_webhook_info()


# ── Demo data (runs once on first start if DB is empty) ───────
def seed_demo_data():
    conn = get_db()
    count = conn.execute("SELECT COUNT(*) FROM visits").fetchone()[0]
    if count > 0:
        conn.close()
        return

    demo = [
        ("button", "2026-05-30T08:15:00Z", None, "On my way",  "Expected",   0),
        ("button", "2026-05-30T13:42:00Z", None, None,         "Unknown",    0),
        ("motion", "2026-05-30T23:10:00Z", None, None,         "Suspicious", 1),
        ("button", "2026-05-31T09:05:00Z", None, "Not home",   "Expected",   0),
        ("button", "2026-05-31T17:30:00Z", None, None,         None,         0),
        ("motion", "2026-06-01T02:20:00Z", None, None,         None,         1),
        ("button", "2026-06-01T10:00:00Z", None, None,         None,         0),
    ]
    conn.executemany(
        "INSERT INTO visits (trigger,timestamp,photo_url,response,tag,silent) VALUES (?,?,?,?,?,?)",
        demo
    )
    conn.commit()
    conn.close()
