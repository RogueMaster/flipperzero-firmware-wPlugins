# Troubleshooting

Common failure modes and what they actually mean. If none of this helps, open an issue
with your board, firmware version, Board Mode, and what the screen said.

## The app won't load

**"App is old" / "API mismatch" / the app refuses to start.** Almost always the wrong
download rather than an outdated release. A `.fap` records the API version it was built
against, and the loader refuses one that does not match your firmware. Releases carry one
file per firmware family:

| Your firmware | Download | API |
|---|---|---|
| Official (OFW) | `flipdeflock.fap` | 87.1 |
| Momentum | `flipdeflock-momentum.fap` | 87.1 |
| Unleashed | `flipdeflock-unleashed.fap` | 88.3 |
| RogueMaster | `deflock.fap` | 88.3 |

"App is old" specifically means the app's API is BELOW the firmware's, which is what you
get installing the OFW file on Unleashed or RogueMaster. The opposite message, asking you
to update the firmware, means the app's API is ahead of it.

If your firmware is not in the table, or it has bumped its API since the last release
here, build from source with `ufbt`. See
[Build from source](../README.md#build-from-source).

**The app crashes or won't open after a flash-heavy session.** The `.fap` loads entirely
into the Flipper's ~256 KB of shared RAM. Reboot the Flipper to reclaim heap.

## No detections / empty screens

**"UART busy — check port".** Something else already owns the serial port — most often
GPS and the ESP32 are configured on the *same* port. They are meant to run on different
ones:

| Device | Flipper port | Pins |
|--------|--------------|------|
| ESP32  | USART  | 13 (TX) / 14 (RX), 3V3, GND |
| GPS    | LPUART | 15 / 16 |

Check **Settings → ESP Port** and **GPS Port**. Both can run at once, but not on the
same UART.

**Nothing appears in BLE Scan, Net Guardian, or Locator.** These three screens
require **Companion** firmware and are blocked in Marauder mode — you should see an
explicit notice saying so. Either flash the companion firmware (**ESP32 Firmware → Flash
a .bin**, no computer needed) or use the screens Marauder mode supports: Flock/ALPR
Detect, Flock Map, and Reports.

**GPS never gets a fix / the badge shows `!PORT`, `!PIN` or `!FW`.** These mean the app knows a fix is
impossible with the current settings, rather than that it is still searching. Almost
always **GPS Port is set to the same UART as the ESP** — one UART cannot serve both, so
move the GPS to the other port (LPUART, pins 15/16 by default) and leave the ESP on
USART (13/14).

If your GPS is a module **on the ESP32 board itself** rather than wired to the Flipper's
header, no pin setting will help: the Flipper cannot see it at all, because the NMEA never
reaches the Flipper's GPIO. Use the companion relay instead — set **Settings → GPS From**
to `ESP32` and **ESP GPS Pin** to the pin your board wires the GPS TX to. The companion
then forwards each sentence over the link it already has.

Notes on the relay:
- It needs **companion firmware v0.52 or newer**. With older firmware nothing is relayed
  and the badge just stays on `GPS` (searching).
- The pin is the ESP GPIO that the GPS module's **TX** connects to. There is no standard,
  so check your board's schematic or silkscreen. To confirm from a serial terminal, send
  `gps` to the companion and it replies `GPSCFG,<on>,<pin>,<baud>`.
- **GPS Baud** applies to both sources; most modules are 9600.

**Flock/ALPR Detect finds nothing at all.** Check in order:

1. Board Mode matches your actual ESP32 firmware (Settings → Board Mode).
2. Baud matches — Settings → ESP Baud. A wrong baud looks exactly like a dead board.
3. TX/RX aren't swapped. This is the single most common wiring mistake.
4. The ESP32 is actually powered (3V3 and GND both connected).

Also worth knowing: detections are deliberately conservative. An OUI-only match reports
as *Possible*, not *Confirmed*. Seeing fewer, better-qualified hits is the intended
behaviour.

**Testing with an OUI emulator? Two identities are supposed to be ignored.** Bench
tools that replay Flock OUIs — [FlockDecoy](https://github.com/JakeSwiz/FlockDecoy) and
similar — emulate `cc:cc:cc` and `f8:a2:d6`. Both were **retracted upstream** as false
positives (`f8:a2:d6` was hitting a Sony Media Player), so FlipDeFlock deliberately does
not match either. If your emulator cycles through them and those two rows never appear,
that is the correct result, not a missed detection. See
[`docs/signatures.md`](signatures.md) for the full retracted list.

A bare OUI on a **beacon** is also ignored on purpose. Flock cameras moved to station
mode around December 2025 — they send probe requests and do not beacon — so an OUI hit
on a beacon is, by construction, some other product on the same silicon. An emulator
that only beacons will produce fewer hits than one that also sends probe requests.

**Still seeing a false positive after updating the app?** Update the **companion
firmware** too. The OUI table and the probe-rate gate are compiled into the ESP32
as well as the app, and the app deliberately trusts the companion for every
confidence rung below *Confirmed* — those depend on probe behaviour it cannot
re-derive on its own. So a board on older firmware keeps reporting a prefix the
app has since dropped. Flash `flipdeflock_companion_esp32wroom.bin` from the same
release as your `.fap`.

## GPS

**No fix / no geotags.** GPS is **off by default** — turn it on in Settings. Then give
the module a clear view of the sky; a cold start outdoors typically takes 30–90 seconds,
and indoors it may never lock. The app will not attach a stale fix to a detection: if
there's no current valid fix, the detection is saved without coordinates rather than with
a wrong one.

**GPS works but nothing geotags.** Check GPS Port (LPUART) and GPS Baud. NMEA sentences
are checksum-verified, so a wrong baud yields silently discarded garbage rather than
visible errors.

## ESP32 flasher

The flasher goes through the ESP32 **ROM loader** (no stub upload), matching the widely
used 0xchocolate ESP Flasher.

**Getting into the bootloader:** hold **BOOT**, tap **RESET**, release BOOT. Connect
retries five times with a pause between attempts, so a fiddly manual entry gets several
chances to latch.

**"VERIFY FAILED (2)"** — error 2 is a *timeout*, not a hash mismatch. Some ESP32 ROMs
never answer the `SPI_FLASH_MD5` query over UART. Every data block was already written
and acknowledged, so this is reported as **"Wrote OK; MD5 n/a — reset ESP + test it"**.
Reset the ESP and test it; it's almost certainly fine.

**"VERIFY FAILED (4)"** — error 4 *is* a genuine MD5 mismatch, meaning corrupted bytes in
transit. Turn **Flash Speed** to **Safe** in Settings and reflash. Fast baud over long or
noisy Flipper↔ESP wiring is the usual cause.

**"Finalize failed (9)"** — a cosmetic quirk of the stubless ROM path. Your firmware
almost certainly wrote fine. Current builds treat it as a soft warning and use the MD5
verify as the real pass/fail gate.

**"INVALID_COMMAND / software loader is resident"** — a flasher stub is stuck on the chip
from a previous attempt. Fully remove power from the ESP32 (not just reset), then re-enter
the bootloader. Current builds never upload a stub for flashing, so this shouldn't recur.

**Backup is slow.** Intentional. Backup reads via the ROM's 64-byte path and forces Safe
(115200) speed regardless of your Flash Speed setting, because reads are integrity-checked
end to end. Flashing keeps your Fast setting since each block is MD5-verified and retried.

**"Low memory" before a flash starts.** The flasher pre-flight found too little heap.
Reboot the Flipper and try again without opening other screens first.

## Reports

**No report file appears.** Reports only include detections you **marked**. Mark them
first, then **Reports → Save Marked → Report**. Files land in
`apps_data/flipdeflock/reports/`.

**Save fails on a full session.** Reports stream row-by-row to the SD card, so memory
isn't usually the limit — check free space on the SD card. If heap is genuinely too
tight, the save fails cleanly rather than crashing, and no empty file is left behind.

## Share to DeFlock

**The QR won't scan.** It encodes a `https://deflock.org/?lat=…&lng=…` deep link and is
rendered entirely offline — the Flipper never opens a connection. A detection with no GPS
fix has no coordinates to share, so it won't produce a QR. Turn GPS on and re-detect.

## Still stuck?

Open an issue with:

- Board and firmware (Flipper firmware + FlipDeFlock version from **About**)
- Board Mode (Companion or Marauder) and your ESP32's firmware
- Settings that differ from defaults (ports, bauds, flash speed)
- The exact on-screen message

Security issues go through [SECURITY.md](../.github/SECURITY.md) instead — please don't file
those publicly.
