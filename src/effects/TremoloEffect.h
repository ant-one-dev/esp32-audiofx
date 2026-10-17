#pragma once
// ---------------------------------------------------------------------
//  Tremolo : modulation d'amplitude (dephasage stereo reglable)
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/Lfo.h"

class TremoloEffect : public Effect {
 public:
  enum { P_RATE, P_DEPTH, P_SHAPE, P_STEREO };

  TremoloEffect() : Effect("Tremolo", gfx::rgb(255, 90, 90)) {
    addParam("Vitesse", "Hz", 0.5f, 15.f, 5.f, ParamType::Log);
    addParam("Profond", "%", 0.f, 1.f, 0.7f);
    addChoice("Forme", dsp::kLfoShapeNames, 3, 0);
    addParam("Stereo", "%", 0.f, 1.f, 0.f);
  }

  void reset() override { gL_ = gR_ = 1.f; }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    const auto shape = (dsp::LfoShape)choice(P_SHAPE);
    drawLfo(g, x, y, w, h, [shape](float u) { u -= floorf(u); return dsp::lfoWave(shape, u); },
            lfo_.phase, p(P_DEPTH));
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    gfx::plotFn(g, cx - 7, cy - 6, 15, 12, col, [&](float u) {
      const float env = 0.5f + 0.5f * sinf(dsp::TWO_PI_F * (u + t * 0.3f));
      return 0.5f + 0.45f * env * sinf(dsp::TWO_PI_F * u * 6.f);
    });
  }

 protected:
  void process(float* L, float* R, int n) override {
    lfo_.setRate(p(P_RATE), fs_);
    const auto shape = (dsp::LfoShape)choice(P_SHAPE);
    const float depth = p(P_DEPTH), off = 0.5f * p(P_STEREO);
    const float smooth = 0.995f;  // adoucit les fronts du carre (pas de clic)
    for (int i = 0; i < n; ++i) {
      lfo_.advance();
      const float tl = 1.f - depth * (0.5f + 0.5f * lfo_.value(shape));
      const float tr = 1.f - depth * (0.5f + 0.5f * lfo_.value(shape, off));
      gL_ = tl + (gL_ - tl) * smooth;
      gR_ = tr + (gR_ - tr) * smooth;
      L[i] *= gL_;
      R[i] *= gR_;
    }
  }

  dsp::Lfo lfo_;
  float gL_ = 1, gR_ = 1;
};
