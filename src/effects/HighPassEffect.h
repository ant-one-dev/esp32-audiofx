#pragma once
// Filtre passe-haut resonant : laisse passer les aigus
#include "FilterEffect.h"

class HighPassEffect : public FilterEffect {
 public:
  HighPassEffect()
      : FilterEffect("High Pass", gfx::rgb(255, 120, 0), dsp::BiquadType::HighPass, 400.f) {}
};
