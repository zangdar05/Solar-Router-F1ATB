// Données décodées d'une TIC Linky (mode standard).
// Une instance pour la source principale (Source == "Linky", MySerial) et une pour le
// Linky auxiliaire (LinkyAux == 1, SerialAux) : lecture seule, remontée MQTT, tarif/Tempo.
#pragma once
#include <Arduino.h>

struct TicData {
  // Index et puissances apparentes lus dans la trame
  long EAST = 0, EAIT = 0;  // Wh soutirés / injectés (index totaux)
  long EASF[10] = { 0 };    // Wh par index tarifaire EASF01..EASF10
  int SINSTS = 0, SINSTI = 0;                       // VA soutirés / injectés
  int SINSTS1 = 0, SINSTS2 = 0, SINSTS3 = 0;        // VA par phase (triphasé)
  int URMS1 = 0, URMS2 = 0, URMS3 = 0;              // V
  int IRMS1 = 0, IRMS2 = 0, IRMS3 = -1;             // A (IRMS3 = -1 : monophasé)
  String LTARF, NGTF, STGE, DATE;

  // Estimation de la puissance active (W) à partir de la dérivée des index
  float moyPWS = 0, moyPWI = 0, moyPVAS = 0, moyPVAI = 0;
  float deltaWS = 0, deltaWI = 0;
  float COSphiS = 1, COSphiI = 1;
  unsigned long TlastEAST = 0, TlastEAIT = 0;
  int PuissanceS = 0, PuissanceI = 0;  // W estimés
  int PVAS = 0, PVAI = 0;              // VA bornés
  bool EASTvalid = false, EAITvalid = false;

  // Diagnostic
  unsigned long lastFrameMs = 0;  // millis() du dernier groupe DATE reçu
  unsigned long nbTrames = 0;
  unsigned long nbErrChecksum = 0;

  // Tampon circulaire de réception (fourni par l'appelant)
  volatile char *buf = nullptr;
  int bufSize = 0;
  volatile int idxRaw = 0;    // prochaine écriture
  volatile int idxDecod = 0;  // début du groupe en cours
  bool LFon = false;
  int boucles = 0;  // compteur d'appels sans donnée (surveillance)
};

// Prototypes explicites : le générateur de prototypes Arduino/PlatformIO ignore
// les fonctions renvoyant une référence.
TicData &TicPourMQTT();
bool LinkyDisponible();
