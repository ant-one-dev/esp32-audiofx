#pragma once
// =====================================================================
//  Classe parent de tous les effets
//
//  Un effet regroupe :
//   - le traitement audio  : process() (appele dans la tache audio)
//   - ses parametres       : jusqu'a 4, pilotes par les 4 potentiometres
//   - son affichage        : icone animee, visualisation, page complete
//
//  Pour creer un effet, il suffit de deriver de Effect, de declarer les
//  parametres dans le constructeur avec addParam(), d'implementer
//  process() et, si besoin, onParamsChanged(), drawIcon() et drawVisual().
// =====================================================================
#include <Arduino.h>
#include "Config.h"
#include "ui/Gfx.h"
#include "dsp/DspUtil.h"

enum class ParamType : uint8_t { Linear, Log, Int, Choice };

struct Param {
  const char* name = "";
  const char* unit = "";
  float min = 0, max = 1, def = 0;
  ParamType type = ParamType::Linear;
  const char* const* choices = nullptr;
  uint8_t numChoices = 0;
  volatile float value = 0;

  float norm() const;                 // valeur -> [0,1]
  float fromNorm(float n) const;      // [0,1] -> valeur
  int choice() const { return (int)(value + 0.5f); }
  void format(char* buf, size_t len) const;
};

// Informations transmises par l'ecran pour dessiner la page d'un effet
struct PageCtx {
  int ox = 0;                          // decalage horizontal (transitions)
  float t = 0;                         // temps en secondes (animations)
  float enableAnim = 0;                // animation de l'interrupteur 0..1
  bool potActive[cfg::NUM_POTS] = {};  // potentiometre "accroche" au parametre
  float potPos[cfg::NUM_POTS] = {};    // position physique des potentiometres
  float glow[cfg::NUM_POTS] = {};      // surbrillance des parametres modifies
  const float* spectrum = nullptr;     // spectre de sortie (fond de la visu)
  int numBands = 0;
};

class Effect {
 public:
  static constexpr int MAX_PARAMS = cfg::NUM_POTS;

  Effect(const char* name, uint16_t color);
  virtual ~Effect() = default;

  // ------------------------------------------------------------- audio
  // Appele avant le demarrage et a chaque changement de frequence d'echantillonnage
  virtual void prepare(float fs);
  // Traitement d'un bloc (gere le bypass progressif et la prise en compte des parametres)
  void run(float* L, float* R, int n);
  // Vide les buffers internes (lignes a retard, etats de filtres...)
  virtual void reset() {}
  // Faux pour les effets toujours actifs (Master)
  virtual bool canBypass() const { return true; }

  // ------------------------------------------------------------- etat
  const char* name() const { return name_; }
  uint16_t color() const { return color_; }
  bool enabled() const { return enabled_; }
  void setEnabled(bool on) { enabled_ = on; }
  void toggle() { enabled_ = !enabled_; }
  bool isActive() const { return enabled_ || mix_ > 0.f; }

  // ------------------------------------------------------------- parametres
  int numParams() const { return numParams_; }
  const Param& param(int i) const { return params_[i]; }
  void setParam(int i, float value);
  void setParamNorm(int i, float norm);
  void resetParams();

  // ------------------------------------------------------------- affichage
  // Icone animee centree en (cx, cy), environ 14x14 pixels
  virtual void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const;
  // Zone de visualisation de l'effet (courbe de reponse, LFO, etc.)
  virtual void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const;
  // Page complete : en-tete, visualisation, boutons rotatifs
  virtual void drawPage(Canvas& g, const PageCtx& ctx) const;

 protected:
  int addParam(const char* name, const char* unit, float min, float max, float def,
               ParamType type = ParamType::Linear);
  int addChoice(const char* name, const char* const* choices, uint8_t count, uint8_t def = 0);

  virtual void process(float* L, float* R, int n) = 0;
  // Recalcul des coefficients (toujours appele depuis la tache audio)
  virtual void onParamsChanged() {}

  float p(int i) const { return params_[i].value; }
  int choice(int i) const { return params_[i].choice(); }

  // --- aides pour les visualisations ---
  // Trace une courbe de reponse en frequence (dB) sur echelle log 20 Hz - 20 kHz
  void drawResponse(Canvas& g, int x, int y, int w, int h, float minDb, float maxDb,
                    const std::function<float(float)>& dbAt) const;
  // Trace une forme d'onde periodique avec un curseur de phase
  void drawLfo(Canvas& g, int x, int y, int w, int h, const std::function<float(float)>& wave,
               float phase, float depth) const;
  // Trace une courbe de transfert entree -> sortie
  void drawTransfer(Canvas& g, int x, int y, int w, int h,
                    const std::function<float(float)>& tf) const;
  void drawGrid(Canvas& g, int x, int y, int w, int h, bool logFreq) const;
  static void drawSpectrumGhost(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx,
                                uint16_t col);

  float fs_ = cfg::DEFAULT_FS;

 private:
  const char* name_;
  uint16_t color_;
  Param params_[MAX_PARAMS];
  int numParams_ = 0;
  volatile bool enabled_ = false;
  float mix_ = 0.f;                     // 0 = bypass total, 1 = effet seul
  volatile uint32_t version_ = 1;       // incremente a chaque modif de parametre
  uint32_t seenVersion_ = 0;
};
