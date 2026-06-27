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
- [ ] **SEC-03**: ESP32 firmware sends the `X-Device-Key` header on visit upload and heartbeat (paired with SEC-01/SEC-02), and the demo round-trip still works end-to-end
- [ ] **SEC-04**: `POST /telegram/webhook` validates the `X-Telegram-Bot-Api-Secret-Token` header; requests without the registered token are rejected
- [ ] **SEC-05**: The Telegram webhook callback parser bounds-checks `reply:`-prefixed `callback_data` and returns a clean response (no IndexError / repeated 500 retries) on malformed input

### Reliability

- [ ] **REL-01**: `init_db()` creates every table the app queries — including `visitors` and `visitor_embeddings` (with `IF NOT EXISTS` guards) — so endpoints do not 500 against a fresh database

### Demo

- [ ] **DEMO-01**: `TRIGGER_ON_BOOT` is set to `false` and the doorbell fires on a real GPIO 13 button press, verified end-to-end (press → photo → Telegram notification → reply → dashboard update)

### Documentation

- [ ] **DOC-01**: README / PLAN / PROGRESS are reconciled with the actual code state (heartbeat implemented, settings model present, recognition built-but-gated) — no stale "not built" claims
- [ ] **DOC-02**: A demo runbook documents the exact live-demo steps (power-on, press, show Telegram, reply, show dashboard) plus recovery steps if something fails

## v2 Requirements

Acknowledged but deferred — not in this submission's roadmap.

### Recognition

- **REC-01**: Validate face-recognition accuracy on real doorbell photos and enable it if it performs acceptably

### Hardening

- **HARD-01**: Pin Telegram/Railway root CA certs on the ESP32 (replace `setInsecure()`)
- **HARD-02**: Introduce a DB connection pool (replace connection-per-query)

## Out of Scope

Explicitly excluded for this submission. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| V3 face recognition (live in demo) | Unvalidated accuracy (~−0.01 similarity on test photos); stays `RECOGNITION_ENABLED=0` |
| V4 motion detection (PIR) | Hardware-dependent; not core to the doorbell loop |
| V4 audio response (MAX98357A + speaker) | Pending hardware + team decision |
| V4 offline SPIFFS buffering | Reliability nicety; not required for a controlled demo |
| Flutter app | Placeholder only; web dashboard is the submission UI |
| ESP32 TLS cert pinning | Lab-prototype bar; deferred to v2 (HARD-01) |
| DB connection pooling | Acceptable at demo scale; deferred to v2 (HARD-02) |

## Traceability

Populated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| SET-01 | TBD | Pending |
| SET-02 | TBD | Pending |
| SEC-01 | TBD | Pending |
| SEC-02 | TBD | Pending |
| SEC-03 | TBD | Pending |
| SEC-04 | TBD | Pending |
| SEC-05 | TBD | Pending |
| REL-01 | TBD | Pending |
| DEMO-01 | TBD | Pending |
| DOC-01 | TBD | Pending |
| DOC-02 | TBD | Pending |

**Coverage:**
- v1 requirements: 11 total
- Mapped to phases: 0 (pending roadmap)
- Unmapped: 11 ⚠️

---
*Requirements defined: 2026-06-27*
*Last updated: 2026-06-27 after initialization*
