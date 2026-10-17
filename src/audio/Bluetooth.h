#pragma once
// Recepteur Bluetooth A2DP + AVRCP (metadonnees, commandes de lecture)
#include <Arduino.h>

namespace bt {

enum class State : uint8_t { Waiting, Connecting, Connected, Playing };

void begin(const char* name);
State state();
const char* stateName();
// Copie titre / artiste (thread-safe). Renvoie le compteur de version.
uint32_t metadata(char* title, size_t tlen, char* artist, size_t alen);

void playPause();
void next();
void previous();

}  // namespace bt
