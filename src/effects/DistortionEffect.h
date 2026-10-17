#pragma once
// ---------------------------------------------------------------------
//  Distorsion : 4 courbes de saturation + filtre de tonalite
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/Biquad.h"

class DistortionEffect : public Effect {
 public:
  enum { P_DRIVE, P_TONE, P_TYPE, P_MIX };
  enum Type { SOFT, HARD, FUZZ, FOLD };

  DistortionEffect() : Effect("Distortion", gfx::rgb(255, 40, 40)) {
    static const char* const types[] = {"Douce", "Dure", "Fuzz", "Fold"};
    addParam("Drive", "dB", 0.f, 40.f, 18.f);
    addParam("Tone", "Hz", 800.f, 12000.f, 5000.f, ParamType::Log);
    addChoice("Type", types, 4, 0);
    addParam("Mix", "%", 0.f, 1.f, 1.f);
  }

  void reset() override {
    tone_[0].reset(); tone_[1].reset();
    dcx_[0] = dcx_[1] = dcy_[0] = dcy_[1] = 0;
  }

  static float shape(int type, float x) {
    switch (type) {
      case SOFT: return dsp::softClip(x);
      case HARD: return dsp::clampf(x, -1.f, 1.f);
      case FUZZ:  // asymetrique, riche en harmoniques paires
        return x > 0.f ? 1.f - expf(-x) : -0.7f * (1.f - expf(0.7f * x * 1.6f));
      default: {  // repliement (wavefolder triangulaire)
        float u = (x + 1.f) * 0.25f;
        u -= floorf(u);
        return 4.f * fabsf(u - 0.5f) - 1.f;
      }
    }
  }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx&) const override {
    const float gain = dsp::dbToGain(p(P_DRIVE)), comp = makeup(gain);
    const int type = choice(P_TYPE);
    drawTransfer(g, x, y, w, h, [&](float v) { return dsp::clampf(shape(type, v * gain) * comp, -1, 1); });
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    const float k = 2.f + 1.5f * sinf(t * 2.f);
    gfx::plotFn(g, cx - 7, cy - 6, 15, 12, col, [&](float u) {
      return 0.5f + 0.45f * dsp::softClip(k * sinf(dsp::TWO_PI_F * u * 1.5f));
    });
  }

 protected:
  static float makeup(float gain) { return 0.2f + 0.6f / sqrtf(gain); }

  void onParamsChanged() override {
    tone_[0].setCutoff(p(P_TONE), fs_);
    tone_[1].setCutoff(p(P_TONE), fs_);
  }

  void process(float* L, float* R, int n) override {
    const float gain = dsp::dbToGain(p(P_DRIVE)), comp = makeup(gain);
    const float mix = p(P_MIX);
    const int type = choice(P_TYPE);
    float* ch[2] = {L, R};
    for (int c = 0; c < 2; ++c) {
      float* s = ch[c];
      for (int i = 0; i < n; ++i) {
        float y = shape(type, s[i] * gain) * comp;
        // bloqueur de continu (la fuzz est asymetrique)
        const float dc = y - dcx_[c] + 0.995f * dcy_[c];
        dcx_[c] = y; dcy_[c] = dc;
        y = tone_[c].process(dc);
        s[i] += (y - s[i]) * mix;
      }
    }
  }

  dsp::OnePoleLP tone_[2];
  float dcx_[2] = {0, 0}, dcy_[2] = {0, 0};
};
