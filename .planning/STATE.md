---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: unknown
last_updated: "2026-06-27T18:25:04.865Z"
progress:
  total_phases: 4
  completed_phases: 0
  total_plans: 3
  completed_plans: 0
  percent: 0
---

# Project State: Smart Doorbell — Group 15

**Last updated:** 2026-06-27
**Milestone:** Submission-Ready Hardening

---

## Project Reference

**Core Value:** When a visitor presses the doorbell, the homeowner reliably gets a photo notification on Telegram, can respond, and the full round-trip is visible live on the dashboard.

**Current Focus:** Phase 02 — security-reliability-hardening

**Production URL:** https://smart-doorbell-production.up.railway.app

---

## Current Position

Phase: 02 (security-reliability-hardening) — EXECUTING
Plan: 1 of 3
| Field | Value |
|-------|-------|
| Phase | 1 — Settings Wiring |
| Plan | None started |
| Status | Not started |
| Phase Progress | 0% |

**Overall Progress:**

```
Phase 1 [          ] 0%
Phase 2 [          ] 0%
Phase 3 [          ] 0%
```

---

## Phase Summary

| Phase | Requirements | Status |
|-------|-------------|--------|
| 1 — Settings Wiring | SET-01, SET-02 | Not started |
| 2 — Security + Reliability Hardening | SEC-01, SEC-02, SEC-03, SEC-04, SEC-05, REL-01, DEMO-01 | Not started |
| 3 — Demo Readiness + Documentation | DOC-01, DOC-02 | Not started |

---

## Accumulated Context

### Key Decisions (from PROJECT.md)

- Face recognition stays OFF for the demo — unvalidated accuracy (~−0.01 similarity) would show "New visitor" every press
- `TRIGGER_ON_BOOT` must be set to `false` before the real-button demo (GPIO 13 is soldered and working)
- Lightweight shared-secret auth added (Phase 2); cert pinning and DB pooling deferred to v2

### Known Constraints

- Railway trial credit (~$5/30 days) — keep service online through demo day
- Supabase free tier — recognition stays disabled (`RECOGNITION_ENABLED=0`)
- Tech stack frozen: no rewrites, no new frameworks
- ESP32 uses `setInsecure()` (no cert pinning) — lab-prototype bar, accepted

### Active Risks

- `TRIGGER_ON_BOOT true` in current firmware — any power cycle sends a real doorbell event before Phase 2 fixes this
- SEC-03 (firmware X-Device-Key header) and DEMO-01 (real-button verification) both require flashing the ESP32 — sequence carefully: flash once with both changes together
- Railway credit exhaustion — monitor before demo day

### Blockers

None currently.

### Session Notes

- Roadmap initialized 2026-06-27 from brownfield analysis (codebase analyzed 2026-06-23)
- Existing features confirmed working: button press, Telegram notification, reply, dashboard, heartbeat, quiet hours, secrets management
- Docs drift known: PLAN.md / PROGRESS.md predate recent work — reconciled in Phase 3

---

## Performance Metrics

| Metric | Value |
|--------|-------|
| Requirements mapped | 11/11 |
| Phases defined | 3 |
| Plans created | 0 |
| Plans complete | 0 |
| Phases complete | 0 |

---

*State initialized: 2026-06-27*
