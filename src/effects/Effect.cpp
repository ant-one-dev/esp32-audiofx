#include "Effect.h"

using namespace gfx;

// =====================================================================
//  Param
// =====================================================================
float Param::norm() const {
  const float v = value;
  switch (type) {
    case ParamType::Log: return logf(v / min) / logf(max / min);
    case ParamType::Choice: return numChoices > 1 ? v / (numChoices - 1) : 0.f;
    default: return (v - min) / (max - min);
  }
}

float Param::fromNorm(float n) const {
  n = dsp::clampf(n, 0.f, 1.f);
  switch (type) {
    case ParamType::Log: return min * powf(max / min, n);
    case ParamType::Choice: return roundf(n * (numChoices - 1));
    case ParamType::Int: return roundf(min + (max - min) * n);
    default: return min + (max - min) * n;
  }
}

void Param::format(char* buf, size_t len) const {
  const float v = value;
  if (type == ParamType::Choice) {
    snprintf(buf, len, "%s", choices[choice()]);
    return;
  }
  if (type == ParamType::Int) { snprintf(buf, len, "%d%s", (int)v, unit); return; }
  if (!strcmp(unit, "%")) { snprintf(buf, len, "%d%%", (int)roundf(v * 100.f)); return; }
  if (!strcmp(unit, "Hz") && v >= 1000.f) { snprintf(buf, len, "%.1fk", v / 1000.f); return; }
  const float a = fabsf(v);
  const int dec = a >= 10.f ? 0 : (a >= 1.f ? 1 : 2);
  snprintf(buf, len, "%.*f%s", dec, v, unit);
}

// =====================================================================
//  Effect : audio et parametres
// =====================================================================
Effect::Effect(const char* name, uint16_t color) : name_(name), color_(color) {}

int Effect::addParam(const char* name, const char* unit, float min, float max, float def,
                     ParamType type) {
  if (numParams_ >= MAX_PARAMS) return -1;
  Param& p = params_[numParams_];
  p.name = name; p.unit = unit; p.min = min; p.max = max; p.def = def;
  p.type = type; p.value = def;
  return numParams_++;
}

int Effect::addChoice(const char* name, const char* const* choices, uint8_t count, uint8_t def) {
  const int i = addParam(name, "", 0, count - 1, def, ParamType::Choice);
  if (i >= 0) { params_[i].choices = choices; params_[i].numChoices = count; }
  return i;
}

void Effect::setParam(int i, float value) {
  if (i < 0 || i >= numParams_) return;
  Param& p = params_[i];
  value = dsp::clampf(value, p.min, p.max);
  if (p.type == ParamType::Choice || p.type == ParamType::Int) value = roundf(value);
  if (value != p.value) { p.value = value; ++version_; }
}

void Effect::setParamNorm(int i, float norm) {
  if (i < 0 || i >= numParams_) return;
  setParam(i, params_[i].fromNorm(norm));
}

void Effect::resetParams() {
  for (int i = 0; i < numParams_; ++i) params_[i].value = params_[i].def;
  ++version_;
}

void Effect::prepare(float fs) {
  fs_ = fs;
  onParamsChanged();
  seenVersion_ = version_;
  reset();
}

void Effect::run(float* L, float* R, int n) {
  const uint32_t v = version_;
  if (v != seenVersion_) { seenVersion_ = v; onParamsChanged(); }

  const bool on = enabled_;
  if (!on && mix_ <= 0.f) return;                       // bypass complet
  if (on && mix_ >= 1.f) { process(L, R, n); return; }  // effet seul
  if (on && mix_ <= 0.f) reset();                       // reprise : buffers propres

  // Fondu enchaine dry/wet pour activer/desactiver sans clic
  static float dryL[cfg::BLOCK], dryR[cfg::BLOCK];
  memcpy(dryL, L, n * sizeof(float));
  memcpy(dryR, R, n * sizeof(float));
  process(L, R, n);
  const float step = (on ? 1.f : -1.f) / (0.03f * fs_);
  float m = mix_;
  for (int i = 0; i < n; ++i) {
    m = dsp::clampf(m + step, 0.f, 1.f);
    L[i] = dryL[i] + (L[i] - dryL[i]) * m;
    R[i] = dryR[i] + (R[i] - dryR[i]) * m;
  }
  mix_ = m;
}

// =====================================================================
//  Effect : affichage
// =====================================================================
void Effect::drawIcon(Canvas& g, int cx, int cy, uint16_t col, float t) const {
  // Icone par defaut : cercle pulse avec l'initiale
  const int r = 6 + (int)(sinf(t * 3.f) * 1.f);
  g.drawCircle(cx, cy, r, col);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(col);
  g.setTextFont(1);
  char s[2] = {name_[0], 0};
  g.drawString(s, cx + 1, cy + 1);
}

void Effect::drawGrid(Canvas& g, int x, int y, int w, int h, bool logFreq) const {
  if (logFreq) {
    static const float marks[] = {100.f, 1000.f, 10000.f};
    for (float f : marks) {
      const int xx = x + (int)(logf(f / 20.f) / logf(1000.f) * (w - 1));
      g.drawFastVLine(xx, y, h, theme::GRID);
    }
    g.setTextFont(1);
    g.setTextColor(theme::DIM);
    g.setTextDatum(BL_DATUM);
    g.drawString("100", x + (int)(logf(5.f) / logf(1000.f) * w) + 2, y + h - 1);
    g.drawString("1k", x + (int)(logf(50.f) / logf(1000.f) * w) + 2, y + h - 1);
    g.drawString("10k", x + (int)(logf(500.f) / logf(1000.f) * w) + 2, y + h - 1);
  } else {
    for (int i = 1; i < 4; ++i) g.drawFastVLine(x + w * i / 4, y, h, theme::GRID);
  }
  g.drawFastHLine(x, y + h / 2, w, theme::GRID);
}

void Effect::drawSpectrumGhost(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx,
                               uint16_t col) {
  if (!ctx.spectrum || ctx.numBands <= 0) return;
  const uint16_t c = scale(col, 0.22f);
  for (int i = 0; i < w; i += 2) {
    const float v = ctx.spectrum[i * ctx.numBands / w];
    const int bh = (int)(v * (h - 2));
    if (bh > 0) g.drawFastVLine(x + i, y + h - 1 - bh, bh, c);
  }
}

void Effect::drawResponse(Canvas& g, int x, int y, int w, int h, float minDb, float maxDb,
                          const std::function<float(float)>& dbAt) const {
  drawGrid(g, x, y, w, h, true);
  const uint16_t fill = scale(color_, 0.30f);
  plotFn(g, x, y, w, h, color_,
         [&](float u) {
           const float f = 20.f * powf(1000.f, u);
           return (dbAt(f) - minDb) / (maxDb - minDb);
         },
         true, fill, true);
}

void Effect::drawLfo(Canvas& g, int x, int y, int w, int h,
                     const std::function<float(float)>& wave, float phase, float depth) const {
  drawGrid(g, x, y, w, h, false);
  const float amp = 0.1f + 0.9f * dsp::clampf(depth, 0.f, 1.f);
  // deux periodes affichees, la courbe defile avec la phase
  plotFn(g, x, y, w, h, color_,
         [&](float u) { return 0.5f + 0.42f * amp * wave(u * 2.f + phase); }, false, 0, true);
  // curseur au milieu : valeur courante du LFO
  const int px = x + w / 2;
  const int py = y + (int)((0.5f - 0.42f * amp * wave(1.f + phase)) * (h - 1));
  g.drawFastVLine(px, y, h, scale(color_, 0.4f));
  g.fillCircle(px, py, 3, theme::TEXT);
  g.drawCircle(px, py, 5, color_);
}

void Effect::drawTransfer(Canvas& g, int x, int y, int w, int h,
                          const std::function<float(float)>& tf) const {
  // carre de gauche : courbe de transfert ; a droite : sinus deforme
  const int s = h;
  g.drawRect(x, y, s, s, theme::GRID);
  g.drawLine(x, y + s - 1, x + s - 1, y, theme::GRID);
  plotFn(g, x, y, s, s, color_, [&](float u) { return 0.5f + 0.5f * tf(u * 2.f - 1.f); },
         false, 0, true);
  const int wx = x + s + 4, ww = w - s - 4;
  g.drawFastHLine(wx, y + h / 2, ww, theme::GRID);
  const float ph = millis() * 0.0006f;
  plotFn(g, wx, y, ww, h, scale(theme::DIM, 0.6f),
         [&](float u) { return 0.5f + 0.45f * sinf(dsp::TWO_PI_F * (u * 2.f + ph)); });
  plotFn(g, wx, y, ww, h, color_,
         [&](float u) { return 0.5f + 0.45f * tf(sinf(dsp::TWO_PI_F * (u * 2.f + ph))); },
         false, 0, true);
}

void Effect::drawVisual(Canvas& g, int x, int y, int w, int h, const PageCtx& ctx) const {
  // Visualisation par defaut : onde animee
  drawGrid(g, x, y, w, h, false);
  plotFn(g, x, y, w, h, color_,
         [&](float u) { return 0.5f + 0.4f * sinf(dsp::TWO_PI_F * (u * 3.f + ctx.t * 0.5f)); },
         false, 0, true);
}

void Effect::drawPage(Canvas& g, const PageCtx& ctx) const {
  const int x0 = ctx.ox;
  const int top = cfg::STATUS_H;
  const int W = cfg::SCREEN_W;

  // --- En-tete : degrade de la couleur de l'effet ---
  const float glowOn = ctx.enableAnim;
  hGradient(g, x0, top, W, 16, scale(color_, 0.25f + 0.35f * glowOn), theme::BG);
  drawIcon(g, x0 + 10, top + 8, color_, ctx.t);
  g.setTextFont(2);
  shadowText(g, name_, x0 + 21, top + 8, theme::TEXT, ML_DATUM);
  if (canBypass()) pill(g, x0 + W - 28, top + 3, 24, 11, ctx.enableAnim, color_);

  // --- Zone de visualisation ---
  const int vx = x0 + 3, vy = top + 18, vw = W - 6, vh = 50;
  g.fillRoundRect(vx, vy, vw, vh, 4, theme::BG2);
  drawSpectrumGhost(g, vx + 2, vy + 2, vw - 4, vh - 4, ctx, color_);
  drawVisual(g, vx + 2, vy + 2, vw - 4, vh - 4, ctx);
  g.drawRoundRect(vx, vy, vw, vh, 4, lerpColor(theme::GRID, color_, 0.3f + 0.5f * glowOn));

  // --- Boutons rotatifs (un par potentiometre) ---
  const int ky = vy + vh + 14;
  char buf[16];
  for (int i = 0; i < MAX_PARAMS; ++i) {
    const int cx = x0 + 20 + i * 40;
    if (i >= numParams_) {
      g.drawCircle(cx, ky, 3, theme::GRID);
      continue;
    }
    const Param& pr = params_[i];
    const float gl = ctx.glow[i];
    if (gl > 0.01f) g.fillCircle(cx, ky, 13 + (int)(2 * gl), scale(color_, 0.25f * gl));
    knob(g, cx, ky, 12, pr.norm(), color_, ctx.potActive[i], ctx.potPos[i],
         gl > 0.01f ? scale(color_, 0.25f * gl) : theme::BG);
    g.setTextFont(1);
    g.setTextDatum(TC_DATUM);
    g.setTextColor(lerpColor(theme::DIM, theme::TEXT, gl));
    g.drawString(pr.name, cx, ky + 15);
    pr.format(buf, sizeof(buf));
    g.setTextColor(lerpColor(theme::TEXT, color_, ctx.potActive[i] ? 0.6f : 0.f));
    g.drawString(buf, cx, ky + 25);
  }
}
