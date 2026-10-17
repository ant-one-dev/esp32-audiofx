#pragma once
// ---------------------------------------------------------------------
//  Reverb de type Freeverb (Jezar) : 8 peignes + 4 passe-tout par canal
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/DelayLine.h"

class ReverbEffect : public Effect {
 public:
  enum { P_SIZE, P_DAMP, P_WIDTH, P_MIX };
  static constexpr int NC = 8, NA = 4, SPREAD = 23;

  ReverbEffect() : Effect("Reverb", gfx::rgb(160, 90, 255)) {
    addParam("Taille", "%", 0.f, 1.f, 0.75f);
    addParam("Amorti", "%", 0.f, 1.f, 0.45f);
    addParam("Largeur", "%", 0.f, 1.f, 1.f);
    addParam("Mix", "%", 0.f, 1.f, 0.3f);
    // un seul bloc de memoire en PSRAM, dimensionne pour MAX_FS
    int total = 0;
    for (int i = 0; i < NC; ++i) total += 2 * scaled(kComb[i] + SPREAD, cfg::MAX_FS);
    for (int i = 0; i < NA; ++i) total += 2 * scaled(kAllp[i] + SPREAD, cfg::MAX_FS);
    mem_ = (float*)dsp::allocBuffer(total * sizeof(float));
    memSize_ = total;
  }
  ~ReverbEffect() override { free(mem_); }

  void prepare(float fs) override {
    float* p = mem_;
    for (int ch = 0; ch < 2; ++ch) {
      const int sp = ch ? SPREAD : 0;
      for (int i = 0; i < NC; ++i) {
        comb_[ch][i].buf = p; comb_[ch][i].size = scaled(kComb[i] + sp, fs);
        p += comb_[ch][i].size;
      }
      for (int i = 0; i < NA; ++i) {
        allp_[ch][i].buf = p; allp_[ch][i].size = scaled(kAllp[i] + sp, fs);
        p += allp_[ch][i].size;
      }
    }
    Effect::prepare(fs);
  }

  void reset() override {
    if (mem_) memset(mem_, 0, memSize_ * sizeof(float));
    for (auto& ch : comb_) for (auto& c : ch) { c.pos = 0; c.store = 0; }
    for (auto& ch : allp_) for (auto& a : ch) a.pos = 0;
  }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const override {
    // reponse impulsionnelle stylisee : reflexions precoces + queue exponentielle
    const float size = p(P_SIZE), damp = p(P_DAMP);
    const float decay = 0.08f + size * 0.9f;
    dsp::Rng rng;
    const int base = y + h - 1;
    for (int i = 0; i < w; i += 2) {
      const float u = (float)i / w;
      float env = expf(-u / decay * 1.6f);
      if (u < 0.12f) env *= (rng.next() > 0.3f) ? 1.f : 0.3f;  // reflexions precoces
      const float shimmer = 0.6f + 0.4f * sinf(ctx.t * 6.f + i * 0.7f);
      const float v = env * (0.55f + 0.45f * fabsf(rng.next())) * shimmer;
      const int hh = (int)(v * (h - 4));
      // l'amortissement "refroidit" la couleur de la queue
      const uint16_t c = gfx::lerpColor(color(), gfx::rgb(40, 80, 200), u * damp * 1.5f);
      if (hh > 0) g.drawFastVLine(x + i, base - hh, hh, c);
    }
    g.drawFastVLine(x + 1, y, h, gfx::theme::TEXT);  // son direct
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    for (int k = 0; k < 3; ++k) {
      const float ph = fmodf(t * 0.8f + k / 3.f, 1.f);
      const int r = 2 + (int)(ph * 6);
      g.drawCircle(cx, cy, r, gfx::scale(col, 1.f - ph));
    }
    g.fillCircle(cx, cy, 1, col);
  }

 protected:
  static int scaled(int n, float fs) { return (int)(n * fs / 44100.f) + 1; }

  void onParamsChanged() override {
    roomFb_ = 0.7f + 0.28f * p(P_SIZE);
    damp_ = 0.4f * p(P_DAMP);
    const float wet = 2.f * p(P_MIX), width = p(P_WIDTH);
    wet1_ = wet * (width * 0.5f + 0.5f);
    wet2_ = wet * ((1.f - width) * 0.5f);
    dry_ = 1.f - 0.6f * p(P_MIX);
  }

  void process(float* L, float* R, int n) override {
    if (!mem_) return;
    for (int i = 0; i < n; ++i) {
      const float in = (L[i] + R[i]) * 0.015f;
      float ol = 0, orr = 0;
      for (int c = 0; c < NC; ++c) {
        ol += comb_[0][c].process(in, roomFb_, damp_);
        orr += comb_[1][c].process(in, roomFb_, damp_);
      }
      for (int a = 0; a < NA; ++a) {
        ol = allp_[0][a].process(ol);
        orr = allp_[1][a].process(orr);
      }
      L[i] = L[i] * dry_ + ol * wet1_ + orr * wet2_;
      R[i] = R[i] * dry_ + orr * wet1_ + ol * wet2_;
    }
  }

  static constexpr int kComb[NC] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
  static constexpr int kAllp[NA] = {556, 441, 341, 225};
  dsp::Comb comb_[2][NC];
  dsp::AllPass allp_[2][NA];
  float* mem_ = nullptr;
  int memSize_ = 0;
  float roomFb_ = 0.9f, damp_ = 0.2f, wet1_ = 1, wet2_ = 0, dry_ = 1;
};
