# CLAUDE.md — Routeur solaire F1ATB V17.29

## Projet
- Firmware Arduino pour ESP32 (avec ou sans écran LCD) : routage du surplus photovoltaïque vers des charges (Triac, SSR, relais, PWM, relais distants HTTP).
- Origine : https://f1atb.fr (RMS = Routeur Multi Sources), licence AGPL-3.0.
- Sources : `Solar_Router_V17_29/` (sketch Arduino : `.ino` concaténés par l'IDE, `Actions.cpp/.h`, pages web en chaînes C dans `Page*.h`/`JS_*.h`).
- Docs : `docs/` (technique, limites, optimisation, architecture MQTT/Linky).
- Tests hôte : `test/` (mocks Arduino + compilation g++ des `.ino` sur PC). Lancer `python test/run_tests.py`.

## Build
- Cible officielle : Arduino IDE 2.3.x, core Espressif ESP32 **3.3.x**, carte « ESP32 Dev Module », partition **custom** (`partitions.csv` : 2 × app 1900 K + LittleFS 140 K).
- Build reproductible ici : `python -m platformio run` (`platformio.ini` à la racine, plateforme pioarduino = core 3.3.x). Mettre `PYTHONIOENCODING=utf-8` sous Windows.
- Bibliothèques : PubSubClient, ArduinoJson 7, DallasTemperature, UrlEncode, LovyanGFX, EthernetESP32. `OneWire` est une copie locale modifiée (ne pas remplacer par celle du gestionnaire de bibliothèques).
- Taille flash : voir `docs/03_optimisation_flash.md` (mesure de référence et après chaque modification).

## Règles de travail
- Toute modification du firmware : 1) `python test/run_tests.py` vert, 2) `python -m platformio run` compile, 3) noter la taille flash/RAM avant/après dans `docs/03_optimisation_flash.md`, 4) commit git avec message explicite.
- Ne jamais changer le format de `parametres.json`, des séparateurs AJAX (GS/RS/US/FS/ES = chr 29/30/31/28/27) ni des topics MQTT sans mettre à jour le JS embarqué, les tests et la doc.
- Les `.ino` n'ont pas de prototypes : l'IDE les génère. Le harnais de test les fournit dans `test/mock/prototypes.h` — l'ajout d'une fonction appelée avant sa définition doit y être ajouté.
- Les variables globales vivent dans `Solar_Router_V17_29.ino` ; pas de `static` dans les `.ino` partagés avec les tests.
- Cœur 0 = `Task_LectureRMS` (acquisition de la source), cœur 1 = `loop()` (serveur web, MQTT, régulation `GestionOverproduction()` toutes les 200 ms). Les ISR (`onTimer` 100 µs, `onTimer10ms`, `currentNull` sur ZC) pilotent Triac/relais via les tableaux `volatile` `Retard[]`, `Actif[]`, `PulseOn[]`…
- Données patient/client : sans objet. Ne jamais mettre de vrais SSID/mots de passe/IP dans les exemples ou tests.
- Langue : français dans les docs et commentaires, identifiants de code inchangés.

## Repères rapides
| Sujet | Fichier |
|---|---|
| Globales, setup/loop, régulation PID, ISR | `Solar_Router_V17_29.ino` |
| Décodage TIC Linky | `Source_Linky.ino` |
| MQTT (discovery HA, état, souscriptions) | `EnvoiMQTT.ino`, `Source_MQTT.ino` |
| Paramètres JSON (LittleFS) | `Stockage.ino` |
| Serveur web + endpoints AJAX | `Server.ino` |
| Classe Action (périodes, conditions) | `Actions.cpp/.h` |
| Maître/esclave entre ESP | `Source_Externe.ino`, `RMS_Externes.ino` |
