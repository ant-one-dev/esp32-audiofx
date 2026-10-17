#include "Bluetooth.h"
#include <BluetoothA2DPSink.h>
#include "AudioEngine.h"

namespace bt {
namespace {

BluetoothA2DPSink sink;
volatile State st = State::Waiting;
volatile bool playing = false;
portMUX_TYPE metaMux = portMUX_INITIALIZER_UNLOCKED;
char title[64] = "";
char artist[48] = "";
volatile uint32_t metaVersion = 0;

void onData(const uint8_t* data, uint32_t len) { audio.pushPcm(data, len); }

void onRate(uint16_t rate) { audio.requestSampleRate(rate); }

void onConnection(esp_a2d_connection_state_t s, void*) {
  switch (s) {
    case ESP_A2D_CONNECTION_STATE_CONNECTED: st = playing ? State::Playing : State::Connected; break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING: st = State::Connecting; break;
    default:
      st = State::Waiting;
      playing = false;
      portENTER_CRITICAL(&metaMux);
      title[0] = artist[0] = 0;
      ++metaVersion;
      portEXIT_CRITICAL(&metaMux);
      break;
  }
}

void onAudioState(esp_a2d_audio_state_t s, void*) {
  playing = (s == ESP_A2D_AUDIO_STATE_STARTED);
  if (st == State::Connected || st == State::Playing) st = playing ? State::Playing : State::Connected;
}

void onMetadata(uint8_t id, const uint8_t* text) {
  portENTER_CRITICAL(&metaMux);
  if (id == ESP_AVRC_MD_ATTR_TITLE) strlcpy(title, (const char*)text, sizeof(title));
  else if (id == ESP_AVRC_MD_ATTR_ARTIST) strlcpy(artist, (const char*)text, sizeof(artist));
  ++metaVersion;
  portEXIT_CRITICAL(&metaMux);
}

}  // namespace

void begin(const char* name) {
  sink.set_task_core(0);                  // le coeur 1 est reserve au traitement audio
  sink.set_stream_reader(onData, false);  // false : on gere l'I2S nous-memes
  sink.set_sample_rate_callback(onRate);
  sink.set_on_connection_state_changed(onConnection);
  sink.set_on_audio_state_changed(onAudioState);
  sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST);
  sink.set_avrc_metadata_callback(onMetadata);
  sink.start(name, true);  // reconnexion automatique au dernier appareil
}

State state() { return st; }

const char* stateName() {
  switch (st) {
    case State::Connecting: return "Connexion...";
    case State::Connected: return "Connecte";
    case State::Playing: return "Lecture";
    default: return "En attente";
  }
}

uint32_t metadata(char* t, size_t tlen, char* a, size_t alen) {
  portENTER_CRITICAL(&metaMux);
  strlcpy(t, title, tlen);
  strlcpy(a, artist, alen);
  const uint32_t v = metaVersion;
  portEXIT_CRITICAL(&metaMux);
  return v;
}

void playPause() {
  if (playing) sink.pause(); else sink.play();
}
void next() { sink.next(); }
void previous() { sink.previous(); }

}  // namespace bt
