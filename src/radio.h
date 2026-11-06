// Shared declarations for the Citroen radio firmware
//   main.cpp     display, Bluetooth, welcome, main screen
//   steering.cpp steering-wheel buttons, button map, learn mode
//   menu.cpp     settings menu and the setup button
#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

#define FW_VERSION "0.3"
#define BT_NAME "CITROEN RADIO"  // name the phone sees when pairing

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;
extern BluetoothA2DPSink a2dp;

const int W = 128;
const int H = 64;
// The radio case covers the lower part of the OLED, like the original radio's window.
// Everything is drawn in rows 0..VIEW_H-1; nothing is drawn below.
const int VIEW_H = 40;

// ---------- Screen layout and fonts ----------
const uint8_t *const FONT_TITLE = u8g2_font_6x12_tf;   // LCD-style fonts with full Latin-1 accents
const uint8_t *const FONT_ARTIST = u8g2_font_5x7_tf;
const uint8_t *const FONT_SMALL = u8g2_font_4x6_tf;
const int BAR_H = 11;

void iconPlay(int x, int y);

// ---------- Bluetooth state (written by the Bluetooth task, read by loop) ----------
struct NowPlaying {
  bool connected;
  bool playing;
  char title[128];
  char artist[128];
  char album[128];
  uint32_t durationMs;
  uint32_t posMs;       // position at posAt
  unsigned long posAt;  // millis() when posMs was set
  int volume;           // 0..127 (AVRCP absolute volume)
};

extern NowPlaying np;
extern portMUX_TYPE npLock;

void setVolume(int v);

// ---------- Sources and settings ----------
// Only Bluetooth exists so far; the others are shown so the status bar can be tested
enum Source { SRC_BT, SRC_FM, SRC_AM, SRC_TAPE, SRC_COUNT };
extern const char *SOURCE_NAMES[];
extern Source source;
extern bool loud;
extern bool muted;
extern int volBeforeMute;

struct Settings {
  uint8_t brightness;   // index into BRIGHTNESS_LEVELS
  uint8_t startVolume;  // volume in % sent to the phone on connect, 0 = keep the phone's
};
extern Settings settings;
const int NUM_BRIGHTNESS = 5;
extern const uint8_t BRIGHTNESS_LEVELS[NUM_BRIGHTNESS];

void loadSettings();
void saveSettings();
void applyBrightness();

// ---------- Steering wheel ----------
enum Action { ACT_NEXT, ACT_PREV, ACT_SELECT, ACT_MENU, ACT_VOL_UP, ACT_VOL_DOWN, ACT_PLAY_PAUSE, ACT_MUTE, NUM_ACTIONS };
extern const char *ACTION_NAMES[];

const int NUM_BUTTONS = 8;     // physical buttons, numbered like their factory functions (1 Next ... 8 Mute)
extern uint8_t buttonAction[NUM_BUTTONS];

struct SwcPoint {
  uint16_t a, b;
};

extern bool swcReady;
extern bool swcRawLog;

void loadSteering();
void saveButtonMap();
void resetButtonMap();
void clearSteering();
void pollSteering();
void pressButton(int button);
void doAction(Action a);
bool learnSteering();
bool steeringWired();
SwcPoint readSwc();
int matchSwc(SwcPoint p);
bool swcIdle(SwcPoint p);

// ---------- Settings menu ----------
void setupMenuButton();
void pollMenuButton();
void menuShortPress();
bool menuOpen();
bool menuKey(int button, Action a);  // true when the menu used the key
void drawMenu();
