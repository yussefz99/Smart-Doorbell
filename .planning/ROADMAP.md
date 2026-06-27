# Roadmap: Smart Doorbell — Group 15

**Milestone:** Submission-Ready Hardening + Voice Notes + Recognition
**Granularity:** Coarse (5 phases)
**Coverage:** 21/21 v1 requirements mapped

---

## Phases

- [ ] **Phase 1: Settings Wiring** - Quiet-hours settings fully round-trip between backend and dashboard; no misleading controls on the Settings page
- [ ] **Phase 2: Security + Reliability Hardening** - Device and webhook endpoints authenticated; callback parser hardened; database bootstraps all tables; firmware updated and real-button round-trip verified
- [ ] **Phase 3: Voice Notes (Speaker)** - Homeowner's Telegram voice note is transcoded backend-side and played on the doorbell speaker for the visitor
- [ ] **Phase 4: Visitor Recognition** - Returning visitors identified by name (feasibility-gated); names shown in dashboard + Telegram; tested end-to-end
- [ ] **Phase 5: Demo Readiness + Documentation** - Docs reconciled with actual code state; live-demo runbook written and rehearsed

---

## Phase Details

### Phase 1: Settings Wiring
**Goal**: The dashboard Settings page fully reflects real backend state — quiet hours load on open and persist on save; no misleading dead controls remain
**Depends on**: Nothing (first phase — backend-only and dashboard-only changes, no hardware)
**Requirements**: SET-01, SET-02
**Success Criteria** (what must be TRUE):
  1. Opening the Settings page fetches current quiet-hours values (enabled flag, start time, end time) from `GET /api/settings` and pre-populates the form controls
  2. Saving Settings POSTs the changed values to `PUT /api/settings` and the updated values are readable back from the API (they persisted)
  3. Quiet-hours enforcement on the next doorbell press reflects the saved settings without a server restart
  4. Any dashboard control that is not wired to the backend is removed or clearly labeled — no control silently does nothing during the demo
**Plans**: TBD

### Phase 2: Security + Reliability Hardening
**Goal**: All three device endpoints (visit upload, heartbeat, reply poll) require a shared secret, the Telegram webhook is authenticated, the callback parser cannot crash, the database bootstraps safely — and the physical button on the OLED demo build (`doorbell_step5_btn_io14`) triggers a verified end-to-end round-trip
**Depends on**: Phase 1
**Requirements**: SEC-01, SEC-02, SEC-03, SEC-04, SEC-05, SEC-06, REL-01, DEMO-01
**Success Criteria** (what must be TRUE):
  1. `POST /api/visits` without the correct `X-Device-Key` header returns 401 or 403; with the correct header returns 201
  2. `POST /api/device/heartbeat` without the correct `X-Device-Key` header returns 401 or 403; with the correct header returns 200
  3. `POST /telegram/webhook` with no `X-Telegram-Bot-Api-Secret-Token` (or a wrong value) returns 403; Telegram's registered token is accepted and callbacks are processed normally
  4. Sending a malformed `callback_data` (e.g., `reply:` with no further parts) to the webhook endpoint returns 200 with a clean error body — no IndexError, no HTTP 500 retry loop
  5. A fresh-database app start (tables dropped) completes `init_db()` and `POST /api/visits` succeeds — no 500 from a missing `visitors` or `visitor_embeddings` table
  6. Physical GPIO 13 button press on the live ESP32 demo build (`doorbell_step5_btn_io14`, with `TRIGGER_ON_BOOT false` and `X-Device-Key` wired in firmware) produces a photo notification in Telegram, a live dashboard update, and the homeowner's reply shown on the visitor OLED — full round-trip confirmed
  7. `GET /api/visits/{id}/response` without the correct `X-Device-Key` returns 401 or 403; with the correct header it returns the visit's reply text
**Plans**: TBD

### Phase 3: Voice Notes (Speaker)
**Goal**: The homeowner can record a voice note in Telegram and have it play on the doorbell's speaker for the visitor — without breaking the existing camera, OLED, or button
**Depends on**: Phase 2 (shares the firmware build → another flash). De-risked by an audio spike before planning.
**Requirements**: AUD-01, AUD-02, AUD-03, AUD-04
**Success Criteria** (what must be TRUE):
  1. Replying to a doorbell photo in Telegram with a voice note causes the backend to store an audio file and attach its `audio_url` to that visit
  2. `GET /api/visits/{id}/response` returns the `audio_url` once the voice note is processed
  3. The ESP32 downloads and plays the clip clearly through the MAX98357A + speaker within a few seconds of the reply
  4. The MAX98357A is wired to free I2S pins (LRC/BCLK/DIN + 5 V/GND) with the camera, OLED, and button all still working; the final pin map is documented
  5. If there is no voice note, nothing plays and the OLED text reply still works (graceful fallback)
**Plans**: TBD

### Phase 4: Visitor Recognition
**Goal**: Returning visitors are identified by name end-to-end — but only if recognition proves feasible on the real ESP32-CAM camera; otherwise it stays disabled and this phase stops at the feasibility gate
**Depends on**: Phase 2 (REL-01 creates the `visitors` / `visitor_embeddings` tables + pgvector). Independent of Phase 3 (no firmware) — can run in parallel with the audio work. De-risked by a feasibility spike first.
**Requirements**: REC-01, REC-02, REC-03, REC-04, REC-05
**Success Criteria** (what must be TRUE):
  1. [GATE] On real doorbell photos, the same person scores clearly higher similarity than different people with the chosen model/settings, and a face is actually detected — the go/no-go is recorded (if no-go, recognition stays OFF and that is an acceptable outcome)
  2. With `RECOGNITION_ENABLED=1`, each visit gets a face embedding matched against known visitors; a recognized visit shows the correct name
  3. A homeowner can name a visitor from the dashboard, and that person is recognized by name on their next visit (dashboard + Telegram)
  4. A recognition failure (no face, model error) never breaks visit creation — the visit still saves and notifies
  5. The model runs within the hosting memory budget without OOM; tested with 2–3 enrolled people across multiple visits, with accuracy + threshold documented
**Plans**: TBD

### Phase 5: Demo Readiness + Documentation
**Goal**: All project documents accurately describe the implemented system and the team has a written runbook to execute and recover the live demo
**Depends on**: Phase 4 (docs must describe the final system, including recognition status)
**Requirements**: DOC-01, DOC-02
**Success Criteria** (what must be TRUE):
  1. README, PLAN.md, and PROGRESS.md contain no claims of unbuilt features that the code already implements (heartbeat, settings model, recognition-gated) — a grader reading the docs sees an accurate picture
  2. The demo runbook lists exact steps: power-on ESP32, press GPIO 13, confirm Telegram notification with photo, send a reply, confirm dashboard update, show Statistics and Diagnostics panels
  3. The runbook includes at least two recovery steps covering the most likely demo failures (ESP32 not connecting, Railway cold-start delay)
**Plans**: TBD

---

## Progress Table

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Settings Wiring | 0/? | Not started | - |
| 2. Security + Reliability Hardening | 0/? | Not started | - |
| 3. Voice Notes (Speaker) | 0/? | Not started | - |
| 4. Visitor Recognition | 0/? | Not started | - |
| 5. Demo Readiness + Documentation | 0/? | Not started | - |

---

*Roadmap created: 2026-06-27*
*Last updated: 2026-06-27 (added Phase 4 Visitor Recognition; Demo+Docs → Phase 5)*
