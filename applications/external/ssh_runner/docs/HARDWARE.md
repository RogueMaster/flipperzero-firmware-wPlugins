# Hardware and Wiring

[← Documentation index](README.md)

## Prototype: Waveshare ESP32-S3-ETH

ESP32-S3R8 (8 MB PSRAM), 16 MB flash, onboard WIZnet W5500 + RJ45, USB-C, TF slot.
Schematic: <https://files.waveshare.com/wiki/ESP32-S3-ETH/ESP32-S3-ETH-Schematic.pdf>.

### W5500 (Fixed by the Board)

| Signal | ESP32-S3 GPIO |
|---|---|
| SCLK | GPIO13 |
| MOSI | GPIO11 |
| MISO | GPIO12 |
| CS | GPIO14 |
| INT | GPIO10 |
| RST | GPIO9 |

Confirmed from two sources that agree:

1. The Waveshare wiki <https://www.waveshare.com/wiki/ESP32-S3-ETH>, "Network port control pin
   description" for the `ETH_DHCP` / `ETH_Web_CAM` demos.
2. The schematic: nets GP9–GP14 each run through a series resistor (R22–R27) to the W5500 and
   go nowhere else.

All of them are Kconfig options (`idf.py menuconfig` > *SSH Runner*), so a custom PCB can
differ. The defaults in `firmware-esp32/main/Kconfig.projbuild` are the values above.
SCLK and MOSI run at drive strength 1 (about 10 mA) rather than the default 2, for softer
edges with no series resistor on SCLK (Kconfig `FES_W5500_SPI_SOFT_EDGES`, on by default).

### Other Onboard Use (from the Schematic and Wiki)

| GPIO | Use | Notes |
|---|---|---|
| 4–7 | TF slot (CS 4, MISO 5, MOSI 6, SCK 7) | leave alone |
| 21 | WS2812 RGB LED | status LED (see "Status LED" below) |
| 19, 20 | native USB (USB-C), used by the USB-Serial-JTAG console | leave alone |
| 0 | BOOT button (strapping) | leave alone |
| 33–37 | header H2 only, but taken by the octal PSRAM on the S3R8 | do not use |
| 43, 44 | U0TXD / U0RXD on header H1 pins 20/19 | free, but the ROM prints the boot log on GPIO43 |
| 1–3, 8, 15, 18, 38–42, 45–48 | camera connector (8 = camera power; 16 can be VSYNC through the R29 option resistor) | free when no camera is fitted |
| 17 | header H1 pin 7 only | free |
| 26 | SPICS1, the PSRAM chip select inside the S3R8 | do not use |

### Status LED

One LED with the usual single-LED patterns, at a low duty cycle (on the custom board it runs on
the Flipper's battery). Which LED depends on the board, a Kconfig choice (`idf.py menuconfig` >
*SSH Runner* > *Board*):

- **Waveshare ESP32-S3-ETH** (the default): the WS2812 on GPIO21, shown dim red.
- **SSH Runner add-on**, rev 0.1 and later (the custom board; build with `sdkconfig.addon`): the
  green LED D201 on GPIO21, lit when the pin is high.

*SSH Runner* > *Status LED* can turn it off for good. The RJ45 LEDs are the W5500's own.

| Pattern | Means |
|---|---|
| one short flash | boot |
| 100 ms flash every second | a link coming up (a WiFi join; or no link up yet and the saved WiFi or the LAN on its way), or an SSH session connecting (until logged in) |
| on for 1 s, then dark | a link came up; dark means all is well |
| two short flashes every 2 s | an SSH session is open (logged in) |
| 5 fast flashes | an error: WiFi join failed, a link lost, an SSH session failed, or the last reset was a crash, watchdog or brownout |

On the Waveshare ESP32-S3-ETH the WS2812 can show a brief red (about 1 s) at a restart: it
latches noise on its data line while GPIO21 floats during the reset, until the firmware starts
and turns it dark. The add-on board's single-colour LED does not latch, so it stays dark.

Console: `led` (mode and pattern now), `led off` / `led auto` (kept in NVS, default `auto`),
`led test` (each pattern for 3 s, also when off). `status` shows the LED line too.
State machine: `firmware-esp32/components/statusled/statusled_sm.c` (host-tested).

### Flipper Link

| Signal | Flipper pin | ESP32-S3 GPIO |
|---|---|---|
| Flipper TX (USART1, PB6) | 13 | GPIO18 (UART1 RX), header H1 pin 10 |
| Flipper RX (USART1, PB7) | 14 | GPIO17 (UART1 TX), header H1 pin 7 |
| GND | 8, 11, 18 | GND (for example H1 pin 8) |

GPIO17 is a pure header pin. GPIO18 is also camera D7, so it is free only while no camera is
plugged into the camera connector, which this project never uses. GPIO43/44 (UART0) were rejected
because the ROM boot log on GPIO43 would reach the Flipper on every reset. This wiring is
verified on hardware; the link runs at up to 921600 baud.

### Power

| Mode | Wiring | Power |
|---|---|---|
| Desk | GND, TX, RX | the board's own USB-C |
| Portable | GND, TX, RX, plus the Flipper's GPIO header pin 9 (3V3) to the board's 3V3 header pin | the Flipper's battery; USB-C unplugged |

Portable mode is bench-tested under full load (WiFi and Ethernet busy at once) with no
brownouts.

> [!WARNING]
> Never connect the board's USB-C while pin 9 is wired: that ties the board's 3V3 regulator to
> the Flipper's 3V3 rail. Remove the pin 9 wire before plugging in USB-C (to flash or to use
> the console). Leave "5V on GPIO" off on the Flipper in both modes.

## See Also

- [Quick Start](QUICKSTART.md): wiring, flashing and the first connection
