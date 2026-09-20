// Bouchons pour les symboles fournis par EcranLCD.ino / EcranLED.ino,
// exclus de la compilation PC (dépendent de LovyanGFX / matériel).
#pragma once

#include <Arduino.h>

// Neutralise le OneWire.h livré dans le dossier du sketch (accès registres GPIO).
#include <OneWire.h>
#ifndef OneWire_h
#define OneWire_h
#endif

// --- EcranLCD ---------------------------------------------------------------
extern int8_t NumPage;
extern int8_t NbrPage;
extern bool ScreenOn;
extern bool ReDraw;

// Ecran factice : seules les méthodes appelées hors EcranLCD.ino sont fournies.
struct EcranFactice {
  int rotationDemandee = -1;
  void setRotation(int r) { rotationDemandee = r; }
};
extern EcranFactice *lcd;

void Ecran_Init(byte ESP32type);
void Ecran_Loop();
void TraceMessages();
void GoPage(int N);
void SetCouleurs();

// --- EcranLED / OLED --------------------------------------------------------
void Init_LED_OLED(void);
void Gestion_LEDs();
void PrintScroll(String m);

// Journal des messages passés à PrintScroll (utile aux tests)
extern std::vector<std::string> mock_scroll;
