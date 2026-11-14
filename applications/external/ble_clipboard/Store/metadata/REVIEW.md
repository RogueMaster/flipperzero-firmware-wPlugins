# App Review Instructions Draft - 0.5.2

This requires a physical Flipper Zero running the independent BLE Clipboard
0.5.2 FAP, official firmware 1.5.1/API 88.2, and an authorized USB target.
The device is not included with the Mac download. No cloud login is required.

Do not submit yet. First attach a fresh physical demonstration of this exact
Mac/FAP pair, its pairing prompts, target output, cancellation and release build
identifiers. Older prototype captures do not certify this replacement engine.

1. Open Apps > Bluetooth > BLE Clipboard 0.5.2 on the Flipper.
2. Open the matching Mac app, Connect, choose the Flipper and approve matching
   Bluetooth pairing codes on both devices.
3. Connect USB to a second computer with a disposable plain text document and
   matching US layout initially. No target-side software is required.
4. Select Timestamp, Review & Run, inspect the expanded output, confirm target
   focus and explicitly Run on USB.
5. A literal snippet containing `[ENTER]` must type those characters; only a
   typed key step or an actual newline can emit Enter.
6. Fill Two Fields should use prompted values and one Tab, without submitting.
7. Stop during a long delay or typing. The independent backend cancels the
   job and releases keys while retaining Bluetooth; it never replays on reconnect.

The standard and sandboxed builds share the same macro UI, compiler and v3
client. The companion uses the project's custom BLE service, validator and USB
state machine. No external script engine or vendor submodule is required.
Corresponding source, build instructions, Flipper FAP and checksums must be
available at permanent URLs matching the actual submitted version.

Only public SwiftUI/AppKit/CoreBluetooth APIs are used. No Accessibility grant,
screen capture, root/shell helper, built-in updater or target clipboard API is
used. The app emits keyboard events only on an explicitly authorized USB target.

Local unit/host tests and unsigned archives are not physical evidence or Store
approval. Video URL, tested hardware, signed build, reviewer access/contact and
owner approval remain required before submission.
