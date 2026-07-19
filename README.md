# Flipper Suite

A collection of 13 external applications (FAPs) for the [Flipper Zero](https://flipperzero.one/) multi-tool. Each app targets a different hardware interface on the Flipper Zero, covering USB HID, NFC, GPIO, Sub-GHz radio, BLE, and WiFi (via ESP32 Dev Board).

> **Note:** This is a work in progress. I'm an independent developer building this alongside AI coding assistants. Not everything has been tested on every firmware variant. Bug reports, pull requests, and contributions are always welcome!

## Applications

### BadUSB Pro (USB)
Advanced USB HID keystroke injection with an extended DuckyScript engine.

- **Extended DuckyScript**: `OS_DETECT` for cross-platform payloads, `IF`/`ELSE`/`END_IF` branching, `WHILE`/`END_WHILE` loops, `FUNCTION`/`END_FUNCTION`/`CALL` subroutines, `LED_CHECK`/`LED_WAIT` feedback channel
- **Consumer Key Support**: `CONSUMER_KEY PLAY_PAUSE`, `CONSUMER_KEY VOL_UP`, etc. (50+ media/system keys, or raw hex like `CONSUMER_KEY 0xCD`)
- **Script Restart**: `RESTART` command to loop scripts from the beginning
- **Call Stack**: Up to 32 levels of nested `CALL`/`END_FUNCTION`

### CCID Emulator (USB)
Programmable USB smartcard (CCID) emulator.

- **Card Profiles**: Load `.ccid` card definition files from SD card with custom APDU response rules
- **APDU Monitor**: Real-time display of command/response pairs from connected readers
- **Log Export**: Press Right in the APDU monitor to export the full session log to `/ext/ccid_emulator/logs/`
- **Sample Cards**: Includes VISA EMV test card, PIV applet, and Java Card profiles

### HID Exfil (USB)
HID-based data exfiltration via keyboard LED feedback channel.

- **7 Payload Types**: System info, WiFi credentials, clipboard contents, browser history, saved passwords, SSH keys, browser bookmarks
- **Cross-Platform**: Separate PowerShell (Windows), Bash (Linux), and osascript (macOS) payload variants
- **Automatic Encoding**: Data is encoded through the keyboard LED status bits feedback channel
- **SD Card Storage**: Exfiltrated data is saved to the Flipper's SD card

### NFC Fuzzer (NFC)
NFC protocol fuzzer for testing reader/tag robustness.

- **11 Fuzzing Profiles**: UID, ATQA/SAK, Frame, NTAG, ISO 15693, Reader Commands, MIFARE Auth, MIFARE Read/Write, RATS/ATS, NFC-B PUPI, FeliCa IDm
- **Multi-Protocol**: Targets ISO 14443-A, ISO 14443-B, ISO 15693, and FeliCa in both listener (tag emulation) and poller (reader) modes
- **4 Fuzz Strategies**: Sequential, random, boundary, and mutation-based approaches
- **Configurable**: Adjustable iteration count and fuzzing parameters

### SPI Flash Dump (GPIO)
Read SPI NOR flash chips via the Flipper's GPIO header.

- **32 Chip Database**: Auto-detection via JEDEC ID for common chips (W25Q, MX25L, AT25, SST25, etc.)
- **Full Read with Verify**: Reads chip contents to SD card, then verifies by re-reading
- **CRC32 Checksum**: Calculates and displays CRC32 after successful read
- **Hex Preview**: Browse dumped data in a hex viewer on-device
- **Adjustable SPI Speed**: Slow (~50 kHz), Medium (~250 kHz), or Fast (~1 MHz)

**Wiring (3.3V logic, directly to Flipper GPIO header):**

| Flash Pin | Flipper Pin |
|-----------|-------------|
| CS        | PA4         |
| CLK       | PB3         |
| MOSI      | PA7         |
| MISO      | PA6         |
| VCC       | 3V3         |
| GND       | GND         |

### UART Sniff (GPIO)
UART protocol sniffer — captures serial data on GPIO pins 13/14.

- **Dual Display Modes**: Hex, ASCII, or side-by-side hex+ASCII views with 100 ms refresh
- **Configurable Baud Rate**: 9600, 19200, 38400, 57600, 115200, or 230400
- **Dual Channel**: USART or LPUART with 4KB ring buffer; scrollable display shows last 256 bytes
- **Passive**: RX-only capture, no transmit — safe for monitoring live buses

### FlipperPwn (USB + GPIO)
Modular pentest payload framework with OS detection, inspired by Metasploit. Supports USB HID keystroke injection and WiFi Dev Board (ESP32 Marauder) via GPIO UART.

- **Module Categories**: Recon, Credentials, Exploit, and Post-Exploit modules loaded from `.fpwn` files on the SD card
- **OS Detection**: Automatic host OS fingerprinting via USB HID LED heuristics (NumLock/ScrollLock probes distinguish Windows, macOS, and Linux)
- **Cross-Platform Payloads**: Each `.fpwn` module can contain platform-specific payload sections (`PLATFORM WIN`, `PLATFORM MAC`, `PLATFORM LINUX`) with DuckyScript-like syntax
- **Template Substitution**: Configurable options via `OPTION` declarations with `{{OPTION_NAME}}` placeholders substituted at runtime
- **WiFi Dev Board Integration**: Connect an ESP32 WiFi Dev Board (running Marauder firmware) to the Flipper's GPIO header for wireless capabilities:
  - WiFi AP scanning with RSSI, channel, and encryption display
  - Network joining (WPA/WPA2/Open)
  - ICMP ping sweep for host discovery
  - Port scanning with service identification
  - Deauth attacks and PMKID sniffing
  - Mixed HID+WiFi payloads via `WIFI_SCAN`, `WIFI_JOIN`, `PING_SCAN`, `PORT_SCAN`, `WIFI_RESULT` module commands
- **21 Built-in Modules**: System info recon, WiFi/network enumeration, AV detection, credential harvesting (WiFi, browser, SSH, environment variables), reverse shells (TCP/DNS), download-and-execute, UAC bypass (fodhelper), MSFvenom stager, persistence (scheduled tasks, startup folder), Defender disablement, user creation, WiFi scan report, evil twin, and port scan report
- **Live Execution View**: Real-time progress display with line count and abort support (press Back to abort)

**Module files**: Copy `flipperpwn_modules/` contents to `/ext/flipperpwn/modules/` on the SD card.

### SubGHz Spectrum (Sub-GHz)
Real-time Sub-GHz spectrum analyzer.

- **4 Frequency Bands**: 315 MHz (310-320), 433 MHz (425-445), 868 MHz (860-880), 915 MHz (900-930)
- **Bar & Waterfall Views**: Toggle between bar graph and waterfall display in Settings
- **Adjustable Step Size**: 10 / 25 / 50 / 100 / 200 kHz
- **Peak Hold**: Optional peak marker overlay
- **CSV Logging**: Scan data exported to `/ext/subghz_spectrum/` with timestamps

### SubGHz Jammer Detector (Sub-GHz)
Detects sustained RF carrier waves indicating Sub-GHz jamming attacks.

- **4 Frequency Bands**: 315 MHz, 433.92 MHz, 868.35 MHz, 915 MHz monitored via CC1101 radio
- **Real-Time RSSI**: Threat classification (OK / Suspicious / Jammer) with visual bar graphs and ~800 ms scan cycle
- **Configurable Alerts**: Silent, blink, or vibrate modes with adjustable detection thresholds

### BLE Scanner (Bluetooth)
BLE advertisement scanner via ESP32 Dev Board — detects AirTags, skimmers, and rogue beacons.

- **Device Discovery**: Real-time BLE enumeration via ESP32 Marauder with RSSI, MAC, and name display (500 ms refresh)
- **AirTag Detection**: Apple AirTag identification via name matching and OUI lookup
- **Sorting & Filtering**: Sort by signal strength, recency, or MAC; configurable RSSI filter; up to 64 devices per session
- **SD Card Logging**: Export discovered devices to TSV files on the Flipper's SD card

### Evil BLE (Bluetooth)
BLE advertisement cloning — scans with ESP32, re-broadcasts as clone via Flipper BLE.

- **Clone Attacks**: Scan for BLE devices via ESP32 Marauder and clone selected advertisements using Flipper's extra_beacon hardware
- **MAC Spoofing**: Spoofs target device MAC address and name to test BLE identity validation
- **Proximity Relay**: Test smart locks and BLE-authenticated systems for relay vulnerabilities

### Rogue AP Detector (WiFi)
Detects evil twin / rogue WiFi access points via ESP32 Dev Board.

- **Evil Twin Detection**: Monitors for duplicate SSIDs from different MAC addresses with RSSI anomaly analysis (>20 dBm delta)
- **Three Threat Levels**: CLEAN, SUSPECT (same SSID from 2+ BSSIDs), and EVIL TWIN (with red LED + vibration alerts)
- **Live AP Table**: Up to 128 entries with 30-second stale pruning; detailed results view with BSSID, RSSI, and channel per SSID
- **RSSI Filter**: Configurable minimum RSSI threshold to focus on nearby APs

### Rayhunter Client (WiFi)
IMSI catcher detection dashboard — displays EFF Rayhunter status via ESP32 WiFi bridge.

- **Cellular Threat Monitoring**: Connects to EFF Rayhunter on an Orbic RC400L hotspot via ESP32 WiFi bridge
- **Threat Indicators**: Monitors for null cipher negotiation, suspicious identity requests, and unusual tower behavior
- **Four Threat Levels**: CLEAN, LOW, MEDIUM, HIGH with configurable polling interval (2s–60s) and host/port settings

## Quick Install (Pre-built)

If you just want to install the apps without building from source, pre-built `.fap` files are available in the **[`dist/`](dist/)** folder of this repository.

1. Download the `.fap` files from the `dist/` folder (or clone the repo)
2. Connect your Flipper Zero via USB and open the SD card
3. Copy each `.fap` file to the appropriate folder on the SD card:
   - `badusb_pro.fap`, `ccid_emulator.fap`, `hid_exfil.fap` &rarr; `/ext/apps/USB/`
   - `flipperpwn.fap` &rarr; `/ext/apps/Tools/`
   - `nfc_fuzzer.fap` &rarr; `/ext/apps/NFC/`
   - `spi_flash_dump.fap`, `uart_sniff.fap` &rarr; `/ext/apps/GPIO/`
   - `subghz_spectrum.fap`, `subghz_jammer.fap` &rarr; `/ext/apps/Sub-GHz/`
   - `ble_scanner.fap`, `evil_ble.fap` &rarr; `/ext/apps/Bluetooth/`
   - `rogue_ap_detector.fap`, `rayhunter_client.fap` &rarr; `/ext/apps/WiFi/`
4. Eject the SD card and the apps will appear in the Flipper's menu

> **Note:** The pre-built FAPs target firmware API 87.1 (official firmware 1.4.x). If you see an "API mismatch" error, rebuild from source using the instructions below.

## Building from Source

### Prerequisites

- A [Flipper Zero](https://flipperzero.one/) running official firmware
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool) installed on your computer
- Python 3.8+ (required by ufbt)

### Install ufbt

```bash
pip install ufbt
```

Or if you prefer pipx:

```bash
pipx install ufbt
```

### Build All Apps

Clone this repository and build each application:

```bash
git clone https://github.com/barkandbite/flipper_suite.git
cd flipper_suite

# Build each app individually
for app in badusb_pro ble_scanner ccid_emulator evil_ble flipperpwn hid_exfil nfc_fuzzer rayhunter_client rogue_ap_detector spi_flash_dump subghz_jammer subghz_spectrum uart_sniff; do
    cd "$app"
    ufbt
    cd ..
done
```

Each app builds to `<app_name>/dist/<app_name>.fap`.

### Install to Flipper Zero

Connect your Flipper Zero via USB, then deploy each app:

```bash
cd badusb_pro
ufbt launch     # Builds, installs, and launches on Flipper
cd ..
```

Or manually copy the `.fap` files:

1. Build the app with `ufbt` (inside the app directory)
2. Copy `dist/<app_name>.fap` to your Flipper's SD card under `/ext/apps/<category>/`
   - BadUSB Pro, CCID Emulator, HID Exfil: `/ext/apps/USB/`
   - FlipperPwn: `/ext/apps/Tools/`
   - NFC Fuzzer: `/ext/apps/NFC/`
   - SPI Flash Dump, UART Sniff: `/ext/apps/GPIO/`
   - SubGHz Spectrum, SubGHz Jammer: `/ext/apps/Sub-GHz/`
   - BLE Scanner, Evil BLE: `/ext/apps/Bluetooth/`
   - Rogue AP Detector, Rayhunter Client: `/ext/apps/WiFi/`

### Sample Files

Copy the included sample files to your Flipper's SD card:

- `badusb_pro_sample_scripts/*.ds` &rarr; `/ext/badusb_pro/` on SD card
- `ccid_emulator_sample_cards/*.ccid` &rarr; `/ext/ccid_emulator/cards/` on SD card
- `flipperpwn_modules/**/*.fpwn` &rarr; `/ext/flipperpwn/modules/` on SD card (preserve subdirectory structure)

## FAQ

**Q: What firmware version do I need?**
A: These apps are built against the official Flipper Zero firmware SDK. They were developed and tested with API version 87.1 (firmware 1.4.x). If you're on a significantly older or newer firmware, rebuild from source with `ufbt` to match your version.

**Q: Do these work with custom firmware (Momentum, Unleashed, etc.)?**
A: They may work if the custom firmware maintains API compatibility with the official SDK. Rebuild from source with `ufbt` pointed at your firmware's SDK for best results. No guarantees are made for third-party firmware.

**Q: How do I update ufbt's SDK version?**
A: Run `ufbt update` to pull the latest SDK matching your Flipper's firmware. If you need a specific version: `ufbt update --channel=release`.

**Q: The app crashes or shows "API mismatch" on my Flipper.**
A: This means the `.fap` was built against a different firmware version than what's running on your Flipper. Rebuild from source: `cd <app_directory> && ufbt`.

**Q: Can I use BadUSB Pro over Bluetooth?**
A: BLE HID is not currently available in the official firmware SDK for external FAP applications. The app will automatically fall back to USB mode. This may change in future firmware releases.

**Q: Where does SPI Flash Dump save files?**
A: Dumps are saved to `/ext/spi_dumps/` on the SD card, named by the detected chip and timestamp.

**Q: Can I add my own CCID card profiles?**
A: Yes. Create a `.ccid` file following the format in the sample cards and place it in `/ext/ccid_emulator/cards/` on your SD card. The format uses `[Card]` headers with `AID`, `RULE`, and `DEFAULT_RESPONSE` directives.

**Q: How do I add custom FlipperPwn modules?**
A: Create a `.fpwn` text file with `NAME`, `DESCRIPTION`, `CATEGORY`, and `PLATFORMS` headers, then add `OPTION` declarations and `PLATFORM WIN`/`PLATFORM MAC`/`PLATFORM LINUX` sections with DuckyScript-like commands. Place the file in `/ext/flipperpwn/modules/` on the SD card. See the included modules in `flipperpwn_modules/` for examples.

**Q: Does FlipperPwn require admin/root on the target?**
A: Most modules operate at user privilege level. Modules like UAC bypass use techniques (e.g., fodhelper.exe) that escalate without a UAC prompt on default Windows settings. Modules that require elevated access are noted in their descriptions.

**Q: What SPI flash chips are supported?**
A: The app includes a database of 32 common SPI NOR flash chips and will auto-detect via JEDEC ID. If your chip isn't recognized, the app will display the raw JEDEC ID so you can verify compatibility manually.

## Project Structure

```
flipper_suite/
├── .github/workflows/build.yml  # CI: builds and lints all 13 FAPs
├── dist/                        # Pre-built .fap files (ready to install)
├── badusb_pro/                  # BadUSB Pro application
├── ble_scanner/                 # BLE Scanner application
├── ccid_emulator/               # CCID Emulator application
├── evil_ble/                    # Evil BLE clone application
├── flipperpwn/                  # FlipperPwn pentest framework
├── hid_exfil/                   # HID Exfil application
├── nfc_fuzzer/                  # NFC Fuzzer application
├── rayhunter_client/            # Rayhunter IMSI catcher dashboard
├── rogue_ap_detector/           # Rogue AP / Evil Twin detector
├── spi_flash_dump/              # SPI Flash Dump application
├── subghz_jammer/               # SubGHz Jammer Detector
├── subghz_spectrum/             # SubGHz Spectrum Analyzer
├── uart_sniff/                  # UART Sniffer
├── evil_portal/                 # ESP32 Marauder captive portal templates (not a FAP)
├── badusb_pro_sample_scripts/   # Sample DuckyScript files
├── ccid_emulator_sample_cards/  # Sample CCID card profiles
├── flipperpwn_modules/          # FlipperPwn payload modules (.fpwn)
├── build_all.sh                 # Build all 13 FAPs locally
├── check_dist.sh                # Verify dist/ .fap files are current
├── CHANGELOG.md                 # Project changelog
└── TODO.md                      # Maintenance tracker
```

Each application directory contains:
- `application.fam` &mdash; Build manifest (app name, category, entry point)
- `*.c` / `*.h` &mdash; Source code
- `dist/` &mdash; Build output (`.fap` file)

## Legal Disclaimer

**This software is provided for educational, entertainment, and research purposes only.**

The authors and contributors of Flipper Suite are **not responsible** for any actions taken by users of this software. By using these applications, you agree to the following:

- You will only use these tools on devices and systems that you own or have explicit written authorization to test.
- You are solely responsible for ensuring that your use of these tools complies with all applicable local, state, federal, and international laws and regulations.
- These tools are intended for legitimate security research, learning, and personal tinkering. Any misuse is entirely the responsibility of the user.
- The authors make no warranties, express or implied, regarding the functionality, safety, or legality of use in any specific jurisdiction.
- The Flipper Zero is a legal multi-tool in most jurisdictions. However, the legality of specific use cases varies by location. **Know your local laws.**

**No warranty.** This software is provided "as is" without warranty of any kind. Use at your own risk.

## Contributing

Contributions are welcome. Please open an issue or pull request on [GitHub](https://github.com/barkandbite/flipper_suite).

When contributing code, please ensure it builds cleanly with `ufbt` and passes `ufbt lint`.

## License

This project is licensed under the [MIT License](LICENSE). All code in this repository is provided for educational and research purposes.
