# Quick Start

[← Documentation index](README.md)

From a Flipper Zero and an ESP32-S3 board to the first command, in about ten minutes. Two
boards run SSH Runner: the SSH Runner add-on board, which plugs straight onto the Flipper, and
the Waveshare ESP32-S3-ETH, wired to it with jumper wires. The files come from the project's
latest release.

## Contents

- [1. Connect the Board](#1-connect-the-board)
- [2. Flash the ESP32](#2-flash-the-esp32)
- [3. Install the App](#3-install-the-app)
- [4. First Run](#4-first-run)
- [5. Messages](#5-messages)
- [6. Wiping the Board](#6-wiping-the-board)

## 1. Connect the Board

### SSH Runner Add-On Board

No wiring: the board plugs straight onto the Flipper's GPIO header and runs from the Flipper's
battery. Its USB-C is for flashing, debugging or bench power, and may stay plugged in while the
board sits on the Flipper: a power mux runs the board from USB then and keeps the two supplies
apart. Leave "5V on GPIO" off on the Flipper. The board is rev 0.1 and later; its design files
are in [`hardware/`](../hardware/README.md). On rev 0.1 the status LED (D201) is fitted by hand
(erratum E1).

### Waveshare ESP32-S3-ETH

| Flipper Zero GPIO | ESP32-S3-ETH | Use |
|---|---|---|
| 13 (TX) | GPIO18 (header H1 pin 10) | always |
| 14 (RX) | GPIO17 (header H1 pin 7) | always |
| 8, 11 or 18 (GND) | GND (e.g. H1 pin 8) | always |
| 9 (3V3) | 3V3 header pin | portable use |

- **Desk use:** the three wires marked "always", and power from the board's own USB-C.
- **Portable use:** add a fourth wire from the Flipper's GPIO header pin 9 (3V3) to the board's
  3V3 pin; the board then runs from the Flipper's battery with its USB-C unplugged.

> [!WARNING]
> Never plug in the board's USB-C while the 3V3 wire is connected. Remove that wire first,
> for example to flash the firmware. Leave "5V on GPIO" off on the Flipper.

## 2. Flash the ESP32

Download the firmware zip for your board from the release and unzip it:

| Board | Zip |
|---|---|
| Waveshare ESP32-S3-ETH | `flipper-ssh-runner-esp32-waveshare-v<version>.zip` |
| SSH Runner add-on, rev 0.1 and later | `flipper-ssh-runner-esp32-addon-v<version>.zip` |

It holds two images, named for the board (`<board>` below is `waveshare` or `addon`); you
flash one:

- **First install** (a new board, or one that ran other firmware): erase the flash, then write
  the factory image, `flipper-ssh-runner-<board>-v<version>-factory.bin`, at `0x0`. It holds
  the bootloader, the partition table and the app in one file, and also overwrites the
  settings partition between them.
- **Update** (the board already runs SSH Runner): do not erase. Write only the app image,
  `flipper-ssh-runner-<board>-v<version>-app.bin`, at `0x20000`; that keeps the board's saved
  WiFi network, remembered passwords and trusted host keys.

Never write the factory image on an update: it erases the board's saved WiFi network,
remembered passwords and trusted host keys like an erase does.

Connect the board's USB-C to the computer (on the Waveshare, remove the 3V3 wire first). It
appears as a USB serial port (USB ID `303a:1001`); the Flipper, if also connected, is
`0483:5740`.

### In the Browser (Recommended)

Spacehuhn's [ESPTool](https://esptool.spacehuhn.com/) flashes from the browser, with nothing
to install. It needs a desktop Chrome or Edge (it uses Web Serial).

1. Open the page and click **Connect**; choose the board's port.
2. **First install only:** click **Erase** and confirm. Skip this step when you update.
3. Each row is an offset (the field after `0x`, in hex) and a file (**Select**). Use one row:

   | When | Offset (`0x`) | File |
   |---|---|---|
   | First install | `0` | `...-factory.bin` |
   | Update | `20000` | `...-app.bin` |

   The page shows one row, at offset `0`, with **Multiple files** off; set its offset and
   select the file. (With **Multiple files** on, any row left without a file is skipped.)
4. Click **Program** and confirm.
5. When it is done, restart the board: unplug it and plug it in again, or press its RESET
   button (the add-on board).

If the board does not connect, put it in download mode: unplug it, hold its BOOT button while
you plug it in again, then click **Connect** again. On the add-on board, hold BOOT and press
RESET.

> [!WARNING]
> Do not click **Erase** when you update: it also erases the board's saved WiFi network,
> remembered passwords and trusted host keys.

Espressif's [esptool-js](https://espressif.github.io/esptool-js/) works the same way:
**Connect**, **Erase Flash** on a first install only, one row with the offset and the file,
**Program**.

### With esptool (Command Line)

From the unzipped folder, with [esptool](https://github.com/espressif/esptool)
(`pip install esptool`). First install: erase the flash, then write the factory image at
`0x0`:

```sh
esptool.py --chip esp32s3 -p <port> erase_flash
esptool.py --chip esp32s3 -p <port> write_flash 0x0 flipper-ssh-runner-<board>-v<version>-factory.bin
```

Update: no erase, the app image at `0x20000`:

```sh
esptool.py --chip esp32s3 -p <port> write_flash 0x20000 flipper-ssh-runner-<board>-v<version>-app.bin
```

`<port>` is `COM5`-style on Windows, `/dev/ttyACM0`-style on Linux (better: its
`/dev/serial/by-id/...` name) and `/dev/cu.usbmodem*` on macOS. On Linux the port needs the
`dialout` group, and ModemManager may probe both devices: exclude them with a udev rule
setting `ID_MM_DEVICE_IGNORE`, or stop it.

The images burn no eFuses.

Building the firmware yourself instead: see [Build Outputs](ARCHITECTURE.md#build-outputs).

## 3. Install the App

1. Close any other program that uses the Flipper's USB port.
2. Install the app in one of two ways:
   - **From the catalog (easiest):** connect the Flipper over USB, open
     [SSH Runner in the Flipper Apps Catalog](https://lab.flipper.net/apps/ssh_runner) in
     desktop Chrome or Edge and click **Install**, or find SSH Runner under Apps in the Flipper
     mobile app.
   - **By hand:** copy `ssh_runner-official.fap` to `SD Card/apps/GPIO/` with qFlipper's file
     manager or a card reader. It runs on the official and on the Unleashed firmware;
     `ssh_runner-unleashed.fap` is an optional build for Unleashed only.
3. On the Flipper: **Apps > GPIO > SSH Runner**.

If the Flipper reports an API mismatch or an outdated app after a firmware update, update the
app from the catalog or download the latest release. A Flipper app runs when its API major
version equals the firmware's and its minor version is not newer. The official build targets
API 88.2, which the official firmware `1.5.1` (88.2) and Unleashed `unlshd-094` (88.19) both
accept.

## 4. First Run

1. The top line shows the board: `No board` (check the board's seat or wires, and power),
   `No network` (board found, no network yet) or its IP address.
2. **Network > LAN:** plug the Ethernet cable into the board. The first row goes `No cable` >
   `Waiting` > the DHCP address. For WiFi use **Network > Join WiFi**; the ESP32-S3 is
   **2.4 GHz only** and cannot see 5 GHz networks.
3. **Quick Connect:** type `user@host` on the keyboard. All symbols are on one page; a long
   press types the alternate (`_` gives a space, `;` gives `:`, a letter gives upper case), and
   a long Back deletes. Check the host key fingerprint, choose Trust, type the password.
4. Pick a command or type one. **Long-press a command to see all of it** (with Edit, Run and
   Delete); OK runs it at once. The output scrolls with Up/Down and the bottom line shows the
   exit status. Back stops a running command.
5. After a successful first run the app offers to save the host under a name (Hosts).

## 5. Messages

| Message | Meaning and what to do |
|---|---|
| No board | no answer on the UART: check that the add-on board sits fully on the header, or the TX, RX and GND wires, and that the board is powered |
| No network | board found, no Ethernet link and no WiFi |
| LAN cable not connected / No cable | plug the cable in; the RJ45 LEDs should light |
| Waiting | link up but no DHCP lease yet: check the network, or set a static address (Network > LAN > Mode) |
| Connection timed out | host unreachable from the board's network |
| Connection refused | nothing listens on that port (SSH off, wrong port) |
| Host not found | DNS failed: use the IP address, or check the network's DNS |
| New host key | first connection to this host: compare the fingerprint, then Trust |
| Host key changed | the host shows a different key than the one trusted. Cancel unless you know why (reinstall, another device on the same IP); Options lets you connect once, add or replace |
| Permission denied | wrong password, or the key is not accepted (3 tries) |
| Firmware mismatch | the board's firmware and the app speak different protocol versions: update both |

## 6. Wiping the Board

The board stores WiFi passwords, remembered SSH passwords and trusted host keys
**unencrypted** in its flash unless secret protection is enabled. Before the board leaves your
hands, erase it:

```sh
esptool.py --chip esp32s3 -p <port> erase_flash
```

To reuse it, flash it as a first install (the factory image at `0x0`, see
[2. Flash the ESP32](#2-flash-the-esp32)). More: [SECURITY.md](SECURITY.md).

## See Also

- [Hardware and Wiring](HARDWARE.md): pins and the two power modes
- [Flipper App](../flipper-app/README.md): every screen and the files on the SD card
- [PC Tool](../cli/README.md): edit hosts and commands from a computer
- [Security](SECURITY.md): what the board stores and how to protect it
