# Flipper Zero Ethernet Troubleshooter

This is a small helper that lets you connect your Flipper via RJ45 to your network
You can then set the interface-mac and test if you can get an IP via DHCP and the ping addresses.

## Prerequisites

To get started you will need a specific module called the W5500 Lite with a chiop from WHIZnet Co., Ltd.
You can find those modules on AliExpress etc. [Find Module here](https://www.google.com/search?q=WS5500+Ethernet)
The second thing you will need is a proto-board for the Flipper Zero, which you can get [here](https://shop.flipperzero.one/products/proto-boards).

## Assembly

Connect pins from module to flipper using this scheme:

W5500 Module -> Flipper GPIO 

MOSI (MO) -> A7 - 2pin
SCLK (SCK) -> B3 - 5pin
nSS (CS) -> A4 - 4pin
MISO (MI) -> A6 - 3pin
RESET (RST) -> C3 - 7pin
3v3 (VCC) -> 3V3 - 9pin
GND (G) -> GND - 8pin or 11pin



Original App author: @karasevia - https://github.com/karasevia/finik_eth
Improved and updated by: @arag0re & @xMasterX
