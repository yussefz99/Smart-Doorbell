---
status: partial
phase: 02-security-reliability-hardening
source: [02-VERIFICATION.md]
started: 2026-06-27
updated: 2026-06-27
---

## Current Test

[awaiting hardware confirmation by Yussef]

## Tests

### 1. DEMO-01 — physical GPIO-13 round-trip (teammate-owned, hardware)
expected: On the flashed `doorbell_step5_btn_io14` build (commit 770a43e, `TRIGGER_ON_BOOT false`),
pressing the soldered GPIO-13 button produces: photo notification in Telegram → homeowner reply →
live dashboard update → reply shown on the visitor OLED. Backend endpoints accept the firmware's
`X-Device-Key` requests (already verified in code/live).
result: [pending — run on Yussef's bench]

note: To enforce auth end-to-end during this test, set `DEVICE_SECRET` on Railway AND the matching
`SECRET_DEVICE_KEY` in firmware `secrets.h`, and set `TELEGRAM_WEBHOOK_SECRET` on Railway + re-run
`POST /admin/set-webhook`. Until then the gates stay OFF (open) and the device keeps working.

## Summary

total: 1
passed: 0
issues: 0
pending: 1
skipped: 0
blocked: 0

## Gaps
