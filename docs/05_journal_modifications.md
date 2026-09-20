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
| 8 | `7f505fb` | Historique 1 an en flux direct, plafond des lignes de diagnostic CSV (tas à 276 o constaté sur un routeur réel) | 1 732 435 | 92 904 | 686 |

Marge flash finale : **213 165 octets** (48 041 à l'origine).

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
- Un seul Linky auxiliaire ; pas de page « Données brutes » dédiée (le tampon de 1 Ko sert au décodage et au diagnostic Telnet).
- Les énergies journalières Linky (`EnergieJour_*`) ne sont pas calculées pour l'auxiliaire : à faire dans HA (utility meter) à partir de `Linky_EAST/EAIT`.
- Le watchdog « puissance non reçue » reste lié à la source principale, par conception.
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

## 6. Procédure après toute modification
```
python tools/gen_web_gz.py      # si une page web a changé
python test/run_tests.py        # doit finir par "0 echec(s)"
python -m platformio run        # depuis PowerShell, PYTHONIOENCODING=utf-8
git commit
```
Compilation Arduino IDE : ouvrir `Solar_Router_V17_29.ino`, core ESP32 3.3.x, partition custom ; `WebGz.h` et `Linky.h` sont dans le dossier du sketch.
