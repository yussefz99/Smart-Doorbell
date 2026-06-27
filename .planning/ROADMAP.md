# Roadmap: Smart Doorbell — Group 15

**Milestone:** Submission-Ready Hardening
**Granularity:** Coarse (3 phases)
**Coverage:** 11/11 v1 requirements mapped

---

## Phases

- [ ] **Phase 1: Settings Wiring** - Quiet-hours settings fully round-trip between backend and dashboard; no misleading controls on the Settings page
- [ ] **Phase 2: Security + Reliability Hardening** - Device and webhook endpoints authenticated; callback parser hardened; database bootstraps all tables; firmware updated and real-button round-trip verified
- [ ] **Phase 3: Demo Readiness + Documentation** - Docs reconciled with actual code state; live-demo runbook written and rehearsed

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
**Goal**: Device endpoints require a shared secret, the Telegram webhook is authenticated, the callback parser cannot crash, the database bootstraps safely — and the physical button triggers a verified end-to-end round-trip
**Depends on**: Phase 1
**Requirements**: SEC-01, SEC-02, SEC-03, SEC-04, SEC-05, REL-01, DEMO-01
**Success Criteria** (what must be TRUE):
  1. `POST /api/visits` without the correct `X-Device-Key` header returns 401 or 403; with the correct header returns 201
  2. `POST /api/device/heartbeat` without the correct `X-Device-Key` header returns 401 or 403; with the correct header returns 200
  3. `POST /telegram/webhook` with no `X-Telegram-Bot-Api-Secret-Token` (or a wrong value) returns 403; Telegram's registered token is accepted and callbacks are processed normally
  4. Sending a malformed `callback_data` (e.g., `reply:` with no further parts) to the webhook endpoint returns 200 with a clean error body — no IndexError, no HTTP 500 retry loop
  5. A fresh-database app start (tables dropped) completes `init_db()` and `POST /api/visits` succeeds — no 500 from a missing `visitors` or `visitor_embeddings` table
  6. Physical GPIO 13 button press on the live ESP32 (with `TRIGGER_ON_BOOT false` and `X-Device-Key` header wired in firmware) produces a photo notification in Telegram and a live dashboard update — full round-trip confirmed
**Plans**: TBD

### Phase 3: Demo Readiness + Documentation
**Goal**: All project documents accurately describe the implemented system and the team has a written runbook to execute and recover the live demo
**Depends on**: Phase 2 (docs must describe the hardened, verified final state)
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
| 3. Demo Readiness + Documentation | 0/? | Not started | - |

---

*Roadmap created: 2026-06-27*
*Last updated: 2026-06-27*
