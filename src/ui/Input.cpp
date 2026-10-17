#include "Input.h"

namespace {
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr uint32_t LONG_MS = 600;
constexpr uint32_t REPEAT_DELAY_MS = 400;
constexpr uint32_t REPEAT_MS = 110;
constexpr float POT_THRESHOLD = 0.012f;

float readPot(int pin) {
  uint32_t acc = 0;
  for (int i = 0; i < 4; ++i) acc += analogRead(pin);
  float v = acc / (4.f * 4095.f);
  // les extremites de l'ADC de l'ESP32 sont peu lineaires : petite zone morte
  v = (v - 0.02f) / 0.96f;
  return v < 0 ? 0 : (v > 1 ? 1 : v);
}
}  // namespace

void Input::begin() {
  const int pins[BTN_COUNT] = {cfg::PIN_BTN_UP, cfg::PIN_BTN_DOWN, cfg::PIN_BTN_OK,
                               cfg::PIN_BTN_BACK};
  for (int i = 0; i < BTN_COUNT; ++i) {
    pins_[i] = pins[i];
    pinMode(pins[i], INPUT_PULLUP);
  }
  analogReadResolution(12);
  for (int i = 0; i < cfg::NUM_POTS; ++i) {
    analogSetPinAttenuation(cfg::PIN_POT[i], ADC_11db);
    pot_[i] = potRef_[i] = readPot(cfg::PIN_POT[i]);
  }
}

void Input::update() {
  const uint32_t now = millis();
  activity_ = false;
  for (int i = 0; i < BTN_COUNT; ++i) {
    pressed_[i] = long_[i] = false;
    const bool r = digitalRead(pins_[i]) == LOW;
    if (r != raw_[i]) { raw_[i] = r; changeMs_[i] = now; }
    if (now - changeMs_[i] < DEBOUNCE_MS || r == down_[i]) {
      // pas de changement stable : gestion du maintien
      if (down_[i]) {
        const uint32_t heldMs = now - downMs_[i];
        if (i == BTN_UP || i == BTN_DOWN) {
          if (heldMs > REPEAT_DELAY_MS && now - repeatMs_[i] > REPEAT_MS) {
            pressed_[i] = true;
            repeatMs_[i] = now;
          }
        }
        if (!longFired_[i] && heldMs > LONG_MS) { long_[i] = true; longFired_[i] = true; }
      }
      continue;
    }
    down_[i] = r;
    activity_ = true;
    if (r) {
      downMs_[i] = repeatMs_[i] = now;
      longFired_[i] = false;
      if (i == BTN_UP || i == BTN_DOWN) pressed_[i] = true;
    } else if (!longFired_[i] && (i == BTN_OK || i == BTN_BACK)) {
      pressed_[i] = true;
    }
  }

  for (int i = 0; i < cfg::NUM_POTS; ++i) {
    const float v = readPot(cfg::PIN_POT[i]);
    float f = pot_[i] + (v - pot_[i]) * 0.35f;
    if (f < 0.002f) f = 0.f;  // permet d'atteindre exactement les butees
    if (f > 0.998f) f = 1.f;
    pot_[i] = f;
  }
}

bool Input::potMoved(int i, float& value) {
  value = pot_[i];
  if (fabsf(pot_[i] - potRef_[i]) > POT_THRESHOLD ||
      (pot_[i] <= 0.001f && potRef_[i] > 0.f) || (pot_[i] >= 0.999f && potRef_[i] < 1.f)) {
    potRef_[i] = pot_[i];
    activity_ = true;
    return true;
  }
  return false;
}

void Input::syncPots() {
  for (int i = 0; i < cfg::NUM_POTS; ++i) potRef_[i] = pot_[i];
}
