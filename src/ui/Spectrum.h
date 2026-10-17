#pragma once
// Analyseur de spectre du signal de sortie (FFT 1024 points, fenetre de Hann)
#include "Gfx.h"
#include "dsp/Fft.h"

class Spectrum {
 public:
  static constexpr int NB = 40;        // bandes logarithmiques
  static constexpr int HIST = 112;     // lignes de l'historique (cascade)
  enum Style : uint8_t { CURVE, BARS, WATERFALL, STYLE_COUNT };

  void begin();
  void update(float dt);
  void draw(Canvas& g, int x, int y, int w, int h, int style, float t);

  const float* bands() const { return bands_; }
  float energy() const { return energy_; }   // niveau global 0..1 (animations)
  static const char* styleName(int s);

 private:
  void computeBins(uint32_t fs);
  float valueAt(float u) const;        // interpolation Catmull-Rom entre bandes

  dsp::Fft* fft_ = nullptr;
  float window_[cfg::FFT_SIZE];
  float re_[cfg::FFT_SIZE], im_[cfg::FFT_SIZE];
  float bands_[NB] = {}, peaks_[NB] = {}, peakHold_[NB] = {};
  float binLo_[NB], binHi_[NB], tilt_[NB];
  uint32_t binRate_ = 0;
  uint8_t hist_[HIST][NB] = {};
  int histPos_ = 0;
  float energy_ = 0;
};
