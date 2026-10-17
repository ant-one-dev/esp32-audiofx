#pragma once
// Filtre passe-bande resonant : ne garde qu'une bande autour de la frequence
#include "FilterEffect.h"

class BandPassEffect : public FilterEffect {
 public:
  BandPassEffect()
      : FilterEffect("Band Pass", gfx::rgb(255, 210, 0), dsp::BiquadType::BandPass, 900.f) {}
};
