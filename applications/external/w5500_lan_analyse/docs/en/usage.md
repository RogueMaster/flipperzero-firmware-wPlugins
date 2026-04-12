[← Back to documentation index](README.md)

# Feature Guide

## Getting Started

1. Connect the W5500 module to Flipper Zero (see [Hardware Setup](hardware.md))
2. Plug an Ethernet cable into the W5500's RJ45 port
3. Open **GPIO → LAN Tester** on the Flipper
4. The menu header shows link status: `LAN [UP 100M FD]` or `LAN [DOWN]`
5. Select a category and then a feature

![Main menu](../screenshots/main_menu.png)

The menu is organized into six categories: **Port Info**, **Scan**, **Diagnostics**, **Traffic**, **Utilities**, and **Settings**. An **Auto Test** entry sits at the very top of the menu for quick cable-in diagnostics. Most features that need an IP address will run DHCP automatically on first use, then cache the result for all subsequent operations.

### Navigation

- **Up/Down**: scroll through menu items
- **OK**: select an item or confirm
- **Back**: return to parent menu or cancel a running operation
- **Left/Right**: cycle through options (in settings and PXE boot file selection)

---

## Auto Test

Auto Test is the first item in the main menu. It provides a fully automatic network diagnostic sequence that runs as soon as an Ethernet cable is plugged in -- no manual navigation required.

### Test Sequence

When a cable is detected, Auto Test executes the following steps in order:

1. **Link Info** -- read link speed and duplex
2. **DHCP** -- obtain IP configuration (Discover/Offer, no lease consumed)
3. **Ping Gateway** -- ICMP ping to the DHCP gateway
4. **DNS Resolve** -- resolve a configurable hostname (default: `google.com`)
5. **Internet Ping** -- ICMP ping to the resolved IP
6. **LLDP/CDP** -- passive listen for switch discovery frames (runs in parallel with the next step)
7. **ARP Scan** -- scan the local subnet for active hosts (optional, configurable)

### Verdict

After all steps complete, the screen shows one of two results:

- **`[Auto Test] OK`** -- all mandatory steps passed
- **`[Auto Test]`** followed by the name of the first step that failed

### Auto-Cycle Behavior

Auto Test continuously cycles through cable states:

1. Cable removed -- screen shows "Waiting for cable..."
2. Cable plugged in -- test sequence runs automatically
3. Cable removed again -- returns to waiting state

This makes it ideal for quickly testing multiple cable drops or switch ports in succession.

### Auto Test Settings

Three settings in **Settings** control Auto Test behavior:

| Setting | Values | Default | Description |
|---------|--------|---------|-------------|
| AT DNS host | hostname | `google.com` | Hostname resolved during the DNS step |
| AT LLDP wait | 10 / 20 / 30 / 60 s | 30 | How long to listen for LLDP/CDP frames |
| AT ARP scan | ON / OFF | ON | Whether to include the ARP scan step |

---

## Port Info

### Link Info

Shows the current state of the physical Ethernet connection.

**Output:**
- **Link**: UP or DOWN
- **Speed**: 10 Mbps or 100 Mbps
- **Duplex**: Half or Full
- **MAC Address**: the W5500's current MAC (persisted on SD card)
- **W5500 Version**: chip version register (should be `0x04`)

**Use first** after connecting hardware to verify the W5500 is detected and the cable is connected.

### DHCP Analyze

Sends a DHCP Discover and parses the Offer response **without accepting the lease**. This is completely safe for production networks -- no IP address is consumed.

**Output:**
- Offered IP address
- DHCP server IP
- Subnet mask
- Gateway
- DNS server(s)
- NTP server (if provided)
- Domain name (if provided)
- Lease time
- DHCP option fingerprint (list of option numbers)

**Timeout**: 15 seconds. If no Offer is received, shows "DHCP: No response".

The DHCP result is cached and reused by all other features that need network configuration.

### LLDP/CDP

Passive listener for switch neighbor discovery protocols.

**LLDP** (Link Layer Discovery Protocol, IEEE 802.1AB): used by most managed switches to advertise their identity. **CDP** (Cisco Discovery Protocol): Cisco-proprietary equivalent.

**How it works:**
1. Opens MACRAW socket in promiscuous mode
2. Listens for up to 60 seconds (countdown shown on screen)
3. Parses received LLDP/CDP frames

**LLDP output** (TLV types 0-8, 127):
- System name, description
- Port ID, port description
- Management IP address
- System capabilities (bridge, router, etc.)
- VLAN name and ID (802.1Q TLV)

**CDP output**:
- Device ID, platform
- Port ID
- Management IP
- Software version
- Native VLAN

![LLDP neighbor result](../screenshots/lldp_result.png)

Press **Back** to stop listening early.

### STP/VLAN

Passive listener for Spanning Tree Protocol and VLAN tags.

**STP/RSTP/MSTP**: listens for BPDU (Bridge Protocol Data Unit) frames for 30 seconds. Shows:
- Root bridge ID and priority
- Bridge ID of the sending switch
- Port role and state
- Path cost
- Protocol version (STP/RSTP/MSTP)

**802.1Q VLAN**: detects VLAN tags on any Ethernet frame passing through. Shows VLAN ID and priority.

---

## Scan

### ARP Scan

Scans the local subnet to discover active hosts.

**How it works:**
1. Runs DHCP first (if not cached) to determine subnet range
2. Sends ARP requests in batches (16 requests per 15 ms)
3. Waits for replies with a tail timeout for late responders
4. Shows each discovered host: IP, MAC (last 3 bytes), vendor name (OUI lookup)

**Output example:**
```
[ARP] 192.168.1.0/24 (254 hosts)
192.168.1.1   AA:BB:CC  Cisco
192.168.1.10  DD:EE:FF  Intel
192.168.1.42  11:22:33  Apple
Found: 3 hosts
```

Discovered hosts are saved to the interactive host list -- see [Interactive Host List](#interactive-host-list) below.

### Ping Sweep

ICMP sweep of an entire CIDR range.

**Input**: CIDR notation (e.g. `192.168.1.0/24`). Auto-detected from DHCP or enter manually via IP keyboard.

**How it works:**
1. Sends one ICMP Echo Request to each IP in the range
2. Waits for replies with configurable timeout
3. Shows progress bar during scan
4. Lists all responding hosts

Discovered hosts feed into the interactive host list.

### mDNS/SSDP Discovery

Discovers services and devices on the local network using two multicast protocols.

**mDNS**: sends a query for `_services._dns-sd._udp.local` to 224.0.0.251:5353. Discovers printers, AirPlay devices, Home Assistant, etc.

**SSDP**: sends an M-SEARCH request to 239.255.255.250:1900. Discovers UPnP devices, media servers, smart home hubs.

Collects responses for approximately 10 seconds.

### Port Scan

TCP connect scan to discover open ports on a target.

**Input**: target IP (default: DHCP gateway).

**Scan modes** (submenu):
- **Top 20**: 18 most common ports (SSH, HTTP, HTTPS, SMB, RDP, etc.) -- fast scan
- **Top 100**: 100 common ports -- comprehensive scan
- **Custom**: any range from 1 to 65535

**How it works:**
1. Attempts TCP connect to each port
2. Short timeout per port (~2-3 seconds)
3. Shows progress bar
4. Lists open ports with service names

---

## Diagnostics

### Ping

Sends ICMP Echo Requests to a target IP.

**Default target**: DHCP gateway (pre-populated in IP input).

**Configurable** (in Settings):
- **Count**: 1-100 packets (default: 4)
- **Timeout**: 500-10000 ms per packet (default: 3000 ms)

**Output**: per-packet RTT and summary (sent/received/lost, min/avg/max RTT).

### Continuous Ping

Real-time ping with a live RTT graph.

**Input**: target IP (default: DHCP gateway).

**Display:**
- 128-pixel wide RTT graph scrolling right-to-left
- Current RTT value
- Average RTT
- Packet loss percentage
- Min/max RTT

**Configurable** (in Settings):
- **Interval**: 200-5000 ms between pings (default: 1000 ms)

Runs continuously until **Back** is pressed.

### DNS Lookup

Resolves a hostname to an IP address via UDP DNS.

**Input**: hostname (e.g. `google.com`)

**DNS server**: from DHCP, or custom if configured in Settings.

**Output**: resolved IPv4 address, or error message.

### Traceroute

ICMP-based hop-by-hop path discovery.

**Input**: IP address or hostname (hostnames are resolved via DNS first).

**How it works:**
1. Sends ICMP Echo Requests with incrementing TTL (starting from 1)
2. Each router along the path responds with ICMP Time Exceeded
3. Records each hop's IP and round-trip time
4. Stops at TTL 30 or when the destination responds

**Output example:**
```
[Traceroute] 8.8.8.8
 1  192.168.1.1      2 ms
 2  10.0.0.1         8 ms
 3  *                timeout
 4  8.8.8.8          15 ms
Done: 4 hops
```

---

## Traffic

### Packet Capture

Standalone PCAP traffic capture without the ETH Bridge.

Opens the MACRAW socket in promiscuous mode and writes all received Ethernet frames to a `.pcap` file on the SD card. Files are saved to `apps_data/lan_tester/pcap/` with timestamped filenames.

The `.pcap` files are compatible with Wireshark and tcpdump.

Press **OK** to start/stop recording. Press **Back** to exit.

### ETH Bridge

USB-to-Ethernet bridge with optional PCAP recording. See [ETH Bridge](eth-bridge.md) for details.

### Statistics

Captures all Ethernet frames for 10 seconds and shows a breakdown.

**Output:**
- Total frame count
- By destination: unicast, broadcast, multicast
- By EtherType: IPv4, ARP, IPv6, LLDP, CDP, unknown

Uses MACRAW socket in promiscuous mode, so it sees all traffic on the wire, not just traffic addressed to the Flipper.

---

## Utilities

### Wake-on-LAN

Sends a WoL magic packet to wake a device on the network.

**Input**: target MAC address (via byte input).

The magic packet is sent as a broadcast UDP packet on port 9. The target machine must have WoL enabled in its BIOS/UEFI and network adapter settings.

### Other Utilities

- **[PXE Server](pxe-server.md)** -- network boot server with DHCP + TFTP
- **[File Manager](file-manager.md)** -- web-based SD card management via HTTP

---

## Interactive Host List

When ARP Scan or Ping Sweep discovers hosts, they are saved to an interactive list. Select any host to perform actions:

![Discovered hosts list](../screenshots/discovered_hosts.png)

- **Ping** -- quick 4-ping test to that host
- **Continuous Ping** -- live RTT graph to that host
- **Traceroute** -- path discovery to that host
- **Port Scan** -- scan that host's ports
- **Wake-on-LAN** -- send magic packet to that host's MAC (if known)

Up to 64 hosts can be stored in the list.

---

## Settings

Access via the main menu → **Settings**. The **About** screen (app version, author, links) is also located inside Settings.

| Setting | Values | Default | Description |
|---------|--------|---------|-------------|
| Auto-save results | ON / OFF | ON | Automatically save scan results to history |
| Sound & vibro | ON / OFF | ON | LED blink and vibration on scan completion/error |
| Custom DNS | ON / OFF | OFF | Use custom DNS server instead of DHCP-provided |
| DNS Server IP | IP address | 8.8.8.8 | Custom DNS server (only when Custom DNS is ON) |
| Ping Count | 1-100 | 4 | Number of ICMP packets for Ping |
| Ping Timeout | 500-10000 ms | 3000 | Per-packet reply timeout |
| Cont. Ping Interval | 200-5000 ms | 1000 | Interval between pings in Continuous Ping |
| AT DNS host | hostname | `google.com` | Hostname resolved during Auto Test DNS step |
| AT LLDP wait | 10 / 20 / 30 / 60 s | 30 | How long Auto Test listens for LLDP/CDP frames |
| AT ARP scan | ON / OFF | ON | Whether Auto Test includes the ARP scan step |
| Clear History | action | -- | Delete all saved result files |
| MAC Changer | action | -- | Generate random MAC or enter custom; saved to SD |

### MAC Changer

The app generates a unique random locally-administered MAC address on first boot, saved to `mac.conf` on the SD card. MAC Changer lets you:

- **Randomize**: generate a new random MAC
- **Custom**: enter any MAC address via byte input

The new MAC is applied immediately and persisted across app restarts. It is also used by the ETH Bridge and all network operations.
