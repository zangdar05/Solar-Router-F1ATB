// ============================================================================
//  Tests de non-régression du firmware routeur solaire F1ATB V17.29
//  Mini-framework maison : CHECK / CHECK_EQ, code retour != 0 si échec.
//  Les tests décrivent le comportement ACTUEL du firmware, y compris lorsqu'il
//  semble erroné (marqué "BUG PROBABLE"). Aucun fichier du firmware n'est modifié.
// ============================================================================
#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "mock/esp_stubs.h"
#include "mock/firmware_globals.h"
#include "mock/prototypes.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Mini framework
// ---------------------------------------------------------------------------
static int g_fail = 0;
static int g_checks = 0;
static const char *g_test = "";
static int g_test_fail = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_checks++;                                                                \
    if (!(cond)) {                                                             \
      g_test_fail++; g_fail++;                                                 \
      printf("      ! %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond);         \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    g_checks++;                                                                \
    if (!((a) == (b))) {                                                       \
      g_test_fail++; g_fail++;                                                 \
      printf("      ! %s:%d  CHECK_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b); \
    }                                                                          \
  } while (0)

#define CHECK_STR(a, b)                                                        \
  do {                                                                         \
    g_checks++;                                                                \
    std::string _x = std::string((a).c_str()), _y = std::string(b);            \
    if (_x != _y) {                                                            \
      g_test_fail++; g_fail++;                                                 \
      printf("      ! %s:%d  CHECK_STR(%s) : \"%s\" != \"%s\"\n",              \
             __FILE__, __LINE__, #a, _x.c_str(), _y.c_str());                  \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                  \
  do {                                                                         \
    g_checks++;                                                                \
    if (std::fabs(double(a) - double(b)) > (tol)) {                            \
      g_test_fail++; g_fail++;                                                 \
      printf("      ! %s:%d  CHECK_NEAR(%s=%g, %g)\n", __FILE__, __LINE__,     \
             #a, double(a), double(b));                                        \
    }                                                                          \
  } while (0)

static void begin_test(const char *nom) { g_test = nom; g_test_fail = 0; }
static void end_test() { printf("[%s] %s\n", g_test_fail ? "FAIL" : " OK " , g_test); }
#define RUN(fn)                                                                \
  do { begin_test(#fn); fn(); end_test(); } while (0)

// ---------------------------------------------------------------------------
// Remise à zéro de l'état global partagé entre tests
// ---------------------------------------------------------------------------
static void reset_commun() {
  mock_set_millis(1000);
  mock_reset_gpio();
  mock_fs_reset();
  mock_client_connect_ok = false;
  mock_client_response.clear();
  mock_client_request.clear();
  mock_mqtt_published.clear();
  mock_mqtt_subscribed.clear();
  mock_mqtt_connected = false;
  mock_mqtt_connect_ok = true;
  mock_last_http_body = "";
  mock_scroll.clear();
  Serial.mock_clear();
  MySerial.mock_clear();

  DATE = "";
  DateAMJ = "";
  Record_Conf = "";
  LastRecordConf = true;  // évite LitLastRecord_Conf() au fil des tests
  idxMessage = 0;
  for (int i = 0; i < 10; i++) MessageH[i] = "";

  Source = "NotDef";
  Source_data = "NotDef";
  ModePara = 1;
  ModeReseau = 0;
  ESP32_Type = 1;
  TempoRTEon = 0;
  Horloge = 0;
  HeureValide = false;
  EnergieActiveValide = false;
  PuissanceRecue = false;
  Pva_valide = false;
  LissageLong = false;
  PmaxReseau = 36000;
  OffsetP = 0;
  ReacCACSI = 1;
  LTARFbin = 0;
  ITmode = 5;
  pTriac = 0;
  pSerial = 0;
  NbActions = 0;
  Discovered = false;

  init_puissance();
  InitTemperature();

  for (int i = 0; i < LES_ACTIONS_LENGTH; i++) {
    LesActions[i] = Action(i);
    LesActions[i].NbPeriode = 0;
    LesActions[i].Titre = "";
    Retard[i] = 100;
    RetardF[i] = 100.0;
    IntegrErrorPw[i] = 100.0;
    LastErrorPw[i] = 0;
    Propor[i] = 0;
    DeriveF[i] = 0;
    Actif[i] = 0;
    Gpio[i] = -1;
    OutOn[i] = 1;
    OutOff[i] = 0;
    PulseOn[i] = 0;
    PulseTotal[i] = 100;
    PulseComptage[i] = 0;
  }
}

// ===========================================================================
// 1. Décodage d'une trame Linky (TIC mode standard)
// ===========================================================================

// Checksum TIC : somme des octets du label et de la valeur + les 2 tabulations,
// & 0x3F puis + 0x20. C'est exactement ce que recalcule LectureLinky().
static char tic_chk(const std::string &label, const std::string &val) {
  int c = 0;
  for (char x : label) c += (unsigned char)x;
  for (char x : val) c += (unsigned char)x;
  c += 18;  // 2 tabulations (0x09 * 2)
  c = (c & 0x3F) + 0x20;
  return (char)c;
}
// Groupe standard : LF label TAB valeur TAB checksum CR
static std::string tic_grp(const std::string &label, const std::string &val, int delta = 0) {
  std::string g = "\n" + label + "\t" + val + "\t";
  g += (char)(tic_chk(label, val) + delta);
  g += "\r";
  return g;
}
// Groupe horodaté (DATE) : LF DATE TAB horodate TAB TAB checksum CR
static std::string tic_grp_date(const std::string &horodate) {
  std::string g = "\nDATE\t" + horodate + "\t\t";
  g += (char)tic_chk("DATE", horodate);
  g += "\r";
  return g;
}

static std::string trame_linky(long east, long eait, int sinsts, int sinsti,
                               int chk_delta_east = 0) {
  std::string t = "\x02";
  t += tic_grp("ADSC", "021728123456");
  t += tic_grp("VTIC", "02");
  t += tic_grp_date("H250920120000");
  t += tic_grp("NGTF", "     BASE       ");
  t += tic_grp("LTARF", "    HC BLEU     ");
  t += tic_grp("EAST", std::to_string(east), chk_delta_east);
  t += tic_grp("EASF01", "001111111");
  t += tic_grp("EASF02", "002222222");
  t += tic_grp("EASF03", "003333333");
  t += tic_grp("EASF04", "004444444");
  t += tic_grp("EASF05", "005555555");
  t += tic_grp("EASF06", "006666666");
  t += tic_grp("EASF07", "007777777");
  t += tic_grp("EASF08", "008888888");
  t += tic_grp("EASF09", "009999999");
  t += tic_grp("EASF10", "001010101");
  t += tic_grp("EAIT", std::to_string(eait));
  t += tic_grp("IRMS1", "012");
  t += tic_grp("URMS1", "235");
  t += tic_grp("SINSTS", std::to_string(sinsts));
  t += tic_grp("SINSTI", std::to_string(sinsti));
  t += tic_grp("STGE", "1A3B0001");
  t += "\x03";
  return t;
}

static void reset_linky() {
  reset_commun();
  Source = "Linky";
  pSerial = 1;
  LFon = false;
  EASTvalid = false;
  EAITvalid = false;
  IdxDataRawLinky = 0;
  IdxBufDecodLinky = 0;
  Energie_M_Soutiree = 0;
  Energie_M_Injectee = 0;
  moyPWS = 0; moyPWI = 0; moyPVAS = 0; moyPVAI = 0;
  COSphiS = 1; COSphiI = 1;
  TlastEASTvalide = 0; TlastEAITvalide = 0;
  LTARF = ""; NGTF = ""; STGE = ""; STGEt = "";
  EASF01 = EASF02 = EASF03 = EASF04 = EASF05 = 0;
  EASF06 = EASF07 = EASF08 = EASF09 = EASF10 = 0;
  Tension_M = 0; Intensite_M = 0;
}

static void test_linky_trame_valide() {
  reset_linky();
  Setup_Linky();
  CHECK(MySerial.started);
  CHECK_EQ(Serial2V, 9600u);

  std::string t1 = trame_linky(1000000, 200000, 1200, 0);
  MySerial.mock_feed(t1);
  mock_set_millis(20000);
  LectureLinky();

  // 2e trame, millis avance, index d'énergie qui progresse
  std::string t2 = trame_linky(1000010, 200005, 1300, 400);
  MySerial.mock_feed(t2);
  mock_set_millis(22000);
  LectureLinky();

  CHECK_EQ(Energie_M_Soutiree, 1000010L);
  CHECK_EQ(Energie_M_Injectee, 200005L);
  CHECK_EQ(PVAS_M, 1300);
  CHECK_EQ(PVAI_M, 400);
  CHECK(EnergieActiveValide);
  CHECK(PuissanceRecue);  // positionné par le groupe DATE
  CHECK_STR(LTARF, "HC BLEU");
  CHECK_STR(NGTF, "BASE");
  CHECK_NEAR(Tension_M, 235.0, 0.01);
  CHECK_NEAR(Intensite_M, 12.0, 0.01);
  CHECK_EQ(EASF01, 1111111L);
  CHECK_EQ(EASF02, 2222222L);
  CHECK_EQ(EASF05, 5555555L);
  CHECK_EQ(EASF10, 1010101L);
  CHECK_EQ((int)IdxDataRawLinky, (int)((t1.size() + t2.size()) % 4000));

  // PuissanceS_M = COSphiS * PVAS_M ; COSphiS part de moyPWS/moyPVAS qui monte
  // lentement (filtre 5%), la puissance active reste donc faible sur 2 trames.
  CHECK(PuissanceS_M >= 0 && PuissanceS_M <= PVAS_M);

  // Correctif B2 : STGE complet conservé (publié en MQTT), STGEt = 2e caractère
  // (nibble Tempo jour/lendemain) utilisé par l'accueil, l'écran et les esclaves.
  CHECK_STR(STGE, "1A3B0001");
  CHECK_STR(STGEt, "A");
}

static void test_linky_checksum_faux() {
  reset_linky();
  Setup_Linky();

  // Première trame correcte pour amorcer le compteur
  MySerial.mock_feed(trame_linky(500000, 100000, 800, 0));
  mock_set_millis(20000);
  LectureLinky();
  long avant = Energie_M_Soutiree;
  CHECK_EQ(avant, 500000L);

  // Deuxième trame avec un checksum EAST volontairement faux
  MySerial.mock_feed(trame_linky(777777, 100000, 800, 0, /*chk_delta_east=*/1));
  mock_set_millis(22000);
  LectureLinky();

  CHECK_EQ(Energie_M_Soutiree, avant);  // valeur inchangée
  bool trouve = false;
  for (int i = 0; i < 10; i++)
    if (MessageH[i].indexOf("Erreur checksum") >= 0 && MessageH[i].indexOf("EAST") >= 0)
      trouve = true;
  CHECK(trouve);
}

// ===========================================================================
// 2. Tables Multi-Sinus
// ===========================================================================
static void test_multisinus_tables() {
  reset_commun();
  // Algorithme recopié à l'identique depuis setup() (Solar_Router_V17_29.ino:1055)
  uint8_t tot[101], on[101];
  float erreur, vrai, target;
  for (int I = 0; I < 101; I++) {
    tot[I] = (uint8_t)-1;
    on[I] = (uint8_t)-1;
    target = float(I) / 100.0;
    for (int T = 20; T < 101; T++) {
      for (int N = 0; N <= T; N++) {
        if (T % 2 == 1 || N % 2 == 0) {
          vrai = float(N) / float(T);
          erreur = std::fabs(vrai - target);
          if (erreur < 0.004) {
            tot[I] = T;
            on[I] = N;
            N = 101;
            T = 101;
          }
        }
      }
    }
  }

  // Invariants de l'algorithme : rapport respecté et pas de composante continue
  for (int I = 0; I < 101; I++) {
    CHECK(tot[I] >= 20 && tot[I] <= 100);
    CHECK(on[I] <= tot[I]);
    CHECK(std::fabs(double(on[I]) / double(tot[I]) - I / 100.0) < 0.004);
    CHECK(tot[I] % 2 == 1 || on[I] % 2 == 0);
  }

  // BUG PROBABLE: les tables constantes tabPulseSinusTotal/tabPulseSinusOn
  // (Solar_Router_V17_29.ino:562-573, "optimisation Michy") NE correspondent PAS
  // à ce que recalcule setup() : setup() les écrase intégralement au démarrage.
  // Le test fige l'écart observé plutôt que de le corriger.
  // Depuis l'optimisation : les tables constantes du .ino sont exactement le
  // résultat de l'algorithme (le recalcul au setup() a été supprimé).
  int nbDiff = 0;
  for (int I = 0; I < 101; I++)
    if (tot[I] != tabPulseSinusTotal[I] || on[I] != tabPulseSinusOn[I]) nbDiff++;
  CHECK_EQ(nbDiff, 0);
  CHECK_EQ((int)tabPulseSinusTotal[0], 20);
  CHECK_EQ((int)tot[0], 20);
  CHECK_EQ((int)tabPulseSinusOn[0], 0);
  CHECK_EQ((int)on[0], 0);
  // Les extrémités "utiles" restent cohérentes après recalcul
  CHECK_EQ((int)on[100], (int)tot[100]);     // 100% d'ouverture
}

// ===========================================================================
// 3. GestionOverproduction : Triac (PID intégral) et relais On/Off
// ===========================================================================
static void config_overproduction() {
  reset_commun();
  Source = "Linky";
  HeureCouranteDeci = 1200;
  ITmode = 5;
  ModePara = 0;
  ReacCACSI = 1;
  NbActions = 2;
  pTriac = 1;
  pulseTriac = 4;  // PulseT[1]
  zeroCross = 5;

  // Action 0 : Triac en découpe de sinus
  Action &a0 = LesActions[0];
  a0.Actif = MODE_DECOUPE_ONOFF;
  a0.Titre = "Chauffe_eau";
  a0.NbPeriode = 1;
  a0.Type[0] = 4;  // Triac
  a0.Hdeb[0] = 0;
  a0.Hfin[0] = 2400;
  a0.Vmin[0] = 0;
  a0.Vmax[0] = 100;
  a0.CanalTemp[0] = -1;
  a0.SelAct[0] = 255;
  a0.Tarif[0] = 0;
  a0.Ki = 10;
  a0.PID = false;
  a0.Gpio = pulseTriac;
  Gpio[0] = pulseTriac;

  // Action 1 : relais On/Off sur GPIO 26 (le GPIO 4 est pris par le Triac)
  Action &a1 = LesActions[1];
  a1.Actif = MODE_DECOUPE_ONOFF;
  a1.Titre = "Radiateur";
  a1.NbPeriode = 1;
  a1.Type[0] = 3;       // PW (régulation)
  a1.Hdeb[0] = 0;
  a1.Hfin[0] = 2400;
  a1.Vmin[0] = -200;    // seuil On  : injection > 200 W
  a1.Vmax[0] = 100;     // seuil Off : soutirage > 100 W
  a1.CanalTemp[0] = -1;
  a1.SelAct[0] = 255;
  a1.Tarif[0] = 0;
  a1.Tempo = 0;
  a1.OrdreOn = "26|1";
  a1.InitGpio(Fpwm);
  Gpio[1] = a1.Gpio;
  OutOn[1] = a1.OutOn;
  OutOff[1] = a1.OutOff;
}

static void test_overproduction_triac() {
  config_overproduction();

  // Injection de 1000 W : le retard doit décroître (l'ouverture augmente)
  PuissanceS_M = 0;
  PuissanceI_M = 1000;
  int precedent = Retard[0];
  for (int k = 0; k < 10; k++) {
    GestionOverproduction();
    CHECK(Retard[0] >= 0 && Retard[0] <= 100);
    CHECK(Retard[0] <= precedent);
    precedent = Retard[0];
  }
  CHECK(Retard[0] < 100);
  CHECK_EQ(Retard[0], 90);  // 100 - 10 * (1000*10/10000)

  // Soutirage de 1000 W : le retard remonte vers 100 (fermeture)
  PuissanceS_M = 1000;
  PuissanceI_M = 0;
  for (int k = 0; k < 20; k++) GestionOverproduction();
  CHECK_EQ(Retard[0], 100);
  CHECK(Retard[0] >= 0 && Retard[0] <= 100);
}

static void test_overproduction_relais() {
  config_overproduction();

  // Injection > 200 W -> relais On
  PuissanceS_M = 0;
  PuissanceI_M = 1000;
  GestionOverproduction();
  CHECK_EQ(Retard[1], 0);
  CHECK_NEAR(RetardF[1], 0.0, 0.001);
  CHECK_EQ(mock_gpio_state[26], 1);

  // Soutirage > 100 W -> relais Off
  PuissanceS_M = 1000;
  PuissanceI_M = 0;
  GestionOverproduction();
  CHECK_EQ(Retard[1], 100);
  CHECK_EQ(mock_gpio_state[26], 0);

  // Zone morte entre les deux seuils : l'état est conservé
  PuissanceS_M = 0;
  PuissanceI_M = 1000;
  GestionOverproduction();
  CHECK_EQ(Retard[1], 0);
  PuissanceS_M = 50;
  PuissanceI_M = 0;
  GestionOverproduction();
  CHECK_EQ(Retard[1], 0);  // ni > 100 ni < -200 : reste On
}

static void test_overproduction_forcage() {
  config_overproduction();
  PuissanceS_M = 1000;  // soutirage : sans forçage tout serait fermé
  PuissanceI_M = 0;
  LesActions[0].tOnOff = 30;
  LesActions[0].ForceOuvre = 60;
  LesActions[1].tOnOff = 30;
  LesActions[1].ForceOuvre = 100;

  GestionOverproduction();
  CHECK_EQ(Retard[0], 40);   // 100 - ForceOuvre
  CHECK_EQ(Retard[1], 0);    // 100 - 100
  CHECK(Retard[0] >= 0 && Retard[0] <= 100);
  CHECK(Retard[1] >= 0 && Retard[1] <= 100);

  // Forçage Off prioritaire
  LesActions[0].tOnOff = -30;
  LesActions[1].tOnOff = -30;
  GestionOverproduction();
  CHECK_EQ(Retard[0], 100);
  CHECK_EQ(Retard[1], 100);
}

// ===========================================================================
// 4. Action::ParaEnCours
// ===========================================================================
static void test_para_en_cours() {
  reset_commun();
  Action a(3);
  a.NbPeriode = 2;
  a.Type[0] = 3;  a.Hdeb[0] = 0;    a.Hfin[0] = 1200;
  a.Vmin[0] = 10; a.Vmax[0] = 80;   a.ONouvre[0] = 100;
  a.Type[1] = 2;  a.Hdeb[1] = 1200; a.Hfin[1] = 2400;
  a.Vmin[1] = 20; a.Vmax[1] = 90;   a.ONouvre[1] = 70;

  // -- Périodes ---------------------------------------------------------
  Action::ParaPeriode p = a.ParaEnCours(600, -120, 0, 100);
  CHECK_EQ(p.Type, 3);
  CHECK_EQ(p.Vmin, 10);
  CHECK_EQ(p.Vmax, 80);

  p = a.ParaEnCours(1800, -120, 0, 100);
  CHECK_EQ(p.Type, 2);
  CHECK_EQ(p.Vmax, 70);  // pour Type=2, Vmax est remplacé par ONouvre

  // -- Hors période -----------------------------------------------------
  p = a.ParaEnCours(2500, -120, 0, 100);
  CHECK_EQ(p.Type, 1);
  // BUG PROBABLE: hors période, P.Vmin/P.Vmax ne sont jamais affectés
  // (Actions.cpp:122-151) : la structure est renvoyée non initialisée.
  // GestionOverproduction ne les lit pas quand Type<=1, l'effet reste latent.

  // -- Condition température avec hystérésis (Tinf < Tsup) --------------
  Action h(4);
  h.NbPeriode = 1;
  h.Type[0] = 3; h.Hdeb[0] = 0; h.Hfin[0] = 2400;
  h.Vmin[0] = 0; h.Vmax[0] = 100;
  h.Tinf[0] = 200;  // 20.0 degC
  h.Tsup[0] = 300;  // 30.0 degC

  CHECK_EQ(h.ParaEnCours(600, 35.0, 0, 100).Type, 1);  // trop chaud -> Off
  CHECK_EQ(h.ParaEnCours(600, 15.0, 0, 100).Type, 3);  // froid -> On
  // Entre les deux seuils : l'état précédent est conservé (hystérésis)
  CHECK_EQ(h.ParaEnCours(600, 25.0, 0, 100).Type, 3);
  CHECK_EQ(h.ParaEnCours(600, 35.0, 0, 100).Type, 1);
  CHECK_EQ(h.ParaEnCours(600, 25.0, 0, 100).Type, 1);

  // -- Condition température sans hystérésis (seul Tinf renseigné) ------
  Action s(5);
  s.NbPeriode = 1;
  s.Type[0] = 3; s.Hdeb[0] = 0; s.Hfin[0] = 2400;
  s.Tinf[0] = 200;    // <= 1000 : coupe au-dessus de 20 degC
  s.Tsup[0] = -1600;  // inutilisé
  CHECK_EQ(s.ParaEnCours(600, 25.0, 0, 100).Type, 1);
  CHECK_EQ(s.ParaEnCours(600, 15.0, 0, 100).Type, 3);
  // Température invalide (<= -100) : aucune condition appliquée
  CHECK_EQ(s.ParaEnCours(600, -120.0, 0, 100).Type, 3);

  // -- Condition tarifaire ---------------------------------------------
  Action t(6);
  t.NbPeriode = 1;
  t.Type[0] = 3; t.Hdeb[0] = 0; t.Hfin[0] = 2400;
  t.Tarif[0] = 4;  // BLEU
  CHECK_EQ(t.ParaEnCours(600, -120, 4, 100).Type, 3);   // tarif bleu -> autorisé
  CHECK_EQ(t.ParaEnCours(600, -120, 1, 100).Type, 1);   // heure pleine -> refusé
  CHECK_EQ(t.ParaEnCours(600, -120, 0, 100).Type, 3);   // Ltarfbin=0 -> pas de filtre

  // -- Forçage tOnOff ---------------------------------------------------
  a.tOnOff = 45;
  a.ForceOuvre = 55;
  p = a.ParaEnCours(600, -120, 0, 100);
  CHECK_EQ(p.Type, 2);
  CHECK_EQ(p.Vmax, 55);
  a.tOnOff = -45;
  CHECK_EQ(a.ParaEnCours(600, -120, 0, 100).Type, 1);
  a.tOnOff = 0;

  // -- CanalTempEnCours --------------------------------------------------
  a.CanalTemp[0] = 2;
  a.CanalTemp[1] = -1;
  CHECK_EQ((int)a.CanalTempEnCours(600), 2);
  CHECK_EQ((int)a.CanalTempEnCours(1800), -1);
}

// ===========================================================================
// 5. Paramètres : SerializeConfiguration / DeserializeConfiguration
// ===========================================================================
static void test_parametres_roundtrip() {
  reset_commun();
  ModePara = 1;  // mode expert : MQTTRepet et subMQTT sont conservés

  ssid = "MonReseauWifi";
  password = "MonMotDePasse";
  MQTTIP = String2IP("192.168.1.77");
  MQTTPort = 1884;
  MQTTUser = "usr";
  MQTTPwd = "pwd";
  MQTTdeviceName = "routeur_test";
  TopicP = "maison/puissance";
  subMQTT = 1;
  MQTTRepet = 7;
  Source = "Linky";
  hostname = "RMS-TEST";
  nomRouteur = "Routeur Essai";
  ComSurv = 9;
  pTriac = 2;
  pSerial = 3;
  Serial2V = 19200;
  NbActions = 2;

  LesActions[0].Actif = MODE_DECOUPE_ONOFF;
  LesActions[0].Titre = "Chauffe_eau";
  LesActions[0].Ki = 42;
  LesActions[0].Kp = 11;
  LesActions[0].Kd = 3;
  LesActions[0].PID = true;
  LesActions[0].ForceOuvre = 66;
  LesActions[0].NbPeriode = 2;
  LesActions[0].Type[0] = 4; LesActions[0].Hdeb[0] = 0;    LesActions[0].Hfin[0] = 1200;
  LesActions[0].Vmin[0] = -50; LesActions[0].Vmax[0] = 90; LesActions[0].ONouvre[0] = 80;
  LesActions[0].Tinf[0] = 150; LesActions[0].Tsup[0] = 250; LesActions[0].CanalTemp[0] = 1;
  LesActions[0].Type[1] = 1; LesActions[0].Hdeb[1] = 1200; LesActions[0].Hfin[1] = 2400;
  LesActions[0].Vmin[1] = 0;  LesActions[0].Vmax[1] = 100; LesActions[0].ONouvre[1] = 100;

  LesActions[1].Actif = MODE_MULTISINUS;
  LesActions[1].Titre = "Radiateur";
  LesActions[1].OrdreOn = "26|1";
  LesActions[1].OrdreOff = "26|0";
  LesActions[1].Host = "192.168.1.90";
  LesActions[1].Port = 8080;
  LesActions[1].Repet = 5;
  LesActions[1].Tempo = 12;
  LesActions[1].NbPeriode = 1;
  LesActions[1].Type[0] = 3; LesActions[1].Hdeb[0] = 0; LesActions[1].Hfin[0] = 2400;
  LesActions[1].Vmin[0] = -200; LesActions[1].Vmax[0] = 100; LesActions[1].Tarif[0] = 4;

  String json = SerializeConfiguration();
  CHECK(json.length() > 100);

  // Remise à zéro puis relecture
  ssid = ""; password = ""; MQTTIP = 0; MQTTPort = 0; MQTTUser = ""; MQTTPwd = "";
  MQTTdeviceName = ""; TopicP = ""; subMQTT = 0; MQTTRepet = 0; Source = "";
  nomRouteur = ""; ComSurv = 0; pTriac = 0; pSerial = 0; Serial2V = 0; NbActions = 0;
  for (int i = 0; i < LES_ACTIONS_LENGTH; i++) LesActions[i] = Action(i);

  DeserializeConfiguration(json);

  CHECK_STR(ssid, "MonReseauWifi");
  CHECK_STR(password, "MonMotDePasse");
  CHECK_EQ(MQTTIP, String2IP("192.168.1.77"));
  CHECK_EQ(MQTTPort, 1884u);
  CHECK_STR(MQTTdeviceName, "routeur_test");
  CHECK_STR(TopicP, "maison/puissance");
  CHECK_EQ((int)subMQTT, 1);
  CHECK_EQ(MQTTRepet, 7u);
  CHECK_STR(Source, "Linky");
  CHECK_STR(nomRouteur, "Routeur Essai");
  CHECK_EQ(ComSurv, 9);
  CHECK_EQ((int)pTriac, 2);
  CHECK_EQ((int)pSerial, 3);
  CHECK_EQ(Serial2V, 19200u);
  CHECK_EQ((int)NbActions, 2);

  CHECK_EQ((int)LesActions[0].Actif, MODE_DECOUPE_ONOFF);
  CHECK_STR(LesActions[0].Titre, "Chauffe_eau");
  CHECK_EQ((int)LesActions[0].Ki, 42);
  CHECK_EQ((int)LesActions[0].Kp, 11);
  CHECK_EQ((int)LesActions[0].Kd, 3);
  CHECK(LesActions[0].PID);
  CHECK_EQ((int)LesActions[0].ForceOuvre, 66);
  CHECK_EQ((int)LesActions[0].NbPeriode, 2);
  CHECK_EQ((int)LesActions[0].Type[0], 4);
  CHECK_EQ((int)LesActions[0].Hdeb[0], 0);
  CHECK_EQ((int)LesActions[0].Hfin[0], 1200);
  CHECK_EQ((int)LesActions[0].Vmin[0], -50);
  CHECK_EQ((int)LesActions[0].Vmax[0], 90);
  CHECK_EQ((int)LesActions[0].ONouvre[0], 80);
  CHECK_EQ((int)LesActions[0].Tinf[0], 150);
  CHECK_EQ((int)LesActions[0].Tsup[0], 250);
  CHECK_EQ((int)LesActions[0].CanalTemp[0], 1);
  CHECK_EQ((int)LesActions[0].Hdeb[1], 1200);  // Hdeb reconstruit depuis Hfin[0]
  CHECK_EQ((int)LesActions[0].Hfin[1], 2400);

  CHECK_EQ((int)LesActions[1].Actif, MODE_MULTISINUS);
  CHECK_STR(LesActions[1].Titre, "Radiateur");
  CHECK_STR(LesActions[1].OrdreOn, "26|1");
  CHECK_STR(LesActions[1].OrdreOff, "26|0");
  CHECK_STR(LesActions[1].Host, "192.168.1.90");
  CHECK_EQ(LesActions[1].Port, 8080);
  CHECK_EQ(LesActions[1].Repet, 5);
  CHECK_EQ(LesActions[1].Tempo, 12);
  CHECK_EQ((int)LesActions[1].Vmin[0], -200);
  CHECK_EQ((int)LesActions[1].Tarif[0], 4);
}

static void test_parametres_mode_standard() {
  reset_commun();
  // Comportement actuel : en mode standard (ModePara=0), la sérialisation force
  // MQTTRepet et subMQTT à 0 (effet de bord sur les variables globales).
  ModePara = 0;
  MQTTRepet = 12;
  subMQTT = 1;
  NbActions = 1;
  LesActions[0].NbPeriode = 1;
  LesActions[0].CanalTemp[0] = 2;
  LesActions[0].SelAct[0] = 3;

  String json = SerializeConfiguration();
  CHECK_EQ(MQTTRepet, 0u);
  CHECK_EQ((int)subMQTT, 0);
  CHECK(json.indexOf("\"MQTTRepet\":0") > 0);
  CHECK(json.indexOf("\"subMQTT\":0") > 0);
  // ... et remet CanalTemp/SelAct aux valeurs "non utilisé"
  CHECK_EQ((int)LesActions[0].CanalTemp[0], -1);
  CHECK_EQ((int)LesActions[0].SelAct[0], 255);
}

// ===========================================================================
// 6. MQTT : découverte Home Assistant, état, callback
// ===========================================================================
static void config_mqtt() {
  reset_commun();
  Source = "Linky";
  ESP32_Type = 1;
  ModeReseau = 0;
  MQTTRepet = 10;
  MQTTIP = String2IP("192.168.1.10");
  MQTTPort = 1883;
  MQTTPrefix = "homeassistant";
  MQTTPrefixEtat = "homeassistant";
  MQTTdeviceName = "routeur_rms";
  MQTTUser = "u";
  MQTTPwd = "p";
  nomRouteur = "Routeur";
  TopicP = "maison/puissance";
  subMQTT = 1;
  NbActions = 2;
  pTriac = 1;
  EnergieActiveValide = true;
  LTARF = "HC BLEU";
  NGTF = "BASE";
  STGE = "A";
  T_On_seconde = 7200;

  PuissanceS_M = 1234;
  PuissanceI_M = 0;
  Tension_M = 235.2f;
  Intensite_M = 5.3f;
  PowerFactor_M = 0.98f;
  Energie_M_Soutiree = 1000000;
  Energie_M_Injectee = 200000;
  EnergieJour_M_Soutiree = 5000;
  EnergieJour_M_Injectee = 600;
  EASF01 = 1111111; EASF02 = 2222222; EASF03 = 3; EASF04 = 4; EASF05 = 5;
  EASF06 = 6; EASF07 = 7; EASF08 = 8; EASF09 = 9; EASF10 = 10;

  LesActions[0].Titre = "Chauffe_eau";
  LesActions[0].Actif = MODE_DECOUPE_ONOFF;
  LesActions[0].NbPeriode = 1;
  LesActions[0].Type[0] = 4;
  LesActions[0].Hfin[0] = 2400;
  LesActions[0].Vmax[0] = 100;
  LesActions[0].H_Ouvre = 1.5f;
  LesActions[1].Titre = "Radiateur";
  LesActions[1].Actif = MODE_DECOUPE_ONOFF;
  LesActions[1].NbPeriode = 1;
  LesActions[1].Type[0] = 3;
  LesActions[1].Hfin[0] = 2400;
  LesActions[1].OrdreOn = "26|1";
  LesActions[1].InitGpio(Fpwm);
  Retard[0] = 40;
  Retard[1] = 100;
}

static void test_mqtt_discovery() {
  config_mqtt();
  mock_mqtt_connected = false;
  mock_mqtt_connect_ok = true;

  CHECK(testMQTTconnected());
  CHECK(mock_mqtt_connected);
  // Souscriptions : TopicP (Source=Pmqtt seulement) non, mais les actions oui
  bool subChauffe = false;
  for (auto &s : mock_mqtt_subscribed)
    if (s == "routeur_rms/Chauffe_eau") subChauffe = true;
  CHECK(subChauffe);

  mock_mqtt_published.clear();
  sendMQTTDiscoveryMsg_global();

  // 2 (LTARF/Code_Tarifaire) + 12 (NGTF/STGE/EASF01..10) + 3 (U,I,PF)
  // + 6 (puissances/énergies M) + 1 (ESP32_On) + 2 actions * 4 = 32
  CHECK_EQ((int)mock_mqtt_published.size(), 32);
  CHECK(Discovered);

  int nbConfig = 0, nbJsonOk = 0;
  bool topicPuissance = false;
  for (auto &m : mock_mqtt_published) {
    if (m.topic.rfind("homeassistant/sensor/routeur_rms_", 0) == 0 &&
        m.topic.size() > 7 && m.topic.compare(m.topic.size() - 7, 7, "/config") == 0)
      nbConfig++;
    if (m.topic == "homeassistant/sensor/routeur_rms_PuissanceS_M/config")
      topicPuissance = true;
    JsonDocument d;
    if (!deserializeJson(d, m.payload)) nbJsonOk++;
  }
  CHECK(topicPuissance);
  CHECK_EQ(nbJsonOk, (int)mock_mqtt_published.size());  // tous les payloads sont du JSON valide
  // 31 topics "sensor", le 32e (Actif_Triac / Actif_Relais) est un binary_sensor
  CHECK_EQ(nbConfig, 30);
}

static void test_mqtt_etat() {
  config_mqtt();
  mock_mqtt_connected = false;
  CHECK(testMQTTconnected());
  mock_mqtt_published.clear();

  SendDataToHomeAssistant();
  CHECK_EQ((int)mock_mqtt_published.size(), 1);
  CHECK_EQ(mock_mqtt_published[0].topic, std::string("homeassistant/routeur_rms_state"));

  JsonDocument d;
  DeserializationError err = deserializeJson(d, mock_mqtt_published[0].payload);
  CHECK(!err);
  CHECK_EQ((int)d["PuissanceS_M"], 1234);
  CHECK_EQ((long)d["EASF01"], 1111111L);
  CHECK_STR(String(d["LTARF"].as<const char *>()), "HC BLEU");
  CHECK_EQ((int)d["Code_Tarifaire"], 11);
  CHECK_EQ((int)d["Ouverture_Triac"], 60);  // 100 - Retard[0]
  CHECK_NEAR((double)d["ESP32_On"], 2.0, 0.001);
  CHECK_STR(String(d["NGTF"].as<const char *>()), "BASE");
}

// Correctif B3 : Linky + 10 actions + 4 températures -> le message d'état
// dépasse l'ancien buffer de 1200 octets ; il doit rester un JSON valide ou
// ne pas être publié, jamais déborder.
static void test_mqtt_etat_long() {
  reset_commun();
  Source = "Linky";
  MQTTRepet = 10;
  EnergieActiveValide = true;
  pTriac = 1;
  NbActions = LES_ACTIONS_LENGTH;
  for (int i = 0; i < NbActions; i++) {
    LesActions[i].Titre = "Action" + String(i);
    LesActions[i].Actif = 1;
    LesActions[i].H_Ouvre = 12.345678;
    LesActions[i].tOnOff = -30;
    Retard[i] = 42;
  }
  for (int c = 0; c < 4; c++) { Source_Temp[c] = "tempInt"; temperature[c] = 21.5; }
  LTARF = "HC BLEU"; NGTF = "TEMPO"; STGE = "1A3B0001";
  EASF01 = 12345678; EASF02 = 12345678; EASF03 = 12345678; EASF04 = 12345678; EASF05 = 12345678;
  EASF06 = 12345678; EASF07 = 12345678; EASF08 = 12345678; EASF09 = 12345678; EASF10 = 12345678;
  Energie_M_Soutiree = 123456789; Energie_M_Injectee = 123456789;
  mock_mqtt_published.clear();
  mock_mqtt_connected = true;
  SendDataToHomeAssistant();
  if (!mock_mqtt_published.empty()) {
    JsonDocument doc;
    CHECK(deserializeJson(doc, mock_mqtt_published.back().payload.c_str()) == DeserializationError::Ok);
    CHECK_EQ((int)doc["Ouverture_Relais_9"], 58);
    CHECK_EQ((long)doc["EASF10"], 12345678L);
  } else {
    CHECK(MessageH[0].indexOf("trop long") >= 0 || MessageH[1].indexOf("trop long") >= 0);
  }
}

static void test_mqtt_callback_actions() {
  config_mqtt();
  mock_mqtt_connected = false;
  CHECK(testMQTTconnected());

  std::string topic = "routeur_rms/Radiateur";
  std::string p1 = "{\"tOnOff\":30}";
  callback(&topic[0], (uint8_t *)p1.data(), (unsigned int)p1.size());
  CHECK_EQ(LesActions[1].tOnOff, 30);

  std::string p2 = "{\"Mode\":\"Multi\"}";
  callback(&topic[0], (uint8_t *)p2.data(), (unsigned int)p2.size());
  CHECK_EQ((int)LesActions[1].Actif, MODE_MULTISINUS);
  // Le changement de mode déclenche une réécriture du fichier de paramètres
  CHECK(mock_fs.count("/parametres.json") > 0);

  std::string p3 = "{\"Periode\":0,\"SeuilOn\":-333,\"SeuilOff\":222}";
  callback(&topic[0], (uint8_t *)p3.data(), (unsigned int)p3.size());
  CHECK_EQ((int)LesActions[1].Vmin[0], -333);
  CHECK_EQ((int)LesActions[1].Vmax[0], 222);
}

static void test_mqtt_source_pmqtt() {
  config_mqtt();
  Source = "Pmqtt";
  LissageLong = false;
  init_puissance();
  EASfloat = 0;
  EAIfloat = 0;
  Energie_M_Soutiree = 0;
  Energie_M_Injectee = 0;
  mock_mqtt_connected = false;
  CHECK(testMQTTconnected());
  bool subP = false;
  for (auto &s : mock_mqtt_subscribed)
    if (s == "maison/puissance") subP = true;
  CHECK(subP);

  std::string topic = "maison/puissance";
  std::string pl = "{\"Pw\":-500,\"Pva\":600}";
  mock_set_millis(50000);
  callback(&topic[0], (uint8_t *)pl.data(), (unsigned int)pl.size());
  CHECK_NEAR(PwMQTT, -500.0, 0.01);
  CHECK_NEAR(PvaMQTT, 600.0, 0.01);

  UpdatePmqtt();
  CHECK_EQ(PuissanceI_M, 500);
  CHECK_EQ(PuissanceS_M, 0);
  CHECK(Pva_valide);
  CHECK(EnergieActiveValide);
  CHECK_EQ(PVAI_M, 600);
}

// ===========================================================================
// 6 bis. Pages web servies compressées (WebGz.h généré par tools/gen_web_gz.py)
// ===========================================================================
#include "WebGz.h"  // constantes à liaison interne : on inclut le fichier généré
static void test_web_gzip() {
  reset_commun();
  mock_last_http_headers = "";
  handleRoot();
  CHECK(mock_last_http_headers.indexOf("Content-Encoding: gzip") >= 0);
  CHECK_EQ((unsigned)mock_last_http_body.length(), (unsigned)MainHtml_gz_len);
  CHECK((uint8_t)mock_last_http_body[0] == 0x1f && (uint8_t)mock_last_http_body[1] == 0x8b);  // magic gzip

  // Page protégée : sans cookie valide -> page de saisie de la clé
  CleAccesRef = "secret";
  mock_http_headers.erase("Cookie");
  handleActions();
  CHECK_EQ((unsigned)mock_last_http_body.length(), (unsigned)ParaCleHtml_gz_len);
  mock_http_headers["Cookie"] = "CleAcces=secret";
  handleActions();
  CHECK_EQ((unsigned)mock_last_http_body.length(), (unsigned)ActionsHtml_gz_len);
  mock_http_headers.erase("Cookie");
  CleAccesRef = "";

  // Variable biSonde servie séparément (avant MainJS1)
  nomSondeFixe = "Prod";
  Source_data = "UxIx2";
  handleBiSonde();
  CHECK_STR(mock_last_http_body, "var biSonde=true;\r\n");
  Source_data = "Linky";
  handleBiSonde();
  CHECK_STR(mock_last_http_body, "var biSonde=false;\r\n");
}

// ===========================================================================
// 7. Fonctions utilitaires
// ===========================================================================
static void test_utilitaires() {
  reset_commun();

  CHECK_STR(IP2String(String2IP("192.168.1.42")), "192.168.1.42");
  CHECK_STR(IP2String(String2IP("10.0.0.1")), "10.0.0.1");
  CHECK_EQ(String2IP("0.0.0.0"), 0UL);

  String js = "{\"Pw\":-12.5,\"Pva\":600,\"Mode\":\"Multi\"}";
  CHECK_NEAR(ValJson("Pw", js), -12.5, 0.001);
  CHECK_NEAR(ValJson("Pva", js), 600.0, 0.001);
  CHECK_NEAR(ValJson("Absent", js), 0.0, 0.001);
  CHECK_STR(StringJson("Mode", js), "Multi");
  CHECK_STR(StringJson("Absent", js), "");
  CHECK_STR(SubJson("\"Pva\"", "}", js), "\"Pva\":600,\"Mode\":\"Multi\"}");

  CHECK_STR(urlDecode("a%20b+c"), "a b c");
  CHECK_STR(urlDecode("100%25"), "100%");
  CHECK_STR(urlDecode("simple"), "simple");

  PmaxReseau = 36000;
  CHECK_EQ(PintMax(50000), 36000);
  CHECK_EQ(PintMax(-50000), -36000);
  CHECK_EQ(PintMax(1234), 1234);
  CHECK_NEAR(PfloatMax(-99999.0f), -36000.0, 0.01);
  CHECK_NEAR(PfloatMax(12.5f), 12.5, 0.01);

  CHECK_STR(Filtre_Nom("Sonde,1#A"), "Sonde.1=A");

  String avant, apres;
  SplitS("cle=valeur", avant, "=", apres);
  CHECK_STR(avant, "cle");
  CHECK_STR(apres, "valeur");
}

// ===========================================================================
// 8. Source externe : handleAjaxData() -> CallESP32_Externe()
// ===========================================================================
static void test_source_externe() {
  reset_commun();

  // --- Production de la trame par le "maître" --------------------------
  HeureValide = true;
  DATE = "20/09/2026 12:00:00";
  Source_data = "Linky";
  LTARF = "HC BLEU";
  STGEt = "A";
  Pva_valide = true;
  PuissanceS_M = 1234;
  PuissanceI_M = 0;
  PVAS_M = 1300;
  PVAI_M = 0;
  EnergieJour_M_Soutiree = 5000;
  EnergieJour_M_Injectee = 600;
  Energie_M_Soutiree = 1000000;
  Energie_M_Injectee = 200000;
  Tension_M = 235.5f;
  Intensite_M = 5.25f;
  Tension_M1 = 235.5f;
  Intensite_M1 = 5.25f;
  Tension_M2 = 0; Intensite_M2 = 0;
  Tension_M3 = 0; Intensite_M3 = 0;

  handleAjaxData();
  String trame = mock_last_http_body;
  CHECK(trame.indexOf("Deb") == 0);
  CHECK(trame.indexOf("Fin") > 0);
  // La trame doit rester sous la limite de 400 caractères imposée par
  // Source_Externe.ino:45, sinon elle est purement et simplement jetée.
  CHECK(trame.length() < 400);

  // --- Lecture par l'"esclave" -----------------------------------------
  int refPS = PuissanceS_M, refPI = PuissanceI_M, refVAS = PVAS_M, refVAI = PVAI_M;
  long refEJS = EnergieJour_M_Soutiree, refEJI = EnergieJour_M_Injectee;
  long refES = Energie_M_Soutiree, refEI = Energie_M_Injectee;

  reset_commun();
  Source = "Ext";
  Horloge = 0;
  RMSextIP = String2IP("192.168.1.60");
  RMSextIdx = 0;
  mock_client_connect_ok = true;
  mock_client_response = std::string(trame.c_str());
  mock_set_millis(60000);

  CallESP32_Externe();

  CHECK_EQ(PuissanceS_M, refPS);
  CHECK_EQ(PuissanceI_M, refPI);
  CHECK_EQ(PVAS_M, refVAS);
  CHECK_EQ(PVAI_M, refVAI);
  CHECK_EQ(EnergieJour_M_Soutiree, refEJS);
  CHECK_EQ(EnergieJour_M_Injectee, refEJI);
  CHECK_EQ(Energie_M_Soutiree, refES);
  CHECK_EQ(Energie_M_Injectee, refEI);
  CHECK_NEAR(Tension_M, 235.5, 0.01);
  CHECK_NEAR(Intensite_M, 5.25, 0.01);
  CHECK_STR(Source_data, "Linky");
  CHECK_STR(LTARF, "HC BLEU");
  CHECK_STR(STGEt, "A");
  CHECK(EnergieActiveValide);
  CHECK(PuissanceRecue);
  CHECK(std::string(mock_client_request).find("GET /ajax_data") != std::string::npos);
}

// ===========================================================================
// 9. Énergie quotidienne : RecordEnergieMinuit / LectureConsoMatinJour
// ===========================================================================
static void test_energie_quotidienne() {
  reset_commun();
  Source = "Linky";
  HeureValide = true;

  Energie_M_Soutiree = 1000000;
  Energie_M_Injectee = 200000;
  Energie_T_Soutiree = 30000;
  Energie_T_Injectee = 4000;
  RecordEnergieMinuit("20260920");
  CHECK(mock_fs.count("/EnergieMinuit.eng") > 0);

  // Relecture après "redémarrage" : les compteurs J0 sont restaurés
  EAS_M_J0 = 0; EAI_M_J0 = 0; EAS_T_J0 = 0; EAI_T_J0 = 0;
  Energie_M_Soutiree = 0; Energie_M_Injectee = 0;
  Energie_T_Soutiree = 0; Energie_T_Injectee = 0;

  LectureConsoMatinJour();
  CHECK_EQ(EAS_M_J0, 1000000L);
  CHECK_EQ(EAI_M_J0, 200000L);
  CHECK_EQ(EAS_T_J0, 30000L);
  CHECK_EQ(EAI_T_J0, 4000L);
  // Les totaux plus faibles que la référence minuit sont remontés
  CHECK_EQ(Energie_M_Soutiree, 1000000L);
  CHECK_EQ(Energie_M_Injectee, 200000L);

  // Consommation du jour
  Energie_M_Soutiree = 1004500;
  Energie_M_Injectee = 200300;
  Energie_T_Soutiree = 30100;
  Energie_T_Injectee = 4050;
  EnergieQuotidienne();
  CHECK_EQ(EnergieJour_M_Soutiree, 4500L);
  CHECK_EQ(EnergieJour_M_Injectee, 300L);
  CHECK_EQ(EnergieJour_T_Soutiree, 100L);
  CHECK_EQ(EnergieJour_T_Injectee, 50L);

  // Source "Ext" : les compteurs jour sont fournis par le maître, pas recalculés
  Source = "Ext";
  EnergieJour_M_Soutiree = 12345;
  EnergieQuotidienne();
  CHECK_EQ(EnergieJour_M_Soutiree, 12345L);
}

// ===========================================================================
// 10. Record_Data : historique CSV mensuel
// ===========================================================================
static void test_record_data() {
  reset_commun();
  Source_data = "Linky";
  nomSondeMobile = "Maison";
  nomSondeFixe = "";
  EnergieJour_M_Soutiree = 5000;
  EnergieJour_M_Injectee = 600;
  Record_Conf = "";

  Record_Data("20260920", "Test", 1230);

  CHECK(mock_fs.count("/Mois_Wh_202609.csv") > 0);
  String csv = String(mock_fs["/Mois_Wh_202609.csv"]);
  CHECK(csv.indexOf("Date,Maison / ") == 0);
  CHECK(csv.indexOf(",Message\r\n") > 0);
  CHECK(csv.indexOf("20260920,5000,600,,,12.30,Test\r\n") > 0);

  // 2e enregistrement le lendemain : ajout d'une ligne dans le même fichier
  Record_Data("20260921", "Suite", 60);
  csv = String(mock_fs["/Mois_Wh_202609.csv"]);
  CHECK(csv.indexOf("20260921,5000,600,,,0.60,Suite\r\n") > 0);

  // Correctif : Record_Conf renseigné à la création du fichier, l'en-tête
  // n'est écrit qu'une fois.
  int premier = csv.indexOf("Date,Maison");
  int second = csv.indexOf("Date,Maison", premier + 1);
  CHECK(second == -1);

  // Date vide : aucun enregistrement
  size_t avant = mock_fs["/Mois_Wh_202609.csv"].size();
  Record_Data("", "Ignore", 100);
  CHECK_EQ(mock_fs["/Mois_Wh_202609.csv"].size(), avant);
}

// ===========================================================================
int main() {
  printf("=== Tests de non-regression - Routeur solaire F1ATB V17.29 ===\n");

  RUN(test_linky_trame_valide);
  RUN(test_linky_checksum_faux);
  RUN(test_multisinus_tables);
  RUN(test_overproduction_triac);
  RUN(test_overproduction_relais);
  RUN(test_overproduction_forcage);
  RUN(test_para_en_cours);
  RUN(test_parametres_roundtrip);
  RUN(test_parametres_mode_standard);
  RUN(test_mqtt_discovery);
  RUN(test_mqtt_etat);
  RUN(test_mqtt_etat_long);
  RUN(test_mqtt_callback_actions);
  RUN(test_mqtt_source_pmqtt);
  RUN(test_utilitaires);
  RUN(test_source_externe);
  RUN(test_energie_quotidienne);
  RUN(test_record_data);
  RUN(test_web_gzip);

  printf("--------------------------------------------------------------\n");
  printf("%d verifications, %d echec(s)\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
