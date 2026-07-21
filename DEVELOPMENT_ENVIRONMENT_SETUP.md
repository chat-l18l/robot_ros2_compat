# Ontwikkelomgeving voor ROS 2, Zenoh en ESP32-P4

> Status: vastgesteld uitgangspunt voor de eerste implementatie  
> Host: Ubuntu 24.04 LTS, x86-64  
> ROS 2: Jazzy Jalisco  
> Embedded target: ESP32-P4, PlatformIO/ESP-IDF, FreeRTOS en Zenoh-Pico  
> Linux-talen: C++17 en Python 3  
> Bijgewerkt: 2026-07-22

## 1. Doel van dit document

Dit document legt vast wat op de Linux-werk-pc moet worden geïnstalleerd om de ROS 2 ↔ Zenoh-gateway en de bijbehorende tests te ontwikkelen. Het beschrijft niet alleen de commando's, maar ook waarom voor deze inrichting is gekozen.

De beoogde ontwikkel-pc wordt al gebruikt voor:

- normale C- en C++-ontwikkeling;
- Python 3;
- ImGui-toepassingen;
- PlatformIO en ESP32-P4-firmwareontwikkeling.

ROS 2 kan hier gewoon naast bestaan. Er is geen afzonderlijke computer, virtuele machine, realtimekernel of nieuwe Ubuntu-installatie nodig.

De architectuur waarop deze setup aansluit staat in:

- [`ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md`](ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md);
- [`MOTION_CORE_IMPLEMENTATION_PLAN.md`](MOTION_CORE_IMPLEMENTATION_PLAN.md).

## 2. Samenvatting van de keuze

We gebruiken:

1. Ubuntu 24.04 als normale ontwikkelomgeving;
2. ROS 2 Jazzy uit officiële Debian-pakketten in `/opt/ros/jazzy`;
3. een gewone `colcon`-workspace binnen deze Git-repository;
4. de standaard ROS 2-middleware voor de ROS-zijde van de eerste gateway;
5. een afzonderlijke Zenoh C/C++-client in de gateway;
6. `zenohd` als lokale router tijdens ontwikkeling;
7. PlatformIO/ESP-IDF afzonderlijk voor de ESP32-P4;
8. vastgezette, onderling geteste Zenoh-versies zodra de gebruikte Zenoh-Pico-versie bekend is.

De dataweg wordt daarmee:

```text
ROS 2-node
    -> rclcpp gateway
    -> Zenoh C/C++ API
    -> zenohd
    -> Ethernet
    -> ESP32-P4 met Zenoh-Pico
```

## 3. Waarom deze inrichting

### 3.1 Native Ubuntu in plaats van een VM

Ubuntu 24.04 is een officieel ondersteund platform voor ROS 2 Jazzy. Native installeren geeft eenvoudige toegang tot Ethernet, debuggers, tijdmetingen en hardware zonder USB- of netwerkdoorvoer door een VM.

Een VM zou voor dit project vooral extra netwerkconfiguratie en timingvariatie toevoegen. Docker kan later nuttig zijn voor CI, maar is niet de primaire interactieve ontwikkelomgeving.

### 3.2 Systeem-underlay plus project-overlay

De officiële ROS-installatie komt in:

```text
/opt/ros/jazzy
```

Dit is de **underlay**: een stabiele basis met ROS 2, standaardberichten en ontwikkelgereedschappen.

De eigen packages worden gebouwd in:

```text
robot_ros2_compat/ros2_ws
```

Dit is de **overlay**. Eigen broncode blijft zo onderdeel van het gewone Git-project en wordt niet in `/opt/ros` of `/usr/local` geplaatst.

Voordelen:

- systeemsoftware en projectcode blijven gescheiden;
- de ROS-installatie kan via `apt` worden bijgewerkt;
- `build/`, `install/` en `log/` zijn reproduceerbare buildproducten;
- meerdere workspaces kunnen dezelfde underlay gebruiken;
- PlatformIO en gewone CMake-projecten blijven onafhankelijk.

### 3.3 ROS 2 Jazzy, niet bouwen vanuit broncode

Voor Ubuntu 24.04 is Jazzy de logische lang ondersteunde ROS 2-distributie. Officiële pakketten zijn eenvoudiger te installeren, bij te werken en opnieuw op te bouwen dan een volledige ROS 2-broncode-installatie.

ROS 2 vanuit bron bouwen is pas gerechtvaardigd als een benodigde patch of niet-gepubliceerde functie dat werkelijk vereist.

### 3.4 Desktopinstallatie op de werk-pc

We installeren `ros-jazzy-desktop`, niet alleen `ros-base`.

De gateway zelf heeft geen grafische desktopcomponenten nodig, maar een ontwikkel-pc profiteert van RViz2, demo-nodes en inspectietools. Op de uiteindelijke robotcomputer kan later een kleinere runtime-installatie worden gekozen.

### 3.5 Geen `rmw_zenoh` als primaire gatewayroute

ROS 2 kan Zenoh als RMW gebruiken, maar onze ESP32-P4 met Zenoh-Pico is daarmee niet automatisch een ROS 2-node. Directe interoperabiliteit vraagt ook ROS-specifieke key expressions, serialisatie, type-informatie en discoveryafspraken.

De eerste gateway gebruikt daarom:

- aan de ROS-zijde: normale `rclcpp` publishers en subscriptions;
- aan de embedded zijde: de gewone Zenoh C/C++ API en eigen vaste wire-messages.

`rmw_zenoh` kan later als meetbare vergelijking worden geïnstalleerd. Het is geen vereiste om de eerste end-to-end GPIO-test te bouwen.

### 3.6 Geen globale Zenoh development-library voordat versies bekend zijn

`zenohd` mag meteen als tool worden geïnstalleerd. De Zenoh C/C++ development-library wordt echter nog niet handmatig in `/usr/local` geïnstalleerd.

Eerst moet worden vastgesteld welke Zenoh-Pico-versie de bestaande ESP32-P4-firmware gebruikt. Daarna leggen we een compatibele set vast voor:

- Zenoh-Pico op de ESP32-P4;
- Zenoh C op Linux;
- de Zenoh C++ headers;
- `zenohd`;
- protocol- en integratietests.

Hiermee voorkomen we dat een toevallige nieuwste systeembibliotheek afwijkt van de embedded client. De gekozen versies moeten in de repository en CI zichtbaar worden vastgezet.

### 3.7 Geen realtime Linux-kernel in de eerste fase

De harde regelkring blijft op de ESP32-P4:

```text
encoder -> snelheidsregelaar -> motoroutput
```

Linux en ROS 2 leveren setpoints en ontvangen status. De ESP32 bewaakt zelfstandig timeouts, e-stop en faults. Daardoor is een normale Ubuntu 24.04-kernel geschikt voor de gatewayontwikkeling.

Een realtimekernel, CPU-affinity of realtime-executor wordt alleen toegevoegd nadat latency- en jittermetingen een concreet probleem aantonen.

## 4. Vooraf controleren

Controleer platform en architectuur:

```bash
lsb_release -a
uname -m
```

Verwacht minimaal:

```text
Ubuntu 24.04
x86_64
```

Controleer of al een ROS-omgeving actief is:

```bash
printenv ROS_DISTRO
command -v ros2
```

Een lege uitvoer is goed op een nog niet ingerichte pc. Als hier een andere ROS-distributie verschijnt, open dan een schone terminal voordat Jazzy wordt geïnstalleerd of gebouwd.

Installeer niet meerdere ROS-distributies in dezelfde shellomgeving. Het combineren van verschillende `setup.bash`-bestanden kan verkeerde package- en librarypaden opleveren.

## 5. ROS 2 Jazzy installeren

De actuele officiële procedure staat op:

<https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html>

### 5.1 Basisgereedschappen en Universe

```bash
sudo apt update
sudo apt install -y software-properties-common curl ca-certificates
sudo add-apt-repository universe
sudo apt update
```

### 5.2 Officiële ROS 2-packagebron toevoegen

Gebruik het door ROS beheerde `ros2-apt-source`-pakket. Dit pakket beheert de repositoryconfiguratie en signing key.

```bash
export ROS_APT_SOURCE_VERSION=$(
    curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest \
    | grep -F '"tag_name"' \
    | awk -F'"' '{print $4}'
)
```

Controleer dat de variabele niet leeg is:

```bash
printf '%s\n' "$ROS_APT_SOURCE_VERSION"
```

Download en installeer daarna het pakket voor Ubuntu Noble:

```bash
curl -L -o /tmp/ros2-apt-source.deb \
    "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo "$VERSION_CODENAME")_all.deb"

sudo apt install -y /tmp/ros2-apt-source.deb
sudo apt update
```

Waarom deze route: losse repositoryregels en sleutels handmatig kopiëren verouderen gemakkelijker. Het officiële source-pakket maakt beheer en toekomstige wijzigingen explicieter.

### 5.3 ROS 2 en ontwikkeltools installeren

```bash
sudo apt install -y ros-jazzy-desktop ros-dev-tools
```

Dit levert onder andere:

- `ros2` command-line tools;
- `rclcpp` en `rclpy`;
- `ament_cmake` en `colcon`;
- `rosdep`;
- standaardmessages zoals `std_msgs`, `geometry_msgs`, `nav_msgs` en `sensor_msgs`;
- RViz2;
- demo-nodes en diagnostische tools.

### 5.4 ROS-installatie controleren

```bash
source /opt/ros/jazzy/setup.bash
ros2 --help
ros2 doctor --report
```

## 6. Algemene C/C++- en Python-gereedschappen

```bash
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    gdb \
    ccache \
    clang \
    clang-format \
    clang-tidy \
    python3-venv \
    python3-pytest
```

Rationale:

| Pakket | Gebruik |
|---|---|
| `build-essential` | GCC, G++ en basisbuildtools |
| `cmake` | CMake-configuratie voor ROS- en gedeelde C/C++-code |
| `ninja-build` | Snelle optionele buildbackend |
| `git` | Versiebeheer |
| `gdb` | Native debugging |
| `ccache` | Snellere herbouw na kleine wijzigingen |
| `clang-format` | Reproduceerbare broncodeopmaak |
| `clang-tidy` | Statische C/C++-controles |
| `python3-venv` | Losse Python-tools buiten de ROS-build |
| `python3-pytest` | Python unit tests |

Gebruik voor ROS-tools de Ubuntu-systeem-Python. Activeer tijdens `colcon build` geen Conda-omgeving of willekeurige Python-venv. PlatformIO mag zijn eigen beheerde Python-omgeving blijven gebruiken.

## 7. `rosdep` initialiseren

Eenmalig per computer:

```bash
sudo rosdep init
rosdep update
```

Als `rosdep init` meldt dat de configuratie al bestaat, is dat geen fout; voer dan alleen `rosdep update` uit.

Waarom `rosdep`: afhankelijkheden horen in `package.xml` te staan. Een agent of ontwikkelaar kan vervolgens de vereiste Ubuntu/ROS-pakketten installeren zonder een handmatig bijgehouden, steeds langer wordende lijst.

## 8. Zenoh-router installeren

De actuele officiële procedure staat op:

<https://zenoh.io/docs/getting-started/installation/>

Zorg dat de keyringdirectory bestaat:

```bash
sudo install -d -m 0755 /etc/apt/keyrings
```

Voeg de officiële Eclipse Zenoh signing key toe:

```bash
curl -L https://download.eclipse.org/zenoh/debian-repo/zenoh-public-key \
    | sudo gpg --dearmor --yes \
        --output /etc/apt/keyrings/zenoh-public-key.gpg
```

Voeg de repository precies eenmaal toe:

```bash
echo "deb [signed-by=/etc/apt/keyrings/zenoh-public-key.gpg] https://download.eclipse.org/zenoh/debian-repo/ /" \
    | sudo tee /etc/apt/sources.list.d/zenoh.list > /dev/null

sudo apt update
sudo apt install -y zenoh
```

Controleer de installatie:

```bash
zenohd --version
zenohd --help
```

Start de router tijdens ontwikkeling voorlopig handmatig:

```bash
zenohd
```

Maak nog geen systemd-service. Eerst moeten luisteradres, poort, discovery, routerconfiguratie, logging en security als projectconfiguratie zijn vastgelegd en getest.

## 9. Shellomgevingen bewust scheiden

Bron ROS 2 in iedere terminal waarin ROS-commando's of een ROS-build worden uitgevoerd:

```bash
source /opt/ros/jazzy/setup.bash
```

Na het bouwen van de eigen workspace:

```bash
source /opt/ros/jazzy/setup.bash
source ros2_ws/install/setup.bash
```

Zet deze regels aanvankelijk niet automatisch in `.bashrc`. Expliciet sourcen voorkomt dat ROS-variabelen onbedoeld invloed hebben op PlatformIO, andere CMake-projecten, Python-venvs of een later geïnstalleerde ROS-distributie.

Een projectspecifiek helperscript mag later hetzelfde gecontroleerd doen, maar moet eerst controleren dat `ROS_DISTRO` leeg of `jazzy` is.

## 10. Aanbevolen repositorystructuur

```text
robot_ros2_compat/
├── DEVELOPMENT_ENVIRONMENT_SETUP.md
├── MOTION_CORE_IMPLEMENTATION_PLAN.md
├── ROS2_ZENOH_GATEWAY_ARCHITECTURE_PLAN.md
├── ros2_ws/
│   └── src/
│       ├── robot_interfaces/
│       └── ros2_zenoh_gateway/
├── firmware/
│   └── esp32p4/
├── protocol/
│   ├── include/
│   ├── src/
│   └── tests/
└── tools/
```

Verantwoordelijkheden:

| Directory | Verantwoordelijkheid | Buildmethode |
|---|---|---|
| `ros2_ws/` | ROS 2-packages en gateway | `colcon`/ament |
| `firmware/esp32p4/` | FreeRTOS- en hardwarecode | PlatformIO/ESP-IDF |
| `protocol/` | Wire-format, codecs en hosttests | gewone CMake-library |
| `tools/` | Test-, meet- en ontwikkelhulpmiddelen | per tool vastleggen |

De gegenereerde ROS-directories horen niet in Git:

```text
ros2_ws/build/
ros2_ws/install/
ros2_ws/log/
```

## 11. Normale ontwikkelcyclus

Vanaf de repositoryroot:

```bash
source /opt/ros/jazzy/setup.bash
cd ros2_ws
```

Installeer gedeclareerde packageafhankelijkheden:

```bash
rosdep install \
    --from-paths src \
    --ignore-src \
    --rosdistro jazzy \
    -y
```

Bouw de workspace:

```bash
colcon build \
    --symlink-install \
    --cmake-args \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Activeer de overlay:

```bash
source install/setup.bash
```

Voer tests uit:

```bash
colcon test --event-handlers console_direct+
colcon test-result --verbose
```

Waarom deze opties:

- `--symlink-install` versnelt iteratie bij scripts, launchfiles en configuratie;
- `RelWithDebInfo` combineert bruikbare optimalisatie met debuginformatie;
- `CMAKE_EXPORT_COMPILE_COMMANDS` ondersteunt editors, `clangd` en statische analyse;
- `colcon test-result --verbose` maakt falende tests zichtbaar voor ontwikkelaars en agents.

## 12. ROS 2-smoketest na installatie

Terminal 1:

```bash
source /opt/ros/jazzy/setup.bash
ros2 run demo_nodes_cpp talker
```

Terminal 2:

```bash
source /opt/ros/jazzy/setup.bash
ros2 run demo_nodes_cpp listener
```

Aanvullende inspectie:

```bash
ros2 node list
ros2 topic list
ros2 topic echo /chatter
```

Geslaagd wanneer:

- talker en listener elkaar ontdekken;
- `/chatter` zichtbaar is;
- berichten zonder fouten worden ontvangen;
- `ros2 doctor --report` geen blokkerende configuratiefout meldt.

Deze test bewijst alleen de lokale ROS 2-installatie. Hij bewijst Zenoh, Ethernet of de ESP32 nog niet.

## 13. Eerste projectmijlpaal na de installatie

Maak eerst een kleine ROS-only gatewaypackage met:

- `ament_cmake`;
- `rclcpp`;
- `std_msgs`;
- `ament_cmake_gtest`;
- één publisher/subscription-smoketest.

Daarna volgt de veilige verticale GPIO-slice:

```text
ROS 2 Bool-command
    -> C++ gateway
    -> Zenoh
    -> ESP32-P4
    -> GPIO
    -> Zenoh-status
    -> C++ gateway
    -> ROS 2 state-topic
```

Pas nadat dit pad en het buildsysteem reproduceerbaar werken, voegen we toe:

- `geometry_msgs/msg/TwistStamped`;
- het eigen `MotionCommand`-wire-format;
- odometrie met `nav_msgs/msg/Odometry`;
- de watchdog en safety gate;
- werkelijke motoroutput.

## 14. Bewust nog niet installeren

Installeer in de eerste fase niet:

- een realtime Linux-kernel;
- ROS 2 vanuit broncode;
- meerdere ROS 2-distributies;
- `rmw_zenoh` als standaard-RMW;
- Micro-ROS;
- Gazebo;
- Nav2;
- MoveIt;
- volledige Clearpath robotsoftware;
- Docker als primaire ontwikkelomgeving;
- willekeurige Zenoh C/C++-versies onder `/usr/local`;
- een automatisch startende `zenohd`-service.

Deze onderdelen zijn niet verboden. Ze worden uitgesteld totdat een concrete requirement, falende test of meting aantoont dat ze nodig zijn. Dat houdt de eerste ontwikkelomgeving klein en verklaarbaar.

## 15. Later optionele installaties

### `rmw_zenoh`

Alleen voor een afzonderlijke vergelijkingsproef:

```bash
sudo apt install ros-jazzy-rmw-zenoh-cpp
```

Zet zonder vastgesteld testplan niet permanent:

```bash
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
```

### Zenoh C en C++

Pas installeren of als dependency opnemen nadat de ESP32-P4-versie is geïnventariseerd. De Zenoh C++ API is een C++17 headerbinding boven Zenoh C of Zenoh-Pico. De gekozen installatiemethode en exacte versies worden dan onderdeel van het buildontwerp.

Officiële bron:

<https://github.com/eclipse-zenoh/zenoh-cpp>

### Simulatie en navigation

Gazebo, Nav2 en de Clearpath-stack zijn pas nodig nadat de minimale gateway en motion core stabiel zijn. Dan worden hun versies en systeemafhankelijkheden als aparte mijlpaal toegevoegd, niet stilzwijgend aan de basisinstallatie.

## 16. Installatiegegevens vastleggen

Na een geslaagde installatie moeten minimaal deze gegevens in een issue, testlog of reproduceerbaar environment-report worden bewaard:

```bash
lsb_release -ds
uname -m
ros2 --version 2>/dev/null || true
apt-cache policy ros-jazzy-desktop ros-dev-tools
zenohd --version
cmake --version
gcc --version | head -n 1
g++ --version | head -n 1
python3 --version
pio --version 2>/dev/null || true
```

Omdat niet iedere `ros2` CLI-versie een bruikbare `--version` rapporteert, zijn `apt-cache policy` en de ROS packageversies de gezaghebbende installatiegegevens.

Voor Zenoh moet later bovendien expliciet worden vastgelegd:

- Zenoh routerversie;
- Zenoh C-versie;
- Zenoh C++-versie;
- Zenoh-Pico commit/tag;
- ESP-IDF- en PlatformIO-platformversie.

## 17. Definitie van gereed

De ontwikkel-pc is gereed voor de eerste implementatie wanneer:

- Ubuntu 24.04 en de juiste architectuur zijn bevestigd;
- ROS 2 Jazzy uit officiële pakketten is geïnstalleerd;
- `rosdep` is geïnitialiseerd;
- de talker/listener-smoketest slaagt;
- `zenohd` handmatig start en zijn versie rapporteert;
- de normale C/C++-tools aanwezig zijn;
- PlatformIO buiten de ROS-shell nog normaal werkt;
- de repository een `ros2_ws/src` kan bevatten;
- een leeg of minimaal ROS-package met `colcon` bouwt en test;
- geen motoroutput aan een netwerkcommando is gekoppeld.

## 18. Bronnen

- ROS 2 Jazzy op Ubuntu via Debian-pakketten: <https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html>
- ROS 2-workspaces en underlay/overlay: <https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Creating-A-Workspace/Creating-A-Workspace.html>
- Bouwen met `colcon`: <https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Colcon-Tutorial.html>
- Afhankelijkheden beheren met `rosdep`: <https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Rosdep.html>
- Zenoh-installatie: <https://zenoh.io/docs/getting-started/installation/>
- Zenoh C++ API: <https://github.com/eclipse-zenoh/zenoh-cpp>

Controleer bij een nieuwe machine altijd eerst de officiële installatiepagina's. Repository-URL's, signing keys en pakketnamen kunnen na publicatie van dit document veranderen.
