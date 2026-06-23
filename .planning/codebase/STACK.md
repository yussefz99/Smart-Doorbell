# Technology Stack

**Analysis Date:** 2026-06-23

## Languages

**Primary:**
- C/C++ (Arduino dialect) — ESP32 firmware (`ESP32/doorbell_step5_complete_v1/doorbell_step5_complete_v1.ino` and all earlier step sketches)
- Python 3 — Backend server (`backend/server.py`, `backend/telegram_bot.py`, `backend/recognition.py`)

**Secondary:**
- JavaScript (Vanilla ES2020+) — Browser dashboard embedded in `backend/dashboard.html`
- HTML/CSS — Dashboard UI, single-file (`backend/dashboard.html`, ~1400 lines)

**Not present:**
- Flutter app directory (`flutter_app/`) exists but contains only a placeholder file — no Dart code implemented yet

## Runtime

**Environment (Backend):**
- Python 3.13 (inferred: `psycopg2-binary` ≥2.9.10 required for Python 3.13 wheels)
- ASGI runtime via `uvicorn[standard]`

**Environment (Firmware):**
- Arduino framework on ESP-IDF
- Target board: AI Thinker ESP32-CAM (dual-core Xtensa LX6, 240 MHz)
- Flash: 4 MB

**Package Manager (Backend):**
- pip with `requirements.txt`
- Lockfile: Not present (no `pip.lock` or `poetry.lock`)

## Frameworks

**Backend Core:**
- FastAPI 0.111.0 — REST API + WebSocket server (`backend/server.py`)
- Uvicorn 0.29.0 (standard extras) — ASGI server; started via `backend/Procfile` as `uvicorn server:app --host 0.0.0.0 --port $PORT`
- Pydantic (bundled with FastAPI) — request body validation via `BaseModel`

**Frontend (Dashboard):**
- No framework — Vanilla JS + native browser `fetch`, `WebSocket`, and CSS variables
- Chart rendering: pure canvas/DOM drawing, no charting library

**Firmware:**
- Arduino core for ESP32 (Arduino IDE, board: AI Thinker ESP32-CAM)
- No PlatformIO — projects use the standard `.ino` Arduino sketch format

**Face Recognition (optional, off by default):**
- InsightFace ≥1.0 — ONNX-based face analysis (`backend/recognition.py`)
- ONNXRuntime ≥1.20 — inference backend (CPU only, `CPUExecutionProvider`)
- OpenCV (headless) ≥4.11 — image decode/preprocess

## Key Dependencies

**Critical (Backend):**
- `fastapi==0.111.0` — main API framework
- `uvicorn[standard]==0.29.0` — production ASGI server
- `psycopg2-binary==2.9.10` — PostgreSQL driver (Supabase); must be ≥2.9.10 for Python 3.13 wheels
- `httpx==0.27.0` — async HTTP client for Telegram Bot API calls and Supabase Storage REST API
- `python-multipart==0.0.9` — multipart/form-data parsing (ESP32 photo uploads)
- `python-dotenv==1.0.0` — `.env` loading for local development

**Optional Face Recognition (Backend):**
- `insightface>=1.0` — face detection + embedding extraction (model: `buffalo_s` or `buffalo_l`)
- `onnxruntime>=1.20` — ONNX model inference
- `opencv-python-headless>=4.11` — image decode without display dependencies
- `psutil>=6` — process memory reporting for `/api/recognition/health`

**Infrastructure / Deploy:**
- `railpack.json` specifies apt packages for Railway: `libgl1`, `libglib2.0-0`, `libxcb1`, `libx11-6`, `libxext6`, `libsm6` (required by OpenCV/InsightFace even in headless mode)

**Firmware (ESP32 Arduino Libraries):**
- `esp_camera.h` — camera capture (ESP-IDF bundled, board-specific)
- `WiFi.h` — WiFi STA mode connection
- `WiFiClientSecure.h` — TLS over TCP for HTTPS calls (certificate check disabled via `setInsecure()`)
- `HTTPClient.h` — used in earlier steps (step2), replaced by raw `WiFiClientSecure` in step5

## Configuration

**Environment (Backend):**
- Loaded from `backend/.env` locally (gitignored) and Railway Variables in production
- Required env vars:
  - `DATABASE_URL` — Supabase PostgreSQL connection string (transaction pooler, port 6543)
  - `BOT_TOKEN` — Telegram Bot token
  - `CHAT_ID` — Telegram chat ID for notifications
- Optional env vars:
  - `SUPABASE_URL` — Supabase project base URL (for Storage API)
  - `SUPABASE_SERVICE_KEY` — Supabase service role key (`sb_secret_...`)
  - `DASHBOARD_PASSWORD` — password gate for dashboard read endpoints
  - `RECOGNITION_ENABLED` — set to `1` to activate face recognition (default: off)
  - `FACE_MODEL` — `buffalo_s` (default, ~150 MB) or `buffalo_l` (better accuracy, ~600 MB)
  - `PUBLIC_BASE_URL` — public URL of the server for Telegram photo URLs (default: Railway URL)

**Firmware (ESP32):**
- Secrets in `ESP32/doorbell_step5_complete_v1/secrets.h` (gitignored; template at `ESP32/doorbell_step5_complete_v1/secrets.h.example`)
- Defines: `SECRET_WIFI_SSID`, `SECRET_WIFI_PASSWORD`, `SECRET_BOT_TOKEN`, `SECRET_CHAT_ID`
- Hardcoded backend URL in sketch: `BACKEND_URL = "https://smart-doorbell-production.up.railway.app"` (`doorbell_step5_complete_v1.ino` line 27)
- Demo-mode flag: `TRIGGER_ON_BOOT true` (line 42) — fires one event on RESET; set to `false` after button is soldered

**Build:**
- Backend: no build step — Python source deployed directly
- Firmware: Arduino IDE (board: AI Thinker ESP32-CAM, port COM3, baud 115200); pre-compiled binary at `ESP32/compiled_program.bin`

## Platform Requirements

**Development:**
- Python 3.13+
- Arduino IDE with ESP32 board support package (AI Thinker ESP32-CAM profile)
- Supabase project with pgvector extension enabled (required for face recognition table `visitor_embeddings`)

**Production:**
- Backend deployed on **Railway** (PaaS, auto-deploys on push to `main` branch of GitHub repo `yussefz99/Smart-Doorbell`)
- Root Directory set to `/backend` in Railway service config
- Database on **Supabase** (`aws-1-ap-northeast-1` region, Tokyo; RLS enabled)
- Photo storage on **Supabase Storage** (public bucket named `photos`)

---

*Stack analysis: 2026-06-23*
