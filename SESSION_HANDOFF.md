# Smart Doorbell – Group 15
## Session Handoff Document
**Date:** 2026-06-10
**Purpose:** Full context for continuing in a new session

---

## Project Overview

An IoT smart doorbell built on the ESP32-CAM (AI Thinker). When a visitor presses a button:
1. ESP32-CAM captures a photo
2. Sends a Telegram notification with the photo and two reply buttons (✅ On my way / 🏠 Not home)
3. Visit is saved to the cloud backend database
4. Dashboard updates live via WebSocket

**Stack:** Arduino C++ (firmware) · Python FastAPI on **Railway** + **Supabase PostgreSQL** (backend) · Vanilla JS (dashboard) · Telegram Bot API

---

## ☁️ Cloud Architecture (NEW — replaced SQLite + ngrok)

```
ESP32 ──→ Railway (FastAPI) ──→ Supabase (Postgres, Tokyo region)
                │
                ├──→ Telegram (notifications + reply webhook)
                └──→ Dashboard (WebSocket live updates)
```

| Piece | Where | Status |
|-------|-------|--------|
| Backend API | https://smart-doorbell-production.up.railway.app | ✅ live |
| Database | Supabase project "Smart-Doorbell" (`aws-1-ap-northeast-1`, transaction pooler port 6543) | ✅ live, RLS enabled |
| Telegram webhook | registered to `<railway-url>/telegram/webhook` | ✅ verified round-trip |
| ngrok | — | 🗑️ no longer needed, ever |

**Verified end-to-end on 2026-06-10:** visit posted from cloud → Telegram notification received → "✅ On my way" tapped → response stored in Supabase (visit id 5) → readable via public API.

**Railway setup notes:**
- Deploys automatically on every push to `main` (GitHub repo `yussefz99/Smart-Doorbell`)
- Service **Root Directory = `/backend`** (required — repo root has no Python app)
- Variables set in Railway: `BOT_TOKEN`, `CHAT_ID`, `DATABASE_URL`
- Settings changes are **staged** — must click the purple "Apply changes/Deploy" banner
- `psycopg2-binary` must be **≥2.9.10** (2.9.9 has no Python 3.13 wheels → `libpq.so.5` ImportError)

---

## 🔑 Credentials — where they live (never in git)

| Secret | Local | Cloud |
|--------|-------|-------|
| `BOT_TOKEN`, `CHAT_ID` | `backend/.env` | Railway → Variables |
| `DATABASE_URL` (Supabase pooler string) | `backend/.env` | Railway → Variables |
| WiFi + bot creds for firmware | `ESP32/doorbell_step5_complete_v1/secrets.h` (gitignored; template: `secrets.h.example`) | — |

⚠️ **Deferred task:** the old bot token leaked in git history (commit `e212e4f`). User chose to keep it for now — **revoke via @BotFather later**, then update: `backend/.env`, Railway `BOT_TOKEN` variable, `secrets.h`, and re-check webhook.

---

## File Locations

| File | Path |
|------|------|
| Step 5 firmware (main) | `ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` |
| Firmware secrets | `ESP32/doorbell_step5_complete_v1/secrets.h` (gitignored) |
| Backend server (Postgres) | `backend/server.py` |
| Telegram bot helper | `backend/telegram_bot.py` |
| Dashboard | `backend/dashboard.html` |
| Local secrets | `backend/.env` (gitignored) |
| Railway start command | `backend/Procfile` |
| HW unit tests report (submitted 9/6) | `C:\Users\yusse\Downloads\HARDWARE UNIT TESTS REPORT - Smart Doorbell - Group 15.docx` |

---

## Hardware State

### Working (tested)
- Camera (QVGA JPEG ~6 KB), WiFi + Telegram text, photo upload to Telegram, flash LED (GPIO 4)

### ⏳ THE ONE BLOCKER: button not yet connected
- Button must connect **GPIO 13 (IO13) + GND**
- CS-CAM carrier exposes only **bare solder pads** (module pins soldered face-down; female jumpers can't grip, holes are filled) → **soldering required**
- User has soldering iron access at university lab
- **Plan:** solder two wires onto the IO13 and GND pads → other ends to **two diagonal legs** of the 6×6 mm tactile button (same-side legs are internally connected!)
- Quick pre-check at lab: with sketch running, briefly short IO13 pad to GND pad with a wire — photo should hit Telegram

### Missing hardware (requested via WhatsApp group, Hebrew message sent)
1. **PIR motion sensor** — team OK to drop the motion feature if hard to get
2. **MAX98357A I²S amp + speaker** — voice/sound to visitor
3. **0.96″ OLED 128×64 SSD1306, I²C** — chosen from catalog (2 pins only; ESP32-CAM is pin-starved) — to display homeowner's reply to the visitor

---

## Step 5 Sketch — Current State

- Credentials come from `secrets.h` (`#include`) — already filled locally, upload-ready
- `BACKEND_URL = "https://smart-doorbell-production.up.railway.app"` — already set
- `BUTTON_PIN 13`, debounce + 3 s min gap, GPIO 0 conflict fixed (GPIO 0 = camera XCLK, unusable)
- Arduino IDE: board **AI Thinker ESP32-CAM**, port **COM3**, Serial 115200

## IMMEDIATE NEXT STEP (at the lab)

1. Solder button to IO13 + GND (diagonal legs)
2. Upload Step 5 → Serial Monitor → wait 3 flashes
3. Press button → photo in Telegram **and** visit appears in dashboard via Railway/Supabase
4. Tap reply button → response shows in dashboard (already proven from cloud side)

---

## How to Run / Deploy

```bash
# Local backend (uses cloud Supabase DB via .env)
cd backend
uvicorn server:app --reload --port 8001

# Deploy = just push to main (Railway auto-deploys)
git push origin main
```

No ngrok. Webhook is permanently registered to Railway.

---

## Known Issues

| # | Issue | Status |
|---|-------|--------|
| 1 | Old bot token in git history | ⏳ revoke later (user deferred) |
| 2 | Button not wired | ⏳ solder at lab — IO13 + GND pads |
| 3 | Telegram "remove buttons after reply" uses `editMessageCaption` — fails silently on **text-only** messages (no photo). Photo messages fine. | Low priority |
| 4 | Settings page UI only — not wired to backend | Deferred to V2 |
| 5 | Railway trial credit ($5 / 30 days) — check usage before demo day | Watch |

---

## What Comes After Step 5

1. **OLED screen feature** (when HW arrives): new endpoint `GET /api/visits/{id}/response` + ESP32 polls it and shows the reply to the visitor
2. **Audio** (when HW arrives): buzzer tones vs voice — open team decision
3. **Settings API** — `GET/POST /api/settings`, wire to dashboard
4. **V3** — DeepFace visitor recognition

---

## Session Log

- **2026-06-08:** GPIO 0/XCLK conflict fixed; wiring attempt blocked by connector type
- **2026-06-10:** Connector mystery solved (solder pads); HW report submitted; secrets redacted from repo; **full cloud migration: Supabase Postgres + Railway deploy + permanent Telegram webhook, verified end-to-end**; ESP32 sketch pointed at cloud; test data cleaned (visit 5 kept as proof)
