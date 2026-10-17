#pragma once
// =====================================================================
//  Egaliseur 3 bandes : grave (shelf), medium (cloche), aigu (shelf)
// =====================================================================
#include <array>
#include "Effect.h"
#include "dsp/Biquad.h"

class Eq3Effect : public Effect {
 public:
  enum { P_LOW, P_MID, P_MIDF, P_HIGH };

  Eq3Effect() : Effect("EQ 3 Bandes", gfx::rgb(120, 255, 80)) {
    addParam("Grave", "dB", -15.f, 15.f, 0.f);
    addParam("Medium", "dB", -15.f, 15.f, 0.f);
    addParam("Freq", "Hz", 200.f, 5000.f, 1000.f, ParamType::Log);
    addParam("Aigu", "dB", -15.f, 15.f, 0.f);
  }

  void reset() override { for (auto& b : st_) for (auto& s : b) s.reset(); }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    BiquadSet c = design();
    drawResponse(g, x, y, w, h, -18.f, 18.f, [&](float f) {
      return c[0].magnitudeDb(f, fs_) + c[1].magnitudeDb(f, fs_) + c[2].magnitudeDb(f, fs_);
    });
    // trois poignees colorees sur les bandes
    static const uint16_t cols[] = {gfx::rgb(255, 80, 80), gfx::rgb(80, 255, 120),
                                    gfx::rgb(80, 160, 255)};
    const float fr[] = {150.f, p(P_MIDF), 5000.f};
    const float gn[] = {p(P_LOW), p(P_MID), p(P_HIGH)};
    for (int i = 0; i < 3; ++i) {
      const int hx = x + (int)(logf(fr[i] / 20.f) / logf(1000.f) * (w - 1));
      const int hy = y + (int)((1.f - (gn[i] + 18.f) / 36.f) * (h - 1));
      g.fillCircle(hx, hy, 3, cols[i]);
      g.drawCircle(hx, hy, 4, gfx::theme::TEXT);
    }
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    for (int i = 0; i < 3; ++i) {
      const int hgt = 4 + (int)(4.f * (0.5f + 0.5f * sinf(t * 2.f + i * 2.1f)));
      g.drawFastVLine(cx - 5 + i * 5, cy - 6, 13, gfx::scale(col, 0.4f));
      g.fillRect(cx - 6 + i * 5, cy + 6 - hgt, 3, 3, col);
    }
  }

 protected:
  using BiquadSet = std::array<dsp::BiquadCoefs, 3>;
  BiquadSet design() const {
    return {dsp::BiquadCoefs::design(dsp::BiquadType::LowShelf, 150.f, 0.7f, p(P_LOW), fs_),
            dsp::BiquadCoefs::design(dsp::BiquadType::Peak, p(P_MIDF), 0.9f, p(P_MID), fs_),
            dsp::BiquadCoefs::design(dsp::BiquadType::HighShelf, 5000.f, 0.7f, p(P_HIGH), fs_)};
  }
  void onParamsChanged() override { c_ = design(); }

  void process(float* L, float* R, int n) override {
    for (int i = 0; i < n; ++i) {
      float l = L[i], r = R[i];
      for (int b = 0; b < 3; ++b) {
        l = st_[b][0].process(l, c_[b]);
        r = st_[b][1].process(r, c_[b]);
      }
      L[i] = l;
      R[i] = r;
    }
  }

  BiquadSet c_;
  dsp::Biquad st_[3][2];
};
