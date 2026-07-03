[← Back to documentation index](README.md)

# Architecture & Internals

## Project Structure

```
├── application.fam              # FAP manifest (appid, entry point, libs)
├── lan_tester_app.c             # Entry point, ViewDispatcher, all feature logic
├── lan_tester_app.h             # Shared types, enums, app state struct
│
├── hal/
│   ├── w5500_hal.c              # SPI init, GPIO setup, MACRAW socket management
│   └── w5500_hal.h              # HAL API: init, deinit, send, recv, link status
│
├── usb_eth/
│   ├── usb_eth.c / .h           # USB CDC-ECM network device (init/deinit/send/recv)
│   └── usb_descriptors.c / .h   # USB device & config descriptors, endpoint callbacks
│
├── bridge/
│   ├── eth_bridge.c             # Bidirectional L2 frame forwarding engine
│   ├── eth_bridge.h
│   ├── pcap_dump.c              # PCAP file writer (global header + per-packet records)
│   └── pcap_dump.h
│
├── protocols/
│   ├── lldp.c / lldp.h         # IEEE 802.1AB LLDP TLV parser (types 0-8, 127)
│   ├── cdp.c / cdp.h           # Cisco CDP parser with LLC/SNAP detection
│   ├── arp_scan.c / arp_scan.h  # ARP request builder, reply parser, batch scanning
│   ├── dhcp_discover.c / .h     # DHCP Discover builder, Offer parser, option fingerprint
│   ├── icmp.c / icmp.h         # ICMP Echo (ping) via IPRAW socket
│   ├── dns_lookup.c / .h       # DNS A-record resolver via UDP
│   ├── wol.c / .h              # Wake-on-LAN magic packet (broadcast UDP port 9)
│   ├── port_scan.c / .h        # TCP connect port scanner with presets
│   ├── traceroute.c / .h       # ICMP traceroute with incrementing TTL
│   ├── ping_graph.c / .h       # 128-sample ring buffer for continuous ping RTT graph
│   ├── discovery.c / .h        # mDNS + SSDP service discovery
│   ├── stp_vlan.c / .h         # STP BPDU parser + 802.1Q VLAN tag extraction
│   ├── mac_changer.c / .h      # MAC randomizer/setter with SD persistence
│   ├── pxe_server.c / .h       # PXE boot server: DHCP responder + TFTP server
│   ├── file_manager.c / .h     # HTTP server for SD card file management
│   └── history.c / .h          # Timestamped result storage on SD card
│
├── utils/
│   ├── oui_lookup.c / .h       # MAC prefix → vendor name (~120 OUI entries)
│   └── packet_utils.c / .h     # Big-endian read/write, IP checksum, formatters
│
├── ip_keyboard.c / .h          # Custom IP address input keyboard widget
├── assets/icon.png              # 10x10 FAP application icon
│
└── lib/
    └── ioLibrary_Driver/        # WIZnet W5500 driver (vendored, do not modify)
```

## W5500 Socket Allocation

The W5500 chip provides 8 hardware sockets. The app uses 6 of them with tuned buffer sizes:

| Socket | RX/TX Buffer | Usage |
|--------|-------------|-------|
| 0 | 8 KB / 8 KB | MACRAW -- promiscuous frame capture (LLDP, CDP, STP, ARP, stats, bridge) |
| 1 | 2 KB / 2 KB | DHCP client (UDP port 68) |
| 2 | 2 KB / 2 KB | ICMP ping and traceroute (IPRAW) |
| 3 | 2 KB / 2 KB | DNS (UDP 53), WoL (UDP 9), mDNS (UDP 5353), SSDP (UDP 1900), File Manager HTTP (TCP 80) |
| 4 | 1 KB / 1 KB | PXE TFTP listen socket (UDP port 69) |
| 5 | 1 KB / 1 KB | PXE TFTP data transfer socket (ephemeral port) |

Socket 0 uses MACRAW mode with `MFEN=0` (MAC Filter Enable disabled), which means it receives **all** Ethernet frames on the wire, not just those addressed to the Flipper's MAC. This enables passive protocol listeners (LLDP, CDP, STP) and the promiscuous bridge mode.

**Auto Test sharing**: during Auto Test, Socket 0 is shared between the LLDP listener and the ARP scan. These two operations run sequentially (not in parallel) to avoid contention on the MACRAW socket.

## Threading Model

The app uses two threads:

### Main Thread (4 KB stack)

Runs the Flipper's `ViewDispatcher` GUI event loop. Handles:

- User input (button presses, navigation)
- View switching between submenu, text box, byte input, text input, and custom views
- Timer callbacks (DHCP 1-second timer)

The main thread **never** performs blocking network operations. All long-running tasks are delegated to the worker thread.

### Worker Thread (8 KB stack)

Created on-demand when a scan/listener/server starts. Destroyed when the operation completes or is cancelled:

```
User selects feature → main thread allocates worker → worker runs protocol logic
                                                     → worker updates text buffer
                                                     → main thread renders UI
User presses Back   → main thread sets worker_running = false
                    → worker exits its loop
                    → main thread joins and frees worker
```

Communication between threads:

- **Cancellation**: `volatile bool worker_running` flag. The worker checks this flag in its main loop and exits when it becomes `false`.
- **Data**: worker writes to `FuriString` text buffers owned by the app struct. The main thread reads them for display. No mutex needed because the TextBox view reads the string pointer atomically.
- **Completion**: worker calls `view_dispatcher_send_custom_event()` to notify the main thread when done.

## Memory Model

The app runs with a 4 KB application stack (defined in `application.fam`). All large allocations are on the heap:

| Allocation | Size | Lifetime |
|-----------|------|----------|
| Frame receive buffer (`frame_buf`) | 1600 B | App lifetime |
| Ping graph state (`ping_graph`) | ~560 B | App lifetime |
| History state | ~2 KB | App lifetime |
| FuriString text buffers (x20+) | Variable | App lifetime |
| Bridge state | ~100 B | App lifetime |
| File manager read buffer | 2 KB | During file manager operation |

The worker thread has its own 8 KB stack, which is sufficient for the deepest protocol parser call chains.

### Auto Test LLDP Thread (3 KB stack)

During Auto Test, a dedicated LLDP listener thread is spawned with a 3 KB stack. It opens Socket 0 in MACRAW mode and passively listens for LLDP/CDP frames while the main Auto Test sequence continues with subsequent steps (the LLDP listen runs in parallel with ARP scan sequencing on the worker thread). The LLDP thread uses a private `malloc` buffer for frame reception to avoid conflicts with the worker thread's frame buffer.

## DHCP Caching

DHCP negotiation takes 3-15 seconds depending on network conditions. To avoid repeating this for every feature, the app caches the first successful DHCP result:

1. First feature that needs network config (ARP scan, ping, etc.) triggers DHCP Discover/Offer
2. Results are stored in `app->dhcp_ip`, `dhcp_mask`, `dhcp_gw`, `dhcp_dns`, `dhcp_valid`
3. All subsequent features reuse these cached values
4. Cache is invalidated only when the app exits

This means switching between Ping, ARP Scan, DNS Lookup, etc. is instant after the initial DHCP.

## View Hierarchy

The app uses Flipper's `ViewDispatcher` pattern with a hierarchical submenu structure:

```
Main Menu
├── Auto Test
├── Port Info (submenu)
│   ├── Link Info
│   ├── DHCP Analyze
│   ├── LLDP/CDP
│   └── STP/VLAN
├── Scan (submenu)
│   ├── ARP Scan
│   ├── Ping Sweep
│   ├── mDNS/SSDP
│   └── Port Scan (submenu: Top 20 / Top 100 / Custom)
├── Diagnostics (submenu)
│   ├── Ping
│   ├── Continuous Ping
│   ├── DNS Lookup
│   └── Traceroute
├── Traffic (submenu)
│   ├── Packet Capture
│   ├── ETH Bridge
│   └── Statistics
├── Utilities (submenu)
│   ├── Wake-on-LAN
│   ├── PXE Server
│   └── File Manager
├── History
└── Settings (includes About)
```

The menu header dynamically shows link status: `LAN [UP 100M FD]` or `LAN [DOWN]`.

## SD Card Data Layout

The app stores data under its `appid` directory on the SD card:

```
/ext/apps_data/lan_tester/
├── settings.conf           # User settings (auto-save, sound, DNS, ping config)
├── mac.conf                # Persisted MAC address (random on first boot)
├── history/                # Auto-saved scan results with timestamps
│   ├── arp_20260401_143022.txt
│   ├── ping_20260401_143105.txt
│   └── ...
├── pcap/                   # PCAP traffic captures
│   ├── capture_20260401_150000.pcap
│   └── ...
└── pxe/                    # PXE boot files
    ├── undionly.kpxe
    └── ipxe.efi
```

The `settings.conf` and `mac.conf` paths use `APP_DATA_PATH()` macro which automatically resolves to `/ext/apps_data/lan_tester/` based on the `appid` in `application.fam`.

The PXE boot directory uses `EXT_PATH("apps_data/lan_tester/pxe")` as a hardcoded path because TFTP file serving needs an absolute path.
