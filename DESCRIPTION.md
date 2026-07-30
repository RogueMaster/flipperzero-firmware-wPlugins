Wake a computer from your Flipper.

The Flipper has no network of its own, so this app uses the official ESP32-S2 WiFi dev
board as its network adapter. Pick a saved target, and the board joins your Wi-Fi and
puts a Wake-on-LAN magic packet on the wire.

The board needs a small companion firmware for that, and the app writes it itself. No
computer, no esptool, no cables. Before overwriting anything it can dump the whole 4 MB
of the board's flash to your SD card, so whatever was there before, Marauder or anything
else, goes back exactly as it was whenever you want it.

**What it does**

* Saves up to 16 targets, each with a name, MAC address, broadcast address and port.
  The MAC is entered as six hex bytes, so there is nothing to mistype.
* Scans for Wi-Fi networks and fills the SSID in from the list.
* Sends the packet to the subnet broadcast as well as the address you set, because
  access points differ in which of the two they pass to the wired side.
* Says what actually failed when something does: network not on the air, key refused, no
  answer from the board, or the Flipper's 5V rail giving out.
* Flashes, backs up and restores the ESP over the same header, with every write verified
  by MD5 against the target.

**What you need**

The official Flipper WiFi dev board on the GPIO header. Bootloader entry is automatic on
it. Third party boards work too, but need BOOT and RESET pressed by hand before flashing.

Wake-on-LAN also has to be enabled on the target machine: in the BIOS or UEFI, and in the
network adapter's power settings. On Windows, turn Fast Startup off as well, it leaves
the adapter in a state that ignores magic packets. Wi-Fi adapters generally do not
support this at all, so use the wired MAC address.

Source, protocol notes and licensing: https://github.com/keetsta/wol-flipper
