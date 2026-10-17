#include "AudioEngine.h"
#include <driver/i2s.h>
#include "effects/AllEffects.h"

AudioEngine audio;

namespace {
// I2S_NUM_1 : la librairie A2DP reconfigure l'horloge de I2S_NUM_0 a chaque connexion
constexpr i2s_port_t I2S_PORT = I2S_NUM_1;
constexpr size_t FRAME_BYTES = 4;                          // stereo int16
constexpr size_t STREAM_BYTES = 32 * 1024;                 // ~185 ms a 44.1 kHz
constexpr size_t PREFILL_BYTES = 8 * 1024;                 // ~46 ms avant de demarrer
constexpr size_t HIGH_WATER = STREAM_BYTES * 3 / 4;        // derive d'horloge : on jette
}  // namespace

void AudioEngine::begin() {
  // ----- creation de tous les effets (ordre initial de la chaine) -----
  effects_ = {
      new HighPassEffect(),  new LowPassEffect(),    new BandPassEffect(), new Eq3Effect(),
      new CompressorEffect(), new DistortionEffect(), new BitcrusherEffect(),
      new RingModEffect(),   new TremoloEffect(),    new AutoPanEffect(),  new PhaserEffect(),
      new FlangerEffect(),   new ChorusEffect(),     new DelayEffect(),    new ReverbEffect(),
  };
  master_ = new MasterEffect();
  for (Effect* e : effects_) e->prepare(sampleRate_);
  master_->prepare(sampleRate_);

  stream_ = xStreamBufferCreate(STREAM_BYTES, cfg::BLOCK * FRAME_BYTES);
  initI2S(sampleRate_);

  xTaskCreatePinnedToCore(taskEntry, "audio", 8192, this, configMAX_PRIORITIES - 2, nullptr,
                          cfg::AUDIO_CORE);
}

void AudioEngine::initI2S(uint32_t rate) {
  i2s_config_t conf = {};
  conf.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  conf.sample_rate = rate;
  conf.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  conf.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  conf.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  conf.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  conf.dma_buf_count = 8;
  conf.dma_buf_len = 256;
  conf.use_apll = true;           // horloge audio precise
  conf.tx_desc_auto_clear = true; // silence en cas de sous-alimentation
  esp_err_t err = i2s_driver_install(I2S_PORT, &conf, 0, nullptr);
  if (err != ESP_OK) Serial.printf("I2S : echec de l'installation (%d)\n", err);

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = cfg::PIN_I2S_BCK;
  pins.ws_io_num = cfg::PIN_I2S_WS;
  pins.data_out_num = cfg::PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

void AudioEngine::pushPcm(const uint8_t* data, uint32_t len) {
  if (!stream_) return;
  lastRxMs_ = millis();
  if (xStreamBufferSpacesAvailable(stream_) < len) { ++overruns_; return; }
  xStreamBufferSend(stream_, data, len, 0);
}

int AudioEngine::indexOf(const Effect* e) const {
  for (int i = 0; i < (int)effects_.size(); ++i)
    if (effects_[i] == e) return i;
  return -1;
}

void AudioEngine::moveEffect(int from, int to) {
  const int n = (int)effects_.size();
  if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
  portENTER_CRITICAL(&chainMux_);
  Effect* e = effects_[from];
  effects_.erase(effects_.begin() + from);
  effects_.insert(effects_.begin() + to, e);
  portEXIT_CRITICAL(&chainMux_);
}

int AudioEngine::numEnabled() const {
  int n = 0;
  for (Effect* e : effects_) n += e->enabled();
  return n;
}

void AudioEngine::copyAnalysis(float* dst) const {
  const int pos = anaPos_;
  const int first = cfg::FFT_SIZE - pos;
  memcpy(dst, analysis_ + pos, first * sizeof(float));
  memcpy(dst + first, analysis_, pos * sizeof(float));
}

void AudioEngine::taskEntry(void* arg) { static_cast<AudioEngine*>(arg)->audioLoop(); }

void AudioEngine::audioLoop() {
  static int16_t raw[cfg::BLOCK * 2];
  static int32_t out[cfg::BLOCK * 2];
  static float L[cfg::BLOCK], R[cfg::BLOCK];
  static Effect* chain[32];
  constexpr size_t BLOCK_BYTES = cfg::BLOCK * FRAME_BYTES;
  bool playing = false;

  for (;;) {
    // ----- changement de frequence d'echantillonnage -----
    const uint32_t pr = pendingRate_;
    if (pr && pr != sampleRate_) {
      sampleRate_ = pr;
      i2s_set_sample_rates(I2S_PORT, pr);
      for (Effect* e : effects_) e->prepare(pr);
      master_->prepare(pr);
    }
    pendingRate_ = 0;

    // ----- lecture du tampon anti-gigue -----
    size_t avail = xStreamBufferBytesAvailable(stream_);
    if (!playing && avail >= PREFILL_BYTES) playing = true;
    if (avail > HIGH_WATER) {  // la source va plus vite que le DAC : on rattrape
      xStreamBufferReceive(stream_, raw, BLOCK_BYTES, 0);
      ++overruns_;
    }
    size_t got = 0;
    if (playing) {
      got = xStreamBufferReceive(stream_, raw, BLOCK_BYTES, 0);
      if (got < BLOCK_BYTES) { playing = false; if (receiving_) ++underruns_; }
    }
    if (got < BLOCK_BYTES) memset((uint8_t*)raw + got, 0, BLOCK_BYTES - got);
    receiving_ = (millis() - lastRxMs_) < 300;

    const uint32_t t0 = micros();

    // ----- int16 -> float -----
    for (int i = 0; i < cfg::BLOCK; ++i) {
      L[i] = raw[2 * i] * (1.f / 32768.f);
      R[i] = raw[2 * i + 1] * (1.f / 32768.f);
    }

    // ----- chaine d'effets (copie protegee de la liste) -----
    portENTER_CRITICAL(&chainMux_);
    const int n = (int)effects_.size();
    for (int i = 0; i < n; ++i) chain[i] = effects_[i];
    portEXIT_CRITICAL(&chainMux_);
    for (int i = 0; i < n; ++i) chain[i]->run(L, R, cfg::BLOCK);
    master_->run(L, R, cfg::BLOCK);

    // ----- float -> int32 pour le DAC + capture pour la FFT -----
    int ap = anaPos_;
    for (int i = 0; i < cfg::BLOCK; ++i) {
      const float l = dsp::clampf(L[i], -1.f, 1.f), r = dsp::clampf(R[i], -1.f, 1.f);
      out[2 * i] = (int32_t)(l * 2147483000.f);
      out[2 * i + 1] = (int32_t)(r * 2147483000.f);
      analysis_[ap] = 0.5f * (l + r);
      if (++ap >= cfg::FFT_SIZE) ap = 0;
    }
    anaPos_ = ap;

    // charge CPU = temps de calcul / duree du bloc
    const float blockUs = cfg::BLOCK * 1e6f / sampleRate_;
    cpuLoad_ = cpuLoad_ * 0.95f + 0.05f * ((micros() - t0) / blockUs);

    size_t written;
    i2s_write(I2S_PORT, out, sizeof(out), &written, portMAX_DELAY);  // cadence la boucle
  }
}
