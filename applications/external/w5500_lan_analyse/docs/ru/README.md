# Flipper Zero LAN Тестер (W5500) -- Документация

Превратите **Flipper Zero + модуль W5500** в профессиональный портативный LAN-тестер. Анализ линков, обнаружение сетевых соседей, сканирование подсетей, фингерпринтинг DHCP, захват пакетов, USB-Ethernet мост, PXE-загрузка, HTTP-менеджер файлов -- всё с карманного устройства.

## Ключевые особенности

- 33 сетевых и security-инструмента в одном приложении Flipper Zero
- Работает с любой SPI-платой на чипе W5500
- DHCP-анализ не занимает IP-адрес (безопасно для продакшн-сетей)
- Результат DHCP кешируется -- нет повторных 15-секундных ожиданий
- Визуальная обратная связь: прогрессбары, таймеры, графики RTT
- Автосохранение результатов с метками времени
- USB-Ethernet мост с опциональной записью PCAP
- Встроенный PXE-сервер и веб-менеджер файлов

## Возможности

| Функция | Категория | Описание |
|---|---|---|
| Auto Test | -- | Автодиагностика: Link → DHCP → Ping GW → DNS → LLDP → ARP |
| Link Info | Port Info | Статус PHY-линка, скорость, дуплекс, MAC, версия W5500 |
| DHCP Analyzer | Port Info | Анализ Discover/Offer с фингерпринтом опций |
| LLDP/CDP | Port Info | Пассивное обнаружение соседей IEEE 802.1AB и Cisco CDP |
| STP/VLAN | Port Info | Захват BPDU и определение 802.1Q VLAN |
| ARP Scanner | Scan | Сканирование подсети с OUI-поиском (~120 вендоров) |
| Ping Sweep | Scan | ICMP-свип CIDR-диапазона с интерактивным списком хостов |
| mDNS/SSDP | Scan | Обнаружение сервисов через multicast DNS и UPnP |
| Port Scanner | Scan | TCP connect: Top-20, Top-100 или свой диапазон (подменю) |
| Ping | Diagnostics | ICMP echo с настраиваемым количеством и таймаутом |
| Continuous Ping | Diagnostics | График RTT с min/max/avg и процентом потерь |
| DNS Lookup | Diagnostics | Разрешение имён через UDP DNS |
| Traceroute | Diagnostics | ICMP-трассировка до 30 хопов |
| Packet Capture | Traffic | Автономный PCAP-дамп на SD-карту |
| ETH Bridge | Traffic | USB-Ethernet мост через CDC-ECM с записью PCAP |
| Statistics | Traffic | Счётчики фреймов по типам и EtherType (10с) |
| SNMP GET | Port Info | Запрос sysName, sysDescr, sysUpTime, ifStatus (v1/v2c) |
| NetBIOS Query | Scan | Имена Windows-машин и рабочие группы |
| NTP Diagnostics | Diagnostics | Stratum, root delay, reference ID, RTT, UTC-время |
| Apply NTP Sync | Diagnostics | Применение NTP-времени к часам Flipper |
| DNS Poison Check | Diagnostics | Сравнение локального и публичного DNS |
| ARP Watch | Security | Обнаружение спуфинга, дубликатов IP, ARP-штормов |
| Rogue DHCP | Security | Обнаружение неавторизованных DHCP-серверов |
| Rogue RA (IPv6) | Security | Обнаружение неавторизованных Router Advertisement |
| DHCP Fingerprint | Security | Определение ОС клиентов по option 55 |
| 802.1X Probe | Security | EAPOL-Start, обнаружение аутентификации на порту |
| VLAN Hopping | Security | Проверка изоляции VLAN (Top 10 / Custom) |
| Wake-on-LAN | Utilities | Отправка magic-пакетов |
| PXE Server | Utilities | Сервер сетевой загрузки (DHCP + TFTP) |
| PXE Download | Utilities | Скачивание iPXE/EFI boot-файлов из интернета на SD |
| File Manager | Utilities | Веб-менеджер файлов SD через HTTP, кастомные CSS/JS темы |
| TFTP Client | Utilities | Скачивание конфигов с оборудования |
| IPMI Query | Utilities | Статус шасси BMC, версия прошивки |
| History | -- | Автосохранение результатов с метками времени |
| Settings | -- | Автосохранение, звук, DNS, пинг, сохранение целей, MAC Changer, About |

## Документация

| Страница | Содержание |
|---|---|
| **[Оборудование и подключение](hardware.md)** | Схема подключения, описание пинов, питание, совместимые платы |
| **[Сборка из исходников](building.md)** | Требования, команды сборки, установка, CI/CD |
| **[Архитектура и внутренности](architecture.md)** | Структура проекта, сокеты, потоки, модель памяти |
| **[Руководство по функциям](usage.md)** | Подробное описание каждой функции по категориям |
| **[ETH Bridge](eth-bridge.md)** | USB-Ethernet мост, запись PCAP, совместимость с ОС |
| **[PXE Server](pxe-server.md)** | Сетевая загрузка, автодетект DHCP, TFTP |
| **[Файловый менеджер](file-manager.md)** | HTTP-сервер, токены авторизации, управление SD |
| **[Безопасность](security.md)** | Модель безопасности, меры защиты, отчёты |
| **[Решение проблем](troubleshooting.md)** | Частые проблемы, ограничения, сторонние библиотеки |

## Благодарности

- Основано на [arag0re/fz-eth-troubleshooter](https://github.com/arag0re/fz-eth-troubleshooter) (форк [karasevia/finik_eth](https://github.com/karasevia/finik_eth))
- Использует [WIZnet ioLibrary_Driver](https://github.com/Wiznet/ioLibrary_Driver)
- Создано для [Flipper Zero OFW](https://github.com/flipperdevices/flipperzero-firmware)

## Лицензия

MIT License. Подробности в файле [LICENSE](../../LICENSE).
