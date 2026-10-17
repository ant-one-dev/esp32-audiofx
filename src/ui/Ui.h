#pragma once
// =====================================================================
//  Interface utilisateur : gestion des ecrans, transitions, overlays
// =====================================================================
#include "Gfx.h"
#include "Input.h"
#include "Spectrum.h"

class Screen {
 public:
  virtual ~Screen() = default;
  virtual const char* title() const = 0;
  virtual uint16_t accent() const { return gfx::theme::ACCENT; }
  virtual void enter() {}                 // a chaque ouverture (push / replace)
  virtual void update(float dt) = 0;      // entrees : seulement l'ecran actif
  virtual void draw(Canvas& g, int ox) = 0;
  virtual bool usesPots() const { return false; }  // sinon pot 1 = volume
  virtual bool showStatus() const { return true; }
};

class Ui {
 public:
  void begin();   // ecran, sprite, premier ecran
  void start();   // lance la tache d'affichage

  // navigation
  void push(Screen* s);
  void pop();
  void replace(Screen* s, int dir);       // dir : +1 vers la gauche, -1 vers la droite
  Screen* current() const { return stack_[depth_ - 1]; }

  void toast(const char* msg, uint16_t col = gfx::theme::ACCENT);

  Input input;
  Spectrum spectrum;
  float time = 0;           // secondes depuis le demarrage
  uint8_t spectrumStyle = Spectrum::CURVE;
  float fps = 0;

 private:
  static void taskEntry(void* arg);
  void loop();
  void frame(float dt);
  void drawStatus(Canvas& g);
  void drawOverlays(Canvas& g, float dt);
  void startTransition(int dir);

  TFT_eSPI tft_;
  Canvas canvas_{&tft_};
  static constexpr int MAX_DEPTH = 8;
  Screen* stack_[MAX_DEPTH] = {};
  int depth_ = 0;
  Screen* from_ = nullptr;
  float trans_ = 1.f;
  int dir_ = 1;

  char toastMsg_[40] = "";
  uint16_t toastCol_ = 0;
  float toastT_ = 0;
  float volT_ = 0;  // affichage du popup de volume
};

extern Ui ui;
