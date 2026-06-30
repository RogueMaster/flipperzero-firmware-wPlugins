# GhostESP Flipper Zero App

A Flipper Zero application for interfacing with the [GhostESP: Revival firmware](https://github.com/jaylikesbunda/Ghost_ESP).

## Preview

<img width="512" height="256" alt="mainmenu" src="https://github.com/user-attachments/assets/84d4cd50-b981-4d42-bee4-7d1cda8fb387" />

## Download Latest Release [Here](https://github.com/jaylikesbunda/ghost_esp_app/releases/latest)

## Features

### WiFi Operations

- **Scanning & Probing**
  - Scan and list WiFi APs and stations
  - Select targets by AP or station number
  - Listen for probe requests with channel hopping or fixed channel
  - Pineapple detection
  - Channel congestion analysis
  - Port scanning on local network or specific IP
  - ARP scanning on local network
  - SSH scanning on target IP

- **Packet Capture**
  - Variable sniffing modes (WPS, Raw, Probe, Deauth, Beacon, EAPOL, Pwn)
  - Cycling menu to switch between capture types
  - PCAP file export

- **Beacon Spam & Attacks**
  - Beacon spam modes: List, Random, Rickroll, Custom SSID
  - Deauthentication attacks
  - EAPOL logoff attacks
  - SAE handshake flood attacks (WPA3)
  - Karma rogue AP (with custom SSID support)
  - DHCP starvation attacks
  - Beacon list management (add, remove, clear, show, spam)

- **Evil Portal & Network**
  - Create captive portal
  - List available portal HTML files
  - Set/clear custom evil portal HTML
  - Connect to WiFi networks
  - Disconnect from WiFi
  - Scan local network for devices
  - Cast random videos to Cast/DIAL devices
  - Control network printers
  - TP-Link smart plug control
  - WebUI credentials configuration

### Bluetooth (BLE) Operations

- **Scanning & Detection**
  - Skimmer detection
  - Flipper discovery
  - AirTag scanning and listing
  - BLE spam detection
  - View all BLE traffic
  - Track Flipper RSSI strength

- **Attacks & Spoofing**
  - BLE spam modes: Apple, Microsoft, Samsung, Google, Random
  - Spoof selected AirTag
  - AirTag and Flipper device selection

- **Packet Capture**
  - Raw BLE packet capture
  - PCAP file export

### GPS

- **GPS Information**
  - Real-time position (Latitude/Longitude)
  - Altitude and speed monitoring
  - Direction and signal quality
  - Satellite status

- **Wardriving Capabilities**
  - WiFi wardriving with GPS logging (CSV export)
  - BLE wardriving with GPS logging (CSV export)
  - Combined mapping of networks and devices

### Configuration & System

- **LED Control**
  - LED effects: Rainbow, Police, Strobe, fixed colors (Red, Green, Blue, Yellow, Purple, Cyan, Orange, White, Pink)
  - LED off mode
  - Set custom RGB pins
  - NeoPixel brightness control (0-100%)

- **SD Card Configuration**
  - Show current SD pin configuration
  - Set SD pins for MMC mode
  - Set SD pins for SPI mode
  - Save configuration to SD card

- **System Settings**
  - Timezone configuration
  - WiFi country code setting
  - Web authentication (enable/disable)
  - Access Point enable/disable across reboots
  - RGB profile selection (normal, rainbow, stealth)

- **Device Management**
  - Show chip info and memory details
  - Settings management (list, get, set, reset)
  - Device reboot
  - Help documentation

---

## Credits

- Made by Spooky (<https://github.com/Spooks4576>)
- Additional contributions by Jay Candel (<https://github.com/jaylikesbunda>) and @tototo31 (<https://github.com/tototo31>)

## Support

- For support, please open an issue (<https://github.com/jaylikesbunda/ghost_esp_app/issues>) on the repository or contact Jay (<https://github.com/jaylikesbunda>) (@fuckyoudeki on Discord - <https://discord.gg/5cyNmUMgwh>).
