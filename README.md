# O-LED // When Your OLED Thinks It's a WLED Strip

> *"What if WLED could 'WLED'... on a tiny 128x64 1-bit OLED?"*

**O-LED** (`make_me_dance_wled-sr_companion`) turns an **ESP32** + **128x64 SSD1306 I2C OLED** into a standalone, sound-reactive **WLED-SR** companion display.

It listens to live **WLED Audio Sync v2 (`00002`)** packets over UDP port `11988` (unicast + `239.0.0.1` multicast), runs 5 demoscene/WLED visualizers with **17-shade 4x4 Ordered Bayer dithering**, hosts its own dark-mode Web UI, and pairs over Wi-Fi with a real ESP8266 WLED-SR light strip (**"Tuub layt"**) so you can watch and control your room lights right from a postage-stamp screen.

```text
   +------------------------------------------+
   | TUUB LAYT ONLINE                         |
   | FX:207 SX:  5 IX:126                     |
   | +--------------------------------------+ |
   | |  ::::####████████████####::::..      | |  <-- 61-LED Bayer-Dithered Twin Slider
   | +--------------------------------------+ |
   | BRI [██████████░░░░]   ..:||█||:..:||:.  |
   +------------------------------------------+
```

---

## Features & Visualizers

No fake fallback BPM generators here — when your audio is silent, the analyzer sits dead flat at `0`. Drop a track and the screen wakes up:

| # | Name | What It Does |
| :-: | :--- | :--- |
| **4** *(Default)* | **`TUUBTWN`** | **Live WLED-SR Twin**: Polls your ESP8266 WLED-SR strip (`/win`), displays its live `FX`, `SX`, `IX`, and `BRI`, and renders a 61-LED Bayer-dithered virtual light strip with long, liquid sliding wave bars + 16-band mini EQ. |
| **2** | **`PLASMA`** | **Bayer Fire2012 & Audio Plasma**: Full-screen 17-gray-level Ordered Bayer-dithered plasma field (`sinf` + `cosf`) boosted by live sub-bass energy. |
| **0** | **`GEQ16`** | **16-Band WLED 2D Spectrum**: Segmented LED equalizer columns with floating gravity peak-hold caps, stereo bass VU meter, and live dominant `Hz` readout. |
| **1** | **`WATER3D`** | **3D Isometric Waterfall**: Scrolling 14-row spectrogram ridge-plot with front-to-back horizon occlusion (Joy Division *Unknown Pleasures* vibes). |
| **3** | **`OUTRUN`** | **Synthwave Audio Horizon**: Scanline sunset + scrolling perspective grid + audio-reactive 3D wireframe octahedron that squashes on kicks and spins on highs. |

---

## One Button to Rule Them All (`GPIO 0` / `BOOT`)

Who needs a rotary encoder when you have one clicky `BOOT` button on the ESP32?

- **Outside Menu (Visualizer Mode)**:
  - **1x Short Press**: Switch to the next effect (`GEQ16` -> `WATER3D` -> `PLASMA` -> `OUTRUN` -> `TUUBTWN`).
  - **Long Press (`>= 520 ms`)**: Open the on-screen **O-LED Menu UI**.
- **Inside Menu (`O-LED MENU`)**:
  - **1x Short Press**: Move the inverted highlight bar to the next option.
  - **Long Press (`>= 520 ms`)**: Select / toggle / step the highlighted option.
  - *Auto-closes after 15 seconds if you wander off.*

### On-Screen Menu Tree
- **`EFFECTS`**: Pick `PLASMA`, `GEQ16 SPEC`, `3D WATERFALL`, `OUTRUN 3D`, or `TUUB TWIN`.
- **`FX OPTIONS`**: Step `SPEED SX`, `INTEN IX`, `SMOOTH` (Attack/Release EMA filter `0..255`), `FFT GAIN %`, or toggle `AUTO CYCLE`.
- **`WLED TUUB CTRL`**: Remote-control your physical ESP8266 WLED strip (`TUUB PWR`, `TUUB BRI`, `TUUB SPD`, `TUUB INT`, `MIRROR TUUB`, `OPEN TWIN`).
- **`SETTINGS UI`**: Toggle `HIDE UI` (100% full-screen visuals), `INVERT OLED`, step `CONTRAST`, or flash your live `IP` banner.

> **Smart Auto-Hiding Top Bar**: Even when `HIDE UI` is off, the top HUD bar (`y = 0..13`) only pops up for **3.5 seconds** when something meaningful changes (Effect, `SR` audio status, `Auto-Cycle`, or `IP`) — completely ignoring FPS fluctuations — and then gets out of the way so your visuals own all `128x64` pixels.
>
> **NVS Flash Memory**: Every setting you tweak (from the OLED menu, button, or Web UI) automatically saves to ESP32 NVS flash 1.5 seconds later and survives power cycles.

---

## Built-In Dark-Mode Web UI (`http://<esp32-ip>/`)

Open the ESP32's IP address in any browser for a live dashboard:
- **Live 16-Band FFT Spectrum** + dominant `Hz` readout & `LIVE UDP 11988` badge.
- **Sliders**: `Effect Speed (SX)`, `Effect Intensity (IX)`, `Audio FFT Gain (%)`, `Audio Smoothness (0..255)`, and `OLED Contrast (BRI)`.
- **Toggles**: `OLED Power`, `Auto-Cycle`, `Hide UI`, `Invert`, and `Mirror Tuub SX/IX`.
- **ESP8266 Twin Remote Card**: Adjust your physical WLED strip's Brightness, Speed, Intensity, and Power right from the same page.

---

## Hardware Wiring

| SSD1306 128x64 OLED | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **VCC** | `3V3` | |
| **GND** | `GND` | |
| **SDA** | **`GPIO 22`** | Auto-scanner also probes common pairs if you wire it backwards |
| **SCL** | **`GPIO 23`** | 400 kHz I2C at address `0x3C` |
| **Button** | **`GPIO 0`** | Built-in `BOOT` button on the ESP32 dev board |

---

## How to Compile Your Own `.bin` (2 Easy Ways)

Because Wi-Fi credentials and your companion WLED-SR IP are compiled directly into the firmware, you can build your personalized `.bin` in one command locally **or** in the cloud using GitHub Actions without installing anything on your PC.

### Option A: One-Command Local Build (`./build.sh`)
You only need Docker (or a local ESP-IDF `v4.4` install). Run:
```bash
# 1. Interactive setup + compile (prompts for Wi-Fi SSID, Password & WLED-SR IP on first run):
./build.sh

# 2. Or compile AND flash directly to your ESP32 in one go:
./build.sh --flash /dev/ttyUSB0

# Need to change your Wi-Fi or WLED IP later?
./build.sh --reconfigure
```
This saves your settings to a gitignored `src/secrets.h` (or you can copy `src/secrets.h.example` -> `src/secrets.h` manually) and outputs:
- `build/oled_wled_factory_0x0.bin` — **All-in-one factory binary** (flash at `0x0` using any web flasher like [ESP Web Tools / esp.huhn.me](https://esp.huhn.me/) or `esptool.py write_flash 0x0 build/oled_wled_factory_0x0.bin`).
- `build/oled_wled.bin` — **App-only binary** (flash at `0x10000`).

### Option B: Zero-Install GitHub Actions Cloud Builder
Don't want to install Docker or ESP-IDF on your computer? Let GitHub's servers compile your `.bin` for free:
1. **Fork** this repository on GitHub.
2. Go to the **Actions** tab in your fork and select **"Build Custom Firmware (.bin)"**.
3. Click **"Run workflow"**, enter your **2.4GHz Wi-Fi SSID**, **Wi-Fi Password**, and **Companion WLED-SR IP** (or store them privately in your fork's *Settings -> Secrets and variables -> Actions* as `WIFI_SSID`, `WIFI_PASS`, and `TUUB_LAYT_IP`).
4. When the workflow finishes (~90 seconds), download the **`oled_wled_custom_firmware`** zip from the bottom of the workflow summary page and flash `oled_wled_factory_0x0.bin` at offset `0x0`!

> **Why does `flash.sh` flash in 128 KB slices?**
> Because life is too short to watch a sketchy micro-USB cable fail at 94% and restart from 0%. `flash.sh` writes `oled_wled.bin` in sector-aligned `128 KB` (`0x20000`) chunks at `115200` baud with automatic per-slice retry.

---

## Nerd Trivia: The 3-Minute `sinf()` Cliff

If you ever write a real-time plasma shader on an ESP32 and notice it runs at a buttery **34 FPS** for the first two minutes after boot and then mysteriously nose-dives to **5 FPS**, blame `newlib`'s `__ieee754_rem_pio2f`!

Once a `float` phase angle grows past `256 * (π / 4) ≈ 201.06` radians (~90 seconds of uptime), `sinf()` and `cosf()` abandon their fast inline `float` range reduction and fall back to **Payne-Hanek multi-precision range reduction** (`__kernel_rem_pio2f`), which emulates 64-bit `double` math in software on the ESP32's single-precision FPU — 4,800 times per frame. Wrapping phase angles modulo $2\pi$ (`fmodf(theta, TWO_PI)`) before the pixel loop keeps `sinf()` on the fast path forever without sacrificing live per-pixel math.
