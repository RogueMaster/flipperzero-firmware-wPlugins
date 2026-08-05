# Flock Companion — universal ESP32 firmware

A Wi-Fi sniffer that turns any ESP32 board wired to the Flipper's UART into a
Flock Safety / ALPR camera detector. It does the 802.11 work on-chip and streams
candidate hits to the **FlipDeFlock** Flipper app.

Works on any ESP32 with Wi-Fi: Flipper Wi-Fi Dev Board, ESP32 Marauder boards,
ReksLab Tri-Board, bare WROOM/WROVER DevKitC, Xiao ESP32-S3, and so on. The board
only needs its UART on the Flipper's pins 13 (TX) / 14 (RX).

> Flock / ALPR detection is passive only. No deauth, injection, or jamming. The
> companion also accepts explicit Flipper-triggered Ping and Ring actions for a
> selected validated tracker. Use lawfully and only where you are authorized.
> OUI-only matches are *possible*, not confirmed; verify by eye.

## Two ways to use the Flipper app

You do not have to flash this firmware. The app has two backends:

1. **Companion (this firmware)** — strict, low-noise line protocol. Recommended.
2. **Marauder / Generic** — leave your existing firmware (e.g. ESP32 Marauder)
   in place. The app scrapes MAC/SSID tokens out of whatever the board prints
   and applies the Flock filter on the Flipper. Set *ESP Backend = Marauder/Gen*
   in the app Settings.

Flash this Companion firmware for the cleanest, most reliable results.

## Flash from the Flipper (no computer / no USB)

Best for boards without a USB port (e.g. ReksLab/CaracalDB multi-boards that only
have a microSD slot): the Flipper flashes the ESP32 over its own UART pins.
**If your board has USB, [flash it directly from a computer](#flash-directly-from-a-computer-no-flipper-needed)
instead — it's significantly faster.**

**One file is all you need.** Download **`flipdeflock_companion_esp32wroom.bin`**
from the FlipDeFlock [release](https://github.com/ReconGrunt/FlipDeFlock/releases)
and copy it to the SD card. It is a merged image containing the bootloader,
partition table and app, and it is flashed **whole at offset `0x0`** — you do not
set three separate files or offsets.

Two ways to flash it:

- **FlipDeFlock's built-in flasher** (no extra app): **ESP32 Firmware → Backup
  current FW** first if you want to keep what is on the board, then **Flash
  from SD** and pick the `.bin`. It always writes a single file at `0x0`.
- **ESP Flasher** (lab.flipper.net/apps/esp_flasher), if you already use it:
  flash the same file at offset `0x0`.

Back up before flashing if the board currently runs something you want back
(e.g. Marauder) — restoring is just flashing that backup the same way.

> **Prebuilt image is for the classic ESP32 (WROOM).** For other targets — S3,
> C3, C5 — build from source (below); the prebuilt image will not boot on them.

> **⏱️ Flashing over the Flipper's UART is slow — budget several minutes, not
> seconds.** The Flipper writes the WHOLE image serially with no skip-blank
> optimization, so time scales with file size, not with how much of it is real
> firmware. In **Settings → ESP32 Firmware → Flash Speed**, `Safe 115k` runs at
> 115200 baud; `Fast 921k` raises it to **230400 baud** during the write only
> (the label is aspirational — 230400 is what actually goes over the wire),
> roughly halving the time. **Backups always run at the safe rate**, regardless
> of that setting.
>
> The **`..._esp32c5_EXPERIMENTAL.bin` is ~4 MB** versus **~1.5 MB** for the
> WROOM image — core 3.x's merge step pads the C5 image out to the full flash
> size, even though the real firmware inside it is a similar size to the
> WROOM's (~1.4 MB). Nothing skips that padding on write, so **the C5 image
> takes roughly 2–3× as long to flash** as the number of useful bytes would
> suggest. If a flash looks stalled, check the on-screen progress percentage
> before assuming it hung — this is expected, not a bug (though shrinking that
> padded image is on the list to fix).

<sub>Earlier revisions of this page listed `flock_companion.ino.bootloader.bin`,
`...partitions.bin`, `...ino.bin` and a `flock_companion-merged.bin`. Those
instructions were wrong: the individual files are CI build artifacts rather than
release downloads, and nothing was ever published under the `-merged` name.
Reported by @h00die in
[#4](https://github.com/ReconGrunt/FlipDeFlock/issues/4), now closed. If anything
on this page is still out of date,
[open a new issue](https://github.com/ReconGrunt/FlipDeFlock/issues/new) or
[start a discussion](https://github.com/ReconGrunt/FlipDeFlock/discussions)
rather than replying on that thread.</sub>

## Flash directly from a computer (no Flipper needed)

If your board has USB, skip the Flipper entirely and flash straight from a
PC/Mac/Linux box with `esptool` — it's faster than the UART path above and
doesn't need the app's flasher or ESP Flasher installed.

```sh
pip install esptool

# Classic ESP32 / WROOM
esptool --chip esp32   --port COM5 write_flash 0x0 flipdeflock_companion_esp32wroom.bin

# ESP32-C5 (EXPERIMENTAL -- see the warning above; unverified on real hardware)
esptool --chip esp32c5 --port COM5 write_flash 0x0 flipdeflock_companion_esp32c5_EXPERIMENTAL.bin
```

Replace `COM5` with your board's port (`/dev/ttyUSB0` etc. on Linux/macOS). Some
`esptool` installs expose the command as `esptool.py` instead of `esptool` --
try that if the above isn't found. Both `.bin` files are the same merged,
0x0-flashable images described above; nothing extra to download. If the board
doesn't auto-reset into bootloader mode, hold **BOOT**, tap **RESET**, release
**BOOT**, then run the command.

## Build from source

**Arduino core 2.x and 3.x are both supported.** The sketch compiles on either;
no external libraries are required. (Core 3.x moved the BLE API to Arduino
`String` and changed `BLEScan::start()` to return a pointer — the sketch shims
both, so you do not need to pin a version. Before v0.48 it only built on 2.x,
which broke every fresh install once 3.x became the default.)

Newer chips — **ESP32-C5, C6, H2** — require core **3.x**; they do not exist in
2.x at all.

### ESP32-C5: dual-band (5 GHz) — EXPERIMENTAL, unverified on hardware

The C5 is the first Espressif part with a **5 GHz** radio. A 2.4-only companion
cannot see a Flock uplink on 5 GHz at all, so on a C5 the sweep covers both
bands: 13 channels on 2.4 GHz plus 28 on 5 GHz.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c5:PartitionScheme=huge_app flock_companion
```

Pick the band at runtime over the serial link:

| Command  | Sweep                          |
|----------|--------------------------------|
| `band 2g`  | 13 channels (classic behaviour) |
| `band 5g`  | 28 channels                     |
| `band all` | 41 channels (**default** on a C5) |

The board replies `BAND,<2g|5g|all>,<channels>` with the band actually in force.
On a 2.4-only radio that answer is always `2g`, whatever you asked for.

> **The cost of `all`:** a full sweep is 41 channels instead of 13, so at the
> same 300 ms dwell it takes ~12.3 s instead of ~3.9 s. Any given camera is
> revisited a third as often. Use `band 2g` if you would rather have the fast
> sweep and know your target is on 2.4 GHz.

> **⚠️ Nobody on this project owns a C5.** The dual-band build is
> **compile-verified only** — it has never been run on the chip. It may not
> boot, hop, or detect anything. The release asset is named
> `..._esp32c5_EXPERIMENTAL.bin` so the warning travels with the file. If you
> have a C5, reports are very welcome on the issue tracker.

> **Flash offset differs on the C5:** its bootloader lives at **`0x2000`**, not
> the classic `0x1000` (and the C3's is at `0x0`). Wrong offset means an
> `invalid header` boot-loop. You do not need to care if you flash the merged
> image at `0x0` — the core generates it with the correct per-chip offsets
> already baked in, which is exactly why we publish the merged file.

### Arduino IDE

1. Install the **esp32** board package (Espressif) via Boards Manager.
2. Open `flock_companion/flock_companion.ino`.
3. Select your board (e.g. "ESP32 Dev Module", "XIAO_ESP32S3", "ESP32C5 Dev Module").
4. **Partition Scheme → Huge APP.** Wi-Fi + BLE together overflow the default
   1.3 MB app partition; without this the build fails at the link step.
5. Upload. The Flipper talks to the board at **115200 baud**.

### arduino-cli

```sh
# The core lives in Espressif's own index, not the default Arduino one.
arduino-cli config init --additional-urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Classic ESP32 / WROOM. PartitionScheme=huge_app is required, not optional.
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app flock_companion
arduino-cli upload  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app -p COM5 flock_companion
```

Replace the FQBN/port to match your board. Note the plain
`--fqbn esp32:esp32:esp32` shown in older revisions of this page omitted the
partition scheme and could fail to link.

## Wiring (standard Flipper UART)

| Flipper pin | ESP32 pin |
|-------------|-----------|
| 13 (TX)     | RX0       |
| 14 (RX)     | TX0       |
| 9  (3V3)    | 3V3       |
| 8/11 (GND)  | GND       |

Most Flipper ESP32 add-on boards (Dev Board, Marauder, Tri board) already route
these, so just seat the board. If your board exposes the ESP UART on different
Flipper pins, change **ESP Port** in the app Settings (USART vs LPUART).

Run a GPS module at the same time on **LPUART (pins 15/16)** to geotag finds.

## Line protocol (for reference)

TX (board → Flipper), newline-terminated ASCII:

```
FLOCKCO,1                                  banner/version
S,<frames>,<hits>,<ch>,<deauths>           status ~1 Hz (deauths = last-interval rate)
D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>][,cls=a][,hid=1]   detection
   type: P=probe-req B=beacon R=probe-resp O=other
   conf: 1=possible 2=likely 3=confirmed
   fp  : FNV-1a hash of the probe's IE skeleton (MAC-independent class tell)
   cls : 'a' = SoundThinking acoustic sensor. Absent = ALPR camera.
   hid : AP beaconed with no SSID. Reported, deliberately NOT scored.
BBEGIN                                     BLE scan started
BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1][,sep=1]   one BLE device
   cat   : 0 unknown 1 Flock/Raven 2 AirTag 3 Tile 4 SmartTag 5 FMDN
   mfghex: raw mfg-data hex (Flock 0x09C8 only), for serial decode
   rv=1  : Raven-specific GATT service seen -> positive acoustic-sensor ID
   sep=1 : Apple Find My tracker advertised separated-state payload
BEND                                       BLE scan finished
WBEGIN                                     WiFi audit scan started
W,<bssid>,<rssi>,<ch>,<auth>,<pair>,<grp>,<wps>,<ssid>   one AP
   auth/pair/grp: esp wifi_auth_mode_t / wifi_cipher_type_t ints
WEND,<count>                               WiFi audit scan finished
DA,<bssid>,<ch>                            deauth/disassoc target (attack indicator)
ATK,<kind>,<value>                         active attack-tool signature
   kind: probeflood | beaconflood | blespam
LOC,<rssi>                                 Locator: live RSSI of the active target
ACT,<op>,<status>[,<rssi>]                  explicit tracker action result
   op: PING (one-shot GATT reachability) or RING (non-owner sound request)
G,<nmea>                                   one NMEA sentence from a GPS on THIS board
   Relayed verbatim from the `$` (RMC/GGA/GLL only; commas are part of the
   sentence, not protocol fields). For boards that wire the GPS to the ESP32
   instead of to the Flipper header, where the Flipper cannot see it at all.
   Off until the app sends `gps <rx> [baud]`. The Flipper decodes it with the
   same NMEA parser its own UART path uses -- no coordinate maths on the ESP.
GPSCFG,<on>,<pin>,<baud>                   echo of the relay config
```

All trailing `key=value` fields are optional and order-independent, so an older
Flipper build simply ignores ones it does not know.

RX (Flipper → board): `scan` (WiFi Flock), `flockcombo` (interleaved WiFi+BLE
Flock), `flockwifi`, `wifiscan`, `blescan`, `stop`, `ver`, `ch <1-14>` (0 = hop),
`locate <w|b> <mac> [ch]` (stream `LOC` for one target; `locate off` ends it),
`ble_ping <mac>` (one-shot active GATT reachability check), `ble_ring <mac>`
(separated-state non-owner sound request for an Apple/Find My tracker),
`gps` (report relay state), `gps off`, `gps <rx_pin> [baud]` (relay NMEA from a
GPS wired to this board; RX-only, default 9600. Pins 1 and 3 are refused -- they
carry this very link).

## Credit / data sources

OUI list and detection approach build on the open counter-surveillance work of
`colonelpanichacks/flock-you`, `0xXyc/flock-you-wifi-recon`, and the DeFlock
community (deflock.org). Thanks to the researchers who mapped these signatures.
