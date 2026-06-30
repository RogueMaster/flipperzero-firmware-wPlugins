# Serial Flasher Tests

## Overview

The two kinds of tests are written for serial flasher:

* Qemu tests
* Target tests

## Qemu Tests

Qemu tests use emulated esp32 to test the correctness of the library.

### Installation

Please refer to [building qemu](https://github.com/espressif/qemu) for instructions on how to compile.

QEMU_PATH environment variable pointing to compiled `qemu/build/xtensa-softmmu/qemu-system-xtensa` has to be defined.

```bash
export QEMU_PATH=path_to_qemu-system-xtensa
./run_qemu_test.sh
```

## Target Tests

To install all the necessary tools for running the Build and Target tests just run the following command:

```pip install -r test/requirements_test.txt```

### Build

Each example can be built according its README. To make things simpler, there is a tool to build all Espressif SoC examples with one command called [idf-build-apps](https://docs.espressif.com/projects/idf-build-apps/en/latest/). Before executing the [idf-build-apps](https://docs.espressif.com/projects/idf-build-apps/en/latest/), you need to run export script of [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html). After this, you are able to run the following command to build the examples:

```bash
python -m idf_build_apps build -v -p .
      --recursive
      --exclude ./examples/binaries
      --config "sdkconfig.defaults*"
      --build-dir "build_"@w
      --check-warnings
```

To build examples for other SoCs, please refer to the README of each example.

### Test

Pytest is used to test the built examples. There is a test written for each example. To execute it, just run the following command with replacing target_name and port with your settings:

```bash
pytest --target=<target_name> --port=<port>
```

The examples for Espressif SoC are built for ESP32 and some of them for ESP32-S3 (`esp32_spi_load_ram_example` and `esp32_usb_cdc_acm_example`). Please be aware that tests for ESP32-S3 need to be run separately as `esp32_usb_cdc_acm_example` fails when runnning after `esp32_spi_load_ram_example`.
