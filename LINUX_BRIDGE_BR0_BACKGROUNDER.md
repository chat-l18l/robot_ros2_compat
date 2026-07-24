# Backgrounder: wanneer heeft een robotcomputer `br0` nodig?

> Doelgroep: studenten en projectleden met basiskennis van IP-adressen
> Status: achtergrondinformatie; de keuze voor `br0` in dit project staat nog open
> Bijgewerkt: 2026-07-24

## 1. Kort antwoord

Een Linux-bridge zoals `br0` is **niet nodig** om apparaten met vaste
IP-adressen in hetzelfde subnet met elkaar te laten communiceren.

Als de hoofdcomputer met één Ethernetinterface op een fysieke switch is
aangesloten en alle robotapparaten ook op die switch zitten, is dit voldoende:

```text
Hoofdcomputer
enp1s0 = 192.168.131.1/24
        |
  fysieke Ethernet-switch
   |        |         |
ESP32     camera     lidar
.2        .10        .20
```

De fysieke switch vormt het gezamenlijke Ethernetnetwerk. De hoofdcomputer,
ESP32-P4, camera en lidar kunnen elkaar rechtstreeks bereiken. ROS 2, Zenoh en
het Clearpath-compatibele IP-adresplan vereisen daarvoor geen `br0`.

Een bridge wordt pas nuttig wanneer meerdere fysieke of virtuele
Ethernetinterfaces van de hoofdcomputer samen één netwerk moeten vormen.

## 2. Eerst het verschil tussen Layer 2 en Layer 3

Ethernet en IP lossen twee verschillende delen van netwerkcommunicatie op:

| Laag | Identiteit | Belangrijkste functie |
|---|---|---|
| Layer 2: Ethernet | MAC-adres | Frames binnen één lokaal netwerk afleveren |
| Layer 3: IP | IP-adres en subnet | Bepalen of een bestemming lokaal is of via een router moet worden bereikt |

Een Ethernet-switch werkt hoofdzakelijk op Layer 2. Hij leert achter welke poort
een MAC-adres zit en stuurt Ethernetframes naar de juiste poort.

Een router werkt op Layer 3. Hij verbindt verschillende IP-subnetten en neemt
beslissingen op basis van IP-routes.

Een Linux-bridge werkt als een softwarematige Layer-2-switch. Hij is dus niet
hetzelfde als een router.

## 3. Wat is `br0`?

`br0` is een veelgebruikte naam voor een virtuele Linux-netwerkinterface van het
type `bridge`. De naam is een conventie; Linux vereist niet dat een bridge
precies zo heet.

De bridge kan meerdere interfaces als poorten bevatten:

```text
ESP32 ─── enp1s0 \
camera ── enp2s0  >── br0 = 192.168.131.1/24
lidar ─── enp3s0 /
```

In deze opzet:

- gedragen `enp1s0`, `enp2s0` en `enp3s0` zich als switchpoorten;
- staat het IP-adres `192.168.131.1/24` op `br0`;
- hebben de onderliggende bridgepoorten normaal geen eigen IP-adres;
- worden Ethernetframes tussen de poorten door de Linux-kernel doorgestuurd.

De officiële Linux-kerneldocumentatie beschrijft een bridge als een component
die twee of meer netwerksegmenten op Layer 2 verbindt en frames op basis van
MAC-adressen filtert en doorstuurt.

## 4. Waarom niet hetzelfde subnet op meerdere losse interfaces?

Zonder bridge lijkt het misschien logisch om dit te configureren:

```text
enp1s0 -> apparaat A in 192.168.131.0/24
enp2s0 -> apparaat B in 192.168.131.0/24
enp3s0 -> apparaat C in 192.168.131.0/24
```

Dat levert een onduidelijke hostconfiguratie op. Voor een bestemming zoals
`192.168.131.20` ziet Linux meerdere interfaces die hetzelfde subnet claimen.
Routekeuze, ARP-verkeer en het gekozen bronadres kunnen daardoor verrassend
gedrag vertonen.

Gebruik in zo'n situatie één van deze ontwerpen:

1. verbind de apparaten via één fysieke switch met één hostinterface;
2. bridge de hostinterfaces tot één Layer-2-netwerk;
3. geef iedere interface een eigen subnet en routeer bewust tussen de subnetten.

Meerdere losse interfaces met hetzelfde subnet is geen goed vervangend ontwerp
voor een bridge.

## 5. Wat kan een bridge wat één gewone interface niet kan?

Een bridge kan:

- meerdere fysieke Ethernetpoorten van een computer als één switch laten werken;
- virtuele interfaces van containers of virtuele machines aan hetzelfde
  Layer-2-netwerk koppelen;
- broadcast- en multicastverkeer tussen zijn poorten doorgeven;
- één host-IP op de bridge aanbieden, onafhankelijk van de gebruikte
  onderliggende poort;
- optioneel VLAN-filtering, multicast-snooping en spanning-treefuncties
  toepassen.

Een bridge is daarom nuttig wanneer de computer zelf een actieve rol als
Ethernet-switch heeft.

## 6. Wat doet een bridge niet?

Alleen een bridge toevoegen:

- maakt geen DHCP-server;
- maakt geen router of default gateway;
- verzorgt geen NAT of internettoegang;
- beveiligt het netwerk niet automatisch;
- reserveert geen IP-adressen;
- maakt een Zenoh-router niet overbodig;
- voegt geen ROS 2- of Clearpath-functionaliteit toe;
- verbetert de latency niet.

Deze functies vereisen afzonderlijke configuratie of software.

## 7. Waarom gebruikt Clearpath standaard een bridge?

Clearpath-robotcomputers kunnen meerdere fysieke Ethernetpoorten hebben. Door
die poorten in `br0` te plaatsen, kunnen camera's, lidars, MCU's en andere
payloads rechtstreeks op verschillende computerpoorten worden aangesloten en
toch in hetzelfde `192.168.131.0/24`-netwerk blijven.

Clearpaths referentieconfiguratie:

- verzamelt de Ethernetinterfaces in `br0`;
- kent `192.168.131.1/24` toe aan `br0`;
- kan de bridge daarnaast via DHCP een extra wired uplinkadres laten ontvangen.

Dit is handig voor hun algemene productconfiguraties, maar het betekent niet dat
iedere compatibele robot dezelfde interne Linux-bridge nodig heeft.

Compatibiliteit met het Clearpath-adresplan betekent voor ons vooral:

- hetzelfde interne subnet gebruiken;
- dezelfde functionele IP-adresblokken gebruiken;
- de primaire robotcomputer op `.1` bereikbaar maken;
- apparaten dezelfde netwerkverwachtingen geven.

Een extern apparaat kan normaal niet zien of `.1` op `br0` of rechtstreeks op
een fysieke interface staat.

## 8. Latency en systeembelasting

Een bridge moet voor ieder frame onder andere bepalen:

- op welke poort een MAC-adres is geleerd;
- of het frame lokaal, naar één poort of naar meerdere poorten moet;
- hoe broadcast en multicast worden afgehandeld;
- of VLAN-, spanning-tree- of firewallregels van toepassing zijn.

Deze verwerking is op een moderne Linuxcomputer meestal klein, maar niet nul.
Er zijn twee verschillende datapaden:

### Verkeer naar de hoofdcomputer

Een frame komt via een fysieke bridgepoort binnen, passeert de bridgeverwerking
en wordt daarna aan de lokale IP-stack geleverd. Vergeleken met een IP-adres
rechtstreeks op de fysieke interface is er een extra softwarelaag.

### Verkeer tussen twee bridgepoorten

Als bijvoorbeeld een camera op `enp1s0` rechtstreeks met een payloadcomputer op
`enp2s0` communiceert, moet de hoofdcomputer de frames tussen beide poorten
doorsturen. Zonder hardware-offload gebruikt dit CPU-tijd en kan belasting extra
jitter veroorzaken.

Een gewone fysieke switch voert dezelfde functie doorgaans in gespecialiseerde
hardware uit. Daarom is een fysieke switch meestal de eenvoudigste keuze voor
veel camera-, lidar- of ander hoog-bandbreedteverkeer.

De bridge-overhead is niet automatisch een praktisch probleem, maar een
onnodige bridge levert ook geen prestatievoordeel op.

## 9. Mogelijke configuratieproblemen

Een bridge voegt configuratiestatus toe die onderzocht en getest moet worden:

- welke interfaces lid zijn van de bridge;
- welk MAC-adres de bridge gebruikt;
- waar het IP-adres en de default route staan;
- of DHCP op de bridge actief is;
- hoe multicast-snooping is ingesteld;
- hoe firewallregels bridged verkeer behandelen;
- of spanning tree nodig is;
- wat er gebeurt wanneer een poort of kabel wegvalt.

Voorbeelden van fouten zijn:

- per ongeluk een uplink of een tweede robot aan dezelfde bridge toevoegen;
- zowel aan `br0` als aan een onderliggende poort een IP-adres geven;
- een netwerkloop maken en een broadcast storm veroorzaken;
- via DHCP een ongewenste default route ontvangen;
- firewallregels schrijven die routed en bridged verkeer door elkaar halen;
- door veranderde interfacenamen de verkeerde NIC in de bridge plaatsen.

Dit betekent niet dat bridges onbetrouwbaar zijn. Het betekent dat we ze alleen
moeten gebruiken wanneer hun functie werkelijk nodig is.

## 10. Keuzehulp voor dit project

| Fysieke opzet of eis | Voorkeursontwerp |
|---|---|
| Eén computerinterface naar een fysieke switch; alle apparaten op die switch | Geen `br0`; zet `.1/24` rechtstreeks op de interface |
| Meerdere computerpoorten moeten samen als één interne switch werken | Gebruik `br0` en zet `.1/24` op de bridge |
| Container of VM moet als zelfstandig apparaat op het robot-LAN verschijnen | `br0` of een andere bewuste virtuele netwerkkoppeling |
| Meerdere interfaces moeten juist van elkaar geïsoleerd blijven | Niet bridgen; gebruik VLANs of afzonderlijke subnetten |
| Alleen Clearpath-compatibele IP-adressen zijn vereist | `br0` is niet vereist |

## 11. Huidig projectbesluit

Voor dit project staat het volgende vast:

- het interne netwerk is `192.168.131.0/24`;
- de primaire robotcomputer gebruikt `192.168.131.1`;
- de ESP32-P4 gebruikt `192.168.131.2`;
- overige apparaten volgen de Clearpath-compatibele functieblokken.

Nog niet besloten is:

- hoeveel interne Ethernetinterfaces de hoofdcomputer krijgt;
- of alle apparaten via een fysieke switch worden aangesloten;
- of de computerpoorten zelf als switchpoorten moeten werken;
- of containers of VM's rechtstreeks op het robot-LAN moeten komen;
- en dus of `br0` werkelijk nodig is.

Onze voorlopige voorkeur is eenvoud: als één hostinterface en een fysieke switch
de vereisten afdekken, configureren we `.1/24` rechtstreeks op die interface en
activeren we geen `br0`.

We kiezen `br0` pas wanneer de definitieve fysieke topologie een concrete
bridgefunctie vereist. Deze keuze verandert het IP-adresplan of de
ROS 2-Zenoh-architectuur niet.

## 12. Handige inspectiecommando's

Deze commando's veranderen de netwerkconfiguratie niet:

```bash
ip -brief address
ip route
ip link show type bridge
bridge link
bridge fdb show
```

Ze tonen respectievelijk de interfaces en adressen, de IP-routes, aanwezige
bridges, bridgepoorten en de geleerde MAC-adressen.

## 13. Begrippen

| Begrip | Betekenis |
|---|---|
| Bridge | Component die Ethernetsegmenten op Layer 2 samenvoegt |
| Bridgepoort | Fysieke of virtuele interface die lid is van een bridge |
| Broadcast domain | Groep interfaces die elkaars Layer-2-broadcasts ontvangen |
| FDB | Forwarding database met geleerde MAC-adressen en bijbehorende poorten |
| NIC | Network Interface Controller; een fysieke netwerkinterface |
| Router | Component die verkeer tussen IP-subnetten doorstuurt |
| Switch | Component die Ethernetframes tussen fysieke poorten doorstuurt |
| VLAN | Logische scheiding van Layer-2-netwerken op gedeelde infrastructuur |

## 14. Verder lezen

- [Linux-kerneldocumentatie over Ethernet bridging](https://docs.kernel.org/networking/bridge.html)
- [Clearpath Linux Network Configuration](https://docs.clearpathrobotics.com/docs/ros/networking/computer_setup/)
- [`ROBOT_NETWORK_SETUP.md`](ROBOT_NETWORK_SETUP.md) — het normatieve subnet en IP-adresplan van dit project
