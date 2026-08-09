# ESP Serial Flasher

[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/espressif/esp-serial-flasher/master.svg)](https://results.pre-commit.ci/latest/github/espressif/esp-serial-flasher/master)
[![Component Registry](https://components.espressif.com/components/espressif/esp-serial-flasher/badge.svg)](https://components.espressif.com/components/espressif/esp-serial-flasher)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

ESP Serial Flasher is a portable C library for programming and interacting with Espressif SoCs from other host microcontrollers.

## Overview

This library enables you to program Espressif SoCs from various host platforms using different communication interfaces. It provides a unified API that abstracts the underlying communication protocol, making it easy to integrate ESP device programming into your projects. In this context, the host (flashing/programming device running this library) controls the target (the ESP-series SoC being programmed). It serves a similar purpose to [esptool](https://github.com/espressif/esptool), but is designed for embedded hosts without a PC or Python runtime or on less powerful single board computers.

- **Connection and identification**: Connect to targets, autodetect chip family, read MAC address, retrieve security info.
- **Flash operations**: Write, read, erase, detect flash size, and verify data integrity via MD5.
- **RAM download and execution**: Load binaries to RAM and run them.
- **Registers and control**: Read/write registers, change transmission rate, reset the target.

### Supported Communication Interfaces

- **UART** - Universal asynchronous communication
- **USB CDC ACM** - USB virtual serial port
- **SPI** - Serial Peripheral Interface (RAM download only)
- **SDIO** - Secure Digital Input/Output (experimental)

> [!NOTE]
> SDIO interface is experimental and currently supports only ESP32-C5 and ESP32-C6 as target. It uses [esp-flasher-stub](https://github.com/espressif/esp-flasher-stub) and supports the full stub command protocol over SDIO.

### Supported Host Platforms (device running this library and performing flashing)

- **STM32** microcontrollers
- **ESP32 series** microcontrollers
- **Zephyr OS** compatible devices
- **Raspberry Pi Pico** (RP2040) and **Raspberry Pi Pico 2** (RP2350)
- **Linux** (via UART or USB serial port, with optional GPIO control)

### Supported Target Devices (ESP device being flashed)

|  Target   | UART | SPI | SDIO | USB CDC ACM |
| :-------: | :--: | :-: | :--: | :---------: |
|  ESP8266  |  ✅  | ❌  |  ❌  |     ❌      |
|   ESP32   |  ✅  | ❌  |  🚧  |     ❌      |
| ESP32-S2  |  ✅  | ❌  |  ❌  |     ❌      |
| ESP32-S3  |  ✅  | ✅  |  ❌  |     ✅      |
| ESP32-C2  |  ✅  | ✅  |  ❌  |     ❌      |
| ESP32-C3  |  ✅  | ✅  |  ❌  |     ✅      |
| ESP32-H2  |  ✅  | ✅  |  ❌  |     ✅      |
| ESP32-C6  |  ✅  | ❌  |  ✅  |     ✅      |
| ESP32-C5  |  ✅  | ❌  |  ✅  |     ✅      |
| ESP32-P4  |  ✅  | 🚧  |  ❌  |     ✅      |
| ESP32-C61 |  ✅  | ❌  |  🚧  |     ✅      |

**Legend**: ✅ Supported | ❌ Not supported | 🚧 Under development

### Feature Support by Interface

|             Feature              | UART | USB CDC ACM | SPI | SDIO |
| :------------------------------: | :--: | :---------: | :-: | :--: |
|     Connect (ROM bootloader)     |  ✅  |     ✅      | ✅  |  ✅  |
|        Connect with stub         |  ✅  |     ✅      | ❌  |  ✅  |
|       Secure Download Mode       |  ✅  |     ✅      | ❌  |  ❌  |
|           Flash write            |  ✅  |     ✅      | ❌  |  ✅  |
| Compressed flash write (deflate) |  🔶  |     🔶      | ❌  |  ✅  |
|        Flash read (fast)         |  🔶  |     🔶      | ❌  |  ✅  |
|        Flash read (slow)         |  ✅  |     ✅      | ❌  |  ❌  |
|        Flash erase (chip)        |  ✅  |     ✅      | ❌  |  ✅  |
|       Flash erase (region)       |  ✅  |     ✅      | ❌  |  ✅  |
|         Flash MD5 verify         |  ✅  |     ✅      | ❌  |  ✅  |
|           RAM download           |  ✅  |     ✅      | ✅  |  ✅  |
|        Get security info         |  ✅  |     ✅      | ❌  |  ✅  |
|     Change baud / clock rate     |  ✅  |     ✅      | ❌  |  ❌  |

**Legend**: ✅ Supported | ❌ Not supported | 🔶 Requires connecting with stub (`esp_loader_connect_with_stub()`)

> [!TIP]
> Connecting with stub (`esp_loader_connect_with_stub()`) is recommended over the plain ROM bootloader connection when flash size on the host is not a limiting constraint. The stub unlocks faster flashing speeds (higher baud rates), flash sizes larger than 2 MB, compressed writes (deflate), and fast flash read. SDIO connects through the stub automatically. All supported chips now have a bundled stub. See [Flash Size Footprint](#flash-size-footprint) for the flash overhead introduced by the bundled stubs.

### Public API

- Public headers: [include/esp_loader.h](include/esp_loader.h), [include/esp_loader_io.h](include/esp_loader_io.h), and [include/esp_loader_error.h](include/esp_loader_error.h) define the stable public API of this library.
- Examples and helpers: [examples/common/](examples/common/) contains helper utilities used by the examples; not part of the library API, but can be used as a reference.

### Versioning and Compatibility

This library follows [Semantic Versioning](https://semver.org/). The public API defined in `include/` folder maintains backward compatibility within the same major version — no breaking changes are introduced in minor or patch releases.

Port implementations (under `port/`) are reference implementations that depend on the SDK of the target platform. They are tested against the specific SDK versions listed in the [Platform Setup Guide](docs/platform-setup.md) and do not carry their own semver guarantee. Ports are maintained on a best-effort basis — breaking changes to them are minimized, but are sometimes necessary to stay up to date with upstream platform SDKs. When such an update is needed, it is released as a minor or patch version of this library.

> [!IMPORTANT]
> **Upgrading from v1?** v2 introduces breaking changes to the public API and the port layer. See the [Migration Guide](docs/migration-v1-to-v2.md) for a complete list of changes and step-by-step upgrade instructions.

## Getting Started

### Prerequisites

To use ESP Serial Flasher, you need:

- **[CMake](https://cmake.org/) 3.22 or later** - Build system
- **[Git](https://git-scm.com/)** - For cloning the repository with submodules

#### Platform Setup

Different host platforms require specific setup procedures:

- **[ESP32 series](docs/platform-setup.md#esp-idf-support)**: Works with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v5.5 or later
- **[STM32](docs/platform-setup.md#stm32-setup)**: Requires [STM32 HAL libraries](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html) and [ARM toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain)
- **[Zephyr](docs/platform-setup.md#zephyr-setup)**: Integrates as [Zephyr](https://zephyrproject.org/) module with specific Kconfig options
- **[Raspberry Pi Pico](docs/platform-setup.md#raspberry-pi-pico-setup)**: Uses [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- **[Linux](docs/platform-setup.md#linux-setup)**: For SBCs (Raspberry Pi, BeagleBone, …) and PCs using [libgpiod](https://libgpiod.readthedocs.io/) or DTR/RTS
- **[Custom platforms](docs/supporting-new-platform.md#implementation-steps)**: ESP Serial Flasher can be used as a git submodule with your own custom platform implementation

For detailed setup instructions, see [Platform Setup Guide](docs/platform-setup.md).

For implementing custom platform support, see [Supporting New Platforms Guide](docs/supporting-new-platform.md), particularly the sections on [using ESP Serial Flasher as an external library](docs/supporting-new-platform.md#option-b-using-esp-serial-flasher-as-external-library) and [implementation steps](docs/supporting-new-platform.md#implementation-steps).

### Basic Usage

```c
#include "esp_loader.h"
#include "esp32_port.h"  // replace with the port header for your platform

esp_loader_error_t err;

// 1. Fill in the port struct with hardware parameters
esp32_port_t port = {
    .port.ops          = &esp32_uart_ops,
    .baud_rate         = 115200,
    .uart_port         = UART_NUM_1,
    .uart_rx_pin       = GPIO_NUM_5,
    .uart_tx_pin       = GPIO_NUM_4,
    .reset_pin = GPIO_NUM_25,
    .boot_pin  = GPIO_NUM_26,
};

// 2. Initialize the loader context — binds the protocol and port vtable
esp_loader_t loader;
err = esp_loader_init_serial(&loader, &port.port);
if (err != ESP_LOADER_SUCCESS) return err;

// 3. Connect to the target chip
esp_loader_connect_args_t connect_args = ESP_LOADER_CONNECT_DEFAULT();
err = esp_loader_connect(&loader, &connect_args);
if (err != ESP_LOADER_SUCCESS) return err;

// 4. Flash a binary (example: 64KB at 0x10000)
// Variable holding your binary image. Typical sources:
// - Read from storage (SD card, filesystem, flash)
// - Received over a link (UART/SPI/USB/Wi-Fi) into a RAM buffer
// - Compiled-in C array generated from a .bin
const uint8_t *data = /* pointer to your firmware image buffer */;

esp_loader_flash_cfg_t flash_cfg = {
    .offset     = 0x10000,
    .image_size = 65536,
    .block_size = 4096,
};
err = esp_loader_flash_start(&loader, &flash_cfg);
if (err != ESP_LOADER_SUCCESS) return err;

size_t offset = 0;
while (offset < flash_cfg.image_size) {
    size_t chunk = MIN(flash_cfg.block_size, flash_cfg.image_size - offset);
    err = esp_loader_flash_write(&loader, &flash_cfg, (void *)(data + offset), chunk);
    if (err != ESP_LOADER_SUCCESS) return err;
    offset += chunk;
}

err = esp_loader_flash_finish(&loader, &flash_cfg);
if (err != ESP_LOADER_SUCCESS) return err;
```

### Examples

For complete implementation examples, see the [examples](examples/) directory:

- [ESP32 Example](examples/esp32_example/) - ESP32 family as host
- [STM32 Example](examples/stm32_example/) - STM32 setup guide
- [Linux Example](examples/linux_example/) - Linux host (PC or SBC such as Raspberry Pi)
- [Zephyr Example](examples/zephyr_example/) - Zephyr OS integration
- [Raspberry Pi Pico Example](examples/pi_pico_example/) - RP2040 or RP2350 as host
- [ESF Demo](https://github.com/Dzarda7/esf-demo) - End-to-end demo flashing ESP targets from an embedded host (M5Stack Dial) over USB CDC ACM; includes SD card image selection and on-device progress UI

### Educational Resources

- [esptool documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32/) - Contains most of the information on how the communication with the chip works, what is and is not possible etc.
- [YouTube Tutorial](https://www.youtube.com/watch?v=hYqkOew8y8w) published 9th September 2024 - Comprehensive guide covering library usage, internals, and custom port implementation

## Configuration

ESP Serial Flasher provides several configuration options to customize its behavior. These options are set as **CMake cache variables** (plain CMake builds) or **Kconfig options** (ESP-IDF / Zephyr builds).

### Basic Configuration

The most common configuration options:

```bash
# Set custom retry count
cmake -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=5 ..
```

For complete configuration reference, see [Configuration Documentation](docs/configuration.md).

## Flash Size Footprint

The library bundles pre-built stub binaries for all supported chips directly in the host firmware. The table below shows the approximate flash rodata overhead from those stubs.

| Connection mode                                    | Flash overhead | How                                                                      |
| :------------------------------------------------- | :------------: | :----------------------------------------------------------------------- |
| ROM bootloader only (`esp_loader_connect()`)       |     ~0 KB      | Stubs stripped by linker GC — none of the stub binary data is referenced |
| With stub (`esp_loader_connect_with_stub()`)       |  **+~87 KB**   | All 11 per-chip stub binaries are pulled into flash rodata               |
| SDIO interface (`CONFIG_SERIAL_FLASHER_PORT_SDIO`) |  **+~20 KB**   | Only SDIO-supported esp-flasher-stub binaries are linked                 |

> [!NOTE]
> SDIO uses the same [esp-flasher-stub](https://github.com/espressif/esp-flasher-stub) command implementation as UART/USB stub mode. The SDIO transport handles packet exchange over the SDIO slave window, while command handling stays shared with the standard stub.

### Reducing Stub Flash Usage

When using ESP-IDF or Zephyr, stub data is automatically removed by the linker's dead-code elimination (`--gc-sections`) unless actively referenced:

- **Not using stubs at all** — call only `esp_loader_connect()` and never `esp_loader_connect_with_stub()`. The entire ~87 KB of stub rodata is stripped automatically; no extra configuration is needed.
- **SDIO targets** — only the esp-flasher-stub objects referenced by the SDIO target selection code are linked. Leave `CONFIG_SERIAL_FLASHER_PORT_SDIO` disabled for non-SDIO builds.

For plain CMake builds with linker GC disabled (e.g. static libraries without `--gc-sections`), or when targeting a host where every byte counts, you can exclude the stub sources at the CMake level by removing the stub `.c` files from the sources list when integrating the library as a subdirectory or submodule.

## Hardware Connections

Each communication interface has specific hardware connection requirements and pin configurations. For complete wiring diagrams, pin assignments, and interface-specific setup instructions, see [Hardware Connections Guide](docs/hardware-connections.md).

## Contributing

We welcome contributions! Before starting work on new features or significant changes, please [open an issue](https://github.com/espressif/esp-serial-flasher/issues) to discuss your proposal.

For detailed contribution guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).

### Adding New Platform Support

If you want to add support for a new host platform, see [Supporting New Host Platforms Guide](docs/supporting-new-platform.md).

## License

This project is licensed under the Apache 2.0 License - see the [LICENSE](LICENSE) file for details.

This repository includes precompiled stub binaries from [esp-flasher-stub](https://github.com/espressif/esp-flasher-stub), which are licensed under the Apache 2.0 OR MIT license.

## Known Limitations

The following limitations are currently known:

- Binary image size must be known before flashing
- ESP8266 targets do not support the MD5 verify command without stub; set `skip_verify = true` in `esp_loader_flash_cfg_t` for ESP8266 targets
- SPI interface only supports RAM download operations
- SDIO interface is experimental with limited platform support
- Only one target can be flashed at a time

For additional limitations and current issues, see the [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues) page.
