# Flipper Zero LAN Tester (W5500)

> **[Русская версия ниже / Russian version below](#русская-версия)**

Turn your **Flipper Zero + W5500 Lite** module into a professional-grade portable LAN tester. Analyze Ethernet links, discover network neighbors, scan subnets, fingerprint DHCP servers --- all from a pocket-sized device.

![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-OFW-orange)
![License](https://img.shields.io/badge/license-MIT-blue)
![Language](https://img.shields.io/badge/language-C99-green)
![Build](https://img.shields.io/badge/build-ufbt-yellow)
![Version](https://img.shields.io/badge/version-2.7.0-brightgreen)

**[English docs](docs/en/README.md)** | **[Документация на русском](docs/ru/README.md)**

![Main menu](docs/screenshots/main_menu.png)

---

## Features

| Feature | Description |
|---|---|
| **Auto Test** | One-touch automated diagnostics: Link → DHCP → Ping GW → DNS → Internet Ping → LLDP/CDP → ARP count. Auto-cycles on cable replug. |
| **Link Info** | PHY link status, speed (10/100 Mbps), duplex (Half/Full), MAC address, W5500 version check |
| **DHCP Analyzer** | Discover-only analysis (no IP lease taken), option fingerprinting, full offer parsing |
| **ARP Scanner** | Active subnet scan with batch requests, OUI vendor lookup (~120 vendors), duplicate detection |
| **Ping** | Echo request/reply to any IP with configurable count and timeout |
| **Continuous Ping** | Real-time RTT graph with min/max/avg and packet loss, configurable interval |
| **DNS Lookup** | Resolve hostnames via UDP DNS, supports custom DNS server |
| **Traceroute** | ICMP-based hop-by-hop path discovery, accepts IPs and hostnames with DNS resolve |
| **Ping Sweep** | ICMP sweep of an entire subnet with interactive host list — click to ping, scan, or WOL |
| **Port Scanner** | TCP connect scan: Top-20, Top-100 presets, or custom port range (1-65535) |
| **LLDP/CDP** | Passive IEEE 802.1AB & Cisco CDP neighbor discovery with full TLV parsing |
| **mDNS/SSDP** | Discover services and devices via multicast DNS and UPnP/SSDP |
| **STP/VLAN** | Passive BPDU listener + 802.1Q VLAN tag detection |
| **Statistics** | Frame counters by type (unicast/broadcast/multicast) and EtherType |
| **Wake-on-LAN** | Send magic packets to any MAC address |
| **Packet Capture** | Standalone PCAP traffic dump — capture raw Ethernet frames to .pcap file on SD card |
| **ETH Bridge** | USB-to-Ethernet bridge: phone/PC gets LAN access via Flipper (CDC-ECM), optional PCAP traffic dump to SD card |
| **PXE Server** | Minimal PXE boot server with built-in DHCP + TFTP, boots .kpxe/.efi files from SD card |
| **PXE Download** | Download iPXE and EFI boot files from the internet directly to SD card for PXE Server |
| **File Manager** | Web-based file manager: browse, download, upload, delete files on microSD via HTTP from any browser on the LAN. Supports custom CSS/JS themes from SD card |
| **SNMP GET** | Query device info via SNMPv1/v2c: sysName, sysDescr, sysUpTime, ifOperStatus |
| **NTP Diagnostics** | NTP server analysis: stratum, root delay/dispersion, reference ID, RTT |
| **NetBIOS Query** | Discover Windows machine names, workgroups, and MAC addresses |
| **DNS Poison Check** | Compare local vs public DNS (8.8.8.8) to detect poisoning or split-horizon |
| **ARP Watch** | Passive ARP monitoring: detect spoofing, duplicate IPs, gratuitous ARP storms |
| **Rogue DHCP** | Send Discover, collect Offers from multiple servers, detect unauthorized DHCP |
| **Rogue RA** | Listen for IPv6 Router Advertisements, detect unauthorized routers |
| **DHCP Fingerprint** | Identify client OS by DHCP option 55 (Windows, Linux, macOS, Android, etc.) |
| **802.1X Probe** | Send EAPOL-Start to detect 802.1X port authentication, identify EAP type |
| **VLAN Hopping** | Send 802.1Q tagged frames to test VLAN isolation (Top 10 / Custom VLANs) |
| **TFTP Client** | Download config files from network equipment via TFTP, save to SD card |
| **IPMI v1.5** | Query BMC: chassis power status, device ID, firmware version |
| **History** | All scan results auto-saved with timestamps, browsable and deletable |
| **Settings** | Auto-save, sound/vibro, custom DNS server, ping count/timeout/interval, clear history, MAC Changer |

### UX Highlights

- **Hierarchical menu**: features grouped into Port Info, Scan, Diagnostics, Traffic, Security, Utilities
- **Link status in header**: see UP/DOWN, speed, duplex without entering Link Info
- **DHCP caching**: single negotiation shared across all operations — no repeated 15s waits
- **Visual progress**: countdown timers for listeners, ASCII progress bars for scans
- **LED/vibro feedback**: green blink on success, red on error (optional, toggle in Settings)
- **Smart defaults**: IP inputs pre-populated with DHCP gateway

## Hardware

### Required

- **Flipper Zero** (OFW firmware)
- **W5500 Lite** Ethernet module (or any W5500-based board with SPI)

### Where to buy

- [W5500 Ethernet Module for Flipper Zero](https://flipperaddons.com/product/w5500-ethernet/) — ready-to-use module with RJ45

### Wiring

```
W5500 Module    Flipper Zero GPIO
─────────────   ─────────────────
MOSI (MO)   →   A7  (pin 2)
SCLK (SCK)  →   B3  (pin 5)
CS   (nSS)  →   A4  (pin 4)
MISO (MI)   →   A6  (pin 3)
RESET (RST) →   C3  (pin 7)
3V3  (VCC)  →   3V3 (pin 9)
GND  (G)    →   GND (pin 8 or 11)
```

> The W5500 is powered via Flipper's OTG 3.3V output, which is enabled automatically when the app starts.

## Building

### Prerequisites

- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool)

### Build & Install

```bash
cd lan_tester
ufbt build              # build only
ufbt launch             # build and run on Flipper via USB
ufbt install            # install .fap to Flipper's SD card
```

The compiled `.fap` file will appear in `dist/`. You can also copy it manually to the Flipper's SD card at `/ext/apps/GPIO/`.

## Architecture

```
├── application.fam              # FAP manifest
├── lan_tester_app.c             # Entry point, ViewDispatcher, feature logic
├── lan_tester_app.h             # Shared types and app state
│
├── hal/
│   ├── w5500_hal.c              # SPI, GPIO, MACRAW socket management
│   └── w5500_hal.h
│
├── usb_eth/
│   ├── usb_eth.c / .h           # USB CDC-ECM network device (init/deinit/send/recv)
│   └── usb_descriptors.c / .h   # USB device & config descriptors, endpoint callbacks
│
├── bridge/
│   ├── eth_bridge.c             # Bidirectional L2 frame forwarding engine
│   ├── eth_bridge.h
│   ├── pcap_dump.c              # PCAP traffic dump to SD card (Wireshark-compatible)
│   └── pcap_dump.h
│
├── protocols/
│   ├── lldp.c / lldp.h         # IEEE 802.1AB LLDP parser
│   ├── cdp.c / cdp.h           # Cisco CDP parser (LLC/SNAP)
│   ├── arp_scan.c / arp_scan.h  # ARP request builder & reply parser
│   ├── arp_watch.c / .h        # ARP spoofing & storm detection
│   ├── dhcp_discover.c / .h     # DHCP Discover builder & Offer parser
│   ├── dhcp_fingerprint.c / .h  # OS fingerprinting via DHCP option 55
│   ├── dns_lookup.c / .h       # DNS A-record resolver via UDP
│   ├── dns_poison.c / .h       # DNS poisoning check (local vs public)
│   ├── discovery.c / .h        # mDNS + SSDP service discovery
│   ├── eapol_probe.c / .h      # 802.1X EAPOL-Start probe
│   ├── icmp.c / icmp.h         # ICMP Echo (ping) via IPRAW
│   ├── ipmi_client.c / .h      # IPMI v1.5 over LAN (chassis, device ID)
│   ├── netbios_query.c / .h    # NetBIOS Name Query (NBSTAT)
│   ├── ntp_diag.c / .h         # NTP diagnostics (stratum, offset, RTT)
│   ├── port_scan.c / .h        # TCP connect port scanner
│   ├── rogue_dhcp.c / .h       # Rogue DHCP server detection
│   ├── rogue_ra.c / .h         # Rogue IPv6 Router Advertisement detection
│   ├── snmp_client.c / .h      # SNMP v1/v2c GET client (BER/ASN.1)
│   ├── stp_vlan.c / .h         # STP BPDU parser + 802.1Q VLAN detection
│   ├── tftp_client.c / .h      # TFTP file download client (RFC 1350)
│   ├── traceroute.c / .h       # ICMP traceroute with TTL
│   ├── vlan_hop.c / .h         # VLAN hopping test (802.1Q tagged frames)
│   ├── wol.c / .h              # Wake-on-LAN magic packet
│   ├── ping_graph.c / .h       # Ring buffer RTT graph for continuous ping
│   ├── mac_changer.c / .h      # Random/custom MAC with SD persistence
│   ├── lldp.c / .h             # IEEE 802.1AB LLDP parser
│   ├── cdp.c / .h              # Cisco Discovery Protocol parser
│   ├── http_download.c / .h    # HTTP file downloader (for PXE boot files)
│   ├── pxe_server.c / .h      # PXE boot server (DHCP + TFTP)
│   ├── file_manager.c / .h    # Web-based SD card file manager (HTTP server)
│   └── history.c / .h          # Timestamped result storage on SD card
│
├── utils/
│   ├── oui_lookup.c / .h       # MAC → Vendor (top ~120 OUI prefixes)
│   └── packet_utils.c / .h     # Endian helpers, checksums, formatters
│
├── assets/
│   └── icon.png                 # 10x10 FAP icon
│
└── lib/
    └── ioLibrary_Driver/        # WIZnet W5500 driver
```

## Usage

1. Connect the W5500 module to Flipper Zero using the wiring diagram above
2. Plug an Ethernet cable into the W5500's RJ45 port
3. Open **GPIO → LAN Tester** on the Flipper
4. The menu header shows link status (e.g. `LAN [UP 100M FD]`)
5. Select a category and then a tool:

### Port Info
- **Link Info** — link status, speed, duplex, MAC. Use first to verify hardware.
- **DHCP Analyze** — sends Discover, parses Offer. Does **not** take an IP lease.
- **LLDP/CDP** — listens up to 60s for switch neighbor advertisements.
- **STP/VLAN** — listens 30s for BPDU frames and 802.1Q VLAN tags.
- **SNMP GET** — query sysName, sysDescr, sysUpTime, ifStatus via SNMPv1/v2c.

### Scan
- **ARP Scan** — scans local subnet via DHCP-detected range, shows IP/MAC/vendor.
- **Ping Sweep** — ICMP sweep of a CIDR range, auto-detected or manually entered.
- **mDNS/SSDP** — discovers services via multicast DNS and UPnP.
- **NetBIOS Query** — discover Windows machine names and workgroups.
- **Port Scan (Top 20/100/Custom)** — TCP connect scan of common ports.

### Diagnostics
- **Ping** — 4 pings to any IP (default: gateway from DHCP).
- **Continuous Ping** — live RTT graph with loss tracking, runs until Back.
- **DNS Lookup** — resolves a hostname via the DHCP-provided DNS server.
- **Traceroute** — hop-by-hop ICMP path discovery up to 30 hops.
- **NTP Diagnostics** — stratum, root delay, reference ID, RTT.
- **DNS Poison Check** — compare local vs public DNS responses.

### Traffic
- **Packet Capture** — capture raw Ethernet frames to .pcap file on SD card.
- **ETH Bridge** — USB-to-Ethernet bridge via CDC-ECM with optional PCAP dump.
- **Statistics** — frame counters by type and EtherType (10s capture).

### Security
- **ARP Watch** — detect ARP spoofing, duplicate IPs, gratuitous ARP storms (15s scan).
- **Rogue DHCP** — send Discover, detect unauthorized DHCP servers.
- **Rogue RA (IPv6)** — listen for unauthorized Router Advertisements (15s scan).
- **DHCP Fingerprint** — identify client OS by option 55 parameter list (30s listen).
- **802.1X Probe** — send EAPOL-Start, detect port authentication and EAP type.
- **VLAN Hop Top10** — test VLAN isolation on common VLANs (1,2,10,20,50,100,150,200,300,999).
- **VLAN Hop Custom** — test user-specified VLAN IDs (comma-separated).
### Utilities
- **Wake-on-LAN** — send magic packet to wake a device by MAC address.
- **PXE Server** — minimal PXE boot server with built-in DHCP + TFTP.
- **File Manager** — web-based SD card file manager via HTTP on port 80.
- **TFTP Client** — download config files from network equipment to SD card.
- **IPMI Query** — query BMC chassis status, device ID, firmware version.

### Settings
- **Auto-save results** — ON/OFF, controls automatic history saving.
- **Sound & vibro** — ON/OFF, controls LED/vibro notifications.
- **Clear History** — delete all saved result files.
- **MAC Changer** — generate random MAC or enter custom, saved to SD.

### File Manager Custom Themes

File Manager supports custom CSS and JavaScript loaded from SD card. Place files in:

- **apps_data/lan_tester/web/custom.css** --- replaces default styles
- **apps_data/lan_tester/web/custom.js** --- replaces default sorting script

Files are detected once when File Manager starts. To apply changes, restart the tool.

Example themes and an advanced custom.js (with search, breadcrumbs, multi-select, drag-and-drop upload, file preview) are included in **docs/filemanager_themes/**. Copy the desired file to the path above on your SD card.

## Technical Details

- **W5500 MACRAW mode**: Socket 0 with `MFEN=0` (promiscuous --- receives all frames including multicast)
- **Worker thread**: 8 KB stack, non-blocking UI via ViewDispatcher + worker pattern
- **DHCP caching**: single negotiation, result reused across all subsequent operations
- **Memory-safe**: large buffers heap-allocated, frame buffer on heap (4 KB app stack), bounds checking on all parsers
- **Endianness**: manual big-endian parsing --- no float printf, no `htons`/`ntohs`

## OUI Vendor Database

The built-in lookup table covers ~120 common OUI prefixes including:

> Cisco, HP/HPE, Dell, Intel, Broadcom, Realtek, Apple, Samsung, Huawei, TP-Link, Ubiquiti, Juniper, Arista, MikroTik, Netgear, ASUS, D-Link, Synology, QNAP, VMware, Microsoft, Google, Amazon, Lenovo, Supermicro, Aruba, Fortinet, Palo Alto, WIZnet, Raspberry Pi, Espressif, and more.

## Credits

- Based on [arag0re/fz-eth-troubleshooter](https://github.com/arag0re/fz-eth-troubleshooter) (fork of [karasevia/finik_eth](https://github.com/karasevia/finik_eth))
- Uses [WIZnet ioLibrary_Driver](https://github.com/Wiznet/ioLibrary_Driver) for W5500 hardware abstraction
- Built for [Flipper Zero OFW](https://github.com/flipperdevices/flipperzero-firmware)

## License

MIT License. See [LICENSE](LICENSE) for details.

---

---

# Русская версия

# Flipper Zero LAN Тестер (W5500)

Превратите **Flipper Zero + модуль W5500 Lite** в профессиональный портативный LAN-тестер. Анализ Ethernet-соединений, обнаружение сетевых соседей, сканирование подсетей, фингерпринтинг DHCP-серверов --- всё с устройства, помещающегося в карман.

---

## Возможности

| Функция | Описание |
|---|---|
| **Auto Test** | Автоматическая диагностика в одно касание: Link → DHCP → Ping GW → DNS → Internet Ping → LLDP/CDP → ARP. Автоцикл при перетыке кабеля. |
| **Link Info** | Статус PHY-линка, скорость (10/100 Мбит/с), дуплекс, MAC-адрес, версия W5500 |
| **DHCP Analyzer** | Анализ Discover/Offer (IP не берётся!), фингерпринтинг опций |
| **ARP Scanner** | Сканирование подсети с определением вендора по OUI (~120 производителей) |
| **Ping** | Echo Request/Reply на любой IP с измерением RTT (4 пинга, таймаут 3с) |
| **Continuous Ping** | Графики RTT в реальном времени с min/max/avg и процентом потерь |
| **DNS Lookup** | Разрешение имён через UDP DNS сервер из DHCP |
| **Traceroute** | ICMP трассировка маршрута с RTT на каждый хоп |
| **Ping Sweep** | ICMP-сканирование всей подсети, CIDR автоопределяется из DHCP |
| **Port Scanner** | TCP connect-сканирование: Top-20 (быстро) и Top-100 (полно) |
| **LLDP/CDP** | Пассивное обнаружение соседей IEEE 802.1AB и Cisco CDP |
| **mDNS/SSDP** | Обнаружение сервисов через multicast DNS и UPnP/SSDP |
| **STP/VLAN** | Пассивный захват BPDU + определение 802.1Q VLAN-тегов |
| **Статистика** | Счётчики фреймов по типу и EtherType |
| **Wake-on-LAN** | Отправка magic-пакетов на любой MAC-адрес |
| **ETH Bridge** | USB-Ethernet мост: телефон/ПК получает доступ в LAN через Flipper (CDC-ECM), опциональный PCAP-дамп трафика на SD |
| **PXE Server** | Минимальный PXE-сервер с DHCP + TFTP, загрузка .kpxe/.efi файлов с SD-карты |
| **PXE Download** | Скачивание iPXE и EFI boot-файлов из интернета на SD-карту для PXE Server |
| **File Manager** | Веб-менеджер файлов: просмотр, скачивание, загрузка, удаление файлов на microSD через HTTP. Поддержка кастомных CSS/JS тем с SD-карты |
| **SNMP GET** | Запрос информации об устройстве по SNMPv1/v2c: sysName, sysDescr, sysUpTime, ifStatus |
| **NTP Diagnostics** | Анализ NTP-сервера: stratum, root delay/dispersion, reference ID, RTT |
| **NetBIOS Query** | Обнаружение имён Windows-машин, рабочих групп и MAC-адресов |
| **DNS Poison Check** | Сравнение локального и публичного DNS (8.8.8.8) для обнаружения подмены |
| **ARP Watch** | Пассивный мониторинг ARP: обнаружение спуфинга, дубликатов IP, ARP-штормов |
| **Rogue DHCP** | Отправка Discover, сбор Offer от нескольких серверов, обнаружение неавторизованных DHCP |
| **Rogue RA** | Прослушивание IPv6 Router Advertisement, обнаружение неавторизованных роутеров |
| **DHCP Fingerprint** | Определение ОС клиентов по DHCP option 55 (Windows, Linux, macOS, Android и др.) |
| **802.1X Probe** | Отправка EAPOL-Start для проверки 802.1X аутентификации на порту |
| **VLAN Hopping** | Отправка 802.1Q tagged-фреймов для проверки изоляции VLAN (Top 10 / Custom) |
| **TFTP Client** | Скачивание конфигурационных файлов с оборудования по TFTP на SD-карту |
| **IPMI v1.5** | Запрос BMC: статус питания шасси, ID устройства, версия прошивки |
| **История** | Все результаты автосохраняются с метками времени, просмотр и удаление |
| **Настройки** | Автосохранение, звук/вибрация, очистка истории, MAC Changer (смена MAC с сохранением на SD) |

### UX-особенности

- **Иерархическое меню**: функции сгруппированы в Port Info, Scan, Diagnostics, Traffic, Security, Utilities
- **Статус линка в заголовке**: UP/DOWN, скорость, дуплекс видны сразу
- **Кеширование DHCP**: одна DHCP-сессия на всё — не ждёте 15 секунд каждый раз
- **Визуальный прогресс**: таймеры обратного отсчёта для прослушиваний, прогрессбары для сканов
- **LED/вибро оповещения**: зелёный при успехе, красный при ошибке (опционально)
- **Умные дефолты**: IP-поля предзаполнены шлюзом из DHCP

## Оборудование

### Необходимо

- **Flipper Zero** (официальная прошивка OFW)
- **W5500 Lite** Ethernet-модуль (или любая плата на W5500 с SPI)

### Где купить

- [W5500 Ethernet модуль для Flipper Zero](https://flipperaddons.com/product/w5500-ethernet/) — готовый модуль с RJ45

### Подключение

```
Модуль W5500     GPIO Flipper Zero
─────────────    ─────────────────
MOSI (MO)    →    A7  (пин 2)
SCLK (SCK)   →    B3  (пин 5)
CS   (nSS)   →    A4  (пин 4)
MISO (MI)    →    A6  (пин 3)
RESET (RST)  →    C3  (пин 7)
3V3  (VCC)   →    3V3 (пин 9)
GND  (G)     →    GND (пин 8 или 11)
```

> W5500 питается через OTG 3.3В Flipper'а, который включается автоматически при запуске приложения.

## Сборка

### Требования

- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool)

### Сборка и установка

```bash
cd lan_tester
ufbt build              # только сборка
ufbt launch             # сборка и запуск на Flipper через USB
ufbt install            # установка .fap на SD-карту Flipper
```

Скомпилированный `.fap` файл появится в `dist/`. Его также можно скопировать вручную на SD-карту Flipper'а в `/ext/apps/GPIO/`.

## Архитектура

```
├── application.fam              # Манифест FAP
├── lan_tester_app.c             # Точка входа, ViewDispatcher, логика функций
├── lan_tester_app.h             # Общие типы и состояние приложения
│
├── hal/                         # Hardware Abstraction Layer
│   ├── w5500_hal.c              # SPI, GPIO, управление MACRAW-сокетом
│   └── w5500_hal.h
│
├── usb_eth/                     # USB CDC-ECM сетевое устройство
│   ├── usb_eth.c / .h           # Инициализация/деинит/отправка/приём
│   └── usb_descriptors.c / .h   # USB-дескрипторы, обработчики endpoints
│
├── bridge/                      # Движок Ethernet-моста
│   ├── eth_bridge.c             # Двунаправленная L2-пересылка фреймов
│   ├── eth_bridge.h
│   ├── pcap_dump.c              # PCAP-дамп трафика на SD (совместим с Wireshark)
│   └── pcap_dump.h
│
├── protocols/                   # Парсеры и генераторы протоколов (29 файлов)
│   ├── arp_scan.c / .h         # ARP-запросы и парсер ответов
│   ├── arp_watch.c / .h        # Обнаружение ARP-спуфинга
│   ├── cdp.c / .h              # Парсер Cisco CDP (LLC/SNAP)
│   ├── dhcp_discover.c / .h    # DHCP Discover/Offer
│   ├── dhcp_fingerprint.c / .h # Фингерпринт ОС по DHCP option 55
│   ├── discovery.c / .h        # mDNS + SSDP обнаружение
│   ├── dns_lookup.c / .h       # DNS A-запросы через UDP
│   ├── dns_poison.c / .h       # Проверка подмены DNS
│   ├── eapol_probe.c / .h      # 802.1X EAPOL-Start проба
│   ├── icmp.c / .h             # ICMP Echo (ping)
│   ├── ipmi_client.c / .h      # IPMI v1.5 over LAN
│   ├── lldp.c / .h             # Парсер IEEE 802.1AB LLDP
│   ├── netbios_query.c / .h    # NetBIOS Name Query (NBSTAT)
│   ├── ntp_diag.c / .h         # NTP-диагностика
│   ├── port_scan.c / .h        # TCP connect сканер портов
│   ├── radius_client.c / .h    # RADIUS Access-Request (PAP/MD5)
│   ├── rogue_dhcp.c / .h       # Обнаружение Rogue DHCP
│   ├── rogue_ra.c / .h         # Обнаружение Rogue RA (IPv6)
│   ├── snmp_client.c / .h      # SNMP v1/v2c GET (BER/ASN.1)
│   ├── stp_vlan.c / .h         # STP BPDU + 802.1Q VLAN
│   ├── tftp_client.c / .h      # TFTP-клиент (RFC 1350)
│   ├── traceroute.c / .h       # ICMP traceroute с TTL
│   ├── vlan_hop.c / .h         # VLAN hopping (802.1Q tagged)
│   ├── wol.c / .h              # Wake-on-LAN magic packet
│   ├── ping_graph.c / .h       # Кольцевой буфер RTT
│   ├── mac_changer.c / .h      # Смена MAC с сохранением на SD
│   ├── pxe_server.c / .h       # PXE-сервер (DHCP + TFTP)
│   ├── file_manager.c / .h     # Веб-менеджер файлов (HTTP)
│   └── history.c / .h          # Хранение результатов на SD
│
├── utils/
│   ├── oui_lookup.c / .h       # MAC → Вендор (~120 OUI-префиксов)
│   └── packet_utils.c / .h     # Байтовый порядок, контрольные суммы
│
├── assets/
│   └── icon.png                 # Иконка FAP 10x10
│
└── lib/
    └── ioLibrary_Driver/        # Драйвер WIZnet W5500
```

## Использование

1. Подключите модуль W5500 к Flipper Zero по схеме выше
2. Вставьте Ethernet-кабель в RJ45 разъём W5500
3. Откройте **GPIO → LAN Tester** на Flipper'е
4. В заголовке меню отображается статус линка (напр. `LAN [UP 100M FD]`)
5. Выберите категорию, затем инструмент:

### Port Info
- **Link Info** — статус линка, скорость, дуплекс, MAC.
- **DHCP Analyze** — Discover/Offer без занятия адреса.
- **LLDP/CDP** — пассивное обнаружение соседей (до 60с).
- **STP/VLAN** — BPDU + определение 802.1Q VLAN-тегов.
- **SNMP GET** — sysName, sysDescr, sysUpTime, ifStatus по SNMPv1/v2c.

### Scan
- **ARP Scan** — сканирование подсети, IP/MAC/вендор.
- **Ping Sweep** — ICMP-свип по CIDR.
- **mDNS/SSDP** — обнаружение сервисов через multicast DNS и UPnP.
- **NetBIOS Query** — имена Windows-машин и рабочие группы.
- **Port Scan (Top 20/100/Custom)** — TCP connect-сканирование.

### Diagnostics
- **Ping** — пинг на любой IP (по умолчанию — шлюз).
- **Continuous Ping** — график RTT с отслеживанием потерь.
- **DNS Lookup** — разрешение имён через DNS.
- **Traceroute** — ICMP-трассировка до 30 хопов.
- **NTP Diagnostics** — stratum, root delay, reference ID, RTT.
- **DNS Poison Check** — сравнение локального и публичного DNS.

### Traffic
- **Packet Capture** — захват фреймов в .pcap на SD.
- **ETH Bridge** — USB-Ethernet мост (CDC-ECM) с опциональным PCAP-дампом.
- **Statistics** — счётчики фреймов по типам (10с).

### Security
- **ARP Watch** — обнаружение спуфинга, дубликатов IP, ARP-штормов (15с).
- **Rogue DHCP** — обнаружение неавторизованных DHCP-серверов.
- **Rogue RA (IPv6)** — обнаружение неавторизованных Router Advertisement (15с).
- **DHCP Fingerprint** — определение ОС клиентов по option 55 (30с).
- **802.1X Probe** — EAPOL-Start, обнаружение аутентификации и типа EAP.
- **VLAN Hop Top10** — проверка изоляции на VLANs 1,2,10,20,50,100,150,200,300,999.
- **VLAN Hop Custom** — проверка произвольных VLAN ID (через запятую).
- **RADIUS Test** — Access-Request с PAP/MD5, проверка ответа сервера.

### Utilities
- **Wake-on-LAN** — magic-пакет для пробуждения устройства.
- **PXE Server** — PXE-сервер с DHCP + TFTP.
- **File Manager** — веб-менеджер SD через HTTP.
- **TFTP Client** — скачивание конфигов с оборудования на SD.
- **IPMI Query** — статус шасси BMC, ID устройства, версия прошивки.

### Settings
- **Auto-save results** — вкл/выкл автосохранение.
- **Sound & vibro** — вкл/выкл LED/вибро.
- **Clear History** — удалить все результаты.
- **MAC Changer** — генерация/ввод MAC, сохранение на SD.

### Кастомные темы File Manager

File Manager поддерживает пользовательские CSS и JavaScript с SD-карты:

- **apps_data/lan_tester/web/custom.css** --- заменяет стандартные стили
- **apps_data/lan_tester/web/custom.js** --- заменяет стандартный скрипт сортировки

Файлы обнаруживаются при запуске File Manager. Для применения изменений перезапустите инструмент.

Примеры тем и расширенный custom.js (поиск, хлебные крошки, мультивыбор, drag-and-drop загрузка, просмотр файлов) находятся в **docs/filemanager_themes/**. Скопируйте нужный файл по указанному пути на SD-карту.

## Технические детали

- **W5500 MACRAW режим**: Socket 0 с `MFEN=0` (принимает все фреймы)
- **Worker thread**: 8 КБ стек, неблокирующий UI через ViewDispatcher + worker
- **Кеширование DHCP**: одна сессия, результат переиспользуется всеми операциями
- **Безопасность памяти**: буферы в куче, frame_buf в куче (стек приложения 4 КБ)
- **Порядок байтов**: ручной парсинг big-endian — нет float printf, нет `htons`/`ntohs`

## База данных OUI-вендоров

Встроенная таблица покрывает ~120 распространённых OUI-префиксов:

> Cisco, HP/HPE, Dell, Intel, Broadcom, Realtek, Apple, Samsung, Huawei, TP-Link, Ubiquiti, Juniper, Arista, MikroTik, Netgear, ASUS, D-Link, Synology, QNAP, VMware, Microsoft, Google, Amazon, Lenovo, Supermicro, Aruba, Fortinet, Palo Alto, WIZnet, Raspberry Pi, Espressif и другие.

## Что нельзя реализовать на Flipper + W5500

- **802.1X** --- нужен полноценный supplicant, не хватит RAM
- **Полный Wireshark-захват на 100 Мбит** --- SPI ограничивает пропускную способность; PCAP-дамп в режиме ETH Bridge записывает трафик, реально проходящий через мост
- **SNMP-запросы** --- ASN.1 парсер слишком тяжёл для RAM
- **TLS/HTTPS** --- нет криптобиблиотек в FAP SDK

## Благодарности

- Основано на [arag0re/fz-eth-troubleshooter](https://github.com/arag0re/fz-eth-troubleshooter) (форк [karasevia/finik_eth](https://github.com/karasevia/finik_eth))
- Использует [WIZnet ioLibrary_Driver](https://github.com/Wiznet/ioLibrary_Driver) для работы с W5500
- Создано для [Flipper Zero OFW](https://github.com/flipperdevices/flipperzero-firmware)

## Лицензия

MIT License. Подробности в файле [LICENSE](LICENSE).
