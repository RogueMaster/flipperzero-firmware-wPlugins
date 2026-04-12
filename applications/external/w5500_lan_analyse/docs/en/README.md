# Flipper Zero LAN Tester (W5500) -- Documentation

Turn your **Flipper Zero + W5500** Ethernet module into a professional-grade portable LAN tester. Analyze links, discover network neighbors, scan subnets, fingerprint DHCP servers, capture packets, bridge USB to Ethernet, PXE-boot machines, and manage SD card files over HTTP -- all from a pocket-sized device.

## Key Highlights

- 33 network & security tools in a single Flipper Zero application
- Works with any W5500-based SPI Ethernet board
- No IP lease consumed during DHCP analysis (safe for production networks)
- DHCP result cached once, reused everywhere -- no repeated 15-second waits
- Real-time visual feedback: progress bars, countdown timers, RTT graphs
- All scan results auto-saved with timestamps for later review
- USB-to-Ethernet bridge with optional PCAP recording
- Built-in PXE boot server and web-based file manager

## Feature Summary

| Feature | Category | Description |
|---|---|---|
| Auto Test | -- | One-touch automated diagnostics: Link → DHCP → Ping → DNS → LLDP → ARP |
| Link Info | Port Info | PHY link status, speed, duplex, MAC address, W5500 version |
| DHCP Analyzer | Port Info | Discover-only analysis with option fingerprinting |
| LLDP/CDP | Port Info | Passive IEEE 802.1AB and Cisco CDP neighbor discovery |
| STP/VLAN | Port Info | BPDU listener and 802.1Q VLAN tag detection |
| ARP Scanner | Scan | Subnet scan with OUI vendor lookup (~120 vendors) |
| Ping Sweep | Scan | ICMP sweep of entire CIDR range with interactive host list |
| mDNS/SSDP | Scan | Multicast DNS and UPnP service discovery |
| Port Scanner | Scan | TCP connect scan: Top-20, Top-100, or custom range |
| Ping | Diagnostics | ICMP echo with configurable count and timeout |
| Continuous Ping | Diagnostics | Real-time RTT graph with min/max/avg and loss tracking |
| DNS Lookup | Diagnostics | Hostname resolution via UDP DNS |
| Traceroute | Diagnostics | ICMP hop-by-hop path discovery (up to 30 hops) |
| Packet Capture | Traffic | Standalone PCAP dump to SD card |
| ETH Bridge | Traffic | USB-to-Ethernet bridge via CDC-ECM with PCAP recording |
| Statistics | Traffic | Frame counters by type and EtherType (10s capture) |
| SNMP GET | Port Info | Query sysName, sysDescr, sysUpTime, ifStatus (v1/v2c) |
| NetBIOS Query | Scan | Discover Windows machine names and workgroups |
| NTP Diagnostics | Diagnostics | Stratum, root delay, reference ID, RTT |
| DNS Poison Check | Diagnostics | Compare local vs public DNS to detect poisoning |
| ARP Watch | Security | Detect spoofing, duplicate IPs, ARP storms |
| Rogue DHCP | Security | Find unauthorized DHCP servers |
| Rogue RA (IPv6) | Security | Find unauthorized Router Advertisements |
| DHCP Fingerprint | Security | Identify client OS by DHCP option 55 |
| 802.1X Probe | Security | EAPOL-Start probe, detect port authentication |
| VLAN Hopping | Security | Test VLAN isolation (Top 10 / Custom VLANs) |
| RADIUS Test | Security | Access-Request with PAP/MD5, check Accept/Reject |
| Wake-on-LAN | Utilities | Magic packet sender |
| PXE Server | Utilities | Network boot server with DHCP + TFTP |
| PXE Download | Utilities | Download iPXE/EFI boot files from the internet to SD card |
| File Manager | Utilities | Web-based SD card manager via HTTP |
| TFTP Client | Utilities | Download config files from network equipment |
| IPMI Query | Utilities | BMC chassis status, device ID, firmware version |
| History | -- | Timestamped auto-saved results, browsable and deletable |
| Settings | -- | Auto-save, sound/vibro, custom DNS, ping config, MAC Changer, About |

## Documentation

| Page | Contents |
|---|---|
| **[Hardware Setup](hardware.md)** | Wiring diagram, pin descriptions, power, compatible boards |
| **[Building from Source](building.md)** | Prerequisites, build commands, installation, CI/CD |
| **[Architecture & Internals](architecture.md)** | Project tree, socket allocation, threading, memory model |
| **[Feature Guide](usage.md)** | Detailed per-feature documentation by menu category |
| **[ETH Bridge](eth-bridge.md)** | USB-to-Ethernet bridging, PCAP recording, platform support |
| **[PXE Server](pxe-server.md)** | Network boot setup, DHCP auto-detection, TFTP serving |
| **[File Manager](file-manager.md)** | HTTP file server, auth tokens, browser-based SD management |
| **[Security](security.md)** | Security model, per-component measures, reporting |
| **[Troubleshooting](troubleshooting.md)** | Common problems, hardware limitations, third-party libraries |

## Credits

- Based on [arag0re/fz-eth-troubleshooter](https://github.com/arag0re/fz-eth-troubleshooter) (fork of [karasevia/finik_eth](https://github.com/karasevia/finik_eth))
- Uses [WIZnet ioLibrary_Driver](https://github.com/Wiznet/ioLibrary_Driver) for W5500 hardware abstraction
- Built for [Flipper Zero OFW](https://github.com/flipperdevices/flipperzero-firmware)

## License

MIT License. See [LICENSE](../../LICENSE) for details.
