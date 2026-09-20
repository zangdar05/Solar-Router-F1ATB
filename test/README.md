# Tests de non-régression — Routeur solaire F1ATB V17.29

Tests exécutables **sur PC** (Windows, g++ MinGW 15, C++17), sans PlatformIO,
sans framework de test externe, sans accès réseau.
Aucun fichier de `Solar_Router_V17_29/` n'est modifié.

## Lancer les tests

```
python test/run_tests.py
```

- Compile puis exécute ; le code de sortie est celui des tests (0 = tout passe).
- Le binaire est produit dans `test/build/tests.exe`.
- `C:\msys64\mingw64\bin` est ajouté au `PATH` par le script : sans cela,
  `g++` échoue **silencieusement** (cc1plus ne trouve pas ses DLL).

## Architecture

- **`test/mock/`** — en-têtes factices reproduisant l'API Arduino/ESP32 utilisée
  par le firmware :
  - `Arduino.h` : classe `String` compatible Arduino (sur `std::string`),
    horloge simulée (`mock_set_millis`, `mock_advance_millis`, `delay()` fait
    avancer `millis()`), GPIO journalisés (`mock_gpio_state[pin]`,
    `mock_gpio_writes[pin]`), `Print`/`Stream`, objet `ESP`, timers, FreeRTOS.
  - `HardwareSerial.h` : file d'entrée injectable via `MySerial.mock_feed(...)`,
    tout ce qui est écrit est conservé dans `.tx`.
  - `WiFi.h` / `WiFiClient.h` : valeurs neutres ; `connect()` échoue par défaut
    (`mock_client_connect_ok`), réponse servie via `mock_client_response`,
    requête émise relue dans `mock_client_request`.
  - `WebServer.h` : `send()` mémorise le corps dans `mock_last_http_body`.
  - `PubSubClient.h` : chaque `publish()` est journalisé dans
    `mock_mqtt_published` (topic/payload), `subscribe()` dans
    `mock_mqtt_subscribed`, `connected()` piloté par `mock_mqtt_connected`.
  - `FS.h` / `LittleFS.h` : système de fichiers **en mémoire** (`mock_fs`,
    `mock_fs_reset()`), avec `FILE_WRITE` / `FILE_APPEND` / `"r"` et
    `openNextFile()`.
  - `esp_stubs.h` : bouchons des symboles fournis par `EcranLCD.ino` /
    `EcranLED.ino` (`Ecran_Init`, `Ecran_Loop`, `PrintScroll`, `GoPage`,
    `Gestion_LEDs`, `lcd`, `NumPage`…), et neutralisation du `OneWire.h` livré
    dans le dossier du sketch (accès direct aux registres GPIO).
  - `prototypes.h` : prototypes de toutes les fonctions des `.ino`, normalement
    générés par l'IDE Arduino. **Généré mécaniquement** depuis les sources.
  - `firmware_globals.h` : déclarations `extern` des variables globales du
    firmware, à l'usage de `test_main.cpp`. **Généré mécaniquement** depuis la
    zone « VARIABLES » du `.ino` principal (éviter de le retaper à la main :
    un type faux ne provoquerait pas d'erreur de lien).
  - `mock_impl.cpp` : les définitions (objets globaux, horloge, GPIO, FS).
    Inclus une seule fois, par `firmware_host.cpp`.
- **`test/third_party/ArduinoJson.h`** — bibliothèque header-only v7.4.2.
  Absente ? la télécharger depuis
  `https://github.com/bblanchon/ArduinoJson/releases/download/v7.4.2/ArduinoJson-v7.4.2.h`.
- **`test/firmware_host.cpp`** — assemble le firmware en une unité de
  compilation : mocks, bouchons, prototypes, puis le `.ino` principal et tous
  les autres onglets **sauf** `EcranLCD.ino` et `EcranLED.ino`.
- **`test/test_main.cpp`** — les tests et le mini-framework
  (`CHECK`, `CHECK_EQ`, `CHECK_STR`, `CHECK_NEAR`, affichage `[ OK ]`/`[FAIL]`).

## Points d'attention

- `Actions.cpp` est compilé comme **unité séparée** : `Actions.h` n'a pas de
  garde d'inclusion, l'inclure une seconde fois redéfinirait `class Action`.
- On ne définit **pas** la macro `ARDUINO` : ArduinoJson exigerait alors
  `PROGMEM`, `Printable` et `Stream`. Seul
  `ARDUINOJSON_ENABLE_ARDUINO_STRING=1` est activé.
- `CST820.h` et `initGT911.h` réels (dossier du sketch) sont utilisés tels
  quels ; seul `Wire.h` est bouchonné.
- `setup()` n'est **jamais** appelé par les tests (il enchaîne WiFi, écran,
  tâches). Chaque test remet lui-même à zéro l'état global dont il a besoin
  via `reset_commun()`.

## Ajouter un test

1. Écrire une fonction `static void test_xxx()` dans `test_main.cpp`.
2. Commencer par `reset_commun();` puis positionner les globales nécessaires.
3. Appeler la fonction du firmware, vérifier avec `CHECK*`.
4. Enregistrer le test dans `main()` avec `RUN(test_xxx);`.
5. Si une globale manque, l'ajouter à `mock/firmware_globals.h` **en copiant
   la déclaration exacte** de la source.

Un test doit décrire le comportement **actuel** du firmware. Si ce comportement
paraît fautif, le figer quand même et le commenter
`// BUG PROBABLE:` ou `// NOTE: comportement actuel`.

## Limites connues

- Pas de test de l'écran LCD/OLED (`EcranLCD.ino`, `EcranLED.ino` exclus :
  dépendance LovyanGFX).
- Pas de test du WiFi / Ethernet / mDNS / OTA réels : uniquement des stubs
  neutres. Les sources réseau (Enphase, Shelly, SmartGateway, HomeWizard, RTE)
  compilent mais ne sont pas couvertes.
- Pas de test des ISR en temps réel (`GestionIT_10ms`, `onTimer`,
  `currentNull`) : elles sont compilées mais jamais déclenchées, le temps est
  purement simulé.
- Pas de test du multi-cœur (`Task_LectureRMS` n'est pas lancée).
- `OneWire` / `DallasTemperature` sont mockés : la couche 1-Wire réelle n'est
  pas vérifiée (`mock_ds18b20_temp[]` permet d'injecter des mesures).
