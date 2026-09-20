// ****************************
// * Source de Mesures LINKY  *
// ****************************
// Décodage TIC (mode standard) partagé :
//  - source principale (Source == "Linky") sur MySerial : alimente la régulation ;
//  - Linky auxiliaire (LinkyAux == 1) sur SerialAux : lecture seule vers MQTT, et
//    tarif / Tempo / horloge quand la source de mesure n'est pas un Linky.
// Evolutions pour les Linky avec CACSI proposée par LJ (septembre 2025)

// ticPrincipal, ticAux et DataRawLinkyAux sont déclarés dans le sketch principal

void Setup_Linky() {
  delay(20);
  Serial2V = 9600;  //On force la vitesse
  MySerial.setRxBufferSize(SER_BUF_SIZE);
  MySerial.begin(Serial2V, SERIAL_7E1, RXD2, TXD2);  //  7-bit Even parity 1 stop bit pour le Linky
  ticPrincipal.buf = DataRawLinky;
  ticPrincipal.bufSize = 4000;
  delay(100);
}
void Setup_LinkyAux() {  // RX seul : la TIC n'a pas besoin de TX
  ticAux.buf = DataRawLinkyAux;
  ticAux.bufSize = sizeof(DataRawLinkyAux);
  SerialAux.setRxBufferSize(SER_BUF_SIZE);
  SerialAux.begin(9600, SERIAL_7E1, RX2_[pSerialAux], -1);
  StockMessage("Linky auxiliaire actif sur gpio " + String(RX2_[pSerialAux]));
}

void LectureLinky() {  //Lecture port série du LINKY principal
  if (ticPrincipal.buf == nullptr) {
    ticPrincipal.buf = DataRawLinky;
    ticPrincipal.bufSize = 4000;
  }
  DecodeTIC(MySerial, ticPrincipal, true);
  IdxDataRawLinky = ticPrincipal.idxRaw;  // Pour la page Données brutes
}
void LectureLinkyAux() {
  if (ticAux.buf == nullptr) return;
  DecodeTIC(SerialAux, ticAux, false);
}

// Estimation de la puissance active moyenne (W) à partir des index Wh : la TIC ne
// donne que la puissance apparente. energie = index courant (mis à jour ici).
void EstimePuissanceTIC(long &energie, long nouvelle, unsigned long &tLast, float &moy, float &deltaW) {
  long OldWh = energie;
  if (OldWh == 0) OldWh = nouvelle;
  energie = nouvelle;
  unsigned long Tm = millis();
  float deltaT = float(Tm - tLast) / float(3600000);
  if (energie == OldWh) {  //Pas de resultat en Wh
    float Pmax = 1.3 / deltaT;
    moy = min(moy, Pmax);
  } else {
    tLast = Tm;
    float deltaWh = float(energie - OldWh);
    deltaW = deltaWh / deltaT;
    float Pmin = (deltaWh - 1) / deltaT;
    moy = max(moy, Pmin);  //saut à la montée en puissance
  }
  moy = 0.05 * deltaW + 0.95 * moy;
}

// Lit les octets disponibles sur le port, décode les groupes complets.
// principal = true : met à jour les variables de régulation (Energie_M_*, PuissanceS_M...).
void DecodeTIC(HardwareSerial &port, TicData &tic, bool principal) {
  int V = 0;
  tic.boucles++;
  if (tic.boucles > 4000) {
    tic.boucles = 0;
    if (principal) {
      port.flush();
      port.write("Ok");
      StockMessage("Attente Linky 4000 boucles = 8s");
    }
  }
  while (port.available() > 0) {
    tic.boucles = 0;
    V = port.read();
    tic.buf[tic.idxRaw] = char(V);
    tic.idxRaw = (tic.idxRaw + 1) % tic.bufSize;
    switch (V) {
      case 2:  //STX (Start Text)
        break;
      case 3:  //ETX (End Text)
        if (principal) {
          previousETX = millis();
          cptLEDyellow = 4;
        }
        tic.LFon = false;
        break;
      case 10:  // Line Feed. Debut Groupe
        tic.LFon = true;
        tic.idxDecod = tic.idxRaw;
        break;
      case 13:  // CR. Fin de groupe
        if (tic.LFon) {
          tic.LFon = false;
          int nb_tab = 0;
          String code = "";
          String val = "";
          int checksum = 0;
          int checkLinky = -1;
          while (tic.idxDecod != tic.idxRaw) {
            if (tic.buf[tic.idxDecod] == char(9)) {  //Tabulation
              nb_tab++;
            } else {
              if (nb_tab == 0) code += tic.buf[tic.idxDecod];
              if (nb_tab == 1) val += tic.buf[tic.idxDecod];
              if (nb_tab <= 1) checksum += (int)tic.buf[tic.idxDecod];
            }
            tic.idxDecod = (tic.idxDecod + 1) % tic.bufSize;
            if (checkLinky == -1 && nb_tab == 2) {
              checkLinky = (int)tic.buf[tic.idxDecod];
              checksum += 18;            //2 tabulations
              checksum = checksum & 63;  //0x3F
              checksum = checksum + 32;  //0x20
            }
          }
          DecodeGroupeTIC(tic, code, val, checksum == checkLinky, principal);
        }
        break;
      default:
        break;
    }
  }
}

void DecodeGroupeTIC(TicData &tic, const String &code, const String &val, bool checksumOk, bool principal) {
  // Le tarif/Tempo/horloge/NGTF du Linky auxiliaire ne sont utilisés que si la source
  // de mesure n'est pas elle-même un Linky
  bool tarifDepuisIci = principal || Source != "Linky";

  if (code.indexOf("EAST") == 0 || code.indexOf("EAIT") == 0 || code == "SINSTS" || code.indexOf("SINSTI") == 0) {
    if (!checksumOk) {
      tic.nbErrChecksum++;
      StockMessage("Erreur checksum code : " + code);
      return;
    }
    if (code.indexOf("EAST") == 0) {
      if (principal) {
        EstimePuissanceTIC(Energie_M_Soutiree, val.toInt(), tic.TlastEAST, tic.moyPWS, tic.deltaWS);
        tic.EAST = Energie_M_Soutiree;
      } else {
        EstimePuissanceTIC(tic.EAST, val.toInt(), tic.TlastEAST, tic.moyPWS, tic.deltaWS);
      }
      tic.EASTvalid = true;
      if (!tic.EAITvalid && millis() > 12000) tic.EAITvalid = true;  //Cas des CACSI ou EAIT n'est jamais positionné
    }
    if (code.indexOf("EAIT") == 0) {
      if (principal) {
        EstimePuissanceTIC(Energie_M_Injectee, val.toInt(), tic.TlastEAIT, tic.moyPWI, tic.deltaWI);
        tic.EAIT = Energie_M_Injectee;
      } else {
        EstimePuissanceTIC(tic.EAIT, val.toInt(), tic.TlastEAIT, tic.moyPWI, tic.deltaWI);
      }
      tic.EAITvalid = true;
    }
    if (principal && tic.EASTvalid && tic.EAITvalid) EnergieActiveValide = true;
    if (code == "SINSTS") {  //Puissance apparente soutirée. Egalité pour ne pas confondre avec SINSTS1 (triphasé)
      tic.SINSTS = val.toInt();
      tic.PVAS = PintMax(tic.SINSTS);
      tic.moyPVAS = 0.05 * float(tic.PVAS) + 0.95 * tic.moyPVAS;
      tic.moyPWS = min(tic.moyPWS, tic.moyPVAS);
      if (tic.moyPVAS > 0) tic.COSphiS = min(float(1.0), tic.moyPWS / tic.moyPVAS);
      tic.PuissanceS = PintMax(int(tic.COSphiS * float(tic.PVAS)));
      if (principal) {
        PVAS_M = tic.PVAS;
        if (tic.moyPVAS > 0) PowerFactor_M = tic.COSphiS;
        PuissanceS_M = tic.PuissanceS;
        Pva_valide = true;
      }
    }
    if (code.indexOf("SINSTI") == 0) {  //Puissance apparente injectée
      tic.SINSTI = val.toInt();
      tic.PVAI = PintMax(tic.SINSTI);
      tic.moyPVAI = 0.05 * float(tic.PVAI) + 0.95 * tic.moyPVAI;
      tic.moyPWI = min(tic.moyPWI, tic.moyPVAI);
      if (tic.moyPVAI > 0) tic.COSphiI = min(float(1.0), tic.moyPWI / tic.moyPVAI);
      tic.PuissanceI = PintMax(int(tic.COSphiI * float(tic.PVAI)));
      if (principal && ReacCACSI != 100) {  //Estimateur OFF, mode normal sans CACSI
        PVAI_M = tic.PVAI;
        if (tic.moyPVAI > 0) PowerFactor_M = tic.COSphiI;
        PuissanceI_M = tic.PuissanceI;
        Pva_valide = true;
      }
    }
    return;
  }

  if (code.indexOf("DATE") == 0) {
    tic.DATE = val;
    tic.lastFrameMs = millis();
    tic.nbTrames++;
    if (principal) PuissanceRecue = true;  //Reset du Watchdog à chaque trame du Linky reçue
    if (Horloge == 1 && tarifDepuisIci && val.length() >= 13) {
      struct tm t = { .tm_sec = 0, .tm_min = 0, .tm_hour = 0, .tm_mday = 0, .tm_mon = 0, .tm_year = 0, .tm_wday = 0, .tm_yday = 0, .tm_isdst = 0 };
      t.tm_year = val.substring(1, 3).toInt() + 100;  // années depuis 1900
      t.tm_mon = val.substring(3, 5).toInt() - 1;
      t.tm_mday = val.substring(5, 7).toInt();
      t.tm_hour = val.substring(7, 9).toInt();
      t.tm_min = val.substring(9, 11).toInt();
      t.tm_sec = val.substring(11, 13).toInt();
      time_t now = mktime(&t);
      struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
      settimeofday(&tv, nullptr);  // mise à l'heure de l’ESP32
      FormatteHeureDate();
    }
    return;
  }
  if (code.indexOf("IRMS1") == 0) {
    tic.IRMS1 = val.toInt();
    if (principal) {
      Intensite_M = val.toFloat();  //Phase 1 uniquement
      Intensite_M1 = Intensite_M;
    }
    return;
  }
  if (code.indexOf("IRMS2") == 0) {
    tic.IRMS2 = val.toInt();
    if (principal) Intensite_M2 = val.toFloat();
    return;
  }
  if (code.indexOf("IRMS3") == 0) {
    tic.IRMS3 = val.toInt();
    if (principal) Intensite_M3 = val.toFloat();
    return;
  }
  if (code.indexOf("URMS1") == 0) {
    tic.URMS1 = val.toInt();
    if (principal) {
      Tension_M = val.toFloat();  //phase 1 uniquement
      Tension_M1 = Tension_M;
    }
    return;
  }
  if (code.indexOf("URMS2") == 0) {
    tic.URMS2 = val.toInt();
    if (principal) Tension_M2 = val.toFloat();
    return;
  }
  if (code.indexOf("URMS3") == 0) {
    tic.URMS3 = val.toInt();
    if (principal) Tension_M3 = val.toFloat();
    return;
  }
  if (code.indexOf("SINSTS1") == 0) { tic.SINSTS1 = val.toInt(); return; }
  if (code.indexOf("SINSTS2") == 0) { tic.SINSTS2 = val.toInt(); return; }
  if (code.indexOf("SINSTS3") == 0) { tic.SINSTS3 = val.toInt(); return; }
  if (code == "SMAXSN" && ReacCACSI == 100) {  // Estimateur d'injection CACSI (le Linky ne fournit pas SINSTI)
    int Psout = principal ? PuissanceS_M : tic.PuissanceS;
    int Pinj = 0;
    if (Psout == 0) {  // estimation de la puissance d'injection si rien n'est soutiré
      int pPuissance;
      if (tic.IRMS3 != -1) {  // triphasé
        pPuissance = 150 + (tic.SINSTS1 == 0 ? -1 : 1) * tic.URMS1 * tic.IRMS1;  // marge de 150W
        pPuissance += (tic.SINSTS2 == 0 ? -1 : 1) * tic.URMS2 * tic.IRMS2;
        pPuissance += (tic.SINSTS3 == 0 ? -1 : 1) * tic.URMS3 * tic.IRMS3;
      } else {
        pPuissance = 150 + (tic.SINSTS == 0 ? -1 : 1) * tic.URMS1 * tic.IRMS1;
      }
      if (pPuissance < 0) Pinj = -pPuissance;  // "-" car on donne la valeur injectée
    }
    if (principal) {
      PuissanceI_M = Pinj;
      PVAI_M = Pinj;  //On egalise Pw et PVA
    } else {
      tic.PuissanceI = Pinj;  //Publié en MQTT (Linky_PuissanceI, Linky_Pw) pour une régulation "estimation CACSI" via HA
      tic.PVAI = Pinj;
    }
    return;
  }
  if (code.indexOf("STGE") == 0) {
    tic.STGE = val;
    tic.STGE.trim();
    if (tarifDepuisIci) {
      STGE = tic.STGE;                                 //Statut complet (MQTT)
      if (TempoRTEon == 0) STGEt = STGE.substring(1, 2);  //Tempo lendemain et jour sur 1 octet
    }
    return;
  }
  if (code.indexOf("LTARF") == 0) {
    tic.LTARF = val;
    tic.LTARF.trim();
    if (tarifDepuisIci && TempoRTEon == 0) LTARF = tic.LTARF;  //Option Tarifaire
    return;
  }
  if (code.indexOf("NGTF") == 0) {
    tic.NGTF = val;
    tic.NGTF.trim();
    if (tarifDepuisIci) NGTF = tic.NGTF;  //Calendrier Tarifaire
    return;
  }
  if (code.indexOf("EASF") == 0 && code.length() == 6) {  // EASF01..EASF10
    int i = code.substring(4).toInt();
    if (i >= 1 && i <= 10) tic.EASF[i - 1] = val.toInt();
    return;
  }
}

// Instance TIC à publier en MQTT : le Linky principal s'il est la source, sinon l'auxiliaire
TicData &TicPourMQTT() {
  return (Source == "Linky") ? ticPrincipal : ticAux;
}
bool LinkyDisponible() {  // Des données Linky (principale ou auxiliaire) existent
  return Source == "Linky" || LinkyAuxActif;
}
