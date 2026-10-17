#pragma once
// ---------------------------------------------------------------------
//  Bitcrusher : reduction de resolution et de frequence d'echantillonnage
// ---------------------------------------------------------------------
#include "Effect.h"

class BitcrusherEffect : public Effect {
 public:
  enum { P_BITS, P_DOWN, P_MIX };

  BitcrusherEffect() : Effect("Bitcrusher", gfx::rgb(200, 255, 0)) {
    addParam("Bits", "b", 1.f, 16.f, 6.f, ParamType::Int);
    addParam("Reduc", "x", 1.f, 32.f, 4.f, ParamType::Int);
    addParam("Mix", "%", 0.f, 1.f, 1.f);
  }

  void reset() override { hold_[0] = hold_[1] = 0; cnt_ = 0; }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const override {
    drawGrid(g, x, y, w, h, false);
    const float levels = exp2f(p(P_BITS) - 1.f);
    const int ds = (int)p(P_DOWN);
    const float ph = ctx.t * 0.3f;
    gfx::plotFn(g, x, y, w, h, gfx::scale(gfx::theme::DIM, 0.6f),
                [&](float u) { return 0.5f + 0.45f * sinf(dsp::TWO_PI_F * (u * 2.f + ph)); });
    // signal "crushe" : echantillonne-bloque puis quantifie
    const int step = 1 + ds / 2;
    gfx::plotFn(g, x, y, w, h, color(), [&](float u) {
      const float uq = floorf(u * w / step) * step / w;
      float v = sinf(dsp::TWO_PI_F * (uq * 2.f + ph));
      v = roundf(v * levels) / levels;
      return 0.5f + 0.45f * v;
    }, false, 0, true);
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    for (int i = 0; i < 5; ++i) {
      const int hh = (int)roundf(3.f * sinf(i * 1.2f + t * 2.f));
      g.fillRect(cx - 7 + i * 3, cy - hh - 1, 3, 2, col);
    }
  }

 protected:
  void process(float* L, float* R, int n) override {
    const float levels = exp2f(p(P_BITS) - 1.f);
    const float inv = 1.f / levels;
    const int ds = (int)p(P_DOWN);
    const float mix = p(P_MIX);
    for (int i = 0; i < n; ++i) {
      if (++cnt_ >= ds) {
        cnt_ = 0;
        hold_[0] = roundf(L[i] * levels) * inv;
        hold_[1] = roundf(R[i] * levels) * inv;
      }
      L[i] += (hold_[0] - L[i]) * mix;
      R[i] += (hold_[1] - R[i]) * mix;
    }
  }

  float hold_[2] = {0, 0};
  int cnt_ = 0;
};
