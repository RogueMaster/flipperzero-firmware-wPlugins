[← Back to documentation index](README.md)

# Troubleshooting & Limitations

## Common Problems

### "W5500 Not Found!"

The app reads the W5500 version register on startup. If it doesn't get the expected response (`0x04`), this error appears.

**Diagnosis steps:**
1. Check all SPI wiring connections (see [Hardware Setup](hardware.md))
2. Verify 3V3 and GND are connected
3. Try unplugging and re-plugging the module
4. Check for bent or dirty pins on the GPIO header
5. Try a different W5500 module if available

**Common causes:**
- Loose connection on one of the SPI pins
- Module not seated properly on the GPIO header
- Module powered but CS/MISO/MOSI wires swapped

### "No Link!"

The W5500 is detected but no Ethernet link is established.

**Diagnosis steps:**
1. Check that an Ethernet cable is plugged into the RJ45 port
2. Verify the other end is connected to an active switch port or device
3. Try a different Ethernet cable
4. Check the link LEDs on the W5500 module (if present)
5. Try a different switch port

**Common causes:**
- Cable not fully inserted
- Bad cable (try another one)
- Remote port is administratively disabled
- Remote device is powered off

### "DHCP failed"

No DHCP Offer received within the timeout period.

**Diagnosis steps:**
1. First verify Link Info shows "UP" -- physical link must be established
2. Check that the network has a DHCP server
3. Try a different switch port or cable
4. Check if the switch port has 802.1X authentication enabled (which would block DHCP)
5. Check if there's a VLAN mismatch (Flipper sends untagged frames)

**Common causes:**
- No DHCP server on the network
- Switch port in a VLAN without DHCP
- 802.1X blocking unauthenticated traffic
- DHCP server overloaded or misconfigured

### Flipper Freezes on Back

Fixed in v1.0. The HTTP server's `send()` function has an internal `while(1)` loop in the WIZnet driver. If a TCP connection is interrupted, this could block forever. The fix force-closes the HTTP socket on exit.

**Solution:** update to the latest version.

### File Manager Shows 403 Forbidden

The authentication token is missing or incorrect in the URL.

**Solution:** use the exact URL shown on the Flipper screen, including the `?t=XXXX` parameter. If you navigated to the IP without the token, add `?t=XXXX` to the URL manually.

### PXE Client Doesn't Boot

**Diagnosis steps:**
1. Verify PXE/Network Boot is enabled in the target's BIOS/UEFI
2. Check boot order -- Network should be first (or use F12 one-time boot menu)
3. Verify boot files exist in `/ext/apps_data/lan_tester/pxe/` on the SD card
4. Check that the correct boot file is selected (BIOS needs `.kpxe`, UEFI needs `.efi`)
5. If using an external DHCP server, ensure Flipper's DHCP is disabled
6. Try a direct cable connection instead of through a switch

---

## Hardware Limitations

These features **cannot** be implemented on Flipper Zero + W5500 due to hardware constraints:

### 802.1X Authentication

Requires a full EAP supplicant with TLS support. The Flipper Zero doesn't have enough RAM for the cryptographic operations, and the FAP SDK doesn't include TLS libraries.

### Full 100 Mbps Traffic Capture

The SPI bus between the Flipper's STM32 and the W5500 runs at ~8 MHz, limiting throughput to ~1 MB/s. This means the PCAP capture in Bridge mode and Packet Capture can only record traffic that actually passes through the bridge or arrives at the MACRAW socket at SPI speed. High-throughput networks will see packet loss in captures.

### SNMP Queries

SNMP uses ASN.1/BER encoding which requires a complex parser. The ASN.1 decode/encode library is too large for the Flipper's available RAM.

### TLS/HTTPS

The FAP SDK does not include cryptographic libraries. No TLS, no HTTPS, no SSH. All network communication is plaintext.

---

## OUI Vendor Database

The built-in OUI lookup table contains ~120 common vendor prefixes, covering the most frequently seen network equipment manufacturers:

Cisco, HP/HPE, Dell, Intel, Broadcom, Realtek, Apple, Samsung, Huawei, TP-Link, Ubiquiti, Juniper, Arista, MikroTik, Netgear, ASUS, D-Link, Synology, QNAP, VMware, Microsoft, Google, Amazon, Lenovo, Supermicro, Aruba, Fortinet, Palo Alto, WIZnet, Raspberry Pi, Espressif, and more.

Unknown OUI prefixes are displayed as "Unknown" in ARP scan results.

---

## Third-Party Libraries

### WIZnet ioLibrary_Driver

Vendored copy (not a git submodule) of the [official W5500 driver](https://github.com/Wiznet/ioLibrary_Driver). Located at `lib/ioLibrary_Driver/`.

**Compiled components:**
- `Ethernet/W5500/*.c` -- W5500 chip-level driver
- `Ethernet/*.c` -- socket abstraction layer (`socket.c`, `wizchip_conf.c`)
- `Internet/DHCP/*.c` -- DHCP client
- `Internet/DNS/*.c` -- DNS resolver
- `Internet/ICMP/*.c` -- ICMP ping and traceroute

**Excluded from build** (not needed):
- MQTT, FTP, HTTP, SNMP, SNTP, TFTP application modules
- W5100, W5100S, W5200, W5300 chip drivers

This library is vendored to ensure reproducible builds. **Do not modify** files in `lib/` -- report issues upstream if you find bugs in the driver.
