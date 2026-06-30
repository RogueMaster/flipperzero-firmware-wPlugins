# Raspberry Pi Pico Example

## Overview

This example demonstrates how to flash an Espressif SoC (target) from a Raspberry Pi Pico Series MCU (host) using `esp_serial_flasher`.
`uart1` is dedicated for communication with the Espressif SoC, whereas, either `uart0` or USB can be used for debug output.

The following steps are performed in order to re-program the targets memory:

1. Peripherals are initialized.
2. Host puts the slave device into boot mode and tries to connect by calling `esp_loader_connect()`.
3. Then `esp_loader_flash_start()` is called to enter the flashing mode and erase the amount of memory to be flashed.
4. The `esp_loader_flash_write()` function is called repeatedly until the whole binary image is transferred.

In addition to the steps mentioned above, `esp_loader_change_transmission_rate()` is called after connection is established in order to increase the flashing speed.
The bootloader is also capable of detecting the baud rate during connection phase, however, it is recommended to start at lower speed and then use dedicated command to increase the baud rate.
This does not apply for the ESP8266, as its bootloader does not support this command, therefore, baud rate can only be changed before the connection phase in this case.

## Hardware Required

* A Raspberry Pi Pico Board with 2MB of flash or more.
* A development board with an Espressif SoC (e.g. ESP-WROVER-KIT, ESP32-DevKitC, etc.).
* Jumper cables to connect the boards.
* One or two USB cables for power supply and programming.

## Hardware Connection

The table below shows the connection between the Raspberry Pi Pico board and the ESP32.

| Pi Pico (host) | ESP32 (slave) |
|:--------------:|:-------------:|
|       18       |      IO0      |
|       19       |      RST      |
|       20       |      RX0      |
|       21       |      TX0      |

## Build and Flash

First, either export the SDK location in your shell:

```bash
export PICO_SDK_PATH=<sdk_location>
```

or set the `PICO_SDK_FETCH_FROM_GIT` CMake variable for the SDK to be pulled from GitHub.

Create and navigate to the example `build` directory:

```bash
mkdir build && cd build
```

Run CMake (with appropriate parameters) and build:

```bash
cmake .. && cmake --build .
```

> [!NOTE]
> CMake 3.13 or later is required.

Binaries to be flashed are placed in the `binaries.c` file for each possible target and converted to C arrays. Flash integrity verification is enabled by default.

For more details regarding `esp_serial_flasher` configuration and Raspberry Pi Pico support, please refer to the top level [README.md](../../README.md).

Finally, flash the board by connecting your Raspberry Pi Pico board to your PC, with the `BOOTSEL` button pressed and copy over the `.uf2` file from the example `build` directory to the Mass Storage Device the Pico presents itself as.

To see the debug output, connect to the boards virtual com port with a serial monitor (e.g. using minicom):

```bash
minicom -b 115200 -o -D /dev/ttyACM0
```

Here is the example's console output:

```text
Connected to target
Transmission rate changed changed
Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Done!
```
