# Smart Doorbell — Group 15

## What This Is

An IoT smart doorbell built on the ESP32-CAM. A visitor presses the physical
button, the device captures a photo and uploads it to a FastAPI backend, which
sends the homeowner a Telegram notification with inline reply buttons. The
homeowner's reply flows back through a Telegram webhook, and every visit —
including the reply — appears live on a web dashboard. The doorbell also shows
the homeowner's reply back to the visitor on a small OLED screen, and can play
a voice note the homeowner records in Telegram through a small speaker. This is a
university capstone project (Group 15), graded on **both a live demo and a
written report/poster**.

## Core Value

When a visitor presses the doorbell, the homeowner reliably receives a photo
notification on Telegram and can respond — and that full round-trip is visible
live on the dashboard. If everything else fails, this press → photo → notify →
reply → dashboard loop must work.

## Requirements

### Validated

<!-- Shipped and confirmed working on real hardware (verified 2026-06-10). -->

- ✓ Button press (GPIO 13) → camera capture → multipart HTTPS upload to backend — existing
- ✓ Backend sends single Telegram notification with photo + "On my way" / "Not home" reply buttons — existing
- ✓ Homeowner reply via Telegram webhook saved to DB and pushed to dashboard live — existing
- ✓ FastAPI backend on Railway + Supabase PostgreSQL + Supabase Storage (photos) — existing
- ✓ Web dashboard: Visit History, Statistics, Diagnostics, Settings (UI) — existing
- ✓ Device heartbeat (RSSI every 60s) → Diagnostics online/offline state — existing
- ✓ Quiet-hours logic enforced server-side — existing
- ✓ Secrets managed via gitignored `secrets.h` / `.env` + Railway Variables — existing
- ✓ Visitor-facing OLED reply display — ESP32 polls `GET /api/visits/{id}/response` and shows "On my way" / "Not home" to the visitor — existing (teammate, 2026-06-27)
- ✓ Reliable button debounce (two-state edge detection) — existing (teammate fix, 2026-06-27)

### Active

<!-- Submission-ready scope. Hardening + finishing loose ends; no new features. -->

- [ ] Settings fully wired backend ↔ dashboard (read + write of quiet hours, toggles), closing the partial V2 gap
- [ ] Device-endpoint auth: shared `X-Device-Key` secret on `POST /api/visits`, `POST /api/device/heartbeat`, and `GET /api/visits/{id}/response` (new teammate endpoint, currently open)
- [ ] Telegram webhook secret token validated (`X-Telegram-Bot-Api-Secret-Token`)
- [ ] Telegram webhook callback parser hardened (bounds-check on `reply:` split → no IndexError/500 retries)
- [ ] `init_db()` creates all tables it depends on (guards so enabling recognition can't crash visit creation)
- [ ] `TRIGGER_ON_BOOT` set to `false` now that the GPIO 13 button is soldered + real-button demo flow verified end-to-end
- [ ] Visitor voice playback: the homeowner's Telegram voice note is transcoded backend-side (Opus→MP3) and played on a MAX98357A + speaker at the door (new feature for this milestone)
- [ ] Docs reconciled (README / PLAN / PROGRESS match actual code state) + a clean demo runbook for the live demo

### Out of Scope

<!-- Frozen for this submission. Reasoning kept to prevent re-adding. -->

- V3 face recognition — code exists but unvalidated (buffalo_s scored ~−0.01 on test photos); stays `RECOGNITION_ENABLED=0`. Demoing a broken "New visitor every press" is worse than not demoing it.
- V4 motion detection (PIR) — hardware-dependent, not core to the doorbell loop.
- ~~V4 audio response to visitor (MAX98357A + speaker)~~ — **moved into scope 2026-06-27** as Phase 3 (Voice Notes): the homeowner records a Telegram voice note, the backend transcodes it, and the doorbell plays it on the speaker. Hardware is on hand.
- V4 offline SPIFFS buffering — reliability nicety, not required for a controlled demo.
- Flutter app (`flutter_app/`) — placeholder only; the web dashboard is the UI for this submission.

## Context

- **Brownfield.** Full codebase map at `.planning/codebase/` (analyzed 2026-06-23). Architecture, stack, and concerns are documented there.
- **Live in production** at `https://smart-doorbell-production.up.railway.app` (Railway auto-deploys on push to `main`).
- **Stack:** Arduino C++ firmware · Python 3.13 FastAPI backend · Vanilla JS single-file dashboard · Supabase Postgres/Storage · Telegram Bot API.
- **Docs drift:** PLAN.md / PROGRESS.md predate recent work — they list heartbeat and settings as unbuilt, but the code already implements heartbeat and a settings model. Reconciling this is part of the Active scope.
- **Known concerns** (full list in `.planning/codebase/CONCERNS.md`): unauthenticated device/webhook endpoints, demo-mode flag still on, connection-per-query DB (acceptable at demo scale), TLS cert checks disabled on ESP32 (lab-acceptable).

## Constraints

- **Tech stack**: Frozen — FastAPI + Supabase + Railway (backend), Arduino/ESP32-CAM (firmware), vanilla JS (dashboard). No rewrites; keep tested code.
- **Deliverable**: Must produce a working **live demo** AND a written **report/poster** — both are graded.
- **Hardware**: Single AI Thinker ESP32-CAM on a CS-CAM carrier; GPIO 13 button soldered and working; OLED on IO14/IO15.
- **Hardware (audio)**: MAX98357A I2S mono amp (Vin 2.5–5.5 V, ~9 dB default gain, 3 W) + 4 Ω 3 W speaker — on hand. Needs 3 free I2S GPIOs (LRC/BCLK/DIN) + 5 V/GND; pin budget is tight, so the free pins must be confirmed on the board. Loud playback may need a bulk capacitor on Vin to avoid brown-out.
- **Budget**: Railway trial credit (~$5/30 days) and Supabase free tier — keep the service online through demo day.
- **Security**: Lab-prototype bar — close the cheap, high-value gaps (endpoint auth, webhook token); cert pinning and connection pooling are explicitly deferred.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Submission-ready scope — freeze features at V1/V2 | Demo + report due; reliability and polish beat half-finished V3/V4 | — Pending |
| Face recognition stays OFF for the demo | Unvalidated accuracy would show "New visitor" every press | — Pending |
| Set `TRIGGER_ON_BOOT=false`, demo on the real soldered button | Button is wired and working; a real press is a stronger demo than RESET | — Pending |
| Add lightweight shared-secret auth, defer cert pinning / DB pooling | High value / low effort vs. low marginal value for a lab demo | — Pending |
| Bootstrap GSD via `~/.claude/get-shit-done` symlink to plugin cache 2.45.9 | `/plugin` unavailable in VS Code; symlink unblocks the runtime | ⚠️ Revisit (reinstall from desktop app later) |
| Demo firmware is `doorbell_step5_btn_io14` (button + OLED reply) | Teammate built visitor-reply OLED; richer demo than the LED-only sketch | — Pending |
| Add Telegram voice-note playback (Phase 3) before finalizing | Richer two-way interaction; MAX98357A + 4 Ω speaker on hand | — Pending |
| Transcode voice notes Opus→MP3 on the backend, not the ESP32 | ESP32 can't realistically decode Opus; keeps firmware simple | — Pending |
| De-risk audio with a spike before planning Phase 3 | I2S pin budget + playback are the project's biggest unknowns | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-27 after adding Phase 3 (Telegram voice-note playback)*
