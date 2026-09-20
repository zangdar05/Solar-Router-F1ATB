// Unité de compilation qui assemble le firmware pour un hôte PC.
// Les .ino sont inclus dans l'ordre imposé par l'IDE Arduino (fichier principal
// en premier), en excluant EcranLCD.ino / EcranLED.ino (dépendance LovyanGFX).
#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <FS.h>
#include <LittleFS.h>

// Implémentation des mocks (objets globaux)
#include "mock/mock_impl.cpp"

// Bouchons écran + neutralisation du OneWire local
#include "mock/esp_stubs.h"
// Prototypes normalement générés par l'IDE Arduino
#include "mock/prototypes.h"

// Fichier principal : variables globales, setup(), loop(), ISR, GestionOverproduction
#include "Solar_Router_V17_29.ino"

// Les autres onglets du sketch
#include "commonFx.ino"
#include "Stockage.ino"
#include "Heure.ino"
#include "Temperature.ino"
#include "Tempo_RTE.ino"
#include "RMS_Externes.ino"
#include "Server.ino"
#include "EnvoiMQTT.ino"
#include "Source_NotDef.ino"
#include "Source_UxI.ino"
#include "Source_UxIx2.ino"
#include "Source_UxIx3.ino"
#include "Source_Linky.ino"
#include "Source_EnphaseEnvoy.ino"
#include "Source_ShellyEm.ino"
#include "Source_ShellyProEm.ino"
#include "Source_SmartG.ino"
#include "Source_HomeWizard.ino"
#include "Source_MQTT.ino"
#include "Source_Externe.ino"

// NB: Actions.cpp est compilé comme unité séparée (Actions.h n'a pas de garde
// d'inclusion, l'inclure ici entrerait en conflit avec le .ino principal).
