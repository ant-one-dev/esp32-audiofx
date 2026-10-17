#pragma once
// Filtre passe-bas resonant : laisse passer les graves
#include "FilterEffect.h"

class LowPassEffect : public FilterEffect {
 public:
  LowPassEffect()
      : FilterEffect("Low Pass", gfx::rgb(0, 170, 255), dsp::BiquadType::LowPass, 1200.f) {}
};
