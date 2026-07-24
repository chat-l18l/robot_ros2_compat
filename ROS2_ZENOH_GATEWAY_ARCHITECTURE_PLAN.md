# ROS 2 ↔ Zenoh gateway: architectuur- en implementatieplan

> Status: vastgesteld uitgangspunt voor de eerste implementatie  
> Doelplatformen: Linux met ROS 2 Jazzy en ESP32-P4 met ESP-IDF/FreeRTOS  
> Transport: Ethernet en Zenoh/Zenoh-Pico  
> Taal: C++17 op Linux, C11 op de ESP32-P4  
> Bijgewerkt: 2026-07-24

## 1. Doel

Dit document legt vast hoe we de Clearpath-compatibele ROS 2-interface verbinden met de bestaande robotfirmware op de ESP32-P4.

De eerste mijlpaal is bewust klein en risicoloos: vanuit ROS 2 een test-GPIO schakelen, de werkelijk toegepaste toestand vanaf de ESP32-P4 terugsturen en de end-to-end round-triplatency meten. Deze verticale slice bewijst in één keer dat de volgende onderdelen samenwerken:

- ROS 2-package en buildsysteem;
- ROS 2-subscription en publisher;
- expliciete ROS 2 ↔ Zenoh-vertaling;
- Zenoh-router en Ethernetverbinding;
- Zenoh-Pico op FreeRTOS;
- vaste binaire serialisatie in C;
- ESP32-handler en GPIO-driver;
- statusretour, sequence matching en foutafhandeling;
- geautomatiseerde host-, integratie- en hardwaretests.

Na deze test gebruiken we hetzelfde framework voor `/cmd_vel`, geaccepteerde motion commands, e-stopstatus en odometrie. Er wordt in deze fase nog geen netwerkcommando aan motorvermogen gekoppeld.

## 2. Vastgestelde architectuurbesluiten

| ID | Besluit | Reden |
|---|---|---|
| ADR-001 | Er komt een expliciete `ros2_zenoh_gateway` op de Linux-computer van de robot. | Houdt ROS 2-discovery, CDR, dynamische ROS-types en type-informatie buiten de ESP32-firmware. |
| ADR-002 | De ESP32-P4 gebruikt Zenoh-Pico en eigen vaste, versieerbare wire-messages. | Eenvoudig in C, begrensd geheugengebruik en onafhankelijk van ROS 2-interne wire-details. |
| ADR-003 | De gateway vertaalt alleen routes uit een compile-time whitelist. | Geen onverwacht verkeer of onbedoelde commandoroutes naar de firmware. |
| ADR-004 | De eerste gateway is één zelfstandige C++17 ROS 2-node met een `SingleThreadedExecutor`. | Klein, voorspelbaar en voldoende voor lage datasnelheden zoals commando, status en odometrie. |
| ADR-005 | Harde realtime regelkringen en alle safety authority blijven op de ESP32-P4. | Netwerk, Linux en ROS 2 mogen niet nodig zijn om veilig te stoppen of een motorsnelheid vast te houden. |
| ADR-006 | De eerste verticale slice is een GPIO-command/status-roundtrip. | Bewijst het volledige pad zonder bewegingsrisico. |
| ADR-007 | Serialisatie gebeurt veld voor veld met expliciete bytevolgorde; C-structs worden niet rechtstreeks verzonden. | Voorkomt padding-, alignment-, compiler- en architectuurafhankelijkheid. |
| ADR-008 | Optimalisatie volgt pas na latency- en jittermetingen. | Een componentcontainer, multithreaded executor of directe ROS-wire-interoperabiliteit wordt alleen toegevoegd voor een gemeten probleem. |
| ADR-009 | Het interne robotnetwerk volgt Clearpaths `192.168.131.0/24`-indeling; de primaire computer is `.1` en de ESP32-P4 is `.2`. | Houdt computer-, MCU- en payloadintegratie herkenbaar voor Jackal/Husky-gebruikers en voorkomt projectspecifieke adreskeuzes. |

Deze besluiten mogen alleen worden gewijzigd met een nieuw ADR, een reden, meetgegevens en bijgewerkte tests.

## 3. Waarom ROS 2 niet rechtstreeks met de ESP32-Zenoh-client praat

ROS 2 kan op Linux Zenoh als RMW gebruiken via `rmw_zenoh`. Daarmee communiceren ROS 2-nodes via Zenoh in plaats van een DDS-RMW. Dit maakt een gewone Zenoh-Pico-applicatie echter niet automatisch een ROS 2-node.

Voor rechtstreekse interoperabiliteit moet een native Zenoh-applicatie dezelfde afspraken als `rmw_zenoh` implementeren voor onder andere:

- key expressions;
- ROS 2-serialisatie;
- attachments en type-informatie;
- discovery en liveliness tokens;
- ROS graph-semantiek.

De officiële `rmw_zenoh`-documentatie noemt dit technisch mogelijk, maar ondersteuning van zulke native Zenoh-applicaties valt buiten de scope van het project. Ook gebruikt `rmw_zenoh` andere key expressions dan `zenoh-bridge-ros2dds`; deze twee routes zijn niet onderling uitwisselbaar.

Daarom is voor deze robot de eerste keuze:

```text
ROS 2 message
    -> expliciete gatewayvertaling
    -> eigen versieerbare Zenoh-payload
    -> ESP32-P4 handler
```

`rmw_zenoh` mag later wel als RMW van de Linux ROS 2-installatie worden gebruikt. Dat verandert de applicatiegrens niet: de gateway blijft het expliciete contract tussen ROS 2 en embedded firmware.

## 4. Systeemtopologie

```mermaid
flowchart LR
    A["ROS 2-applicaties"] --> B["ros2_zenoh_gateway"]
    B <--> C["zenohd op voertuig-PC"]
    C <--> D["ESP32-P4 + Zenoh-Pico"]
    D <--> E["GPIO en later motion core"]
```

| Onderdeel | Locatie | Verantwoordelijkheid |
|---|---|---|
| ROS 2-applicaties | Linux-computer | Teleop, navigatie, logging, visualisatie en Clearpath API |
| `ros2_zenoh_gateway` | Dezelfde Linux-computer | Whitelisted messagevertaling, validatie en metingen |
| `zenohd` | Dezelfde Linux-computer | Vast lokaal router-endpoint voor embedded clients |
| Zenoh-Pico-client | ESP32-P4 via Ethernet | Ontvangen/publiceren van vaste embedded berichten |
| Hardware- en motion core | ESP32-P4/FreeRTOS | GPIO, watchdog, e-stop, kinematica en motorregeling |

### Netwerktopologie voor versie 1

- De primaire voertuigcomputer gebruikt `192.168.131.1/24`.
- De ESP32-P4 gebruikt `192.168.131.2/24`.
- ESP32-P4 draait in Zenoh `client` mode.
- De firmware verbindt met een expliciet geconfigureerd TCP-endpoint van `zenohd`.
- Multicast discovery is niet nodig voor normale werking.
- De router draait lokaal op de voertuigcomputer om extra netwerkhops te vermijden.
- Adressen, robot-ID en router-endpoint zijn configuratie en worden niet hard gecodeerd in handlers.

De eerste tests gebruiken TCP over bedraad Ethernet. Andere transports worden pas toegevoegd als daar een concrete eis voor bestaat.
De volledige adresindeling, NIC-configuratie, multi-robotisolatie en
netwerkacceptatietest staan in
[`ROBOT_NETWORK_SETUP.md`](ROBOT_NETWORK_SETUP.md).

## 5. Realtime- en latencybeleid

### Wat wel realtime moet zijn

De volgende keten draait periodiek en volledig lokaal op de ESP32-P4:

```text
encoder -> snelheidsregelaar -> safety gate -> motoroutput
```

Ook de command-watchdog, fysieke e-stop en controllerfaults worden lokaal verwerkt. De laatst geaccepteerde netwerkopdracht is alleen een setpoint met een beperkte geldigheidsduur.

### Wat niet hard realtime is

ROS 2, Linux, Zenoh en de gateway vormen een low-latency besturingspad, maar geen hard-realtime safety-loop. Een vertraging, crash of verbroken verbinding moet door de ESP32-watchdog leiden tot een veilige stop.

### Executor-keuze

Versie 1 gebruikt:

- één node;
- één `SingleThreadedExecutor`;
- korte callbacks;
- geen disk-I/O, consolelogging of onbegrensde allocatie in het snelle pad;
- een begrensde overdracht tussen Zenoh-callbackcontext en ROS-executorcontext;
- rate-limited statistieken buiten het snelle pad.

Een ROS 2-component maakt code niet vanzelf realtime. Component composition kan later kopieën tussen meerdere lokale ROS 2-nodes verminderen. Een `MultiThreadedExecutor` kan later nuttig zijn voor onafhankelijke, blokkerende of zware callbacks, maar introduceert ook synchronisatie en extra schedulingvariatie. Beide worden pas gekozen na profiling.

### Eerste meetdoelen

Dit zijn engineeringdoelen voor de lokale bedrade opstelling, geen veiligheidsclaims:

| Meting | Eerste doel |
|---|---|
| Softwareloopback | 1.000/1.000 correcte antwoorden |
| GPIO-wisseltest | 100/100 correcte toegepaste toestanden op 10 Hz |
| Verkeerde of niet-gematchte sequences | 0 |
| p99 round-triplatency GPIO | < 20 ms |
| Maximale round-triplatency GPIO | < 100 ms |
| Uitgang na boot | veilige toestand `off` |

Als een doel niet wordt gehaald, bewaren we de meetdata en lokaliseren we eerst de vertraging. We veranderen niet meteen executor-, component- of transportarchitectuur.

## 6. Gatewayontwerp

De gateway bridge niet automatisch alle topics. Iedere route is een expliciete beschrijving:

```cpp
struct RouteDefinition
{
    const char *ros_topic;
    const char *ros_type;
    const char *zenoh_key;
    RouteDirection direction;
    uint16_t wire_message_type;
};
```

De precieze implementatie mag afwijken, maar de routegegevens moeten centraal en controleerbaar blijven.

### Eerste routes

| Richting | ROS 2-topic | ROS 2-type | Zenoh-key |
|---|---|---|---|
| ROS -> ESP | `/test/gpio/command` | `std_msgs/msg/Bool` | `robot/v1/io/test_output/cmd` |
| ESP -> ROS | `/test/gpio/state` | `std_msgs/msg/Bool` | `robot/v1/io/test_output/state` |

De robot-ID wordt later vóór `v1` in de namespace opgenomen zodra de naamgevingsstrategie is vastgesteld. Voor de eerste robot en lokale test houden we de keys exact zoals hierboven.

### Callbackregels

ROS-commandcallback:

1. lees en valideer de ROS-message;
2. reserveer een oplopende sequence;
3. noteer Linux monotone verzendtijd in een begrensde pending-tabel;
4. encodeer de vaste payload in een lokale buffer;
5. publiceer op de expliciete Zenoh-key;
6. keer direct terug.

Zenoh-statuscallback:

1. controleer totale lengte, magic, versie en message type;
2. decodeer ieder veld met expliciete bytevolgorde;
3. match de sequence met de pending-tabel;
4. bereken round-triptijd op de Linux monotone klok;
5. plaats de status in een begrensde single-producer/single-consumer-queue;
6. laat de ROS-executor de status publiceren;
7. werk counters bij zonder per-message logging.

Overflowbeleid voor queues en pending entries is expliciet: de nieuwste veiligheidsrelevante toestand blijft behouden, een overflow verhoogt een foutcounter en wordt rate-limited gelogd. Stil verlies is niet toegestaan.

## 7. Embedded wire-contract versie 1

### Algemene regels

- Alle multibyte integers zijn little-endian.
- Er worden uitsluitend integers op de wire gebruikt in de GPIO-test.
- Geen C/C++-struct wordt met `memcpy` als compleet wire-bericht verzonden.
- Encoder- en decoderfuncties controleren bufferlengte vóór iedere toegang.
- Gereserveerde bytes worden bij verzenden nul gemaakt en bij ontvangen voorlopig genegeerd.
- `sequence` loopt modulo `2^32`; vergelijking moet wraparound aankunnen.
- Een onbekende hoofdversie of message type wordt verworpen.
- Zenoh encoding/content type voor deze payloads is `application/octet-stream`.
- Versie 1 bevat geen applicatie-CRC; TCP en Zenoh leveren transportintegriteit. Een CRC wordt alleen toegevoegd voor een transport of risico dat dit nodig maakt.

### Gemeenschappelijke header — 16 bytes

| Offset | Grootte | Veld | Type | Waarde/betekenis |
|---:|---:|---|---|---|
| 0 | 4 | `magic` | `uint32_t` | `0x31434252` (`RBC1` als bytes) |
| 4 | 2 | `protocol_version` | `uint16_t` | `1` |
| 6 | 2 | `message_type` | `uint16_t` | Zie tabel hieronder |
| 8 | 2 | `payload_length` | `uint16_t` | Bytes na de header |
| 10 | 2 | `flags` | `uint16_t` | In versie 1 altijd `0` |
| 12 | 4 | `sequence` | `uint32_t` | Door commandobron toegekend; status echoot deze waarde |

Message types:

| Waarde | Naam |
|---:|---|
| 1 | `GPIO_COMMAND` |
| 2 | `GPIO_STATE` |

### `GPIO_COMMAND` — payload 4 bytes, totaal 20 bytes

| Payloadoffset | Grootte | Veld | Type | Geldige waarde |
|---:|---:|---|---|---|
| 0 | 1 | `requested_state` | `uint8_t` | `0` of `1` |
| 1 | 3 | `reserved` | bytes | Alle bytes `0` bij verzenden |

### `GPIO_STATE` — payload 20 bytes, totaal 36 bytes

| Payloadoffset | Grootte | Veld | Type | Betekenis |
|---:|---:|---|---|---|
| 0 | 1 | `requested_state` | `uint8_t` | Ontvangen verzoek |
| 1 | 1 | `observed_state` | `uint8_t` | Werkelijk teruggelezen toestand |
| 2 | 1 | `result` | `uint8_t` | Resultaatcode |
| 3 | 1 | `reserved` | `uint8_t` | `0` |
| 4 | 8 | `esp_receive_time_us` | `uint64_t` | ESP monotone tijd bij geldige ontvangst |
| 12 | 8 | `esp_apply_time_us` | `uint64_t` | ESP monotone tijd na toepassen/teruglezen |

De headersequence van `GPIO_STATE` moet gelijk zijn aan de sequence van het bijbehorende `GPIO_COMMAND`.

Resultaatcodes:

| Waarde | Naam | Betekenis |
|---:|---|---|
| 0 | `GPIO_RESULT_OK` | Toegepast en waargenomen toestand is gelijk aan verzoek |
| 1 | `GPIO_RESULT_INVALID_STATE` | Andere waarde dan `0` of `1` |
| 2 | `GPIO_RESULT_APPLY_FAILED` | Driver kon uitgang niet instellen |
| 3 | `GPIO_RESULT_READBACK_MISMATCH` | Teruglezing wijkt af van verzoek |
| 4 | `GPIO_RESULT_INTERNAL_ERROR` | Overige interne fout |

De ESP-timestamps worden niet gebruikt om one-way netwerklatency te berekenen zolang Linux- en ESP-klokken niet aantoonbaar gesynchroniseerd zijn. Wel kan `esp_apply_time_us - esp_receive_time_us` de lokale handler- en hardwaretijd meten.

## 8. GPIO-gedrag en veiligheid

- Gebruik een vrije test-LED of risicoloze GPIO, nooit een motor-enable, contactor, rem of ander actuatorpad.
- De fysieke pin en actieve polariteit zijn configuratie.
- De toestand bij boot, reset, decodefout en sessieverlies is `off`.
- De handler past alleen een volledig gevalideerd `GPIO_COMMAND` toe.
- Na schrijven wordt de uitgang via een echte input teruggelezen indien de hardware dit ondersteunt.
- Als elektrische teruglezing nog niet beschikbaar is, wordt voorlopig het outputregister gelezen en wordt deze beperking in het testrapport vermeld.
- Een duplicate sequence verandert de hardware niet opnieuw, maar mag de laatst bekende status opnieuw beantwoorden.
- Een oudere/out-of-order sequence wordt verworpen en geteld.
- De GPIO-callback logt niet per bericht.

## 9. Voorgestelde repositorystructuur

Deze repository bevat nu vooral contractspecificaties. De eerste implementatie kan als volgt worden opgebouwd:

```text
robot_ros2_compat/
├── docs/
│   ├── architecture/
│   └── test-results/
├── protocol/
│   ├── include/robot_wire_protocol.h
│   ├── src/robot_wire_protocol.c
│   └── tests/
├── ros2_ws/src/ros2_zenoh_gateway/
│   ├── CMakeLists.txt
│   ├── package.xml
│   ├── include/ros2_zenoh_gateway/
│   ├── src/
│   └── test/
├── esp32/
│   ├── components/robot_wire_protocol/
│   ├── components/zenoh_gpio_adapter/
│   └── test/
├── CMakeLists.txt
├── README.md
└── CHANGELOG.md
```

De protocolimplementatie is C11 en wordt door hosttests en ESP-IDF uit dezelfde bron gebouwd. Maak geen tweede handgeschreven encoder/decoder in C++.

### Builduitgangspunten

- ROS 2-package: `ament_cmake`, C++17 en `-Wall -Wextra -Wpedantic`.
- Wire-protocol: C11, zonder dynamische allocatie.
- ESP32: bestaande ESP-IDF/FreeRTOS-build hergebruiken; eerst versies inventariseren.
- Hosttests: GoogleTest voor C++-gateway en een klein C-testframework voor het protocol, tenzij de bestaande repo al een standaard gebruikt.
- CI bouwt minimaal de hostprotocoltests en de ROS 2-package.
- Een `.deb`-package voor de Linux-gateway is onderdeel van de eerste leverbare release, niet van de eerste GPIO-proof.

## 10. Gefaseerd implementatieplan

### G0 — Bestaande code en versies inventariseren

- [x] Leg ROS 2-distributie en gekozen RMW vast.
- [ ] Leg ESP-IDF-, FreeRTOS-, compiler- en Zenoh-Pico-versies vast.
- [ ] Vind herbruikbare Ethernet-, Zenoh-, GPIO- en loggingcode in de bestaande robot.
- [ ] Leg pin, actieve polariteit en mogelijke hardware-readback vast.
- [ ] Leg IP-adressen/router-endpoint en huidig startproces vast.
- [ ] Meet beschikbare heap en taakstacks vóór wijzigingen.

Exitcriterium: versies, interfaces en hardwareaannames staan in de repo; productiegedrag is niet gewijzigd.

### G1 — Wire-protocol met tests

- [ ] Voeg header-, command- en state-constants toe.
- [ ] Schrijf eerst falende golden-vector-tests.
- [ ] Implementeer begrensde encode/decodefuncties.
- [ ] Test te korte, te lange, corrupte, onbekende en gereserveerde waarden.
- [ ] Test sequence-wraparound en endianness.
- [ ] Draai dezelfde golden vectors op host en ESP32.

Exitcriterium: iedere geldige payload heeft exact de bytes uit de golden vector; iedere ongeldige lengte of versie wordt veilig verworpen.

### G2 — Zenoh softwareloopback zonder ROS 2 of GPIO

- [ ] Start `zenohd` met een vast lokaal configuratiebestand.
- [ ] Maak een hostpublisher voor `GPIO_COMMAND`.
- [ ] Maak een tijdelijke echo/responder met dezelfde wire-library.
- [ ] Match antwoorden op sequence en meet RTT met de Linux monotone klok.
- [ ] Test 1.000 berichten en herstarts van router en responder.

Exitcriterium: 1.000/1.000 correcte antwoorden, nul payload- of sequencefouten en opgeslagen latencyrapport.

### G3 — ESP32 Zenoh-Pico en GPIO

- [ ] Verbind de ESP32 als client met het vaste router-endpoint.
- [ ] Subscribe op `robot/v1/io/test_output/cmd`.
- [ ] Decodeer en valideer buiten de GPIO-driver.
- [ ] Pas GPIO toe en lees de toestand terug.
- [ ] Publiceer `GPIO_STATE` met dezelfde sequence.
- [ ] Forceer `off` bij boot en netwerk/sessieverlies.
- [ ] Voeg begrensde counters toe voor valid, invalid, stale, duplicate en apply error.

Exitcriterium: commandline-hosttest schakelt de veilige GPIO 100 keer op 10 Hz correct, inclusief statusretour.

### G4 — ROS 2-gateway verticale slice

- [x] Maak package `ros2_zenoh_gateway`.
- [ ] Subscribe op `/test/gpio/command` als `std_msgs/msg/Bool`.
- [ ] Publiceer `/test/gpio/state` als `std_msgs/msg/Bool`.
- [ ] Implementeer sequence-toekenning en begrensde pending-tabel.
- [ ] Implementeer statusmatching en RTT-statistieken.
- [ ] Voeg parameters toe voor keys, router-endpoint en test-GPIO-enable.
- [ ] Voeg een launchbestand toe voor router/gatewayconfiguratie.
- [ ] Voeg automatische ROS 2-integratietest toe.

Exitcriterium: een ROS 2-test schakelt de fysieke GPIO end-to-end en ontvangt iedere juiste status terug.

### G5 — Fout- en herstarttests

- [ ] Verbreek Ethernet tijdens een testreeks.
- [ ] Stop en start `zenohd` opnieuw.
- [ ] Stop en start de gateway opnieuw.
- [ ] Reset de ESP32 tijdens `on` en controleer veilige boottoestand.
- [ ] Injecteer verkeerde magic, versie, lengte, type, state en sequence.
- [ ] Vul queues/pending-tabel gecontroleerd tot de limiet.
- [ ] Controleer rate-limited logs en alle counters.

Exitcriterium: geen crash, geen ongecontroleerde heapgroei, geen blijvend `on` na reset/verbindingsverlies en voorspelbaar herstel.

### G6 — Motion-command zonder motoroutput

- [ ] Definieer `MOTION_COMMAND` in hetzelfde wire-protocol.
- [ ] Map `/cmd_vel` (`TwistStamped`) naar de Zenoh motion key.
- [ ] Ontvang, valideer en log op ESP32 met motor-enable hard uit.
- [ ] Publiceer accepted-command met sequence en toegepaste limieten terug.
- [ ] Implementeer command-watchdog en bewijs overgang naar `TIMEOUT`.

Exitcriterium: ROS 2-motion commands bereiken correct de ESP32-state machine, terwijl alle motoroutputs aantoonbaar nul blijven.

### G7 — Odometrie terug naar ROS 2

- [ ] Definieer encoder/odometry wire-messages.
- [ ] Publiceer ESP32-odometrie via Zenoh.
- [ ] Map naar `/platform/odom` als `nav_msgs/msg/Odometry`.
- [ ] Controleer frames, eenheden, tekens, timestamps en covariance.
- [ ] Meet sample age, frequentie, drops, jitter en gatewaylatency.

Exitcriterium: rosbag en ROS 2-test bevestigen correcte odometrie bij handmatig gedraaide of vrijhangende wielen.

Pas daarna koppelen we motion commands via de safety gate aan de bestaande motorlaag.

## 11. Minimale testmatrix

### Protocol-unittests

- [ ] Geldige `GPIO_COMMAND` encode/decode roundtrip.
- [ ] Geldige `GPIO_STATE` encode/decode roundtrip.
- [ ] Golden bytes zijn identiek op x86-64 en ESP32.
- [ ] Buffer één byte te kort wordt verworpen.
- [ ] Extra onverwachte bytes worden volgens contract verworpen.
- [ ] Verkeerde magic, versie, message type en payloadlengte worden verworpen.
- [ ] `requested_state > 1` wordt verworpen.
- [ ] Maximumsequence en wrap naar nul werken.
- [ ] Decoder schrijft bij fout geen gedeeltelijk geldige output.

### Gateway-unittests

- [ ] ROS `false` wordt wire state `0`.
- [ ] ROS `true` wordt wire state `1`.
- [ ] Iedere opdracht krijgt precies één nieuwe sequence.
- [ ] Status met onbekende sequence wordt geteld en niet als geldige ack gebruikt.
- [ ] Duplicate status maakt geen tweede ROS-event.
- [ ] Pending-table timeout verwijdert oude entries.
- [ ] Queue-overflow volgt het gedocumenteerde beleid.
- [ ] Statistieken berekenen min, mediaan, p95, p99 en maximum correct.

### End-to-endtests

- [ ] Softwareloopback: 1.000 berichten.
- [ ] Fysieke GPIO: `off -> on -> off`.
- [ ] Fysieke GPIO: 100 wisselingen op 10 Hz.
- [ ] Routerverlies en automatisch herstel.
- [ ] Ethernetverlies en automatisch herstel.
- [ ] Gatewayherstart.
- [ ] ESP32-reset terwijl de uitgang `on` is.
- [ ] Beschadigde payload veroorzaakt geen GPIO-wijziging.
- [ ] Testresultaten worden als machineleesbaar bestand en korte Markdown-samenvatting bewaard.

## 12. Logging en observability

Log direct:

- sessie verbonden/verbroken;
- onbekende versie of message type;
- malformed payload;
- sequencefout of timeout;
- queue/pending overflow;
- GPIO apply/readback error;
- start, stop en samenvatting van een testreeks.

Log periodiek, maximaal eenmaal per seconde:

- verzonden/ontvangen/ongeldig/timeout/duplicate counters;
- actuele queue-high-water marks;
- aantal pending commands;
- RTT min/mediaan/p95/p99/max;
- ESP handlerduur min/p95/max;
- vrije heap en minimum vrije taakstack indien beschikbaar.

Nooit ieder bericht in de normale 10-100 Hz-datastroom printen. Logging mag de gemeten latency niet domineren.

## 13. Agentwerkpakketten en wijzigingsregels

Werkpakketten zijn klein genoeg voor afzonderlijke PR's:

| WP | Scope | Afhankelijkheid |
|---|---|---|
| WP-G1 | Wire-header, GPIO-codecs en golden tests | G0 |
| WP-G2 | Zenoh hostloopback en latencytool | WP-G1 |
| WP-G3 | ESP32 Zenoh/GPIO-adapter | WP-G1, bestaande firmware |
| WP-G4 | ROS 2-gatewaynode en ROS-tests | WP-G1, WP-G2 |
| WP-G5 | Foutinjectie en herstarttests | WP-G3, WP-G4 |
| WP-G6 | Motion-commandcontract zonder motoroutput | G5 geslaagd |
| WP-G7 | Odometriecontract en ROS-publicatie | Encodertoegang beschikbaar |

Iedere agent:

1. leest dit document en `MOTION_CORE_IMPLEMENTATION_PLAN.md` volledig;
2. inspecteert de bestaande code voordat interfaces worden verzonnen;
3. claimt één work package in issue/PR;
4. voegt eerst een falende test of reproduceerbaar testrapport toe;
5. wijzigt geen safety- of wire-semantiek impliciet;
6. documenteert versies, aannames en meetomgeving;
7. levert testbewijs en werkt checkboxes alleen bij na aantoonbare verificatie;
8. mengt geen refactor of telemetry-uitbreiding in dezelfde PR.

## 14. Open beslissingen vóór motion-implementatie

De GPIO-proof blokkeert niet op deze punten. Ze moeten wel vóór G6/G7 worden vastgesteld:

- [ ] Definitieve robot-ID en Zenoh-keynamespace voor meerdere robots.
- [ ] Exacte Zenoh-Pico/routerversies en versiepinning.
- [ ] QoS, congestion control en prioriteit per motion/odometry key.
- [ ] Commandofrequentie en definitieve watchdogtimeout.
- [ ] Maximum lineaire/angulaire snelheid en acceleratie.
- [ ] Clamp- versus rejectbeleid.
- [ ] Linux/ESP-kloksynchronisatie en timestampsemantiek.
- [ ] Queuegroottes op basis van gemeten rates en beschikbare heap.
- [ ] Bezit van `odom -> base_link` TF.
- [ ] Security/authentication voor een netwerk dat buiten de afgeschermde voertuig-LAN komt.
- [ ] Packaging en servicebeheer van `zenohd` en gateway op de voertuigcomputer.

## 15. Definition of done: eerste end-to-end bewijs

De eerste verticale slice is pas gereed wanneer:

- [ ] de host- en ROS 2-build vanaf een schone checkout reproduceerbaar zijn;
- [ ] de ESP32-firmware met de vastgelegde toolchain reproduceerbaar bouwt;
- [ ] de protocol-golden tests op host en ESP32 slagen;
- [ ] ROS 2 `/test/gpio/command` de fysieke veilige GPIO schakelt;
- [ ] `/test/gpio/state` de toegepaste toestand terugmeldt;
- [ ] iedere status aan de juiste sequence wordt gekoppeld;
- [ ] softwareloopback en GPIO-test de aantallen uit sectie 5 halen;
- [ ] router-, Ethernet-, gateway- en ESP32-herstart veilig zijn getest;
- [ ] latencydata en foutcounters in de repo zijn opgeslagen;
- [ ] de ESP32-uitgang bij boot en verbindingsverlies `off` is;
- [ ] geen motoroutput of andere gevaarlijke actuator door deze slice kan worden geactiveerd.

## 16. Referenties

- [ROS 2 `rmw_zenoh`](https://github.com/ros2/rmw_zenoh) — installatie, routergebruik, ontwerp en beperkingen van native Zenoh-interoperabiliteit.
- [ROS 2 executors (Jazzy)](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Executors.html) — callbackuitvoering en single-/multithreaded executors.
- [ROS 2 callback groups (Jazzy)](https://docs.ros.org/en/jazzy/How-To-Guides/Using-callback-groups.html) — controle over callbackconcurrency wanneer later meerdere threads nodig zijn.
- [ROS 2 composition (Jazzy)](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Composition.html) — componenten en intra-process composition.
- [ROS 2 QoS (Jazzy)](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html) — reliability, history, depth, deadline en liveliness.
- [ROS 2 realtime design proposal](https://design.ros2.org/articles/realtime_proposal.html) — determinisme, allocatie en scheduling.
- [Eclipse Zenoh deployment](https://zenoh.io/docs/getting-started/deployment/) — client-, peer- en routertopologie.
- [Eclipse Zenoh-Pico](https://github.com/eclipse-zenoh/zenoh-pico) — pure-C embedded implementatie en ESP-IDF/FreeRTOS-ondersteuning.
- [Zenoh ROS 2 DDS bridge](https://github.com/eclipse-zenoh/zenoh-plugin-ros2dds) — alternatieve DDS-bridge; niet verwarren met `rmw_zenoh` of onze applicatiegateway.
- [`PROJECT_VISION_AND_ARCHITECTURE.md`](PROJECT_VISION_AND_ARCHITECTURE.md) — projectdoel, platformgrenzen en de ROS 2- en lichtgewicht gebruiksprofielen.
- [`ROBOT_NETWORK_SETUP.md`](ROBOT_NETWORK_SETUP.md) — normatief intern subnet, vaste adressen en netwerkacceptatie.
- [`MOTION_CORE_IMPLEMENTATION_PLAN.md`](MOTION_CORE_IMPLEMENTATION_PLAN.md) — veilige motion core, watchdog, skid-steer en odometrie.
- [`CLEARPATH_ROS2_COMPATIBILITY_TODO.md`](CLEARPATH_ROS2_COMPATIBILITY_TODO.md) — volledige Jackal/Husky-interfacebacklog.
