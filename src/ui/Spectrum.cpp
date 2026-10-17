#include "Spectrum.h"
#include "audio/AudioEngine.h"

using namespace gfx;

namespace {
constexpr float F_MIN = 30.f, F_MAX = 16000.f;
constexpr float DB_FLOOR = -80.f, DB_RANGE = 70.f;
}  // namespace

const char* Spectrum::styleName(int s) {
  static const char* const n[] = {"Courbe", "Barres", "Cascade"};
  return n[s % STYLE_COUNT];
}

void Spectrum::begin() {
  fft_ = new dsp::Fft(cfg::FFT_SIZE);
  for (int i = 0; i < cfg::FFT_SIZE; ++i)
    window_[i] = 0.5f - 0.5f * cosf(dsp::TWO_PI_F * i / (cfg::FFT_SIZE - 1));
  computeBins(audio.sampleRate());
}

void Spectrum::computeBins(uint32_t fs) {
  binRate_ = fs;
  const float binHz = (float)fs / cfg::FFT_SIZE;
  for (int b = 0; b < NB; ++b) {
    const float f0 = F_MIN * powf(F_MAX / F_MIN, (float)b / NB);
    const float f1 = F_MIN * powf(F_MAX / F_MIN, (float)(b + 1) / NB);
    binLo_[b] = f0 / binHz;
    binHi_[b] = f1 / binHz;
    // compensation +3 dB/octave : un bruit rose apparait plat
    tilt_[b] = 3.f * log2f(sqrtf(f0 * f1) / 1000.f);
  }
}

void Spectrum::update(float dt) {
  if (audio.sampleRate() != binRate_) computeBins(audio.sampleRate());
  audio.copyAnalysis(re_);
  for (int i = 0; i < cfg::FFT_SIZE; ++i) { re_[i] *= window_[i]; im_[i] = 0.f; }
  fft_->transform(re_, im_);

  // module normalise : 2/N pour le spectre unilateral, /0.5 gain de la fenetre de Hann
  const float norm = 4.f / cfg::FFT_SIZE;
  auto mag2 = [&](int k) { return re_[k] * re_[k] + im_[k] * im_[k]; };
  float sum = 0;
  for (int b = 0; b < NB; ++b) {
    float m2;
    const int lo = (int)binLo_[b], hi = (int)binHi_[b];
    if (hi - lo < 1) {
      // bande plus etroite qu'un bin : interpolation lineaire
      const float c = 0.5f * (binLo_[b] + binHi_[b]);
      const int k = (int)c;
      const float f = c - k;
      m2 = mag2(k) * (1 - f) + mag2(k + 1) * f;
    } else {
      m2 = 0;
      for (int k = lo; k <= hi && k < cfg::FFT_SIZE / 2; ++k) m2 = fmaxf(m2, mag2(k));
    }
    const float db = 10.f * log10f(m2 * norm * norm + 1e-12f) + tilt_[b];
    const float v = dsp::clampf((db - DB_FLOOR) / DB_RANGE, 0.f, 1.f);
    // attaque rapide, retombee douce
    if (v > bands_[b]) bands_[b] += (v - bands_[b]) * 0.7f;
    else bands_[b] = fmaxf(v, bands_[b] - dt * 0.9f);
    // crete : maintien puis chute avec gravite
    if (bands_[b] >= peaks_[b]) { peaks_[b] = bands_[b]; peakHold_[b] = 0.5f; }
    else if ((peakHold_[b] -= dt) < 0) peaks_[b] = fmaxf(0.f, peaks_[b] + peakHold_[b] * dt * 2.f);
    sum += bands_[b];
    hist_[histPos_][b] = (uint8_t)(bands_[b] * 255.f);
  }
  histPos_ = (histPos_ + 1) % HIST;
  energy_ = sum / NB;
}

float Spectrum::valueAt(float u) const {
  const float x = u * (NB - 1);
  const int i = (int)x;
  const float t = x - i;
  auto at = [&](int k) { return bands_[k < 0 ? 0 : (k >= NB ? NB - 1 : k)]; };
  const float p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
  const float v = p1 + 0.5f * t * (p2 - p0 + t * (2 * p0 - 5 * p1 + 4 * p2 - p3 + t * (3 * (p1 - p2) + p3 - p0)));
  return dsp::clampf(v, 0.f, 1.f);
}

void Spectrum::draw(Canvas& g, int x, int y, int w, int h, int style, float t) {
  // palette verticale : bleu profond en bas -> cyan -> magenta en haut (teinte qui derive)
  uint16_t lut[128], lutDim[128];
  const int hh = h > 128 ? 128 : h;
  for (int r = 0; r < hh; ++r) {
    const float u = (float)r / (hh - 1);  // 0 = bas
    const uint16_t c = hsv(0.62f - 0.45f * u + 0.04f * sinf(t * 0.4f), 0.9f, 0.55f + 0.45f * u);
    lut[r] = c;
    lutDim[r] = scale(c, 0.45f);
  }
  const int base = y + h - 1;

  // grille de frequences discrete
  static const float marks[] = {100.f, 1000.f, 10000.f};
  for (float f : marks) {
    const int gx = x + (int)(logf(f / F_MIN) / logf(F_MAX / F_MIN) * (w - 1));
    for (int yy = y; yy < base; yy += 3) g.drawPixel(gx, yy, theme::GRID);
  }

  if (style == CURVE) {
    int py = base;
    for (int i = 0; i < w; ++i) {
      const float v = valueAt((float)i / (w - 1));
      const int top = base - (int)(v * (h - 2));
      // remplissage en degrade (par segments de 3 lignes)
      for (int yy = top + 1; yy <= base; yy += 3) {
        const int seg = (base - yy + 1) < 3 ? (base - yy + 1) : 3;
        g.drawFastVLine(x + i, yy, seg, lutDim[base - yy]);
      }
      const uint16_t lc = lerpColor(lut[base - top < hh ? base - top : hh - 1], theme::TEXT, 0.35f);
      if (i > 0) {
        g.drawLine(x + i - 1, py, x + i, top, lc);
        g.drawLine(x + i - 1, py - 1, x + i, top - 1, scale(lc, 0.6f));
      }
      py = top;
    }
    // cretes : points
    for (int b = 0; b < NB; ++b) {
      const int px = x + b * (w - 1) / (NB - 1);
      g.drawPixel(px, base - (int)(peaks_[b] * (h - 2)) - 2, theme::TEXT);
    }
  } else if (style == BARS) {
    const float bw = (float)w / NB;
    for (int b = 0; b < NB; ++b) {
      const int bx = x + (int)(b * bw), bwi = (int)((b + 1) * bw) - (int)(b * bw) - 1;
      const int top = base - (int)(bands_[b] * (h - 3));
      for (int yy = top; yy <= base; yy += 2) g.drawFastHLine(bx, yy, bwi, lut[base - yy]);
      const int pk = base - (int)(peaks_[b] * (h - 3)) - 2;
      g.drawFastHLine(bx, pk, bwi, theme::TEXT);
    }
  } else {  // WATERFALL : la ligne la plus recente en haut
    const float bw = (float)w / NB;
    const int rows = h < HIST ? h : HIST;
    static uint16_t heatLut[64];
    static bool init = false;
    if (!init) { for (int i = 0; i < 64; ++i) heatLut[i] = heat(i / 63.f); init = true; }
    for (int r = 0; r < rows; ++r) {
      const uint8_t* row = hist_[(histPos_ - 1 - r + HIST) % HIST];
      for (int b = 0; b < NB; ++b) {
        const int bx = x + (int)(b * bw), bwi = (int)((b + 1) * bw) - (int)(b * bw);
        g.drawFastHLine(bx, y + r, bwi, heatLut[row[b] >> 2]);
      }
    }
  }
}
