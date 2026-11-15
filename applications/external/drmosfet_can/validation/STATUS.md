# Release 1.1 validation

- Official release SDK API 88.2: build and APPCHK passed.
- Device SDK API 87.1: build and APPCHK passed. The binary in dist targets API 87.1.
- Nine compiled Cortex-M4 parser tests in Unicorn: passed.
- Installed FAP read back from SD and compared byte for byte: passed.
- Physical device: Flipper Zero, Momentum mntm-012, API 87.1.
- Footer checked using a screen frame captured from the running device: no separator overlaps the text.
- USB CDC second interface enumerates separately. GVRET keepalive, bus count and bus configuration requests return the expected replies.
- MCP2515 reports ready. Physical CAN TX/RX with an external node has not been verified during this release check.
- Launcher icon: 10x10, 1-bit PNG, embedded in application metadata.

## Screenshot

screenshots/bridge.png is a screen frame captured over the device RPC interface. The app switches to dual USB CDC, so the USB-takeover exception in the catalog guidance is used: the actual 128x64 frame is rendered at 4x nearest-neighbor using only black and qFlipper orange (254,138,44). No screen content is fabricated.
