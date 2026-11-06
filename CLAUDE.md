# Project context for Claude Code

Car radio for a classic early-90s Citroën. Claude Code reads this file automatically, so the context carries over to any laptop that clones this repo.
- **Full docs:** [README.md](README.md) (wiring, screens, menu, controls, serial keys)
- **Planned features:** [FUTURE.md](FUTURE.md)

## Hardware
- **Board:** ESP32 WROOM-32 (ESP32-D0WD-V3), CP2102 USB chip, PlatformIO `esp32dev`. Develop **only** for this board; the ESP32-S3 is not used.
- **Audio chain:** phone → Bluetooth A2DP → ESP32 → I2S → MCU-PCM5102 DAC → 2 × TPA3116D2 amps.
- **Display:** 2.42" SSD1309 128x64 OLED (SPI, U8g2).
  - **Only rows 0–39 are visible**, because the radio case covers the rest. This is `VIEW_H` in `src/radio.h`, and every screen must fit in it.
- **Steering wheel:** the original Citroën 8-button resistor-ladder PCB, read on GPIO 34/35 (ADC1, because ADC2 doesn't work while Bluetooth is on).
  - It needs a mod: 1.5 kΩ in series with buttons 4 and 8. Details are in the README.
- **Setup button:** GPIO 32 to GND. It's the only way to open the settings menu; the wheel's Menu key must **not** open it.
- **Reserved:** GPIO 16/17 for a future UART link to a second ESP.

## Working rules (from the user)
- **Docs:** only in `README.md` / `FUTURE.md`, never in separate docs or artifacts. Keep them updated with every change.
- **UI style:** emulates a 90s dot-matrix display, with all text in French.
  - No clock, because the car has its own.
  - No on-screen toasts or press labels; feedback goes in the status bar and the serial log.
- **Music first:** don't risk Bluetooth music stability for new features.
- **Upload:** the board doesn't enter upload mode by itself.
  - Build first with `pio run`.
  - Then ask the user to **hold BOOT** and say "go", then run `pio run -t upload`.
  - The COM port may differ on another laptop; this laptop used COM9, set in `platformio.ini`. The port needs the Silicon Labs CP210x driver.
- **Git:** the repo is `github.com/Seltsames/radio-oled-screen`, branch `main`.
  - Commits use the identity `Seltsames <72458874+Seltsames@users.noreply.github.com>`, set in the repo's local git config (not global).
  - Commit or push only when the user asks.

## Code layout
| File | Contents |
| --- | --- |
| `src/radio.h` | Shared declarations |
| `src/main.cpp` | Dot-matrix font, Bluetooth (A2DP/AVRCP), welcome sequence, main screen, serial keys |
| `src/steering.cpp` | Wheel reading, button → action map, learn mode |
| `src/menu.cpp` | Settings (Preferences `radio`), menu |
| `tools/serial_remote.py` | Keyboard remote over serial |

## Status (2026-11-06)
- **Done and tested on the board:**
  - Bluetooth music with metadata
  - Main screen
  - Settings menu, tested over serial
  - Every screen fits rows 0–39
- **Pending on the user's side:**
  - The 1.5 kΩ PCB mod and the pull-up wiring for the steering wheel
  - Wiring the setup button
  - Then learn the wheel buttons and test with the real wheel
- **Not tested yet:** audio through the PCM5102 and the amps.
- **Next feature:** Phase B in FUTURE.md (phone calls: caller ID, answer/reject from the wheel, voice stays on the phone). Then Google Maps navigation through the Chronos app (Android).
- **Undecided:** the Select and Menu wheel functions outside the menu. The user will say.
