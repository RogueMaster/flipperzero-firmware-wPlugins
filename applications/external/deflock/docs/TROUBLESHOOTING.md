# Troubleshooting

Common failure modes and what they actually mean. If none of this helps, open an issue
with your board, firmware version, Board Mode, and what the screen said.

## The app won't load

**"API mismatch" / the app refuses to start.** A firmware update bumped the Flipper API
past the one the release was built against (currently **87.1**). Rebuild from source with
`ufbt`, or wait for a release targeting the new API. See
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

**Nothing appears in WiFi Audit, BLE Scan, Net Guardian, or Locator.** These four screens
require **Companion** firmware and are blocked in Marauder mode — you should see an
explicit notice saying so. Either flash the companion firmware (**ESP32 Firmware → Flash
a .bin**, no computer needed) or use the screens Marauder mode supports: Flock/ALPR
Detect, NFC/RFID Audit, Flock Map, and Reports.

**Flock/ALPR Detect finds nothing at all.** Check in order:

1. Board Mode matches your actual ESP32 firmware (Settings → Board Mode).
2. Baud matches — Settings → ESP Baud. A wrong baud looks exactly like a dead board.
3. TX/RX aren't swapped. This is the single most common wiring mistake.
4. The ESP32 is actually powered (3V3 and GND both connected).

Also worth knowing: detections are deliberately conservative. An OUI-only match reports
as *Possible*, not *Confirmed*. Seeing fewer, better-qualified hits is the intended
behaviour.

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

## NFC

**The MIFARE Classic deep check takes minutes.** Expected — it runs the full SD key
dictionary against every sector. It uses the Flipper's stock
`nfc/assets/mf_classic_dict.nfc`; if that file is missing, it falls back to a much
smaller built-in key list and will find less.

**The verdict looks like it's for the wrong card.** Fixed in v0.43 — the verdict now
binds to the card actually presented. Update if you're on an older build.

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

Security issues go through [SECURITY.md](../SECURITY.md) instead — please don't file
those publicly.
