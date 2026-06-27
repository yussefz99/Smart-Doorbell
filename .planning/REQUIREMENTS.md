# Requirements: Smart Doorbell — Group 15

**Defined:** 2026-06-27
**Core Value:** When a visitor presses the doorbell, the homeowner reliably gets a photo notification on Telegram, can respond, and the full round-trip is visible live on the dashboard.

## v1 Requirements

Submission-ready scope: hardening and finishing loose ends on the existing V1/V2 system. Each maps to a roadmap phase.

### Settings

- [ ] **SET-01**: Quiet-hours settings (enabled, start time, end time) load from the backend on dashboard open and persist to the backend on save via `/api/settings`
- [ ] **SET-02**: The dashboard Settings page reflects real backend state — controls that are not wired to the backend are removed or clearly marked as non-functional, so nothing on the page is misleading in the demo

### Security

- [ ] **SEC-01**: `POST /api/visits` requires a shared device secret (`X-Device-Key`); requests without the correct secret are rejected with 401/403
- [ ] **SEC-02**: `POST /api/device/heartbeat` requires the same `X-Device-Key` secret
- [ ] **SEC-03**: ESP32 firmware (`doorbell_step5_btn_io14`, the demo build) sends the `X-Device-Key` header on visit upload, heartbeat, and the reply poll (paired with SEC-01/SEC-02/SEC-06), and the demo round-trip still works end-to-end
- [ ] **SEC-04**: `POST /telegram/webhook` validates the `X-Telegram-Bot-Api-Secret-Token` header; requests without the registered token are rejected
- [ ] **SEC-05**: The Telegram webhook callback parser bounds-checks `reply:`-prefixed `callback_data` and returns a clean response (no IndexError / repeated 500 retries) on malformed input
- [ ] **SEC-06**: `GET /api/visits/{id}/response` (teammate's device poll endpoint, currently unauthenticated) requires the same `X-Device-Key` secret

### Reliability

- [ ] **REL-01**: `init_db()` creates every table the app queries — including `visitors` and `visitor_embeddings` (with `IF NOT EXISTS` guards) — so endpoints do not 500 against a fresh database

### Demo

- [ ] **DEMO-01**: `TRIGGER_ON_BOOT` is set to `false` in `doorbell_step5_btn_io14` and the doorbell fires on a real GPIO 13 button press, verified end-to-end (press → photo → Telegram notification → reply → dashboard update → reply shown on the visitor OLED)

### Audio / Voice

- [ ] **AUD-01**: Homeowner records a voice note in Telegram as a reply to the doorbell photo message; the backend webhook captures the `voice` file and links it to that visit (via `reply_to_message` → `telegram_message_id`)
- [ ] **AUD-02**: Backend transcodes the voice note (OGG/Opus) to a low-bitrate MP3 with `ffmpeg`, stores it in Supabase Storage, and exposes an `audio_url` on the visit (returned by `GET /api/visits/{id}/response`)
- [ ] **AUD-03**: The ESP32 detects a new `audio_url` via the existing reply poll, downloads the clip, and plays it clearly through the MAX98357A + speaker over I2S; if there is no voice note, nothing plays and the OLED text reply still works
- [ ] **AUD-04**: The MAX98357A is wired to free ESP32 I2S pins (LRC/BCLK/DIN + 5 V/GND) without breaking the camera, OLED (IO14/15), or button (IO13); the final pin map is documented

### Recognition

- [ ] **REC-01** (spike gate): Face recognition produces *usable* similarity scores on real ESP32-CAM doorbell photos — same-person clearly higher than different-person, and a face is actually detected — with a chosen model (`buffalo_s`/`buffalo_l`) and detector settings; a go/no-go is documented. If no usable accuracy is reachable, recognition stays OFF and Phase 4 stops here.
- [ ] **REC-02**: With `RECOGNITION_ENABLED=1`, the backend extracts a face embedding per visit and matches it against known visitors via pgvector cosine similarity (tuned threshold), attaching `visitor_id` + name; recognition deps are pinned to exact versions; a recognition failure never blocks visit creation
- [ ] **REC-03**: The homeowner can register/name a visitor from the dashboard, and subsequent visits by that person are labeled with the name in both the dashboard and the Telegram notification
- [ ] **REC-04**: Recognition runs within the hosting memory budget — model choice + Railway plan confirmed to not OOM
- [ ] **REC-05**: End-to-end recognition is tested with at least 2–3 enrolled people across multiple visits; measured accuracy and the chosen threshold are documented

### Documentation

- [ ] **DOC-01**: README / PLAN / PROGRESS are reconciled with the actual code state (heartbeat implemented, settings model present, recognition built-but-gated) — no stale "not built" claims
- [ ] **DOC-02**: A demo runbook documents the exact live-demo steps (power-on, press, show Telegram, reply, show dashboard) plus recovery steps if something fails

## v2 Requirements

Acknowledged but deferred — not in this submission's roadmap.

### Recognition

- *(Recognition promoted to v1 — now Phase 4, see REC-01…05 above.)*

### Hardening

- **HARD-01**: Pin Telegram/Railway root CA certs on the ESP32 (replace `setInsecure()`)
- **HARD-02**: Introduce a DB connection pool (replace connection-per-query)

## Out of Scope

Explicitly excluded for this submission. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| ~~V3 face recognition~~ | **Moved into scope 2026-06-27** — now Phase 4 (REC-01…05), gated by a feasibility spike; stays OFF only if the spike fails |
| V4 motion detection (PIR) | Hardware-dependent; not core to the doorbell loop |
| ~~V4 audio response (MAX98357A + speaker)~~ | **Moved into scope 2026-06-27** — now Phase 3 (Voice Notes), see AUD-01…04 |
| V4 offline SPIFFS buffering | Reliability nicety; not required for a controlled demo |
| Flutter app | Placeholder only; web dashboard is the submission UI |
| ESP32 TLS cert pinning | Lab-prototype bar; deferred to v2 (HARD-01) |
| DB connection pooling | Acceptable at demo scale; deferred to v2 (HARD-02) |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SET-01 | Phase 1 | Pending |
| SET-02 | Phase 1 | Pending |
| SEC-01 | Phase 2 | Pending |
| SEC-02 | Phase 2 | Pending |
| SEC-03 | Phase 2 | Pending |
| SEC-04 | Phase 2 | Pending |
| SEC-05 | Phase 2 | Pending |
| SEC-06 | Phase 2 | Pending |
| REL-01 | Phase 2 | Pending |
| DEMO-01 | Phase 2 | Pending |
| AUD-01 | Phase 3 | Pending |
| AUD-02 | Phase 3 | Pending |
| AUD-03 | Phase 3 | Pending |
| AUD-04 | Phase 3 | Pending |
| REC-01 | Phase 4 | Pending |
| REC-02 | Phase 4 | Pending |
| REC-03 | Phase 4 | Pending |
| REC-04 | Phase 4 | Pending |
| REC-05 | Phase 4 | Pending |
| DOC-01 | Phase 5 | Pending |
| DOC-02 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 21 total
- Mapped to phases: 21
- Unmapped: 0

---
*Requirements defined: 2026-06-27*
*Last updated: 2026-06-27 after adding Phase 4 (face recognition) — added REC-01…05*
