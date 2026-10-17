#pragma once
// =====================================================================
//  Classe intermediaire des filtres resonants (Low Pass, High Pass, Band Pass)
//  Parametres : Cutoff, Resonance, Pente (12/24 dB/oct), LFO (filtre auto)
// =====================================================================
#include "Effect.h"
#include "dsp/Biquad.h"
#include "dsp/Lfo.h"

class FilterEffect : public Effect {
 public:
  enum { P_FREQ, P_RES, P_SLOPE, P_LFO };

  FilterEffect(const char* name, uint16_t color, dsp::BiquadType type, float defFreq)
      : Effect(name, color), type_(type) {
    static const char* const slopes[] = {"12dB", "24dB"};
    addParam("Cutoff", "Hz", 30.f, 18000.f, defFreq, ParamType::Log);
    addParam("Reso", "", 0.5f, 10.f, 0.9f, ParamType::Log);
    addChoice("Pente", slopes, 2, 0);
    addParam("LFO", "Hz", 0.f, 8.f, 0.f);
    curFreq_ = defFreq;
  }

  void reset() override {
    for (auto& st : st_) for (auto& b : st) b.reset();
  }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const override {
    const float f = curFreq_, q = p(P_RES);
    const bool steep = choice(P_SLOPE) == 1;
    const auto c1 = dsp::BiquadCoefs::design(type_, f, q, 0, fs_);
    const auto c2 = dsp::BiquadCoefs::design(type_, f, 0.707f, 0, fs_);
    drawResponse(g, x, y, w, h, -36.f, 18.f, [&](float fr) {
      float db = c1.magnitudeDb(fr, fs_);
      if (steep) db += c2.magnitudeDb(fr, fs_);
      return db;
    });
    // marqueur de frequence de coupure
    const int fx = x + (int)(logf(f / 20.f) / logf(1000.f) * (w - 1));
    g.drawFastVLine(fx, y, h, gfx::scale(color(), 0.5f));
    g.fillTriangle(fx - 3, y, fx + 3, y, fx, y + 4, gfx::theme::TEXT);
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    const float f = 1000.f * powf(2.f, sinf(t * 1.5f) * 1.2f);
    const auto c = dsp::BiquadCoefs::design(type_, f, 2.5f, 0, 44100.f);
    gfx::plotFn(g, cx - 7, cy - 6, 15, 12, col, [&](float u) {
      return (c.magnitudeDb(20.f * powf(1000.f, u), 44100.f) + 30.f) / 42.f;
    });
  }

 protected:
  void onParamsChanged() override { updateCoefs(); }

  void process(float* L, float* R, int n) override {
    // LFO : modulation de +/- 1.5 octave autour de la coupure
    const float rate = p(P_LFO);
    float target = p(P_FREQ);
    if (rate > 0.05f) {
      lfo_.setRate(rate, fs_);
      lfo_.advance(n);
      target *= exp2f(1.5f * lfo_.sine());
    }
    // lissage logarithmique pour eviter les "zips" quand on tourne le potentiometre
    const float nf = expf(logf(curFreq_) + (logf(target) - logf(curFreq_)) * 0.35f);
    if (fabsf(nf - curFreq_) > 0.01f) { curFreq_ = nf; updateCoefs(); }

    const bool steep = choice(P_SLOPE) == 1;
    for (int i = 0; i < n; ++i) {
      float l = st_[0][0].process(L[i], c1_);
      float r = st_[0][1].process(R[i], c1_);
      if (steep) {
        l = st_[1][0].process(l, c2_);
        r = st_[1][1].process(r, c2_);
      }
      L[i] = l;
      R[i] = r;
    }
  }

  void updateCoefs() {
    c1_ = dsp::BiquadCoefs::design(type_, curFreq_, p(P_RES), 0, fs_);
    c2_ = dsp::BiquadCoefs::design(type_, curFreq_, 0.707f, 0, fs_);
  }

  dsp::BiquadType type_;
  dsp::BiquadCoefs c1_, c2_;
  dsp::Biquad st_[2][2];  // [etage][canal]
  dsp::Lfo lfo_;
  volatile float curFreq_;
};
