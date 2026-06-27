---
phase: 02-security-reliability-hardening
plan: 02
requirements: [SEC-04, SEC-05]
status: complete
---

# Plan 02-02: Webhook Auth + Callback Hardening — Summary

## What was built
Authenticated the Telegram webhook and made the reply-callback parser crash-proof.

## Changes
- `backend/server.py`:
  - New constant `TELEGRAM_WEBHOOK_SECRET = os.getenv("TELEGRAM_WEBHOOK_SECRET", "").strip()`.
  - SEC-04: in `telegram_webhook`, reject with `HTTPException(403)` when the secret is set and
    `X-Telegram-Bot-Api-Secret-Token` mismatches. Placed at the **top** of the handler, before
    body parsing — so it guards both the typed-reply path (Yussef's `c09854c`) and the inline-button path.
  - SEC-05: bounds-checked the `reply:` parser — requires 3 colon-parts and a numeric id before
    indexing; malformed input logs and returns `{"ok": true}` (HTTP 200) instead of raising.
- `backend/telegram_bot.py`:
  - New `WEBHOOK_SECRET` constant; `set_webhook` now includes `secret_token` in the setWebhook
    body when set (omitted when empty so it never clears an existing token).
- `backend/.env` (gitignored): documented `TELEGRAM_WEBHOOK_SECRET=` (empty by default).

## Verification (live, via venv)
- **SEC-04 gate ON** (`TELEGRAM_WEBHOOK_SECRET=tok123`): wrong token → 403, no token → 403, correct token → 200.
- **SEC-04 gate OFF** (empty): webhook accepts with no header → 200.
- **SEC-05**: `reply:`, `reply:abc:Hi`, `reply:42` all → 200, no tracebacks in server log.

## Requirements
- SEC-04 (webhook auth) and SEC-05 (callback parser bounds-check) — met.
- Gate-off-when-empty proven for the webhook secret (a deploy before re-running setWebhook
  degrades to open, never to dropped callbacks).

## Operator follow-up
After setting `TELEGRAM_WEBHOOK_SECRET` on Railway, re-run `POST /admin/set-webhook` so Telegram
starts sending the header (otherwise valid callbacks would 403).

## Commit
- `838febd` feat(02-02): authenticate Telegram webhook + harden callback parser

## Self-Check: PASSED
- Happy-path callback flow (save response, edit message, broadcast) left unchanged.
- Yussef's typed-reply handler preserved and now also protected by the secret-token check.
