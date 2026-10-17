#pragma once
// Petites fonctions DSP communes
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

namespace dsp {

constexpr float PI_F  = 3.14159265358979f;
constexpr float TWO_PI_F = 6.28318530717959f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return powf(10.f, db * 0.05f); }
inline float gainToDb(float g) { return 20.f * log10f(g + 1e-9f); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Saturation douce rapide (approximation de tanh)
inline float softClip(float x) {
  if (x > 3.f) return 1.f;
  if (x < -3.f) return -1.f;
  float x2 = x * x;
  return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Coefficient d'un lissage exponentiel pour une constante de temps donnee
inline float onePoleCoef(float timeMs, float fs) {
  if (timeMs <= 0.f) return 0.f;
  return expf(-1.f / (timeMs * 0.001f * fs));
}

// Allocation de gros buffers : PSRAM si disponible, sinon RAM interne
inline void* allocBuffer(size_t bytes) {
  void* p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = calloc(1, bytes);
  return p;
}

// Generateur de nombres pseudo-aleatoires tres rapide
struct Rng {
  uint32_t s = 0x12345678;
  inline float next() {  // [-1, 1]
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return (int32_t)s * (1.f / 2147483648.f);
  }
};

}  // namespace dsp
