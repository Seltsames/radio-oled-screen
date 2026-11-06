// ---------- Steering wheel buttons ----------
// Two resistor ladders share P1 (ground). Each press pulls one line down to its own voltage
// and the other line part of the way (they are linked by 666 Ohm on the PCB), so every button
// has a unique (A, B) reading. Readings are learned once and stored in flash.
// Each physical button runs an action from buttonAction[], which the settings menu can change.

#include "radio.h"
#include <Preferences.h>

#define PIN_SWC_A 34  // PCB P4: Vol-, Vol+, Select, Menu
#define PIN_SWC_B 35  // PCB P5: Play/Pause, Prev, Next, Mute

const char *ACTION_NAMES[] = {"SUIVANT", "PRÉCÉDENT", "SÉLECTION", "MENU", "VOLUME +", "VOLUME -", "LECTURE/PAUSE", "MUET"};

// The factory layout: physical button n does action n
const uint8_t DEFAULT_MAP[NUM_BUTTONS] = {ACT_NEXT, ACT_PREV, ACT_SELECT, ACT_MENU,
                                          ACT_VOL_UP, ACT_VOL_DOWN, ACT_PLAY_PAUSE, ACT_MUTE};
uint8_t buttonAction[NUM_BUTTONS];

SwcPoint swcTable[NUM_BUTTONS];
bool swcReady = false;  // the buttons stay off until learned (unconnected inputs float)
bool swcRawLog = false;

const int SWC_IDLE = 3950;          // both lines above this = no button pressed
const int SWC_MATCH = 400;          // max |dA| + |dB| from a learned point
const int SWC_TOO_CLOSE = 300;      // learned points closer than this are reported
const int SWC_STABLE = 4;           // samples (10 ms apart) before a press counts
const int SWC_REPEAT_DELAY = 500;   // volume repeats while held
const int SWC_REPEAT_MS = 150;
const int LEARN_TIMEOUT_MS = 30000; // learn mode gives up if a button isn't pressed in time
const int VOL_STEP = 8;             // of 127

Preferences swcPrefs;

void loadSteering() {
  swcPrefs.begin("swc", true);
  swcReady = swcPrefs.getBytesLength("table") == sizeof(swcTable) &&
             swcPrefs.getBytes("table", swcTable, sizeof(swcTable)) == sizeof(swcTable);
  bool mapped = swcPrefs.getBytesLength("map") == sizeof(buttonAction) &&
                swcPrefs.getBytes("map", buttonAction, sizeof(buttonAction)) == sizeof(buttonAction);
  swcPrefs.end();
  if (!mapped) memcpy(buttonAction, DEFAULT_MAP, sizeof(buttonAction));
  for (int i = 0; i < NUM_BUTTONS; i++)
    if (buttonAction[i] >= NUM_ACTIONS) buttonAction[i] = DEFAULT_MAP[i];
  Serial.println(swcReady ? "Steering wheel: learned buttons loaded" : "Steering wheel: not learned yet (send 'L')");
}

void saveSteering() {
  swcPrefs.begin("swc", false);
  swcPrefs.putBytes("table", swcTable, sizeof(swcTable));
  swcPrefs.end();
}

void saveButtonMap() {
  swcPrefs.begin("swc", false);
  swcPrefs.putBytes("map", buttonAction, sizeof(buttonAction));
  swcPrefs.end();
}

void resetButtonMap() {
  memcpy(buttonAction, DEFAULT_MAP, sizeof(buttonAction));
}

// Forgets the learned readings (the button map is kept)
void clearSteering() {
  swcPrefs.begin("swc", false);
  swcPrefs.remove("table");
  swcPrefs.end();
  swcReady = false;
}

SwcPoint readSwc() {
  uint32_t a = 0, b = 0;
  for (int i = 0; i < 4; i++) {
    a += analogRead(PIN_SWC_A);
    b += analogRead(PIN_SWC_B);
  }
  return {(uint16_t)(a / 4), (uint16_t)(b / 4)};
}

bool swcIdle(SwcPoint p) { return p.a > SWC_IDLE && p.b > SWC_IDLE; }
int swcDistance(SwcPoint p, SwcPoint q) { return abs((int)p.a - q.a) + abs((int)p.b - q.b); }

// Returns the physical button (0..7) closest to the reading, or -1
int matchSwc(SwcPoint p) {
  int best = -1, bestDist = SWC_MATCH;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int d = swcDistance(p, swcTable[i]);
    if (d < bestDist) { bestDist = d; best = i; }
  }
  return best;
}

// True when both lines sit steadily at idle for 1 s, which means the pull-ups are wired
// (unconnected inputs float and wander)
bool steeringWired() {
  for (int i = 0; i < 100; i++) {
    if (!swcIdle(readSwc())) return false;
    delay(10);
  }
  return true;
}

// Feedback is on the status bar (volume, muted speaker) and in the serial log only
void doAction(Action a) {
  Serial.printf("Steering: %s\n", ACTION_NAMES[a]);
  switch (a) {
    case ACT_NEXT: a2dp.next(); break;
    case ACT_PREV: a2dp.previous(); break;
    case ACT_PLAY_PAUSE:
      if (np.playing) a2dp.pause();
      else a2dp.play();
      break;
    case ACT_VOL_UP:
    case ACT_VOL_DOWN:
      setVolume(np.volume + (a == ACT_VOL_UP ? VOL_STEP : -VOL_STEP));
      break;
    case ACT_MUTE:
      if (muted) {
        setVolume(volBeforeMute);
      } else {
        volBeforeMute = np.volume;
        a2dp.set_volume(0);
        muted = true;
      }
      break;
    case ACT_SELECT:
    case ACT_MENU:
    default:
      break;  // functions still to be decided
  }
}

// A physical button was pressed (from the wheel or the serial keys 1-8)
void pressButton(int button) {
  Action a = (Action)buttonAction[button];
  if (!menuKey(button, a)) doAction(a);
}

void pollSteering() {
  static unsigned long lastSample = 0, lastRaw = 0, pressedAt = 0, lastRepeat = 0;
  static int candidate = -1, count = 0, held = -1;
  if (millis() - lastSample < 10) return;
  lastSample = millis();

  SwcPoint p = readSwc();
  if (swcRawLog && millis() - lastRaw >= 100) {
    lastRaw = millis();
    Serial.printf("SWC A %4u  B %4u\n", p.a, p.b);
  }
  if (!swcReady) return;

  int m = swcIdle(p) ? -1 : matchSwc(p);
  if (m == candidate) count++;
  else { candidate = m; count = 1; }
  if (count < SWC_STABLE) return;

  if (candidate != held) {
    held = candidate;
    if (held >= 0) {
      pressButton(held);
      pressedAt = lastRepeat = millis();
    }
  } else if (held >= 0 && (buttonAction[held] == ACT_VOL_UP || buttonAction[held] == ACT_VOL_DOWN) &&
             millis() - pressedAt > SWC_REPEAT_DELAY && millis() - lastRepeat > SWC_REPEAT_MS) {
    pressButton(held);
    lastRepeat = millis();
  }
}

// ---------- Steering wheel learn mode ----------
// Buttons are learned in their physical order, named by their factory function.
// Bar with the step and hint, button name, live readings (rows 0..38)
void drawLearn(int index, const char *hint, SwcPoint p) {
  char buf[32];
  u8g2.clearBuffer();
  u8g2.drawBox(0, 0, W, BAR_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_SMALL);
  snprintf(buf, sizeof(buf), "APPRENTISSAGE %d/%d", index + 1, NUM_BUTTONS);
  u8g2.drawStr(3, 8, buf);
  u8g2.drawUTF8(W - 3 - u8g2.getUTF8Width(hint), 8, hint);
  u8g2.setDrawColor(1);
  u8g2.setFont(FONT_TITLE);
  const char *name = ACTION_NAMES[DEFAULT_MAP[index]];
  u8g2.drawUTF8((W - u8g2.getUTF8Width(name)) / 2, 24, name);
  u8g2.setFont(FONT_SMALL);
  snprintf(buf, sizeof(buf), "A %4u   B %4u", p.a, p.b);
  u8g2.drawStr((W - u8g2.getStrWidth(buf)) / 2, 37, buf);
  u8g2.sendBuffer();
}

// Waits until a button reading stays steady for 400 ms and returns its average.
// Returns false on timeout.
bool waitSteadyPress(int index, SwcPoint &out) {
  SwcPoint ref = {0, 0};
  unsigned long since = 0, start = millis();
  uint32_t sumA = 0, sumB = 0, n = 0;
  while (millis() - start < LEARN_TIMEOUT_MS) {
    SwcPoint p = readSwc();
    drawLearn(index, "TENEZ", p);
    if (swcIdle(p)) {
      since = 0;
    } else {
      if (since == 0 || swcDistance(p, ref) > 60) {
        ref = p;
        since = millis();
        sumA = sumB = n = 0;
      }
      sumA += p.a;
      sumB += p.b;
      n++;
      if (millis() - since >= 400) {
        out = {(uint16_t)(sumA / n), (uint16_t)(sumB / n)};
        return true;
      }
    }
    delay(10);
  }
  return false;
}

void waitRelease(int index) {
  unsigned long idleSince = 0;
  while (idleSince == 0 || millis() - idleSince < 200) {
    SwcPoint p = readSwc();
    drawLearn(index, "RELÂCHEZ", p);
    if (!swcIdle(p)) idleSince = 0;
    else if (idleSince == 0) idleSince = millis();
    delay(10);
  }
}

// Returns false if it was cancelled (no press within 30 s); the old readings are kept then
bool learnSteering() {
  Serial.println("\n=== Steering wheel learn mode: press and hold each button when asked ===");
  SwcPoint learned[NUM_BUTTONS];
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!waitSteadyPress(i, learned[i])) {
      Serial.println("Learn mode cancelled: no button pressed for 30 s");
      return false;
    }
    const char *name = ACTION_NAMES[DEFAULT_MAP[i]];
    Serial.printf("%d %-14s A %4u  B %4u\n", i + 1, name, learned[i].a, learned[i].b);
    for (int k = 0; k < i; k++)
      if (swcDistance(learned[i], learned[k]) < SWC_TOO_CLOSE)
        Serial.printf("  WARNING: too close to button %d %s (distance %d). Check the resistor mod on this button.\n",
                      k + 1, ACTION_NAMES[DEFAULT_MAP[k]], swcDistance(learned[i], learned[k]));
    waitRelease(i);
  }
  memcpy(swcTable, learned, sizeof(swcTable));
  saveSteering();
  swcReady = true;
  Serial.println("Steering wheel buttons saved");
  return true;
}
