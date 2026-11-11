# UHF Expansion

UHF Expansion turns Flipper Zero into a portable EPC Gen2 inventory reader by
connecting a compatible UHF RFID module through the GPIO UART bridge.

## Features

- Fast real-time EPC inventory with unique-tag and read-rate counters.
- Radar, counter, and paged EPC list views.
- Tag details with EPC, RSSI, protocol control data, and read count.
- Read and write EPC, TID, and User memory on compatible changeable tags.
- Reader temperature and output-power telemetry.
- Reader frequency queries, linked region presets, and custom frequency editing.
- Settings drafts with Save/Discard on exit and a long-press reader restart.
- Optional audible feedback when a tag is detected.
- CSV export to the application's data directory.
- Reader startup recovery and continuous-inventory renewal.

CHN_1* is a software preset for 840–845 MHz sent with the Custom command because
its native channel index table is absent from the reader manual. Custom plans
support 1–255 channels with at least 25 kHz spacing within 840–960 MHz.

## Required Hardware

- MTools Tec UHF Expansion or a compatible UCM601 UHF module.
- UART connection to Flipper Zero GPIO pins 13 and 14.
- Reset connection to GPIO pin 16 on supported expansion revisions.

Use this application only with RFID tags and systems you own or are authorized
to test. Follow local radio, privacy, and data-protection requirements.
