// ************************************************************************
// * Source Zendure 1CT-S : écoute du bus RS485 entre le 1CT-S et le      *
// * SolarFlow (protocole propriétaire, 115200 8N1, une trame / 600 ms)   *
// ************************************************************************
// Structure déduite des captures (validée par le CRC) :
//   [préfixe 0-2 o] AA 55 01 <compteur u16> FF <longueur u16> <charge> <CRC16/MODBUS poids fort d'abord>
//   CRC calculé de AA 55 à la fin de la charge. Le compteur s'incrémente à chaque trame de l'émetteur :
//   la trame de mesure du 1CT-S (préfixe 01 00, toutes les 600 ms) se reconnaît donc à sa charge en blocs
//   [ID u16][taille u16 = 4][valeur int32 en W], big-endian. La trame du préfixe 02 00 (toutes les 3,6 s, charge FFFF 0002 FFFF) est ignorée.
//   ID observés : 1, 2, 3 (entrées de mesure) et 15 (égal à la somme des entrées sur les captures).
// L'ID lu est le paramètre EnphaseSerial (vide = 3, entrée de mesure par défaut du 1CT-S) ;
// un ID négatif inverse le signe. Convention du 1CT-S : positif = soutirage réseau, négatif = injection.
// Les ID 0 à 2 présents sont en plus publiés tels quels en MQTT (Zendure_ID0..2).

void Setup_Zendure() {
  Serial2V = 115200;  //On force la vitesse
  MySerial.setRxBufferSize(SER_BUF_SIZE);
  MySerial.begin(Serial2V, SERIAL_8N1, RXD2, -1);  // RX seul : le routeur n'émet jamais sur le bus
  ZdN = 0;
}

// Trame complète au CRC valide, p[0..1] = AA 55
void TrameZendure(const uint8_t *p, int n) {
  ZdNbOK++;
  int id = EnphaseSerial.toInt();
  bool inverse = id < 0;
  id = abs(id);
  if (id == 0) id = 3;
  String S = "Trame n°" + String((p[3] << 8) | p[4]) + " :";
  bool trouve = false;
  float Pw = 0;
  int32_t vals[3];
  uint8_t vus = 0;
  int fin = 8 + ((p[6] << 8) | p[7]);
  if (fin == 8 || (fin - 8) % 8) return;  // pas une trame de mesure
  for (int k = 8; k + 8 <= fin; k += 8) {
    if (((p[k + 2] << 8) | p[k + 3]) != 4) return;
    int ident = (p[k] << 8) | p[k + 1];
    int32_t v = (int32_t)(((uint32_t)p[k + 4] << 24) | ((uint32_t)p[k + 5] << 16) | ((uint32_t)p[k + 6] << 8) | p[k + 7]);
    S += " ID" + String(ident) + "=" + String(v);
    if (ident < 3) {
      vals[ident] = v;
      vus |= 1 << ident;
    }
    if (ident == id) {
      trouve = true;
      Pw = inverse ? -v : v;
    }
  }
  for (int i = 0; i < 3; i++)
    if (vus & (1 << i)) ZdID[i] = vals[i];
  ZdIDvus = vus;
  Zendure_dataBrute = S + "<br>ID utilisé : " + String(inverse ? -id : id) + (trouve ? "" : " (absent de la trame)") +
                      "<br>Trames valides : " + String(ZdNbOK) + ", rejetées (CRC) : " + String(ZdNbKO);
  if (!trouve) return;

  unsigned long tps = millis();
  if (ZdLastMillis == 0) {  // première mesure : reprise des totaux relus au démarrage (valeurs de minuit)
    EASfloat = Energie_M_Soutiree;
    EAIfloat = Energie_M_Injectee;
  }
  float dt = (ZdLastMillis == 0 || tps - ZdLastMillis > 5000) ? 0 : (tps - ZdLastMillis) / 3600000.0;  // heures
  ZdLastMillis = tps;
  Pw = PfloatMax(Pw);
  if (Pw >= 0) {
    PuissanceS_M_inst = Pw;
    PuissanceI_M_inst = 0;
    PVAS_M_inst = Pw;
    PVAI_M_inst = 0;
    EASfloat += Pw * dt;
    Energie_M_Soutiree = long(EASfloat);
  } else {
    PuissanceS_M_inst = 0;
    PuissanceI_M_inst = -Pw;
    PVAS_M_inst = 0;
    PVAI_M_inst = -Pw;
    EAIfloat += -Pw * dt;
    Energie_M_Injectee = long(EAIfloat);
  }
  Pva_valide = false;
  filtre_puissance();
  EnergieActiveValide = true;
  PuissanceRecue = true;  //Reset du Watchdog
  if (cptLEDyellow > 30) {
    cptLEDyellow = 4;
  }
}

void LectureZendure() {
  while (MySerial.available() && ZdN < (int)sizeof(ZdBuf)) ZdBuf[ZdN++] = MySerial.read();
  int i = 0;
  for (;;) {
    while (i + 1 < ZdN && !(ZdBuf[i] == 0xAA && ZdBuf[i + 1] == 0x55)) i++;
    if (i + 8 > ZdN) break;
    int len = 8 + ((ZdBuf[i + 6] << 8) | ZdBuf[i + 7]) + 2;
    if (len > (int)sizeof(ZdBuf)) {  // longueur aberrante : faux en-tête
      i += 2;
      continue;
    }
    if (i + len > ZdN) break;  // trame incomplète : attend la suite
    uint16_t crc = 0xFFFF;
    for (int k = i; k < i + len - 2; k++) {
      crc ^= ZdBuf[k];
      for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    if (crc == ((ZdBuf[i + len - 2] << 8) | ZdBuf[i + len - 1])) {
      TrameZendure(ZdBuf + i, len);
      i += len;
    } else {  // trame tronquée (octets perdus) ou bruitée : on cherche l'en-tête suivant
      ZdNbKO++;
      i += 2;
    }
  }
  if (i == 0 && ZdN == (int)sizeof(ZdBuf)) i = ZdN;  // tampon plein sans trame : purge
  memmove(ZdBuf, ZdBuf + i, ZdN - i);
  ZdN -= i;
}
