# Copy this file to  ports_local.py  (which is gitignored) and set your own two
# Flipper Zero CDC serial ports. Nothing here is committed with real device names.
#
# Find the ports:
#   macOS:  ls /dev/cu.usbmodemflip_*
#   Linux:  ls /dev/serial/by-id/*Flipper*   (or /dev/ttyACM*)
#
# Roles are just conventions for the scripts (they can be swapped freely):
PORT_RX = "/dev/cu.usbmodemflip_YOURDEV1"  # the Flipper used as receiver
PORT_TX = "/dev/cu.usbmodemflip_YOURDEV2"  # the Flipper used as sender
