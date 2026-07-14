# Wendigo Protocol

In order to develop Wendigo, which renders data gathered by an ESP32 in a native Flipper Zero interface, the ESP32 firmware packages data into binary packets and sends them to Flipper Zero over a serial (UART) interface. This document describes the structure of the packets Wendigo uses for communication between ESP32-Wendigo and Flipper-Wendigo.

The protocol is being documented in the hope others will find it useful, and maybe even extend the protocol and develop additional applications using it.

## Flipper-to-ESP32 Communication

For simplicity, and to preserve the ability to use Wendigo without a Flipper Zero, ESP32-Wendigo receives commands in plain text. The commands supported by ESP32-Wendigo are described [in the commands reference](https://github.com/chris-bc/wendigo/blob/bug-buffer-and-device-view/docs/ESP32-Wendigo-Commands.md).

## ESP32-to-Flipper Communication

All communication from the ESP32 is in the form of packets that follow a predictable structure, allowing them to be parsed by Flipper-Wendigo. Packets start with a four-byte preamble that specifies the type of packet (i.e. access point, MAC address, etc.) and end with a four-byte packet terminator.

### Bluetooth device

* Preamble: 0xFF, 0xFE, 0xFD, 0xFC (4 bytes)
* Device name length (1 byte, uint8)
* EIR length (1 byte, uint8)
* RSSI (2 bytes, int16)
* Class of Device (4 bytes, uint32)
* Bluetooth Device Address (MAC) (6 bytes)
* Device type: 0: Bluetooth Classic, 1: BLE (1 byte, uint8)
* Tagged: 0: Not tagged, 1: Tagged (1 byte, uint8)
* Last Seen (struct timeval) (19 bytes)
* Number of services (1 byte, uint8)
* Known services length (1 byte, uint8)
* Class of device string description length (1 byte, uint8)
* Device name (Length specified at beginning of packet)
* EIR (Length specified at beginning of packet)
* Class of Device (Length specified earlier in packet)
* Packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)

### WiFi Access Point

* Preamble: 0x99, 0x98, 0x97, 0x96 (4 bytes)
* Device type: 2 (1 byte, uint8)
* MAC (6 bytes)
* Channel (1 byte, uint8)
* RSSI (2 bytes, int16)
* Last seen (struct timeval) (19 bytes)
* Taggod: 0: Not tagged, 1: Tagged (1 byte, uint8)
* Authentication mode (1 byte, uint8)
* SSID Length (1 byte, uint8)
* Station count (1 byte, uint8)
* SSID (Length specified above)
* For each connected station:
  * MAC (6 bytes)
* Packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)

### WiFi Station

* Preamble: 0x99, 0x98, 0x97, 0x96 (4 bytes)
* Device type: 2 (1 byte, uint8)
* MAC (6 bytes)
* Channel (1 byte, uint8)
* RSSI (2 bytes, int16)
* Last seen (struct timeval) (19 bytes)
* Taggod: 0: Not tagged, 1: Tagged (1 byte, uint8)
* Preferred Network List (saved networks) count (1 byte, uint8)
* Access Point MAC (6 bytes)
* Access Point SSID length (1 byte, uint8)
* Access Point SSID (length defined above)
* For each preferred/saved network:
  * SSID length (1 byte, uint8)
  * SSID (length defined above)
* Packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)

### MAC Addresses

* Preamble: 0x55, 0x54, 0x53, 0x52 (4 bytes)
* Count of interfaces/MAC addresses (1 byte, uint8)
* For each interface/MAC:
  * MAC type: 0: Base MAC, 1: WiFi AP MAC, 2: Bluetooth Device Address (1 byte, uint8)
  * MAC/BDA (6 bytes)
* Packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)

### Enabled WiFi Channels

* Preamble: 0x77, 0x76, 0x75, 0x74 (4 bytes)
* Channel count (1 byte, uint8)
* Channels (1 byte uint8 per channel)
* Packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)

### Status

* Preamble: 0x66, 0x65, 0x64, 0x63 (4 bytes)
* Attribute count (1 byte, uint8)
* For each attribute
  * Attribute name length (1 byte, uint8)
  * Attribute name (length as above)
  * Attribute value length (1 byte, uint8)
  * Attribute value (length as above)
* Packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)

### Version

* Preamble: 0x57, 0x65, 0x6E, 0x64 (ASCII "Wend", 4 bytes)
* This is followed by the remainder of the version string, for example "igo v0.5.0"
* Followed by the packet terminator: 0xAA, 0xBB, 0xCC, 0xDD (4 bytes)
