#include <TM16XXFonts.h>
#include <TM1638.h>

int hh, mm, ss; // current

unsigned long now; // current
unsigned long time; // previous

unsigned long last_step; // last step time
byte step, strobe; // steps 0->9, strobe every 100ms

int h1, m1; // on
int h0, m0; // off
int h_, m_; // temporary

TM1638 ctrl(A1, A2, A3);

byte keymask[] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
byte handled[] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned long updated[] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned long pressed[] = {0, 0, 0, 0, 0, 0, 0, 0};

byte menu;

const byte KEY_MENU = 0;
const byte KEY_MINUS = 1;
const byte KEY_PLUS = 2;
const byte KEY_APPLY = 3;
const byte KEY_TIME_MINUS = 6;
const byte KEY_TIME_PLUS = 7;

const byte ATX_PIN = A0;
byte atx_on;
byte atx_mode, atx_mode_selector; // 0=off, 1=auto, 2=on

void setup() {
  hh = 00;
  mm = 00;

  h1 = 9;
  m1 = 0;
  h0 = 21;
  m0 = 0;

  step = 0;

  menu = 8;

  pinMode(LED_BUILTIN, OUTPUT);

  atx_on = 0;
  atx_mode = 0;
  control_atx();
}

void control_atx() {
  if ((atx_mode == 1 && atx_on) || atx_mode == 2) { // on
    pinMode(ATX_PIN, OUTPUT);
    digitalWrite(ATX_PIN, LOW);
  }
  if ((atx_mode == 1 && !atx_on) || atx_mode == 0) { // off
    digitalWrite(ATX_PIN, HIGH);
    pinMode(ATX_PIN, INPUT);
  }
}

void read_keys() {
  // keys
  byte keys = ctrl.getButtons();
  for (byte i = 0; i < 8; i++) {
    boolean down = ((keys & keymask[i]) == 0) ? false : true;
    if (down) {
      updated[i] = now;
      if (pressed[i] == 0) {
        pressed[i] = now;
      }
    } else {
      if (now - updated[i] > 100) {
        updated[i] = 0;
        pressed[i] = 0;
      }
    }
  }
}

void tick() {
  if (time > now || now - time > 1000) {
    time = millis();
    ss++;
  }
}

void normalize_time() {
  // decrease time
  while (ss < 0) {
    ss += 60;
    mm--;
  }
  while (mm < 0) {
    mm += 60;
    hh--;
  }
  while (hh < 0) {
    hh += 24;
  }

  // increase time
  if (ss >= 60) {
    mm += ss / 60;
    ss = ss % 60;
  }
  if (mm >= 60) {
    hh += mm / 60;
    mm = mm % 60;
  }
  if (hh >= 24) {
    hh = hh % 24;
  }

  // TMP
  // decrease time
  while (m_ < 0) {
    m_ += 60;
    h_--;
  }
  while (h_ < 0) {
    h_ += 24;
  }

  // increase time
  if (m_ >= 60) {
    h_ += m_ / 60;
    m_ = m_ % 60;
  }
  if (h_ >= 24) {
    h_ = h_ % 24;
  }
}

void check_time() {
  int hhmm = hh * 100 + mm;
  int h0m0 = h0 * 100 + m0;
  int h1m1 = h1 * 100 + m1;

  if (h0m0 < h1m1) {
    // on < h0m0 <= off < h1m1 <= on
    atx_on = (hhmm < h0m0 || hhmm >= h1m1);
  }
  
  if (h1m1 < h0m0) {
    // off < h1m1 <= on < h0m0 <= off
    atx_on = (hhmm >= h1m1 && hhmm < h0m0);
  }

}

void handle_keys() {
  for (byte i = 0; i <= 7; i++) {
    if (!pressed[i]) {
      handled[i] = 0;
      continue;
    }

    if (handled[i] == 0) {
      handle_key(i, 0);
      handled[i] = step + 1;
      continue;
    }

    if (!strobe || ((handled[i] & 0x0F) - 1 != step && (handled[i] & 0x0F) - 1 != (step + 5) % 10)) {
      continue;
    }

    byte times = (handled[i] & 0xF0) >> 4;
    handle_key(i, times + 1);

    if (times < 15) {
      handled[i] += 0x10;
      continue;
    }
  }
}

void handle_key(byte num, byte times) {
  if (num == KEY_TIME_PLUS) { // time +
    if (times == 0) mm++;
    if (times >= 1 && times <= 6) mm += 10;
    if (times >= 7) hh++;
  }
  if (num == KEY_TIME_MINUS) { // time -
    if (times == 0) mm--;
    if (times >= 1 && times <= 6) mm -= 10;
    if (times >= 7) hh--;
  }
  if (num == KEY_MENU) { // menu
    menu = (menu + 1) % 9;
    if (menu == 0) { // on-time
      h_ = h1;
      m_ = m1;
    }
    if (menu == 1) { // off-time
      h_ = h0;
      m_ = m0;
    }
    if (menu == 2) { // atx mode
      atx_mode_selector = atx_mode;
    }
    if (menu >= 3) { // skip
      menu = 8;
    }
  }
  if (num == KEY_PLUS && (menu == 0 || menu == 1)) { // on-time +, off-time +
    if (times == 0) m_++;
    if (times >= 1 && times <= 6) m_ += 10;
    if (times >= 7) h_++;
  }
  if (num == KEY_MINUS && (menu == 0 || menu == 1)) { // on-time -, off-time -
    if (times == 0) m_--;
    if (times >= 1 && times <= 6) m_ -= 10;
    if (times >= 7) h_--;
  }
  if (num == KEY_APPLY && menu == 0) { // on-time apply
    h1 = h_;
    m1 = m_;
  }
  if (num == KEY_APPLY && menu == 1) { // off-time apply
    h0 = h_;
    m0 = m_;
  }
  if (num == KEY_PLUS && menu == 2) { // atx mode +
    atx_mode_selector = (atx_mode_selector + 1) % 3;
  }
  if (num == KEY_MINUS && menu == 2) { // atx mode -
    atx_mode_selector = (atx_mode_selector + 3 - 1) % 3;
  }
  if (num == KEY_APPLY && menu == 2) { // atx mode apply
    atx_mode = atx_mode_selector;
  }
}

void show_time() {
  ctrl.setDisplayDigit(hh / 10, 4, false);
  ctrl.setDisplayDigit(hh % 10, 5, true);
  ctrl.setDisplayDigit(mm / 10, 6, false);
  ctrl.setDisplayDigit(mm % 10, 7, ss % 2);
}

byte* map_font(String str, byte* digits, byte dots) {
  char chars[str.length() + 1];
  str.toCharArray(chars, str.length() + 1);
  for (byte i = 0; i < str.length(); i++) {
    char c = chars[i];
    digits[i] = FONT_DEFAULT[c - 32] | ((dots & (1 << i)) ? 0b10000000 : 0);
  }
  return digits;
}

void show_menu() {
  for (byte i = 0; i < 8; i++) {
    ctrl.setLED(menu == i ? 1 : 0, i);
  }
  if (menu == 0 || menu == 1) {
    ctrl.setDisplayDigit(h_ / 10, 0, false);
    ctrl.setDisplayDigit(h_ % 10, 1, true);
    ctrl.setDisplayDigit(m_ / 10, 2, false);
    ctrl.setDisplayDigit(m_ % 10, 3, (menu == 0 && h1 == h_ && m1 == m_) || (menu == 1 && h0 == h_ && m0 == m_));
    return;
  }
  if (menu == 2) {
    byte applied = (atx_mode_selector == atx_mode) ? 8 : 0;
    byte buf[] = {0, 0, 0, 0};
    if (atx_mode_selector == 0) {
      ctrl.setDisplay(map_font(" OFF", buf, applied), 4);
    }
    if (atx_mode_selector == 1) {
      ctrl.setDisplay(map_font("AUTO", buf, applied), 4);
    }
    if (atx_mode_selector == 2) {
      ctrl.setDisplay(map_font("  ON", buf, applied), 4);
    }
    return;
  }
  ctrl.clearDisplayDigit(0, false);
  ctrl.clearDisplayDigit(1, false);
  ctrl.clearDisplayDigit(2, false);
  ctrl.clearDisplayDigit(3, false);
}

void loop() {
  // step
  now = millis();

  strobe = 0;
  if (now - last_step > 100 || now < last_step) {
    last_step = now;
    step = (step + 1) % 10;
    strobe = 1;
  }

  tick();
  read_keys();
  handle_keys();
  normalize_time();

  if (strobe) {
    // blink
    digitalWrite(LED_BUILTIN, ss % 2);

    // display
    ctrl.setupDisplay(true, 7);
    show_menu();
    show_time();

    // control atx
    check_time();
    control_atx();
  }


  // sleep
  delay(10);
}


