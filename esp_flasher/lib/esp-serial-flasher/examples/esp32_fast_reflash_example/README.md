# Flash Multiple Partitions If MD5 Mismatch Example

## Overview

This example demonstrates how to flash an Espressif SoC (target) from another MCU (host) using `esp_serial_flasher`. The process is optimized by checking the MD5 hash of each partition before flashing and only flashing the binaries if there is a mismatch, indicating that the binary is not already flashed. Two ESP32 chips are used in this case. Binaries to be flashed from the host MCU to the Espressif SoC can be found in [binaries](../binaries/) folder and are converted into C-array during the build process.

The following steps are performed in order to re-program the target's memory:

1. UART1 through which the new binary will be transferred is initialized.
2. The host puts the target device into boot mode and tries to connect by calling `esp_loader_connect()`.
3. The binary file is opened, its MD5 and size is acquired, as it has to be known before flashing.
4. Then `esp_loader_flash_start()` is called to enter the flashing mode and erase the amount of memory to be flashed.
5. `esp_loader_flash_write()` function is called repeatedly until the whole binary image is transferred, but only if the MD5 hash does not match the existing partition.

**Note:** In addition to the steps mentioned above, `esp_loader_change_transmission_rate()` is called after the connection is established in order to increase the flashing speed. This does not apply to the ESP8266, as its bootloader does not support this command. However, the ESP8266 is capable of detecting the baud rate during the connection phase and can be changed before calling `esp_loader_connect()`, if necessary.

## Connection Configuration

In the majority of cases, the `ESP_LOADER_CONNECT_DEFAULT` helper macro is used to initialize the `loader_connect_args_t` data structure passed to `esp_loader_connect()`. This helper macro sets the maximum time to wait for a response and the number of retrials. For more detailed information, refer to the [serial protocol](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/serial-protocol.html).

## Hardware Required

* Two development boards with the ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).
* One or two USB cables for power supply and programming.
* Cables to connect the host to the target according to the table below.

## Hardware Connection

The table below shows the connection between the two ESP32 devices.

| ESP32 (host) | ESP32 (target) |
|:------------:|:-------------:|
|    IO26      |      IO0      |
|    IO25      |     RESET     |
|    IO4       |      RX0      |
|    IO5       |      TX0      |

> [!NOTE]
> The interconnection is the same for ESP32, ESP32-S2, and ESP8266 targets.

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.

## Configuration

For details about available configuration options, please refer to the top-level [README.md](../../README.md).
Compile definitions can be specified on the command line when running `idf.py`, for example:

```bash
idf.py build -DMD5_ENABLED=1
```

Binaries to be flashed are placed in a separate folder (binaries.c) for each possible target and converted to C-array. Without explicitly enabling MD5 check, flash integrity verification is disabled by default.

## Example Output

Here is the example's console output:

```text
...
Connected to target
Transmission rate changed.
I (1211) serial_flasher: Bootloader MD5 match, skipping...
I (1221) serial_flasher: Partition table MD5 match, skipping...
I (1401) serial_flasher: Application MD5 match, skipping...
I (1401) serial_flasher: Done!
I (2001) serial_flasher: ********************************************
I (2001) serial_flasher: *** Logs below are print from slave .... ***
I (2001) serial_flasher: ********************************************
Hello world!
```
