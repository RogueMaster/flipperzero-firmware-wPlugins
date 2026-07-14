# KyberWrite — Flipper Zero FAP

Program EM4x05 RFID tubes to act as Disney Galaxy's Edge kyber crystals,
using a Flipper Zero running **Unleashed firmware**.

Converted from the original Proxmark3 Lua script by Spandox.

---

## Requirements

| Requirement | Notes |
|---|---|
| Flipper Zero | Any hardware revision |
| Unleashed firmware | ≥ 0.9x (needs `lfrfid_raw_write_em4x05_word`) |
| EM4x05 tube blank | FDX-B animal chips work; the script re-formats them |
| `fbt` build toolchain | From the Unleashed repo |

The stock Flipper firmware does **not** expose EM4x05 block-write capability.
You must be on Unleashed (or RogueMaster, which forks Unleashed).

---

## Building
(You can just take the fap I compiled in the root and use directly
by placing in the /ext/apps/RFID/kyberwrite.fap or compile it yourself)

```bash
# Clone Unleashed firmware
git clone https://github.com/DarkFlippers/unleashed-firmware
cd unleashed-firmware

# Drop this folder into applications_user/
cp -r /path/to/kyberwrite applications_user/kyberwrite

# Build 
./fbt fap_kyberwrite

# build will be located at
unleashed-firmware/build/f7-firmware-D/.extapps/kyberwrite.fap

# DEPLOY (install)
I use [Flipper Lab](https://lab.flipper.net/archive)
Copy the compiled `.fap` (or the one I compiled at the root) to your Flipper at
/ext/apps/RFID/kyberwrite.fap
```


---

## Usage

**Full Init** — use on a fresh/blank EM4x05 chip (e.g. an FDX-B animal chip).
Writes all 13 data blocks in the correct order (skipping the password block,
writing block 1 last — same logic as the original Proxmark script).

**Quick Change** — use on a chip already formatted as a kyber crystal.
Writes only address 6 (the color/voice selector).  Much faster.

### Flow

```
Main Menu
  ├─ Full Init     ─┐
  └─ Quick Change  ─┴─▶  Color Picker (19 crystals)
                               │
                               ▼
                        Writing (progress)
                               │
                         ┌─────┴──────┐
                       Pass          Fail
                      popup         popup
                         └─────┬──────┘
                               ▼
                          Main Menu
```

### Physical placement

Hold the EM4x05 tube flat against the back of the Flipper, centered over
the RFID coil (the coil is the teardrop-shaped antenna trace near the top).
Keep it still during the write sequence — moving it mid-write will cause a
block-write failure.

---

## Crystal reference

| Code       | Color   | Sith Voice   | Jedi Voice      | Jedi (in Sith mode) |
|------------|---------|--------------|-----------------|---------------------|
| 18003000   | White   | Palpatine    | Ahsoka Tano     | Old Luke            |
| 5E003000   | Red     | Darth Vader  | Yoda            | Qui-Gon, Yoda       |
| 3D003000   | Orange  | —            | —               |                     |
| 7B003000   | Yellow  | Palpatine    | Temple Guard    | Old Luke            |
| 0C803000   | Green   | Palpatine    | Qui-Gon Jinn    | Old Luke, Mace      |
| 4A803000   | Cyan    | —            | —               |                     |
| 29803000   | Blue    | Palpatine    | Old Obi-Wan     | Old Luke            |
| 6F803000   | Purple  | Palpatine    | Mace Windu 1    | Old Luke            |
| 14403000   | White   | Palpatine    | Chirrut Imwe    | Old Luke            |
| 52403000   | Red     | Palpatine    | Yoda            | Young Obi-Wan       |
| 31403000   | Red     | Count Dooku  | Yoda            | Yoda, Old Obi-Wan   |
| 77403000   | Yellow  | Palpatine    | Maz Kanata      | Old Luke            |
| 00C03000   | Green   | Palpatine    | Yoda            | Old Luke            |
| 46C03000   | Red     | Darth Maul   | Yoda            | Bendu, sound fx     |
| 25C03000   | Blue    | Palpatine    | Old Luke        | Old Luke            |
| 63C03000   | Purple  | Palpatine    | Mace Windu 2    | Old Luke, Mace      |
| 3E183000   | Red     | Vader 8-Ball | Yoda            | Mace Windu          |
| 5D183000   | Green   | Palpatine    | Yoda 8-Ball     | Old Luke            |
| 1B183000   | Black   | Snoke        | Yoda            | Old Obi-Wan         |

Data sourced from the community spreadsheet originally linked in the Proxmark script.

---

## Notes & caveats

- **Password block (addr 2) is intentionally skipped.** The Proxmark script
  skips it too. If you need to change the password you'll need to extend the
  write order and add the password word.
- **Block 1 is written last**, matching the original script — writing it
  early causes failures on some chips.
- The 300 ms inter-block delay mirrors the `sleep(1)` in the Lua original but
  is shorter. If you get write failures try increasing `WRITE_DELAY_MS` in
  `kyberwrite_scene_writing.c`.
- Orange (3D003000) and Cyan (4A803000) are listed as having no associated
  voice lines in the community data.

---

## License

Do what you want with it. Based on public community research into Galaxy's
Edge lightsaber electronics. No affiliation with Disney.
