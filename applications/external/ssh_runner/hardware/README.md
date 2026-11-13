# SSH Runner Add-on Board

[← Documentation index](../docs/README.md)

<!-- SPDX-License-Identifier: CC-BY-NC-SA-4.0 -->

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

A Flipper Zero GPIO add-on for SSH Runner: an ESP32-S3-WROOM-1-N16R8, a W5500 Ethernet controller
and an RJ45 magjack on one 67.34 × 36 mm 4-layer board that plugs into the Flipper's top header.
It runs from the Flipper's 3.3 V pin; USB-C is for flashing, the console and bench power.

<table>
<tr>
<td width="50%" align="center"><img src="../docs/media/pcb-rev0.1-top.png" width="100%" alt="3D render of the SSH Runner rev 0.1 board from above at an angle: the ESP32-S3 module, the W5500 chip, a USB-C port, an RJ45 jack and two rows of right-angle header pins on black solder mask"></td>
<td width="50%" align="center"><img src="../docs/media/pcb-rev0.1-bottom.png" width="100%" alt="3D render of the SSH Runner rev 0.1 board from below: the SSH Runner name, rev 0.1, the project's GitHub address and a logo in white on black, with the header pins along the bottom edge"></td>
</tr>
</table>

## Revision

**Rev 0.1**, made and brought up: power, boot, WiFi, Ethernet and SSH from Flipper power all
work ([`BRINGUP.md`](BRINGUP.md)). The firmware's pin map is the same as the Waveshare
ESP32-S3-ETH's; build it with `sdkconfig.addon` for the board's status LED
([Hardware and Wiring](../docs/HARDWARE.md)).

**Errata:**

| | Issue | Workaround | Fixed in |
|---|---|---|---|
| E1 | The reverse-mount LED holes are too small for the specified LEDs (D103, D201), so they cannot be assembled | Leave them out of the assembly; hand-fit Dialight 597-6901-607F (white) and 597-6301-607F (green), which fit the holes | rev 0.2 ([`DESIGN.md`](DESIGN.md) §8) |

## Ordering

The ready-made files for JLCPCB are in [`fab/`](fab/): Gerbers and drill files, the BOM and the
pick-and-place file, and the schematic as a PDF. [`CAM.md`](CAM.md) lists the order settings,
above all the **JLC04121H-7628** stack-up that the trace widths are sized for. In short: 4
layers, 1.2 mm, black mask, white silkscreen, top-side assembly including the two headers, and
D103/D201 left out (E1).

## Files

| Path | What |
|---|---|
| [`DESIGN.md`](DESIGN.md) | The design: power path, pin map, Ethernet, layout, review checklist, errata |
| [`CAM.md`](CAM.md) | Making the fabrication files, and the order settings |
| [`BRINGUP.md`](BRINGUP.md) | Bring-up procedure, and the rev 0.1 boards' measured results |
| `fab/` | Fabrication files for rev 0.1 (Gerbers and drill zip, BOM, CPL, schematic PDF) |
| `ssh-runner.kicad_pro`, `ssh-runner.kicad_sch` | KiCad 10 project and root sheet |
| `power.kicad_sch` | USB LDO, TPS2116 power mux, Flipper 3.3 V / 5 V inputs, power LED |
| `mcu.kicad_sch` | ESP32-S3 module, reset and boot, USB-C, status LED, Qwiic and expansion (DNP) |
| `ethernet.kicad_sch` | W5500, crystal, PHY termination, RJ45 magjack |
| `flipper.kicad_sch` | The two Flipper GPIO headers, UART series resistors, DNP reset and boot links |
| `ssh-runner.kicad_pcb` | The board: official outline (mirrored), 4-layer stack-up, placement, routing, pours, art |
| `ssh-runner.kicad_dru` | Design rules inside JLCPCB's 4-layer capabilities |
| `ssh_runner.pretty/`, `fp-lib-table` | Project footprints: the reverse-mount LED, the expansion pad column, and edge-trimmed copies of the module, USB-C and header footprints |
| `art/` | The logo (all rights reserved, see below) and the title font's licence |
| `tools/kicad.sh` | Runs a command in the pinned `kicad/kicad:10.0.7-full` container |
| `tools/gen_sch.py`, `tools/schlib.py` | Draw the schematic from the parts and nets they list, and check an exported netlist against it |
| `tools/gen_board.py`, `tools/place.py`, `tools/title_font.py` | Generated the starting board (outline, stack-up, art), the floorplan, and the title in Orbitron Bold |
| `tools/routes.py`, `tools/route.py` | The hand routes, and the step that applies them with mitres, ground vias, stitching and pours |
| `tools/check_angles.py` | Fails on any track not at 45°, any unmitred bend, acute angles and doubled-back tracks |
| `tools/fit_check.py` | Numeric fit check of the 18 header pins on the Flipper, from Flipper Devices' STEP files |
| `tools/cam.py`, `tools/jlc_rotations.csv` | Gerbers, drill, BOM and CPL for JLCPCB, with checks; rotation corrections for JLC's placement |
| `tools/render.py`, `tools/lcsc_3d.py` | Raytraced renders; fetches the 3D models KiCad's library lacks |
| `ref/README.md` | Where to get Flipper Devices' reference files (not included) |

Every symbol and footprint comes from KiCad's stock libraries, except the project footprints in
`ssh_runner.pretty`.

**Not included:**

- **3D models of the USB-C, RJ45 and reverse-mount LEDs.** KiCad's library has none, and the
  LCSC/EasyEDA models carry no licence that allows redistribution. `tools/lcsc_3d.py fetch` and
  `tools/lcsc_3d.py wrl` download and convert them into `hardware/3d/` for your own use. Without
  them, KiCad's 3D view and the renders show those four parts without bodies. Every other part
  uses KiCad's own 3D library.
- **Flipper Devices' reference files** (outline DXF, drawing, STEP models): see
  [`ref/README.md`](ref/README.md). Only the outline generator and the fit check need them.

## Command Line

Everything runs in the pinned `kicad/kicad:10.0.7-full` container; nothing is installed on the
host. Run these from the repository root with Docker running. Outputs go to `.cache/hardware/`.

```bash
hardware/tools/kicad.sh kicad-cli sch erc --severity-all -o /project/.cache/hardware/erc.rpt ssh-runner.kicad_sch
```

```bash
hardware/tools/kicad.sh kicad-cli pcb drc --schematic-parity --severity-all -o /project/.cache/hardware/drc.rpt ssh-runner.kicad_pcb
```

```bash
hardware/tools/kicad.sh python3 tools/check_angles.py
```

```bash
hardware/tools/kicad.sh python3 tools/render.py
```

`tools/gen_sch.py` draws the schematic and keeps every symbol's UUID, so the board's links to it
survive a rerun. If the schematic is edited by hand in KiCad, the `.kicad_sch` files become the
source and the generator must not run again. Its `check` mode compares an exported netlist with
the parts and nets it lists:

```bash
hardware/tools/kicad.sh sh -c 'kicad-cli sch export netlist -o /project/.cache/hardware/ssh-runner.net ssh-runner.kicad_sch && python3 tools/gen_sch.py check /project/.cache/hardware/ssh-runner.net'
```

To open the design in the KiCad GUI, use KiCad 10.0.7 or a newer 10.0.x and open
`hardware/ssh-runner.kicad_pro`.

## License

The hardware design in `hardware/` is licensed under the
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
([`LICENSE`](LICENSE)). Commercial use of the hardware design or any derivative works is not
permitted without explicit permission from the author. For commercial licensing, contact:
scx@gapp.nthu.edu.tw

What that covers, and what keeps its own licence:

- **Hardware design**, CC BY-NC-SA 4.0: the schematics, the board, the project's own footprint
  (`Pads_1x09_P1.27mm_SMD`), the fabrication files and these documents.
- **Scripts** in `tools/`: GPL-3.0-only, unchanged, as the firmware, the Flipper app and the PC
  tool ([`../LICENSE`](../LICENSE)).
- **Project footprints** in `ssh_runner.pretty/` derived from KiCad's library (the `*_EdgeTrim`
  copies and the reverse-mount LED): CC-BY-SA-4.0, the
  [KiCad libraries' licence](https://www.kicad.org/libraries/license/). KiCad's exception for
  designs covers the board and its fabrication files.
- **Logo** (`art/logo-x.svg` and its copies on the board's layers): all rights reserved
  ([`art/LICENSE.md`](art/LICENSE.md)). You may build and use the board unchanged; remove or
  replace the logo in a modified design.
- **Title font:** Orbitron, SIL Open Font License 1.1 ([`art/Orbitron-OFL.txt`](art/Orbitron-OFL.txt));
  only its outlines are on the board.
