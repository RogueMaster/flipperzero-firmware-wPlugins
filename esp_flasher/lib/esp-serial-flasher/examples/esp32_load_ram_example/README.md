# Load Program Into RAM Example

## Overview

This example demonstrates how to load a program into an Espressif SoC (target) RAM from another MCU (host) using `esp_serial_flasher`. An ESP32 is also used as the host MCU in this case. Binaries to be loaded from the host MCU to the Espressif SoC can be found in [binaries](../binaries/) folder and are converted into C-array during build process.

The following steps are performed in order to re-program targets memory:

1. UART1 through which the new binary will be transfered is initialized.
2. The host puts target device into the boot mode and tries to connect by calling `esp_loader_connect()`.
3. The binary file is opened and its size is acquired, as it has to be known before flashing.
4. Then `esp_loader_mem_start()` is called for each segment in RAM.
5. `esp_loader_mem_finish()` is called with the binary entrypoint, telling the chip to start the uploaded program.
6. UART2 is initialized for the connection to the target.
7. Target output is continually read and printed out.

**Note:** In addition to the steps mentioned above, `esp_loader_change_transmission_rate()`  is called after connection is established in order to increase flashing speed. This does not apply for the ESP8266, as its bootloader does not support this command. However, the ESP8266 is capable of detecting the baud rate during connection phase and can be changed before calling `esp_loader_connect()`, if necessary.

## Connection Configuration

In the majority of cases `ESP_LOADER_CONNECT_DEFAULT` helper macro is used in order to initialize `loader_connect_args_t` data structure passed to `esp_loader_connect()`. Helper macro sets the maximum time to wait for a response and the number of retrials. For more detailed information refer to [serial protocol](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/serial-protocol.html).

## Hardware Required

* Two development boards with the ESP32-series chip (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).

* One or two USB cables for power supply and programming.

* Cables to connect host to target according to table below.

## Hardware Connection

Table below shows connection between two ESP32 devices.

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

Binaries to be flashed are placed in a separate folder (binaries.c) for each possible target and converted to C-array.

## Example Output

Here is the example's console output:

```text
...
Connected to target
Transmission rate changed.
I (1063) serial_ram_loader: Loading app to RAM ...
Start loading
Downloading 13628 bytes at 0x3ffb0000...
Downloading 512 bytes at 0x3ffb3c50...
Downloading 97108 bytes at 0x40080000...
...
Downloading 20 bytes at 0x40097b54...

Finished loading
I (11273) serial_ram_loader: ********************************************
I (11273) serial_ram_loader: *** Logs below are print from slave .... ***
I (11273) serial_ram_loader: ********************************************
Hello world!
Hello world!
Hello world!
Hello world!
Hello world!
Hello world!
Hello world!
...
```
