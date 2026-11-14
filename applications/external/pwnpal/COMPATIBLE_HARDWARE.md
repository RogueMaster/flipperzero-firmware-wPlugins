# pwnpal — compatible hardware & firmware

pwnpal is a Flipper Zero app that drives a companion **ESP32 Wi-Fi board**
running a Marauder fork with the `pwnpal` command. The Flipper is the brain
(persona, UI, pcap/wardrive on SD); the ESP32 is the radio (beacon, recon,
capture). You need both.

## Supported boards

| Board | Chip | Status | Notes |
|-------|------|--------|-------|
| **Feberis Pro** (bpmcircuits) | **classic ESP32** (chip_id 0) | ✅ tested | GPS + BT + NeoPixel on board. Ships with stock Marauder — reflash with our build. Rear switch must be set to **ESP32**. |
| **Rabbit Labs EU ESP32-S3 v4** | ESP32-S3 | ✅ community-confirmed | Flash the published `multiboard-s3` build. Needs the Flipper 5V rail, which the app raises automatically since v3.0.1. Reported working in [#2](https://github.com/mrq1911/pwnpal/issues/2). |
| Any board running the pwnpal Marauder fork | ESP32 / S2 / S3 | ⚠️ works, build-it-yourself | The app talks to *any* Marauder that has the `pwnpal` command, over the standard UART. We only publish a **classic-ESP32** firmware image; other chips need a matching build (see below). |
| Official Flipper Wi-Fi devboard | ESP32-S2 | ⚠️ untested | Needs an S2 build of the fork; not currently published. |

**Wiring / link:** ESP32 ↔ Flipper over the standard GPIO USART — **TX 13 / RX 14
@ 115200** (the Marauder default). If the app shows "No ESP32 detected", check
the cable, the rear switch (Feberis → ESP32), and that the board is flashed with
the pwnpal firmware.

## Get the firmware

Two files:

- **`pwnpal.fap`** — the Flipper app → copy to `/ext/apps/GPIO/pwnpal.fap`.
- **`pwnpal-firmware-feberis.bin`** — the ESP32 firmware (Marauder fork +
  `pwnpal`), merged image → flash at address `0x0`.

Where to get them:

- **GitHub Actions:** the [`build-pwnpal`](../../actions/workflows/build-pwnpal.yml)
  workflow builds the classic-ESP32 firmware; download the `pwnpal-feberis-pro`
  artifact. The `.fap` is built with [ufbt](https://pypi.org/project/ufbt/) against
  the Unleashed SDK (API 88.11).
- **Build locally:** see the workflow file for the exact arduino-cli + pinned-library
  recipe; run `pwnpal-marauder/apply_pwnpal.py <marauder-checkout>` to add the
  command, then compile for your board.

## Flash it (on-Flipper, no PC needed)

1. Copy both files to the Flipper SD (qFlipper / mobile app over Bluetooth or USB).
2. Flash the ESP32: **Apps → GPIO → ESP Flasher → Manual Flash**
   - **S3 toggle: OFF** (the Feberis Pro is a classic ESP32, not S3).
   - Custom slot → select `pwnpal-firmware-feberis.bin`, address `0x0`.
   - Leave every other slot empty → **[>] FLASH** → wait for *Done flashing* → reset.
3. Launch **Apps → GPIO → Pwnpal**.

Flashing once gives you full Marauder **and** the `pwnpal` command — the
Marauder companion app keeps working too. To recover a board, reflash its stock
vendor firmware.

## Using it

See the app's on-screen help and `INSTRUCTIONS.txt` in the release. Capture and
deauth are **for networks you own or are explicitly authorized to test** —
educational use only.
