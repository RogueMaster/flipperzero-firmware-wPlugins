# Screenshot Provenance

Info and Downloads are genuine qFlipper 1.3.3 Save Screenshot exports from a
Flipper Zero on official firmware 1.5.1, captured on 2026-11-04 from 0.5.3. They are copied
byte-for-byte without resizing, recoloring, cropping or annotations. Both are
512x256 with exactly the qFlipper orange (254,138,44) and black (0,0,0) palette.

Normal BLE Clipboard switches USB to HID, disconnecting qFlipper. To capture
safely, the app supports a read-only --info launch argument. It opens the same
Info/QR renderer but returns before Bluetooth, USB HID or saved-library setup.
OK opens QR; Back returns to Info and then exits. It cannot receive or execute
jobs. The captured FAP was uploaded to a temporary app-owned SD directory.
The owner's installed FAP was read back unchanged before and after capture;
the loader was idle afterward. The saved library and pairing keys were not
replaced or read by the Info-only application. Installation of the new pair
is a separate owner-authorized step, not part of this capture evidence.

The optional host canvas adapter in scripts/catalog_screen produces previews
under ignored dist/catalog-preview. Those are not these catalog screenshots.
Physical Info/QR navigation was checked; no keyboard/mouse output or camera
scan was performed. The exported QR decodes to the exact 0.5.3 release URL.
