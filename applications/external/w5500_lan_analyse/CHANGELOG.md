# 2.8.0

## Added
- **Apply NTP Sync** — apply cached NTP time to Flipper clock after reviewing diff in NTP Diagnostics
- **NTP Diagnostics** now shows received UTC time and clock difference
- **Settings persistence** — all tool targets (IP addresses, hostnames) saved to settings.conf and restored on next launch
- **MAC address** stored in settings.conf (mac.conf no longer used)

# 2.7.0

## Added
- **File Manager** custom CSS/JS support — place custom.css and/or custom.js in apps_data/lan_tester/web/ on SD card to override default styles and scripts
- **File Manager** JS-based alphabetical sorting (directories first, then files) replaces two-pass C rendering
- **File Manager** serves .css and .js files with correct Content-Type (text/css, text/javascript) instead of octet-stream
- Example themes in docs/filemanager_themes/: Hacker Green, Arctic Light, Cyberpunk, Solarized Dark
- Example custom.js with search, breadcrumbs, multi-select delete, drag and drop upload, file preview (text, images, audio, video, PDF, hex dump)
- Build excludes docs/ directory via sources parameter in application.fam

# 2.6.0

## Added
- **Port Scan (Custom)** available from Discovered Hosts actions menu
- **Discovered Hosts pagination** — browse all found hosts in pages of 24
- **Ping Sweep** and **ARP Scan** no longer have host count limits (results stored on SD card)
- **ARP Scan** dedup array supports up to 1024 hosts (was 128)

## Improved
- **Ping Sweep** progress bar on header line, compact format: found/scanned/total
- **Ping Sweep** BACK button now interrupts immediately (was blocked by ping timeout)
- **ARP Scan** results written to SD during scan (no large heap buffer at finish)
- **ARP Scan** dedup uses 2 bytes per host instead of 16 (same 2 KB, 4x more hosts)
- Worker thread allocated once at startup, reused for all tools (eliminates heap fragmentation)
- Discovered Hosts loaded from SD card in single file pass per page (was N file opens per host)
- Storage API opened once per scan, not per host (was 176 open/close cycles)
- Progress bar uses fixed-width characters (no visual jitter on Flipper font)

## Fixed
- **Ping Sweep** crash when clicking host after large scan (worker thread OOM)
- **ARP Scan** out of memory on networks with >128 hosts
- **Discovered Hosts** out of memory with >64 hosts in submenu

# 2.5.0

## Improved
- Reduced flash usage by ~2.8 KB through code deduplication and dead code removal
- Reduced heap usage by ~2.4 KB: DHCP buffer optimized, unused struct fields removed, USB RX buffer now allocated on demand
- Removed unused DNS and ICMP modules from WIZnet library build
- Disabled debug logging in WIZnet DHCP library to save flash
- Simplified PXE Help screen and File Manager web interface styling

## Removed
- **RADIUS Test** tool removed (limited by Flipper keyboard — no special characters for passwords)

# 2.4.5

## Added
- **PXE Server** auto-selects boot file by client architecture (DHCP Option 93) — BIOS clients get .kpxe/.pxe, UEFI clients get .efi
- **PXE Server** TFTP block size negotiation (RFC 2348) — supports blksize up to 1468 bytes, allowing files over 512 KB to transfer without "PXE-E3A: TFTP too many packages" error
- **PXE Server** TFTP transfer size option (RFC 2349) — reports file size to client in OACK for progress display
- **PXE Download** now includes ipxe.pxe (native driver build) for better Legacy BIOS compatibility

## Improved
- **PXE Server** TFTP sends DATA from port 69 (single socket) — fixes iPXE rejecting packets from ephemeral port
- **PXE Server** gracefully restarts transfer when client sends new RRQ during active session instead of rejecting with "Server busy"
- **PXE Server** uses real DHCP-assigned IP when built-in DHCP is OFF, so clients on external networks can reach TFTP
- **Auto Test** LLDP/CDP listener now runs inline instead of a separate thread — saves ~2 KB heap
- **Discovery** no longer allocates a device array — uses compact dedup and streams results live to screen
- **File Manager** directory listing uses two-pass streaming instead of sorted array (~3.8 KB saved)
- Tools now free leftover state before launching, reducing heap fragmentation

## Fixed
- **PXE Server** TFTP transfer broken — clients rejected DATA from wrong source port (port 51000 instead of 69)
- **Auto Test** instant out-of-memory — worker thread stack reduced from 8 KB to 4 KB
- Stack overflows in Settings, LLDP/CDP, STP/VLAN, DNS, VLAN hopping, EAPOL, RADIUS, Ping, Traceroute, DHCP, IPMI, PXE, History, PCAP dump (128-512 byte buffers moved off stack)
- **RADIUS client** broken response parsing
- **Rogue DHCP** and **TFTP client** broken receive — size was pointer size, not buffer size
- **File Manager** stack overflow in HTML escape and response header buffers
- **ETH Bridge** crash on failed allocation
- Multiple NULL-check guards added after malloc

# 2.4.0

## Added
- **PXE Download** — download iPXE and EFI boot files from the internet directly to SD card for PXE Server

## Improved
- File Manager auth token is now digits-only for easier entry on mobile devices
- History files organized into a dedicated subdirectory
- Cleaner output: removed unnecessary blank lines in PXE Server, File Manager, and PXE Download screens

## Fixed
- **File Manager** out-of-memory crash when browsing directories with many files
- **File Manager** "Unable to connect" error after delete or redirect operations
- **File Manager** disconnect race condition during HTTP downloads
- **W5500 socket buffers** not powers of 2 — caused mDNS discovery failures
- **PXE Download** hang on large files, missing progress display, broken Back button
- **PXE Download** incorrect URLs for EFI boot files
- **SNMP** stack overflow in packet builder (379 bytes on stack, limit 128)
- **SNMP** redundant header output reset

# 2.2.1

## Added
- **Host Info** action — view IP, MAC, vendor for any discovered host
- **NetBIOS Query** action — identify Windows machine names from host list
- **SNMP GET** action — query device info directly from host list
- **IPMI Query** action — check BMC status from host list

## Improved
- ARP scan rate reduced to ~4 pps to avoid triggering switch DAI/storm control
- Back navigation from host actions returns to actions menu instead of scan category
- Interrupted ARP scan / Ping Sweep now shows discovered hosts list

# 2.0.0

## Improved
- Significantly reduced memory usage — app is stable during prolonged use
- Compact output for all tools — results fit on screen without scrolling
- Safe W5500 init/deinit — no hangs when reopening app after crash
- History: fixed browsing after heavy tool usage

## Changed
- VLAN Hop: Top 10 and Custom modes (like Port Scan)
- Rogue DHCP: concise output with key details per server

# 1.5.0

## Added
- New menu category: **Security**
- **SNMP v1/v2c GET** — query sysName, sysDescr, sysUpTime, ifStatus
- **NTP Diagnostics** — stratum, offset, root delay, reference ID
- **NetBIOS Name Query** — discover Windows machine names and workgroups
- **DNS Poisoning Check** — compare local vs public DNS responses
- **ARP Watch** — detect spoofing, duplicate IPs, ARP storms
- **Rogue DHCP Detection** — find unauthorized DHCP servers
- **Rogue RA Detection** — find unauthorized IPv6 routers
- **DHCP Fingerprinting** — identify client OS by option 55
- **802.1X Probe** — check if port authentication is enabled
- **VLAN Hopping Test** — verify VLAN isolation
- **TFTP Client** — download config files from network equipment
- **IPMI v1.5** — chassis status, BMC device info
- **RADIUS Test Client** — send Access-Request, check Accept/Reject

# 1.4.0

## Added
- **Auto Test**: one-touch automated network diagnostics — plug cable, get results
  - Tests: Link Info → DHCP → Ping Gateway → DNS Resolve → Internet Ping → LLDP/CDP → ARP Host Count
  - LLDP listener runs in a parallel thread (Socket 0) alongside DHCP/Ping/DNS (Sockets 1-3)
  - ARP scan runs only after LLDP thread completes (Socket 0 shared, sequential access)
  - Verdict: Auto Test OK if DHCP + GW ping + DNS all pass; internet ping is informational only
  - Auto-cycles: cable removed → "Waiting for link..." → cable inserted → new test
  - Settings: AT DNS host (default: google.com), AT LLDP wait (10/20/30/60s), AT ARP scan (On/Off)

## Changed
- **Menu restructured**:
  - Network Info → **Port Info** (Link Info, DHCP Analyze, LLDP/CDP, STP/VLAN)
  - Discovery → **Scan** (ARP Scan, Ping Sweep, mDNS/SSDP, Port Scan submenu)
  - **Traffic** (new): Packet Capture, ETH Bridge, Statistics
  - Tools → **Utilities** (Wake-on-LAN, PXE Server, File Manager)
  - **Port Scan**: consolidated into submenu (Top 20 / Top 100 / Custom Range)
  - **About**: moved from main menu to last item in Settings
- **Settings**: replaced magic index numbers with LanTesterSettingsItem enum for maintainability

# 1.2.0

## Changed
- **Renamed**: appid changed from eth_tester to lan_tester; SD card data path is now /ext/apps_data/lan_tester/

# 1.1.0

## Added
- **Custom DNS Server**: configurable DNS server in Settings (default: 8.8.8.8), overrides DHCP-provided DNS
- **Ping Settings**: configurable ping count (1-100), timeout (500-10000ms), and continuous ping interval (200-5000ms) in Settings
- **Traceroute DNS Resolve**: traceroute now accepts hostnames in addition to IP addresses, with automatic DNS resolution
- **Port Scan (Custom)**: scan any port range (1-65535) in addition to Top-20/Top-100 presets
- **Packet Capture**: standalone PCAP traffic dump in Tools — capture raw Ethernet frames to .pcap file on SD card without ETH Bridge
- **Interactive Host List**: discovered hosts from Ping Sweep and ARP Scan are now clickable — select a host to Ping, Continuous Ping, Traceroute, Port Scan, or Wake-on-LAN directly

## Fixed
- **Continuous Ping graph**: graph now correctly fills from right edge to left; increased sample buffer from 100 to 128 to fill full screen width
- **Continuous Ping back button**: Back now properly exits the continuous ping view (was stuck on screen after pressing Back)
- **Ping Sweep / ARP Scan back button**: first Back press stops running scan and shows "Scan stopped by user"; second Back returns to Discovery menu (was: immediately navigated away, killing scan silently)

## Changed
- **Settings**: added Custom DNS, DNS Server IP, Ping Count, Ping Timeout, Continuous Ping Interval items
- **Traceroute**: replaced IP-only keyboard with TextInput supporting both IPs and hostnames

# 1.0.0

## Added
- **File Manager**: web-based file manager for Flipper's microSD card in Tools category
  - HTTP server on port 80 accessible from any browser on the LAN
  - Browse directories, download files, upload files, create folders, delete files/folders
  - Dark theme web UI optimized for mobile and desktop
  - Reliable TCP send layer with SEND_OK polling for W5500's non-blocking send()
  - Streams large file downloads in chunks; handles multipart/form-data uploads

## Changed
- **MAC Changer** moved from Tools to Settings
- **Settings** now includes: Auto-save, Sound & vibro, Clear History, MAC Changer
- **PXE Server**: auto-detect external DHCP when entering settings — if found, disable own DHCP and populate Server IP/Client IP/Subnet from detected network
- **PXE Server**: boot file selector with cycling (left/right arrows) when multiple .kpxe/.efi/.pxe/.0 files found (up to 8)
- **PXE Server**: settings UI reordered — Start PXE first, then DHCP toggle, Boot File, IPs, Help
- **PXE Server**: IP fields auto-populated from external DHCP when available

## Security
- **File Manager**: path traversal protection — reject ".." in all URL paths
- **File Manager**: XSS prevention — HTML-escape filenames in directory listings
- **File Manager**: random auth token per session, displayed on Flipper screen, required for all HTTP requests
- **File Manager**: upload filename sanitization — strip path separators from filenames
- **TFTP Server**: reject path traversal (".." and "/") in TFTP filenames
- **mDNS**: recursive DNS pointer following limited to depth 4 (prevents stack overflow)
- **DNS**: validate response source IP matches expected DNS server
- **MAC**: unique MAC address per device (random on first boot, persisted to SD)

## Fixed
- Force-close HTTP socket on back/exit to prevent Flipper freeze (WIZnet send() has internal while(1) with no timeout)

# 0.11.0

## Added
- **PXE Server**: minimal PXE boot server in Tools category
  - Configurable Server IP, Client IP, Subnet via IP Keyboard before start
  - Optional DHCP server (toggle ON/OFF; OFF = TFTP-only mode)
  - TFTP server reads boot files from SD (/ext/apps_data/eth_tester/pxe/)
  - Auto-detects boot file: .kpxe, .efi, .pxe, .0
  - Real-time progress bar with block counters
  - Resets to idle after each transfer (multi-client sequential)

# 0.10.0

## Added
- **ETH Bridge**: new tool that turns Flipper Zero into a USB-to-Ethernet bridge
  - Phone/PC connects via USB CDC-ECM, Flipper bridges traffic to LAN via W5500 MACRAW at Layer 2
  - Host transparently gets an IP from the LAN's DHCP server
  - Live status screen showing USB connection state, LAN link info, frame counters (USB->LAN, LAN->USB), and error count
  - Automatic USB profile save/restore: switches to CDC-ECM on start, restores original USB (CDC Serial) on exit
  - Compatible with Linux, macOS, and Android (native CDC-ECM support)
- New usb_eth/ module: USB CDC-ECM device implementation using Flipper's FuriHalUsbInterface
- New bridge/ module: bidirectional Ethernet frame forwarding engine

# 0.9.0

## Added
- **Continuous Ping**: real-time RTT graph with min/max/avg/loss stats, target IP displayed on screen
- **DNS Lookup**: resolve hostnames via UDP DNS (A-record) using DHCP-provided DNS server
- **Wake-on-LAN**: send magic packets to any MAC address via broadcast UDP
- **Port Scanner**: TCP connect scan with Top-20 (quick) and Top-100 (full) presets
- **Traceroute**: ICMP-based hop-by-hop path discovery with per-hop RTT
- **Ping Sweep**: ICMP sweep of entire subnet with CIDR input, auto-detected from DHCP
- **mDNS/SSDP Discovery**: find services and devices via multicast DNS and UPnP/SSDP
- **STP/VLAN Detection**: passive BPDU listener + 802.1Q VLAN tag extraction
- **MAC Changer**: randomize or set custom MAC, persisted to SD card
- **History Browser**: timestamped results saved to SD, browse and delete old entries
- **Settings screen**: toggle auto-save and sound/vibro notifications, clear history
- **Hierarchical menu**: features grouped into Network Info, Discovery, Diagnostics, Tools
- **Link status header**: main menu shows live link state (UP/DOWN, speed, duplex)
- **LED/vibro notifications**: green blink + vibro on scan completion, red on errors (optional)
- **Countdown timers**: LLDP/CDP (60s), STP/VLAN (30s), Statistics (10s) show remaining time
- **ASCII progress bars**: visual progress indicator for Ping Sweep and Port Scan
- **Smart IP defaults**: ping/traceroute inputs pre-populated with DHCP gateway

## Changed
- **DHCP caching**: single DHCP negotiation shared across all operations (was: repeated per scan)
- **Compact output**: ARP scan shows 2 lines per host (was: 3), Statistics uses single-screen layout
- **Compact headers**: shorter section markers — saves one line per screen
- **Menu reordered**: most-used tools (Ping, ARP, DHCP) at the top
- **MAC Changer**: requires user confirmation via ByteInput before applying (was: auto-randomize)
- **Back navigation**: returns to parent category submenu, not always to main menu
- **Cancel feedback**: shows "Stopping..." in header when cancelling a running operation
- **frame_buf moved to heap**: reduces static memory footprint by 1.6 KB
- **Stack safety**: 2 KB history read buffer moved from stack to heap

## Fixed
- History file TextBox scroll (was broken by input callback override)
- Version mismatch between application.fam and About screen

# 0.1.0

## Added
- **Link Info** view: PHY status, speed, duplex, MAC address, W5500 version check
- **LLDP Listener**: passive IEEE 802.1AB neighbor discovery with full TLV parsing (Types 0-8, 127)
- **CDP Listener**: Cisco Discovery Protocol parser with LLC/SNAP detection
- **ARP Scanner**: active subnet scan with batch requests, OUI vendor lookup (~120 vendors)
- **DHCP Analyzer**: Discover-only mode with option fingerprinting (no IP lease taken)
- **ICMP Ping**: Echo request/reply to gateway with RTT measurement
- **Packet Statistics**: frame counters by destination type and EtherType
- **SD Card Export**: all results saved to /ext/apps_data/eth_tester/
- W5500 HAL with SPI init, hardware reset, PHYCFGR register read, MACRAW socket management
- OUI lookup table covering ~120 common network equipment vendors
- Modular architecture: hal/, protocols/, utils/
- ViewDispatcher-based UI with Submenu and TextBox views