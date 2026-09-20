# Optimisation de l'espace flash — journal des modifications

Mesures faites avec `python -m platformio run` (core ESP32 3.3.12, partition custom : app = 1 945 600 octets).

## 1. Point de départ (V17.29 d'origine, commit `dd37251`)

| Poste | Valeur |
|---|---|
| Flash utilisée | **1 897 559 / 1 945 600 octets (97,5 %)** — 48 Ko de marge |
| RAM statique | 91 760 / 327 680 octets (28 %) |
| `.flash.text` (code) | 1 270 708 |
| `.flash.rodata` (constantes) | 501 116 |
| Objet du sketch (`Solar_Router_V17_29.ino.cpp.o`, text+data) | 537 244 |
| dont pages web HTML/JS/CSS en chaînes brutes | ≈ 235 500 |
| Reste (framework Arduino/IDF : WiFi, lwIP, mbedTLS, mDNS, LovyanGFX, WPS, Ethernet…) | ≈ 1 360 000 |

Plus grosses fonctions du sketch : `LectureShellyProEm` 9,9 K, `sendMQTTDiscoveryMsg_global` 9,2 K, `LectureShellyEm` 8,7 K, `DeserializeConfiguration` 6,7 K, `Init_Server` 6,4 K, `setup` 6,0 K, `SerializeConfiguration` 5,3 K, `loop` 5,3 K, `LectureLinky` 4,9 K, `handleAjaxRMS` 4,2 K.

## 2. Gisements identifiés

| # | Piste | Gain estimé | Risque | Retenu |
|---|---|---|---|---|
| G1 | **Compression gzip des pages web** à la génération (`tools/gen_web_gz.py` → `WebGz.h`), envoi avec `Content-Encoding: gzip` | **≈ −170 Ko** (235 K → 65 K) | Faible : tous les navigateurs décompressent ; vérification automatique de réversibilité (`--check`) | Oui |
| G2 | Minification des sources JS/HTML (commentaires, espaces) | −58 Ko seuls, ≈ −10 Ko en plus de G1 | Moyen (lisibilité, fusion avec l'amont) | Non (gzip suffit) |
| G3 | Code mort C++ : `TypeEnCours`, `Valmin`, `Valmax`, recalcul des tables multi-sinus au `setup()`, globales inutilisées | −1 à −2 Ko | Faible | Oui |
| G4 | Code mort JS (`touchMove/mouseMove/NewPosition`, doublons) | −1 Ko après gzip | Faible | Oui |
| G5 | Factoriser les 11 parseurs JSON d'`Source_EnphaseEnvoy.ino` et les 4 blocs Shelly Pro dupliqués | −3 à −5 Ko | Moyen (pas de matériel pour valider) | Non, documenté |
| G6 | Compiler sans écran (`LovyanGFX`, fontes, tactiles) via `#define` de variante | −80 à −100 Ko | Élevé (binaire unique OTA distribué par l'auteur) | Non, documenté |
| G7 | Retirer WPS / Ethernet / IPv6 selon carte | −20 à −60 Ko | Idem G6 | Non |
| G8 | `SendDataToHomeAssistant`, discovery : passer les `String` par référence const, `F()`/`PSTR` sur messages Telnet | −2 à −4 Ko | Faible | Partiel |

## 3. Modifications réalisées

Chaque étape : tests hôte (`python test/run_tests.py`) verts, build PlatformIO, mesure, commit.

| Étape | Commit | Flash | Δ | RAM | Contenu |
|---|---|---|---|---|---|
| 0 | `dd37251` | 1 897 559 | — | 91 760 | Origine |
