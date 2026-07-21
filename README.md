# Robot ROS 2 Compatibility

> **Status: experimenteel onderzoeks- en ontwikkelproject.**  
> De software is nog niet productierijp, veiligheidsgecertificeerd of vrijgegeven voor commercieel gebruik.

## Wat is dit project?

Dit project onderzoekt hoe we een mobiele robot met moderne software- en embeddedtechnieken kunnen laten samenwerken met het ROS 2-ecosysteem en de interfaces van Clearpath Jackal en Husky.

De beoogde architectuur combineert onder andere:

- ROS 2 Jazzy op Ubuntu 24.04;
- een C++ gateway tussen ROS 2 en Zenoh;
- Zenoh voor de communicatie tussen Linux en de embedded motion core;
- een ESP32-P4 met PlatformIO, ESP-IDF, FreeRTOS en Zenoh-Pico;
- expliciete, versieerbare wire-protocollen;
- lokale motorregeling, watchdogs en veiligheidslogica op de ESP32-P4;
- C, C++ en Python voor implementatie, tests en ontwikkelgereedschappen.

Het einddoel is een rijdende robot die via een bruikbare ROS 2-interface veilig commando's ontvangt en actuele status, encoderdata en odometrie teruglevert. Compatibiliteit, latency, betrouwbaarheid en veilig gedrag bij fouten moeten met tests en metingen worden aangetoond; ze worden niet alleen verondersteld.

## Waarom bestaat dit project?

Dit is ook een experiment in de manier waarop technische projecten in 2026 kunnen worden ontwikkeld. We onderzoeken hoe snel een klein team een kwalitatief goed robotsysteem kan bouwen door het combineren van:

- eigen kennis en praktijkervaring met robots, elektronica en programmeren;
- moderne ontwikkeltools;
- large language models (LLM's);
- gespecialiseerde software-agents;
- kleine, meetbare mijlpalen;
- test-driven en incrementele ontwikkeling;
- expliciete architectuur- en ontwerpbesluiten.

Het project moet niet alleen code opleveren, maar ook inzicht geven in welke aanpak goed werkt, welke aannames onjuist blijken en hoeveel tijd moderne tooling werkelijk bespaart. Of de aanpak slaagt, blijkt uiteindelijk uit de praktijk: de robot moet aantoonbaar betrouwbaar en veilig rijden.

De ambitie is om dit vóór het einde van 2026 te bereiken. Dit is een onderzoeksdoel en geen toezegging of garantie.

## Globaal plan van aanpak

We bouwen het systeem in kleine verticale stappen. Iedere stap moet afzonderlijk te bouwen, testen en meten zijn voordat een volgende stap echte motoroutput kan beïnvloeden.

1. **Ontwikkelomgeving reproduceren**  
   Ubuntu 24.04, ROS 2 Jazzy, de C/C++-toolchain, PlatformIO en Zenoh gecontroleerd installeren en met smoketests verifiëren.

2. **Interfaces en wire-protocol vastleggen**  
   Voor ieder bericht key, topic, versie, datatypes, eenheden, bereiken, timeout en foutgedrag expliciet definiëren.

3. **Veilige end-to-end GPIO-test bouwen**  
   Eerst een risicoloze command/status-roundtrip realiseren: ROS 2 → gateway → Zenoh → ESP32-P4 → GPIO → Zenoh → gateway → ROS 2. Hiermee testen we het buildsysteem, de serialisatie, het netwerk en de round-triplatency.

4. **Motion commands ontvangen zonder te rijden**  
   `/cmd_vel` ontvangen, vertalen, valideren, loggen en bevestigen terwijl motoruitgangen gegarandeerd uitgeschakeld blijven.

5. **Safety gate en watchdog implementeren**  
   Alleen geldige, actuele commando's toestaan. Bij timeout, e-stop, fout, reboot of verbroken communicatie zelfstandig naar een veilige toestand gaan.

6. **Skid-steer en motorregeling toevoegen**  
   Voertuigsnelheid vertalen naar linker- en rechtersnelheid. De harde regelkring blijft lokaal op de ESP32-P4 en is niet afhankelijk van ROS 2 of een netwerkverbinding.

7. **Encoders en odometrie implementeren**  
   Encoderdata robuust verwerken, pose en snelheid bepalen en via de gateway als ROS 2-odometrie publiceren.

8. **Compatibiliteit, prestaties en veiligheid aantonen**  
   Functionele tests, foutinjectie, latency- en jittermetingen en fysieke rijproeven uitvoeren. Gemiddelde waarden alleen zijn onvoldoende; ook verlies, p95, p99 en worst-case gedrag worden beoordeeld.

9. **Gefaseerd uitbreiden**  
   Pas na een betrouwbare motion core volgen aanvullende telemetrie, verlichting, diagnose, IMU, GPS, LiDAR en verdere Jackal/Husky-compatibiliteit.

## Belangrijke architectuurkeuze

De eerste implementatie gebruikt een expliciete ROS 2 ↔ Zenoh-gateway op de Linux-computer. De ESP32-P4 gebruikt Zenoh-Pico en eenvoudige embedded berichten; hij hoeft geen volledige ROS 2-node te zijn.

```text
ROS 2 applicaties
    -> C++ ROS 2–Zenoh gateway
    -> Zenoh router
    -> Ethernet
    -> ESP32-P4 motion core
    -> motorcontrollers en encoders
```

De gateway verwerkt uitsluitend vooraf vastgelegde, whitelisted routes. De realtime motorregelkring, watchdog, e-stopafhandeling en veilige stop blijven lokaal op de ESP32-P4.

## Documentatie

De belangrijkste plannen zijn:

- [`DEVELOPMENT_ENVIRONMENT_SETUP.md`](DEVELOPMENT_ENVIRONMENT_SETUP.md) — installatie en rationale voor de ontwikkel-pc;
- [`ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md`](ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md) — gatewayarchitectuur, protocol en eerste end-to-endtest;
- [`MOTION_CORE_IMPLEMENTATION_PLAN.md`](MOTION_CORE_IMPLEMENTATION_PLAN.md) — gefaseerde implementatie van motion, safety en odometrie;
- [`CLEARPATH_ROS2_COMPATIBILITY_TODO.md`](CLEARPATH_ROS2_COMPATIBILITY_TODO.md) — compatibiliteitsbacklog voor Jackal/Husky ROS 2-interfaces.

Deze documenten leggen de huidige technische uitgangspunten vast. Omdat dit een experiment is, mogen beslissingen veranderen wanneer tests, metingen of hardwaregedrag daar aanleiding toe geven. Wijzigingen horen gemotiveerd en versieerbaar te zijn.

## Geen product en geen veiligheidsverklaring

Deze repository bevat experimentele software, ontwerpen, documentatie en onderzoeksresultaten. Niets in deze repository is een verklaring dat het systeem:

- geschikt is voor een bepaald doel;
- veilig is voor personen, dieren, eigendommen of verkeer;
- voldoet aan wetgeving, normen of certificeringseisen;
- compatibel is met iedere Jackal-, Husky-, ROS 2-, Zenoh- of ESP32-configuratie;
- foutloos, volledig of actueel is.

Gebruik de software niet voor een operationele robot, veiligheidskritische toepassing of commerciële installatie zonder een eigen risicoanalyse, passende beveiligingen, deskundige verificatie en alle wettelijk vereiste keuringen. Een netwerkcommandopad mag nooit de fysieke noodstop of lokale veiligheidslogica kunnen omzeilen.

Aan de inhoud, plannen, voorbeelden, resultaten, interfaces, planning of beschikbaarheid van dit project kunnen geen rechten, garanties of toezeggingen worden ontleend.

## Duale licentie

Copyright © 2026 Edwin van den Oetelaar. Alle rechten voorbehouden.

Dit project wordt aangeboden onder **één van twee alternatieve licenties**. U moet vóór ieder gebruik bepalen welke licentie op uw situatie van toepassing is.

### Optie A — kosteloze onderwijs- en studielicentie

U mag de publiek beschikbare inhoud kosteloos bekijken, bestuderen en gebruiken voor persoonlijk leren, niet-commercieel onderwijs en niet-commercieel academisch onderzoek, mits:

- het auteursrecht en deze licentievoorwaarden behouden blijven;
- het gebruik geen commercieel doel heeft en niet plaatsvindt ten behoeve van betaalde producten, diensten, consultancy, interne bedrijfsvoering of productontwikkeling;
- de software niet wordt ingezet op een operationele of productie-robot;
- u de inhoud niet verkoopt, sublicentieert of als eigen werk aanbiedt;
- afgeleide kopieën uitsluitend onder dezelfde beperkingen worden gedeeld.

Deze optie verleent bedrijven en andere commerciële organisaties geen gebruiksrecht, ook niet voor interne evaluatie, proof-of-concepts, training, onderzoek of productontwikkeling, tenzij vooraf schriftelijk anders is overeengekomen.

### Optie B — betaalde commerciële licentie

Voor ieder commercieel, bedrijfsmatig, professioneel of productioneel gebruik is vooraf een afzonderlijke schriftelijke en betaalde licentie van de auteursrechthebbende vereist. De voorwaarden, reikwijdte, duur, ondersteuning, aansprakelijkheid en vergoeding worden in die overeenkomst vastgelegd.

Neem contact op met de auteursrechthebbende om een commerciële licentie af te spreken. Zonder zo'n ondertekende overeenkomst worden geen commerciële rechten verleend. Het bekijken, downloaden, forken of toegankelijk zijn van de repository vormt geen commerciële licentie.

Bij twijfel geldt dat gebruik niet is toegestaan totdat schriftelijke toestemming is verkregen.

## Uitsluiting van garantie en aansprakelijkheid

Voor zover wettelijk toegestaan wordt alle inhoud geleverd **“zoals deze is”** en **“zoals beschikbaar”**, zonder enige uitdrukkelijke of stilzwijgende garantie, waaronder garanties van juistheid, volledigheid, verkoopbaarheid, geschiktheid voor een bepaald doel, niet-inbreuk, veiligheid of beschikbaarheid.

De auteursrechthebbende, auteurs en bijdragers zijn, voor zover wettelijk toegestaan, niet aansprakelijk voor enige directe of indirecte schade, gevolgschade, letsel, overlijden, verlies van gegevens, winst, inkomsten, gebruik of eigendom, bedrijfsstagnatie of andere schade die voortvloeit uit of verband houdt met de inhoud, het gebruik, het niet kunnen gebruiken of het gedrag van hardware of software uit dit project, ongeacht de juridische grondslag en ook wanneer op de mogelijkheid van dergelijke schade is gewezen.

U gebruikt dit project volledig op eigen risico. U bent zelf verantwoordelijk voor beoordeling, testen, beveiliging, wettelijke naleving en veilige toepassing.

## Bijdragen

Een pull request of andere bijdrage wordt niet automatisch geaccepteerd. Voordat bijdragen van derden worden opgenomen, moeten afspraken over auteursrecht en licentiëring duidelijk zijn, zodat het duale licentiemodel behouden kan blijven.

## Juridische opmerking

Deze README beschrijft de beoogde gebruiksvoorwaarden, maar is geen juridisch advies. Voor commerciële publicatie en afdwingbaarheid wordt een afzonderlijk `LICENSE`-bestand en, waar nodig, beoordeling door een juridisch deskundige aanbevolen.
