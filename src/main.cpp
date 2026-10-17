// =====================================================================
//  ESP32 Audio FX : recepteur Bluetooth A2DP -> effets -> PCM5102
//  Carte : Freenove ESP32-WROVER-E, ecran ST7735 160x128
// =====================================================================
#include <Arduino.h>
#include "Config.h"
#include "audio/AudioEngine.h"
#include "audio/Bluetooth.h"
#include "ui/Ui.h"

void setup() {
  Serial.begin(115200);
  Serial.printf("\nESP32 Audio FX - PSRAM %s (%u Ko libres)\n", psramFound() ? "OK" : "ABSENTE",
                (unsigned)(ESP.getFreePsram() / 1024));

  ui.begin();                // ecran + animation de demarrage
  audio.begin();             // effets, I2S, tache audio (coeur 1)
  bt::begin(cfg::BT_NAME);   // recepteur A2DP
  ui.start();                // tache d'affichage (coeur 1, priorite basse)

  Serial.printf("RAM interne libre : %u Ko\n", (unsigned)(ESP.getFreeHeap() / 1024));
}

void loop() {
  // tout tourne dans les taches FreeRTOS dediees
  vTaskDelete(nullptr);
}
