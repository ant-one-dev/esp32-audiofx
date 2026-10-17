#pragma once
// FFT radix-2 complexe en place (taille puissance de 2)
#include <utility>
#include "DspUtil.h"

namespace dsp {

class Fft {
 public:
  explicit Fft(int n) : n_(n) {
    cosT_ = new float[n / 2];
    sinT_ = new float[n / 2];
    for (int i = 0; i < n / 2; ++i) {
      cosT_[i] = cosf(TWO_PI_F * i / n);
      sinT_[i] = -sinf(TWO_PI_F * i / n);
    }
  }
  ~Fft() { delete[] cosT_; delete[] sinT_; }
  int size() const { return n_; }

  void transform(float* re, float* im) const {
    // permutation par inversion de bits
    for (int i = 1, j = 0; i < n_; ++i) {
      int bit = n_ >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    // papillons
    for (int len = 2; len <= n_; len <<= 1) {
      const int half = len >> 1, step = n_ / len;
      for (int i = 0; i < n_; i += len) {
        for (int k = 0; k < half; ++k) {
          const float wr = cosT_[k * step], wi = sinT_[k * step];
          const int a = i + k, b = a + half;
          const float tr = re[b] * wr - im[b] * wi;
          const float ti = re[b] * wi + im[b] * wr;
          re[b] = re[a] - tr; im[b] = im[a] - ti;
          re[a] += tr;        im[a] += ti;
        }
      }
    }
  }

 private:
  int n_;
  float* cosT_;
  float* sinT_;
};

}  // namespace dsp
