# Ontwikkel- en runtimeomgeving voor ROS 2, Pixi en ESP32-P4

> Status: vastgesteld voor ontwikkeling; voorwaardelijk voor de robot-runtime
> ROS 2: Jazzy Jalisco via RoboStack
> Ontwikkel-pc: Ubuntu 24.04, x86-64 (linux-64)
> Beoogde robotcomputer: NVIDIA Jetson AGX Orin, ARM64 (linux-aarch64)
> Embedded target: ESP32-P4, ESP-IDF/FreeRTOS en Zenoh-Pico
> Bijgewerkt: 2026-07-22

## 1. Kort antwoord

Voor ontwikkeling gebruiken we **Pixi als ROS 2-underlay** en
**ros2_ws/install als project-overlay**. Een ROS-installatie in
/opt/ros/jazzy is daarbij niet nodig en mag niet in dezelfde shell worden
geactiveerd.

Voor de NVIDIA Jetson AGX Orin heeft dezelfde Pixi-omgeving onze voorkeur, mits
zij de hardware-acceptatietest uit dit document doorstaat. Pixi ondersteunt
linux-aarch64 en de huidige RoboStack Jazzy-dependencies zijn voor die
architectuur succesvol in pixi.lock opgelost. Dat maakt een Pixi-runtime
realistisch, maar bewijst nog niet dat JetPack, CUDA/TensorRT, device access,
CycloneDDS en systemd op de echte robot correct samenwerken.

De keuze is daarom:

1. ontwikkeling op de werk-pc: Pixi;
2. eerste runtimeproef op AGX Orin: eveneens Pixi;
3. Pixi op de robot behouden als alle acceptatietests slagen;
4. pas bij een aantoonbare incompatibiliteit een native installatie of container
   als fallback ontwerpen.

## 2. Het mentale model: host, underlay en overlay

De termen underlay en overlay blijven relevant. Alleen de locatie van de
underlay is veranderd.

| Laag | Locatie/bron | Bevat | Beheer |
|---|---|---|---|
| Host | Ubuntu/Jetson Linux | kernel, netwerk, gebruikers, devices en NVIDIA-drivers | apt/JetPack |
| ROS 2-underlay | .pixi/envs/default | ROS 2 Jazzy, CycloneDDS, berichten, colcon, compilers en Python | Pixi en pixi.lock |
| Project-overlay | ros2_ws/install | onze gateway en andere eigen ROS-packages | colcon via Pixi |
| Embedded firmware | firmware/esp32p4 | ESP32-P4 motion core en Zenoh-Pico | PlatformIO/ESP-IDF |

In schema:

~~~text
Ubuntu of Jetson Linux
└── Pixi-omgeving                         ROS 2-underlay
    └── ros2_ws/install                   project-overlay
        └── ros2_zenoh_gateway            eigen package

ESP32-P4-firmware                         afzonderlijke toolchain/runtime
~~~

De host blijft verantwoordelijk voor zaken die een geïsoleerde packageomgeving
niet kan leveren: de Linux-kernel, Ethernetinterfaces, USB/CAN/GPIO-devices,
NVIDIA-kerneldrivers en de bij JetPack horende CUDA/TensorRT-installatie.

### Wat is niet meer van toepassing?

Voor de normale ontwikkelworkflow doen we niet meer:

~~~bash
sudo apt install ros-jazzy-desktop
source /opt/ros/jazzy/setup.bash
~~~

Die commando's horen bij een native ROS-installatie en niet bij onze Pixi-setup.
Het mengen van /opt/ros/jazzy met RoboStack in één shell kan verkeerde libraries,
Python-modules en CMake-packages selecteren.

## 3. Waarom Pixi voor dit project

De volledige omgeving staat in:

- pixi.toml: gewenste platforms, dependencies, middleware en taken;
- pixi.lock: exact opgeloste packages voor x86-64 en ARM64;
- .pixi/: lokale, gegenereerde omgeving; deze staat niet in Git.

Dit geeft de werk-pc en mogelijk ook de robotcomputer dezelfde ROS-versies en
buildtools. Een update is een zichtbare wijziging aan het manifest en de lockfile,
niet een stil verschil tussen twee handmatig ingerichte machines.

We gebruiken voorlopig gewone colcon-packages. Pixi levert de underlay en voert
colcon uit; pixi-build-ros is niet nodig voor de eerste implementatie.

## 4. Installatie en dagelijks gebruik

Controleer het platform en voorkom een gemengde Conda-omgeving:

~~~bash
uname -m
conda deactivate 2>/dev/null || true
printenv ROS_DISTRO
~~~

uname -m hoort op de werk-pc x86_64 te tonen en op de AGX Orin aarch64. Een
vooraf ingestelde ROS_DISTRO hoort leeg te zijn voordat Pixi wordt gestart.

Installeer Pixi volgens de officiële instructie en voer vanuit de repositoryroot
uit:

~~~bash
pixi install --locked
pixi run ros-info
pixi run list-packages
pixi run build
pixi run test-result
~~~

De optie --locked voorkomt dat een machine ongemerkt andere dependencyversies
kiest dan in pixi.lock staan.

### Eigen gebouwde ROS-packages uitvoeren

pixi run talker en pixi run listener komen rechtstreeks uit de Pixi-underlay.
Een eigen package uit ros2_ws/src is pas vindbaar nadat de project-overlay is
gebouwd en geactiveerd:

~~~bash
pixi run build
pixi shell
source ros2_ws/install/setup.bash
ros2 run <package> <executable>
~~~

De volgorde is belangrijk:

1. pixi shell activeert de ROS 2-underlay;
2. source ros2_ws/install/setup.bash legt onze project-overlay erbovenop.

Source niet daarnaast ook /opt/ros/jazzy/setup.bash.

### Bewezen smoketest

Start vanuit de repositoryroot twee terminals.

Terminal 1:

~~~bash
pixi run talker
~~~

Terminal 2:

~~~bash
pixi run listener
~~~

De listener moet oplopende Hello World-berichten ontvangen. Deze test is op
22 juli 2026 op de Ubuntu 24.04 x86-64-werk-pc geslaagd.

### Post-linkscripts

Pixi kan melden dat het librsvg-post-linkscript is overgeslagen. Dat blokkeert
de ROS CLI, CycloneDDS of colcon niet. Alleen bij een concreet probleem met
SVG-weergave in bijvoorbeeld RViz of rqt mag dit lokaal worden ingeschakeld:

~~~bash
pixi config set --local run-post-link-scripts insecure
pixi reinstall
~~~

Insecure betekent hier dat package-scripts code mogen uitvoeren. Schakel dit
niet globaal of preventief in.

## 5. AGX Orin: kan Pixi daar de runtime beheren?

**Ja, technisch is dat aannemelijk en daarom onze voorkeursroute.** De AGX Orin
gebruikt een 64-bit ARM-userspace. Pixi kent dit als linux-aarch64. Het
projectmanifest bevat:

~~~toml
platforms = ["linux-64", "linux-aarch64"]
~~~

De locksolver heeft op 22 juli 2026 alle huidige dependencies, waaronder ROS 2
Jazzy, CycloneDDS, colcon en de C/C++-toolchain, voor linux-aarch64 gevonden.

### Waarom dit nog geen definitieve runtimegoedkeuring is

Een opgeloste lockfile test geen echte Jetson. In het bijzonder:

- JetPack 6 gebruikt een Ubuntu 22.04-gebaseerd rootbestandssysteem;
- ROS 2 Jazzy is door ROS zelf primair ondersteund op Ubuntu 24.04 voor amd64 en
  arm64;
- RoboStack levert ROS als geïsoleerde conda-packages en kan daardoor op andere
  geschikte Linux-hosts werken, maar dat is niet hetzelfde als officiële Jazzy
  Debian-pakketten op Ubuntu 24.04;
- NVIDIA-kerneldrivers, CUDA, TensorRT, cuDNN en device libraries blijven aan de
  geïnstalleerde JetPack-versie gekoppeld;
- packages die rechtstreeks NVIDIA- of cameraspecifieke libraries gebruiken
  kunnen aanvullende koppeling met de hostsysteemlibraries nodig hebben.

Installeer daarom niet zomaar Ubuntu 24.04 ROS-debs over een JetPack 6-systeem.
Dat combineert twee distributieaannames en kan de ondersteunde NVIDIA-stack
verstoren.

### Verantwoordelijkheden op de robot

| Onderdeel | Installatiemethode |
|---|---|
| Jetson Linux, kernel en NVIDIA-drivers | NVIDIA JetPack/BSP |
| CUDA, TensorRT, cuDNN en VPI | bij de JetPack-release passende NVIDIA-pakketten |
| ROS 2 Jazzy en gewone userspace-dependencies | Pixi/RoboStack |
| Onze gateway en configuratie | deze repository en de colcon-overlay |
| Procesbeheer | expliciete systemd-unit, pas na runtimeacceptatie |

Pixi vervangt dus niet JetPack. Het beheert het ROS-deel boven op de
NVIDIA-hostinstallatie.

## 6. Verplichte AGX Orin-acceptatietest

Leg eerst de basis vast:

~~~bash
cat /etc/nv_tegra_release
uname -m
lsb_release -ds
pixi --version
pixi install --locked
pixi run ros-info
~~~

Daarna moeten op de echte AGX Orin aantoonbaar slagen:

- installatie vanaf een schone checkout met de gecommitte pixi.lock;
- talker/listener over loopback en over de bedoelde Ethernetinterface;
- CycloneDDS-discovery met de uiteindelijke netwerkconfiguratie;
- build en start van de ROS 2-Zenoh-gateway;
- toegang tot benodigde CAN-, serial-, USB- en netwerkdevices;
- toegang tot CUDA/TensorRT en camera- of sensordrivers als die nodig zijn;
- start, stop, restart en logging onder systemd;
- herstart na power cycle zonder interactieve shell;
- latency-, jitter-, pakketverlies- en langdurige stabiliteitstest;
- correcte veilige stop bij gateway-, router- en netwerkuitval.

De testresultaten, JetPack/L4T-versie en hardwareconfiguratie worden in de repo
opgeslagen. Alleen een succesvolle package-installatie is geen runtimebewijs.

## 7. Fallback wanneer Pixi op Orin niet voldoet

De fallback wordt pas gekozen nadat het concrete probleem is gemeten. Mogelijke
routes zijn:

1. JetPack op de host behouden en alleen de incompatibele hardwaredependency
   buiten Pixi gebruiken;
2. de gateway in een gecontroleerde container draaien met expliciete device-,
   netwerk- en NVIDIA-runtimekoppeling;
3. een native ROS-installatie gebruiken op een OS/JetPack-combinatie die de
   gekozen ROS-distributie daadwerkelijk ondersteunt;
4. de gateway naar een afzonderlijke Ubuntu 24.04-computer verplaatsen en de
   Orin alleen voor NVIDIA-specifieke workloads gebruiken.

Een native apt-runtime is dus niet automatisch eenvoudiger of beter. Op JetPack 6
is vooral de combinatie Ubuntu 22.04 en ROS 2 Jazzy/Ubuntu 24.04 een punt dat
expliciet opgelost moet worden.

## 8. Zenoh en ESP32-P4

De Linuxgateway gebruikt aan de ROS-zijde normale rclcpp-interfaces en aan de
embedded zijde een expliciete Zenoh C/C++-client met eigen wire-messages.
rmw_zenoh is niet de primaire route.

De exacte versies van Zenoh, Zenoh C/C++ en Zenoh-Pico worden pas toegevoegd
nadat de bestaande ESP32-P4-firmware is geïnventariseerd. Daarna moeten Linux en
ESP32 dezelfde vastgelegde, onderling geteste versies gebruiken.

De harde motorregelkring, watchdog, e-stopafhandeling en veilige stop blijven op
de ESP32-P4. Geen keuze voor Pixi, ROS 2 of Jetson Linux verandert die
safetygrens.

## 9. Versies en bewijs vastleggen

Voor iedere ontwikkel- of robotcomputer bewaren we minimaal:

~~~bash
uname -m
lsb_release -ds
pixi --version
pixi list
pixi run ros-info
cmake --version
git rev-parse HEAD
~~~

Voor de AGX Orin komen daar JetPack/L4T, kernel, CUDA, TensorRT, cuDNN,
netwerkconfiguratie, de hash van pixi.lock en de acceptatieresultaten bij.

## 10. Definitie van gereed

De ontwikkelomgeving is gereed wanneer:

- pixi install --locked slaagt;
- pixi run ros-info Jazzy en CycloneDDS toont;
- talker/listener werkt;
- een eigen minimaal package bouwt en na overlayactivatie start;
- tests via de Pixi-taken slagen;
- geen /opt/ros-omgeving met Pixi wordt gemengd.

De AGX Orin-runtime is pas gereed wanneer daarnaast alle tests uit sectie 6
slagen. Tot die tijd is Pixi op ARM64 een onderbouwde voorkeursroute, geen
productieclaim.

## 11. Bronnen

- Pixi-installatie en architectuurdetectie: <https://pixi.sh/latest/installation/>
- Pixi-platformconfiguratie: <https://pixi.sh/latest/workspace/multi_platform_configuration/>
- RoboStack Jazzy-kanaal: <https://prefix.dev/channels/robostack-jazzy>
- ROS 2 Jazzy binary platforms: <https://docs.ros.org/en/jazzy/Installation/Alternatives/Ubuntu-Install-Binary.html>
- NVIDIA JetPack 6 release notes: <https://docs.nvidia.com/jetson/jetpack/6.0/release-notes/>
- NVIDIA Jetson AGX Orin setup: <https://docs.nvidia.com/jetson/agx-orin-devkit/user-guide/setup_jetpack.html>
- ROS 2-workspaces en underlay/overlay: <https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Creating-A-Workspace/Creating-A-Workspace.html>
- Gatewayarchitectuur: [ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md](ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md)
- Motion core: [MOTION_CORE_IMPLEMENTATION_PLAN.md](MOTION_CORE_IMPLEMENTATION_PLAN.md)
