---
phase: 02-security-reliability-hardening
plan: 02
type: execute
wave: 2
depends_on: ["02-01"]
files_modified:
  - backend/server.py
  - backend/telegram_bot.py
  - backend/.env
autonomous: true
requirements: [SEC-04, SEC-05]
must_haves:
  truths:
    - "POST /telegram/webhook with a wrong/missing X-Telegram-Bot-Api-Secret-Token returns 403 when TELEGRAM_WEBHOOK_SECRET is set"
    - "POST /telegram/webhook with the correct token (or when TELEGRAM_WEBHOOK_SECRET is empty) processes callbacks normally"
    - "POST /telegram/webhook with malformed reply callback_data (e.g. 'reply:' alone, or a non-numeric id) returns HTTP 200 with no IndexError/ValueError and no 500"
    - "set_webhook registers Telegram's secret_token so Telegram sends the header on every callback"
  artifacts:
    - path: "backend/server.py"
      provides: "webhook secret-token validation + bounds-checked reply callback parser"
      contains: "TELEGRAM_WEBHOOK_SECRET"
    - path: "backend/telegram_bot.py"
      provides: "set_webhook passes secret_token to Telegram setWebhook"
      contains: "secret_token"
    - path: "backend/.env"
      provides: "TELEGRAM_WEBHOOK_SECRET documented, defaulting to empty"
      contains: "TELEGRAM_WEBHOOK_SECRET"
  key_links:
    - from: "telegram_webhook (POST /telegram/webhook)"
      to: "TELEGRAM_WEBHOOK_SECRET env var"
      via: "X-Telegram-Bot-Api-Secret-Token header comparison, empty-disables-gate"
      pattern: "X-Telegram-Bot-Api-Secret-Token"
    - from: "set_webhook"
      to: "Telegram setWebhook API"
      via: "secret_token field in the JSON body"
      pattern: "secret_token"
---

<objective>
Authenticate the Telegram webhook and make the reply-callback parser crash-proof.

Purpose: Two distinct gaps. (1) `POST /telegram/webhook` is fully unauthenticated — anyone
who learns the URL can forge reply callbacks. (2) The callback parser indexes `parts[1]`/`parts[2]`
and calls `int(parts[1])` with no bounds/type check, so `reply:` alone raises IndexError and a
non-numeric id raises ValueError — each produces an HTTP 500 that Telegram retries in a loop.
Output: a secret-token check in `telegram_webhook`, a `secret_token` passthrough in `tg.set_webhook`,
a bounds-checked parser, and `TELEGRAM_WEBHOOK_SECRET` documented in `backend/.env`.

CRITICAL SAFETY PROPERTY (gate-off-when-empty): When `TELEGRAM_WEBHOOK_SECRET` is empty/unset
the webhook check is skipped. If it hard-required the secret, Telegram callbacks would die the
instant the backend deploys — before `set_webhook` is re-registered with the token. The check
turns on only once the secret is set AND setWebhook has been re-run with it.
</objective>

<execution_context>
@~/.claude/get-shit-done/workflows/execute-plan.md
@~/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/02-security-reliability-hardening/02-CONTEXT.md
@CLAUDE.md

<interfaces>
<!-- Current webhook handler + parser (backend/server.py:606-663) — the parser at 627-629 is the bug. -->
```python
@app.post("/telegram/webhook")
async def telegram_webhook(request: Request):
    body = await request.json()
    callback = body.get("callback_query")
    if not callback:
        return {"ok": True}
    callback_id = callback["id"]
    data        = callback.get("data", "")   # e.g. "reply:42:On my way"
    if not data.startswith("reply:"):
        await tg.answer_callback(callback_id, "Unknown action")
        return {"ok": True}
    # the unsafe parse (server.py:627-629)
    parts      = data.split(":", 2)
    visit_id   = int(parts[1])      # IndexError on "reply:", ValueError on non-numeric
    response   = parts[2]           # IndexError on "reply:42"
```

<!-- Current set_webhook (backend/telegram_bot.py:144-158) — extend the JSON body with secret_token. -->
```python
async def set_webhook(public_url: str):
    webhook_url = f"{public_url}/telegram/webhook"
    async with httpx.AsyncClient() as client:
        resp = await client.post(
            f"{TELEGRAM_API}/setWebhook",
            json={"url": webhook_url},
            timeout=10,
        )
    data = resp.json()
    print(f"[Telegram] setWebhook → {data}")
    return data
```

<!-- The empty-disables-gate analog to mirror for the webhook check (backend/server.py:44, 51-53). -->
```python
DASHBOARD_PASSWORD = os.getenv("DASHBOARD_PASSWORD", "").strip()
def require_dashboard_key(x_dashboard_key: str = Header(default="")):
    if DASHBOARD_PASSWORD and x_dashboard_key != DASHBOARD_PASSWORD:
        raise HTTPException(401, "Invalid dashboard password")
```
Telegram secret_token constraint: 1–256 chars, only `A-Z a-z 0-9 _ -` (the project's
generated hex value satisfies this). The wire header Telegram sends is
`X-Telegram-Bot-Api-Secret-Token`.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Validate the webhook secret token + register it via set_webhook</name>
  <files>backend/server.py, backend/telegram_bot.py, backend/.env</files>
  <read_first>
    - backend/server.py:44 and :51-53 (empty-disables-gate analog: DASHBOARD_PASSWORD + require_dashboard_key)
    - backend/server.py:606-612 (telegram_webhook signature; it already has `request: Request`)
    - backend/telegram_bot.py:144-158 (set_webhook to extend with secret_token)
    - backend/telegram_bot.py:16-17 (module-level env-read style: os.getenv(...).strip().strip('"').strip("'"))
    - backend/.env (env-var documentation style)
  </read_first>
  <action>
    In backend/server.py add a module-level `TELEGRAM_WEBHOOK_SECRET = os.getenv("TELEGRAM_WEBHOOK_SECRET", "").strip()`
    near DASHBOARD_PASSWORD (server.py:44). In `telegram_webhook` (server.py:607), read the incoming header via
    `request.headers.get("X-Telegram-Bot-Api-Secret-Token", "")` at the top of the handler and, ONLY when
    `TELEGRAM_WEBHOOK_SECRET` is truthy AND the header value does not equal it, raise `HTTPException(403, ...)`.
    When the secret is empty, skip the check entirely (empty-disables-gate). Perform the check before parsing the body.
    In backend/telegram_bot.py, add a module-level `WEBHOOK_SECRET = os.getenv("TELEGRAM_WEBHOOK_SECRET", "").strip()`
    using the existing strip-and-dequote style, and extend `set_webhook` so the setWebhook JSON body includes
    `"secret_token": WEBHOOK_SECRET` only when WEBHOOK_SECRET is non-empty (omit the field when empty so an
    empty value does not clear a previously-set token unexpectedly). In backend/.env, add a documented
    `TELEGRAM_WEBHOOK_SECRET=` line defaulting to empty, noting empty = check disabled and that
    `POST /admin/set-webhook` must be re-run after the value is set so Telegram starts sending the header.
  </action>
  <verify>
    <automated>cd backend && python -c "src=open('server.py').read(); assert 'TELEGRAM_WEBHOOK_SECRET' in src and 'X-Telegram-Bot-Api-Secret-Token' in src" && python -c "src=open('telegram_bot.py').read(); assert 'secret_token' in src" && grep -q '^TELEGRAM_WEBHOOK_SECRET=' .env && (TELEGRAM_WEBHOOK_SECRET=tok123 uvicorn server:app --port 8097 & echo $! > /tmp/sd_p2t1.pid); sleep 4; echo "wrong-token:"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8097/telegram/webhook -H "Content-Type: application/json" -H "X-Telegram-Bot-Api-Secret-Token: wrong" -d '{}'; echo "no-token:"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8097/telegram/webhook -H "Content-Type: application/json" -d '{}'; echo "correct-token-no-callback:"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8097/telegram/webhook -H "Content-Type: application/json" -H "X-Telegram-Bot-Api-Secret-Token: tok123" -d '{}'; kill "$(cat /tmp/sd_p2t1.pid)" 2>/dev/null</automated>
  </verify>
  <acceptance_criteria>
    - backend/server.py defines `TELEGRAM_WEBHOOK_SECRET` from `os.getenv("TELEGRAM_WEBHOOK_SECRET", "")`
    - backend/server.py reads `X-Telegram-Bot-Api-Secret-Token` from request headers and raises HTTPException(403) only inside an `if TELEGRAM_WEBHOOK_SECRET and ...` guard
    - With TELEGRAM_WEBHOOK_SECRET set: wrong token returns 403, missing token returns 403, correct token returns 200
    - With TELEGRAM_WEBHOOK_SECRET empty: webhook accepts requests without the header (no 403)
    - backend/telegram_bot.py set_webhook includes `secret_token` in the setWebhook JSON body when the secret is set
    - backend/.env contains an uncommented `TELEGRAM_WEBHOOK_SECRET=` line defaulting to empty
  </acceptance_criteria>
  <done>The webhook rejects forged callbacks when the secret is set, stays open when empty, and set_webhook registers the token with Telegram.</done>
</task>

<task type="auto">
  <name>Task 2: Bounds-check the reply callback parser (no IndexError / 500 retry loop)</name>
  <files>backend/server.py</files>
  <read_first>
    - backend/server.py:620-635 (the data/parts parsing and the db_execute UPDATE that follows)
    - backend/server.py:636-645 (the "visit not found" answer_callback path — reuse its clean-return style)
  </read_first>
  <action>
    Replace the unguarded parse at server.py:627-629. After confirming `data.startswith("reply:")`, split with
    `data.split(":", 2)` and validate BEFORE indexing: require exactly 3 parts AND that `parts[1]` is a valid
    integer (use a try/except around `int(parts[1])` or an isdigit check). On malformed input — fewer than 3
    parts, or a non-numeric id — call `tg.answer_callback(callback_id, ...)` with a short message and `return {"ok": True}`
    (HTTP 200) so Telegram does not retry. No exception may escape the handler for malformed callback_data. Leave
    the happy-path behavior (save response, edit message, broadcast) unchanged once visit_id and response are
    validated. Follow the existing bracketed-print logging convention if logging the malformed input
    (e.g. `print(f"[Telegram] malformed callback_data: {data!r}")`).
  </action>
  <verify>
    <automated>cd backend && (uvicorn server:app --port 8096 & echo $! > /tmp/sd_p2t2.pid); sleep 4; echo "malformed reply: only -> expect 200:"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8096/telegram/webhook -H "Content-Type: application/json" -d '{"callback_query":{"id":"cb1","data":"reply:"}}'; echo "non-numeric id -> expect 200:"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8096/telegram/webhook -H "Content-Type: application/json" -d '{"callback_query":{"id":"cb2","data":"reply:abc:Hi"}}'; echo "missing response part -> expect 200:"; curl -s -o /dev/null -w "%{http_code}\n" -X POST http://127.0.0.1:8096/telegram/webhook -H "Content-Type: application/json" -d '{"callback_query":{"id":"cb3","data":"reply:42"}}'; kill "$(cat /tmp/sd_p2t2.pid)" 2>/dev/null</automated>
  </verify>
  <acceptance_criteria>
    - POST /telegram/webhook with callback_data `reply:` returns HTTP 200 (no 500, no IndexError)
    - POST /telegram/webhook with callback_data `reply:abc:Hi` (non-numeric id) returns HTTP 200 (no ValueError)
    - POST /telegram/webhook with callback_data `reply:42` (missing response) returns HTTP 200
    - On every malformed case the handler calls answer_callback and returns `{"ok": true}` without raising
    - The valid-callback path (3 parts, numeric id) still saves the response, edits the message, and broadcasts
  </acceptance_criteria>
  <done>Malformed reply callback_data returns a clean 200 with no raised exception; valid callbacks still work end-to-end.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Telegram (or anyone) → POST /telegram/webhook | Untrusted JSON crosses here. The URL is internet-reachable; without the secret token anyone can forge a reply callback. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-02-06 | Spoofing | POST /telegram/webhook (telegram_webhook) | mitigate | validate X-Telegram-Bot-Api-Secret-Token against TELEGRAM_WEBHOOK_SECRET; 403 when set and mismatched |
| T-02-07 | Tampering | reply callback parser (server.py:627-629) | mitigate | bounds-check parts count and integer id before indexing; clean 200 on malformed input |
| T-02-08 | Denial of Service | reply callback parser → Telegram retry loop | mitigate | malformed input returns 200 instead of 500, breaking the Telegram retry storm |
| T-02-09 | Tampering | TELEGRAM_WEBHOOK_SECRET misconfiguration | accept | gate-off-when-empty is intentional so a deploy before re-running setWebhook degrades to open, not to dropped callbacks |
| T-02-10 | Repudiation | webhook with secret unset | accept | lab-prototype bar — no signed-update verification beyond the shared secret token (per CLAUDE.md) |
</threat_model>

<verification>
Boot `uvicorn server:app` locally.
1. With `TELEGRAM_WEBHOOK_SECRET` set: POST /telegram/webhook with wrong/missing token → 403; with correct token → 200.
2. With `TELEGRAM_WEBHOOK_SECRET` empty: POST with no token → processes normally (no 403).
3. Malformed callback_data (`reply:`, `reply:abc:Hi`, `reply:42`) → 200, no 500, no traceback in server logs.
</verification>

<success_criteria>
- ROADMAP Phase 2 success criteria 3 (webhook auth) and 4 (malformed-callback safety) are met.
- Gate-off-when-empty proven for the webhook secret.
- set_webhook registers the secret_token so the live deployment can enforce SEC-04 after re-running /admin/set-webhook.
</success_criteria>

<output>
Create `.planning/phases/02-security-reliability-hardening/02-02-SUMMARY.md` when done.
</output>
