# Specter — NFC reader & skimmer bug-sweep

Specter turns your Flipper Zero into a pocket counter-surveillance bug-sweep for active 13.56 MHz NFC readers. It **passively listens** for the RF field that a powered-on reader constantly emits — a hidden card skimmer slipped into a payment terminal, a covert reader behind a door panel, a rogue logger taped under a desk — then tells you where it is, what kind of thing it is, whether the room is clean, and — left on watch — the moment one appears while you are away.

**No extra hardware. It never transmits.** Specter uses the Flipper's onboard NFC chip in field-detect mode, so there is nothing to buy and nothing to plug in.

## What it does

Specter answers four questions, each on its own screen:

- **Sweep — where is it?** An analog-style EMF gauge whose needle rides the field strength in real time, with a peak-hold marker and a hot zone. When a reader's carrier is sensed the screen frames itself in an alarm border and shows a proximity readout (FAINT / NEAR / CLOSE / STRONG). Optional geiger clicks speed up as the field gets stronger, so you can home in on the source without looking at the screen.
- **Fingerprint — what is it?** Classifies the reader's polling cadence as CONTINUOUS, POLLING, or INTERMITTENT with a confidence readout and a logic-analyzer pulse train.
- **Site Survey — is the room clean?** Sweeps over time and returns a CLEAN / TRACE / ACTIVE verdict.
- **Watch — did one show up while I was away?** An unattended monitor that wakes the screen and logs the moment a reader appears, with a running clock, contact count, and peak field.

## Logbook

Every detection can be saved to the SD card, RTC-timestamped, as both a human-readable `logbook.txt` and a spreadsheet-friendly `logbook.csv`.

## How it works

Specter reads the external-field-detected bit on the onboard ST25R3916 NFC chip and samples it continuously, condensing it into a field-strength percentage and timing the carrier's on/off cadence. It is **listen-only** — it detects the reader's field but never emits one of its own.

## Limitations

- **13.56 MHz (HF) only.** Specter cannot sense 125 kHz (LF) readers — the onboard field-detect path exists only on the HF radio.
- It detects an active, powered reader's field. A passive tag or a reader that is switched off produces no field to detect.

MIT licensed. Source, full documentation, and releases: https://github.com/at0m-b0mb/Specter-FlipperZero
