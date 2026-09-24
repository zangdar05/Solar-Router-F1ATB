# Espace mémoire de l'ESP32 et optimisation flash

Mesures : `python -m platformio run` (core ESP32 3.3.12, `partitions.csv` du projet). Les chiffres Arduino IDE 2.3.10 / core 3.3.11 sont équivalents à ±1 %.

## 1. Espace disponible sur l'ESP32 (4 Mo de flash, 320 Ko de RAM)

| Partition (`partitions.csv`) | Taille | Usage |
|---|---|---|
| nvs | 20 Ko | Config WiFi IDF |
| otadata | 8 Ko | Sélection app0/app1 |
| **app0** | **1 900 Ko (1 945 600 o)** | Firmware actif |
| **app1** | **1 900 Ko** | Firmware reçu par OTA (double image obligatoire pour l'OTA) |
| coredump | 64 Ko | Vidage après plantage |
| spiffs (LittleFS) | 140 Ko | `parametres.json`, `EnergieMinuit.eng`, CSV mensuels |

- Le firmware doit tenir dans **1 945 600 octets** ; l'OTA impose deux partitions app égales, donc on ne peut pas « emprunter » à app1.
- Marge possible côté partitions : supprimer `coredump` (+64 Ko répartis) ou réduire LittleFS ; peu intéressant et incompatible OTA avec les binaires de l'auteur (la table doit être identique). Non retenu.
- RAM : 320 Ko dont ≈ 91 Ko statiques (tableaux d'historique ≈ 19 Ko, tampon Linky 4 Ko + RX 4 Ko, pages/Strings). Le tas libre courant est affiché dans « Données brutes » ; la RAM n'est pas la contrainte.

## 2. Bilan avant / après

| Étape | Commit | Flash (octets) | Δ | % app | RAM |
|---|---|---|---|---|---|
| 0 Origine V17.29 | `dd37251` | 1 897 559 | — | 97,5 % | 91 760 |
| 1 Corrections de bugs (B1, B2, B3, Vmin/Vmax, CSV) | `66d6c0f` | 1 898 435 | +876 | 97,6 % | 91 760 |
| 2 Pages web gzip | `500dd0d` | 1 733 583 | **−164 852** | 89,1 % | 91 648 |
| 3 Code mort + tables multi-sinus | `c59b506` | 1 732 479 | −1 104 | 89,0 % | 91 440 |
| 4 Linky auxiliaire (S1) + patchs + parseurs Enphase | `3eb0485` | 1 734 015 | +1 536 | 89,1 % | 92 904 |
| 5 Source Zendure 1CT-S (réf. `21f80cb` : 1 733 507 / 92 904) | `de1199b` | 1 735 567 | +2 060 | 89,2 % | 93 240 |
| 6 Zendure : trame reconnue à sa charge | `HEAD` | 1 735 559 | −8 | 89,2 % | 93 240 |
| **Total** | | **1 735 559** | **−162 000 (−8,5 %)** | | +1 480 |

Marge disponible : **210 041 octets** (au lieu de 48 041), soit de quoi ajouter le « Linky auxiliaire » (`04_architecture_mqtt_linky.md`, ≈ 6 Ko), un second UART, des entités HA supplémentaires, etc.

## 3. Détail des optimisations réalisées

### 3.1 Pages web compressées (−165 Ko)
- Principe : les 26 chaînes HTML/JS/SVG (235 148 o) sont compressées en gzip **à la génération** (`python tools/gen_web_gz.py` → `Solar_Router_V17_29/WebGz.h`, 65 Ko de tableaux `PROGMEM`). Le serveur envoie `Content-Encoding: gzip` (`sendGz()` dans `Server.ino`) ; le navigateur décompresse. Aucune charge CPU sur l'ESP32, envoi 3,6 × plus court.
- Sources lisibles inchangées (`Page*.h`, `JS_*.h`) : elles restent le point de vérité pour l'édition et les fusions avec l'amont F1ATB, mais ne sont plus compilées (`#define WEB_GZ` dans le sketch, `PageCommun.h` garde `CommunCSS` et `CouleurDefaut` en clair car le CSS est assemblé dynamiquement).
- `var biSonde` (injecté dynamiquement en tête de `MainJS1`) est servi par un nouveau script `/biSonde` chargé avant `MainJS1` par l'accueil.
- Procédure après toute modification d'une page : `python tools/gen_web_gz.py` puis compiler. `python test/run_tests.py` refuse un `WebGz.h` obsolète (`--check` : régénération identique + décompression bit à bit égale à la source).
- Non-régression : `test_web_gzip` (en-tête gzip, longueur, magic 1F 8B, page clé d'accès vs page Actions selon cookie, `/biSonde`).

### 3.2 Code mort et tables multi-sinus (−1,1 Ko flash, −208 o RAM)
- `Action::TypeEnCours`, `Valmin`, `Valmax` (jamais appelées), déclarations `Definir`, `Lire`, `Activer` sans définition.
- Globales `Cle_ROM`, `DateCeJour`, `idxPromDuJour`, `baseTick10ms`.
- JS : `touchMove`, `mouseMove`, `NewPosition` remplacées depuis longtemps par `startDrag/dragHandle/stopDrag`.
- Tables multi-sinus : les littéraux « optimisation Michy » étaient **écrasés au boot** par un recalcul (79 valeurs sur 101 différaient). Les tables sont maintenant `const` en flash avec les valeurs effectivement utilisées ; le test `test_multisinus_tables` vérifie qu'elles sont exactement le résultat de l'algorithme. Comportement à l'exécution inchangé.

### 3.3 Corrections de bugs incluses (+0,9 Ko)
- B3 : buffer d'état MQTT 1 200 → 2 000 o avec garde anti-débordement, buffer PubSubClient 1 700 → 2 300 o (10 actions + Linky + 4 températures ≈ 1 500 o).

## 4. Chiffrage de la flash par fonctionnalité (firmware optimisé, 1 732 Ko)

Ventilation des symboles du `firmware.elf` (`xtensa-esp32-elf-nm --size-sort`) par expressions régulières sur les noms. Ordres de grandeur : ≈ 370 Ko de constantes anonymes (littéraux de chaînes, formats, tableaux gzip) ne sont pas attribuables par symbole et « Non classé » regroupe le reste de l'IDF.

| Fonctionnalité | Flash | Optionnel ? | Commentaire |
|---|---|---|---|
| Pilote WiFi, supplicant WPA, WPS, événements | ≈ 289 Ko | Non (sauf carte Ethernet) | Bloc IDF incompressible ; WPS ≈ 15-20 Ko retirable par `#ifdef` |
| Système ESP-IDF (FreeRTOS, timers, heap, log, périphériques) | ≈ 128 Ko | Non | |
| TLS mbedTLS (`WiFiClientSecure` : Enphase, RTE) | ≈ 129 Ko | **Oui** | Seules les sources Enphase et Tempo RTE en ont besoin ; une variante « sans HTTPS » gagnerait ≈ 130 Ko |
| lwIP (TCP/UDP, IPv6, DHCP, DNS, mDNS, Ethernet) | ≈ 122 Ko | Partiel | IPv6 ≈ 15 Ko, mDNS ≈ 20 Ko, Ethernet ≈ 15 Ko |
| Bibliothèque C/C++ (printf, String, math) | ≈ 114 Ko | Non | |
| Écran LCD/OLED (LovyanGFX, 2 pilotes de dalle + OLED, fontes, tactile CST820/GT911) | ≈ 81 Ko | **Oui** | Inutile sur ESP32 nu ; variante « sans écran » ≈ −80 Ko |
| Pages web compressées (`WebGz.h`) | 65 Ko | Non | 235 Ko avant gzip |
| Paramètres, LittleFS, ArduinoJson, partitions | ≈ 61 Ko | Non | |
| Serveur web, Update OTA, ArduinoOTA | ≈ 58 Ko | Non | ArduinoOTA (IDE) ≈ 8 Ko retirable si OTA web suffit |
| Sources Shelly Em/Pro, SmartG, HomeWizard | ≈ 22 Ko | Oui | 4 blocs dupliqués dans ShellyPro (≈ 3 Ko factorisables) |
| MQTT (PubSubClient, discovery, état, callback) | ≈ 20 Ko | Non | |
| Régulation, Actions, ISR, setup/loop | ≈ 19 Ko | Non | Cœur du routeur |
| Heure, NTP, fuseaux | ≈ 9 Ko | Non | |
| Sources UxI / UxIx2 / UxIx3 | ≈ 7 Ko | Oui | |
| Source Enphase (hors TLS) | ≈ 6 Ko | Oui | 11 parseurs JSON quasi identiques (≈ 2 Ko factorisables) |
| Source externe / routeurs partenaires | ≈ 6 Ko | Non | |
| Températures (OneWire, Dallas) | ≈ 4 Ko | Oui | |
| Source Zendure 1CT-S (RS485, écoute) | ≈ 2 Ko | Oui | |
| Source Linky | ≈ 4 Ko | Non | Un « Linky auxiliaire » coûte ≈ +6 Ko (décodage partagé) |
| Console série/Telnet, utilitaires | ≈ 4 Ko | Non | |
| Tempo RTE (hors TLS) | ≈ 3 Ko | Oui | |

Lecture : les trois seuls postes « lourds » réellement optionnels sont **TLS (129 Ko)**, **écran (81 Ko)** et une partie du réseau (WPS/IPv6/mDNS ≈ 50 Ko). Tout le reste est le socle Arduino/IDF ou le cœur fonctionnel. Descendre au niveau de chaque source de mesure (4-22 Ko) n'apporte pas grand-chose.

## 5. Pistes non réalisées (et pourquoi)

| Piste | Gain | Raison de ne pas la faire maintenant |
|---|---|---|
| Variantes de compilation (`#define SANS_ECRAN`, `SANS_TLS`, `SANS_WPS`) | 80 à 260 Ko | L'auteur distribue un binaire unique pour l'OTA ; à réserver si une fonction future exige plus de 200 Ko |
| Minification des JS/HTML | ≈ 10 Ko après gzip | Détruit la lisibilité des sources, gain marginal |
| Factoriser les parseurs JSON Enphase et les blocs Shelly Pro | ≈ 5 Ko | Pas de matériel pour valider ; risque > gain |
| `F()`/`PSTR` sur les messages Telnet, `const String&` | 2-4 Ko | Marginal |
| Réduire `coredump` ou LittleFS dans `partitions.csv` | 64-100 Ko | Casse la compatibilité OTA avec les images existantes |
