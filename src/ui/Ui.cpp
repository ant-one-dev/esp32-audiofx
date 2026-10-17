#include "Ui.h"
#include "Screens.h"
#include "audio/AudioEngine.h"
#include "audio/Bluetooth.h"
#include "effects/MasterEffect.h"

using namespace gfx;

Ui ui;

void Ui::begin() {
  tft_.init();
  tft_.setRotation(1);  // paysage 160x128
  tft_.fillScreen(TFT_BLACK);

  // sprite plein ecran en RAM interne (plus rapide que la PSRAM)
  canvas_.setColorDepth(16);
  canvas_.setAttribute(PSRAM_ENABLE, false);
  if (!canvas_.createSprite(cfg::SCREEN_W, cfg::SCREEN_H)) {
    canvas_.setAttribute(PSRAM_ENABLE, true);
    canvas_.createSprite(cfg::SCREEN_W, cfg::SCREEN_H);
  }

  input.begin();
  spectrum.begin();
  // RETOUR maintenu au demarrage : ecran de test des boutons
  Screen* first = digitalRead(cfg::PIN_BTN_BACK) == LOW ? (Screen*)&buttonTestScreen
                                                       : (Screen*)&splashScreen;
  stack_[0] = first;
  depth_ = 1;
  first->enter();
}

void Ui::start() {
  xTaskCreatePinnedToCore(taskEntry, "ui", 8192, this, 1, nullptr, cfg::UI_CORE);
}

void Ui::taskEntry(void* arg) { static_cast<Ui*>(arg)->loop(); }

void Ui::loop() {
  uint32_t last = micros();
  for (;;) {
    const uint32_t now = micros();
    const float dt = (now - last) * 1e-6f;
    last = now;
    frame(dt > 0.1f ? 0.1f : dt);
    fps = fps * 0.9f + 0.1f / (dt > 0.001f ? dt : 0.001f);
    // ~40 images/s max, et on laisse toujours la main aux autres taches du coeur 0
    const uint32_t spent = (micros() - now) / 1000;
    vTaskDelay(pdMS_TO_TICKS(spent < 24 ? 25 - spent : 1));
  }
}

// ----------------------------------------------------------------- navigation
void Ui::startTransition(int dir) {
  dir_ = dir;
  trans_ = 0.f;
  input.syncPots();  // evite les sauts de parametre en changeant d'ecran
}

void Ui::push(Screen* s) {
  if (depth_ >= MAX_DEPTH) return;
  from_ = current();
  stack_[depth_++] = s;
  s->enter();
  startTransition(1);
}

void Ui::pop() {
  if (depth_ <= 1) return;
  from_ = current();
  --depth_;
  startTransition(-1);
}

void Ui::replace(Screen* s, int dir) {
  from_ = current();
  stack_[depth_ - 1] = s;
  s->enter();
  startTransition(dir);
}

void Ui::toast(const char* msg, uint16_t col) {
  strlcpy(toastMsg_, msg, sizeof(toastMsg_));
  toastCol_ = col;
  toastT_ = 1.6f;
}

// ----------------------------------------------------------------- image
void Ui::frame(float dt) {
  time += dt;
  input.update();
  spectrum.update(dt);

  Screen* cur = current();
  // potentiometre 1 = volume general sur les ecrans qui n'utilisent pas les pots
  float v;
  if (!cur->usesPots() && cur->showStatus() && input.potMoved(0, v)) {
    audio.master().setParamNorm(MasterEffect::P_VOL, v);
    volT_ = 1.5f;
  }
  cur->update(dt);
  cur = current();  // l'ecran a pu changer

  Canvas& g = canvas_;
  g.fillSprite(theme::BG);
  if (trans_ < 1.f) {
    trans_ += dt / 0.28f;
    if (trans_ > 1.f) trans_ = 1.f;
    const float e = easeInOutCubic(trans_);
    const int W = cfg::SCREEN_W;
    if (from_) from_->draw(g, (int)(-dir_ * e * W));
    cur->draw(g, (int)(dir_ * (1.f - e) * W));
  } else {
    cur->draw(g, 0);
  }
  if (cur->showStatus()) drawStatus(g);
  drawOverlays(g, dt);
  g.pushSprite(0, 0);
}

void Ui::drawStatus(Canvas& g) {
  const int H = cfg::STATUS_H;
  vGradient(g, 0, 0, cfg::SCREEN_W, H, theme::BG2, theme::BG);
  const uint16_t acc = current()->accent();
  hGradient(g, 0, H - 1, cfg::SCREEN_W, 1, theme::BG, acc);

  // icone Bluetooth : clignote en attente, bleue connectee, verte en lecture
  const bt::State s = bt::state();
  uint16_t bc = theme::DIM;
  if (s == bt::State::Waiting) bc = fmodf(time, 1.2f) < 0.6f ? theme::DIM : theme::GRID;
  else if (s == bt::State::Connecting) bc = theme::WARN;
  else if (s == bt::State::Connected) bc = theme::ACCENT;
  else bc = theme::OK;
  btIcon(g, 3, 1, bc);
  if (s == bt::State::Playing) {  // mini egaliseur anime
    const float* b = spectrum.bands();
    for (int i = 0; i < 3; ++i) {
      const int hh = 1 + (int)(b[4 + i * 12] * 7.f);
      g.fillRect(11 + i * 3, 9 - hh, 2, hh, theme::OK);
    }
  }

  g.setTextFont(1);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(theme::TEXT);
  g.drawString(current()->title(), cfg::SCREEN_W / 2, H / 2);

  // charge CPU de la tache audio
  char buf[8];
  const float load = audio.cpuLoad();
  snprintf(buf, sizeof(buf), "%d%%", (int)(load * 100.f));
  g.setTextDatum(MR_DATUM);
  g.setTextColor(load < 0.6f ? theme::DIM : (load < 0.85f ? theme::WARN : theme::BAD));
  g.drawString(buf, cfg::SCREEN_W - 2, H / 2);
}

void Ui::drawOverlays(Canvas& g, float dt) {
  // ---- popup de volume (glisse depuis le bas) ----
  if (volT_ > 0.f) {
    volT_ -= dt;
    const float a = easeOutCubic(fminf(1.f, fminf(volT_ * 4.f, (1.5f - volT_) * 6.f)));
    const int h = 20, y = cfg::SCREEN_H - (int)(a * (h + 4));
    g.fillRoundRect(10, y, 140, h, 6, theme::PANEL);
    g.drawRoundRect(10, y, 140, h, 6, theme::ACCENT);
    const Param& vp = audio.master().param(MasterEffect::P_VOL);
    const float n = vp.norm();
    hGradient(g, 50, y + 7, (int)(90 * n), 6, theme::ACCENT, theme::ACCENT2);
    g.drawRect(50, y + 6, 92, 8, theme::GRID);
    char buf[12];
    vp.format(buf, sizeof(buf));
    g.setTextFont(1);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(theme::TEXT);
    g.drawString(buf, 16, y + h / 2);
  }
  // ---- notification (descend sous la barre d'etat) ----
  if (toastT_ > 0.f) {
    toastT_ -= dt;
    const float a = easeOutBack(fminf(1.f, fminf(toastT_ * 4.f, (1.6f - toastT_) * 5.f)));
    g.setTextFont(1);
    const int w = g.textWidth(toastMsg_) + 16;
    const int x = (cfg::SCREEN_W - w) / 2, y = -18 + (int)(a * 32.f);
    g.fillRoundRect(x, y, w, 16, 8, scale(toastCol_, 0.3f));
    g.drawRoundRect(x, y, w, 16, 8, toastCol_);
    g.setTextDatum(MC_DATUM);
    g.setTextColor(theme::TEXT);
    g.drawString(toastMsg_, cfg::SCREEN_W / 2, y + 8);
  }
}
