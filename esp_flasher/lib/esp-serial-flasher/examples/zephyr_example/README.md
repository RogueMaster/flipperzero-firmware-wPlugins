# ESP32 Zephyr Example

## Overview

This sample code demonstrates how to flash an Espressif SoC (target) from another MCU (host) using
`esp_serial_flasher`. In this case, the ESP32 is used as the host MCU.
Binaries to be flashed from the host MCU to another Espressif SoC can be found in [binaries](../binaries/) folder
and are converted into C-array during build process.

## How the Example Works

The example performs the following steps to flash the target device:

1. Initialization: Sets up the required peripherals (GPIO for BOOT and EN pins, and UART)
2. Target Reset: Puts the target into bootloader mode using the BOOT and EN pins
3. Connection: Establishes communication with the target bootloader using esp_loader_connect()
4. Baud Rate Change: Increases the transmission speed for faster flashing (except for ESP8266)
5. Flashing Process:
   - Calls esp_loader_flash_start() to begin flashing and erase the required memory
   - Repeatedly calls esp_loader_flash_write() to transfer the binary
   - Verifies the flash integrity if MD5 checking is enabled

**Note:** In addition, to steps mentioned above, `esp_loader_change_transmission_rate()` is called after connection is established in order to increase the flashing speed. This does not apply for ESP8266, as its bootloader does not support this command. However, ESP8266 is capable of detecting the baud rate during connection phase and can be changed before calling `esp_loader_connect()`, if necessary.

## Hardware Required

- ESP32-DevKitC board or another compatible ESP32 development board as a host
- Any supported Espressif SoC development board as a target
- One or two USB cables for power supply and programming
- Cables to connect the host to the target according to the table below.

## Hardware Connection

Table below shows connection between the two devices:

| ESP32 (host) | Espressif SoC (target) |
|:------------:|:----------------------:|
|    IO26      |           IO0          |
|    IO25      |          RESET         |
|    IO4       |           RX0          |
|    IO5       |           TX0          |

> [!NOTE]
> Pin assignments can be modified in the device tree overlay.

## Prerequisites

Before building the example, you need to set up the Zephyr development environment as stated in [Zephyr's Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html). After this step, you can proceed to building and flashing the example.

## Build and Flash

To run the example, type the following commands:

```bash
# Initialize west workspace if not done already
west init

# Update Zephyr modules
west update

# Build the example with esp-serial-flasher module
west build -p -b <supported board from the boards folder> path/to/esp-serial-flasher/examples/zephyr_example -D ZEPHYR_EXTRA_MODULES=/path/to/esp-serial-flasher

west flash
west espressif monitor
```

> [!NOTE]
> Instead of using the `-D ZEPHYR_EXTRA_MODULES` parameter, you can add the esp-serial-flasher module to your `west.yml` file.

## Configuration

For details about available configuration option, please refer to top level [README.md](../../README.md).
Compile definitions can be specified in `prj.conf` file.

Binaries to be flashed are placed in a separate folder (binaries.c) for each possible target and converted to C-array. Without explicitly enabling MD5 check, flash integrity verification is disabled by default.

## Example Output

Here is the example's console output:

``` text
Running ESP Flasher from Zephyr
Connected to target
Transmission rate changed.
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
********************************************
*** Logs below are print from slave .... ***
********************************************
Hello world!
```
