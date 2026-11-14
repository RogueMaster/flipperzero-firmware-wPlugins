---
description: How a session finds the Black Magic probe on the WiFi Devboard, how the board is set up, and what it exposes.
---

# Debug probe

A session finds the probe by itself,
over USB first and through `blackmagic.local` when the board has no cable.

## Wi-Fi

Wi-Fi needs one cable fewer:
with the device on its own USB cable, the board runs from the GPIO header,
so skip the board's cable in the attach order and run the session as usual.

For Wi-Fi, put the board on a 2.4 GHz network once, from a phone so the Mac stays online:
join the board's `blackmagic` network, password `iamwitcher`,
open `http://192.168.4.1`, tab WiFi,
set Mode to `STA` with the network's SSID and Pass, press SAVE, then REBOOT.
Holding BOOT for 10 seconds restores the board's own network.

## Firmware

The board needs firmware 0.1.1:
older firmware hung starting USB, so neither USB nor GDB answered.
Flash the checked archive rather than `ufbt devboard_flash`,
which runs a command from an archive it never verifies.
Hold BOOT, press RESET, release BOOT, and the board appears as `ESP32_S2`, here on `/dev/cu.usbmodem01`:

```shell
dir=$(mktemp -d)

curl -fsS \
  -o "$dir/fw.tgz" \
  https://update.flipperzero.one/builds/blackmagic-firmware/0.1.1/blackmagic-firmware-s2-full-0.1.1.tgz

echo "2e63724446004bd359d895d6843bb8f95cc374e27e3e37569b2e035930f845aa  $dir/fw.tgz" \
  | shasum -a 256 -c

tar -xzf "$dir/fw.tgz" \
  -C "$dir" \
  ./bootloader.bin \
  ./blackmagic.bin \
  ./partition-table.bin

.ufbt/toolchain/arm64-darwin/bin/python3 -I -m esptool \
  --chip esp32s2 \
  --port /dev/cu.usbmodem01 \
  --before no_reset \
  --after no_reset \
  write_flash \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x1000 "$dir/bootloader.bin" \
  0x10000 "$dir/blackmagic.bin" \
  0x8000 "$dir/partition-table.bin"
```

Press RESET when it finishes.

## Risks

Take the board off and pull its cable after the session.
Its GDB port and its HTTP API have no password,
and `/api/v1/wifi/get_credentials` hands out the password of the network it joined.
