#pragma once
// =====================================================================
//  Moteur audio
//   Bluetooth (PCM 16 bits) -> tampon anti-gigue -> chaine d'effets
//   -> Master -> I2S 32 bits -> PCM5102
//  Coeur 1 : tache audio (priorite haute) + IHM (priorite basse) ; coeur 0 : Bluetooth.
// =====================================================================
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <vector>
#include "Config.h"
#include "effects/Effect.h"

class MasterEffect;

class AudioEngine {
 public:
  void begin();

  // Appele par la pile Bluetooth avec des trames stereo int16
  void pushPcm(const uint8_t* data, uint32_t len);
  // Changement de frequence d'echantillonnage annonce par la source
  void requestSampleRate(uint32_t sr) { pendingRate_ = sr; }
  uint32_t sampleRate() const { return sampleRate_; }

  // --- chaine d'effets (l'ordre de la liste = l'ordre de traitement) ---
  int numEffects() const { return (int)effects_.size(); }
  Effect* effect(int i) const { return effects_[i]; }
  int indexOf(const Effect* e) const;
  void moveEffect(int from, int to);  // reordonne (thread-safe)
  int numEnabled() const;
  MasterEffect& master() { return *master_; }

  // --- mesures pour l'affichage ---
  // Copie les FFT_SIZE derniers echantillons (mono) dans l'ordre chronologique
  void copyAnalysis(float* dst) const;
  float cpuLoad() const { return cpuLoad_; }
  bool isReceiving() const { return receiving_; }
  uint32_t underruns() const { return underruns_; }
  uint32_t overruns() const { return overruns_; }

 private:
  static void taskEntry(void* arg);
  void audioLoop();
  void initI2S(uint32_t rate);

  std::vector<Effect*> effects_;
  MasterEffect* master_ = nullptr;
  portMUX_TYPE chainMux_ = portMUX_INITIALIZER_UNLOCKED;

  StreamBufferHandle_t stream_ = nullptr;
  volatile uint32_t pendingRate_ = 0;
  uint32_t sampleRate_ = (uint32_t)cfg::DEFAULT_FS;

  float analysis_[cfg::FFT_SIZE] = {};
  volatile int anaPos_ = 0;

  volatile float cpuLoad_ = 0;
  volatile bool receiving_ = false;
  volatile uint32_t underruns_ = 0, overruns_ = 0;
  volatile uint32_t lastRxMs_ = 0;
};

extern AudioEngine audio;
