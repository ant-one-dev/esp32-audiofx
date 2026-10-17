#include "Gfx.h"

namespace gfx {

uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
  if (t <= 0.f) return a;
  if (t >= 1.f) return b;
  uint8_t ar, ag, ab, br, bg, bb;
  unpack(a, ar, ag, ab);
  unpack(b, br, bg, bb);
  return rgb(ar + (br - ar) * t, ag + (bg - ag) * t, ab + (bb - ab) * t);
}

uint16_t scale(uint16_t c, float k) {
  uint8_t r, g, b;
  unpack(c, r, g, b);
  auto s = [k](uint8_t v) { float x = v * k; return (uint8_t)(x > 255.f ? 255.f : x); };
  return rgb(s(r), s(g), s(b));
}

uint16_t hsv(float h, float s, float v) {
  h -= floorf(h);
  const float hh = h * 6.f;
  const int i = (int)hh;
  const float f = hh - i;
  const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
  float r, g, b;
  switch (i) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return rgb(r * 255, g * 255, b * 255);
}

uint16_t heat(float v) {
  v = v < 0 ? 0 : (v > 1 ? 1 : v);
  // noir -> violet -> magenta -> orange -> jaune -> blanc
  static const uint16_t stops[] = {rgb(4, 4, 16), rgb(70, 10, 140), rgb(220, 20, 140),
                                   rgb(255, 120, 20), rgb(255, 230, 60), rgb(255, 255, 255)};
  const float x = v * 5.f;
  int i = (int)x;
  if (i >= 5) return stops[5];
  return lerpColor(stops[i], stops[i + 1], x - i);
}

void vGradient(Canvas& g, int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
  for (int i = 0; i < h; ++i)
    g.drawFastHLine(x, y + i, w, lerpColor(top, bottom, h > 1 ? (float)i / (h - 1) : 0));
}

void hGradient(Canvas& g, int x, int y, int w, int h, uint16_t left, uint16_t right) {
  for (int i = 0; i < w; ++i)
    g.drawFastVLine(x + i, y, h, lerpColor(left, right, w > 1 ? (float)i / (w - 1) : 0));
}

void plotFn(Canvas& g, int x, int y, int w, int h, uint16_t col,
            const std::function<float(float)>& fn, bool fill, uint16_t fillCol, bool thick) {
  int py = 0;
  for (int i = 0; i < w; ++i) {
    float v = fn(w > 1 ? (float)i / (w - 1) : 0.f);
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    const int yy = y + (int)((1.f - v) * (h - 1) + 0.5f);
    if (fill && yy < y + h - 1) g.drawFastVLine(x + i, yy + 1, y + h - 1 - yy, fillCol);
    if (i > 0) {
      g.drawLine(x + i - 1, py, x + i, yy, col);
      if (thick) g.drawLine(x + i - 1, py - 1, x + i, yy - 1, col);
    }
    py = yy;
  }
}

void pill(Canvas& g, int x, int y, int w, int h, float anim, uint16_t onCol) {
  const uint16_t bg = lerpColor(theme::PANEL, scale(onCol, 0.55f), anim);
  g.fillRoundRect(x, y, w, h, h / 2, bg);
  g.drawRoundRect(x, y, w, h, h / 2, lerpColor(theme::DIM, onCol, anim));
  const int r = h / 2 - 2;
  const int cx = x + h / 2 + (int)((w - h) * anim);
  g.fillCircle(cx, y + h / 2, r, lerpColor(theme::DIM, theme::TEXT, anim));
}

void knob(Canvas& g, int cx, int cy, int r, float norm, uint16_t col, bool active,
          float potNorm, uint16_t bg) {
  norm = norm < 0 ? 0 : (norm > 1 ? 1 : norm);
  const uint16_t track = theme::GRID;
  const uint16_t c = active ? col : scale(col, 0.55f);
  // piste de fond : de 45 a 315 degres (0 = bas, sens horaire)
  g.drawSmoothArc(cx, cy, r, r - 3, 45, 315, track, bg, true);
  const int end = 45 + (int)(270 * norm);
  if (end > 46) g.drawSmoothArc(cx, cy, r, r - 3, 45, end, c, bg, true);
  // centre
  g.fillCircle(cx, cy, r - 5, theme::PANEL);
  const float a = (45.f + 270.f * norm) * DEG_TO_RAD;
  const int ix = cx - (int)(sinf(a) * (r - 6)), iy = cy + (int)(cosf(a) * (r - 6));
  g.drawLine(cx, cy, ix, iy, active ? theme::TEXT : theme::DIM);
  // position physique du potentiometre si pas encore "accroche"
  if (!active && potNorm >= 0.f) {
    const float b = (45.f + 270.f * potNorm) * DEG_TO_RAD;
    g.fillCircle(cx - (int)(sinf(b) * (r + 1)), cy + (int)(cosf(b) * (r + 1)), 1, theme::WARN);
  }
}

void shadowText(Canvas& g, const char* s, int x, int y, uint16_t col, uint8_t datum) {
  g.setTextDatum(datum);
  g.setTextColor(rgb(0, 0, 0));
  g.drawString(s, x + 1, y + 1);
  g.setTextColor(col);
  g.drawString(s, x, y);
}

void btIcon(Canvas& g, int x, int y, uint16_t col) {
  // rune Bluetooth 5x9
  g.drawFastVLine(x + 2, y, 9, col);
  g.drawLine(x + 2, y, x + 4, y + 2, col);
  g.drawLine(x + 4, y + 2, x, y + 6, col);
  g.drawLine(x + 2, y + 8, x + 4, y + 6, col);
  g.drawLine(x + 4, y + 6, x, y + 2, col);
}

}  // namespace gfx
