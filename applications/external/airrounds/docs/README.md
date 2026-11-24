# AirRounds

AirRounds runs passive, receive-only Wi-Fi capture sessions on the Espressif
ESP32-S2 Wi-Fi Devboard, controlled from the Flipper Zero. It is a defensive
measurement tool: it shows what frames your own access points expose to a
passive listener.

The public AirRounds firmware is receive-only: its radio stays in a
receive-only mode and it has no transmit call. Capture never transmits.

## Requirements

- Flipper Zero with the Espressif ESP32-S2 Wi-Fi Devboard.
- The AirRounds companion firmware flashed to the Devboard.
- A microSD card in the Devboard for the captured frames.

Close the Flipper USB-to-UART Bridge before opening AirRounds, and keep the
Devboard powered during a session.

## What it does

- **Scan area** lists nearby 2.4 GHz access points by network name.
- **Capture mode** chooses Beacon or EAPOL capture for the next round.
- **Capture** records the approved points. The Devboard writes the frames it
  accepts to its microSD card as a PCAP file with a receipt.
- **About** shows the app version and the board firmware version.

Select one to five access points to capture in a round. Identical network names
receive a number, and the scan screen keeps BSSIDs and channels out of the
interface.

## Board firmware

AirRounds can flash the companion firmware to the Devboard from the app. The
default image is receive-only. The app can also flash an active image, which you
supply on the Devboard microSD card and confirm on screen; that image transmits
a targeted deauthentication frame to test a network you own. The active image is
a separate, opt-in build and is not part of the default distribution.

## Scope

AirRounds captures only the access points you approve for a round, which you own
or have written permission to test. It decrypts nothing and derives no key. Use
it only within that scope, on networks you own or have written permission to
test.
