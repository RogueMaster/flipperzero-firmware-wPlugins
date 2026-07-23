v0.7.0:
Dump / read-out via crack (from the ftdude findings): new "Dump -> SD" (writes /ext/lgt_dump.hex, sacrifices the first 1 KB, recovers 0x400+) and "Crack" (breaks read protection alone). "Read chip ID" now also shows the lock-independent GUID.

v0.6.3:
BLE: reclaim the serial channel on every connect. The BLE STK500 round-trip (30 20 -> 14 10) is hardware-verified; avrdude flashes and verifies over BLE via a PC-side bridge.

v0.6.0:
New BLE (avrdude) mode: STK500 slave over BLE-serial, a drop-in twin of the USB mode. New menu item with a live RX activity bar.

v0.5.0:
Chip selection in the menu (328P / 168P / 88P): sets the displayed flash size and the avrdude signature, stored on the SD card. Only the 328P is hardware-verified; 168P / 88P share the protocol but are untested.

v0.4.0:
Bilingual UI (English / Deutsch), switchable in the menu and remembered on the SD card.

v0.3.2:
More drawn graphics: DIP chip on detect, drawn pinout, "ISP mode active" screen.

v0.2.3:
USB mode reworked after the official AVR-ISP pattern (hardware-verified).

v0.1.0:
First release: SD flash / verify + chip ID, LGT-SWD core ported from ft2232_lgtisp.
