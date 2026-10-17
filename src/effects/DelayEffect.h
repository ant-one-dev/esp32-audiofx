#pragma once
// ---------------------------------------------------------------------
#include "Effect.h"
#include "dsp/Biquad.h"
#include "dsp/DelayLine.h"

class DelayEffect : public Effect {
 public:
  enum { P_TIME, P_FB, P_MIX, P_MODE };
  static constexpr float MAX_MS = 1500.f;

  DelayEffect() : Effect("Delay", gfx::rgb(0, 230, 200)) {
    static const char* const modes[] = {"Stereo", "PingPg"};
    addParam("Temps", "ms", 20.f, MAX_MS, 350.f, ParamType::Log);
    addParam("Feedbk", "%", 0.f, 0.95f, 0.45f);
    addParam("Mix", "%", 0.f, 1.f, 0.35f);
    addChoice("Mode", modes, 2, 1);
    const int maxSamples = (int)(cfg::MAX_FS * MAX_MS / 1000.f) + 8;
    line_[0].allocate(maxSamples);
    line_[1].allocate(maxSamples);
  }

  void prepare(float fs) override {
    lp_[0].setCutoff(5500.f, fs);
    lp_[1].setCutoff(5500.f, fs);
    Effect::prepare(fs);
  }

  void reset() override {
    line_[0].clear(); line_[1].clear();
    lp_[0].reset(); lp_[1].reset();
    curDelay_ = p(P_TIME) * 0.001f * fs_;
  }

  void drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const override {
    // repetitions successives : hauteur = feedback^k, alternance G/D en ping-pong
    const bool pp = choice(P_MODE) == 1;
    const float fb = p(P_FB);
    const int taps = 7;
    const int mid = y + h / 2;
    g.drawFastHLine(x, mid, w, gfx::theme::GRID);
    const float period = p(P_TIME) * 0.001f;
    const float cycle = period * taps;
    const float pos = fmodf(ctx.t, cycle) / cycle;  // impulsion qui voyage
    for (int k = 0; k < taps; ++k) {
      const float amp = k == 0 ? 1.f : powf(fb, (float)k);
      const int tx = x + 4 + k * (w - 8) / (taps - 1);
      const int hh = (int)(amp * (h / 2 - 3));
      const float near = 1.f - fminf(1.f, fabsf(pos * (taps - 1) - k) * 2.f);
      const uint16_t c = gfx::lerpColor(k == 0 ? gfx::theme::DIM : color(), gfx::theme::TEXT, near);
      const bool up = !pp || (k % 2 == 0);
      if (hh > 0) {
        if (!pp) g.fillRect(tx - 1, mid - hh, 3, hh * 2, c);
        else if (up) g.fillRect(tx - 1, mid - hh, 3, hh, c);
        else g.fillRect(tx - 1, mid, 3, hh, gfx::lerpColor(gfx::theme::ACCENT2, gfx::theme::TEXT, near));
      }
      if (near > 0.f) g.drawCircle(tx, mid, 2 + (int)(near * 5), c);
    }
    if (pp) {
      g.setTextFont(1);
      g.setTextColor(gfx::theme::DIM);
      g.setTextDatum(TR_DATUM);
      g.drawString("L", x + w - 2, y + 1);
      g.setTextDatum(BR_DATUM);
      g.drawString("R", x + w - 2, y + h - 1);
    }
  }

  void drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const override {
    for (int k = 0; k < 4; ++k) {
      const float ph = fmodf(t * 1.2f + k * 0.25f, 1.f);
      const int hh = 6 - k;
      g.fillRect(cx - 7 + k * 4, cy - hh, 2, hh * 2, gfx::scale(col, 1.f - 0.6f * ph));
    }
  }

 protected:
  void process(float* L, float* R, int n) override {
    const float target = p(P_TIME) * 0.001f * fs_;
    const float fb = p(P_FB), mix = p(P_MIX);
    const float dry = 1.f - 0.5f * mix;
    const bool pp = choice(P_MODE) == 1;
    float d = curDelay_;
    for (int i = 0; i < n; ++i) {
      d += (target - d) * 0.0005f;  // glissement facon "bande" quand on change le temps
      const float yl = line_[0].readFrac(d);
      const float yr = line_[1].readFrac(d);
      const float fl = lp_[0].process(yl), fr = lp_[1].process(yr);
      if (pp) {
        line_[0].write(dsp::softClip(0.5f * (L[i] + R[i]) + fb * fr));
        line_[1].write(dsp::softClip(fb * fl));
      } else {
        line_[0].write(dsp::softClip(L[i] + fb * fl));
        line_[1].write(dsp::softClip(R[i] + fb * fr));
      }
      L[i] = L[i] * dry + yl * mix;
      R[i] = R[i] * dry + yr * mix;
    }
    curDelay_ = d;
  }

  dsp::DelayLine line_[2];
  dsp::OnePoleLP lp_[2];
  float curDelay_ = 15000.f;
};
