#pragma once
// ---------------------------------------------------------------------
//  Phaser : chaine de filtres passe-tout du 1er ordre balayes par un LFO
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/Lfo.h"

class PhaserEffect : public Effect {
 public:
  enum { P_RATE, P_DEPTH, P_FB, P_STAGES };
  static constexpr int MAX_STAGES = 12;

  PhaserEffect() : Effect("Phaser", gfx::rgb(255, 150, 60)) {
    static const char* const st[] = {"4", "6", "8", "12"};
    addParam("Vitesse", "Hz", 0.05f, 5.f, 0.4f, ParamType::Log);
    addParam("Profond", "%", 0.f, 1.f, 0.85f);
    addParam("Feedbk", "%", 0.f, 0.9f, 0.5f);
    addChoice("Etages", st, 4, 1);
  }

  void reset() override {
    memset(z_, 0, sizeof(z_));
    fb_[0] = fb_[1] = 0;
    osc_.reset();
  }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    // reponse du phaser : somme dry + chaine passe-tout -> encoches
    const int stages = numStages();
    const float fc = sweepFreq(osc_.s);
    drawResponse(g, x, y, w, h, -30.f, 6.f, [&](float f) {
      const float ph = -2.f * stages * atanf(f / fc);  // phase de la chaine
      const float re = 1.f + cosf(ph), im = sinf(ph);
      return 10.f * log10f((re * re + im * im) * 0.25f + 1e-4f);
    });
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    for (int k = 0; k < 3; ++k) {
      const float ph = t * 2.f + k * 1.2f;
      g.drawCircle(cx - 4 + k * 4, cy + (int)(sinf(ph) * 3.f), 2, gfx::scale(col, 1.f - k * 0.25f));
    }
  }

 protected:
  int numStages() const {
    static const int n[] = {4, 6, 8, 12};
    return n[choice(P_STAGES)];
  }
  float sweepFreq(float lfo) const {
    // balayage de 200 Hz a ~4 kHz (logarithmique)
    return 200.f * exp2f((0.5f + 0.5f * lfo) * p(P_DEPTH) * 4.3f);
  }
  // coefficient passe-tout pour une frequence (approximation de tan)
  inline float apCoef(float f) const {
    float x = dsp::PI_F * f / fs_;
    const float x2 = x * x;
    const float t = x * (1.f + x2 * (0.33333f + x2 * 0.13333f));
    return (t - 1.f) / (t + 1.f);
  }

  void process(float* L, float* R, int n) override {
    osc_.setRate(p(P_RATE), fs_);
    const int stages = numStages();
    const float fb = p(P_FB);
    float aL = 0, aR = 0;
    for (int i = 0; i < n; ++i) {
      osc_.step();
      if ((i & 7) == 0) {  // coefficients mis a jour tous les 8 echantillons
        aL = apCoef(sweepFreq(osc_.s));
        aR = apCoef(sweepFreq(osc_.c));
      }
      float l = L[i] + fb_[0] * fb, r = R[i] + fb_[1] * fb;
      for (int s = 0; s < stages; ++s) {
        const float yl = aL * l + z_[0][s];
        z_[0][s] = l - aL * yl;
        l = yl;
        const float yr = aR * r + z_[1][s];
        z_[1][s] = r - aR * yr;
        r = yr;
      }
      fb_[0] = dsp::softClip(l);
      fb_[1] = dsp::softClip(r);
      L[i] = 0.5f * (L[i] + l);
      R[i] = 0.5f * (R[i] + r);
    }
    osc_.normalize();
  }

  dsp::QuadOsc osc_;
  float z_[2][MAX_STAGES] = {};
  float fb_[2] = {0, 0};
};
