#pragma once
// ---------------------------------------------------------------------
//  AutoPan : deplacement du son entre gauche et droite (loi a puissance constante)
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/Lfo.h"

class AutoPanEffect : public Effect {
 public:
  enum { P_RATE, P_DEPTH, P_SHAPE };

  AutoPanEffect() : Effect("AutoPan", gfx::rgb(80, 140, 255)) {
    addParam("Vitesse", "Hz", 0.05f, 10.f, 0.5f, ParamType::Log);
    addParam("Profond", "%", 0.f, 1.f, 0.8f);
    addChoice("Forme", dsp::kLfoShapeNames, 3, 0);
  }

  void reset() override { pan_ = 0; }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    // une balle qui se deplace entre les deux haut-parleurs
    const float pan = pan_;
    g.drawFastHLine(x + 10, y + h / 2, w - 20, gfx::theme::GRID);
    g.setTextFont(2);
    g.setTextDatum(MC_DATUM);
    g.setTextColor(gfx::lerpColor(gfx::theme::DIM, gfx::theme::TEXT, 0.5f - 0.5f * pan));
    g.drawString("L", x + 5, y + h / 2);
    g.setTextColor(gfx::lerpColor(gfx::theme::DIM, gfx::theme::TEXT, 0.5f + 0.5f * pan));
    g.drawString("R", x + w - 5, y + h / 2);
    const int bx = x + w / 2 + (int)(pan * (w / 2 - 18));
    for (int k = 3; k >= 1; --k)
      g.fillCircle(bx, y + h / 2, 4 + k * 3, gfx::scale(color(), 0.12f * (4 - k)));
    g.fillCircle(bx, y + h / 2, 5, color());
    g.fillCircle(bx - 1, y + h / 2 - 2, 1, gfx::theme::TEXT);
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    const int bx = cx + (int)(sinf(t * 2.f) * 5.f);
    g.drawFastHLine(cx - 7, cy, 15, gfx::scale(col, 0.4f));
    g.fillCircle(bx, cy, 3, col);
  }

 protected:
  void process(float* L, float* R, int n) override {
    lfo_.setRate(p(P_RATE), fs_);
    const auto shape = (dsp::LfoShape)choice(P_SHAPE);
    const float depth = p(P_DEPTH);
    float pan = pan_;
    for (int i = 0; i < n; ++i) {
      lfo_.advance();
      const float target = depth * lfo_.value(shape);
      pan = target + (pan - target) * 0.995f;
      const float gl = sqrtf(0.5f * (1.f - pan)), gr = sqrtf(0.5f * (1.f + pan));
      const float m = L[i], s = R[i];
      L[i] = m * gl * 1.414f;
      R[i] = s * gr * 1.414f;
    }
    pan_ = pan;
  }

  dsp::Lfo lfo_;
  volatile float pan_ = 0;
};
