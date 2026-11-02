# Ducobox Flipper app

A Flipper Zero FAP that emulates a Ducobox RF remote. A Ducobox is a Duco ventilation unit for homes: it extracts stale air and supplies fresh air through ductwork. On first start the app finds the remote's channel, learns the remote from one button frame, and maps the Flipper's D-pad to Speed 1/2/3/Auto. Config allows re-learning or deleting the saved remote.

## Disclaimer

I do not work for Duco and this project is not affiliated with or endorsed by Duco. It is an amateur learning project, not a supported Duco product. Use it at your own risk.

## Install

If Ducobox is available in the Flipper Apps Catalog, install it from there under **Sub-GHz**.

To build and install from source, connect the Flipper over USB and, from the repository root, run:

```bash
nix-shell --run "make launch"
```

This builds the FAP, copies it to `/ext/apps/Sub-GHz/ducobox.fap`, and launches it. The first build downloads the pinned SDK and ARM toolchain, so it takes longer than later builds.

To build without launching, run `nix-shell --run "make"`; the result is `dist/ducobox.fap`, which you can copy to `/ext/apps/Sub-GHz/` on the SD card.

Make sure 868.3 MHz transmission is permitted in your region; the Flipper's Sub-GHz region setting is enforced by the firmware. See [Build](#build) for toolchain and firmware-version details.

## First run

Start the app. It sweeps 868.244, 868.294, 868.326, 868.350 and 868.394 MHz, starting at 868.294, and uses the first decoded Duco frame to identify the channel. If that frame is already a remote-to-box `0x22` button frame, the remote is learned immediately. Otherwise the screen shows the found frequency and asks for one more press to read the remote.

The learned frame is stored as `/ext/apps_data/ducobox/remote.duco`, and the app derives the network ID, addresses and temperature byte from it. Later starts load that template and go straight to the remote screen. The setup screen shows live RSSI, the current or locked frequency and frame counters. If nothing is captured, see [Troubleshooting setup](#troubleshooting-setup).

## Remote screen

| Flipper key | Function |
| --- | --- |
| Left | Speed 1 |
| Up | Speed 2 |
| Right | Speed 3 |
| Down | Auto |
| OK | Config |
| Back | Exit |

- **Tap** a speed key to run that speed temporarily for 15 minutes. Tapping the same key again within 10 seconds extends it to 30, then 45 minutes, matching the remote's 1/2/3 press behaviour.
- **Hold** a speed key for 5 seconds to set it permanently.
- Auto is always sent as auto; a hold does not create a permanent variant.
- The screen shows the last sent action, and the 4-bit counter is incremented for every frame.

**Config** contains:

- **Re-learn remote** — captures a new remote frame and overwrites the saved template.
- **Delete remote** — removes `remote.duco` after confirmation and returns to the first-run setup.
- **Exit** — closes the app.

## Troubleshooting setup

The setup screen is the diagnostic tool. It sweeps 868.244, 868.294, 868.326, 868.350 and 868.394 MHz and shows live RSSI and frame counters:

- **RSSI does not move at all** (stays around the noise floor, e.g. `-100 dBm` or lower): the radio does not hear the remote. Move the remote within a few centimetres of the Flipper and press the button again.
- **RSSI jumps but the frame count stays at 0**: the radio hears energy but the packet engine never matches the sync word. That points at a different protocol revision or a channel outside the five swept frequencies.
- **The title changes to Remote found but no remote screen appears**: the first frame was not a button command (for example a Ducobox confirmation). Press the remote button again.
- **The title stays on Finding frequency**: no decoded frame has been seen yet. Press and hold the remote button so it repeats across the sweep.

If the remote still cannot be learned, move it closer and use **Config → Re-learn remote**. The debug log prints the found frequency; attach the Flipper, run `ufbt cli`, then `log debug`, and look for `Ducobox` lines.

## Build

The project builds inside its `nix-shell`, which provides `ufbt`. `ufbt` downloads the SDK and ARM toolchain matching the pinned firmware on first use.

```bash
nix-shell       # enter the build shell
make            # builds dist/ducobox.fap
make launch     # builds, uploads and launches on a connected Flipper
make clean      # removes build artefacts
```

Enter the shell with a plain `nix-shell` and run `make` inside it, or run a command directly with `nix-shell --run "make launch"`. The `--run` form works because `shell.nix` passes the command into the FHS environment.

### Why the shell is an FHS environment

The Flipper SDK is distributed as generic Linux binaries: the bundled Python 3.11 and the ARM GCC toolchain link against `/lib64/ld-linux-x86-64.so.2`. Stock NixOS installs the stub loader there, so those binaries fail with:

```
Could not start dynamically linked executable: python3
NixOS cannot run dynamically linked executables intended for generic
linux environments out of the box.
```

`shell.nix` therefore returns a `buildFHSEnv` shell that provides a real glibc loader and the libraries the toolchain needs. It imports the nixpkgs channel from `NIX_PATH` (`<nixpkgs>`). This keeps the build working without enabling `programs.nix-ld` system-wide. If you prefer nix-ld, enable it and the regular `mkShell` would work too.

`buildFHSEnv` normally replaces the outer shell with its own, which would silently drop `nix-shell --run`. `shell.nix` recovers the command from nix-shell's temporary rcfile and executes it inside the FHS, so both `nix-shell` and `nix-shell --run "make launch"` behave as expected.

### Firmware and API version

A FAP carries the API version of the SDK it was built with. If that version is newer than the firmware on the device, the Flipper reports "app too new". The tools also refuse to load it.

This project therefore pins the SDK to a firmware release instead of always using the latest one. The default is firmware 1.4.3, which has API 87.1 and matches the Momentum `mntm-012` build on the test device. Build against a different release with:

```bash
FW_VERSION=1.5.1 make
```

The SDK is installed into the project-local `.ufbt/current` directory (ignored by Git). The ARM toolchain is shared with the global `~/.ufbt` state through a symlink, so switching SDK versions does not download another toolchain. `make update` re-fetches the pinned SDK; `make sdk` ensures it is present without building.

To check the API version of a connected Flipper, run `ufbt cli`, then `info device`, and look for `firmware.api.major` and `firmware.api.minor`. The FAP's API must not be newer than those values. The SDK version used for the last build is printed by `make` as `APPCHK ... API: <version>`.

### Screen capture

For development, `make screen` (or `./screen`) reads the current screen over USB and writes `/tmp/ducobox-screen.png`. It speaks the Flipper RPC protobuf protocol directly over the serial port, the same mechanism the web app uses. Close qFlipper or anything else holding `/dev/ttyACM0` first.

Optional arguments: `./screen [port] [output.png]`.

## Why a separate app

The Ducobox remote is not OOK. It uses GFSK at 868.294 MHz, 38.3835 kBaud, 20.63 kHz deviation, with sync word `0xD391`. The Flipper's built-in Sub-GHz presets use different data rates and deviations, so `Read RAW` only catches fragments and replaying those does nothing.

This app loads the reverse engineered Duco CC1101 register set (from the [Ducobox-ESPEasy-Plugin](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin)) with `furi_hal_subghz_load_custom_preset()`. The CC1101 then handles sync detection, packet length and CRC in hardware, and the app only moves payload bytes in and out of the FIFO.

## File format

`.duco` files use the Flipper `FlipperFormat` text container:

```
Filetype: Ducobox RF
Version: 1
Frequency: 868294000
Length: 30
Crc: 1
Data: 0A 12 34 56 ...
```

`Data` is the on-air payload without the CC1101 length byte, CRC or appended RSSI/LQI status bytes. `Crc` records whether the captured frame passed the hardware CRC check. The frequency is the channel found during setup; all synthesised commands are sent there. `remote.duco` is the learned remote template.

## Packet layout

The raw payload matches the Duco packet structure:

| Byte | Meaning |
| --- | --- |
| 0 | Message type |
| 1–4 | 4-byte network ID |
| 5–6 | Source, destination and original addresses |
| 7 | High nibble: original destination bits; low nibble: 4-bit counter |
| 8 | RSSI reported by the sender |
| 9… | Command payload |

The counter increment in the app only changes the low nibble of byte 7.

### Ventilation command encoding

The button commands use command `0x22` with a 6-byte payload. Captures from the physical controller confirmed the layout:

```
payload[9]  = 0x60        command 0 length
payload[10] = 0x00        command 1-4 lengths
payload[11] = 0x22        change/confirm ventilation mode
payload[12] = percentage  0 from the controller
payload[13] = mode byte   0x08 | mode | (hold ? 0x20 : 0)
payload[14] = temperature controller temperature, 0 in the captured frames
payload[15] = subscribe   0
payload[16] = button      0x08 | (hold ? 0x04 : 0) | button number
```

Mode values are `0x00` auto, `0x04` low, `0x05` middle, `0x06` high, `0x07` away. Button numbers are 0 auto, 1 low, 2 middle, 3 high. The `0x08` in the button byte is one button press, which means 15 minutes for a temporary command. The remote screen sends 1, 2 or 3 presses for 15, 30 or 45 minutes on repeated taps.

The physical controller's frames can fail the CC1101 CRC check (`Crc: 0`) because its CRC differs from the CC1101's. The app treats that as non-fatal, and the Ducobox accepts the synthesised frames.

## Caveats

- The Ducobox only accepts traffic from its paired network. The learned template keeps the network ID and addresses of your own remote, so synthesised commands use the right identity.
- Setup accepts only remote-to-box frames (destination address 1). This avoids learning the Ducobox's own `0x22` confirmation frames when a button is pressed.
- The app sends one frame per tap or hold. The 4-bit counter is incremented for every send; the box accepted this during testing.
- 868.3 MHz transmission must be permitted in your region. `furi_hal_subghz_tx()` enforces the Flipper region setting and the app reports a failure if transmission is blocked.
- This is an amateur learning project. Check the local rules for short-range devices before transmitting.

## Sources

- [Ducobox-ESPEasy-Plugin](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin) — reverse-engineered Duco RF gateway and the main source for the CC1101 register set, packet layout, command `0x22`, mode values and counter semantics. The [`DucoCC1101.cpp`](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/blob/master/lib/Duco/DucoCC1101.cpp) and [`_P150_DucoRFGateway.ino`](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/blob/master/_P150_DucoRFGateway.ino) files were used directly.
- [Duco RF protocol wiki](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/RF-Protocol) — frame-format diagrams for the Duco packet.
- [Duco RF gateway configuration wiki](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/Configureren---RF-gateway) — `BUTTON_PRESSES` 1/2/3 = 15/30/45 minutes and the ventilation mode names.
- [Boxicons](https://github.com/atisawd/boxicons) — source of the application icon, adapted to 10x10 and used under the MIT license (see [`ICON.md`](ICON.md)).
- [IthoEcoFanRFT](https://github.com/supersjimmie/IthoEcoFanRFT) — Itho/Duco CC1101 packet foundations that the Duco plugin builds on.
- [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware) — `furi_hal_subghz`, the CC1101 driver, `api_symbols.csv` and the FAP build system. This project builds against the 1.4.3 tree (API 87.1).
- [ufbt](https://pypi.org/project/ufbt/) — Flipper app build tool used by `Makefile` and `sdk`.
- [Flipper Lab web app](https://lab.flipper.net) — demonstrated that the screen can be streamed over USB; `screen` implements the same RPC mechanism.
- [flipperzero-protobuf](https://pypi.org/project/flipperzero-protobuf/) — reference for the RPC screen-stream message layout implemented by `screen`.
- [Flipper developer documentation](https://developer.flipper.net/flipperzero/doxygen/) — FAP, GUI and Sub-GHz API reference used while porting to the SDK.
- [TI CC1101 datasheet](https://www.ti.com/lit/ds/symlink/cc1101.pdf) — GFSK, packet engine, FIFO and register behaviour.
- [DUCO DucoBox Reno](https://www.duco.eu/en/products/mechanical-ventilation/ventilation-units/ducobox-reno) — product documentation. The [installation manual](<https://www.duco.eu/Wes/CDN/1/Attachments/installation-manual-DucoBox-Reno-(en)_639135752278711479.pdf>) specifies 868.3 MHz and 0.4 dBm, and the [user controller manual](<https://www.duco.eu/Wes/CDN/1/Attachments/Installation-Manual-User-Controller-Room-Sensor-(en)_639017389205883763.pdf>) documents the four-button controller.
- Captures from the author's own remote — the Speed 3 pulse and hold frames used to confirm the mode and button-byte encoding.

## Layout

```
 application.fam    FAP manifest
 ducobox.png        10x10 application icon (see ICON.md)
 ducobox_app.c      remote UI, learning and storage
 screen             USB RPC screenshot tool
 duco_radio.c/.h    CC1101 preset and packet RX/TX
 sdk                pinned Flipper SDK installer
 shell.nix          ufbt build environment
 Makefile           build/clean/launch wrappers
 CATALOG.md         Apps Catalog description
 changelog.md       Apps Catalog changelog
 manifest.template.yml  Apps Catalog manifest template (copy to manifest.yml)
 screenshots/       Apps Catalog screenshots
 LICENSE            MIT license
 PUBLISHING.md      Apps Catalog submission steps
```

## License

MIT. See [`LICENSE`](LICENSE). The application icon is based on Boxicons and is covered by the notice in [`ICON.md`](ICON.md).

