# External BLE plugin

The nRF52840 dongle stack: the serial port, three-wire HCI, L2CAP, ATT, and
the peripheral and central roles built on them.

Built as a `.fal` and shipped inside the app's `.fap`, so none of it occupies
memory until a scene asks for a BLE stack. It reaches back into the app for
the Seos exchange and everything cryptographic, resolved through the app's own
symbol table; keeping the crypto on that side is deliberate, since a plugin
that called mbedTLS would have to carry its own copy of it.

`ufbt` warns that these symbols are not in the firmware's API. That is
expected: they are the app's, and they resolve when the plugin is loaded.
