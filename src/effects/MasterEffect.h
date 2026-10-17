#pragma once
// ---------------------------------------------------------------------
//  Master : volume, balance, largeur stereo, limiteur. Toujours actif,
//  toujours en fin de chaine.
// ---------------------------------------------------------------------
#include "Effect.h"

class MasterEffect : public Effect {
 public:
  enum { P_VOL, P_BAL, P_WIDTH, P_LIMIT };

  MasterEffect() : Effect("Master", gfx::rgb(240, 240, 255)) {
    static const char* const onoff[] = {"Off", "On"};
    addParam("Volume", "%", 0.f, 1.5f, 0.8f);
    addParam("Balance", "", -1.f, 1.f, 0.f);
    addParam("Largeur", "%", 0.f, 2.f, 1.f);
    addChoice("Limiteur", onoff, 2, 1);
    setEnabled(true);
  }

  bool canBypass() const override { return false; }
  void reset() override { limEnv_ = 1.f; }

  float levelL() const { return levelL_; }
  float levelR() const { return levelR_; }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    // vumetres G/D (dB) avec maintien de crete
    const float lv[2] = {levelL_, levelR_};
    const int mw = w * 3 / 5;
    for (int c = 0; c < 2; ++c) {
      const float db = dsp::clampf(dsp::gainToDb(lv[c]), -48.f, 0.f);
      const float v = (db + 48.f) / 48.f;
      if (v > peak_[c]) peak_[c] = v; else peak_[c] -= 0.008f;
      const int by = y + 8 + c * 20;
      g.setTextFont(1);
      g.setTextDatum(ML_DATUM);
      g.setTextColor(gfx::theme::DIM);
      g.drawString(c ? "R" : "L", x, by + 4);
      const int bw = (int)((mw - 10) * v);
      for (int i = 0; i < bw; i += 3) {
        const float u = (float)i / (mw - 10);
        const uint16_t col = u < 0.7f ? gfx::theme::OK : (u < 0.9f ? gfx::theme::WARN : gfx::theme::BAD);
        g.fillRect(x + 9 + i, by, 2, 9, col);
      }
      g.drawFastVLine(x + 9 + (int)((mw - 10) * fmaxf(peak_[c], 0.f)), by - 1, 11, gfx::theme::TEXT);
    }
    // champ stereo : ellipse dont la largeur suit "Largeur", decalee par la balance
    const int cx = x + mw + (w - mw) / 2 + (int)(p(P_BAL) * 10.f), cy = y + h / 2;
    const int rx = 3 + (int)(p(P_WIDTH) * 9.f), ry = 4 + (int)(fmaxf(levelL_, levelR_) * 14.f);
    g.drawEllipse(cx, cy, rx, ry, color());
    g.drawEllipse(cx, cy, rx / 2 + 1, ry / 2 + 1, gfx::scale(color(), 0.5f));
    g.fillCircle(cx, cy, 1, gfx::theme::ACCENT);
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    for (int i = 0; i < 3; ++i) {
      const int hh = 3 + (int)(3.f * (0.5f + 0.5f * sinf(t * 4.f + i)));
      g.fillRect(cx - 5 + i * 4, cy + 6 - hh * 2, 3, hh * 2, col);
    }
  }

 protected:
  void process(float* L, float* R, int n) override {
    const float target = p(P_VOL);
    const float bal = p(P_BAL), width = p(P_WIDTH);
    const float gl = bal > 0 ? 1.f - bal : 1.f, gr = bal < 0 ? 1.f + bal : 1.f;
    const bool limit = choice(P_LIMIT) == 1;
    const float rel = 1.f - 1.f / (0.15f * fs_);
    float vol = vol_, pl = 0, pr = 0;
    for (int i = 0; i < n; ++i) {
      vol += (target - vol) * 0.002f;
      // largeur : traitement Mid/Side
      const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * width;
      float l = (m + s) * gl * vol, r = (m - s) * gr * vol;
      if (limit) {
        // limiteur a crete : attaque instantanee, relache 150 ms, plafond -0.5 dBFS
        const float pk = fmaxf(fabsf(l), fabsf(r));
        const float need = pk > 0.944f ? 0.944f / pk : 1.f;
        limEnv_ = need < limEnv_ ? need : 1.f - (1.f - limEnv_) * rel;
        l *= limEnv_;
        r *= limEnv_;
      }
      L[i] = l;
      R[i] = r;
      pl = fmaxf(pl, fabsf(l));
      pr = fmaxf(pr, fabsf(r));
    }
    vol_ = vol;
    levelL_ = fmaxf(pl, levelL_ * 0.93f);
    levelR_ = fmaxf(pr, levelR_ * 0.93f);
  }

  float vol_ = 0.8f, limEnv_ = 1.f;
  volatile float levelL_ = 0, levelR_ = 0;
  mutable float peak_[2] = {0, 0};
};
