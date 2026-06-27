# Smart Doorbell — Group 15

## What This Is

An IoT smart doorbell built on the ESP32-CAM. A visitor presses the physical
button, the device captures a photo and uploads it to a FastAPI backend, which
sends the homeowner a Telegram notification with inline reply buttons. The
homeowner's reply flows back through a Telegram webhook, and every visit —
including the reply — appears live on a web dashboard. The doorbell also shows
the homeowner's reply — and, when face recognition is enabled, a greeting with the
returning visitor's name — to the visitor on a small OLED screen. This is a
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
- [ ] Face recognition working + tested: returning visitors identified by name — shown in the dashboard, Telegram, **and on the door OLED** — by enabling `recognition.py`, *if* it proves feasible on the real camera (a feasibility spike gates this)
- [ ] Docs reconciled (README / PLAN / PROGRESS match actual code state) + a clean demo runbook for the live demo

### Out of Scope

<!-- Frozen for this submission. Reasoning kept to prevent re-adding. -->

- ~~V3 face recognition~~ — **moved into scope 2026-06-27** as Phase 4 (Visitor Recognition). Prior testing scored ~−0.01 similarity (no usable match) on real photos, so a feasibility spike gates this phase — if accuracy can't be achieved on the ESP32-CAM, it stays `RECOGNITION_ENABLED=0` (a broken "New visitor every press" is worse than none; OFF remains a valid ship state).
- V4 motion detection (PIR) — hardware-dependent, not core to the doorbell loop.
- V4 audio response to visitor (MAX98357A + speaker) — **out of scope again 2026-06-27**: it can't share the ESP32-CAM pins with the OLED, and the OLED won (it also displays recognized visitor names). Hardware is on hand for a future board revision.
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
- **Hardware (audio, shelved)**: MAX98357A amp + 4 Ω 3 W speaker are on hand but NOT used — they can't share pins with the OLED on the ESP32-CAM (only D2/D4/D12 remain, all strapping/flash pins), and the OLED was kept. Available for a future board revision.
- **Hosting (recognition)**: InsightFace model is ~150 MB (`buffalo_s`) to ~600 MB (`buffalo_l`); the Railway free tier (~512 MB) may OOM with `buffalo_l`. Recognition feasibility AND fit-on-Railway must both be confirmed — a Railway plan bump may be required.
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
| Chose the OLED over the speaker (they can't coexist on the board) | No clean I²S pins free while the OLED holds D14/D15; OLED also displays recognized names, pairing it with recognition | ✓ Good |
| Use the OLED to display recognized visitor names | OLED + recognition reinforce each other; richer than either alone | — Pending |
| Add face recognition (Phase 4), gated by a feasibility spike | Team wants name-ID of returning visitors; but it failed accuracy testing before (~−0.01) | — Pending |
| Keep recognition OFF if it can't hit usable accuracy on the real camera | A broken "New visitor every press" demo is worse than none; OFF is a valid ship state | — Pending |

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
*Last updated: 2026-06-27 — dropped the speaker (pin budget); focusing on face recognition (now Phase 3, with OLED name display)*
