# ============================================================
#  Smart Doorbell – Telegram Bot Helper
#  Handles: sending photos with reply buttons,
#           answering callback queries,
#           registering the webhook with Telegram
# ============================================================

import os
import httpx
from dotenv import load_dotenv

load_dotenv()

BOT_TOKEN = os.getenv("BOT_TOKEN", "")
CHAT_ID   = os.getenv("CHAT_ID",   "")

TELEGRAM_API = f"https://api.telegram.org/bot{BOT_TOKEN}"


# ── Send photo + reply buttons ────────────────────────────────
async def send_visit_notification(visit: dict) -> int | None:
    """
    Sends a Telegram photo notification with two inline buttons.
    Returns the Telegram message_id so we can link callbacks back
    to the visit in the database.
    """
    visit_id = visit["id"]
    trigger  = visit["trigger"]
    ts       = visit["timestamp"][:16].replace("T", " ")
    silent   = visit.get("silent", False)

    caption = (
        f"{'🔔 Doorbell pressed' if trigger == 'button' else '👁 Motion detected'}\n"
        f"🕐 {ts}\n"
        f"{'🔕 Silent mode — recorded only' if silent else ''}"
    ).strip()

    # Inline keyboard with two callback buttons
    reply_markup = {
        "inline_keyboard": [[
            {"text": "✅ On my way",  "callback_data": f"reply:{visit_id}:On my way"},
            {"text": "🏠 Not home",   "callback_data": f"reply:{visit_id}:Not home"},
        ]]
    }

    photo_url = visit.get("photo_url")

    async with httpx.AsyncClient() as client:
        if photo_url:
            # Send actual photo from our server
            full_url = f"http://localhost:8000{photo_url}"
            resp = await client.post(
                f"{TELEGRAM_API}/sendPhoto",
                json={
                    "chat_id":      CHAT_ID,
                    "photo":        full_url,
                    "caption":      caption,
                    "reply_markup": reply_markup,
                },
                timeout=10,
            )
        else:
            # No photo — send text message instead
            resp = await client.post(
                f"{TELEGRAM_API}/sendMessage",
                json={
                    "chat_id":      CHAT_ID,
                    "text":         caption + "\n\n⚠️ No photo available",
                    "reply_markup": reply_markup,
                },
                timeout=10,
            )

    data = resp.json()
    if data.get("ok"):
        return data["result"]["message_id"]
    else:
        print(f"[Telegram] Send failed: {data}")
        return None


# ── Answer a callback query (removes loading spinner on button) ──
async def answer_callback(callback_query_id: str, text: str):
    async with httpx.AsyncClient() as client:
        await client.post(
            f"{TELEGRAM_API}/answerCallbackQuery",
            json={"callback_query_id": callback_query_id, "text": text},
            timeout=5,
        )


# ── Edit message to remove buttons after reply ────────────────
async def remove_buttons(message_id: int, new_caption: str):
    """Replace inline buttons with a plain confirmation text."""
    async with httpx.AsyncClient() as client:
        await client.post(
            f"{TELEGRAM_API}/editMessageCaption",
            json={
                "chat_id":      CHAT_ID,
                "message_id":   message_id,
                "caption":      new_caption,
                "reply_markup": {"inline_keyboard": []},
            },
            timeout=5,
        )


# ── Register webhook with Telegram ───────────────────────────
async def set_webhook(public_url: str):
    """
    Tell Telegram where to send updates.
    Call this once after starting ngrok with the ngrok public URL.
    """
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


# ── Get current webhook info ──────────────────────────────────
async def get_webhook_info():
    async with httpx.AsyncClient() as client:
        resp = await client.get(f"{TELEGRAM_API}/getWebhookInfo", timeout=5)
    return resp.json()


# ── Delete webhook (switch back to polling / no webhook) ─────
async def delete_webhook():
    async with httpx.AsyncClient() as client:
        resp = await client.post(f"{TELEGRAM_API}/deleteWebhook", timeout=5)
    return resp.json()
