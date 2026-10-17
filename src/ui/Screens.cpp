#include "Screens.h"
#include "audio/AudioEngine.h"
#include "audio/Bluetooth.h"
#include "effects/MasterEffect.h"
#include <esp_heap_caps.h>
#include <algorithm>

using namespace gfx;

SplashScreen splashScreen;
HomeScreen homeScreen;
MenuScreen menuScreen;
EffectListScreen effectListScreen;
ChainScreen chainScreen;
InfoScreen infoScreen;
ButtonTestScreen buttonTestScreen;

namespace {
constexpr int W = cfg::SCREEN_W;
constexpr int H = cfg::SCREEN_H;
constexpr int TOP = cfg::STATUS_H;

EffectScreen fxScreens[2];  // deux instances pour animer le passage d'un effet a l'autre
int fxIdx = 0;

inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
}  // namespace

void openEffect(Effect* e, int dir) {
  fxIdx ^= 1;
  EffectScreen* s = &fxScreens[fxIdx];
  s->setEffect(e);
  if (dir == 0) ui.push(s);
  else ui.replace(s, dir);
}

// =====================================================================
//  Splash
// =====================================================================
void SplashScreen::update(float dt) {
  t_ += dt;
  if (t_ > 2.8f || (t_ > 0.5f && ui.input.anyActivity())) ui.replace(&homeScreen, 1);
}

void SplashScreen::draw(Canvas& g, int ox) {
  // trois ondes colorees qui s'amplifient
  const float grow = easeOutCubic(clamp01(t_ / 1.2f));
  for (int layer = 0; layer < 3; ++layer) {
    const uint16_t col = hsv(0.55f + layer * 0.15f + t_ * 0.05f, 0.9f, 0.9f);
    int py = 0;
    for (int x = 0; x < W; ++x) {
      const float u = (float)x / W;
      const float env = sinf(dsp::PI_F * u);
      const float y = 64 + grow * env * 26.f *
                               sinf(dsp::TWO_PI_F * (u * (1.5f + layer * 0.7f)) + t_ * (2.f + layer));
      const int yy = (int)y;
      if (x) g.drawLine(ox + x - 1, py, ox + x, yy, scale(col, 0.35f + 0.25f * layer));
      py = yy;
    }
  }

  // titre : chaque lettre tombe avec un rebond
  static const char* kTitle = "AUDIO FX";
  g.setTextFont(4);
  const int tw = g.textWidth(kTitle);
  int x = ox + (W - tw) / 2;
  g.setTextDatum(TL_DATUM);
  for (int i = 0; kTitle[i]; ++i) {
    char c[2] = {kTitle[i], 0};
    const int cw = g.textWidth(c);
    const float k = clamp01((t_ - 0.25f - i * 0.08f) / 0.45f);
    if (k > 0.f) {
      const int y = 30 - (int)((1.f - easeOutBack(k)) * 40.f);
      const uint16_t col = hsv(i / 9.f + t_ * 0.15f, 0.7f, 1.f);
      g.setTextColor(TFT_BLACK);
      g.drawString(c, x + 2, y + 2);
      g.setTextColor(col);
      g.drawString(c, x, y);
    }
    x += cw;
  }

  const float sub = clamp01((t_ - 1.1f) / 0.5f);
  if (sub > 0.f) {
    g.setTextFont(2);
    g.setTextDatum(TC_DATUM);
    g.setTextColor(lerpColor(theme::BG, theme::TEXT, sub));
    g.drawString("Processeur Bluetooth", ox + W / 2, 94);
    g.setTextFont(1);
    g.setTextColor(lerpColor(theme::BG, theme::DIM, sub));
    g.drawString(cfg::BT_NAME, ox + W / 2, 110);
  }
  // barre de progression
  const int pw = (int)(clamp01(t_ / 2.6f) * (W - 40));
  g.drawRoundRect(ox + 19, 121, W - 38, 4, 2, theme::GRID);
  if (pw > 2) hGradient(g, ox + 20, 122, pw, 2, theme::ACCENT, theme::ACCENT2);
}

// =====================================================================
//  Accueil : spectre
// =====================================================================
void HomeScreen::update(float dt) {
  Input& in = ui.input;
  scroll_ += dt * 24.f;
  if (in.pressed(BTN_OK)) ui.push(&menuScreen);
  if (in.longPress(BTN_OK)) {
    ui.spectrumStyle = (ui.spectrumStyle + 1) % Spectrum::STYLE_COUNT;
    ui.toast(Spectrum::styleName(ui.spectrumStyle), theme::ACCENT2);
  }
  if (in.pressed(BTN_BACK)) { bt::playPause(); ui.toast("Lecture / Pause"); }
  if (in.pressed(BTN_UP)) { bt::next(); ui.toast("Piste suivante"); }
  if (in.pressed(BTN_DOWN)) { bt::previous(); ui.toast("Piste precedente"); }
  if (in.longPress(BTN_BACK)) {
    // bypass global : memorise puis restaure les effets actifs
    if (!bypassAll_) {
      numSaved_ = 0;
      for (int i = 0; i < audio.numEffects(); ++i) {
        Effect* e = audio.effect(i);
        if (e->enabled()) saved_[numSaved_++] = e;
        e->setEnabled(false);
      }
      bypassAll_ = true;
      ui.toast("Bypass general", theme::WARN);
    } else {
      for (int i = 0; i < numSaved_; ++i) saved_[i]->setEnabled(true);
      bypassAll_ = false;
      ui.toast("Effets restaures", theme::OK);
    }
  }
}

void HomeScreen::draw(Canvas& g, int ox) {
  const int y0 = TOP + 1, barH = 16, sh = H - barH - y0;
  ui.spectrum.draw(g, ox, y0, W, sh, ui.spectrumStyle, ui.time);

  // pastilles des effets actifs (dans l'ordre de la chaine)
  int dx = ox + W - 6;
  const float pulse = ui.spectrum.energy();
  for (int i = audio.numEffects() - 1; i >= 0; --i) {
    Effect* e = audio.effect(i);
    if (!e->enabled()) continue;
    g.fillCircle(dx, y0 + 5, 2 + (int)(pulse * 2.f), e->color());
    dx -= 8;
  }

  // invitation a l'appairage
  if (bt::state() == bt::State::Waiting) {
    const float a = 0.5f + 0.5f * sinf(ui.time * 3.f);
    g.fillRoundRect(ox + 20, 38, W - 40, 34, 8, theme::PANEL);
    g.drawRoundRect(ox + 20, 38, W - 40, 34, 8, lerpColor(theme::GRID, theme::ACCENT, a));
    btIcon(g, ox + 28, 50, lerpColor(theme::DIM, theme::ACCENT, a));
    g.setTextFont(1);
    g.setTextDatum(TL_DATUM);
    g.setTextColor(theme::DIM);
    g.drawString("Appairez :", ox + 40, 45);
    g.setTextFont(2);
    g.setTextColor(theme::TEXT);
    g.drawString(cfg::BT_NAME, ox + 40, 54);
  }

  // bandeau du bas : titre / artiste defilant
  const int by = H - barH;
  vGradient(g, ox, by, W, barH, theme::PANEL, theme::BG);
  g.drawFastHLine(ox, by, W, theme::GRID);
  static char title[64], artist[48], line[120];
  static uint32_t ver = ~0u;
  const uint32_t v = bt::metadata(title, sizeof(title), artist, sizeof(artist));
  if (v != ver) {
    ver = v;
    if (title[0] && artist[0]) snprintf(line, sizeof(line), "%s - %s", title, artist);
    else if (title[0]) strlcpy(line, title, sizeof(line));
    else line[0] = 0;
    scroll_ = 0;
  }
  const char* txt = line[0] ? line : bt::stateName();
  g.setTextFont(2);
  g.setTextColor(line[0] ? theme::TEXT : theme::DIM);
  g.setTextDatum(ML_DATUM);
  const int tw = g.textWidth(txt);
  if (tw <= W - 8) {
    g.setTextDatum(MC_DATUM);
    g.drawString(txt, ox + W / 2, by + barH / 2);
  } else {
    const int gap = 40;
    const int sx = ox + 4 - (int)fmodf(scroll_, (float)(tw + gap));
    g.drawString(txt, sx, by + barH / 2);
    g.drawString(txt, sx + tw + gap, by + barH / 2);
  }
}

// =====================================================================
//  Menu principal (carrousel)
// =====================================================================
namespace {
enum MenuItem { M_EFFECTS, M_CHAIN, M_DISPLAY, M_MASTER, M_INFO, M_COUNT };
const char* const kMenuNames[M_COUNT] = {"Effets", "Chaine", "Affichage", "Master", "Infos"};
const uint16_t kMenuCols[M_COUNT] = {rgb(0, 200, 255), rgb(60, 230, 120), rgb(255, 60, 200),
                                     rgb(230, 230, 255), rgb(255, 190, 0)};

void drawMenuIcon(Canvas& g, int item, int cx, int cy, float s, uint16_t col, float t) {
  const int r = (int)(12 * s);
  switch (item) {
    case M_EFFECTS:  // trois boutons rotatifs qui tournent
      for (int i = 0; i < 3; ++i) {
        const int kx = cx + (i - 1) * (int)(9 * s), kr = (int)(4 * s) + 1;
        g.drawCircle(kx, cy, kr, col);
        const float a = t * (1.f + i * 0.5f) + i;
        g.drawLine(kx, cy, kx + (int)(sinf(a) * kr), cy - (int)(cosf(a) * kr), col);
      }
      break;
    case M_CHAIN: {  // blocs relies, un point circule
      for (int i = 0; i < 3; ++i) {
        const int bx = cx + (i - 1) * (int)(10 * s);
        g.drawRect(bx - (int)(3 * s), cy - (int)(3 * s), (int)(6 * s) + 1, (int)(6 * s) + 1, col);
      }
      g.drawFastHLine(cx - r, cy, 2 * r, scale(col, 0.5f));
      const float u = fmodf(t * 0.7f, 1.f);
      g.fillCircle(cx - r + (int)(u * 2 * r), cy, 1 + (int)s, theme::TEXT);
      break;
    }
    case M_DISPLAY: {  // spectre miniature anime
      const float* b = ui.spectrum.bands();
      for (int i = 0; i < 6; ++i) {
        const float v = 0.25f + 0.75f * fmaxf(b[3 + i * 6], 0.3f + 0.3f * sinf(t * 3.f + i));
        const int hh = (int)(v * 2 * r);
        g.fillRect(cx - r + i * (int)(4 * s) + 1, cy + r - hh, (int)(3 * s), hh, hsv(0.55f + i * 0.06f, 0.8f, 1.f));
      }
      break;
    }
    case M_MASTER: {  // haut-parleur + ondes
      const int u = (int)(3 * s) + 1;
      g.fillRect(cx - 2 * u, cy - u, u, 2 * u, col);
      g.fillTriangle(cx - u, cy - u, cx - u, cy + u, cx + u / 2, cy - 2 * u - 1, col);
      g.fillTriangle(cx - u, cy + u, cx + u / 2, cy + 2 * u + 1, cx + u / 2, cy - 2 * u - 1, col);
      const float lv = audio.master().levelL();
      for (int k = 1; k <= 2; ++k)
        g.drawArc(cx + u, cy, u + k * (int)(4 * s), u + k * (int)(4 * s) - 1, 220, 320,
                  lerpColor(scale(col, 0.4f), col, lv * 2.f), theme::BG, false);
      break;
    }
    default: {  // info
      g.drawCircle(cx, cy, r, col);
      g.fillCircle(cx, cy - (int)(6 * s), 1 + (int)s, col);
      g.fillRect(cx - (int)s, cy - (int)(2 * s), 2 * (int)s + 1, (int)(9 * s), col);
      break;
    }
  }
}
}  // namespace

void MenuScreen::update(float dt) {
  Input& in = ui.input;
  if (in.pressed(BTN_UP)) --sel_;
  if (in.pressed(BTN_DOWN)) ++sel_;
  pos_ = approach(pos_, (float)sel_, 12.f, dt);
  bump_ = approach(bump_, 0.f, 8.f, dt);
  const int item = ((sel_ % M_COUNT) + M_COUNT) % M_COUNT;

  if (in.pressed(BTN_OK)) {
    bump_ = 1.f;
    switch (item) {
      case M_EFFECTS: ui.push(&effectListScreen); break;
      case M_CHAIN: ui.push(&chainScreen); break;
      case M_DISPLAY:
        ui.spectrumStyle = (ui.spectrumStyle + 1) % Spectrum::STYLE_COUNT;
        ui.toast(Spectrum::styleName(ui.spectrumStyle), kMenuCols[M_DISPLAY]);
        break;
      case M_MASTER: openEffect(&audio.master()); break;
      default: ui.push(&infoScreen); break;
    }
  }
  if (in.pressed(BTN_BACK)) ui.pop();
}

void MenuScreen::draw(Canvas& g, int ox) {
  const float t = ui.time;
  // particules de fond qui reagissent au niveau sonore
  const float e = ui.spectrum.energy();
  for (int i = 0; i < 28; ++i) {
    const float sx = fmodf(i * 37.7f + t * (6.f + (i % 5) * 3.f), (float)W);
    const float sy = TOP + fmodf(i * 53.3f + sinf(t * 0.5f + i) * 6.f, (float)(H - TOP));
    const uint16_t c = scale(kMenuCols[i % M_COUNT], 0.25f + 0.6f * e);
    g.fillCircle(ox + (int)sx, (int)sy, i % 3 == 0 ? 1 : 0, c);
  }

  // cartes : de la plus eloignee a la plus proche
  const int cy = 52;
  int order[M_COUNT];
  float dist[M_COUNT];
  for (int k = 0; k < M_COUNT; ++k) {
    float d = fmodf(k - pos_ + M_COUNT * 100.f + M_COUNT / 2.f, (float)M_COUNT) - M_COUNT / 2.f;
    dist[k] = d;
    order[k] = k;
  }
  std::sort(order, order + M_COUNT, [&](int a, int b) { return fabsf(dist[a]) > fabsf(dist[b]); });
  for (int oi = 0; oi < M_COUNT; ++oi) {
    const int k = order[oi];
    const float d = dist[k], ad = fabsf(d);
    if (ad > 2.2f) continue;
    const float s = 1.f - 0.36f * fminf(ad, 1.6f) + (ad < 0.5f ? 0.12f * bump_ : 0.f);
    const float a = 1.f - 0.5f * fminf(ad, 1.5f);
    const int cx = ox + W / 2 + (int)(d * 54.f);
    const int sz = (int)(46 * s);
    const uint16_t col = scale(kMenuCols[k], a);
    // halo de la carte centrale
    if (ad < 0.5f) {
      const float glow = 0.5f + 0.5f * sinf(t * 3.f);
      g.fillRoundRect(cx - sz / 2 - 3, cy - sz / 2 - 3, sz + 6, sz + 6, 10,
                      scale(kMenuCols[k], 0.12f + 0.08f * glow));
    }
    g.fillRoundRect(cx - sz / 2, cy - sz / 2, sz, sz, 8, scale(kMenuCols[k], 0.22f * a));
    g.drawRoundRect(cx - sz / 2, cy - sz / 2, sz, sz, 8, col);
    drawMenuIcon(g, k, cx, cy, s, col, t);
  }

  // libelle + description
  const int item = ((sel_ % M_COUNT) + M_COUNT) % M_COUNT;
  char desc[32];
  switch (item) {
    case M_EFFECTS: snprintf(desc, sizeof(desc), "%d effets disponibles", audio.numEffects()); break;
    case M_CHAIN: snprintf(desc, sizeof(desc), "%d effet(s) actif(s)", audio.numEnabled()); break;
    case M_DISPLAY: snprintf(desc, sizeof(desc), "Style : %s", Spectrum::styleName(ui.spectrumStyle)); break;
    case M_MASTER: {
      char v[12];
      audio.master().param(MasterEffect::P_VOL).format(v, sizeof(v));
      snprintf(desc, sizeof(desc), "Volume %s", v);
      break;
    }
    default: snprintf(desc, sizeof(desc), "%s", bt::stateName()); break;
  }
  g.setTextFont(2);
  shadowText(g, kMenuNames[item], ox + W / 2, 92, theme::TEXT, MC_DATUM);
  g.setTextFont(1);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(theme::DIM);
  g.drawString(desc, ox + W / 2, 106);
  // indicateur de position
  for (int k = 0; k < M_COUNT; ++k) {
    const int dx = ox + W / 2 + (k - M_COUNT / 2) * 9;
    if (k == item) g.fillRoundRect(dx - 3, 117, 7, 3, 1, kMenuCols[k]);
    else g.fillCircle(dx, 118, 1, theme::GRID);
  }
}

// =====================================================================
//  Liste des effets
// =====================================================================
namespace {
constexpr int ROW = 19;
constexpr int VIEW_TOP = TOP + 2;
constexpr int VIEW_H = H - VIEW_TOP;
}  // namespace

void EffectListScreen::enter() {
  for (int i = 0; i < audio.numEffects(); ++i) onAnim_[i] = audio.effect(i)->enabled();
}

void EffectListScreen::update(float dt) {
  Input& in = ui.input;
  const int n = audio.numEffects();
  if (in.pressed(BTN_UP)) sel_ = (sel_ + n - 1) % n;
  if (in.pressed(BTN_DOWN)) sel_ = (sel_ + 1) % n;
  if (in.pressed(BTN_OK)) openEffect(audio.effect(sel_));
  if (in.longPress(BTN_OK)) {
    Effect* e = audio.effect(sel_);
    e->toggle();
    char buf[40];
    snprintf(buf, sizeof(buf), "%s %s", e->name(), e->enabled() ? "ON" : "OFF");
    ui.toast(buf, e->color());
  }
  if (in.pressed(BTN_BACK)) ui.pop();

  hl_ = approach(hl_, (float)sel_, 16.f, dt);
  const float maxScroll = fmaxf(0.f, n * ROW - VIEW_H);
  const float target = dsp::clampf(sel_ * ROW - (VIEW_H - ROW) / 2.f, 0.f, maxScroll);
  scroll_ = approach(scroll_, target, 12.f, dt);
  for (int i = 0; i < n; ++i) onAnim_[i] = approach(onAnim_[i], audio.effect(i)->enabled(), 14.f, dt);
}

void EffectListScreen::draw(Canvas& g, int ox) {
  const int n = audio.numEffects();
  const float t = ui.time;
  // barre de selection : glisse et change de couleur entre les lignes
  const int i0 = (int)floorf(hl_);
  const int i1 = i0 + 1 < n ? i0 + 1 : i0;
  const uint16_t hc = lerpColor(audio.effect(i0)->color(), audio.effect(i1)->color(), hl_ - i0);
  const int hy = VIEW_TOP + (int)(hl_ * ROW - scroll_);
  g.fillRoundRect(ox + 2, hy, W - 8, ROW - 1, 5, scale(hc, 0.28f));
  g.fillRoundRect(ox + 2, hy + 3, 3, ROW - 7, 1, hc);

  for (int i = 0; i < n; ++i) {
    const int ry = VIEW_TOP + (int)(i * ROW - scroll_);
    if (ry < VIEW_TOP - ROW || ry > H) continue;
    Effect* e = audio.effect(i);
    const bool selected = i == sel_;
    const float on = onAnim_[i];
    const uint16_t ic = lerpColor(scale(e->color(), 0.45f), e->color(), fmaxf(on, selected ? 1.f : 0.f));
    e->drawIcon(g, ox + 15, ry + ROW / 2, ic, selected ? t : 0.f);
    g.setTextFont(2);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(selected ? theme::TEXT : lerpColor(theme::DIM, theme::TEXT, on));
    g.drawString(e->name(), ox + 27 + (selected ? 2 : 0), ry + ROW / 2);
    pill(g, ox + W - 33, ry + 4, 22, 10, on, e->color());
  }

  // barre de defilement
  const int trackH = VIEW_H - 4;
  const int thumbH = trackH * VIEW_H / (n * ROW);
  const float maxScroll = fmaxf(1.f, n * ROW - VIEW_H);
  const int thumbY = VIEW_TOP + 2 + (int)((trackH - thumbH) * scroll_ / maxScroll);
  g.drawFastVLine(ox + W - 3, VIEW_TOP + 2, trackH, theme::GRID);
  g.fillRect(ox + W - 4, thumbY, 3, thumbH, hc);
}

// =====================================================================
//  Chaine d'effets
// =====================================================================
int ChainScreen::collect(Effect** out) const {
  int n = 0;
  for (int i = 0; i < audio.numEffects(); ++i)
    if (audio.effect(i)->enabled()) out[n++] = audio.effect(i);
  return n;
}

void ChainScreen::enter() {
  grab_ = false;
  grabAnim_ = 0;
  Effect* list[32];
  const int n = collect(list);
  if (sel_ >= n) sel_ = n > 0 ? n - 1 : 0;
  pos_ = (float)sel_;
}

void ChainScreen::update(float dt) {
  Input& in = ui.input;
  Effect* list[32];
  const int n = collect(list);
  if (sel_ >= n) sel_ = n > 0 ? n - 1 : 0;

  if (n > 0) {
    const int delta = in.pressed(BTN_DOWN) ? 1 : (in.pressed(BTN_UP) ? -1 : 0);
    if (delta) {
      const int to = sel_ + delta;
      if (to >= 0 && to < n) {
        if (grab_) audio.moveEffect(audio.indexOf(list[sel_]), audio.indexOf(list[to]));
        sel_ = to;
      }
    }
    if (in.pressed(BTN_OK) && n > 1) {
      grab_ = !grab_;
      ui.toast(grab_ ? "Deplacer : HAUT / BAS" : "Pose !", grab_ ? theme::WARN : theme::OK);
    }
    if (in.longPress(BTN_OK)) { grab_ = false; openEffect(list[sel_]); }
  }
  if (in.pressed(BTN_BACK)) {
    if (grab_) grab_ = false;
    else ui.pop();
  }
  pos_ = approach(pos_, (float)sel_, 10.f, dt);
  grabAnim_ = approach(grabAnim_, grab_ ? 1.f : 0.f, 12.f, dt);
}

void ChainScreen::draw(Canvas& g, int ox) {
  Effect* list[32];
  const int n = collect(list);
  const float t = ui.time;
  const int cy = 54;
  auto xOf = [&](float k) { return ox + W / 2 + (int)((k - pos_) * 42.f); };

  // ligne du signal IN -> OUT avec impulsions qui circulent
  const int xIn = xOf(-1.f), xOut = xOf((float)n);
  g.drawFastHLine(xIn, cy, xOut - xIn, theme::GRID);
  const float speed = 0.6f + 2.f * ui.spectrum.energy();
  for (int k = -1; k < n; ++k) {
    const int a = xOf((float)k), b = xOf(k + 1.f);
    for (int j = 0; j < 2; ++j) {
      const float u = fmodf(t * speed + j * 0.5f + k * 0.17f + 10.f, 1.f);
      const uint16_t c = k >= 0 ? list[k]->color() : theme::ACCENT;
      g.fillCircle(a + (int)((b - a) * u), cy, 1, lerpColor(c, theme::TEXT, 0.3f));
    }
  }
  // extremites
  g.setTextFont(1);
  g.setTextDatum(MC_DATUM);
  for (int side = 0; side < 2; ++side) {
    const int x = side ? xOut : xIn;
    g.fillCircle(x, cy, 9, theme::PANEL);
    g.drawCircle(x, cy, 9, theme::DIM);
    g.setTextColor(theme::DIM);
    g.drawString(side ? "OUT" : "IN", x, cy + 1);
  }

  if (n == 0) {
    g.setTextFont(2);
    g.setTextDatum(MC_DATUM);
    g.setTextColor(theme::TEXT);
    g.drawString("Aucun effet actif", ox + W / 2, 88);
    g.setTextFont(1);
    g.setTextColor(theme::DIM);
    g.drawString("Activez-en dans Effets", ox + W / 2, 104);
    return;
  }

  for (int k = 0; k < n; ++k) {
    const int x = xOf((float)k);
    if (x < ox - 20 || x > ox + W + 20) continue;
    const bool selected = k == sel_;
    Effect* e = list[k];
    int sz = selected ? 30 : 24;
    int y = cy;
    if (selected && grabAnim_ > 0.01f) {
      y -= (int)(7.f * grabAnim_);
      sz += (int)(2.f * sinf(t * 18.f) * grabAnim_);
      g.fillRoundRect(x - sz / 2 + 2, cy - sz / 2 + 5, sz, sz, 6, theme::BG2);  // ombre
    }
    g.fillRoundRect(x - sz / 2, y - sz / 2, sz, sz, 6, scale(e->color(), selected ? 0.4f : 0.22f));
    g.drawRoundRect(x - sz / 2, y - sz / 2, sz, sz, 6, selected ? theme::TEXT : e->color());
    e->drawIcon(g, x, y, e->color(), selected ? t : 0.f);
    char num[4];
    snprintf(num, sizeof(num), "%d", k + 1);
    g.setTextFont(1);
    g.setTextDatum(TC_DATUM);
    g.setTextColor(theme::DIM);
    g.drawString(num, x, cy + 19);
  }

  Effect* s = list[sel_];
  g.setTextFont(2);
  shadowText(g, s->name(), ox + W / 2, 92, s->color(), MC_DATUM);
  g.setTextFont(1);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(theme::DIM);
  g.drawString(grab_ ? "HAUT/BAS: deplacer  OK: poser" : "OK: deplacer  OK long: regler",
               ox + W / 2, 110);
}

// =====================================================================
//  Page d'un effet
// =====================================================================
void EffectScreen::enter() {
  ctx_ = PageCtx();
  ctx_.enableAnim = fx_->enabled() ? 1.f : 0.f;
  ctx_.spectrum = ui.spectrum.bands();
  ctx_.numBands = Spectrum::NB;
}

void EffectScreen::update(float dt) {
  Input& in = ui.input;
  // "prise au vol" : un potentiometre ne pilote le parametre que lorsqu'on le tourne
  for (int i = 0; i < fx_->numParams(); ++i) {
    float v;
    if (in.potMoved(i, v)) {
      ctx_.potActive[i] = true;
      fx_->setParamNorm(i, v);
      ctx_.glow[i] = 1.f;
    }
    ctx_.potPos[i] = in.pot(i);
    ctx_.glow[i] = approach(ctx_.glow[i], 0.f, 2.5f, dt);
  }
  ctx_.enableAnim = approach(ctx_.enableAnim, fx_->enabled() ? 1.f : 0.f, 14.f, dt);

  char buf[40];
  if (in.pressed(BTN_OK) && fx_->canBypass()) {
    fx_->toggle();
    snprintf(buf, sizeof(buf), "%s %s", fx_->name(), fx_->enabled() ? "ON" : "OFF");
    ui.toast(buf, fx_->color());
  }
  if (in.longPress(BTN_OK)) {
    fx_->resetParams();
    for (bool& a : ctx_.potActive) a = false;
    ui.toast("Valeurs par defaut", fx_->color());
  }
  // HAUT / BAS : effet precedent / suivant (avec glissement)
  if (fx_->canBypass() && (in.pressed(BTN_UP) || in.pressed(BTN_DOWN))) {
    const int n = audio.numEffects();
    const int dir = in.pressed(BTN_DOWN) ? 1 : -1;
    const int next = (audio.indexOf(fx_) + dir + n) % n;
    effectListScreen.select(next);
    openEffect(audio.effect(next), dir);
    return;
  }
  if (in.pressed(BTN_BACK)) ui.pop();
}

void EffectScreen::draw(Canvas& g, int ox) {
  ctx_.ox = ox;
  ctx_.t = ui.time;
  fx_->drawPage(g, ctx_);
}

// =====================================================================
//  Infos systeme
// =====================================================================
void InfoScreen::update(float dt) {
  cpu_ = approach(cpu_, audio.cpuLoad(), 4.f, dt);
  if (ui.input.pressed(BTN_BACK) || ui.input.pressed(BTN_OK)) ui.pop();
}

void InfoScreen::draw(Canvas& g, int ox) {
  char v[32];
  int y = TOP + 4;
  auto line = [&](const char* label, const char* value, uint16_t col) {
    g.setTextFont(1);
    g.setTextDatum(TL_DATUM);
    g.setTextColor(theme::DIM);
    g.drawString(label, ox + 6, y);
    g.setTextDatum(TR_DATUM);
    g.setTextColor(col);
    g.drawString(value, ox + W - 6, y);
    y += 11;
  };

  line("Nom Bluetooth", cfg::BT_NAME, theme::TEXT);
  const bt::State s = bt::state();
  line("Etat", bt::stateName(), s == bt::State::Playing ? theme::OK : (s == bt::State::Waiting ? theme::DIM : theme::ACCENT));
  snprintf(v, sizeof(v), "%u Hz", (unsigned)audio.sampleRate());
  line("Echantillonnage", v, theme::TEXT);

  // charge CPU avec barre animee
  snprintf(v, sizeof(v), "%d%%", (int)(cpu_ * 100.f));
  const uint16_t cc = cpu_ < 0.6f ? theme::OK : (cpu_ < 0.85f ? theme::WARN : theme::BAD);
  line("CPU audio", v, cc);
  g.drawRect(ox + 6, y - 2, W - 12, 5, theme::GRID);
  hGradient(g, ox + 7, y - 1, (int)((W - 14) * clamp01(cpu_)), 3, theme::OK, cc);
  y += 6;

  snprintf(v, sizeof(v), "%d / %d", audio.numEnabled(), audio.numEffects());
  line("Effets actifs", v, theme::TEXT);
  snprintf(v, sizeof(v), "%u Ko", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  line("RAM interne libre", v, theme::TEXT);
  snprintf(v, sizeof(v), "%u Ko", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  line("PSRAM libre", v, theme::TEXT);
  snprintf(v, sizeof(v), "%u / %u", (unsigned)audio.underruns(), (unsigned)audio.overruns());
  line("Coupures (vide/plein)", v, theme::TEXT);
  const uint32_t up = millis() / 1000;
  snprintf(v, sizeof(v), "%02u:%02u:%02u  %dfps", (unsigned)(up / 3600), (unsigned)(up / 60 % 60),
           (unsigned)(up % 60), (int)ui.fps);
  line("Uptime", v, theme::TEXT);
}

// =====================================================================
//  Test des boutons : lecture directe des broches, sans anti-rebond
// =====================================================================
namespace {
struct TestPin { const char* name; int gpio; };
const TestPin kTestPins[ButtonTestScreen::N] = {
    {"HAUT", cfg::PIN_BTN_UP}, {"BAS", cfg::PIN_BTN_DOWN}, {"OK", cfg::PIN_BTN_OK},
    {"RETOUR", cfg::PIN_BTN_BACK}, {"BOOT", 0}, {"GPIO13", 13}};
}  // namespace

void ButtonTestScreen::enter() {
  // GPIO 0 (bouton BOOT de la carte) et GPIO 13 ne sont pas configures ailleurs
  pinMode(0, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  for (int i = 0; i < N; ++i) { count_[i] = 0; low_[i] = false; }
}

void ButtonTestScreen::update(float dt) {
  for (int i = 0; i < N; ++i) {
    const bool low = digitalRead(kTestPins[i].gpio) == LOW;
    if (low && !low_[i]) { ++count_[i]; flash_[i] = 1.f; }
    low_[i] = low;
    flash_[i] = approach(flash_[i], 0.f, 5.f, dt);
  }
  if (ui.input.longPress(BTN_BACK)) ui.replace(&homeScreen, 1);
}

void ButtonTestScreen::draw(Canvas& g, int ox) {
  const int cw = 50, ch = 44, gx = 3, gy = TOP + 4;
  char buf[16];
  for (int i = 0; i < N; ++i) {
    const int x = ox + 3 + (i % 3) * (cw + gx), y = gy + (i / 3) * (ch + 4);
    const uint16_t bg = low_[i] ? theme::OK : lerpColor(theme::PANEL, theme::OK, flash_[i] * 0.5f);
    g.fillRoundRect(x, y, cw, ch, 6, bg);
    g.drawRoundRect(x, y, cw, ch, 6, low_[i] ? theme::TEXT : theme::GRID);
    const uint16_t fg = low_[i] ? theme::BG : theme::TEXT;
    g.setTextFont(2);
    g.setTextDatum(TC_DATUM);
    g.setTextColor(fg);
    g.drawString(kTestPins[i].name, x + cw / 2, y + 3);
    g.setTextFont(1);
    snprintf(buf, sizeof(buf), "GPIO %d", kTestPins[i].gpio);
    g.setTextColor(low_[i] ? theme::BG : theme::DIM);
    g.drawString(buf, x + cw / 2, y + 20);
    snprintf(buf, sizeof(buf), "x%u", (unsigned)count_[i]);
    g.setTextColor(fg);
    g.drawString(buf, x + cw / 2, y + 32);
  }
  g.setTextFont(1);
  g.setTextDatum(BC_DATUM);
  g.setTextColor(theme::DIM);
  g.drawString("RETOUR long : quitter", ox + W / 2, H - 2);
}
