# Journal des modifications — branche de travail (depuis V17.29)

Chaque étape = tests hôte verts (`python test/run_tests.py`) + build PlatformIO + mesure + commit.

## 1. Vue d'ensemble

| # | Commit | Contenu | Flash | RAM | Tests |
|---|---|---|---|---|---|
| 0 | `dd37251` | V17.29 d'origine | 1 897 559 | 91 760 | — |
| 1 | `269cdda` | CLAUDE.md, doc technique, `platformio.ini`, `tools/gen_web_gz.py` | — | — | — |
| 2 | `36ff197` | Harnais de tests hôte (mocks Arduino, 17 tests) | — | — | 635 vérif. |
| 3 | `66d6c0f` | Bugs B1, B2, B3, Vmin/Vmax, en-tête CSV | 1 898 435 | 91 760 | 638 |
| 4 | `500dd0d` | Pages web gzip (`WebGz.h`) | 1 733 583 | 91 648 | 645 |
| 5 | `c59b506` | Code mort, tables multi-sinus figées | 1 732 479 | 91 440 | 645 |
| 6 | `383e90f` | Docs : chiffrage flash par fonctionnalité | — | — | — |
| 7 | `4aa2cdb` | **Linky auxiliaire (S1)**, patchs B12/B15-B21/B27, parseurs Enphase | 1 734 015 | 92 904 | 679 |
| 15 | `HEAD` | Zendure : ID 0 à 2 présents publiés en MQTT (`Zendure_ID0..2`, W), octets 3-4 de l'en-tête lus comme compteur 16 bits (« Trame n° »), test sur trames réelles du 25/09 (§ 6) | 1 736 995 | 93 256 | 737 |
| 14 | `97124c7` | Zendure : trame de mesure reconnue à sa charge (le type change d'une session à l'autre : 0x010A, 0x0106), trame 02 00 de 3,6 s ignorée ; signe confirmé (négatif = injection) | 1 735 559 | 93 240 | 717 |
| 13 | `de1199b` | **Source Zendure 1CT-S** : écoute RS485 du bus 1CT-S ↔ SolarFlow (trames AA 55, CRC16), ID de mesure réglable (défaut 3), bloc Données brutes (§ 6) | 1 735 567 | 93 240 | 714 |
| 12 | `21f80cb` | Linky auxiliaire : estimateur d'injection CACSI aussi sur l'auxiliaire, entité MQTT `Linky_Pw` (W signés), flux TIC auxiliaire affiché dans Données brutes (tableau Linky) | 1 733 507 | 92 904 | 690 |
| 11 | `0214fb2` | MQTT : état publié dès que le Linky auxiliaire est actif, même sans source de puissance valide (Source = Pmqtt sans publication) | 1 732 815 | 92 904 | 690 |
| 10 | `3c79fd1` | Linky auxiliaire : sauvegarde des paramètres corrigée (chaîne JS vs `\|` ArduinoJson), état d'exécution `LinkyAuxActif` séparé du paramètre | 1 732 807 | 92 904 | 690 |
| 9 | `8bcdc36` | Raisons de reset du core 3.x (SW_CPU_RESET, EXT_CPU_RESET, TGWDT_CPU_RESET) ; page Données brutes : lignes NGTF, STGE, couleur Tempo du jour et du lendemain décodées depuis STGE | 1 732 815 | 92 904 | 686 |
| 8 | `7f505fb` | Historique 1 an en flux direct, plafond des lignes de diagnostic CSV (tas à 276 o constaté sur un routeur réel) | 1 732 435 | 92 904 | 686 |

Marge flash finale : **210 041 octets** (48 041 à l'origine).

Hashes GitHub (dépôt `zangdar05/Solar-Router-F1ATB`, branche `main`, historique rejoué sur V17.26) : V17.29 = `d22173d`, S1 = `4aa2cdb`, étape 8 = `7f505fb`.

## 0. Déploiement OTA validé (20/09/2026)
- Routeur de test : ESP32 Wroom, Source Linky, 2 actions, MQTT 20 s, mode expert. Avant mise à jour : RAM libre minimum **412 octets** (puis 276 après un appel `/ajax_histo1an`).
- Cause : CSV mensuel de 62 Ko (1 133 lignes « Puissances non reçues => Reset » sur 4 jours, un reset toutes les 5 mn pendant une coupure Linky) chargé entièrement dans un `JsonDocument` par `/ajax_histo1an`, appelé par l'accueil toutes les 5 mn. Corrigé à l'étape 8 ; CSV du routeur nettoyé (4 lignes conservées) par la page Import.
- OTA par `curl -F update=@firmware.bin http://<ip>/update` : 10 s, redémarrage en 5 s. Après : RAM libre 89 892, minimum 83 924 ; pages gzip servies (accueil 1 549 o) ; Linky, MQTT et actions inchangés.
- Reste cosmétique : « Dernier reset : Code inconnu » après un reset logiciel (codes RTC du core 3.x non mappés dans `get_reset_reason_text`).


## 2. Étape 7 — Linky auxiliaire (solution S1 de `04_architecture_mqtt_linky.md`)

### 2.1 Objectif
Lire la TIC du Linky en permanence sur un UART dédié, indépendamment de la source de mesure choisie pour le routage (JSY UxIx2/UxIx3, Shelly, Pmqtt…), et remonter ses données vers Home Assistant en MQTT. Le tarif (LTARF), la couleur Tempo (STGE), le calendrier (NGTF) et l'horloge Linky deviennent disponibles pour les conditions d'Actions même sans « Source = Linky ».

### 2.2 Ce qui a changé

| Fichier | Modification |
|---|---|
| `Linky.h` (nouveau) | `struct TicData` : index EAST/EAIT/EASF01-10, SINSTS/SINSTI (+ par phase), URMS/IRMS 1-3, LTARF/NGTF/STGE/DATE, état de l'estimateur de puissance active (moyennes, cosφ, `PuissanceS/I`), diagnostic (`nbTrames`, `nbErrChecksum`, `lastFrameMs`), tampon circulaire. Prototypes de `TicPourMQTT()` et `LinkyDisponible()` (le générateur Arduino ignore les fonctions renvoyant une référence) |
| `Source_Linky.ino` (réécrit) | `DecodeTIC(port, tic, principal)` : lecture des octets, découpe des groupes, checksum ; `DecodeGroupeTIC()` : affectation par étiquette ; `EstimePuissanceTIC()` : estimation W depuis la dérivée des index (algorithme d'origine, extrait). En mode `principal` les variables de régulation sont mises à jour exactement comme avant (`Energie_M_*`, `PuissanceS/I_M`, `PVAS/PVAI_M`, `PowerFactor_M`, `Tension_M`, `Intensite_M*`, CACSI/SMAXSN, `PuissanceRecue`). En mode auxiliaire seule `ticAux` est remplie, plus LTARF/STGE/STGEt/NGTF/horloge **si la source n'est pas un Linky** |
| `Solar_Router_V17_29.ino` | Globales `SerialAux` (UART1), `LinkyAux`, `pSerialAux`, `ticPrincipal`, `ticAux`, `DataRawLinkyAux[1024]` ; suppression des globales devenues internes à `TicData` (`moyPWS…`, `EASF01…10`, `LFon`, `IdxBufDecodLinky`). `setup()` : ouverture de l'auxiliaire (9600 7E1, RX = `RX2_[pSerialAux]`, TX = −1) après contrôle de conflit avec le port série 2 ou `Source == "Linky"`. `Task_LectureRMS()` : `LectureLinkyAux()` à chaque itération (cœur 0). Boucle 30 s : diagnostic Telnet + message si aucune trame depuis 60 s |
| `Stockage.ino` | Clés `LinkyAux`, `pSerialAux` (valeurs par défaut 0 si absentes : compatibilité des anciens `parametres.json`) |
| `EnvoiMQTT.ino` | `LinkyDisponible()` remplace `Source == "Linky"` pour LTARF/Code_Tarifaire/NGTF/STGE/EASF ; EASF publiés depuis `TicPourMQTT()` (boucle au lieu de 10 lignes) ; entités supplémentaires si auxiliaire : `Linky_EAST`, `Linky_EAIT` (Wh), `Linky_SINSTS`, `Linky_SINSTI` (VA), `Linky_PuissanceS`, `Linky_PuissanceI` (W estimés), `Linky_URMS1` (V), `Linky_IRMS1` (A) |
| `PagePara.h`, `JS_Para.h` | Lignes « Linky auxiliaire » (Oui/Non) et « GPIO RX du Linky auxiliaire » (16/26/18/5/21), visibles en mode expert quand la source n'est pas Linky ; sauvegardées dans `F.LinkyAux` / `F.pSerialAux` |
| `test/` | `test_linky_aux` : trame TIC sur `SerialAux` avec `Source = UxIx2` → `ticAux` renseigné, LTARF/STGEt pris sur l'aux, variables de régulation intactes, watchdog non réarmé, 8 entités `Linky_*` en discovery, JSON d'état valide ; avec `Source = Linky` pas de doublon `Linky_*`. Tests Linky existants adaptés (`ticPrincipal.EASF[i]`) |

### 2.3 Câblage et configuration
1. Sortie TIC du Linky → même montage optocoupleur que l'entrée Linky habituelle → GPIO RX choisi (ex. 26). Ne pas utiliser un GPIO du port série 2 ni un GPIO d'Action.
2. Page Paramètres, mode expert : « Linky auxiliaire = Oui », GPIO RX, MQTT activé (`MQTTRepet > 0`). Sauvegarder, redémarrer.
3. Vérification : Telnet (port 23) affiche toutes les 30 s `Linky auxiliaire : N trames, E erreurs checksum, EAST=…` ; messages « aucune trame depuis 60 s » / « trames de nouveau reçues » dans la liste des messages (page Données brutes).
4. Home Assistant : nouvelles entités `Linky …` sur l'appareil du routeur (discovery renouvelée toutes les 5 mn).

### 2.4 Limites
- Un seul Linky auxiliaire. La page Données brutes affiche son flux TIC (tableau Linky) en plus des données de la source (étape 12).
- Les énergies journalières Linky (`EnergieJour_*`) ne sont pas calculées pour l'auxiliaire : à faire dans HA (utility meter) à partir de `Linky_EAST/EAIT`.
- Le watchdog « puissance non reçue » reste lié à la source principale, par conception : en `Source = Pmqtt` sans publication sur `TopicP`, le routeur redémarre toutes les 2,5 min (constaté le 20/09 ; corrigé côté HA par une publication périodique).
- Entités MQTT de l'auxiliaire : `Linky_EAST`, `Linky_EAIT`, `Linky_SINSTS`, `Linky_SINSTI`, `Linky_PuissanceS`, `Linky_PuissanceI` (estimation CACSI si `ReacCACSI = 100`), `Linky_Pw` (S − I), `Linky_URMS1`, `Linky_IRMS1` ; l'état est publié même sans source de puissance valide.
- Non testé sur matériel : la trame TIC synthétique des tests couvre le décodage, pas la couche UART réelle (7E1, 9600) ni la charge du cœur 0 avec deux ports actifs (≈ 2 ms par appel, comme le Linky principal).

## 3. Étape 7 — patchs de bugs (`01_doc_technique.md` §8)

| Bug | Fichier | Correctif |
|---|---|---|
| B12 | `EnvoiMQTT.ino` | `TopicAct[60]` → `[100]` : titres d'Action longs (accents UTF-8) tronqués → ordres MQTT jamais reconnus |
| B15 | `JS_Para.h` | Validation de l'IP MQTT effective (`value == 0` au lieu de `.checked` sur un champ number) |
| B16 | `JS_Actions.h` | Apostrophe échappée dans `title`, quote parasite retirée |
| B17 | `JS_Actions.h` | Garde sur `GID("CACSI"+ReacCACSI)` (TypeError si valeur inattendue) |
| B18 | `JS_Commun.h` | URL `https://F1ATB.fr/fr` ; `setTimeout(() => location.reload(), 2000)` au lieu d'un rechargement immédiat |
| B19 | `EcranLCD.ino` | Page graphe 48 h rafraîchie (case 2 ajouté au switch 3 s) |
| B20 | `Source_ShellyProEm.ino` | `indexOf("shellypro3em") == 0` dans les deux branches : le 3EM-63 fonctionne en monophasé |
| B21 | `Source_EnphaseEnvoy.ino` | Énergies cumulées lues avec `LongJson()` (entier) au lieu de `ValJson()` (float 24 bits) |
| B27 | `Source_Externe.ino` | Limite de la réponse HTTP 400 → 800 caractères (trames bi-sonde/triphasé) |

Non corrigés (documentés) : B10 appels bloquants sur le cœur 1, B11 absence d'authentification serveur, B14 écriture LittleFS depuis la tâche WiFi, B9/B13 sans effet pratique.

## 4. Étape 7 — optimisation complémentaire
- `Source_EnphaseEnvoy.ino` : suppression de 5 parseurs JSON jamais appelés (`ULongJson`, `IntJson`, `ByteJson`, `UShortJson`, `ShortJson`). `ValJson`, `LongJson`, `myLongJson`, `StringJson`, `SubJson`, `PrefiltreJson` conservés (utilisés par Enphase, Shelly, RTE, MQTT).
- Discovery MQTT EASF : boucle au lieu de 10 appels explicites.
- Bilan de l'étape : +1,5 Ko de flash (code S1 ≈ +4 Ko, nettoyages ≈ −2,5 Ko) et +1,5 Ko de RAM (tampon aux 1 Ko + 2 × `TicData`).

## 5. Étapes 3 à 5 (rappel)
- Étape 3 : B1 écriture hors tableau `RMS_NomEtat[8]` ; B2 `STGE` tronqué et `STGEt` jamais renseigné en Linky ; B3 débordement de pile possible du JSON d'état MQTT (garde `RESTE()` + buffer 2 000 o, PubSubClient 2 300 o) ; `ParaEnCours` Vmin/Vmax initialisés ; en-tête CSV mensuel écrit une seule fois.
- Étape 4 : pages web compressées gzip, −165 Ko ; sources HTML/JS inchangées ; `/biSonde` séparé ; `run_tests.py` vérifie `WebGz.h`.
- Étape 5 : code mort C++/JS ; tables multi-sinus `const` (valeurs réellement utilisées, le recalcul au boot donnait 79 valeurs différentes des littéraux).

## 6. Étape 13 — Source Zendure 1CT-S

### 6.1 Objectif
Utiliser comme source de puissance maison le compteur Zendure 1CT-S (pince CT) qui pilote un SolarFlow 1600 AC+, sans matériel de mesure supplémentaire : le routeur écoute en parallèle le bus RS485 entre les deux appareils. Le protocole n'est pas du Modbus : sa structure a été établie à l'aide du firmware « ESP Modbus Spy » sur des captures réelles, puis validée par le CRC.

```
[01 00]  AA 55  01  cpt u16  FF  00 20  { ID u16 | taille u16 = 4 | valeur int32 (W) } × 4  CRC_fort CRC_faible
préfixe  début      compteur      long.   charge (big-endian)                                CRC16/MODBUS de AA 55 à la fin de la charge
```

La trame de mesure du 1CT-S (préfixe `01 00`) arrive toutes les 600 ms (115200 8N1). Elle porte les ID 1, 2, 3 et 15 (pas d'ID 0) ; sur l'installation observée (une seule pince), l'ID 3 porte la puissance, les ID 1 et 2 valent 0 et l'ID 15 la même valeur que l'ID 3 : c'est probablement la somme des entrées (non confirmable sans plusieurs pinces). Signe confirmé par l'allumage d'un appareil de ≈ 2 kW (capture du 25/09) : positif = soutirage, négatif = injection.

Les octets 3-4 (après `AA 55 01`) forment un **compteur 16 bits** incrémenté à chaque trame de l'émetteur (capture du 25/09 : `0x0BDF` → `0x0C24` sans saut, passage `0x0BFF` → `0x0C00`). Les « types » `0x010A`, `0x0106`, `0x0122`, `0x012E` relevés sur les premières captures n'étaient que l'octet haut de ce compteur.

| Trame | Préfixe | Type observé | Période | Charge |
|---|---|---|---|---|
| Mesure du 1CT-S | `01 00` | `01` + compteur 16 bits | 600 ms | 4 blocs [ID][4][int32] |
| Émetteur n° 2 (SolarFlow probable) | `02 00` | `01` + compteur 16 bits propre | 3,6 s, 10 à 85 ms après une trame de mesure sur 6 | 6 octets constants `FFFF 0002 FFFF` (bloc ID 0xFFFF, taille 2, valeur 0xFFFF ?) : acquittement ou signe de vie, aucune mesure |

Le routeur ne filtre donc pas sur l'en-tête : une trame est une mesure si sa charge est faite de blocs de 4 octets.

**MQTT** : les ID 0 à 2 présents dans la dernière trame de mesure sont publiés tels quels (W, + soutirée / − injectée) sous `Zendure_ID0`, `Zendure_ID1`, `Zendure_ID2` ; seuls les ID effectivement vus sont découverts (ici `Zendure_ID1` et `Zendure_ID2`), un ID apparu plus tard l'est au passage de discovery suivant (5 min). L'ID de régulation reste publié via `PuissanceS_M` / `PuissanceI_M`.

### 6.2 Ce qui a changé
| Fichier | Modification |
|---|---|
| `Source_Zendure.ino` (nouveau) | `Setup_Zendure()` (UART2 115200 8N1, RX seul), `LectureZendure()` (recherche AA 55, longueur, CRC, resynchronisation après trame tronquée), `TrameZendure()` (lecture de l'ID, puissances S/I, intégration des Wh, watchdog) |
| `Solar_Router_V17_29.ino` | Globales `Zendure_dataBrute`, `ZdBuf`, compteurs ; setup et lecture toutes les 10 ms dans le bloc port série ; alerte « port série non défini » |
| `Server.ino`, `PageBrute.h`, `JS_Brute.h` | Bloc « Données Zendure 1CT-S » : valeurs de tous les ID, ID utilisé, trames valides / rejetées |
| `PagePara.h`, `JS_Para.h` | Option « Zendure 1CT-S (RS485) » en fin de liste (non désactivée en mode AP), vitesse 115200 forcée, champ `EnphaseSerial` réutilisé comme ID de mesure |
| `EnvoiMQTT.ino` | Discovery et état `Zendure_ID0..2` (ID vus seulement) ; globales `ZdID[3]`, `ZdIDvus` |
| `test/` | `test_zendure` : trame capturée reproduite, soutirage ID 3, trame tronquée + trame en deux morceaux, ID 15 inversé, ID absent, CRC faux, trames réelles du 25/09 (compteur, 2 102 W) ; `test_zendure_mqtt` : discovery et état des ID 0 à 2 |

Aucune nouvelle clé dans `parametres.json`.

### 6.3 Câblage et configuration
1. Module TTL↔RS485 alimenté en 3,3 V : A/B en parallèle sur le bus 1CT-S ↔ SolarFlow, RO (ou TXD d'un module auto-direction) sur le RX du port série 2 choisi, masses reliées. Le TX n'est pas utilisé : le routeur n'émet jamais sur le bus.
2. Paramètres : Source = « Zendure 1CT-S (RS485) », Port série 2 = broches choisies (vitesse forcée à 115200).
3. Champ « ID de la mesure Zendure » : vide = 3 ; `15` pour l'ID 15 ; un ID négatif (`-3`) inverse le signe (inutile avec le 1CT-S, dont la convention est celle du routeur).

### 6.4 Limites
- Rôle des ID 1 et 2 (autres entrées) et de l'ID 15 (somme probable) non confirmé (une seule pince disponible) ; trames du préfixe `02 00` ignorées.
- Pas de VA ni de cos φ : `Pva_valide = false`. Les totaux d'énergie repartent des valeurs de minuit après un reset (comme la source MQTT).
- Watchdog : sans trame valide pendant ≈ 2 min, le routeur redémarre (« Puissances non reçues »), comme pour les autres sources. Un câblage bruité fait perdre des trames (compteur « rejetées ») : sur les captures du 24/09, environ une trame de mesure sur deux arrivait tronquée, la fin de la trame manquant entièrement.

## 7. Procédure après toute modification
```
python tools/gen_web_gz.py      # si une page web a changé
python test/run_tests.py        # doit finir par "0 echec(s)"
python -m platformio run        # depuis PowerShell, PYTHONIOENCODING=utf-8
git commit
```
Compilation Arduino IDE : ouvrir `Solar_Router_V17_29.ino`, core ESP32 3.3.x, partition custom ; `WebGz.h` et `Linky.h` sont dans le dossier du sketch.
