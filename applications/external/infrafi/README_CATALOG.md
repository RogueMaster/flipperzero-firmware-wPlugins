# InfraFi

Transmit WiFi credentials from a **Flipper Zero** to a **Linux server** using infrared. Point, press Send, connected.

Built for headless servers (NAS boxes, Intel NUCs, etc.) where typing WiFi passwords is painful or impossible.

## How It Works

The Flipper encodes WiFi credentials as a sequence of **RC-6 IR messages** and blasts them at the server's CIR (Consumer IR) receiver. The daemon decodes the transmission and connects to the network automatically. No pairing, no Bluetooth, no network required — just line-of-sight IR.

With **ACK enabled**, the daemon transmits a response back via IR — the Flipper displays whether the connection succeeded (with IP address) or failed.

## Features

- **Manual entry** — On-screen keyboard for SSID and password, security type selector (Open/WPA/WEP/SAE)
- **NFC WiFi tags** — Scan an NTAG213/215/216 tag with WiFi credentials and transmit instantly
- **Saved networks** — Credentials auto-save to SD card after transmit. Browse, resend, or delete
- **Fast transmission** — Full credentials sent in under a second via RC-6 protocol
- **Hidden network support** — Toggle hidden SSID flag
- **ACK feedback** — Optional. When enabled in Settings, the Flipper waits for a response from the server. Shows "Connected! IP: x.x.x.x" or "Failed" on screen

## Requirements

- A Linux server with an IR receiver (tested with ITE8708 CIR in Intel NUCs)
- The **infrafid** daemon running on the server — see [GitHub](https://github.com/amd989/InfraFi) for install instructions
- Pre-built .deb and .rpm packages available from [GitHub Releases](https://github.com/amd989/InfraFi/releases)

## Usage

**Manual Entry:**
1. Open **InfraFi** on your Flipper
2. Select **Send Credentials**
3. Enter SSID, password, and security type
4. Review on the confirm screen, press **Send**
5. Point the Flipper at the server's IR receiver

**NFC Tag:**
1. Write WiFi credentials to an NFC tag using a phone app (e.g., **NFC Tools**)
2. Open **InfraFi** — **Scan NFC Tag**
3. Hold the tag to the back of the Flipper
4. Review credentials, press **Send**

**Saved Networks:**
- Previously transmitted networks are auto-saved to the SD card
- Open **InfraFi** — **Saved** to browse and resend

## Author

**Alejandro Mora** — [github.com/amd989](https://github.com/amd989)
