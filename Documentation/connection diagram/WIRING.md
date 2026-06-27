# Smart Doorbell — Wiring Reference (V1 + OLED)

Final wiring for the working prototype: **See-Sys CS-CAM** carrier board
(AI-Thinker ESP32-CAM) with a push button and a 128×64 I²C OLED.

Matches firmware: `ESP32/doorbell_step5_btn_io14/doorbell_step5_btn_io14.ino`

---

## Bill of materials

| Qty | Part | Notes |
|-----|------|-------|
| 1 | See-Sys **CS-CAM** carrier + AI-Thinker ESP32-CAM | Camera (OV2640) + WiFi; programmed/powered over **USB-B** |
| 1 | Tactile push button | The doorbell trigger |
| 1 | 128×64 I²C OLED (**SH1106**, 4-pin: VCC/GND/SCK/SDA) | On-device status + homeowner reply |
| 2 | ~1 kΩ resistor | Series protection on the OLED's SDA/SCL (see ⚠️ below) |
| — | Breadboard + jumper wires | |
| 1 | 5 V USB wall charger | Recommended supply (camera + OLED brown out on weak USB) |

---

## Pin map (ESP32-CAM GPIO → use)

| GPIO | Used for | Notes |
|------|----------|-------|
| **IO13** | Button | `INPUT_PULLUP`; other leg → GND |
| **IO14** | OLED **SDA** | software I²C |
| **IO15** | OLED **SCK/SCL** | software I²C |
| IO4 | Built-in flash LED | onboard (capture indicator); not on a header |
| IO16 | **PSRAM** | reserved — camera photo buffer, do **not** use |
| IO2, IO12 | *free* | only spare pins (both are boot-strapping pins) |
| 0,5,18,19,21,22,23,25,26,27,32,34–39 | Camera bus | do not touch |

---

## Connections

### Power
```
USB-B (wall charger 5V) ──► CS-CAM   (POWER switch ON)
CS-CAM  VCC tap ──► breadboard (+) rail   (5 V)
CS-CAM  GND tap ──► breadboard (−) rail
```

### Button  (yellow front header + GND rail)
```
button leg A ──────────────► IO13   (yellow header)
button leg B ──────────────► (−) GND rail
```
No resistor needed — IO13 uses the internal pull-up; pressing pulls it to GND.

### OLED 128×64 I²C
```
OLED VCC ─────────────────► (+) 5 V rail
OLED GND ─────────────────► (−) GND rail
OLED SCK ──[ ~1 kΩ ]──────► IO15   (yellow header)
OLED SDA ──[ ~1 kΩ ]──────► IO14   (yellow header)
```

---

## ⚠️ Important notes

1. **OLED runs on 5 V → protect the I²C lines.** The panel's pull-ups idle at
   5 V, but the ESP32's pins are **3.3 V max**. Put **~1 kΩ in series** on SCK
   and SDA. (A measured SDA reading of ~4.5 V with VCC = 5 V confirmed the
   pull-ups are on the 5 V rail.) Ideal fix would be powering the OLED from
   3.3 V, but only 5 V is exposed on this board.

2. **Why the button uses IO14's old spot for the OLED.** The CS-CAM yellow
   header has no GND pin, so early on IO14 was driven LOW as a fake ground.
   Once a real GND (the VCC/GND power tap) was used for the button, **IO14
   freed up for the OLED's SDA**.

3. **IO16 is off-limits** — it's wired to the PSRAM the camera needs to buffer
   a photo. Using it breaks the camera.

4. **Power matters.** The camera's start-up current surge + the OLED browns out
   a weak USB port (symptom: `Camera probe failed`). Use a **5 V wall charger**.
   Note: a plain charger has no data line — use the laptop USB (or a powered
   hub) when you need to upload / see Serial Monitor.

5. **Upload tip.** If the upload shows `Connecting....` and fails, hold the
   **BOOT** button until writing starts.

---

## Status: pins in use

```
 IO13  ● button
 IO14  ● OLED SDA
 IO15  ● OLED SCK
 IO16  ◌ PSRAM (reserved)
 IO2   ○ free (strapping)
 IO12  ○ free (strapping)
```
Only **IO2** and **IO12** remain free — which is why an I²S speaker
(needs 3 pins) cannot be added alongside the OLED on this board.
