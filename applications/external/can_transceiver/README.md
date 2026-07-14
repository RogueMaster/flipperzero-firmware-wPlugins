# Flipper Zero CAN Transceiver App

This application allows a Flipper Zero to use an MCP2515 CAN transceiver to send and receive messages on a standard CAN bus. That includes those found in vehicles.

Using this app, you can:
* Receive CAN messages and their flags (e.g. remote transmit, extended IDs)
* Store up to 1000 received CAN messages
* Transmit arbitrary standard CAN messages

Good for debugging CAN connections when you lack a more advanced, more expensive transceiver like a Kvaser Leaf Light.

## Note

This is my first Flipper Zero application; it runs on FreeRTOS, with its own APIs for graphics, interrupts, DMA, SPI, etc. This may not be up to scratch, and may contain some bugs. However, I still found it very useful at work.

This project was intended to be released eventually, but is not quite complete. There were some features like USB-to-CAN and OBD-II commands that I had planned but never got to implementing, mainly because I didn't critically need them at the time. You will notice a lot of TODOs and FIXMEs.
