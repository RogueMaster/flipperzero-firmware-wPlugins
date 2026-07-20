# Flock Companion — universal ESP32 firmware

A Wi-Fi sniffer that turns any ESP32 board wired to the Flipper's UART into a
Flock Safety / ALPR camera detector. It does the 802.11 work on-chip and streams
candidate hits to the **FlipDeFlock** Flipper app.

Works on any ESP32 with Wi-Fi: Flipper Wi-Fi Dev Board, ESP32 Marauder boards,
ReksLab Tri-Board, bare WROOM/WROVER DevKitC, Xiao ESP32-S3, and so on. The board
only needs its UART on the Flipper's pins 13 (TX) / 14 (RX).

> Passive recon only. No deauth, no injection. Use lawfully and only where you
> are authorized. OUI-only matches are *possible*, not confirmed; verify by eye.

## Two ways to use the Flipper app

You do not have to flash this firmware. The app has two backends:

1. **Companion (this firmware)** — strict, low-noise line protocol. Recommended.
2. **Marauder / Generic** — leave your existing firmware (e.g. ESP32 Marauder)
   in place. The app scrapes MAC/SSID tokens out of whatever the board prints
   and applies the Flock filter on the Flipper. Set *ESP Backend = Marauder/Gen*
   in the app Settings.

Flash this Companion firmware for the cleanest, most reliable results.

## Flash with the Flipper ESP Flasher app (no computer / no USB)

Best for boards without a USB port (e.g. ReksLab/CaracalDB multi-boards that only
have a microSD slot): the Flipper flashes the ESP32 over its own UART pins.

1. Install **ESP Flasher** on the Flipper (lab.flipper.net/apps/esp_flasher).
2. Download the prebuilt binaries from the FlipDeFlock **release** (built by CI):
   `flock_companion.ino.bootloader.bin`, `...partitions.bin`, `...ino.bin`
   (or the single `flock_companion-merged.bin`). Copy them to the SD card.
3. Seat the board on the Flipper, open **ESP Flasher**:
   - Either flash the **merged** image at offset `0x0`, or
   - set Bootloader = `...bootloader.bin`, Part Table = `...partitions.bin`,
     FirmwareA/app = `...ino.bin` (offsets 0x1000 / 0x8000 / 0x10000), then **Flash**.
4. Done. The board now runs FlipDeFlock companion. Reflash Marauder anytime the
   same way.

> Built for the classic **ESP32 (WROOM)**. For ESP32-S3 boards (e.g. Xiao S3),
> build from source for `esp32:esp32:XIAO_ESP32S3` instead (below).

## Flash with Arduino IDE (needs a USB-capable board)

1. Install the **esp32** board package (Espressif) via Boards Manager.
2. Open `flock_companion/flock_companion.ino`.
3. Select your board (e.g. "ESP32 Dev Module" / "XIAO_ESP32S3").
4. Tools → set Upload Speed as needed; the **app talks at 115200 baud**.
5. Upload. No external libraries are required.

## Flash with arduino-cli

```sh
arduino-cli core install esp32:esp32
arduino-cli compile --fqbn esp32:esp32:esp32 flock_companion
arduino-cli upload  --fqbn esp32:esp32:esp32 -p COM5 flock_companion
```

Replace the FQBN/port to match your board.

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
S,<frames>,<hits>,<ch>,<deauths>           status ~1 Hz (deauths = last-interval deauth/disassoc rate)
D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>   detection
   type: P=probe-req B=beacon R=probe-resp O=other
   conf: 1=possible 2=likely 3=confirmed
WBEGIN                                      WiFi audit scan started
W,<bssid>,<rssi>,<ch>,<auth>,<pair>,<grp>,<wps>,<ssid>   one AP
   auth/pair/grp: esp wifi_auth_mode_t / wifi_cipher_type_t ints
WEND,<count>                               WiFi audit scan finished
```

RX (Flipper → board): `scan` (WiFi Flock), `flockcombo` (interleaved WiFi+BLE
Flock), `flockwifi`, `wifiscan`, `blescan`, `stop`, `ver`, `ch <1-14>` (0 = hop).

## Credit / data sources

OUI list and detection approach build on the open counter-surveillance work of
`colonelpanichacks/flock-you`, `0xXyc/flock-you-wifi-recon`, and the DeFlock
community (deflock.org). Thanks to the researchers who mapped these signatures.
