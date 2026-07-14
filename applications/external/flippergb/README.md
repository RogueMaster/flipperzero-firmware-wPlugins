# FlipGB — Game Boy Emulator for Flipper Zero

A Game Boy (DMG) emulator for the Flipper Zero, based on the
[jgilchrist/gbemu](https://github.com/jgilchrist/gbemu) core, heavily adapted
for microcontrollers. Loads `.gb` ROMs from the SD card, streams ROM banks,
supports battery saves and adaptive frameskip.

---

## Quick Start

1. Copy `dist/flipgb.fap` to your SD card under `SD/apps/Games/`
   (or run `ufbt launch` with the Flipper connected via USB).
2. Copy your `.gb` ROM files anywhere on the SD card (e.g. `SD/gb_roms/`).
3. On the Flipper: **Apps → Games → FlipGB**.
4. Pick a ROM with the file browser. The game starts immediately.
5. To open the emulator menu at any time: **press Up + Down together**.

> Only use ROMs you legally own or free homebrew (e.g. µCity, Tobu Tobu Girl).

---

## Controls

### In game

| Flipper button | Game Boy button |
|---|---|
| **Up / Down / Left / Right** | D-pad |
| **OK (center)** | **A** |
| **Back** | **B** |
| **Up + Down pressed together** | Open the emulator menu |

There is no direct Start/Select button on the Flipper — they are sent from
the emulator menu (see below).

**Why Up+Down for the menu?** A real Game Boy d-pad physically cannot press
opposite directions at the same time, so no game ever reads that combination
— it can never conflict with gameplay. (A long-press on Back was rejected
because Back is B, and many games hold B continuously — e.g. running in
platformers — which would keep popping the menu open.)

### Emulator menu (Up + Down)

Navigate with **Up/Down**, activate with **OK**, close with **Back**.

| Item | What it does |
|---|---|
| **Continue** | Close the menu and resume the game |
| **Press START** | Sends a Start press to the game (pause menus, "PRESS START" screens) and resumes |
| **Press SELECT** | Sends a Select press to the game and resumes |
| **Frameskip** | Change with **Left/Right**: `auto` (recommended), or fixed `0–4`. `auto` shows the skip level currently in use |
| **Sound** | Toggle piezo sound on/off (`n/a` if the speaker is in use by another app) |
| **Save SRAM** | Writes the cartridge battery save (`.sav`) to the SD card immediately |
| **Exit** | Saves SRAM (if the cartridge has a battery) and quits the app |

The game is paused while the menu is open; all buttons are released for the
game so nothing stays "stuck".

---

## Saves

- Games with battery-backed cartridge RAM (Zelda, Pokémon, etc.) are saved to
  a file **next to the ROM**: `MyGame.gb` → `MyGame.gb.sav`.
- Saving happens **automatically on Exit**, and manually via **Save SRAM** in
  the menu (recommended before pulling the battery/USB, since a hard power
  loss cannot auto-save).
- Save states are **not** supported — only real in-game saving, like original
  hardware.

---

## Display

The Game Boy screen (160×144, 4 shades of gray) is downscaled to the
Flipper's 128×64 1-bit LCD:

- Full screen is always visible (no cropping), slightly squashed vertically.
- The 4 shades become ordered-dither patterns: white → lit, light gray → 3/4
  lit, dark gray → 1/4 lit, black → off.

## Sound

The Flipper's speaker is a single-tone piezo — it plays exactly one
frequency at one volume at a time, so the real 4-channel Game Boy mix
cannot be reproduced. Instead, FlipGB emulates the APU **at register level**
(frequencies, CH1 frequency sweep, volume envelopes, length counters,
NR52 power/status) and every frame sends the **dominant voice** to the
piezo:

1. the louder of the two pulse channels (they carry the melody in almost
   every GB soundtrack); the most recently triggered wins ties,
2. otherwise the wave channel (bass lines),
3. otherwise the noise channel, mapped to a short low buzz (percussion).

The result is a monophonic ringtone-style rendition of the game's music
and sound effects (sweeps like Mario's jump work). It can be toggled in
the emulator menu. Since no waveforms are synthesized, the CPU cost is
negligible (one counter per emulated instruction) and RAM cost is ~120
bytes.

## Performance

- The upstream core had a cycle-domain mismatch: the CPU tables count
  machine cycles but the PPU counted them against T-cycle constants, so
  **every frame emulated 4x the hardware-correct amount of CPU work**
  (70224 M-cycles instead of 17556). Unnoticeable on a desktop, a slideshow
  on a 64 MHz Cortex-M4. Fixed — this alone made everything ~4.5x faster.
- While halted (games spend most of each frame in HALT waiting for vblank),
  the CPU steps 4 M-cycles at a time instead of 1, making the idle part of
  the frame cheap.
- **Frameskip `auto`** (default) measures the real cost of each emulated frame
  and skips *rendering* (never emulation) to keep the game running at correct
  speed. Games stay full-speed logically; visible FPS drops instead.
- Fixed frameskip `0–4` is available in the menu if you prefer consistency.
- The PPU only renders the 64 scanlines (out of 144) that survive the
  downscale to the Flipper LCD — ~55% of the per-frame rendering work is
  skipped with zero visual difference.
- Bank-switch heavy games may micro-stutter when a 16 KB bank has to be
  streamed from the SD card (only happens when the ROM doesn't fit in RAM).
- The emulator menu shows the free heap (`NNk free`) so you can see the
  memory headroom of the current game at a glance.

## Compatibility

| Feature | Status |
|---|---|
| Mappers | ROM-only, MBC1, MBC2, MBC3 (no RTC), MBC5 |
| Game Boy Color | Not supported. CGB-only ROMs are rejected with a message; dual-mode ROMs run in DMG mode |
| MBC3 real-time clock | Not emulated (Pokémon Gold/Silver run without the clock) |
| Audio | Monophonic: register-level APU + dominant voice on the piezo (see *Sound* above). No waveform mixing — the hardware physically can't play it |
| Link cable | Not supported |
| ROM size | Any (streamed from SD; small ROMs are fully loaded to RAM) |

---

## Building from source

```sh
pip install ufbt
cd FlipperGB
ufbt          # produces dist/flipgb.fap
ufbt launch   # builds, installs and runs on a connected Flipper
```

### Verifying the emulator core on your PC

The exact core that ships in the FAP can be compiled and tested on a desktop:

```sh
g++ -std=c++17 -O2 -fno-exceptions -fno-rtti -I gb \
    -o hosttest/hosttest hosttest/main.cpp gb/*.cc

# Blargg CPU tests (print Passed/Failed via the serial port):
./hosttest/hosttest path/to/01-special.gb 4000

# ASCII dump of a game frame:
./hosttest/hosttest game.gb 600 --dump-frame

# Trace of what the piezo would play (dominant APU voice per frame):
./hosttest/hosttest game.gb 1500 --dump-audio
```

Current status: **Blargg `cpu_instrs` 11/11 PASS**.

---

## Technical notes (PC core → MCU adaptations)

| Upstream (PC) | This port (Flipper) |
|---|---|
| Whole ROM in RAM (with several transient copies) | 16 KB bank streaming from SD with an adaptive LRU cache; bank 0 resident; when every bank fits, the whole ROM is preloaded into individual 16 KB slots (O(1) switching, SD file closed) |
| — | All ROM-dependent allocations are 16 KB or smaller and are checked against the largest free heap block first: heap fragmentation can never crash the firmware, the app degrades to streaming or shows "Not enough RAM" instead |
| Renders all 144 scanlines | Renders only the 64 scanlines that are actually displayed after the 144→64 downscale (row mask, ~2x faster rendering) |
| PPU counts M-cycles against T-cycle constants (4x too much CPU emulation per frame) | Hardware-correct M-cycle constants (114 per scanline): ~4.5x faster overall |
| DIV register incremented every M-cycle (64x too fast) | Correct 16384 Hz rate (games use DIV for delays and randomness) |
| Framebuffer stores all 144 rows | Flipper build stores only the 64 displayed rows (2.5 KB instead of 5.7 KB) |
| 92 KB framebuffer of 4-byte enums + unused 256 KB background map | Packed 2bpp framebuffer (5.7 KB); dead buffer removed |
| 32 KB WRAM / 16 KB VRAM (CGB provision) | Real DMG sizes: 8 KB / 8 KB |
| Virtual methods on every CPU register access | Devirtualized, inlined registers |
| Heap allocation per tile per scanline in the PPU | Zero-alloc per-tile rendering |
| `std::function` / `std::string` / `ifstream` / exceptions | Function pointers + Flipper Storage API, builds with `-fno-exceptions -fno-rtti` |
| Nintendo boot ROM embedded | Removed; documented post-boot register state instead |
| MBC1 partial (bugs, 512 KB max), no MBC2/MBC5 | MBC1 complete, MBC2/MBC3/MBC5 implemented |
| No joypad interrupt | Added (wakes games waiting in HALT/STOP) |
| No APU at all | Register-level APU (sweep/envelope/length/NR52, proper read-back masks) driving the piezo with the dominant voice |

RAM budget on device (256 KB total, ~140 KB heap; the app binary itself
loads into ~32 KB of that heap): ~19.5 KB emulation state, 16 KB bank 0,
adaptive bank cache (10 KB heap kept in reserve for the system), 0–32 KB
cartridge RAM per game, 4 KB stack. The bank cache is allocated greedily in
independent 16 KB blocks until the reserve would be touched, so any `.gb`
ROM size works: small ROMs end up fully resident, large ones stream through
however many slots fit. Worst case (1 MB ROM + 32 KB battery RAM, e.g.
Pokémon Red/Blue) needs ~78 KB before the first cache slot, which fits the
post-launch heap with room for 1–2 streaming slots.

## License

The emulator core derives from jgilchrist/gbemu — see its upstream license.
No Nintendo code or assets are included in this repository.
