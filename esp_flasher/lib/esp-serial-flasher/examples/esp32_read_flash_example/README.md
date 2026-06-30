# Read From Target Flash Example

## Overview

This example demonstrates how to read from target flash.

The following steps are performed to demonstrate the flash reading functionality:

1. UART1 through which the the new binary will be transferred is initialized.
2. The host puts the target device into the boot mode and tries to connect by calling `esp_loader_connect()`.
3. `esp_loader_flash_start()` is called to enter the flashing mode and erase the amount of memory to be flashed.
4. `esp_loader_flash_write()` function is called repeatedly until the whole example data is transfered.
5. `esp_loader_flash_read()` is called to read back the data programmed into the target flash
6. Data is compared to verify successful reading

**Note:** In addition to the steps mentioned above, `esp_loader_change_transmission_rate()` is called after the connection is established in order to increase the flashing and reading speed. This does not apply for the ESP8266, as its bootloader does not support this command. However, the ESP8266 is capable of detecting the baud rate during connection phase and can be changed before calling `esp_loader_connect()`, if necessary.

## Connection Configuration

In the majority of cases `ESP_LOADER_CONNECT_DEFAULT` helper macro is used in order to initialize `loader_connect_args_t` data structure passed to `esp_loader_connect()`. Helper macro sets the maximum time to wait for a response and the number of retrials. For more detailed information refer to [serial protocol](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/serial-protocol.html).

## Hardware Required

* Two development boards with the ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).

* One or two USB cables for power supply and programming.

* Cables to connect host to target according to table below.

## Hardware Connection

Table below shows connection between the two ESP32 devices.

| ESP32 (host) | ESP32 (target) |
|:------------:|:-------------:|
|    IO26      |      IO0      |
|    IO25      |     RESET     |
|    IO4       |      RX0      |
|    IO5       |      TX0      |

> [!NOTE]
> Interconnection is the same for ESP32, ESP32-S2 and ESP8266 targets.

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.

## Configuration

For details about available configuration options, please refer to the top level [README.md](../../README.md).
Compile definitions can be specified in the command line when running `idf.py`, for example:

```bash
idf.py build -DMD5_ENABLED=1
```

## Example Output

Here is the example's console output:

```text
...
Connected to target
Transmission rate changed.
I (703) serial_flasher: Loading example data
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (1013) serial_flasher: Flash contents match example data
```
