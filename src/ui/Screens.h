#pragma once
// Tous les ecrans de l'interface
#include "Ui.h"
#include "effects/Effect.h"

// Animation de demarrage
class SplashScreen : public Screen {
 public:
  const char* title() const override { return ""; }
  bool showStatus() const override { return false; }
  void enter() override { t_ = 0; }
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  float t_ = 0;
};

// Ecran principal : spectre de sortie + morceau en cours
class HomeScreen : public Screen {
 public:
  const char* title() const override { return "Spectre"; }
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  float scroll_ = 0;
  bool bypassAll_ = false;
  Effect* saved_[32] = {};
  int numSaved_ = 0;
};

// Menu principal en carrousel
class MenuScreen : public Screen {
 public:
  const char* title() const override { return "Menu"; }
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  int sel_ = 0;
  float pos_ = 0;
  float bump_ = 0;
};

// Liste des effets (ordre = ordre de la chaine)
class EffectListScreen : public Screen {
 public:
  const char* title() const override { return "Effets"; }
  void enter() override;
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
  void select(int i) { sel_ = i; }
 private:
  int sel_ = 0;
  float hl_ = 0, scroll_ = 0;
  float onAnim_[32] = {};
};

// Visualisation / reordonnancement de la chaine active
class ChainScreen : public Screen {
 public:
  const char* title() const override { return "Chaine"; }
  void enter() override;
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  int collect(Effect** out) const;
  int sel_ = 0;
  float pos_ = 0;
  bool grab_ = false;
  float grabAnim_ = 0;
};

// Page d'un effet : 4 potentiometres -> 4 parametres
class EffectScreen : public Screen {
 public:
  void setEffect(Effect* e) { fx_ = e; }
  Effect* effect() const { return fx_; }
  const char* title() const override { return fx_ ? fx_->name() : ""; }
  uint16_t accent() const override { return fx_ ? fx_->color() : gfx::theme::ACCENT; }
  bool usesPots() const override { return true; }
  void enter() override;
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  Effect* fx_ = nullptr;
  PageCtx ctx_;
};

// Informations systeme
class InfoScreen : public Screen {
 public:
  const char* title() const override { return "Infos"; }
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  float cpu_ = 0;
};

// Test materiel : etat brut des broches de boutons (RETOUR maintenu au demarrage)
class ButtonTestScreen : public Screen {
 public:
  static constexpr int N = 6;
  const char* title() const override { return "Test boutons"; }
  uint16_t accent() const override { return gfx::theme::OK; }
  void enter() override;
  void update(float dt) override;
  void draw(Canvas& g, int ox) override;
 private:
  bool low_[N] = {};
  uint16_t count_[N] = {};
  float flash_[N] = {};
};

extern SplashScreen splashScreen;
extern HomeScreen homeScreen;
extern MenuScreen menuScreen;
extern EffectListScreen effectListScreen;
extern ChainScreen chainScreen;
extern InfoScreen infoScreen;
extern ButtonTestScreen buttonTestScreen;

// Ouvre la page d'un effet (dir = 0 : push, sinon remplacement avec glissement)
void openEffect(Effect* e, int dir = 0);
