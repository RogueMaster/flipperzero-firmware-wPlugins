# Fabrication and Assembly (JLCPCB)

[← Hardware](README.md)

How the fabrication files are made, and the order settings they need. The design rationale is
in [`DESIGN.md`](DESIGN.md) (stack-up §5.1, look §5.3, routing §5.5).

## Ready-made Files

[`fab/`](fab/) holds the files for rev 0.1, made from the sources in this folder:

| File | For |
|---|---|
| `ssh-runner-gerbers.zip` | The PCB order: Gerbers for 4 copper layers, mask, paste, silkscreen and outline, plus the Excellon drill files (PTH and NPTH apart) |
| `ssh-runner-bom-jlc.csv` | The assembly BOM: Comment, Designator, Footprint, LCSC Part # |
| `ssh-runner-cpl-jlc.csv` | The pick-and-place file: Designator, Mid X, Mid Y, Layer, Rotation |
| `ssh-runner-schematic.pdf` | The schematic, all five sheets |

Rev 0.1 has one known erratum: the two reverse-mount LEDs do not fit their holes (E1,
[`DESIGN.md`](DESIGN.md) §8). Read it before ordering.

## Making the Files

Docker must be running. From the repository root, with the project's venv (`requirements.txt`):

```bash
venv/bin/python hardware/tools/cam.py --board hardware/ssh-runner.kicad_pcb --sch hardware/ssh-runner.kicad_sch
```

On Windows, use `venv\Scripts\python` and backslashes. `cam.py` runs `kicad-cli` in the pinned
`kicad/kicad:10.0.7-full` container.

The output goes to `.cache/hardware/cam/`, which is replaced on every run:

| File | For |
|---|---|
| `ssh-runner-gerbers.zip`, `ssh-runner-bom-jlc.csv`, `ssh-runner-cpl-jlc.csv` | As in `fab/` above |
| `gerbers/`, `raw/` | The unzipped files, and kicad-cli's raw position file and BOM |

The schematic PDF:

```bash
hardware/tools/kicad.sh kicad-cli sch export pdf -o /project/.cache/hardware/ssh-runner-schematic.pdf ssh-runner.kicad_sch
```

The run fails, and nothing is written as if it were good, when:
- a fitted part has no LCSC number;
- a DNP part would reach either CSV;
- a designator repeats;
- the BOM and the CPL disagree.

For rev 0.1 the run passes:
- **BOM:** 37 lines, 65 parts. **CPL:** 65 placements, all on the top side.
- **Included:** the two THT headers (J401/J402), both buttons (SW201/SW202) and both
  reverse-mount LEDs (D103/D201; see E1).
- **DNP, left out:** D102, J202, J203, R403, R404.
- **Board only, never in the BOM or CPL:** the fiducials FID1–FID3 and the test pad TP104.
- **Top silkscreen:** RESET, BOOT and QWIIC visible; USB and ETH under their connectors.
  No reference designators.

**Rotations:** `tools/jlc_rotations.csv` corrects KiCad's rotation to JLC's for each
footprint, on the convention of
[Bouni/kicad-jlcpcb-tools](https://github.com/Bouni/kicad-jlcpcb-tools): each part is placed at
the centre of its soldered pads. Every polarized part was checked in JLC's placement preview
for rev 0.1 (`verified=yes`). Only the passives and the two switches, which are not polarized,
stay unverified. `cam.py` lists unverified placements on every run.

## Order Settings

| Setting | Value |
|---|---|
| Layers | 4 |
| Dimensions | 67.34 × 36 mm |
| Thickness | 1.2 mm (finished 1.12 mm nominal, 1.01–1.23 mm, on this stack-up) |
| **Layer stack-up** | **JLC04121H-7628**, JLC's default free stack-up for 4 layers at 1.2 mm: 1 oz outer, 7628 prepreg 0.2104 mm, 0.6 mm core, 7628 prepreg 0.2104 mm, 1 oz outer. Select it explicitly: the trace widths are sized for it. On JLC04121H-3313 the Ethernet and USB pairs would be far off |
| Impedance control | Not required. If ordered: 100 Ω differential on F (Ethernet) and 90 Ω differential on In2 (USB), with the geometries below |
| Outer / inner copper | 1 oz / 0.5 oz |
| Min via hole | **0.3 mm**, JLC's free tier. Vias are 0.6/0.3 and 0.5/0.3 mm; nothing is smaller. A 0.2 mm hole would add a surcharge, forced four-wire via testing and TG155 material. The rules file (`ssh-runner.kicad_dru`) fails anything below 0.3 mm |
| Solder mask / silkscreen | Black / white (rev 0.1: JLC's matte black) |
| Surface finish | ENIG for flat pads at the W5500's 0.5 mm pitch; lead-free OSP and lead-free HASL also work for these parts. Rev 0.1 was made with OSP |
| Via covering | Tented, or ink-plugged (JLC's 4-layer default). Eleven vias sit within 0.5 mm of the back lettering (§5.5) |
| Mark on PCB | "Remove mark" keeps the art clean; the board has no placeholder for an order number |
| Assembly | Top side only, including the two THT headers J401/J402 |
| Panel / edge rails | Left and right only, or a 1×1 panel with rails as JLC needs for its 7 × 7 cm SMT minimum. The header pins overhang the bottom edge and the RJ45 and USB-C sit on the top edge; C201 and R402 are within about 0.5 mm of the left edge, so that side needs its rail |
| Fiducials | Three global ones on F (FID1–FID3, 1 mm copper, 2 mm mask opening) |

**Impedance:** JLC's impedance calculator for JLC04121H-7628 gives:
- Ethernet: outer differential with mask, L1 over L2, 0.20 mm gap. 100 Ω needs 0.2258 mm.
- USB: inner differential, L3 between L2 and L4, 0.25 mm gap. 90 Ω needs 0.2418 mm.

The board's widths, with estimates calibrated to those two points (§5.5):

| Run | Width / gap | Estimate |
|---|---|---|
| Ethernet at the W5500's 0.5 mm pin pitch | 0.25 / 0.25 mm | ≈ 100 Ω |
| Ethernet TX on its 0.45 mm run | 0.23 / 0.22 mm | ≈ 101 Ω |
| USB on In2 | 0.23 / 0.22 mm | ≈ 91 Ω |

## Parts to Watch

Every part has its LCSC number in the BOM. Check stock in the live quote; these matter most:

| Part | LCSC |
|---|---|
| HY911105AE RJ45 magjack (J301) | C69427 |
| ESP32-S3-WROOM-1-N16R8 (U201) | C2913202 |
| W5500 (U301) | C32843 |
| TPS2116DRLR (U102), TLV75733PDBVR (U101) | C3235557, C485517 |
| Reverse-mount LEDs, white / green (D103 / D201): see E1 | C3646935 / C3646937 |
| 25 MHz crystal, 12 pF load (Y301), Basic | C9006 |

## Before Paying

- Look at every layer in JLC's Gerber viewer.
- Set each option in the table above, the stack-up above all.
- Upload the BOM and CPL, and check the rotation and pin 1 of every polarized part in the
  placement preview. If a correction changes, update `tools/jlc_rotations.csv` and run
  `cam.py` again.
- Decide about D103/D201 (E1): leave them out of the assembly and fit replacements by hand.
