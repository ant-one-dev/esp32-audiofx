#pragma once
// ---------------------------------------------------------------------
//  Modulateur en anneau : multiplication par une porteuse
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/Lfo.h"

class RingModEffect : public Effect {
 public:
  enum { P_FREQ, P_SHAPE, P_MIX };

  RingModEffect() : Effect("Ring Mod", gfx::rgb(0, 255, 255)) {
    addParam("Freq", "Hz", 20.f, 3000.f, 440.f, ParamType::Log);
    addChoice("Forme", dsp::kLfoShapeNames, 3, 0);
    addParam("Mix", "%", 0.f, 1.f, 0.7f);
  }

  void reset() override { osc_.reset(); lfo_.phase = 0; }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const override {
    drawGrid(g, x, y, w, h, false);
    const auto shape = (dsp::LfoShape)choice(P_SHAPE);
    const float k = 2.f + p(P_FREQ) / 150.f;  // nombre de periodes visibles
    const float ph = ctx.t * 0.2f;
    gfx::plotFn(g, x, y, w, h, gfx::scale(gfx::theme::DIM, 0.6f),
                [&](float u) { return 0.5f + 0.45f * sinf(dsp::TWO_PI_F * (u + ph)); });
    gfx::plotFn(g, x, y, w, h, color(), [&](float u) {
      float c = u * k + ph * 3.f; c -= floorf(c);
      return 0.5f + 0.45f * sinf(dsp::TWO_PI_F * (u + ph)) * dsp::lfoWave(shape, c);
    }, false, 0, true);
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    g.drawCircle(cx, cy, 6, col);
    g.drawLine(cx - 4, cy - 4, cx + 4, cy + 4, col);
    g.drawLine(cx - 4, cy + 4, cx + 4, cy - 4, col);
    g.fillCircle(cx + (int)(cosf(t * 3.f) * 6), cy + (int)(sinf(t * 3.f) * 6), 1, gfx::theme::TEXT);
  }

 protected:
  void process(float* L, float* R, int n) override {
    const auto shape = (dsp::LfoShape)choice(P_SHAPE);
    const float mix = p(P_MIX);
    osc_.setRate(p(P_FREQ), fs_);
    lfo_.setRate(p(P_FREQ), fs_);
    for (int i = 0; i < n; ++i) {
      osc_.step();
      lfo_.advance();
      const float c = shape == dsp::LfoShape::Sine ? osc_.s : lfo_.value(shape);
      L[i] += (L[i] * c - L[i]) * mix;
      R[i] += (R[i] * c - R[i]) * mix;
    }
    osc_.normalize();
  }

  dsp::QuadOsc osc_;
  dsp::Lfo lfo_;
};
