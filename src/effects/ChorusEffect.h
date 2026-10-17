#pragma once
// ---------------------------------------------------------------------
//  Chorus : 2 voix par canal, retard module autour de 14 ms
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/DelayLine.h"
#include "dsp/Lfo.h"

class ChorusEffect : public Effect {
 public:
  enum { P_RATE, P_DEPTH, P_WIDTH, P_MIX };

  ChorusEffect() : Effect("Chorus", gfx::rgb(0, 255, 170)) {
    addParam("Vitesse", "Hz", 0.05f, 5.f, 0.8f, ParamType::Log);
    addParam("Profond", "%", 0.f, 1.f, 0.5f);
    addParam("Largeur", "%", 0.f, 1.f, 0.8f);
    addParam("Mix", "%", 0.f, 1.f, 0.5f);
    line_[0].allocate((int)(cfg::MAX_FS * 0.04f));
    line_[1].allocate((int)(cfg::MAX_FS * 0.04f));
  }

  void reset() override { line_[0].clear(); line_[1].clear(); osc_.reset(); }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    const float ph = osc_.phase, d = p(P_DEPTH);
    // les deux voix (decalees de 90 deg) + la voix du canal droit
    drawLfo(g, x, y, w, h, [](float u) { return sinf(dsp::TWO_PI_F * u); }, ph, d);
    const float amp = (0.1f + 0.9f * d) * 0.42f;
    gfx::plotFn(g, x, y, w, h, gfx::scale(gfx::theme::ACCENT2, 0.8f), [&](float u) {
      return 0.5f + amp * sinf(dsp::TWO_PI_F * (u * 2.f + ph + 0.25f + 0.25f * p(P_WIDTH)));
    });
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    gfx::plotFn(g, cx - 7, cy - 6, 15, 12, col,
                [&](float u) { return 0.5f + 0.4f * sinf(dsp::TWO_PI_F * (u + t * 0.4f)); });
    gfx::plotFn(g, cx - 7, cy - 6, 15, 12, gfx::scale(col, 0.5f),
                [&](float u) { return 0.5f + 0.4f * sinf(dsp::TWO_PI_F * (u + t * 0.4f + 0.3f)); });
  }

 protected:
  void process(float* L, float* R, int n) override {
    osc_.setRate(p(P_RATE), fs_);
    const float base = 0.014f * fs_, depth = p(P_DEPTH) * 0.006f * fs_;
    const float mix = p(P_MIX), dry = 1.f - 0.5f * mix;
    const float wo = dsp::PI_F * (0.5f + 0.5f * p(P_WIDTH));  // dephasage canal droit
    const float co = cosf(wo), so = sinf(wo);
    for (int i = 0; i < n; ++i) {
      osc_.step();
      const float s = osc_.s, c = osc_.c;  // voix 1 : sin, voix 2 : cos (90 deg)
      line_[0].write(L[i]);
      line_[1].write(R[i]);
      const float wl = 0.5f * (line_[0].readFrac(base + depth * s) +
                               line_[0].readFrac(base * 0.8f + depth * c));
      const float sr = osc_.sinOffset(co, so), cr = c * co - s * so;
      const float wr = 0.5f * (line_[1].readFrac(base + depth * sr) +
                               line_[1].readFrac(base * 0.8f + depth * cr));
      L[i] = L[i] * dry + wl * mix;
      R[i] = R[i] * dry + wr * mix;
    }
    osc_.normalize();
  }

  dsp::DelayLine line_[2];
  dsp::QuadOsc osc_;
};
