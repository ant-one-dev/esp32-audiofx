#pragma once
// Ligne a retard circulaire (buffer en PSRAM) avec lecture fractionnaire
#include "DspUtil.h"

namespace dsp {

class DelayLine {
 public:
  DelayLine() = default;
  DelayLine(const DelayLine&) = delete;
  DelayLine& operator=(const DelayLine&) = delete;
  ~DelayLine() { free(buf_); }

  bool allocate(int maxSamples) {
    free(buf_);
    size_ = maxSamples + 4;
    buf_ = (float*)allocBuffer(size_ * sizeof(float));
    pos_ = 0;
    return buf_ != nullptr;
  }
  void clear() { if (buf_) memset(buf_, 0, size_ * sizeof(float)); }
  int size() const { return size_; }

  inline void write(float x) {
    buf_[pos_] = x;
    if (++pos_ >= size_) pos_ = 0;
  }
  // Lecture d'un echantillon entier (delay >= 1 : le plus recent ecrit = 1)
  inline float read(int delay) const {
    int i = pos_ - delay;
    if (i < 0) i += size_;
    return buf_[i];
  }
  // Lecture interpolee lineairement (delay en echantillons, >= 1)
  inline float readFrac(float delay) const {
    delay = clampf(delay, 1.f, (float)(size_ - 3));
    const int di = (int)delay;
    const float f = delay - di;
    const float a = read(di), b = read(di + 1);
    return a + (b - a) * f;
  }

 private:
  float* buf_ = nullptr;
  int size_ = 0;
  int pos_ = 0;
};

// Filtre en peigne et passe-tout pour la reverb (Freeverb)
struct Comb {
  float* buf = nullptr; int size = 0, pos = 0;
  float store = 0;
  inline float process(float x, float feedback, float damp) {
    const float out = buf[pos];
    store = out * (1.f - damp) + store * damp;
    buf[pos] = x + store * feedback;
    if (++pos >= size) pos = 0;
    return out;
  }
};

struct AllPass {
  float* buf = nullptr; int size = 0, pos = 0;
  inline float process(float x) {
    const float b = buf[pos];
    const float out = b - x;
    buf[pos] = x + b * 0.5f;
    if (++pos >= size) pos = 0;
    return out;
  }
};

}  // namespace dsp
