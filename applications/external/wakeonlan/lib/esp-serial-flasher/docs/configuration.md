# Configuration Reference

This document provides a comprehensive reference for all configuration options available in ESP Serial Flasher.

## Configuration Types

ESP Serial Flasher supports configuration through **CMake cache variables** in regular CMake builds and **Kconfig** in ESP-IDF or Zephyr builds. These options control the behavior and features of the library.

## Configuration Options

### Debug and Diagnostics

#### `SERIAL_FLASHER_LOG_LEVEL`

- **Type**: CMake cache variable (`NONE`, `ERROR`, `WARN`, `INFO`, `DEBUG`, or `0..4`) / Kconfig choice (`CONFIG_SERIAL_FLASHER_LOG_LEVEL_*`)
- **Default**: `2` (WARN)
- **Description**: Sets the minimum log level compiled into the library. Messages below this level are compiled out, including their format strings.

| Value | Level | Includes                                                              |
| ----- | ----- | --------------------------------------------------------------------- |
| `0`   | None  | No library logging                                                    |
| `1`   | Error | Protocol errors and failed target responses                           |
| `2`   | Warn  | Errors plus unexpected but recoverable events                         |
| `3`   | Info  | Connection, target, flash size, write start, and erase start messages |
| `4`   | Debug | Per-command traces and transfer hex dumps                             |

For CMake builds, pass the level name or numeric value directly:

```bash
cmake -DSERIAL_FLASHER_LOG_LEVEL=DEBUG ..
```

For ESP-IDF and Zephyr builds, select one of the `CONFIG_SERIAL_FLASHER_LOG_LEVEL_*` Kconfig choices. Kconfig derives the internal numeric `CONFIG_SERIAL_FLASHER_LOG_LEVEL` value used by the C code. `SERIAL_FLASHER_DEBUG_TRACE` / `CONFIG_SERIAL_FLASHER_DEBUG_TRACE` has been removed.

### Protocol / Interface Selection

Starting from v2, the communication protocol is **selected at runtime** by calling the appropriate per-protocol init function. There are no compile-time interface flags.

```c
esp_loader_t loader;
esp32_port_t port = { .port.ops = &esp32_uart_ops, /* ... */ };
esp_loader_init_serial(&loader, &port.port);   // UART, USB CDC-ACM, …
```

All init functions accept an `esp_loader_port_t *` (the embedded base of a caller-owned port struct):

| Init function              | Interface   | Notes                                          |
| -------------------------- | ----------- | ---------------------------------------------- |
| `esp_loader_init_serial()` | Serial SLIP | UART, USB CDC-ACM, Linux tty; full feature set |
| `esp_loader_init_spi()`    | SPI         | RAM download only                              |
| `esp_loader_init_sdio()`   | SDIO        | Experimental; limited platform support         |

Functions not supported by a given protocol return `ESP_LOADER_ERROR_UNSUPPORTED_FUNC`.

### Port Compilation (ESP-IDF / Kconfig only)

For ESP-IDF builds, you choose which port implementations to compile into the library. Multiple ports can be enabled simultaneously, enabling runtime switching between interfaces in the same firmware image.

#### `CONFIG_SERIAL_FLASHER_PORT_UART`

- **Type**: Kconfig (`bool`)
- **Default**: Enabled (`y`)
- **Description**: Compile the ESP32 UART port (`esp32_port.c`). Exposes `esp32_uart_ops`.

#### `CONFIG_SERIAL_FLASHER_PORT_SPI`

- **Type**: Kconfig (`bool`)
- **Default**: Disabled
- **Description**: Compile the ESP32 SPI port (`esp32_spi_port.c`). Exposes `esp32_spi_ops`. Only supports RAM download operations.

#### `CONFIG_SERIAL_FLASHER_PORT_SDIO`

- **Type**: Kconfig (`bool`)
- **Default**: Disabled
- **Description**: Compile the ESP32 SDIO port (`esp32_sdio_port.c`). Exposes `esp32_sdio_ops`. Experimental; only available on targets with SDIO host support. SDIO uploads `esp-flasher-stub` during connection and supports the full stub command protocol.

#### `CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM`

- **Type**: Kconfig (`bool`)
- **Default**: Disabled
- **Depends on**: `SOC_USB_OTG_SUPPORTED`
- **Description**: Compile the ESP32 USB CDC-ACM port (`esp32_usb_cdc_acm_port.c`). Exposes `esp32_usb_cdc_acm_ops`. Requires the `espressif/usb_host_cdc_acm` managed component.

### Retry and Timing Configuration

#### `SERIAL_FLASHER_WRITE_BLOCK_RETRIES`

- **Type**: CMake cache variable / Kconfig (`CONFIG_SERIAL_FLASHER_WRITE_BLOCK_RETRIES`)
- **Default**: 3 (used when the macro is not defined at compile time; the library falls back to 3 in `esp_loader.c`)
- **Description**: Number of retry attempts for writing blocks to target flash or RAM.

#### `SERIAL_FLASHER_RESET_HOLD_TIME_MS`

- **Type**: CMake cache variable
- **Default**: 100 (milliseconds)
- **Description**: Duration for which the reset pin is asserted during a hard reset.

#### `SERIAL_FLASHER_BOOT_HOLD_TIME_MS`

- **Type**: CMake cache variable
- **Default**: 50 (milliseconds)
- **Description**: Duration for which the boot pin is asserted during a hard reset.

### GPIO Pin Inversion

#### `SERIAL_FLASHER_RESET_INVERT`

- **Type**: CMake cache variable
- **Default**: Disabled
- **Description**: Invert the output of the reset GPIO pin.
- **Use case**: Useful when hardware has an inverting connection between host and target reset pin.
- **Availability**: UART interface only.

#### `SERIAL_FLASHER_BOOT_INVERT`

- **Type**: CMake cache variable
- **Default**: Disabled
- **Description**: Invert the output of the boot GPIO pin.
- **Use case**: Useful when hardware has an inverting connection between host and target boot pin.
- **Availability**: UART interface only.

## Setting Configuration Options

Configuration options can be set when running CMake using the `-D` flag:

```bash
cmake -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=5 .. && cmake --build .
```

### Multiple Options Example

```bash
cmake \
  -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=5 \
  -DSERIAL_FLASHER_RESET_HOLD_TIME_MS=200 \
  .. && cmake --build .
```

### ESP-IDF and Zephyr Configuration

When using ESP-IDF or Zephyr, configuration options can be set using **Kconfig** instead of CMake flags. Many options have corresponding Kconfig names with the `CONFIG_` prefix:

- `SERIAL_FLASHER_LOG_LEVEL` → `CONFIG_SERIAL_FLASHER_LOG_LEVEL_*` Kconfig choice
- Port compilation → `CONFIG_SERIAL_FLASHER_PORT_UART`, `CONFIG_SERIAL_FLASHER_PORT_SPI`, etc.

You can configure these options using:

**ESP-IDF:**

```bash
idf.py menuconfig
```

**Zephyr:**

```bash
west build -t menuconfig
```

Alternatively, you can set them directly in your `sdkconfig` (ESP-IDF) or `prj.conf` (Zephyr) files:

```ini
# ESP-IDF sdkconfig or Zephyr prj.conf
CONFIG_SERIAL_FLASHER_LOG_LEVEL_DEBUG=y
CONFIG_SERIAL_FLASHER_PORT_UART=y
CONFIG_SERIAL_FLASHER_PORT_SPI=y
```

## Platform-Specific Configuration

Some platforms may have additional configuration requirements. See the platform-specific setup guides:

- [STM32 Setup](platform-setup.md#stm32-setup)
- [Zephyr Setup](platform-setup.md#zephyr-setup)
- [Raspberry Pi Pico Setup](platform-setup.md#raspberry-pi-pico-setup)
