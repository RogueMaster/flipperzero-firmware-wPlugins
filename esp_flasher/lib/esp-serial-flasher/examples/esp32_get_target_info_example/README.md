# Get Target Info Example

## Overview

This example demonstrates how to get the target info including the flash chip size, the WIFI MAC address and security info of an Espressif SoC from another (host) MCU using `esp-serial-flasher`. In this case, an ESP32 is used as the host MCU.

The following steps are performed:

1. UART1 through which new binary will be transfered is initialized.
2. The host puts target device into the boot mode and tries to connect by calling `esp_loader_connect()`.
3. The host attempts to read the target flash size and the WIFI MAC and prints them out.
4. The host attempts to read the target security info and prints it out.

**Note:** In addition, to steps mentioned above, `esp_loader_change_transmission_rate()`  is called after connection is established in order to increase communication speed. This does not apply for the ESP8266, as its bootloader does not support this command. However, the ESP8266 is capable of detecting the baud rate during connection phase, and can be changed before calling `esp_loader_connect()`, if necessary.

## Connection Configuration

In the majority of cases `ESP_LOADER_CONNECT_DEFAULT` helper macro is used in order to initialize `loader_connect_args_t` data structure passed to `esp_loader_connect()`. Helper macro sets the maximum time to wait for a response and the number of retrials. For more detailed information refer to [serial protocol](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/serial-protocol.html).

## Hardware Required

* Two development boards with the ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).
* One or two USB cables for power supply and programming.
* Cables to connect host to target according to table below.

## Hardware Connection

Table below shows connection between two ESP32 devices.

| ESP32 (host) | ESP32 (target) |
|:------------:|:--------------:|
|    IO26      |      IO0       |
|    IO25      |     RESET      |
|    IO4       |      RX0       |
|    IO5       |      TX0       |

> [!NOTE]
> Interconnection is the same for ESP32, ESP32-S2 and ESP8266 targets.

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

Here is the example's console output:

```text
...
Connected to target
Transmission rate changed.
I (726) serial_flasher: Target flash size [B]: 4194304
I (726) serial_flasher: Target WIFI MAC:
I (726) serial_flasher: 70 04 1d 77 08 0c 
I (736) serial_flasher: Target Security Information:
I (736) serial_flasher: Target chip: ESP32-C3
I (746) serial_flasher: Eco version number: 3
I (746) serial_flasher: Secure boot: DISABLED
I (756) serial_flasher: Secure boot agressive revoke: DISABLED
I (756) serial_flasher: Flash encryption: DISABLED
I (766) serial_flasher: Secure download mode: DISABLED
I (766) serial_flasher: Secure boot key 0 revoked: FALSE
I (776) serial_flasher: Secure boot key 1 revoked: FALSE
I (786) serial_flasher: Secure boot key 2 revoked: FALSE
I (786) serial_flasher: JTAG access: ENABLED
I (796) serial_flasher: USB access: ENABLED
I (796) serial_flasher: Flash encryption: DISABLED
I (806) serial_flasher: Data cache in UART download mode: ENABLED
I (806) serial_flasher: Instruction cache in UART download mode: ENABLED
I (816) main_task: Returned from app_main()
```
