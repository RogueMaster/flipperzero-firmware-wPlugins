# Contributing

Read `docs/DEVELOPMENT.md` and `docs/PROTOCOL.md` before changing the BLE protocol.
Keep the Flipper and Mac implementations compatible and test framing changes.

Run `swift test` inside `BLEClipboardMac`, build the universal app, and build the
FAP against the pinned SDK. Include the firmware/API, Mac version, keyboard
layout, and a non-sensitive sample when reporting a hardware issue.

Never commit real clipboard data, Bluetooth pairing keys, account credentials,
build caches, virtual environments, or firmware SDK checkouts. Screenshots must
not include user content. New features should keep text transfer explicit and
must not bypass pairing or operating-system security controls.
