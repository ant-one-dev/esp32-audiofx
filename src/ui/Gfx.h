#pragma once
// Utilitaires graphiques : couleurs, degrades, easing, widgets
#include <TFT_eSPI.h>
#include <functional>
#include "Config.h"

using Canvas = TFT_eSprite;

namespace gfx {

// ---------------------------------------------------------------- couleurs
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
inline void unpack(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (c >> 8) & 0xF8; r |= r >> 5;
  g = (c >> 3) & 0xFC; g |= g >> 6;
  b = (c << 3) & 0xF8; b |= b >> 5;
}
uint16_t lerpColor(uint16_t a, uint16_t b, float t);
uint16_t scale(uint16_t c, float k);              // assombrit (k<1) / eclaircit (k>1)
uint16_t hsv(float h, float s, float v);          // h dans [0,1[
uint16_t heat(float v);                           // palette "chaleur" 0..1

// ---------------------------------------------------------------- theme
namespace theme {
constexpr uint16_t BG      = rgb(6, 8, 18);
constexpr uint16_t BG2     = rgb(14, 18, 36);
constexpr uint16_t PANEL   = rgb(22, 28, 52);
constexpr uint16_t GRID    = rgb(28, 36, 64);
constexpr uint16_t TEXT    = rgb(235, 240, 255);
constexpr uint16_t DIM     = rgb(120, 130, 165);
constexpr uint16_t ACCENT  = rgb(0, 200, 255);
constexpr uint16_t ACCENT2 = rgb(255, 60, 200);
constexpr uint16_t OK      = rgb(60, 230, 120);
constexpr uint16_t WARN    = rgb(255, 170, 0);
constexpr uint16_t BAD     = rgb(255, 60, 60);
}  // namespace theme

// ---------------------------------------------------------------- animation
inline float easeOutCubic(float t) { t = 1.f - t; return 1.f - t * t * t; }
inline float easeInOutCubic(float t) {
  return t < 0.5f ? 4.f * t * t * t : 1.f - powf(-2.f * t + 2.f, 3.f) / 2.f;
}
inline float easeOutBack(float t) {
  const float c1 = 1.70158f, c3 = c1 + 1.f;
  return 1.f + c3 * powf(t - 1.f, 3.f) + c1 * powf(t - 1.f, 2.f);
}
// Rapproche cur de target de facon exponentielle (independant du framerate)
inline float approach(float cur, float target, float speed, float dt) {
  return target + (cur - target) * expf(-speed * dt);
}

// ---------------------------------------------------------------- dessin
void vGradient(Canvas& g, int x, int y, int w, int h, uint16_t top, uint16_t bottom);
void hGradient(Canvas& g, int x, int y, int w, int h, uint16_t left, uint16_t right);
// Trace une fonction y = fn(x), x et y dans [0,1] (y=1 en haut)
void plotFn(Canvas& g, int x, int y, int w, int h, uint16_t col,
            const std::function<float(float)>& fn, bool fill = false,
            uint16_t fillCol = 0, bool thick = false);
// Interrupteur ON/OFF anime (anim 0 = off, 1 = on)
void pill(Canvas& g, int x, int y, int w, int h, float anim, uint16_t onCol);
// Bouton rotatif : norm 0..1, potNorm = position physique (<0 si inconnue)
void knob(Canvas& g, int cx, int cy, int r, float norm, uint16_t col, bool active,
          float potNorm, uint16_t bg);
// Texte centre avec ombre portee
void shadowText(Canvas& g, const char* s, int x, int y, uint16_t col, uint8_t datum);
// Icone Bluetooth
void btIcon(Canvas& g, int x, int y, uint16_t col);

}  // namespace gfx
