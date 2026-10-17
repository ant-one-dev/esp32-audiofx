#pragma once
// Oscillateur basse frequence
#include "DspUtil.h"

namespace dsp {

enum class LfoShape : uint8_t { Sine, Triangle, Square, Saw };

// Noms affiches pour les 3 premieres formes (parametres de type "Choice")
inline const char* const kLfoShapeNames[] = {"Sinus", "Triang", "Carre"};

// Valeur d'une forme d'onde pour une phase [0,1[ -> [-1, 1]
inline float lfoWave(LfoShape s, float ph) {
  switch (s) {
    case LfoShape::Sine:     return sinf(TWO_PI_F * ph);
    case LfoShape::Triangle: return ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph;
    case LfoShape::Square:   return ph < 0.5f ? 1.f : -1.f;
    case LfoShape::Saw:      return 2.f * ph - 1.f;
  }
  return 0.f;
}

struct Lfo {
  float phase = 0, inc = 0;
  void setRate(float hz, float fs) { inc = hz / fs; }
  // Avance de n echantillons (utilise pour une mise a jour par bloc)
  inline void advance(int n = 1) {
    phase += inc * n;
    phase -= floorf(phase);
  }
  inline float sine(float offset = 0.f) const {
    float p = phase + offset; p -= floorf(p);
    return sinf(TWO_PI_F * p);
  }
  inline float value(LfoShape s, float offset = 0.f) const {
    float p = phase + offset; p -= floorf(p);
    return lfoWave(s, p);
  }
};

// Oscillateur sinusoidal en quadrature : sin et cos sans appel a sinf()
// (rotation d'un phaseur, renormalise a chaque bloc)
struct QuadOsc {
  float s = 0, c = 1, cr = 1, sr = 0;
  float phase = 0, inc = 0;   // phase suivie en parallele (affichage / formes non sinus)
  void setRate(float hz, float fs) {
    const float w = TWO_PI_F * hz / fs;
    cr = cosf(w); sr = sinf(w);
    inc = hz / fs;
  }
  inline void step() {
    const float ns = s * cr + c * sr;
    c = c * cr - s * sr;
    s = ns;
    phase += inc;
    if (phase >= 1.f) phase -= 1.f;
  }
  // a appeler une fois par bloc pour eviter la derive d'amplitude
  void normalize() {
    const float k = 1.5f - 0.5f * (s * s + c * c);
    s *= k; c *= k;
  }
  // sin(phase + offset) pour un offset fixe donne par (cos o, sin o)
  inline float sinOffset(float co, float so) const { return s * co + c * so; }
  void reset() { s = 0; c = 1; phase = 0; }
};

}  // namespace dsp
