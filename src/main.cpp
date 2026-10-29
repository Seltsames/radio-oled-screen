// 90s Citroen car radio (ESP32 WROOM-32)
//
// - Bluetooth audio receiver (A2DP) -> I2S -> MCU-PCM5102 DAC
// - Song info from the phone (AVRCP): title, artist, duration, position, play/pause, volume
// - 2.42" SSD1309 OLED: dot-matrix welcome sequence, then a now-playing screen
//
// Display wiring (OLED pin -> ESP32 GPIO):
//   GND -> GND, VCC -> 3V3, SCK/D0 -> 18, SDA/D1 -> 23, RES -> 19, DC -> 21, CS -> 5
// DAC wiring (PCM5102 pin -> ESP32 GPIO):
//   BCK -> 26, LCK -> 25, DIN -> 22, SCK -> GND, VIN -> 5V, GND -> GND
// Steering wheel PCB (two resistor ladders):
//   P1 -> GND, P4 -> GPIO 34 + 1 kOhm to 3V3, P5 -> GPIO 35 + 1 kOhm to 3V3,
//   P2 (backlight +12 V) and P3 not connected
// (GPIO 16/17 are reserved for the UART link)

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include <Preferences.h>

// ---------- Pins ----------
#define PIN_SCK  18
#define PIN_MOSI 23
#define PIN_CS   5
#define PIN_DC   21
#define PIN_RST  19

#define PIN_I2S_BCK  26
#define PIN_I2S_WS   25
#define PIN_I2S_DATA 22

#define BT_NAME "CITROEN RADIO"  // name the phone sees when pairing

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, PIN_CS, PIN_DC, PIN_RST);
I2SStream i2s;
BluetoothA2DPSink a2dp(i2s);

const int W = 128;
const int H = 64;

// ---------- Dot-matrix font (welcome sequence) ----------
// Classic 5x7 character generator, ' ' to 'Z', one byte per dot column (bit 0 = top dot)
const uint8_t FONT5X7[][5] = {
  {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},  //   ! "
  {0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},  // # $ %
  {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},  // & ' (
  {0x00, 0x41, 0x22, 0x1C, 0x00}, {0x08, 0x2A, 0x1C, 0x2A, 0x08}, {0x08, 0x08, 0x3E, 0x08, 0x08},  // ) * +
  {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},  // , - .
  {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},  // / 0 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10},  // 2 3 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},  // 5 6 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},  // 8 9 :
  {0x00, 0x56, 0x36, 0x00, 0x00}, {0x00, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14},  // ; < =
  {0x41, 0x22, 0x14, 0x08, 0x00}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x32, 0x49, 0x79, 0x41, 0x3E},  // > ? @
  {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},  // A B C
  {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x01, 0x01},  // D E F
  {0x3E, 0x41, 0x41, 0x51, 0x32}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},  // G H I
  {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},  // J K L
  {0x7F, 0x02, 0x04, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},  // M N O
  {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},  // P Q R
  {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},  // S T U
  {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x7F, 0x20, 0x18, 0x20, 0x7F}, {0x63, 0x14, 0x08, 0x14, 0x63},  // V W X
  {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43},                                  // Y Z
};

// Extra 5x7 glyphs at codes 1..N: French accents and the Citroen double chevron
enum { G_A_GRAVE = 1, G_E_ACUTE, G_E_GRAVE, G_E_DIAER, G_C_CEDIL, G_CHEVRONS, NUM_CUSTOM = G_CHEVRONS };
const uint8_t CUSTOM[NUM_CUSTOM][5] = {
  {0x78, 0x15, 0x16, 0x14, 0x78},  // À
  {0x7C, 0x54, 0x56, 0x55, 0x44},  // É
  {0x7C, 0x55, 0x56, 0x54, 0x44},  // È
  {0x7C, 0x55, 0x54, 0x55, 0x44},  // Ë
  {0x1E, 0x21, 0x61, 0x61, 0x12},  // Ç
  {0x24, 0x12, 0x09, 0x12, 0x24},  // double chevron (written as ^)
};

const uint8_t *glyph(uint8_t code) {
  if (code >= 1 && code <= NUM_CUSTOM) return CUSTOM[code - 1];
  if (code >= 'a' && code <= 'z') code -= 32;
  if (code < ' ' || code > 'Z') code = '?';
  return FONT5X7[code - ' '];
}

// Converts UTF-8 text to dot-matrix glyph codes: French accents get their own glyphs, '^' is the chevron logo
int toCodes(const char *s, uint8_t *out, int max) {
  int n = 0;
  while (*s && n < max) {
    uint8_t c = *s++;
    if (c == 0xC3 && *s) {
      uint8_t d = (uint8_t)*s++ | 0x20;  // fold uppercase accents onto lowercase
      switch (d) {
        case 0xA0: case 0xA2: c = G_A_GRAVE; break;  // à â
        case 0xA9: c = G_E_ACUTE; break;             // é
        case 0xA8: case 0xAA: c = G_E_GRAVE; break;  // è ê
        case 0xAB: c = G_E_DIAER; break;             // ë
        case 0xA7: c = G_C_CEDIL; break;             // ç
        default: c = '?';
      }
    } else if (c == '^') {
      c = G_CHEVRONS;
    } else if (c >= 0x80) {
      continue;
    }
    out[n++] = c;
  }
  return n;
}

// Every dot is 1 pixel with a 1 pixel gap
const int PITCH = 2;
const int BIG_ADV = 11;  // 5x7 character: 9 x 13 px + 2 px gap

void dot(int x, int y) {
  if (x >= 0 && x < W && y >= 0 && y < H) u8g2.drawPixel(x, y);
}

int bigWidth(int n) { return n > 0 ? n * BIG_ADV - 2 : 0; }

void drawBig(int x, int y, uint8_t code) {
  const uint8_t *g = glyph(code);
  for (int dx = 0; dx < 5; dx++)
    for (int dy = 0; dy < 7; dy++)
      if (g[dx] & (1 << dy)) dot(x + dx * PITCH, y + dy * PITCH);
}

void drawBigCodes(int x, int y, const uint8_t *codes, int n) {
  for (int i = 0; i < n; i++) drawBig(x + i * BIG_ADV, y, codes[i]);
}

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

NowPlaying np;
portMUX_TYPE npLock = portMUX_INITIALIZER_UNLOCKED;

uint32_t currentPos(const NowPlaying &s) {
  uint32_t p = s.posMs + (s.playing ? millis() - s.posAt : 0);
  return (s.durationMs && p > s.durationMs) ? s.durationMs : p;
}

void onConnection(esp_a2d_connection_state_t state, void *) {
  portENTER_CRITICAL(&npLock);
  np.connected = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
  if (!np.connected) {
    np.playing = false;
    np.title[0] = np.artist[0] = np.album[0] = 0;
    np.durationMs = np.posMs = 0;
  }
  portEXIT_CRITICAL(&npLock);
}

void onMetadata(uint8_t id, const uint8_t *text) {
  const char *t = (const char *)text;
  portENTER_CRITICAL(&npLock);
  switch (id) {
    case ESP_AVRC_MD_ATTR_TITLE:
      if (strcmp(np.title, t) != 0) {  // new song: position restarts
        strlcpy(np.title, t, sizeof(np.title));
        np.posMs = 0;
        np.posAt = millis();
      }
      break;
    case ESP_AVRC_MD_ATTR_ARTIST: strlcpy(np.artist, t, sizeof(np.artist)); break;
    case ESP_AVRC_MD_ATTR_ALBUM: strlcpy(np.album, t, sizeof(np.album)); break;
    case ESP_AVRC_MD_ATTR_PLAYING_TIME: np.durationMs = strtoul(t, nullptr, 10); break;
  }
  portEXIT_CRITICAL(&npLock);
}

void onPlayStatus(esp_avrc_playback_stat_t status) {
  portENTER_CRITICAL(&npLock);
  np.posMs = currentPos(np);  // freeze the position at the moment of the change
  np.posAt = millis();
  np.playing = status == ESP_AVRC_PLAYBACK_PLAYING;
  portEXIT_CRITICAL(&npLock);
}

void onPlayPos(uint32_t posMs) {
  portENTER_CRITICAL(&npLock);
  np.posMs = posMs;
  np.posAt = millis();
  portEXIT_CRITICAL(&npLock);
}

void onVolume(int volume) {
  portENTER_CRITICAL(&npLock);
  np.volume = volume;
  portEXIT_CRITICAL(&npLock);
}

void startBluetooth() {
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = PIN_I2S_BCK;
  cfg.pin_ws = PIN_I2S_WS;
  cfg.pin_data = PIN_I2S_DATA;
  i2s.begin(cfg);

  a2dp.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
                                        ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
  a2dp.set_avrc_metadata_callback(onMetadata);
  a2dp.set_avrc_rn_playstatus_callback(onPlayStatus);
  a2dp.set_avrc_rn_play_pos_callback(onPlayPos, 1);
  a2dp.set_avrc_rn_volumechange(onVolume);
  a2dp.set_on_connection_state_changed(onConnection);
  a2dp.start(BT_NAME);
  np.volume = a2dp.get_volume();
}

// ---------- Welcome ----------
// Lit pixels switch off at random until the screen is empty
void dissolve(int ms) {
  uint8_t *buf = u8g2.getBufferPtr();
  int size = u8g2.getBufferTileWidth() * u8g2.getBufferTileHeight() * 8;
  for (int s = 0; s < 20; s++) {
    for (int k = 0; k < 150; k++) buf[random(size)] &= ~(1 << random(8));
    u8g2.sendBuffer();
    delay(ms / 20);
  }
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void welcome() {
  uint8_t codes[24];
  const int y = (H - 13) / 2;  // vertically centered row

  // Chevron + CITROËN appears in the center one character at a time, then dissolves
  int n = toCodes("^ CITROËN", codes, 24);
  int x = (W - bigWidth(n)) / 2;
  for (int i = 1; i <= n; i++) {
    u8g2.clearBuffer();
    drawBigCodes(x, y, codes, i);
    u8g2.sendBuffer();
    delay(120);
  }
  delay(1800);
  dissolve(900);
  delay(300);

  // BIENVENUE À BORD runs across the center, one dot column per step like an LED sign
  n = toCodes("BIENVENUE À BORD", codes, 24);
  for (int sx = W; sx > -bigWidth(n); sx -= PITCH) {
    u8g2.clearBuffer();
    drawBigCodes(sx, y, codes, n);
    u8g2.sendBuffer();
    delay(25);
  }
  delay(300);
}

// ---------- Sources ----------
// Only Bluetooth exists so far; the others are shown so the status bar can be tested
enum Source { SRC_BT, SRC_FM, SRC_AM, SRC_TAPE, SRC_COUNT };
const char *SOURCE_NAMES[] = {"BLUETOOTH", "RADIO FM", "RADIO AM", "CASSETTE"};
Source source = SRC_BT;
bool loud = false;
bool muted = false;
int volBeforeMute = 0;

// Short message in the free area under the progress bar (button feedback)
char toast[32];
unsigned long toastUntil = 0;

void showToast(const char *text) {
  strlcpy(toast, text, sizeof(toast));
  toastUntil = millis() + 1500;
}

// ---------- Main screen ----------
// Layout: inverted status bar on top, then title, artist and playback strip spaced evenly
const uint8_t *FONT_TITLE = u8g2_font_6x12_tf;   // LCD-style fonts with full Latin-1 accents
const uint8_t *FONT_ARTIST = u8g2_font_5x7_tf;
const uint8_t *FONT_SMALL = u8g2_font_4x6_tf;

const int BAR_H = 11;
const int TITLE_Y = 22;   // baselines: capitals span 13..21
const int ARTIST_Y = 32;  // capitals span 26..31, 4 px below the title
const int STRIP_Y = 36;   // progress bar rows 36..38, 4 px below the artist

const int SCROLL_PAUSE_MS = 2500;  // still time before (and between) scrolls
const int SCROLL_STEP_MS = 45;     // one pixel every 45 ms, about 22 px per second
const int SCROLL_GAP = 32;         // space before the text repeats

// A line of text: centered if it fits, otherwise it waits, then glides left pixel by pixel and wraps around
struct ScrollText {
  char text[128];
  unsigned long since;
};

ScrollText titleLine, artistLine;

void setText(ScrollText &s, const char *text) {
  if (strcmp(s.text, text) == 0) return;
  strlcpy(s.text, text, sizeof(s.text));
  s.since = millis();
}

void drawScrolling(const ScrollText &s, int y, const uint8_t *font) {
  u8g2.setFont(font);
  int w = u8g2.getUTF8Width(s.text);
  if (w <= W - 4) {
    u8g2.drawUTF8((W - w) / 2, y, s.text);
    return;
  }
  int cycle = w + SCROLL_GAP;
  unsigned long period = SCROLL_PAUSE_MS + (unsigned long)cycle * SCROLL_STEP_MS;
  unsigned long t = (millis() - s.since) % period;
  int offset = t < SCROLL_PAUSE_MS ? 0 : (t - SCROLL_PAUSE_MS) / SCROLL_STEP_MS;
  int x = 2 - offset;
  u8g2.drawUTF8(x, y, s.text);
  u8g2.drawUTF8(x + cycle, y, s.text);
}

// Icons, drawn in the current draw color
void iconBluetooth(int x, int y) {  // 5 x 9
  u8g2.drawVLine(x + 2, y, 9);
  u8g2.drawLine(x + 2, y, x + 4, y + 2);
  u8g2.drawLine(x + 4, y + 2, x, y + 6);
  u8g2.drawLine(x + 2, y + 8, x + 4, y + 6);
  u8g2.drawLine(x + 4, y + 6, x, y + 2);
}

void iconPhone(int x, int y) {  // 5 x 9 smartphone
  u8g2.drawFrame(x, y, 5, 9);
  u8g2.drawPixel(x + 2, y + 7);
}

void iconCross(int x, int y) {  // 5 x 5
  u8g2.drawLine(x, y, x + 4, y + 4);
  u8g2.drawLine(x, y + 4, x + 4, y);
}

void iconSpeaker(int x, int y) {  // 6 x 7
  u8g2.drawBox(x, y + 2, 2, 3);
  u8g2.drawLine(x + 2, y + 2, x + 4, y);
  u8g2.drawLine(x + 2, y + 4, x + 4, y + 6);
  u8g2.drawVLine(x + 4, y, 7);
  u8g2.drawVLine(x + 3, y + 1, 5);
}

void iconCassette(int x, int y) {  // 11 x 7
  u8g2.drawFrame(x, y, 11, 7);
  u8g2.drawCircle(x + 3, y + 3, 1);
  u8g2.drawCircle(x + 7, y + 3, 1);
}

void iconPlay(int x, int y) {  // 3 x 5
  for (int i = 0; i < 3; i++) u8g2.drawVLine(x + i, y + i, 5 - 2 * i);
}

void iconPause(int x, int y) {  // 4 x 5
  u8g2.drawBox(x, y, 1, 5);
  u8g2.drawBox(x + 3, y, 1, 5);
}

void drawStatusBar(const NowPlaying &s) {
  u8g2.drawBox(0, 0, W, BAR_H);
  u8g2.setDrawColor(0);  // icons are cut out of the bar
  iconBluetooth(3, 1);
  if (s.connected) iconPhone(12, 1);
  else iconCross(12, 3);

  // Indicators after the phone: the selected source (when it is not Bluetooth) and LOUD
  u8g2.setFont(FONT_SMALL);
  int x = 23;
  if (source == SRC_FM || source == SRC_AM) {
    const char *band = source == SRC_FM ? "FM" : "AM";
    u8g2.drawStr(x, 8, band);
    x += u8g2.getStrWidth(band) + 5;
  } else if (source == SRC_TAPE) {
    iconCassette(x, 2);
    x += 16;
  }
  if (loud) u8g2.drawStr(x, 8, "LOUD");

  if (muted) {
    iconCross(W - 8, 3);
    iconSpeaker(W - 17, 2);
  } else if (s.connected) {
    char v[6];
    snprintf(v, sizeof(v), "%d", s.volume * 100 / 127);
    u8g2.setFont(FONT_SMALL);
    int vw = u8g2.getStrWidth(v);
    u8g2.drawStr(W - 3 - vw, 8, v);
    iconSpeaker(W - 3 - vw - 9, 2);
  }
  u8g2.setDrawColor(1);
}

void formatTime(char *buf, size_t len, uint32_t ms) {
  uint32_t s = ms / 1000;
  snprintf(buf, len, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// Played part = solid 3 px bar, rest = dotted line through the bar's center; y = top row
void drawProgress(int x0, int x1, int y, float progress) {
  int filled = (int)((x1 - x0 + 1) * progress + 0.5f);
  if (filled > 0) u8g2.drawBox(x0, y, filled, 3);
  for (int x = x0 + filled; x <= x1; x++)
    if ((x - x0) % PITCH == 0) u8g2.drawPixel(x, y + 1);
}

// Icon, times and bar share one row, centered on the bar's middle line (STRIP_Y + 1)
void drawPlaybackStrip(const NowPlaying &s) {
  const int iconY = STRIP_Y - 1;      // 5 px icons span STRIP_Y - 1 .. STRIP_Y + 3
  const int textBase = STRIP_Y + 4;   // 5 px digits span STRIP_Y - 1 .. STRIP_Y + 3
  if (s.playing) iconPlay(2, iconY);
  else iconPause(1, iconY);

  char elapsed[8], total[8];
  uint32_t pos = currentPos(s);
  formatTime(elapsed, sizeof(elapsed), pos);
  u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(9, textBase, elapsed);
  if (s.durationMs) {
    formatTime(total, sizeof(total), s.durationMs);
    u8g2.drawStr(W - 1 - u8g2.getStrWidth(total), textBase, total);
  }
  drawProgress(32, 103, STRIP_Y, s.durationMs ? (float)pos / s.durationMs : 0);
}

void render() {
  NowPlaying s;
  portENTER_CRITICAL(&npLock);
  s = np;
  portEXIT_CRITICAL(&npLock);

  bool showPlayback = source == SRC_BT && s.connected;
  if (source != SRC_BT) {
    setText(titleLine, SOURCE_NAMES[source]);
    setText(artistLine, "NON DISPONIBLE");  // no tuner or tape deck yet
  } else if (!s.connected) {
    setText(titleLine, "EN ATTENTE");
    setText(artistLine, BT_NAME);
  } else if (!s.title[0]) {
    setText(titleLine, "CONNECTÉ");
    setText(artistLine, "");
  } else {
    setText(titleLine, s.title);
    setText(artistLine, s.artist);
  }

  u8g2.clearBuffer();
  drawStatusBar(s);
  bool waiting = source == SRC_BT && !s.connected;
  if (!waiting || (millis() / 600) % 2) drawScrolling(titleLine, TITLE_Y, FONT_TITLE);  // "EN ATTENTE" blinks
  drawScrolling(artistLine, ARTIST_Y, FONT_ARTIST);
  if (showPlayback) drawPlaybackStrip(s);
  if (millis() < toastUntil) {
    u8g2.setFont(FONT_ARTIST);
    int w = u8g2.getUTF8Width(toast);
    u8g2.drawRFrame((W - w) / 2 - 5, 46, w + 10, 12, 3);
    u8g2.drawUTF8((W - w) / 2, 55, toast);
  }
  u8g2.sendBuffer();
}

// Prints song changes to the serial monitor
void logChanges() {
  static char lastTitle[128];
  static bool lastConnected = false, lastPlaying = false;
  NowPlaying s;
  portENTER_CRITICAL(&npLock);
  s = np;
  portEXIT_CRITICAL(&npLock);
  if (s.connected != lastConnected) Serial.println(s.connected ? "BT connected" : "BT disconnected");
  if (s.playing != lastPlaying) Serial.println(s.playing ? "Playing" : "Paused");
  if (strcmp(s.title, lastTitle) != 0 && s.title[0])
    Serial.printf("Now playing: %s - %s [%s] %lu ms\n", s.title, s.artist, s.album, (unsigned long)s.durationMs);
  lastConnected = s.connected;
  lastPlaying = s.playing;
  strlcpy(lastTitle, s.title, sizeof(lastTitle));
}

void loadSteering();

void setup() {
  Serial.begin(115200);
  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  u8g2.setBusClock(8000000);
  u8g2.begin();
  randomSeed(esp_random());

  loadSteering();
  startBluetooth();  // the phone can already connect during the welcome
  Serial.println("Bluetooth started as \"" BT_NAME "\"");
  welcome();
}

// ---------- Steering wheel buttons ----------
// Two resistor ladders share P1 (ground). Each press pulls one line down to its own voltage
// and the other line part of the way (they are linked by 666 Ohm on the PCB), so every button
// has a unique (A, B) reading. Readings are learned once and stored in flash.
#define PIN_SWC_A 34  // PCB P4: Vol-, Vol+, Select, Menu
#define PIN_SWC_B 35  // PCB P5: Play/Pause, Prev, Next, Mute

enum Action { ACT_NEXT, ACT_PREV, ACT_SELECT, ACT_MENU, ACT_VOL_UP, ACT_VOL_DOWN, ACT_PLAY_PAUSE, ACT_MUTE, NUM_ACTIONS };
const char *ACTION_NAMES[] = {"SUIVANT", "PRÉCÉDENT", "SÉLECTION", "MENU", "VOLUME +", "VOLUME -", "LECTURE/PAUSE", "MUET"};

struct SwcPoint {
  uint16_t a, b;
};

SwcPoint swcTable[NUM_ACTIONS];
bool swcReady = false;  // the buttons stay off until learned (unconnected inputs float)
bool swcRawLog = false;

const int SWC_IDLE = 3950;        // both lines above this = no button pressed
const int SWC_MATCH = 400;        // max |dA| + |dB| from a learned point
const int SWC_TOO_CLOSE = 300;    // learned points closer than this are reported
const int SWC_STABLE = 4;         // samples (10 ms apart) before a press counts
const int SWC_REPEAT_DELAY = 500; // volume repeats while held
const int SWC_REPEAT_MS = 150;
const int VOL_STEP = 8;           // of 127

Preferences swcPrefs;

void loadSteering() {
  swcPrefs.begin("swc", true);
  swcReady = swcPrefs.getBytesLength("table") == sizeof(swcTable) &&
             swcPrefs.getBytes("table", swcTable, sizeof(swcTable)) == sizeof(swcTable);
  swcPrefs.end();
  Serial.println(swcReady ? "Steering wheel: learned buttons loaded" : "Steering wheel: not learned yet (send 'L')");
}

void saveSteering() {
  swcPrefs.begin("swc", false);
  swcPrefs.putBytes("table", swcTable, sizeof(swcTable));
  swcPrefs.end();
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

int matchSwc(SwcPoint p) {
  int best = -1, bestDist = SWC_MATCH;
  for (int i = 0; i < NUM_ACTIONS; i++) {
    int d = swcDistance(p, swcTable[i]);
    if (d < bestDist) { bestDist = d; best = i; }
  }
  return best;
}

void setVolume(int v) {
  v = constrain(v, 0, 127);
  muted = false;
  a2dp.set_volume(v);
  portENTER_CRITICAL(&npLock);
  np.volume = v;
  portEXIT_CRITICAL(&npLock);
}

void doAction(Action a) {
  Serial.printf("Steering: %s\n", ACTION_NAMES[a]);
  char msg[32];
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
      snprintf(msg, sizeof(msg), "VOLUME %d", np.volume * 100 / 127);
      showToast(msg);
      return;
    case ACT_MUTE:
      if (muted) {
        setVolume(volBeforeMute);
        showToast("SON ACTIVÉ");
      } else {
        volBeforeMute = np.volume;
        a2dp.set_volume(0);
        muted = true;
        showToast("MUET");
      }
      return;
    case ACT_SELECT:
    case ACT_MENU:
    default:
      break;  // functions still to be decided
  }
  showToast(ACTION_NAMES[a]);
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
      doAction((Action)held);
      pressedAt = lastRepeat = millis();
    }
  } else if ((held == ACT_VOL_UP || held == ACT_VOL_DOWN) && millis() - pressedAt > SWC_REPEAT_DELAY &&
             millis() - lastRepeat > SWC_REPEAT_MS) {
    doAction((Action)held);
    lastRepeat = millis();
  }
}

// ---------- Steering wheel learn mode ----------
void drawLearn(int index, const char *hint, SwcPoint p) {
  char buf[32];
  u8g2.clearBuffer();
  u8g2.drawBox(0, 0, W, BAR_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_SMALL);
  snprintf(buf, sizeof(buf), "APPRENTISSAGE %d/%d", index + 1, NUM_ACTIONS);
  u8g2.drawStr(3, 8, buf);
  u8g2.setDrawColor(1);
  u8g2.setFont(FONT_ARTIST);
  u8g2.drawUTF8((W - u8g2.getUTF8Width(hint)) / 2, 23, hint);
  u8g2.setFont(FONT_TITLE);
  u8g2.drawUTF8((W - u8g2.getUTF8Width(ACTION_NAMES[index])) / 2, 38, ACTION_NAMES[index]);
  u8g2.setFont(FONT_SMALL);
  snprintf(buf, sizeof(buf), "A %4u   B %4u", p.a, p.b);
  u8g2.drawStr((W - u8g2.getStrWidth(buf)) / 2, 58, buf);
  u8g2.sendBuffer();
}

// Waits until a button reading stays steady for 400 ms and returns its average
SwcPoint waitSteadyPress(int index) {
  SwcPoint ref = {0, 0};
  unsigned long since = 0;
  uint32_t sumA = 0, sumB = 0, n = 0;
  while (true) {
    SwcPoint p = readSwc();
    drawLearn(index, "Appuyez et tenez", p);
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
      if (millis() - since >= 400) return {(uint16_t)(sumA / n), (uint16_t)(sumB / n)};
    }
    delay(10);
  }
}

void waitRelease(int index, SwcPoint learned) {
  unsigned long idleSince = 0;
  while (idleSince == 0 || millis() - idleSince < 200) {
    SwcPoint p = readSwc();
    drawLearn(index, "OK - relâchez", p);
    if (!swcIdle(p)) idleSince = 0;
    else if (idleSince == 0) idleSince = millis();
    delay(10);
  }
}

void learnSteering() {
  Serial.println("\n=== Steering wheel learn mode: press and hold each button when asked ===");
  SwcPoint learned[NUM_ACTIONS];
  for (int i = 0; i < NUM_ACTIONS; i++) {
    learned[i] = waitSteadyPress(i);
    Serial.printf("%-14s A %4u  B %4u\n", ACTION_NAMES[i], learned[i].a, learned[i].b);
    for (int k = 0; k < i; k++)
      if (swcDistance(learned[i], learned[k]) < SWC_TOO_CLOSE)
        Serial.printf("  WARNING: too close to %s (distance %d). Check the resistor mod on this button.\n",
                      ACTION_NAMES[k], swcDistance(learned[i], learned[k]));
    waitRelease(i, learned[i]);
  }
  memcpy(swcTable, learned, sizeof(swcTable));
  saveSteering();
  swcReady = true;
  Serial.println("Steering wheel buttons saved");
  showToast("BOUTONS ENREGISTRÉS");
}

// Test keys from the serial monitor
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 's') {
      source = (Source)((source + 1) % SRC_COUNT);
      Serial.printf("Source: %s\n", SOURCE_NAMES[source]);
    } else if (c == 'l') {
      loud = !loud;
      Serial.printf("Loudness: %s\n", loud ? "on" : "off");
    } else if (c == 'L') {
      learnSteering();
    } else if (c == 'w') {
      swcRawLog = !swcRawLog;
    } else if (c == 'c') {
      swcPrefs.begin("swc", false);
      swcPrefs.clear();
      swcPrefs.end();
      swcReady = false;
      Serial.println("Steering wheel buttons cleared (send 'L' to learn again)");
    }
  }
}

void loop() {
  static unsigned long lastFrame = 0;
  handleSerial();
  pollSteering();
  if (millis() - lastFrame >= 25) {  // 40 FPS for smooth scrolling
    lastFrame = millis();
    render();
    logChanges();
  }
  delay(1);
}
