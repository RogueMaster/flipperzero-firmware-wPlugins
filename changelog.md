# Changelog

## 1.0

First release.

* Wake a machine by sending the magic packet as a UDP broadcast through the ESP32-S2
  WiFi dev board, which the Flipper drives over UART.
* Up to 16 saved targets, each with a name, MAC, broadcast address and port. The MAC is
  entered as six hex bytes, so there is nothing to mistype.
* The packet also goes to the subnet directed broadcast of the board's own network, not
  only to the configured address. Access points differ in which of the two they bridge
  onto the wired segment.
* Wi-Fi setup scans for networks and fills the SSID in from the list. Failures report
  what actually went wrong: network not on the air, key refused, or no answer at all.
* Built in ESP flasher. Dumps the current 4 MB of the board to the SD card, writes the
  companion firmware that ships inside the app, and puts the old image back on request.
  All writes are MD5 verified against the target.
* Bootloader entry and reset are automatic on the official dev board, which routes those
  lines to header pins 7 and 6. Boards that leave them unconnected still need the manual
  BOOT and RESET sequence, and the app says so when it gets no answer.
* The board's LED reports what it is doing: a colour self test at boot, a green blip
  while idle, pulsing blue during a command, green or red on the result.
