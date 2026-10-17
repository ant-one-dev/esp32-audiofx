#pragma once
// =====================================================================
//  Configuration materielle et constantes globales
// =====================================================================
#include <Arduino.h>

namespace cfg {

// ---------- Bluetooth ----------
constexpr const char* BT_NAME = "ESP32-FX";

// ---------- I2S -> PCM5102 ----------
// PCM5102 : SCK -> GND (PLL interne), FMT -> GND, FLT -> GND, DEMP -> GND,
//           XSMT -> 3V3 (sortie active)
constexpr int PIN_I2S_BCK  = 26;   // BCK
constexpr int PIN_I2S_WS   = 25;   // LCK / LRCK
constexpr int PIN_I2S_DOUT = 22;   // DIN

// ---------- Potentiometres (ADC1 uniquement : ADC2 inutilisable avec le BT) ----------
constexpr int NUM_POTS = 4;
constexpr int PIN_POT[NUM_POTS] = {32, 33, 34, 35};

// ---------- Boutons (vers GND, pull-up interne) ----------
constexpr int PIN_BTN_UP   = 15;
constexpr int PIN_BTN_DOWN = 14;
constexpr int PIN_BTN_OK   = 27;
constexpr int PIN_BTN_BACK = 19;   // libre : l'ecran n'utilise pas MISO

// ---------- Audio ----------
constexpr int   BLOCK          = 128;      // trames par bloc de traitement
constexpr float DEFAULT_FS     = 44100.f;
constexpr float MAX_FS         = 48000.f;  // dimensionnement des buffers
constexpr int   FFT_SIZE       = 1024;

// ---------- Ecran ----------
constexpr int SCREEN_W = 160;
constexpr int SCREEN_H = 128;
constexpr int STATUS_H = 11;               // hauteur de la barre d'etat

// ---------- Taches FreeRTOS ----------
constexpr int AUDIO_CORE = 1;
constexpr int UI_CORE    = 1;   // avec l'audio (priorite plus basse) : le coeur 0 est charge par le Bluetooth

}  // namespace cfg
