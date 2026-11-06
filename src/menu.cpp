// ---------- Settings menu ----------
// Opened by the setup button (not by the steering wheel). Once open, the wheel navigates:
//   Next / Prev = down / up, Vol+ / Vol- = change value, Select = enter, Menu = back
// Setup button: short press = open / back, 1 s = close, 3 s = learn the steering wheel.
// Settings are saved to flash when the menu closes, only if something changed.

#include "radio.h"
#include <Preferences.h>

#define PIN_MENU_BTN 32  // push button to GND (internal pull-up)

// ---------- Settings ----------
const uint8_t BRIGHTNESS_LEVELS[NUM_BRIGHTNESS] = {8, 40, 90, 143, 255};  // 143 = SSD1309 default
Settings settings = {3, 0};
Preferences radioPrefs;

void loadSettings() {
  radioPrefs.begin("radio", true);
  settings.brightness = radioPrefs.getUChar("bright", 3);
  settings.startVolume = radioPrefs.getUChar("vol0", 0);
  source = (Source)(radioPrefs.getUChar("source", SRC_BT) % SRC_COUNT);
  loud = radioPrefs.getBool("loud", false);
  radioPrefs.end();
  if (settings.brightness >= NUM_BRIGHTNESS) settings.brightness = 3;
  if (settings.startVolume > 100) settings.startVolume = 0;
}

void saveSettings() {
  radioPrefs.begin("radio", false);
  radioPrefs.putUChar("bright", settings.brightness);
  radioPrefs.putUChar("vol0", settings.startVolume);
  radioPrefs.putUChar("source", source);
  radioPrefs.putBool("loud", loud);
  radioPrefs.end();
}

void applyBrightness() {
  u8g2.setContrast(BRIGHTNESS_LEVELS[settings.brightness]);
}

// ---------- Menu structure ----------
// i = the item's index in its list, so one set of callbacks can serve a whole list
struct MenuItem {
  const char *label;
  void (*value)(int i, char *buf, size_t len);  // text on the right, or nullptr
  void (*change)(int i, int dir);               // Vol+ / Vol-, or nullptr
  void (*enter)(int i);                         // Select; nullptr = same as Vol+
  bool submenu;                                 // shows a ▶ marker
};

struct MenuList {
  const char *title;
  const MenuItem *items;
  int count;
};

enum Screen { SCR_LIST, SCR_TEST, SCR_ABOUT };

struct Level {
  const MenuList *list;
  int cursor, top;
};

const int MAX_DEPTH = 4;
const int MENU_TIMEOUT_MS = 15000;
// Bar (rows 0..10) + 3 rows of 9 px (11..37), all inside the visible window
const int ROW_H = 9;
const int ROWS = 3;
const int LIST_Y = BAR_H;

Level stack[MAX_DEPTH];
int depth = 0;  // 0 = menu closed
Screen screen = SCR_LIST;
unsigned long lastKey = 0;
bool settingsDirty = false, mapDirty = false;
int testLast = -1;  // last button seen on the test screen

void push(const MenuList *list) {
  if (depth < MAX_DEPTH) stack[depth++] = {list, 0, 0};
}

void closeMenu() {
  if (settingsDirty) saveSettings();
  if (mapDirty) saveButtonMap();
  if (settingsDirty || mapDirty) Serial.println("Settings saved");
  settingsDirty = mapDirty = false;
  depth = 0;
  screen = SCR_LIST;
}

void back() {
  if (screen != SCR_LIST) screen = SCR_LIST;
  else if (--depth <= 0) closeMenu();
}

void backItem(int) { back(); }

// ---------- Main list ----------
void valSource(int, char *buf, size_t len) { strlcpy(buf, SOURCE_NAMES[source], len); }
void chgSource(int, int dir) {
  source = (Source)((source + dir + SRC_COUNT) % SRC_COUNT);
  settingsDirty = true;
}

void valLoud(int, char *buf, size_t len) { strlcpy(buf, loud ? "OUI" : "NON", len); }
void chgLoud(int, int) {
  loud = !loud;
  settingsDirty = true;
}

void valBright(int, char *buf, size_t len) { snprintf(buf, len, "%d/%d", settings.brightness + 1, NUM_BRIGHTNESS); }
void chgBright(int, int dir) {
  settings.brightness = constrain(settings.brightness + dir, 0, NUM_BRIGHTNESS - 1);
  applyBrightness();
  settingsDirty = true;
}

// 0 (keep the phone's volume), then 20..100 % in steps of 10
void valStartVol(int, char *buf, size_t len) {
  if (settings.startVolume == 0) strlcpy(buf, "DERNIER", len);
  else snprintf(buf, len, "%d %%", settings.startVolume);
}
void chgStartVol(int, int dir) {
  int v = settings.startVolume;
  if (v == 0) v = dir > 0 ? 20 : 0;
  else if (v == 20 && dir < 0) v = 0;
  else v = constrain(v + dir * 10, 20, 100);
  settings.startVolume = v;
  settingsDirty = true;
}

extern const MenuList buttonsList, btList;
void openButtons(int) { push(&buttonsList); }
void openBluetooth(int) { push(&btList); }
void openAbout(int) { screen = SCR_ABOUT; }
void quit(int) { closeMenu(); }

const MenuItem MAIN_ITEMS[] = {
  {"SOURCE", valSource, chgSource, nullptr, false},
  {"LOUD", valLoud, chgLoud, nullptr, false},
  {"LUMINOSITÉ", valBright, chgBright, nullptr, false},
  {"VOLUME INITIAL", valStartVol, chgStartVol, nullptr, false},
  {"BOUTONS VOLANT", nullptr, nullptr, openButtons, true},
  {"BLUETOOTH", nullptr, nullptr, openBluetooth, true},
  {"À PROPOS", nullptr, nullptr, openAbout, true},
  {"QUITTER", nullptr, nullptr, quit, false},
};
const MenuList mainList = {"RÉGLAGES", MAIN_ITEMS, sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0])};

// ---------- Steering wheel buttons ----------
void learn(int) {
  learnSteering();  // blocking, draws its own screens
  lastKey = millis();
}

const char *BUTTON_LABELS[NUM_BUTTONS] = {"BOUTON 1", "BOUTON 2", "BOUTON 3", "BOUTON 4",
                                          "BOUTON 5", "BOUTON 6", "BOUTON 7", "BOUTON 8"};
void valAssign(int i, char *buf, size_t len) { strlcpy(buf, ACTION_NAMES[buttonAction[i]], len); }
void chgAssign(int i, int dir) {
  buttonAction[i] = (buttonAction[i] + dir + NUM_ACTIONS) % NUM_ACTIONS;
  mapDirty = true;
}

const MenuItem ASSIGN_ITEMS[] = {
  {BUTTON_LABELS[0], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[1], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[2], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[3], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[4], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[5], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[6], valAssign, chgAssign, nullptr, false},
  {BUTTON_LABELS[7], valAssign, chgAssign, nullptr, false},
  {"RETOUR", nullptr, nullptr, backItem, false},
};
const MenuList assignList = {"ASSIGNER", ASSIGN_ITEMS, sizeof(ASSIGN_ITEMS) / sizeof(ASSIGN_ITEMS[0])};

void openAssign(int) { push(&assignList); }
void openTest(int) {
  screen = SCR_TEST;
  testLast = -1;
}
void defaultMap(int) {
  resetButtonMap();
  mapDirty = true;
  Serial.println("Button map reset to the factory functions");
}

const MenuItem BUTTONS_ITEMS[] = {
  {"APPRENDRE", nullptr, nullptr, learn, false},
  {"ASSIGNER", nullptr, nullptr, openAssign, true},
  {"TESTER", nullptr, nullptr, openTest, true},
  {"PAR DÉFAUT", nullptr, nullptr, defaultMap, false},
  {"RETOUR", nullptr, nullptr, backItem, false},
};
const MenuList buttonsList = {"BOUTONS VOLANT", BUTTONS_ITEMS, sizeof(BUTTONS_ITEMS) / sizeof(BUTTONS_ITEMS[0])};

// ---------- Bluetooth ----------
void valBtState(int, char *buf, size_t len) { strlcpy(buf, np.connected ? "CONNECTÉ" : "EN ATTENTE", len); }

void disconnectPhone(int) {
  if (!np.connected) return;
  a2dp.disconnect();
  Serial.println("Bluetooth: phone disconnected");
}

// Removes every paired phone, so the radio doesn't reconnect to it
void forgetDevices(int) {
  a2dp.disconnect();
  int n = esp_bt_gap_get_bond_device_num();
  if (n > 0) {
    esp_bd_addr_t *list = (esp_bd_addr_t *)malloc(n * sizeof(esp_bd_addr_t));
    if (list && esp_bt_gap_get_bond_device_list(&n, list) == ESP_OK)
      for (int i = 0; i < n; i++) esp_bt_gap_remove_bond_device(list[i]);
    free(list);
  }
  a2dp.clean_last_connection();
  Serial.printf("Bluetooth: %d paired device(s) forgotten\n", n);
  back();
}

const MenuItem CONFIRM_ITEMS[] = {
  {"NON", nullptr, nullptr, backItem, false},
  {"OUI", nullptr, nullptr, forgetDevices, false},
};
const MenuList confirmList = {"OUBLIER LES APPAREILS ?", CONFIRM_ITEMS, 2};

void openConfirm(int) { push(&confirmList); }

const MenuItem BT_ITEMS[] = {
  {"ÉTAT", valBtState, nullptr, nullptr, false},
  {"DÉCONNECTER", nullptr, nullptr, disconnectPhone, false},
  {"OUBLIER APPAREILS", nullptr, nullptr, openConfirm, true},
  {"RETOUR", nullptr, nullptr, backItem, false},
};
const MenuList btList = {"BLUETOOTH", BT_ITEMS, sizeof(BT_ITEMS) / sizeof(BT_ITEMS[0])};

// ---------- Keys ----------
bool menuOpen() { return depth > 0; }

void menuShortPress() {
  lastKey = millis();
  if (depth == 0) push(&mainList);
  else back();
}

bool menuKey(int button, Action a) {
  if (depth == 0) return false;
  lastKey = millis();
  if (screen == SCR_TEST) {
    testLast = button;
    return true;
  }
  if (screen == SCR_ABOUT) {
    screen = SCR_LIST;
    return true;
  }

  Level &lv = stack[depth - 1];
  const MenuItem &it = lv.list->items[lv.cursor];
  switch (a) {
    case ACT_NEXT: lv.cursor = (lv.cursor + 1) % lv.list->count; break;
    case ACT_PREV: lv.cursor = (lv.cursor + lv.list->count - 1) % lv.list->count; break;
    case ACT_VOL_UP:
    case ACT_VOL_DOWN:
      if (it.change) it.change(lv.cursor, a == ACT_VOL_UP ? 1 : -1);
      break;
    case ACT_SELECT:
      if (it.enter) it.enter(lv.cursor);
      else if (it.change) it.change(lv.cursor, 1);
      break;
    case ACT_MENU: back(); break;
    default: break;  // Play/Pause and Mute do nothing in the menu
  }
  return true;
}

void setupMenuButton() {
  pinMode(PIN_MENU_BTN, INPUT_PULLUP);
}

void pollMenuButton() {
  static unsigned long lastRead = 0, since = 0;
  static bool down = false, fired = false;
  if (depth > 0 && millis() - lastKey > MENU_TIMEOUT_MS) closeMenu();
  if (millis() - lastRead < 20) return;  // sampling every 20 ms skips contact bounce
  lastRead = millis();

  bool pressed = digitalRead(PIN_MENU_BTN) == LOW;
  if (pressed && !down) {
    down = true;
    fired = false;
    since = millis();
  } else if (pressed && !fired && millis() - since >= 3000) {
    fired = true;  // held 3 s: learn the steering wheel
    learnSteering();
    lastKey = millis();
  } else if (!pressed && down) {
    down = false;
    unsigned long held = millis() - since;
    if (fired || held < 40) return;
    if (held >= 1000) {
      if (depth > 0) closeMenu();
      else menuShortPress();
    } else {
      menuShortPress();
    }
  }
}

// ---------- Drawing ----------
void drawBar(const char *title) {
  u8g2.drawBox(0, 0, W, BAR_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_ARTIST);
  u8g2.drawUTF8(3, 9, title);
  u8g2.setDrawColor(1);
}

// Highlighted row is inverted, values right-aligned, like a 90s head unit
void drawList() {
  Level &lv = stack[depth - 1];
  const MenuList &m = *lv.list;
  if (lv.cursor < lv.top) lv.top = lv.cursor;
  if (lv.cursor >= lv.top + ROWS) lv.top = lv.cursor - ROWS + 1;

  drawBar(m.title);
  bool scroll = m.count > ROWS;
  int right = scroll ? W - 5 : W - 1;  // room for the scroll bar
  u8g2.setFont(FONT_ARTIST);
  for (int r = 0; r < ROWS && lv.top + r < m.count; r++) {
    int i = lv.top + r;
    const MenuItem &it = m.items[i];
    int y = LIST_Y + r * ROW_H;
    if (i == lv.cursor) {
      u8g2.drawBox(0, y, right + 1, ROW_H);
      u8g2.setDrawColor(0);
    }
    u8g2.drawUTF8(3, y + 7, it.label);
    if (it.submenu) {
      iconPlay(right - 5, y + 2);
    } else if (it.value) {
      char v[24];
      it.value(i, v, sizeof(v));
      u8g2.drawUTF8(right - 2 - u8g2.getUTF8Width(v), y + 7, v);
    }
    u8g2.setDrawColor(1);
  }
  if (scroll) {  // dotted track with a solid thumb
    int trackH = ROWS * ROW_H;
    int thumbH = max(3, trackH * ROWS / m.count);
    int thumbY = LIST_Y + (trackH - thumbH) * lv.top / (m.count - ROWS);
    for (int y = LIST_Y; y < LIST_Y + trackH; y += 2) u8g2.drawPixel(W - 2, y);
    u8g2.drawBox(W - 3, thumbY, 3, thumbH);
  }
}

void drawCentered(const char *text, int y, const uint8_t *font) {
  u8g2.setFont(font);
  u8g2.drawUTF8((W - u8g2.getUTF8Width(text)) / 2, y, text);
}

// Matching button + its action, then the live readings (rows 0..38).
// The setup button goes back.
void drawTest() {
  char buf[32];
  drawBar("TEST BOUTONS");
  SwcPoint p = readSwc();
  snprintf(buf, sizeof(buf), "A %4u   B %4u", p.a, p.b);
  drawCentered(buf, 38, FONT_SMALL);
  if (!swcReady) {
    drawCentered("NON APPRIS", 23, FONT_TITLE);
    drawCentered("Utilisez APPRENDRE", 31, FONT_ARTIST);
  } else {
    int m = swcIdle(p) ? -1 : matchSwc(p);
    if (m >= 0) {
      testLast = m;
      lastKey = millis();  // stay open while buttons are being pressed
    }
    if (testLast < 0) {
      drawCentered("APPUYEZ", 23, FONT_TITLE);
    } else {
      drawCentered(BUTTON_LABELS[testLast], 23, FONT_TITLE);
      drawCentered(ACTION_NAMES[buttonAction[testLast]], 31, FONT_ARTIST);
    }
  }
}

void drawAbout() {
  char buf[40];
  drawBar("À PROPOS");
  // 4 lines of the 4x6 font, 7 px apart (rows 12..38)
  drawCentered("CITROËN RADIO v" FW_VERSION, 17, FONT_SMALL);
  snprintf(buf, sizeof(buf), "Bluetooth: %s", np.connected ? "connecté" : "en attente");
  drawCentered(buf, 24, FONT_SMALL);
  snprintf(buf, sizeof(buf), "Mémoire libre: %u Ko", (unsigned)(ESP.getFreeHeap() / 1024));
  drawCentered(buf, 31, FONT_SMALL);
  drawCentered(swcReady ? "Volant: appris" : "Volant: non appris", 38, FONT_SMALL);
}

void drawMenu() {
  u8g2.clearBuffer();
  if (screen == SCR_TEST) drawTest();
  else if (screen == SCR_ABOUT) drawAbout();
  else drawList();
  u8g2.sendBuffer();
}
