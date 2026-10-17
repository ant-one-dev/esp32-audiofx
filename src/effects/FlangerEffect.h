#pragma once
// ---------------------------------------------------------------------
//  Flanger : retard court (0.3 - 5 ms) module, avec reinjection
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/DelayLine.h"
#include "dsp/Lfo.h"

class FlangerEffect : public Effect {
 public:
  enum { P_RATE, P_DEPTH, P_FB, P_MIX };

  FlangerEffect() : Effect("Flanger", gfx::rgb(255, 70, 160)) {
    addParam("Vitesse", "Hz", 0.05f, 5.f, 0.25f, ParamType::Log);
    addParam("Profond", "%", 0.f, 1.f, 0.7f);
    addParam("Feedbk", "%", -0.95f, 0.95f, 0.6f);
    addParam("Mix", "%", 0.f, 1.f, 0.5f);
    line_[0].allocate((int)(cfg::MAX_FS * 0.01f));
    line_[1].allocate((int)(cfg::MAX_FS * 0.01f));
  }

  void reset() override { line_[0].clear(); line_[1].clear(); fb_[0] = fb_[1] = 0; osc_.reset(); }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    // filtre en peigne courant : |1 + g e^{-jwD}|
    const float delay = (0.0003f + p(P_DEPTH) * 0.0045f * (0.5f + 0.5f * osc_.s));
    const float fb = p(P_FB);
    drawResponse(g, x, y, w, h, -24.f, 18.f, [&](float f) {
      const float ph = dsp::TWO_PI_F * f * delay;
      const float re = 1.f + cosf(ph) * 0.7f, im = sinf(ph) * 0.7f;
      const float dr = 1.f - fb * cosf(ph), di = fb * sinf(ph);
      return 10.f * log10f((re * re + im * im) / (dr * dr + di * di + 1e-6f) + 1e-6f) - 3.f;
    });
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    const float k = 3.f + 2.f * sinf(t);
    gfx::plotFn(g, cx - 7, cy - 6, 15, 12, col,
                [&](float u) { return 0.5f + 0.45f * cosf(dsp::TWO_PI_F * u * k); });
  }

 protected:
  void process(float* L, float* R, int n) override {
    osc_.setRate(p(P_RATE), fs_);
    const float minD = 0.0003f * fs_, depth = p(P_DEPTH) * 0.0045f * fs_;
    const float fb = p(P_FB), mix = p(P_MIX), dry = 1.f - 0.5f * mix;
    for (int i = 0; i < n; ++i) {
      osc_.step();
      const float dl = minD + depth * (0.5f + 0.5f * osc_.s);
      const float dr = minD + depth * (0.5f + 0.5f * osc_.c);  // canal droit a 90 deg
      line_[0].write(L[i] + fb_[0] * fb);
      line_[1].write(R[i] + fb_[1] * fb);
      const float wl = line_[0].readFrac(dl), wr = line_[1].readFrac(dr);
      fb_[0] = dsp::softClip(wl);
      fb_[1] = dsp::softClip(wr);
      L[i] = L[i] * dry + wl * mix;
      R[i] = R[i] * dry + wr * mix;
    }
    osc_.normalize();
  }

  dsp::DelayLine line_[2];
  dsp::QuadOsc osc_;
  float fb_[2] = {0, 0};
};
