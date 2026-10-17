#pragma once
// Boutons (anti-rebond, appui long, repetition) et potentiometres (filtres)
#include <Arduino.h>
#include "Config.h"

enum Btn : uint8_t { BTN_UP, BTN_DOWN, BTN_OK, BTN_BACK, BTN_COUNT };

class Input {
 public:
  void begin();
  void update();  // a appeler une fois par image

  // UP/DOWN : evenement a l'appui puis repetition auto si maintenu
  // OK/BACK : evenement au relachement si appui court
  bool pressed(Btn b) const { return pressed_[b]; }
  bool longPress(Btn b) const { return long_[b]; }  // une seule fois apres 600 ms
  bool held(Btn b) const { return down_[b]; }
  bool anyActivity() const { return activity_; }

  float pot(int i) const { return pot_[i]; }  // 0..1, filtre
  bool potMoved(int i, float& value);          // vrai si le pot a bouge sensiblement
  void syncPots();                             // la position actuelle devient la reference

 private:
  uint8_t pins_[BTN_COUNT] = {};
  bool down_[BTN_COUNT] = {}, raw_[BTN_COUNT] = {};
  bool pressed_[BTN_COUNT] = {}, long_[BTN_COUNT] = {}, longFired_[BTN_COUNT] = {};
  uint32_t changeMs_[BTN_COUNT] = {}, downMs_[BTN_COUNT] = {}, repeatMs_[BTN_COUNT] = {};
  float pot_[cfg::NUM_POTS] = {};
  float potRef_[cfg::NUM_POTS] = {};
  bool activity_ = false;
};
