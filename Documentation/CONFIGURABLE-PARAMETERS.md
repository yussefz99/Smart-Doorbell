# Configurable Parameters — ESP32 Firmware

All tunable parameters live at the **top of the final sketch**
[`ESP32/ESP32.ino`](../ESP32/ESP32.ino).
Edit them there before flashing. Secrets are kept out of the sketch in a
gitignored `secrets.h` (copy from `secrets.h.example`).

---

## 1. Secrets — `secrets.h` (never committed)

Copy `ESP32/secrets.h.example` to `secrets.h` and fill in:

| Macro | What it is |
|-------|------------|
| `SECRET_WIFI_SSID` | Your WiFi network name |
| `SECRET_WIFI_PASSWORD` | Your WiFi password |
| `SECRET_BOT_TOKEN` | Telegram bot token (from @BotFather) |
| `SECRET_CHAT_ID` | Telegram chat id that receives notifications |
| `SECRET_DEVICE_KEY` | Shared key the device sends to authenticate to the backend (must match the backend's `DEVICE_SECRET`). Falls back to a placeholder if not set, so the sketch still compiles. |

---

## 2. Backend

| Parameter | Line | Default | Meaning |
|-----------|------|---------|---------|
| `BACKEND_URL` | ~54 | `https://smart-doorbell-production.up.railway.app` | The backend the device uploads visits/heartbeats to. **Leave empty (`""`) to skip the backend and send photos straight to Telegram.** |

---

## 3. Pins

| Parameter | Line | Value | Meaning |
|-----------|------|-------|---------|
| `BUTTON_PIN` | ~64 | `13` | The doorbell button (IO13, `INPUT_PULLUP`; other leg → GND). ⚠️ GPIO 0 can't be used — it's the camera clock. |
| `FLASH_LED` | ~67 | `4` | Built-in white flash LED, used as the "capturing" indicator. |
| `OLED_SDA` | ~27 | `14` | OLED I²C data line (IO14). |
| `OLED_SCL` | ~28 | `15` | OLED I²C clock line (IO15). |
| `OLED_X` | ~29 | `4` | Horizontal text offset on the OLED (px). Increase to shift text right. |

> Camera data/control pins (`Y2..Y9`, `XCLK`, `PCLK`, `VSYNC`, `HREF`, `SIOD`,
> `SIOC`, `PWDN`, `RESET`) are fixed by the AI-Thinker ESP32-CAM board and should
> not be changed.

---

## 4. OLED display driver

The panel driver is selected by which line is uncommented (~30–31):

```cpp
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C oled(...);  // use this for SSD1306 panels
U8G2_SH1106_128X64_NONAME_F_SW_I2C   oled(...);    // active: SH1106 (common 1.3")
```

If the screen shows garbage or a shifted image, switch to the other line — your
panel is the other controller. Requires the **U8g2** library.

---

## 5. Behaviour flags

| Parameter | Line | Default | Meaning |
|-----------|------|---------|---------|
| `TRIGGER_ON_BOOT` | ~73 | `false` | Demo mode. When `true`, the device fires one doorbell event on boot (so the RESET button acts as the doorbell). Set `false` once the physical button is wired. |

---

## 6. Timing

| Parameter | Line | Default | Meaning |
|-----------|------|---------|---------|
| `DEBOUNCE_MS` | ~98 | `50` | Button must be stable this long before a press is accepted. |
| `MIN_PRESS_GAP` | ~99 | `3000` | Minimum milliseconds between two doorbell triggers (anti-spam). |
| `HEARTBEAT_MS` | ~105 | `60000` | How often the device reports WiFi signal to the backend (online/offline status). |
| `REPLY_WATCH_MS` | ~134 | `180000` | After a visit, keep watching for the homeowner's reply for up to this long (3 min). |
| `REPLY_POLL_MS` | ~135 | `4000` | How often to poll the backend for a reply during the watch window. |

---

## 7. Camera quality

| Parameter | Line | Default | Meaning |
|-----------|------|---------|---------|
| `config.frame_size` | ~437 | `FRAMESIZE_VGA` | Photo resolution (640×480). Larger = more detail, bigger upload. |
| `config.jpeg_quality` | ~438 | `10` | JPEG quality (lower number = higher quality, larger file). |

---

### Quick-start checklist
1. Copy `secrets.h.example` → `secrets.h`, fill in WiFi + Telegram + device key.
2. Confirm `BACKEND_URL` points at your backend (or `""` for Telegram-only).
3. Match the OLED driver line to your panel (SSD1306 vs SH1106).
4. Set `TRIGGER_ON_BOOT false` for normal button operation.
5. Flash: board *AI Thinker ESP32-CAM*, 115200 baud (U8g2 library installed).
