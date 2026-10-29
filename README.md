# Citroen_Radio

Car radio for a classic 90s Citroën, built on an **ESP32 WROOM-32** (ESP32-D0WD-V3, USB chip CP2102 on COM9).

Signal chain: phone → **Bluetooth (A2DP)** → ESP32 → I2S → **MCU-PCM5102** DAC → 2 × **TPA3116D2** amps.
A 2.42" SSD1309 OLED emulates a 90s dot-matrix display, with French text.

Planned for later:
- UART link to a second ESP

## Current features

- Welcome sequence: `^ CITROËN` appears in the center and dissolves, then `BIENVENUE À BORD` scrolls across the center
- Bluetooth audio receiver named **`CITROEN RADIO`**, with output to the PCM5102
- Song info from the phone: title, artist, album, duration, position, play/pause state and volume
- Main screen with an inverted status bar (Bluetooth, phone connected/✕, volume), title, artist, play/pause icon, progress bar and times

## Wiring

### Display (SSD1309, 7-pin SPI)

| OLED pin | ESP32 GPIO |
| --- | --- |
| GND | GND |
| VCC | 3V3 |
| SCK (D0) | 18 |
| SDA (D1) | 23 |
| RES | 19 |
| DC | 21 |
| CS | 5 |

### DAC (MCU-PCM5102 / GY-PCM5102)

| PCM5102 pin | ESP32 |
| --- | --- |
| VIN | 5V (VIN pin of the ESP32 board) |
| GND | GND |
| BCK | GPIO 26 |
| LCK (LRCK) | GPIO 25 |
| DIN | GPIO 22 |
| SCK | GND (the DAC makes its own clock) |

The back of the module has 4 solder jumpers. They should be set to **H1L = L, H2L = L, H3L = H, H4L = L**.
H3L is XSMT (soft mute). If it is low, the DAC stays silent.
Most modules come pre-set this way. If you hear nothing, check H3L first.

The 3.5 mm jack outputs line level. For a first test you can plug in headphones at low phone volume, or connect it to the TPA3116D2 inputs.

### Steering-wheel buttons

The original Citroën button PCB has 8 buttons, 5 pins and no labels. It was mapped with the `../Steering_Probe` tool and a multimeter. It has **two resistor ladders that share P1**:

```
P4 ──333Ω── P3 ──333Ω── P5          P1 ──45Ω (backlight bulb)── P2

P4 ladder (button connects P1 to P4)    P5 ladder (button connects P1 to P5)
  6 Vol−          0 Ω                     7 Play/Pause     0 Ω
  5 Vol+        185 Ω                     2 Prev         182 Ω
  3 Select      653 Ω                     1 Next         666 Ω
  4 Menu        653 Ω + 1.5 kΩ mod        8 Mute           0 Ω + 1.5 kΩ mod
```

From the factory, buttons 3/4 ("next"/"MEMO") and 7/8 (both Mute) were electrically identical duplicates.
To give every button its own function, a **1.5 kΩ resistor is added in series with buttons 4 and 8**: cut the trace into the button's ladder-side pad and bridge the cut with the resistor.
After the mod, these should read:
- Menu: P1–P4 ≈ 2150 Ω
- Mute: P1–P5 ≈ 1500 Ω

| PCB pin | Connects to |
| --- | --- |
| P1 | GND (common for buttons and backlight) |
| P2 | Not to the ESP32. In the car it goes to the illumination +12 V (backlight bulb). |
| P3 | Not connected |
| P4 | GPIO 34 + **1 kΩ** pull-up to 3V3 + 100 nF to GND |
| P5 | GPIO 35 + **1 kΩ** pull-up to 3V3 + 100 nF to GND |

GPIO 34/35 are ADC1, because ADC2 can't be used while Bluetooth is on.

A press pulls its own line low. It also pulls the other line part of the way down, because the lines are linked by 666 Ω on the PCB. That makes every button a unique (A, B) pair. Expected voltages:

| Button | Line A (P4) | Line B (P5) |
| --- | --- | --- |
| none | 3.3 V | 3.3 V |
| Vol− | 0 V | 1.3 V |
| Vol+ | 0.75 V | 1.8 V |
| Select | 1.7 V | 2.3 V |
| Menu (mod) | 2.6 V | 2.85 V |
| Play/Pause | 1.3 V | 0 V |
| Prev | 1.8 V | 0.75 V |
| Next | 2.3 V | 1.7 V |
| Mute (mod) | 2.7 V | 2.3 V |

### Reserved pins

- GPIO 16 / 17: UART link to the second ESP

## Screen layout

The welcome sequence uses a dot-matrix font: every dot is 1 pixel with a 1-pixel gap. Unlit dots are not drawn, because the SSD1309 has no per-pixel brightness.
The main screen uses solid LCD-style pixel fonts, so it fits more text and shows every accent.

```
███████████████████████████████
█ ᛒ ▯ FM LOUD          🔈 64 █   <- inverted status bar
███████████████████████████████
    Rain On The Windshield        <- title (6x12 font)
         Static Bloom             <- artist (5x7 font)
 ▶ 1:23 █████████·········· 3:21  <- play/pause, elapsed, progress, total
```

| Area | Y (px) | Content |
| --- | --- | --- |
| Status bar (inverted) | 0–10 | Bluetooth icon. Then a phone icon when connected, or an ✕ when not. After that, the source indicator when it isn't Bluetooth (`FM`, `AM` or a cassette icon for tape) and `LOUD` when loudness is on. Speaker icon + phone volume (0–100) on the right. No clock, because the car has its own. |
| Title | baseline 22 (capitals 13–21) | `u8g2_font_6x12_tf`, up to ~21 characters |
| Artist | baseline 32 (capitals 26–31) | `u8g2_font_5x7_tf` (classic 5x7 LCD font), up to ~25 characters. Sits 4 px below the title. |
| Playback strip | bar rows 36–38 | 4 px below the artist. Progress bar 3 px tall: the played part is solid, and the rest is a dotted line through the bar's center. The play ▶ / pause ❚❚ icon, elapsed time and total time (`u8g2_font_4x6_tf`) are centered on the bar's middle line. |

Rows 40–63 are free for future content.

**Sources:** only Bluetooth works for now. FM, AM and cassette show their name with `NON DISPONIBLE` so the status bar can be tested.
Until the steering-wheel buttons are connected, use the serial monitor: `s` cycles the source (Bluetooth → FM → AM → cassette) and `l` toggles `LOUD`. Loudness is only an indicator so far and doesn't change the sound yet.

- **Scrolling:** text that fits is centered and never scrolls. Longer text waits 2.5 s, then glides left one pixel every 45 ms (about 22 px/s), wraps around with a 32 px gap, and pauses again. To change the speed, edit `SCROLL_PAUSE_MS`, `SCROLL_STEP_MS` and `SCROLL_GAP`.
- With no phone connected, the title blinks `EN ATTENTE` and the artist line shows the Bluetooth name. Right after connecting, before any song info arrives, it shows `CONNECTÉ`.
- The position comes from the phone every second and is interpolated between updates, so the bar moves smoothly. When the song changes, it restarts from 0.
- The screen redraws at 40 FPS so scrolling is smooth.

### Fonts and text

- **Welcome (dot-matrix)**: classic 5x7 with uppercase, digits and punctuation, plus the glyphs À É È Ë Ç and `^` for the Citroën double chevron.
- **Main screen**: U8g2 `_tf` fonts drawn with `drawUTF8`. All Latin-1 accents work (é ñ ü ç…). Characters outside Latin-1, such as emoji, are not shown.

## Code overview (`src/main.cpp`)

| Part | What it does |
| --- | --- |
| Dot-matrix font | `FONT5X7`, `CUSTOM` (accents + chevron), `toCodes()` (UTF-8 → glyph codes), `drawBig`, `drawBigCodes`, `dot` |
| Bluetooth | `startBluetooth()` sets up I2S + A2DP. Callbacks `onConnection`, `onMetadata`, `onPlayStatus`, `onPlayPos` and `onVolume` fill `np`, protected by a spinlock because they run in the Bluetooth task. |
| Welcome | `welcome()` and `dissolve()` |
| Main screen | `render()` at 40 FPS: `drawStatusBar`, `drawScrolling` (centered or smooth scroll), `drawPlaybackStrip`, `drawProgress`, icons (`iconBluetooth`, `iconPhone`, `iconCross`, `iconSpeaker`, `iconCassette`, `iconPlay`, `iconPause`) |
| Sources | `Source` enum, `source`, `loud`; `handleSerial()` changes them with the `s` / `l` keys |
| Steering wheel | `readSwc`, `matchSwc`, `pollSteering` (debounce + volume repeat), `doAction`, learn mode (`learnSteering`, `waitSteadyPress`, `waitRelease`), `loadSteering` / `saveSteering` |
| Feedback | `showToast()`: message under the progress bar for 1.5 s |
| Debug | `logChanges()` prints connection, play state and song changes to the serial monitor |

Libraries (downloaded by PlatformIO):
- U8g2 2.36
- [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) 1.8.11
- [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) 1.2.6

The app uses the `huge_app.csv` partition because the Bluetooth stack does not fit in the default one.

## Uploading

```
pio run -t upload
pio device monitor
```

This board does not enter upload mode automatically. **Hold the BOOT button** while the upload connects, and release it when writing starts.
If the screen stays blank after uploading, press **EN** to restart the board.

The COM9 port needs the Silicon Labs CP210x driver.

## Testing Bluetooth

1. Power the board and wait for the welcome sequence.
2. On the phone, open Bluetooth settings and pair with **CITROEN RADIO**.
3. Play music. Row 1 shows the title, row 2 the artist, and the bar and time move.
4. Pause on the phone. The icon changes to ❚❚ and the time stops.
5. The serial monitor (`pio device monitor`) logs each song, the play state and connection changes.

## Steering wheel: learn mode and actions

The buttons are **off until learned**. GPIO 34/35 float when nothing is connected and would cause random presses.

1. Wire the PCB as above, then open `pio device monitor`.
2. Send **`L`**. The screen shows `APPRENTISSAGE n/8`, the button to press and the live A/B readings.
3. Press and hold each button when asked, in this order: Suivant, Précédent, Sélection, Menu, Volume +, Volume −, Lecture/Pause, Muet. Each one is saved after 400 ms steady. Release before the next one.
4. The readings are saved to flash (Preferences namespace `swc`) and survive power-off. The log warns if two buttons are too close, which means the 1.5 kΩ mod is missing or wrong.

Other serial keys:
- `w`: stream raw A/B readings every 100 ms
- `c`: clear the learned buttons
- `s` / `l`: source / LOUD test keys

| Button | Action |
| --- | --- |
| Next / Prev | Next / previous track on the phone |
| Play/Pause | Play or pause on the phone |
| Vol+ / Vol− | Bluetooth volume ±8 (of 127). Repeats while held after 0.5 s. |
| Mute | Volume to 0 (speaker ✕ in the status bar), press again to restore |
| Select / Menu | Shown on screen only. Functions still to be decided. |

Every press shows a short label under the progress bar for 1.5 s, for example `VOLUME 64`, `SUIVANT` or `MUET`.

Matching: the line readings are sampled every 10 ms (4 samples averaged). A press counts when the nearest learned point is within 400 (|ΔA| + |ΔB|) for 4 samples in a row. "No button" means both lines are above 3950.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| Phone can't find `CITROEN RADIO` | Wait until the welcome sequence ends. Forget old pairings and search again. |
| Connected but no sound | Check BCK/LCK/DIN, that DAC SCK is on GND, and the H3L (XSMT) jumper. Turn up the phone volume. |
| Distorted or crackling sound | Use short wires for BCK/LCK/DIN and a common ground. Power the DAC from 5V. |
| Title and artist stay empty | Some apps don't send song info. Try Spotify, YouTube Music or the phone's own player. |
| Time doesn't move | The phone doesn't send position updates. The time still counts from the start of the song. |
