# DroidBeacon — Flipper Zero FAP

Emulate Disney Galaxy's Edge park location BLE beacons to trigger reactions
from Droid Depot droids (R-series, BB-series) at home.

> **DBAD: Don't use at Disney** — using this in the parks would interfere
> with other guests' droid experiences. Keep it at home.

## DBAD

Disney has a curated experience. I made this application so you could
play with your own droid at home, not to modify or interfere with the
experience in the park. Please don't use in the park as it may
reduce the enjoyment of others.

---

## How it works

Galaxy's Edge Droid Depot droids use Bluetooth Low Energy to detect park
location beacons and react accordingly — playing sounds, animating, and
changing behavior based on which area of Black Spire Outpost they're in.

This app emulates those beacons using the Flipper Zero's BLE radio, making
your droid think it's in a specific park location.

The beacons use Disney's manufacturer ID `0x0183` with a 6-byte location
payload. All Droid Depot droids react to the same location codes — the droid
itself determines what reaction to perform.

---

## Requirements

| Requirement | Notes |
|---|---|
| Flipper Zero | Any hardware revision |
| Unleashed firmware | Required — uses the extra beacon BLE API |
| Droid Depot droid | R-series or BB-series with BLE support |

---

## Building

```bash
# Clone Unleashed firmware
git clone --recursive https://github.com/DarkFlippers/unleashed-firmware
cd unleashed-firmware

# Drop this repo into applications_user/
cp -r /path/to/droidbeacon applications_user/droidbeacon

# Build
./fbt fap_droidbeacon
```

The compiled `.fap` lands at:
```
build/f7-firmware-D/.extapps/droidbeacon.fap
```

Deploy via [lab.flipper.net](https://lab.flipper.net) — connect over USB,
navigate to `/ext/apps/Bluetooth/` and upload the `.fap`.

---

## Usage

Launch from **Apps → Bluetooth → DroidBeacon**.

1. Press **Next** on the splash screen
2. Select a park location from the menu
3. Hold your Flipper near your droid
4. Press **Stop** or back when done

Your droid should react within a few seconds of the beacon starting.

---

## Location reference

| Location | Description |
|---|---|
| Market | Black Spire Outpost market area |
| Droid Depot | Where droids are built |
| Resistance | Resistance base area |
| Rides | Rise of the Resistance / Millennium Falcon |
| Cantina | Oga's Cantina |
| Dok Ondar / Savi's | Ancient relics and lightsaber workshop |
| First Order | First Order territory |

---

## Beacon format

For reference, the BLE advertisement packet structure:

```
Byte 0:  0x09  — AD length (9 bytes follow)
Byte 1:  0xFF  — AD type: manufacturer specific
Byte 2:  0x83  — Disney manufacturer ID low byte (0x0183 little-endian)
Byte 3:  0x01  — Disney manufacturer ID high byte
Byte 4-9:       Location payload (e.g. 0A 04 01 02 A6 01 for Market)
```

Beacon codes sourced from community research on r/GalaxysEdge.

---

## Notes

- Hold the Flipper within a meter of your droid for best results
- The beacon broadcasts continuously until you press Stop or go back
- Each location triggers different droid reactions — experiment!
- Works with R2-series, R-series, and BB-series Droid Depot droids

---

## License

Do what you want with it. Based on public community research into Galaxy's
Edge droid BLE beacon codes. No affiliation with Disney.

