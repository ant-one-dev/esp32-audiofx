#pragma once
// Filtre biquad (formules du "Audio EQ Cookbook" de R. Bristow-Johnson)
#include "DspUtil.h"

namespace dsp {

enum class BiquadType : uint8_t { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

struct BiquadCoefs {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

  static BiquadCoefs design(BiquadType type, float freq, float q, float gainDb, float fs) {
    BiquadCoefs c;
    freq = clampf(freq, 10.f, fs * 0.49f);
    q = fmaxf(q, 0.05f);
    const float w0 = TWO_PI_F * freq / fs;
    const float cw = cosf(w0), sw = sinf(w0);
    const float alpha = sw / (2.f * q);
    const float A = powf(10.f, gainDb / 40.f);
    float b0, b1, b2, a0, a1, a2;
    switch (type) {
      case BiquadType::LowPass:
        b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case BiquadType::HighPass:
        b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case BiquadType::BandPass:  // gain unitaire au pic
        b0 = alpha; b1 = 0; b2 = -alpha;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case BiquadType::Peak:
        b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
        a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
      case BiquadType::LowShelf: {
        const float sa = 2 * sqrtf(A) * alpha;
        b0 = A * ((A + 1) - (A - 1) * cw + sa);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - sa);
        a0 = (A + 1) + (A - 1) * cw + sa;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - sa; break;
      }
      case BiquadType::HighShelf:
      default: {
        const float sa = 2 * sqrtf(A) * alpha;
        b0 = A * ((A + 1) + (A - 1) * cw + sa);
        b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - sa);
        a0 = (A + 1) - (A - 1) * cw + sa;
        a1 = 2 * ((A - 1) - (A + 1) * cw);
        a2 = (A + 1) - (A - 1) * cw - sa; break;
      }
    }
    const float inv = 1.f / a0;
    c.b0 = b0 * inv; c.b1 = b1 * inv; c.b2 = b2 * inv;
    c.a1 = a1 * inv; c.a2 = a2 * inv;
    return c;
  }

  // Module de la reponse en frequence (en dB) - utilise pour l'affichage
  float magnitudeDb(float freq, float fs) const {
    const float w = TWO_PI_F * freq / fs;
    const float c1 = cosf(w), s1 = sinf(w), c2 = cosf(2 * w), s2 = sinf(2 * w);
    const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    const float dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    const float num = nr * nr + ni * ni, den = dr * dr + di * di;
    return 10.f * log10f((num + 1e-12f) / (den + 1e-12f));
  }
};

// Etat d'un biquad pour un canal (forme directe transposee II)
struct Biquad {
  float z1 = 0, z2 = 0;
  inline float process(float x, const BiquadCoefs& c) {
    const float y = c.b0 * x + z1;
    z1 = c.b1 * x - c.a1 * y + z2;
    z2 = c.b2 * x - c.a2 * y;
    return y;
  }
  void reset() { z1 = z2 = 0; }
};

// Passe-bas a un pole (amortissement, tonalite...)
struct OnePoleLP {
  float a = 0, z = 0;
  void setCutoff(float fc, float fs) { a = expf(-TWO_PI_F * fc / fs); }
  inline float process(float x) { z = x + a * (z - x); return z; }
  void reset() { z = 0; }
};

}  // namespace dsp
