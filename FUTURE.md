# Citroen_Radio — future features

Ideas planned for later, with what was checked about each one. Nothing here is implemented yet.
The current features are documented in [README.md](README.md).

Planned order:
1. Phone calls (control only)
2. Google Maps navigation
3. Hands-free voice (optional)

Music quality and stability always come first. Every step below must keep Bluetooth music working as it does now.

## What was checked

| Topic | Finding |
| --- | --- |
| Call control (HFP) | The stock Arduino core used by PlatformIO (`framework-arduinoespressif32` 3.20017) is built with `CONFIG_BT_HFP_CLIENT_ENABLE=y`. The `esp_hf_client_*` API (connect, answer, reject, disconnect audio) is available without a custom build. |
| Effect on music | HFP control is only short commands, so music quality doesn't change. The phone pauses music during a call anyway. |
| Call voice | The core sends call audio (SCO) to the ESP32's hardware PCM pins (`SCO_DATA_PATH_PCM`), not to the software. Hearing the caller in the car needs a custom Bluetooth build. |
| Phone mic + car speakers | Not possible over Bluetooth. The hands-free profile always moves both directions of a call together, either to the phone or to the car. |
| Navigation on Android | The free **Chronos** app forwards Google Maps turn-by-turn and notifications over BLE. The `ChronosESP32` library (v1.9.1) decodes it: direction text, distance, duration, ETA, street, and a 48x48 1-bit arrow icon. It also gives caller names (`setRingerCallback`) and notifications (`setNotificationCallback`). |
| BLE next to music | `ChronosESP32` needs the **NimBLE** stack. The music uses the **Bluedroid** stack, and the two can't run on the same chip. Bluedroid itself supports BLE (`CONFIG_BT_BLE_ENABLED=y`), so the Chronos protocol could be rewritten on Bluedroid, if RAM allows. |

## Phase B — phone calls (control only)

Goal: see who is calling, and answer, reject or hang up from the steering wheel. The conversation itself stays on the phone.

- After `a2dp.start()`: `esp_hf_client_register_callback(onHfp)` and `esp_hf_client_init()`.
- When the phone connects over A2DP: `esp_hf_client_connect(*a2dp.get_current_peer_address())`. The phone then lists "Phone calls" for CITROEN RADIO.
- Call state in `np` (protected by the same spinlock):
  - `callState`: none, ringing, outgoing or active
  - `callerNumber`, `callerName`
- These come from:
  - `ESP_HF_CLIENT_CIND_CALL_SETUP_EVT` and `ESP_HF_CLIENT_CIND_CALL_EVT`
  - `ESP_HF_CLIENT_CLIP_EVT`: the number, plus the contact name if the phone sends it
  - `ESP_HF_CLIENT_RING_IND_EVT`
- When the call audio link opens (`ESP_HF_CLIENT_AUDIO_STATE_EVT`), call `esp_hf_client_disconnect_audio(peer)`. The phone then keeps the voice on its own speaker and mic.
  - Menu setting `AUDIO APPEL: TÉLÉPHONE`, ready for hands-free later.
- **Call screen:**
  - Bar: phone icon + `APPEL ENTRANT` or `EN COMMUNICATION`.
  - Caller name in 6x12 (scrolls if long), number in 5x7, call timer.
  - Hint line: `▶ DÉCROCHER   ✕ REFUSER`.
- **Steering wheel during a call:**
  - Ringing: Play/Pause or Select answers (`esp_hf_client_answer_call()`), Mute rejects (`esp_hf_client_reject_call()`).
  - Active: Play/Pause, Select or Mute hangs up.
  - Vol± still change the music volume.
- Serial test keys: `a` answer, `r` reject / hang up.
- **Test:**
  - Call the phone from another phone. Check the caller shows, answer and reject work, and the voice stays on the phone.
  - Music must resume after the call, with no glitches during normal playback.

## Phase C — Google Maps navigation (Android + Chronos)

1. **RAM test first.** With music playing, print the free heap (it's shown in `À PROPOS`). Then add a minimal Bluedroid BLE GATT server (`BLEDevice.h`, the same stack as the music) and measure again.
2. **If at least ~40 KB stays free:** port the Chronos BLE protocol from `ChronosESP32` to Bluedroid on this ESP. One ESP stays connected to the phone, for both music and navigation.
3. **If RAM is too tight:** run `ChronosESP32` (NimBLE) on the **second ESP** that will be linked over UART (GPIO 16/17). It sends the direction, distance, street and arrow icon to this one.

**Navigation screen:**
- Must fit the visible window (rows 0–39, the rest is behind the radio case). It could replace the title/artist rows while a turn is close.
- Shows the 48x48 arrow redrawn in the dot-matrix style, the distance in the big dot font, and the street name scrolling.

**Phone setup:**
- Install Chronos, then go to **Watches → Pair New Devices** and pick the radio.
- Allow notification access.
- Navigation needs Chronos v3.7.5 or later.

## Phase D — hands-free voice (optional)

Hearing the caller in the car and talking through a mic:
- **Custom Bluetooth build** with `CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y`, so call audio reaches the software. Either PlatformIO `framework = arduino, espidf`, or the Arduino lib-builder.
- **I2S microphone** such as an INMP441.
- Call audio at 8/16 kHz (CVSD / mSBC). The I2S output switches between music and call audio for the PCM5102.
- Echo is likely with car speakers, so it would need echo cancellation or at least half-duplex.

Only start this once music has proven stable in the car.

## Other ideas
- UART link to the second ESP (GPIO 16/17), with simple `TAG:value` text lines such as `TI:song title` or `CMD:next`
- Real loudness (bass/treble boost in software before the DAC)
- FM/AM tuner and cassette sources
- Functions for the Select and Menu steering-wheel buttons outside the menu (to be decided)
