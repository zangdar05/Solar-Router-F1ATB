# Découpler la lecture Linky du routage — solutions

Objectif : remonter les données Linky (index, tarif, tension, courant, statut Tempo) vers Home Assistant en MQTT, **tout en** régulant le routage à partir d'une autre mesure (JSY-MK-194 précis, puissance reçue en MQTT, Shelly…), ou inversement piloter les Actions par MQTT tout en gardant la lecture Linky.

## 1. Pourquoi c'est impossible aujourd'hui

| Verrou | Où | Effet |
|---|---|---|
| Une seule variable `Source` sélectionne **la** source de mesure ; chaque `Source_*.ino` écrit directement les globales `PuissanceS/I_M_inst`, `Energie_M_*`, `Tension_M`… | `Task_LectureRMS()` | Deux sources actives se marcheraient dessus |
| Un seul UART applicatif `MySerial(2)` partagé Linky / JSY, initialisé par `Setup_Linky()` ou `Setup_UxIx2()` selon `Source` | `Solar_Router_V17_29.ino`, `InitGPIOs()` | Linky et JSY exclusifs |
| La publication MQTT des champs Linky (NGTF, STGE, EASF01-10, LTARF) est conditionnée à `Source == "Linky"` | `EnvoiMQTT.ino` (discovery + état) | Rien n'est publié si le Linky n'est pas la source de régulation |
| La puissance MQTT entrante n'est traitée que si `Source == "Pmqtt"` (`callback()` + `UpdatePmqtt()`) | `EnvoiMQTT.ino`, `Source_MQTT.ino` | Pas de « source de régulation MQTT » en parallèle du Linky |
| Les ordres Actions MQTT (`subMQTT`) fonctionnent avec toute source, **mais** uniquement en mode expert (`ModePara == 1`) ; en mode standard `SerializeConfiguration()` force `MQTTRepet = 0` et `subMQTT = 0` | `Stockage.ino` | Le pilotage MQTT « disparaît » dès qu'on repasse en standard |
| Page Paramètres : `JS_Para.h:420` masque `#ligneTopicP` dès que `Source != "Pmqtt"` ; le bloc broker `#Zmqtt` n'apparaît qu'en mode expert avec `MQTTRepet > 0`, `subMQTT` coché ou une température `tempMqtt` ; l'option `Pmqtt` du sélecteur est désactivée en mode standard | UI | L'utilisateur ne peut pas saisir un topic de puissance avec Linky ; le port série (`#port_serie`) reste, lui, toujours visible |

Point important : le pilotage MQTT des Actions avec un Linky **fonctionne déjà** en mode expert (`subMQTT = 1`, topics `<device>/<TitreAction>`). Ce qui manque réellement, c'est (a) une **seconde source de mesure** et (b) une **lecture Linky indépendante** de la régulation.

## 2. Solutions

### S0 — Deux ESP32 (aucune modification de code)
- ESP « Linky » : `Source = Linky`, 0 action, mode expert, `MQTTRepet = 10` → publie tout le Linky vers HA.
- ESP « Routeur » : `Source = Ext` (IP de l'ESP Linky) ou `Source = UxIx2` (JSY précis) ou `Source = Pmqtt`; actions locales ; `subMQTT = 1` pour les ordres HA.
- Avantages : disponible immédiatement, isolation des pannes, chaque ESP garde toute sa flash.
- Inconvénients : deuxième carte + alimentation ; en `Ext`, latence ≈ 1 s (période 800 ms + HTTP) ; deux entités HA.

### S1 — « Linky auxiliaire » sur un second UART (recommandée mono-ESP)
Principe : la TIC est lue en permanence sur un **UART dédié (UART1)**, décodée dans une structure `TIC` indépendante, et publiée en MQTT quelle que soit `Source`. La régulation continue d'utiliser `Source` (JSY sur UART2, Pmqtt, Shelly…). Si `Source == "Linky"`, la même structure alimente la régulation (comportement identique à aujourd'hui).

Modifications :
| Étape | Fichier | Contenu |
|---|---|---|
| 1. Paramètre | `Stockage.ino`, `PagePara.h`/`JS_Para.h` | `LinkyAux` (0 = non, 1 = oui) + `pSerialAux` (index dans `RX2_[]`, TX inutile : la TIC est RX seul) |
| 2. UART | `Solar_Router_V17_29.ino` | `HardwareSerial SerialAux(1);` `Setup_LinkyAux()` = `begin(9600, SERIAL_7E1, RX_aux, -1)`, buffer 4096 |
| 3. Décodage | `Source_Linky.ino` | Factoriser `LectureLinky()` en `DecodeTIC(Stream&, TicData&)` qui remplit `struct TicData { long EAST, EAIT, EASF[10]; int SINSTS, SINSTI, SINSTS1..3, URMS1..3, IRMS1..3; String LTARF, NGTF, STGE, DATE; unsigned long lastFrameMs; }`. `LectureLinky()` devient : `DecodeTIC(MySerial, tic)` puis la partie « puissances/énergies pour régulation » existante lit `tic` au lieu des `val` |
| 4. Tâche | `Task_LectureRMS()` | En plus de la source : `if (LinkyAux) DecodeTIC(SerialAux, ticAux);` (≈ 2 ms de CPU, déjà le cas en Linky) |
| 5. MQTT | `EnvoiMQTT.ino` | Remplacer `Source == "Linky"` par `(Source == "Linky" \|\| LinkyAux)` ; publier depuis `ticAux` : EAST/EAIT (sous les noms `Linky_EAST`, `Linky_EAIT` pour ne pas entrer en conflit avec `Energie_M_*` de la source de régulation), SINSTS/SINSTI, URMS/IRMS 1-3, LTARF, NGTF, STGE, EASF01-10, `Linky_PuissanceS/I` (calcul cosφ existant) ; `LTARFbin` (tarif pour les conditions d'Actions) et `STGEt` (Tempo) alimentés par `ticAux` si `TempoRTEon == 0` |
| 6. Watchdog | `loop()` | Ne pas mettre `PuissanceRecue = true` sur les trames aux (le watchdog doit rester lié à la source de régulation) ; ajouter un message si aucune trame aux depuis 60 s |
| 7. Données brutes | `Server.ino` | Onglet « Linky aux » dans `/ajax_dataRMS` (réutiliser `DataRawLinky`) |
| 8. Tests | `test/` | Test `DecodeTIC` sur trame synthétique ; test que `Source = UxIx2 + LinkyAux` laisse `Energie_M_*` au JSY et publie `Linky_*` |

Coût : ≈ +6 Ko de flash (code) et +8 Ko de RAM (buffer RX 4096 + tampon brut 4000 si on garde le second `DataRawLinky`) ; ~250 lignes.
Câblage : RX aux sur un GPIO libre (ex. 26 ou 5 selon carte) via le même montage optocoupleur que l'entrée Linky actuelle ; JSY sur l'UART2 habituel.

### S2 — Source principale = MQTT, Linky en auxiliaire (routage piloté par HA)
- Cas où HA calcule la consigne (ex. surplus après batterie / VE) et l'envoie sur `TopicP` (`{"Pw":-800}`) : `Source = Pmqtt` + S1 pour remonter le Linky.
- Aucune autre modification : `UpdatePmqtt()` régule déjà ; les ordres d'Actions restent disponibles via `subMQTT`.
- Sécurité : conserver le timeout `LastPwMQTTMillis` (30 s) sinon reset watchdog ; documenter dans HA une automatisation qui republie la puissance toutes les 10 s.

### S2 bis — Linky principal + puissance MQTT en seconde sonde (variante légère)
- Cas inverse : le Linky reste la source de régulation, une puissance MQTT (ex. production onduleur publiée par HA) alimente les variables `_T` (seconde sonde) pour l'affichage et l'énergie produite.
- Modifications : `JS_Para.h:420` afficher `#ligneTopicP` dès que `TopicP` est renseigné ; `EnvoiMQTT.ino:65` et `:132` remplacer `Source == "Pmqtt"` par `TopicP != ""` ; nouvelle fonction `UpdatePmqttT()` copiant `PwMQTT/PvaMQTT` vers `PuissanceS/I_T_inst` + intégration Wh dans `Energie_T_*` ; `biSonde` (Server.ino, Record_Data) étendu à ce cas.
- ≈ 60 lignes, +1 Ko. Ne couvre pas le besoin « régulation sur JSY + lecture Linky ».

### S3 — Multi-source générique (refonte)
- Tableau de sources `Source[0..1]` avec un rôle chacune (`REGULATION`, `AFFICHAGE_MQTT`) et une structure `Mesure` par source (P, VA, U, I, E) ; `filtre_puissance()` prend la source de rôle régulation.
- Plus propre mais touche tous les `Source_*.ino`, la page Paramètres, les trames maître/esclave (`/ajax_data`) et le MQTT : ≈ 1 000 lignes modifiées. À réserver si l'on veut aussi Shelly + JSY, Enphase + Linky, etc.

### S4 — Lecture Linky déportée sans changer le routeur
- Module TIC dédié (ESP8266/ESP32 « Teleinfo2MQTT », Lixee ZLinky, addon HA « TIC ») publie le Linky ; le routeur F1ATB reste sur JSY/Pmqtt.
- Zéro modification firmware ; perd l'horloge Linky et le tarif dans les conditions d'Actions (sauf `TempoRTEon = 1`).

## 3. Comparatif

| | S0 deux ESP | S1 Linky aux | S2 Pmqtt + aux | S3 refonte | S4 module tiers |
|---|---|---|---|---|---|
| Code à écrire | 0 | ≈ 250 lignes | S1 | ≈ 1 000 lignes | 0 |
| Flash | 0 | +6 Ko | +6 Ko | +15 Ko | 0 |
| Matériel | 2ᵉ ESP | 1 GPIO + optocoupleur | idem | idem | module TIC |
| Latence régulation | 1 s (Ext) | inchangée | dépend HA | inchangée | inchangée |
| Linky vers HA | oui | oui | oui | oui | oui |
| Tarif/Tempo pour Actions | via Ext | oui | oui | oui | non |
| Pilotage Actions MQTT | oui (expert) | oui | oui | oui | oui |

Recommandation : **S1** (avec S2 comme cas d'usage), après avoir libéré la flash (voir `03_optimisation_flash.md`) ; S0 en attendant.

## 4. Autres fonctions intéressantes dans la même veine
- **Autoriser `MQTTRepet`/`subMQTT` en mode standard** : une ligne dans `SerializeConfiguration()`/`EcritureEnROM()` ; la plupart des utilisateurs HA n'ont pas besoin du mode expert.
- **Topic de commande unique** `<device>/cmd` (JSON `{"action":1,"tOnOff":30}`) en plus des topics par titre : évite B12 (titres longs/accents) et facilite les scripts HA.
- **Entités HA de type `number`/`select`/`switch`** pour Force On/Off et Mode (le code prépare déjà les constantes `NB`, `SLCT`, `SWTC` non utilisées) : pilotage natif depuis l'UI HA sans automatisation.
- **Publier `Puissance_M` signée** (`PuissanceS_M − PuissanceI_M`) et la consigne/ouverture pour tracer la régulation dans HA.
- **Seconde sonde par MQTT** : accepter `{"Pw_T":...}` sur `TopicP` pour alimenter `*_T` (affichage production) quand la source principale n'a qu'une voie.
- **Mode « capteur seul »** (0 action, régulation désactivée, `LissageLong`) déjà toléré : le documenter comme configuration « ESP Linky » de S0.
- **Sauvegarde énergies Linky aux** dans `EnergieMinuit.eng` pour fournir `EnergieJour_Linky_*` à HA sans template.
