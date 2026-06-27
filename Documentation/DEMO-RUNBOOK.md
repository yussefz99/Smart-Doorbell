# Smart Doorbell — Live Demo Runbook

Step-by-step script to run (and recover) the live demo.
Core loop being shown: **press → photo → Telegram → reply → OLED → dashboard.**

---

## A. Before the demo (5-minute checklist)

- [ ] **Power:** ESP32-CAM on a **5 V wall charger** (not a laptop port — avoids the camera brownout). POWER switch ON.
- [ ] **Firmware:** `doorbell_step5_btn_io14` flashed, with `TRIGGER_ON_BOOT = false`.
- [ ] **Device key:** `secrets.h` `SECRET_DEVICE_KEY` matches Railway `DEVICE_SECRET` (only matters once backend enforces it — see Recovery #5).
- [ ] **WiFi:** the SSID in `secrets.h` is reachable; signal decent (move near the router — RSSI has been weak).
- [ ] **Warm the backend:** open the dashboard URL once so Railway isn't cold-starting during the demo:
      `https://smart-doorbell-production.up.railway.app`
- [ ] **Telegram:** the homeowner's chat with the bot is open on a phone (to tap/type the reply).
- [ ] **Dashboard:** open and unlocked (password entered) on the presentation screen.
- [ ] **OLED:** shows `Ready` + the IP address (confirms WiFi + display OK).

---

## B. The demo (the round-trip)

1. **Show the device.** OLED reads `Ready` with the IP — point out it's online.
2. **Press the doorbell button.**
   - OLED: `Ding dong!` → `Capturing...` (flash LED fires) → `Sending...`
3. **Telegram notification** arrives on the phone — photo of the visitor with two buttons (**On my way** / **Not home**). Show it.
4. **Greeting** appears on the OLED: `New visitor / Hello!` (would be `Welcome, [Name]` if recognition were on).
5. **Reply** — either:
   - **Tap** a button (On my way / Not home), **or**
   - **Reply** to the photo message and **type** a custom message (e.g. "Be there in 5").
6. **OLED shows the reply** (word-wrapped) for ~11 s — the visitor sees the homeowner's answer at the door.
7. **Dashboard updates live** (WebSocket, no refresh):
   - New row in **Visit History** with the photo + timestamp.
   - **Response** column fills in with the reply.
8. **Show the other panels:**
   - **Statistics** — visits per day, hourly heatmap.
   - **Diagnostics** — device **Online**, uptime, WiFi signal, live system log.

> Tip: do one **full** run first (press → reply → dashboard), then a second quick press to show it's repeatable and live.

---

## C. Recovery steps (if something fails)

**1. ESP32 won't upload / `Connecting......` fails**
→ Click Upload, and the moment it shows `Connecting...`, **hold the BOOT button** until `Writing...` appears. Close the Serial Monitor first (it locks the COM port).

**2. `Camera probe failed` / no photo**
→ Power off, **reseat the camera ribbon** (flip the black latch, push the flex cable fully in, close it), and make sure you're on the **wall charger** — the camera browns out on weak USB.

**3. Railway cold start (first press is slow / times out)**
→ Hit the dashboard URL once a minute before the demo to wake the service. If a press times out, just press again — it'll be warm.

**4. OLED blank or garbled**
→ Check VCC/GND power and SDA→IO14 / SCL→IO15. If the image is **shifted/garbled**, the driver is wrong — swap the `SSD1306`/`SH1106` constructor line (we use SH1106). Tune `OLED_X` if text is off-center.

**5. Doorbell gets 401 / visits stop after backend auth goes live**
→ The `X-Device-Key` value in `secrets.h` doesn't match Railway `DEVICE_SECRET`. Set them equal, re-flash, redeploy. (Flash firmware + deploy backend *together*.)

**6. Button doesn't trigger**
→ Confirm the button bridges **IO13** and **GND** (diagonal legs). Watch Serial for `[BUTTON] Doorbell pressed!`. If it fires nonstop, the legs are shorted — move one.

**7. WiFi won't connect**
→ OLED stays on `Starting...`; Serial shows dots. Check SSID/password in `secrets.h` and move closer to the router. The CS-CAM is 2.4 GHz only.

---

## D. One-line summary for the panel

> *"Visitor presses the bell → the ESP32-CAM photographs them and uploads to our
> FastAPI backend → the homeowner gets a Telegram alert and replies → the reply
> shows on the door's OLED and the whole visit appears live on the dashboard."*
