v1.0:
First release.

Reads tyre pressure sensors and shows what they report on the Flipper
screen: sensor id, pressure, temperature and signal level. Reception
starts with the app, so the Flipper is useful on its own.

Thirty-nine protocols, all ported from rtl_433 and all searched at the
same time — no protocol has to be chosen. Both bands the sensors use are
supported, 433.92 MHz and 315 MHz, and both modulations, FSK and OOK,
with a scan that steps between them.

A sensor at rest can be woken with the 125 kHz coil, one pulse or one
every five seconds. Frames also go out over the USB CLI as line delimited
JSON.

Only the Renault group sensor has been verified against real hardware;
every other decoder is checked against rtl_433 itself.
