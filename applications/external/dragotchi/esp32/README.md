# Dragotchi WiFi devboard reporter (ESP32-S2)

**Optional.** Dragotchi hunts the sub-GHz airwaves on its own. If you attach a
Flipper **ESP32-S2 WiFi devboard** running this firmware, hunting also senses
nearby **WiFi access-point density** — a busy area triggers a *signal storm*
that boosts your catches (higher tiers, better egg odds). Without the board the
game plays exactly as before; the board is a pure enrichment.

> The ESP32-S2 has **WiFi only, no Bluetooth**, so the enrichment is WiFi AP
> density (count + strongest RSSI). Scanning is receive-only.

## Protocol

The board continuously transmits the latest reading (~5 lines/second) on its
UART at **115200 baud, 8N1**, while rescanning WiFi on a slow background timer:

```
DRAGO wifi=<ap_count> rssi=<strongest_dbm>
```

A WiFi scan blocks for 2–4 s, so the reporter **decouples scanning from
transmitting** — it re-sends the cached result every ~200 ms and only rescans
every ~5 s. That keeps fresh data on the wire so the Flipper's short listen
window during a Forage always catches a line (transmitting once per scan was too
sparse and got missed most of the time).

The Flipper's expansion USART (pins **13 TX / 14 RX**) is wired by the devboard
to the ESP's UART0 pins (**GPIO43 TX / GPIO44 RX**). `main.py` writes the report
on `UART(1, tx=43, rx=44)`, which reaches the Flipper's RX. The game listens for
~1.5 s during a Forage; if it sees a valid `DRAGO` line it flags the board
present and folds the AP count into the catch roll.

## Flashing (one time)

You need `esptool` and `pyserial` (`pip install esptool` provides both).

1. **Enter download mode** on the devboard: **hold BOOT, tap RST once while
   holding BOOT, then release BOOT.** It enumerates as USB `303a:0002`
   (e.g. `/dev/ttyACM1`). A udev rule can symlink it to `/dev/esp32-dl`.
2. **Flash MicroPython** (downloads the image the first time):
   ```sh
   ./flash.sh /dev/esp32-dl
   ```
   Use the **default baud** (the script does): the S2's native USB is unreliable
   at the 460800 fast-flash rate and can error out mid-erase. If a flash fails,
   just re-enter download mode and re-run — the chip stays in the bootloader.
3. After the hard reset the board comes back as MicroPython on USB `303a:4001`
   (e.g. `/dev/ttyACM1`). **Upload the reporter:**
   ```sh
   ./upload_main.py /dev/ttyACM1
   ```
4. **Verify** — read the USB REPL and you should see the report each second:
   ```sh
   python3 -c "import serial,time; s=serial.Serial('/dev/ttyACM1',115200,timeout=1); \
   s.write(b'\r\x04'); time.sleep(6); print(s.read(s.in_waiting).decode())"
   # DRAGO wifi=20 rssi=-28
   ```

## Using it with the Flipper

Unplug the devboard from USB and **seat it on the Flipper's GPIO header** (the
normal WiFi-devboard position). The Flipper powers it from 3V3; MicroPython runs
`main.py` on boot with no USB attached. Open Dragotchi → **Hunt**: when the board
is heard, the catch opens the animated **Signal Storm** screen (a sweeping radar
that pings each nearby network) and your catch gets the storm boost — better
tiers, a guaranteed floor, WiFi-themed names, and a shot at an exclusive **storm
egg**. Without the board (or before it has booted) you get the normal sub-GHz
catch banner instead.

## Reverting to Marauder

This overwrites Marauder. To restore it, put the board back in download mode and
reflash the Marauder image with `esptool` (the installer `.bin` set from the
Marauder release), then use it from the Marauder Flipper app as before.

## Files

- `main.py` — the reporter (scans WiFi, prints `DRAGO …` on UART1 + USB).
- `flash.sh` — downloads + flashes MicroPython (ESP32_GENERIC_S2 v1.29.0).
- `upload_main.py` — pushes `main.py` to the board over the REPL.
