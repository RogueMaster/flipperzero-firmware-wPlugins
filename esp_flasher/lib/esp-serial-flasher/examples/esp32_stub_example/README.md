# Flashing Multiple Partitions While Using the Flasher Stub Example

## Overview

This example demonstrates how to flash an Espressif SoC from another Espressif SoC using `esp-serial-flasher` and the flasher stub. Binaries to be flashed from the host to the Espressif SoC can be found in the `binaries` folder and are converted into C arrays during build process.

The following steps are performed in order to re-program the target's memory:

1. UART1 through which the new binary will be transferred is initialized.
2. The host puts the slave device into boot mode and tries to connect and then upload the flasher stub by calling `esp_loader_connect_with_stub()`.
3. `esp_loader_flash_start()` is called to enter flashing mode and erase amount of memory to be flashed.
4. `esp_loader_flash_write()` function is called repeatedly until the whole binary image is transferred.

## Hardware Required

* One ESP32-series Espressif SoC for the host
* Any Espressif SoC for the target
* One or two USB cables for power supply and programming.

## Building and Flashing

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Configuration

For details about available configuration options, please refer to top level [README.md](../../README.md).
Compile definitions can be specified on command line when running `idf.py`, for example:

```bash
idf.py build -DMD5_ENABLED=1
```

Binaries to be flashed are placed in separate folder (binaries.c) for each possible target and converted to C-array. Without explicitly enabling MD5 check, flash integrity verification is disabled by default.

## Pulling Newer Stub Binaries

The stub binaries are embedded into `esp_stubs.c` from a fixed release of [esp-flasher-stub](https://github.com/esp-rs/esp-flasher-stub).

If a new [esp-flasher-stub](https://github.com/esp-rs/esp-flasher-stub) version is available, you can obtain binaries by passing a CMake cache variable to `idf.py` like so:

```bash
idf.py -DSERIAL_FLASHER_STUB_PULL_VERSION=<version> build
```

Additionally, it is possible to override the stub sources by either providing a custom url:

```bash
idf.py -DSERIAL_FLASHER_STUB_PULL_SOURCE=<source_url> -DSERIAL_FLASHER_STUB_PULL_VERSION=<version> build
```

where the following schema is required: `<stub_download_url>/v<stub_version>/esp32xx.json`,
or a custom local folder:

```bash
idf.py -DSERIAL_FLASHER_STUB_PULL_OVERRIDE_PATH=<local_folder_path> build
```

containing stub `.json` files.

> [!NOTE]
> If overriding with custom stub sources, please check that the license under which the they are released fits your usecase.

## Example Output

Here is the example's console output:

```text
...
I (541) main_task: Calling app_main()
Connected to target
I (1121) serial_stub_flasher: Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
I (1681) serial_stub_flasher: Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
I (1771) serial_stub_flasher: Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
I (5011) serial_stub_flasher: Done!
I (5095) serial_stub_flasher: ********************************************
I (5095) serial_stub_flasher: *** Logs below are print from slave .... ***
I (5096) serial_stub_flasher: ********************************************
Hello world!
```
