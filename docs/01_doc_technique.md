# Documentation technique — Routeur solaire F1ATB V17.29

Version analysée : 17.29 (août 2026), core ESP32 3.3.11, Arduino IDE 2.3.10, partition custom.

## 1. Vue d'ensemble

| Élément | Valeur |
|---|---|
| Cible | ESP32 Wroom (WiFi), WT32-ETH01 / ESP32-ETH01 (Ethernet LAN8720), cartes avec écran 2.4"/2.8"/3.2" (ILI9341/ST7789, tactile résistif XPT2046 ou capacitif CST820/GT911) |
| Rôle | Mesurer la puissance en entrée de maison, piloter jusqu'à 10 « Actions » (Triac gradateur, SSR, relais GPIO, PWM, relais HTTP distants) pour absorber le surplus PV |
| Sources de mesure (exclusives, une seule à la fois) | `NotDef` (simulation), `UxI` (ADC tension+courant), `Linky` (TIC standard), `UxIx2` (JSY-MK-194T Modbus RTU), `UxIx3` (JSY-MK-333 triphasé), `Enphase` (Envoy-S), `ShellyEm`, `ShellyPro`, `SmartG`, `HomeW`, `Pmqtt` (puissance reçue en MQTT), `Ext` (autre routeur F1ATB par HTTP) |
| Interfaces | HTTP :80 (pages + AJAX), Telnet :23 (console), Série USB 115200, MQTT (client, auto-discovery Home Assistant), OTA (ArduinoOTA + page /OTA), mDNS `hostname.local` |
| Persistance | LittleFS 140 K : `parametres.json`, `EnergieMinuit.eng`, `Mois_Wh_AAAAMM.csv` |
| Partition | `partitions.csv` : nvs 20K, otadata 8K, app0/app1 1900K chacune, coredump 64K, spiffs (LittleFS) 140K |

## 2. Organisation des fichiers

| Fichier | Lignes | Rôle |
|---|---|---|
| `Solar_Router_V17_29.ino` | 1999 | Historique, includes, **toutes les variables globales**, ISR, WPS, `setup()`, tâche cœur 0, `loop()` cœur 1, `GestionOverproduction()` (régulation), `InitGPIOs()`, énergie journalière, raisons de reset |
| `Actions.h/.cpp` | 352 | Classe `Action` : périodes horaires, conditions (température, tarif, autre action), sorties GPIO/PWM/HTTP |
| `Source_*.ino` | 2 100 | Un fichier par source de mesure ; chacun met à jour les mêmes globales `Puissance*_inst`, `Energie_M_*`, puis appelle `filtre_puissance()` |
| `EnvoiMQTT.ino` | 455 | Connexion broker, discovery HA, publication JSON d'état, callback de souscription (puissance, températures, ordres Actions) |
| `Stockage.ino` | 643 | Sérialisation/désérialisation `parametres.json` (ArduinoJson), énergies minuit, CSV mensuels |
| `Server.ino` | 1050 | Routes HTTP, réponses AJAX, import/export, OTA web, scan WiFi, cookie clé d'accès |
| `commonFx.ino` | 255 | IP↔String, Telnet/Série, console de commandes, `ReseT()`, `urlDecode` |
| `Heure.ino` | 139 | NTP/fuseaux, horloge Linky/interne/IT secteur, changement de jour |
| `RMS_Externes.ino` | 125 | Découverte des routeurs partenaires, état des actions distantes |
| `Temperature.ino` | 99 | DS18B20 (4 canaux), températures externes (autre ESP) ou MQTT |
| `Tempo_RTE.ino` | 132 | Couleur Tempo via API RTE (HTTPS) quand pas de Linky |
| `EcranLCD.ino/.h`, `EcranLED.ino/.h` | 1 283 | Affichage LovyanGFX, tactile, LEDs/OLED |
| `Page*.h`, `JS_*.h` | ≈ 5 000 | HTML/CSS/JS embarqués en `const char PROGMEM` |
| `OneWire.*`, `CST820.*`, `initGT911.*` | 1 500 | Bibliothèques embarquées (modifiées) |

## 3. Architecture d'exécution

### 3.1 Tâches et cœurs

| Contexte | Périodicité | Contenu |
|---|---|---|
| `Task_LectureRMS` (cœur 0, prio 10, pile 10 000) | `PeriodeProgMillis` selon source (Linky 2 ms, UxI 40 ms, UxIx2 400 ms, UxIx3 500-800 ms, Shelly/SmartG/HomeW 300 ms + ralenti, Enphase 2 s adaptatif, Ext 800 ms, Pmqtt 600 ms) | Acquisition de la source unique choisie par `Source` |
| `loop()` (cœur 1) | continu, `delay(1)` | OTA, `server.handleClient()`, Telnet, console série, histo 5 mn / 2 s, `GestionOverproduction()` toutes les 200 ms, `GestionMQTT()` toutes les 500 ms, LEDs 50 ms, températures 15 s, surveillance WiFi/Ethernet/puissance 30 s, RTE, écran |
| ISR `onTimer` | 100 µs | Découpe de phase Triac (compte 100 pas sur 10 ms, allume la gâchette quand `PulseComptage[0] > Retard[0]`) |
| ISR `currentNull` | front montant ZC (10 ms) | Déglitch 2 ms, `ITmode` monte jusqu'à 5, appelle `GestionIT_10ms()` |
| ISR `onTimer10ms` | 10 ms | `ITmode` descend ; si < 0 (pas de ZC) appelle `GestionIT_10ms()` en horloge interne |
| `GestionIT_10ms()` | chaque demi-onde | Multi-sinus / train de sinus / demi-sinus : commutation des GPIO selon `PulseOn[]`, `PulseTotal[]`, `PulseComptage[]` |

### 3.2 Chaîne de mesure → régulation → sortie

1. Source : calcule `PuissanceS_M_inst` / `PuissanceI_M_inst` (W soutiré / injecté maison), `PVAS/PVAI_M_inst` (VA), `Energie_M_Soutiree/Injectee` (Wh cumulés), éventuellement `*_T` (seconde sonde), `Tension_M`, `Intensite_M`, `PowerFactor_M`.
2. `filtre_puissance()` (Stockage.ino) : filtre RC (A=0,3 si `LissageLong`, sinon passe-tout) et découpe en entiers `PuissanceS_M`, `PuissanceI_M`, `PVAS_M`, `PVAI_M`.
3. `GestionOverproduction()` toutes les 200 ms : pour chaque Action i, `ParaEnCours()` donne le type de période (OFF/ON/PW/Triac) et les seuils ; calcul intégral (`IntegrErrorPw[i] += ErrorPw·Ki/10000`, borné 0..100) ou PID complet (mode expert), `RetardF[i]` = 100 − ouverture ; relais On/Off : hystérésis Vmin (On) / Vmax (Off) ; PWM via `ledcWrite`.
4. Sorties : Triac (`Retard[0]` lu par l'ISR 100 µs), relais GPIO (`RelaisOn()`/`Arreter()` avec `Tempo` anti-rebond), relais HTTP distants (`CallExterne()` GET bloquant, jusqu'à 5 s), multi-sinus (tables `tabPulseSinusOn/Total`).
5. `H_Ouvre_Equivalent()` toutes les 2 s : durée équivalente d'ouverture pleine puissance (sin² pour la découpe).

### 3.3 Convention des puissances
- `*_M` = maison (entrée), `*_T` = seconde sonde (Triac/production) présente seulement pour UxIx2, ShellyEm/Pro 2 voies.
- Toujours deux entiers positifs (S = soutiré, I = injecté), un seul non nul.
- `PmaxReseau` = 36 000 W borne toutes les valeurs (`PintMax`/`PfloatMax`).

## 4. Détail des fonctions principales

### 4.1 `Solar_Router_V17_29.ino`
| Fonction | Rôle / points notables |
|---|---|
| `setup()` | Watchdog tâche 180 s (réarmé uniquement dans la boucle 30 s si `PuissanceRecue`), LittleFS, hostname `RMS-ESP32-<chipId>`, recalcul des tables multi-sinus (déjà initialisées en dur), `ReadFichierParametres()`, `LectureConsoMatinJour()`, détection WT32-ETH01, `InitGPIOs()`, écran, réseau (Ethernet ou WiFi STA → WPS → AP `192.168.4.1`), NTP, Telnet, serveur web, OTA, setup de la source, création tâche cœur 0, interruption ZC, deux timers matériels |
| `loop()` | Voir 3.1. Contient la logique de reset : WiFi perdu > `ComSurv`×30 s, Ethernet, puissance non reçue pendant 5 cycles de 30 s, AP_STA > 5 mn |
| `GestionOverproduction()` | Régulation (3.2). `ReacCACSI` : gain × sur Ki quand injection (2/4/8) ; 100 = estimateur d'injection CACSI |
| `InitGPIOs()` | Affecte GPIO Triac/ZC selon `pTriac` et type de carte, port série `pSerial` → `RX2_[]/TX2_[]`, DS18B20, entrées ADC UxI |
| `EnergieQuotidienne()` | Énergies du jour = total − référence minuit (`EAS_M_J0`…) |
| `WiFiEvent()` / `wpsStop()` | Appairage WPS (bibliothèque 3.3.6+) |
| `print_cpu_reset_reason()` | Raison du dernier reset des deux cœurs |

### 4.2 `Source_Linky.ino`
- `Setup_Linky()` : `MySerial` (UART2) 9600 bauds 7E1, buffer RX 4096, pins `RX2_[pSerial]`.
- `LectureLinky()` : lit tous les octets disponibles dans le tampon circulaire `DataRawLinky[4000]` (exposé à la page Données brutes), détecte LF/CR, découpe `code TAB val TAB checksum`, vérifie le checksum seulement pour EAST/EAIT/SINSTS/SINSTI.
- Puissance active soutirée : dérivée des index EAST (Wh) lissée (`moyPWS`), bornée par la puissance apparente SINSTS ; cosφ = moyPWS/moyPVAS. Idem injection avec EAIT/SINSTI.
- CACSI (`ReacCACSI == 100`) : injection estimée sur `SMAXSN` via URMS×IRMS − 150 W.
- Champs remontés : DATE (horloge si `Horloge==1`), IRMS1-3, URMS1-3, SINSTS1-3, STGE, LTARF, NGTF, EASF01-10.
- Sentinelle watchdog : `PuissanceRecue = true` à chaque groupe DATE.

### 4.3 `EnvoiMQTT.ino`
| Fonction | Rôle |
|---|---|
| `GestionMQTT()` | Active si `ModeReseau < 2` et (`MQTTRepet > 0` ou température MQTT ou `Source == "Pmqtt"` ou `subMQTT == 1`) |
| `testMQTTconnected()` | Connexion avec LWT `<prefixEtat>/<device>/Available`, souscriptions : `TopicT[c]`, `TopicP`, `<device>/<TitreAction>` ; construit `DEVICE` (JSON device HA) ; échec → 30 s de pénalité |
| `envoiVersMQTT()` | Toutes les `MQTTRepet` s : discovery si `!Discovered` (remis à faux toutes les 5 mn), puis état |
| `sendMQTTDiscoveryMsg_global()` | Un message `config` retained par entité : puissances/énergies M (et T selon source), températures, LTARF/Code_Tarifaire, RTE, NGTF/STGE/EASF (Linky), Enphase, UxIx3, actions (Ouverture, Actif, Durée, Force) |
| `SendDataToHomeAssistant()` | Un seul JSON d'état sur `<prefixEtat>/<device>_state` (buffer 1 200 octets) |
| `callback()` | Réception : température `{"temperature":x}`, puissance `{"Pw":..,"Pva":..,"Pf":..}`, ordres Action `{"tOnOff":min,"Mode":"Decoupe|OnOff|Multi|Train|PWM|Demi|Inactif","Periode":n,"SeuilOn":w,"SeuilOff":w,"OuvreMax":%}` (sauvegarde en flash si Mode/Periode) |

### 4.4 `Stockage.ino`
- `SerializeConfiguration()` / `DeserializeConfiguration()` : ≈ 70 clés + tableau `Actions[]` (chaque action : Actif, Titre, Host, Port, OrdreOn/Off, ForceOuvre, Repet, Tempo, Kp/Ki/Kd, PID, NbPeriode, `Periodes[]` : Type, Hfin, Vmin, Vmax, ONouvre, Tinf, Tsup, Hmin, Hmax, CanalTemp, SelAct, Ooff, O_on, Tarif).
- Compatibilité ascendante : `Reactivite` (V15) → Ki, `Périodes` avec accent, valeurs par défaut via `| defaut` ou `isNull()`.
- `RecordEnergieMinuit()` / `RecordEnergieEncours()` / `LectureConsoMatinJour()` : références minuit et compteurs float (UxI/UxIx3) dans `EnergieMinuit.eng`.
- `Record_Data()` : CSV mensuel `Date,soutiré,injecté,[T soutiré],[T injecté],heure,message` ; purge du fichier le plus ancien à 80 % d'occupation.

### 4.5 `Server.ino` — endpoints
| Route | Réponse | Utilisée par |
|---|---|---|
| `/`, `/MainJS1..3` | Accueil | graphes temps réel |
| `/Para`, `/ParaJS1..2`, `/ParaCommunJS`, `/ParaFixe` (parametres.json brut), `/ParaVar` (JSON dynamique), `/ParaNew` (POST JSON complet → `DeserializeConfiguration` + `EcritureEnROM`) | Paramètres | |
| `/Actions`, `/ActionsJS1..4`, `/PinsActionsJS`, `/ShowAction`, `/UpdateK` | Actions, réglage PID en direct (restauré après 6 s sans requête) | |
| `/Brute`, `/BruteJS1..2`, `/ajax_dataRMS` | Données brutes de la source (Linky : delta du tampon `DataRawLinky`) | |
| `/ajax_data` | Trame maître→esclave : `Deb`,date,source,LTARF,STGEt,temp,Pva_valide GS puissances/énergies M GS [T ou `Fake`] GS FS tensions/courants FS `Fin` | `Source_Externe` |
| `/ajax_data10mn`, `/ajax_histo48h`, `/ajax_histo1an` (CSV LittleFS en JSON chunké), `/ajax_dataESP32`, `/ajax_etatActions` (+ forçage avec clé), `/ajax_etatActionX`, `/ForceAction`, `/ajax_Temperature`, `/ajax_Noms` (nom + températures + actions : trame US/FS/ES) | | |
| `/Export`, `/export_file`, `/ListeFile`, `/import` (POST fichier : `.json` paramètres ou `EnergieMinuit.eng` ou autre) | Import/export | |
| `/OTA`, `/update` (POST firmware), `/restart`, `/Wifi`, `/AP_ScanWifi`, `/AP_SetWifi`, `/Heure`, `/HourUpdate`, `/Couleurs`, `/CouleurUpdate`, `/SetGPIO`, `/commun.css`, `/favicon.ico`, `/manifest.json` | | |

Séparateurs : `ES`=27, `FS`=28, `GS`=29, `RS`=30, `US`=31.

### 4.6 `Actions.cpp`
| Méthode | Rôle |
|---|---|
| `ParaEnCours(Heure, T, Ltarfbin, Retard)` | Parcourt les périodes (`Hdeb..Hfin` en heure décimale ×100) ; conditions : température (hystérésis si `Tinf<Tsup<150 °C`, sinon seuils simples), tarif (`Ltarfbin & Tarif`), action de référence (`SelAct` : Hmin/Hmax durée équivalente, Ooff/O_on ouverture) ; forçage `tOnOff` (>0 On avec `ForceOuvre`, <0 Off) |
| `RelaisOn()` / `Arreter()` | GPIO, PWM (`ledcWrite`) ou HTTP distant avec `Tempo` mini entre commutations et `Repet` |
| `InitGpio(Fpwm)` | `OrdreOn = "gpio|niveau"` ; PWM 8 bits canal = index |
| `CallExterne()` | GET HTTP bloquant (2 tentatives, timeout 5 s) |
| `TypeEnCours()`, `Valmin()`, `Valmax()` | **Non utilisées** (code mort) |

### 4.7 Autres
- `Heure.ino` : `Horloge` 0 NTP (`configTzTime`, 7 fuseaux), 1 Linky (DATE), 2 manuelle, 3/4 comptage IT 10/20 ms, 5 routeur maître. `JourHeureChange()` : RAZ `H_Ouvre` à 6 h, sauvegarde CSV + énergies à minuit.
- `RMS_Externes.ino` : jusqu'à 7 routeurs partenaires (`RMS_IP[1..7]`), note de qualité `RMS_Note`, état des actions distantes pour conditions `SelAct >= 10`.
- `Temperature.ino` : canaux `tempInt` (DS18B20 par index), `tempExt` (autre routeur), `tempMqtt`, validité décrémentée (5 mesures de tolérance).
- `Tempo_RTE.ino` : `services-rte.com/cms/open_data/v1/tempoLight` (HTTPS `setInsecure`), horaires de consultation fixes, `STGEt` = code hexa lendemain (4/8/C).
- `commonFx.ino` : console (`ssid:`, `password:`, `restart`, `dispPw`, `dispAct`, `T:`/`P:` test train de sinus, `R:x` trace PID, `Offset:`, `partition`, `ETH01`).

### 4.8 Interface web embarquée (`Page*.h`, `JS_*.h`) — 231 Ko de chaînes PROGMEM
| Page | Chaînes | Endpoints | Notes |
|---|---|---|---|
| Accueil | `MainHtml` 6 K, `MainJS1..3` 46 K | `/ajax_data` (2 s), `/ajax_data10mn`, `/ajax_histo48h` (5 mn), `/ajax_histo1an`, `/ajax_etatActions` (3,5 s) | 11 graphes SVG réordonnables (localStorage), forçage ±30 mn, iframes des autres routeurs ; `var biSonde` injecté en tête de `/MainJS1` |
| Données brutes | `PageBrute` 4 K, `PageBruteJS1..2` 24 K | `/ajax_dataRMS?idx=`, `/ajax_dataESP32` (5 s) | Le flux Linky est réassemblé côté navigateur (STX/LF/TAB) ; tables de libellés Linky/UxIx2/Enphase en dur |
| Paramètres | `ParaHtml` 22 K, `ParaJS1..2` 23 K | `/ParaFixe`, `/ParaVar`, `POST /ParaNew` (objet `F` complet) | ≈ 60 champs ; affichage conditionnel par `Source`, `ModePara`, `ModeReseau`, `ESP32_Type` ; **`#ligneTopicP` visible seulement si `Source == "Pmqtt"`** ; bloc broker `#Zmqtt` seulement en expert |
| Actions | `ActionsHtml` 10 K, `ActionsJS1..4` 56 K | `/PinsActionsJS`, `/ShowAction` (200 ms), `/UpdateK`, `POST /ParaNew` | Action = 14 champs + ≤ 8 périodes × 14 champs ; suppression par `Actif = -1` puis compactage ; `OrdreOn = "gpio|niveau"` pour une sortie locale |
| Heure, Export, Couleurs, WiFi, OTA, Clé | 28 K | `/HourUpdate`, `/ListeFile`, `/export_file`, `/import`, `/CouleurUpdate`, `/AP_ScanWifi`, `/AP_SetWifi`, `/update`, `/CleUpdate` | OTA charge jQuery depuis un CDN Google et un iframe f1atb.fr |
| Communs | `ParaCommunJS` 5 K, `CommunCouleurJS` 5 K, `CommunCSS` 1,5 K, favicons 1,1 K | `/ParaCommunJS`, `/CommunCouleurJS`, `/commun.css` | Palette de 19 couleurs sérialisée en hex concaténé dans `Couleurs` |

### 4.9 Écrans
- LCD (`EcranLCD.ino`, LovyanGFX) : 7 pages par glissé (accueil, courbe 10 mn, courbe 48 h, messages, réseau, fond couleur, jauge) + page de forçage ; tactile résistif XPT2046 ou capacitif CST820/GT911 ; extinction après `DurEcran`, réveil tactile ou GPIO 35.
- LED/OLED (`EcranLED.ino`) : 2 LEDs (réseau / routage) ou SSD1306/SH1106 I²C avec bandeau puissance et % d'ouverture.
- Coût flash dominant : LovyanGFX (deux pilotes de panneau + OLED) et la fonte `FreeSansBold18pt7b` (≈ 12 Ko, 2 usages).

### 4.10 Enphase et Shelly Pro
- `Source_EnphaseEnvoy.ino` (685 lignes) : jeton via `enlighten.enphaseenergy.com` puis `entrez.enphaseenergy.com` (HTTPS `setInsecure`), jeton persisté dans `/tokenenphase.json`, lecture `/ivp/meters/readings` en keep-alive ; héberge le **parseur JSON maison** (`ValJson`, `StringJson`, `SubJson`, `LongJson`, `IntJson`… 11 variantes quasi identiques) utilisé par tout le projet.
- `Source_ShellyProEm.ino` (346 lignes) : `Shelly.GetDeviceInfo` puis `Shelly.GetStatus`, 4 branches (3EM triphasé, 3EM mono, EM50 2 voies) ; numéro de voie lu dans `EnphaseSerial`.

## 5. Modèle de données `parametres.json` (extraits clés)
| Clé | Type | Rôle |
|---|---|---|
| `Source` | string | Source de mesure exclusive |
| `pSerial`, `Serial2V` | byte, uint | Index pins UART2 (`RX2_[]`/`TX2_[]`), vitesse (0 = défaut de la source) |
| `pTriac`, `pUxI`, `pTemp` | byte | Index de tables de GPIO |
| `ESP32_Type` | byte | 0 inconnu, 1 Wroom, 2/3 cartes relais, 4-9 écrans, 10 Ethernet, 101 écran 3.2" |
| `ModePara` | 0/1 | Standard / expert. **En standard, `MQTTRepet` et `subMQTT` sont forcés à 0 à chaque sauvegarde** |
| `ModeReseau` | 0/1/2 | Internet / LAN seul / AP isolé (MQTT et RTE désactivés si 2) |
| `MQTTRepet`, `MQTTIP`, `MQTTPort`, `MQTTUser`, `MQTTPwd`, `MQTTPrefix`, `MQTTPrefixEtat`, `MQTTdeviceName`, `TopicP`, `subMQTT` | | Publication (période s), broker, préfixes discovery/état, topic puissance entrante, souscription ordres |
| `RMSextIP`, `RMS_IP1..7` | ulong | IP source externe / Shelly / SmartG / HomeWizard ; routeurs partenaires |
| `EnphaseUser/Pwd/Serial` | | Enphase ; `EnphaseSerial` sert aussi de n° de voie Shelly (0/1, 3 = 3EM, 30+ Gen3, 63 = 3EM-63) |
| `Horloge`, `idxFuseau`, `ntpServer` | | Horloge |
| `Actions[]` | array | Voir 4.4 |

## 6. Flux MQTT (état actuel)
- Topic état : `<MQTTPrefixEtat>/<MQTTdeviceName>_state` (JSON plat), disponibilité `<MQTTPrefixEtat>/<device>/Available`.
- Discovery : `<MQTTPrefix>/sensor|binary_sensor/<device>_<Var>/config`.
- Entrées : `TopicP` (puissance, seulement si `Source == "Pmqtt"`), `TopicT[c]` (températures), `<device>/<TitreAction>` (ordres, si `subMQTT == 1`).
- Contenu Linky publié (si `Source == "Linky"`) : LTARF, Code_Tarifaire, NGTF, STGE, EASF01-10, Energie_M_*, PuissanceS/I_M, Tension_M, Intensite_M, PowerFactor_M.

## 7. Limites actuelles du projet
| Limite | Conséquence |
|---|---|
| **Une seule source** (`Source`) alimente la régulation, l'affichage et les énergies | **Levée partiellement** : le Linky auxiliaire (`LinkyAux`, `Linky.h`) lit la TIC sur un second UART et publie index/tarif/Tempo en MQTT quelle que soit la source (voir `05_journal_modifications.md` §2). Les autres combinaisons (Shelly + JSY…) restent exclusives |
| Port série applicatif `MySerial` (UART2) pour la source ; `SerialAux` (UART1, RX seul) pour le Linky auxiliaire | Un JSY et un Linky peuvent coexister (JSY = source, Linky = auxiliaire) |
| MQTT sortant et souscription seulement en mode expert | Le mode standard écrase `MQTTRepet`/`subMQTT` à 0 |
| Puissance MQTT entrante ignorée si `Source != "Pmqtt"` | Pas de « source secondaire » |
| Régulation et serveur web sur le même cœur avec des appels bloquants (`CallExterne` 5 s, `waitForConnectResult` 10 s, Shelly/RTE) | Gel possible de la régulation à 200 ms pendant plusieurs secondes |
| Aucune authentification sur `/ParaNew`, `/ParaFixe` (mots de passe WiFi/MQTT/Enphase en clair), `/SetGPIO`, `/restart`, `/update`, `/import`, `/export_file` (lecture/suppression de tout fichier) ; clé d'accès = cookie en clair, seulement pour les pages HTML | Sécurité reposant entièrement sur l'isolement du LAN |
| Buffers fixes (`SendDataToHomeAssistant` 1 200 o, MQTT 1 700 o, discovery 800 o) | Troncature ou dépassement possible avec 10 actions + Linky (voir bugs) |
| Réponses AJAX construites en `String` (histogramme 48 h ≈ 20-30 Ko) | Fragmentation du tas ; les JS ont déjà été découpés pour cette raison |
| Flash app 1 900 K, partition sans marge pour de nouvelles bibliothèques (mbedTLS, LovyanGFX, WPS, Ethernet inclus) | Toute fonction ajoutée doit être compensée (voir `03_optimisation_flash.md`) |
| Pas de tests automatisés d'origine ; code monolithique en `.ino` avec ≈ 300 globales | Régressions fréquentes dans l'historique (V17.11→17.13) |
| OTA impossible depuis < V17 (table de partitions changée) | |
| ISR appellent `digitalWrite` sur jusqu'à 10 GPIO toutes les 10 ms | OK sur core 3.x, mais pas de protection concurrence sur `NbActions`, `Actif[]` (int volatile, écrits par `loop`) |

## 8. Bugs et incohérences relevés (V17.29 d'origine)
| # | Fichier:ligne | Description | Gravité |
|---|---|---|---|
| B1 | `Solar_Router_V17_29.ino:1029` | `RMS_NomEtat[LES_ROUTEURS_MAX] = "";` dans la boucle : écriture hors tableau (index 8 d'un tableau de 8) à chaque itération ; devrait être `RMS_NomEtat[i]` | Moyenne (corruption mémoire au boot) |
| B2 | `Source_Linky.ino:222-230` | Avec Linky et `TempoRTEon == 0`, `STGE` est écrasé par son 2ᵉ caractère (`substring(1,2)`) : le registre de statut complet publié en MQTT est faux ; `STGEt` (couleur du lendemain, utilisée par l'accueil, l'écran et les esclaves) n'est **jamais** renseigné en mode Linky | Moyenne (fonctionnel Tempo) |
| B3 | `EnvoiMQTT.ino:SendDataToHomeAssistant` | `len` peut dépasser 1 200 (Linky + 10 actions ≈ 1 500 o) ; `sizeof(value) - len` devient négatif → `size_t` géant → **débordement de pile** | Haute (rare mais crash) |
| B4 | `Solar_Router_V17_29.ino:1045-1070` | Tables multi-sinus recalculées à chaque boot alors qu'elles sont initialisées en dur (« optimisation Michy ») : temps de boot et code inutiles ; `tabPulseSinusTotal[I] = -1` sur `uint8_t` | Faible |
| B5 | `Server.ino:handleAjaxRMS` | En `Source == "Ext"`, un échec de connexion sort sans `server.send()` : le navigateur attend le timeout | Faible |
| B6 | `Actions.cpp` | `TypeEnCours`, `Valmin`, `Valmax` : code mort ; `Definir`, `Activer` déclarés sans définition | Faible (flash) |
| B7 | `Solar_Router_V17_29.ino` | Globales inutilisées : `DateCeJour`, `Cle_ROM`, `idxPromDuJour`, `baseTick10ms` | Faible |
| B8 | `Stockage.ino:DeserializeConfiguration` | `hostname = conf["hostname"] \| hostname;` en double ; `RecordFichierParametres()` n'ouvre pas le fichier en vérifiant le succès | Faible |
| B9 | `Source_UxIx2.ino:LectureUxIx2` | `ByteArray[j] << 24` promu en `int` signé : valeur négative si octet ≥ 0x80 (énergie > 214 MWh en 0,1 Wh, improbable) | Faible |
| B10 | `Solar_Router_V17_29.ino:loop` | `WiFi.waitForConnectResult(10000)` et `delay(5000)` avant reset bloquent la régulation Triac sur cœur 1 | Design |
| B11 | `Server.ino:handleParaNew` | Aucune vérification de la clé d'accès côté serveur (seule la page HTML est protégée) | Sécurité LAN |
| B12 | `EnvoiMQTT.ino:callback` | `TopicAct[60]` : `MQTTdeviceName + "/" + Titre` tronqué silencieusement si > 59 caractères → ordre jamais reconnu | Faible |
| B13 | `Source_Linky.ino` | `code.indexOf("SINSTI") == 0` accepte aussi `SINSTI1..3` hypothétiques ; `EAST`/`EAIT` acceptent tout préfixe (`EASTvalide` n'existe pas dans la TIC, sans effet) | Info |
| B14 | `Solar_Router_V17_29.ino:WiFiEvent` | `EcritureEnROM()` (écriture LittleFS + JSON) appelé depuis la tâche événement WiFi, concurrent de `loop()` | Faible |
| B15 | `JS_Para.h:254` | `!GID("MQTTRepet").checked` sur un `<input type=number>` : la validation de l'IP MQTT n'est jamais exécutée | Faible |
| B16 | `JS_Actions.h:98`, `:286` | Apostrophe non échappée dans un attribut `title`, quote parasite dans un `div` | Faible (HTML) |
| B17 | `JS_Actions.h:1239` | `GID("CACSI"+ReacCACSI)` : `TypeError` si `ReacCACSI` ∉ {1,2,4,8,100} | Faible |
| B18 | `JS_Commun.h:80`, `:104` | URL `https:F1ATB.fr` malformée ; `setInterval(location.reload(), 2000)` recharge immédiatement | Faible |
| B19 | `EcranLCD.ino:76-96` | Page graphe 48 h jamais rafraîchie ; `RMS_Note[i] = false/true` sur un compteur numérique (`:522,533`) | Faible |
| B20 | `Source_ShellyProEm.ino:106` vs `:156,195` | `indexOf("shellypro3em")==0` puis `== "shellypro3em"` : le 3EM-63 en monophasé ne fonctionne pas | Moyenne (matériel concerné) |
| B21 | `Source_EnphaseEnvoy.ino:364,396` | Compteurs `long` alimentés par `ValJson()` (float 24 bits) : perte de précision > 16,7 MWh | Faible |
| B22 | `JS_Actions.h:391-465` | `touchMove/mouseMove/NewPosition` : code mort (≈ 2,6 Ko) | Flash |
| B23 | `PagePara.h:27` | Sélecteur CSS `#ligneTopicT` mort (ids réels `ligneTopicT0..3`) | Info |
| B24 | `Solar_Router_V17_29.ino` (setup) | Les tables multi-sinus littérales étaient écrasées au boot par un recalcul donnant 79 valeurs différentes sur 101 | Info (corrigé : tables figées aux valeurs calculées) |
| B25 | `Stockage.ino:Record_Data` | `Record_Conf` non renseigné à la création du CSV mensuel : en-tête réécrit au 2ᵉ enregistrement | Faible (corrigé) |
| B26 | `Actions.cpp:ParaEnCours` | `P.Vmin`/`P.Vmax` non initialisés hors période (non lus tant que `Type <= 1`) | Latent (corrigé) |
| B27 | `Source_Externe.ino:45` | Réponse > 400 caractères jetée silencieusement ; une trame bi-sonde + en-têtes HTTP s'en approche | Faible |

Corrigés dans ce dépôt (commit `66d6c0f`, tests associés) : B1, B2, B3, B25, B26. Les autres sont documentés, non corrigés (pas de matériel de validation ou hors périmètre). Voir `03_optimisation_flash.md` pour les mesures et `test/README.md` pour la non-régression.
