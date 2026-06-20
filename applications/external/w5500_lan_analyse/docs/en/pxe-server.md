[← Back to documentation index](README.md)

# PXE Server Guide

The PXE Server turns Flipper Zero into a minimal network boot server. It can PXE-boot computers, servers, and embedded devices over Ethernet using standard DHCP + TFTP protocols.

## What is PXE Boot?

PXE (Preboot Execution Environment) allows a computer to boot an operating system over the network instead of from a local disk. The boot process:

1. Target machine sends a DHCP request with PXE vendor extensions
2. DHCP server responds with an IP address and the name of a boot file
3. Target machine downloads the boot file via TFTP
4. Boot file loads and starts the operating system or boot loader

The Flipper can act as both the DHCP server and the TFTP server, or as TFTP-only if an external DHCP server exists on the network.

## Supported Boot File Formats

| Extension | Type | Description |
|-----------|------|-------------|
| `.kpxe` | Legacy BIOS | iPXE/gPXE UNDI boot loader |
| `.efi` | UEFI | EFI boot loader (x64, ARM) |
| `.pxe` | Legacy BIOS | Standard PXE boot program |
| `.0` | Legacy BIOS | Generic PXE boot file (e.g., `pxelinux.0`) |

## Setup

### 1. Place Boot Files on SD Card

Copy boot files to the Flipper's SD card:

```
/ext/apps_data/lan_tester/pxe/
├── undionly.kpxe        # Legacy BIOS boot
├── ipxe.efi             # UEFI boot
└── (other boot files)
```

**Recommended**: download `undionly.kpxe` (~70 KB) from [netboot.xyz](https://boot.netboot.xyz). This iPXE loader provides a menu to boot various OS installers and utilities over the internet.

### 2. Connect to Target

Connect the Flipper's Ethernet port to the target machine:

- **Direct cable**: Flipper ↔ target machine (any cable type, Auto-MDI/MDI-X handles crossover)
- **Through a switch**: Flipper and target on the same switch/VLAN

### 3. Configure and Start

Open **Tools → PXE Server** on the Flipper. The settings screen appears.

## DHCP Auto-Detection

When you enter PXE settings, the app automatically probes the network for an existing DHCP server:

1. Sends a DHCP Discover packet
2. Waits 5 seconds for an Offer

**If an external DHCP server responds:**
- Built-in DHCP is disabled (you don't want two DHCP servers!)
- Server IP, Client IP, and Subnet are populated from the detected network
- Flipper acts as **TFTP-only** server

**If no DHCP server responds:**
- Built-in DHCP is enabled
- Default subnet: 192.168.77.0/24
- Default Server IP: 192.168.77.1
- Default Client IP: 192.168.77.10
- Flipper provides both **DHCP + TFTP**

All IP fields are editable -- auto-detected values are defaults you can override.

## Settings Screen

| Item | Description |
|------|-------------|
| **Start PXE** | Launch the server |
| **DHCP Server** | ON/OFF -- toggle built-in DHCP server |
| **Boot File** | Select from detected files (left/right arrows to cycle) |
| **Server IP** | Flipper's IP on the PXE network |
| **Client IP** | IP to offer to the booting machine |
| **Subnet Mask** | Network mask |
| **Help** | Quick reference screen |

### Boot File Selection

If multiple boot files are found in the PXE directory (up to 8), use **left/right arrows** on the "Boot File" item to cycle through them. Files are sorted with preferred names first:

1. `undionly.kpxe`
2. `ipxe.efi`
3. `snponly.efi`
4. All other files alphabetically

## Target Machine Configuration

The target machine must have PXE/Network Boot enabled:

1. Enter BIOS/UEFI settings (usually F2, F12, Del, or Esc during POST)
2. Find **Boot** or **Network Boot** settings
3. Enable **PXE Boot** or **Network Boot**
4. Set boot order to **Network** first (or use the one-time boot menu, usually F12)
5. For UEFI boot, ensure the boot mode matches your boot file (UEFI for `.efi`, Legacy for `.kpxe`)

## Progress Display

While serving a boot file, the screen shows:

- Current transfer state (waiting, sending, complete)
- Block counter (current block / total blocks)
- Progress bar
- Transfer speed

After each transfer completes, the server resets to idle and waits for the next client. Multiple machines can be booted sequentially (not simultaneously).

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Target doesn't start PXE boot | Enable Network Boot in BIOS. Check boot order. |
| "No boot file found" | Place `.kpxe`/`.efi` files in `/ext/apps_data/lan_tester/pxe/` |
| DHCP offer not received by target | Check cable. If on a switch with existing DHCP, disable Flipper's DHCP. |
| Transfer stalls | Try a shorter/better Ethernet cable. SPI throughput may limit large files. |
| UEFI machine gets BIOS file | Select the correct `.efi` boot file in settings. |
