#pragma once
// ---------------------------------------------------------------------
//  Compresseur stereo lie, genou doux, gain de compensation automatique
// ---------------------------------------------------------------------
#include "Effect.h"

class CompressorEffect : public Effect {
 public:
  enum { P_THR, P_RATIO, P_ATT, P_REL };

  CompressorEffect() : Effect("Compressor", gfx::rgb(255, 200, 120)) {
    addParam("Seuil", "dB", -40.f, 0.f, -18.f);
    addParam("Ratio", ":1", 1.f, 20.f, 4.f, ParamType::Log);
    addParam("Attaque", "ms", 0.1f, 100.f, 5.f, ParamType::Log);
    addParam("Relache", "ms", 10.f, 1000.f, 120.f, ParamType::Log);
  }

  void reset() override { env_ = 0; grDb_ = 0; levelDb_ = -90; }

  float gainComputer(float inDb) const {
    const float thr = p(P_THR), ratio = p(P_RATIO), knee = 6.f;
    const float over = inDb - thr;
    if (over <= -knee / 2) return inDb;
    if (over >= knee / 2) return thr + over / ratio;
    const float k = over + knee / 2;
    return inDb + (1.f / ratio - 1.f) * k * k / (2.f * knee);
  }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    // courbe entree/sortie en dB (-60..0) + vumetre de reduction de gain
    const int s = h;
    g.drawRect(x, y, s, s, gfx::theme::GRID);
    g.drawLine(x, y + s - 1, x + s - 1, y, gfx::theme::GRID);
    gfx::plotFn(g, x, y, s, s, color(), [&](float u) {
      return (gainComputer(-60.f + 60.f * u) + 60.f) / 60.f;
    }, false, 0, true);
    // point de fonctionnement courant
    const float lv = dsp::clampf(levelDb_, -60.f, 0.f);
    const int px = x + (int)((lv + 60.f) / 60.f * (s - 1));
    const int py = y + (int)((1.f - (gainComputer(lv) + 60.f) / 60.f) * (s - 1));
    g.fillCircle(px, py, 2, gfx::theme::TEXT);

    const int mx = x + s + 8, mw = w - s - 10;
    g.setTextFont(1);
    g.setTextDatum(TL_DATUM);
    g.setTextColor(gfx::theme::DIM);
    g.drawString("Reduction", mx, y + 2);
    const float gr = dsp::clampf(-grDb_, 0.f, 20.f) / 20.f;
    g.drawRect(mx, y + 13, mw, 9, gfx::theme::GRID);
    g.fillRect(mx + 1, y + 14, (int)((mw - 2) * gr), 7, gfx::theme::WARN);
    char buf[16];
    snprintf(buf, sizeof(buf), "-%.1f dB", -grDb_ < 0 ? 0.f : -grDb_);
    g.setTextColor(gfx::theme::TEXT);
    g.drawString(buf, mx, y + 26);
    snprintf(buf, sizeof(buf), "Gain +%.1f", makeupDb());
    g.setTextColor(gfx::theme::DIM);
    g.drawString(buf, mx, y + 37);
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    const float k = 0.3f + 0.2f * sinf(t * 2.f);
    gfx::plotFn(g, cx - 6, cy - 6, 13, 13, col,
                [&](float u) { return u < 0.5f ? u : 0.5f + (u - 0.5f) * k; });
  }

 protected:
  float makeupDb() const { return -p(P_THR) * (1.f - 1.f / p(P_RATIO)) * 0.5f; }

  void onParamsChanged() override {
    att_ = dsp::onePoleCoef(p(P_ATT), fs_);
    rel_ = dsp::onePoleCoef(p(P_REL), fs_);
    makeup_ = dsp::dbToGain(makeupDb());
  }

  void process(float* L, float* R, int n) override {
    float env = env_, peak = -120.f;
    for (int i = 0; i < n; ++i) {
      const float a = fmaxf(fabsf(L[i]), fabsf(R[i]));
      const float inDb = dsp::gainToDb(a);
      const float gr = gainComputer(inDb) - inDb;  // <= 0
      // attaque quand la reduction augmente, relache sinon
      env = gr < env ? gr + (env - gr) * att_ : gr + (env - gr) * rel_;
      const float g = dsp::dbToGain(env) * makeup_;
      L[i] *= g;
      R[i] *= g;
      if (inDb > peak) peak = inDb;
    }
    env_ = env;
    grDb_ = env;
    levelDb_ = peak > levelDb_ ? peak : levelDb_ * 0.9f + peak * 0.1f;
  }

  float env_ = 0, att_ = 0, rel_ = 0, makeup_ = 1;
  volatile float grDb_ = 0, levelDb_ = -90;
};
