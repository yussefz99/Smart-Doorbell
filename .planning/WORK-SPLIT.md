# Work Split — Rami & Yussef

**Milestone:** Submission-Ready Hardening + Recognition (4 phases, 18 requirements)
**Created:** 2026-06-27

## Ownership

| Owner | Domain | Files |
|-------|--------|-------|
| **Rami** | Backend correctness + face recognition + Railway/Supabase | `backend/server.py`, `backend/recognition.py`, `backend/telegram_bot.py`, `railpack.json`, Railway/Supabase config |
| **Yussef** | Everything else: dashboard frontend + ESP32 firmware + hardware/assembly | `backend/dashboard.html`, `ESP32/doorbell_step5_btn_io14/`, physical build |

Tags below: **[R]** = Rami, **[Y]** = Yussef, **[BOTH]**.

---

## ⚠️ Handshakes — agree on these Day 1 (they unblock parallel work)

These are the interfaces where backend (Rami) meets frontend/firmware (Yussef). Lock them first so you can each build against a known contract.

1. **`X-Device-Key`** — Rami picks the header name (`X-Device-Key`) + the env var (`DEVICE_SECRET`) and the value; Yussef puts the same value in firmware `secrets.h`. *(Blocks SEC-03.)*
2. **`/api/visits/{id}/response` payload** — Rami adds `visitor_name` to the JSON (keeps `response`); Yussef's OLED reads both. Agreed shape: `{ "id", "response", "visitor_name" }`. *(Blocks REC-06.)*
3. **Visitors API for registration** — Rami confirms the endpoints (`GET /api/visitors`, `PATCH /api/visitors/{id}` to set a name); Yussef builds the dashboard UI against them. *(Blocks REC-03 frontend.)*
4. **Real face photos** — Yussef captures several real doorbell photos (2+ of the same person, a couple of different people) and hands them to Rami for the recognition spike. *(Blocks REC-01.)*

---

## Phase 1 — Settings Wiring  *(Yussef leads, frontend)*

- [ ] **[Y] SET-01** — In `dashboard.html`, fetch `GET /api/settings` on Settings open and pre-fill quiet-hours fields; `PUT /api/settings` on save.
- [ ] **[Y] SET-02** — Remove or clearly label dashboard controls that aren't backed by the API (motion sliders, device toggles).
- [ ] **[R]** (small) — Confirm `/api/settings` is complete; add `timezone` to `SettingsUpdate` only if you want it editable (optional).

## Phase 2 — Security + Reliability  *(split)*

- [ ] **[R] SEC-01** — Require `X-Device-Key` on `POST /api/visits` (reject without it).
- [ ] **[R] SEC-02** — Require `X-Device-Key` on `POST /api/device/heartbeat`.
- [ ] **[R] SEC-04** — Validate `X-Telegram-Bot-Api-Secret-Token` on `POST /telegram/webhook` (register the token via `setWebhook`).
- [ ] **[R] SEC-05** — Bounds-check the `reply:` callback parser (no IndexError / 500 retry loop).
- [ ] **[R] REL-01** — `init_db()` creates `visitors` + `visitor_embeddings` (with pgvector + `IF NOT EXISTS`). **This also sets up the recognition tables.**
- [ ] **[Y] SEC-03** — Firmware sends `X-Device-Key` on visit upload + heartbeat (needs handshake #1).
- [ ] **[Y] DEMO-01** — Set `TRIGGER_ON_BOOT false`, flash `doorbell_step5_btn_io14`, verify the real-button round-trip end-to-end. *(Bundle SEC-03 into this same flash — one upload.)*

## Phase 3 — Visitor Recognition  *(gated by Rami's spike)*

- [ ] **[R] REC-01 (SPIKE — DO FIRST)** — Run InsightFace on Yussef's real photos; check a face is detected + same-person vs different-person similarity. **Go/no-go.** If no-go → recognition stays OFF, skip the rest of this phase.
- [ ] **[R] REC-02** — `RECOGNITION_ENABLED=1` path: embed each visit, match via pgvector (tuned threshold), attach `visitor_id` + name; failures never block visit creation; pin recognition deps.
- [ ] **[R] REC-04** — Confirm the model fits Railway memory (model choice / plan); no OOM.
- [ ] **[R] REC-03 (backend)** — Visitors API + include recognized name in the Telegram notification + dashboard data.
- [ ] **[Y] REC-03 (frontend)** — Dashboard UI to name/register a visitor (needs handshake #3).
- [ ] **[R] REC-06 (backend)** — Add `visitor_name` to `/api/visits/{id}/response` (handshake #2).
- [ ] **[Y] REC-06 (firmware)** — OLED shows "Welcome, [Name]" when recognized; text-reply still works.
- [ ] **[BOTH] REC-05** — Enroll 2–3 people, test across multiple visits; Rami records accuracy + threshold, Yussef runs the hardware.

## Phase 4 — Demo Readiness + Documentation  *(split, last)*

- [ ] **[R] DOC-01 (backend/recognition)** — Update README/PROGRESS for the backend, security, and recognition state.
- [ ] **[Y] DOC-01 (firmware/hardware)** — Update PLAN/README for the firmware, OLED, wiring, pin map.
- [ ] **[BOTH] DOC-02** — Write + rehearse the demo runbook (power-on → press → photo → Telegram → reply → OLED → dashboard), with recovery steps.

---

## Recommended order (maximize parallelism)

1. **Now:** agree the 4 handshakes. Rami → **REC-01 spike** (the gate). Yussef → **Phase 1** + capture face photos for Rami.
2. **Next:** Rami → Phase 2 backend (SEC + REL-01). Yussef → Phase 2 firmware (SEC-03 + DEMO-01, single flash).
3. **If spike passed:** Phase 3 in parallel — Rami backend/recognition, Yussef registration UI + OLED name.
4. **Last:** Phase 4 docs split + joint demo rehearsal.

## The gate that matters
If **REC-01** shows recognition can't work on the real camera, both pivot: recognition stays OFF, polish + demo the rest. You still ship a complete, hardened doorbell with the OLED text reply — a solid project either way.
