# Migration Guide: v1 → v2

This guide describes the breaking changes introduced in v2 and explains how to update existing code.

## Overview of Breaking Changes

| Area                        | v1                                                                      | v2                                                                                                     |
| --------------------------- | ----------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| Library state               | Global / static variables                                               | Caller-owned `esp_loader_t` context                                                                    |
| Initialization              | Not required                                                            | `esp_loader_init_serial()` / `esp_loader_init_spi()` / etc. required                                   |
| Function signatures         | No context parameter                                                    | First parameter is `esp_loader_t *loader`                                                              |
| Flash / mem operation state | Stored inside `esp_loader_t`                                            | Separate `esp_loader_flash_cfg_t` / `esp_loader_flash_deflate_cfg_t` / `esp_loader_mem_cfg_t` contexts |
| Port layer                  | Free functions (`loader_port_write`, etc.)                              | `esp_loader_port_ops_t` vtable                                                                         |
| Protocol selection          | Compile-time `SERIAL_FLASHER_INTERFACE_*`                               | Call `esp_loader_init_serial()` / `esp_loader_init_spi()` / `esp_loader_init_sdio()`                   |
| Kconfig options             | `SERIAL_FLASHER_INTERFACE_*` choice                                     | `SERIAL_FLASHER_PORT_*` booleans (multiple can be enabled)                                             |
| Error type header           | Defined in `esp_loader.h`                                               | Separate `esp_loader_error.h` (auto-included by `esp_loader.h`)                                        |
| Conditionally-compiled APIs | Many functions guarded by `#ifdef SERIAL_FLASHER_INTERFACE_*`           | All functions always available; return `ESP_LOADER_ERROR_UNSUPPORTED_FUNC` if not applicable           |
| `esp_loader_flash_finish()` | Accepted `reboot` bool; not recommended due to ROM issues               | `reboot` parameter removed; **must now be called** — performs MD5 verification                         |
| Logging                     | Optional `debug_print` string callback and `SERIAL_FLASHER_DEBUG_TRACE` | Levelled `log` / `log_hex` callbacks and `SERIAL_FLASHER_LOG_LEVEL`                                    |
| Raspberry Pi port           | Dedicated `port/raspberry_port.{c,h}` using pigpio                      | Replaced by the generic `port/linux_port.{c,h}` (see below)                                            |

---

## 1. Add `esp_loader_t` Context and Call a Per-Protocol Init Function

The library no longer uses global state. All state is stored in an `esp_loader_t` struct that you allocate and own.

**v1:**

```c
loader_port_esp32_init(&config);

esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
esp_loader_connect(&args);
```

**v2:**

```c
esp32_port_t port = {
    .port.ops          = &esp32_uart_ops,
    .baud_rate         = 115200,
    .uart_port         = UART_NUM_1,
    .uart_rx_pin       = GPIO_NUM_5,
    .uart_tx_pin       = GPIO_NUM_4,
    .reset_pin         = GPIO_NUM_25,
    .boot_pin          = GPIO_NUM_26,
};

esp_loader_t loader;
esp_loader_init_serial(&loader, &port.port);

esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
esp_loader_connect(&loader, &args);
```

The per-protocol init functions (`esp_loader_init_serial`, `esp_loader_init_spi`,
`esp_loader_init_sdio`) bind the protocol and port vtable
to the context and call the port's `init` function automatically. Pass `&port.port`
(the embedded `esp_loader_port_t` base) as the second argument.

> [!NOTE]
> Pin fields in all port structs were renamed for clarity: `gpio0_pin` → `boot_pin`
> (all platforms) and, on the Linux port, `gpio0_trigger_pin` / `reset_trigger_pin`
> → `boot_pin` / `reset_pin`. Update any existing struct initialisers accordingly.

---

## 2. Pass `esp_loader_t *` to Every API Call

All public functions now take the loader context as their first argument.

| v1                                                           | v2                                                                                                                                                                  |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `esp_loader_connect(&args)`                                  | `esp_loader_connect(&loader, &args)`                                                                                                                                |
| `esp_loader_connect_with_stub(&args)`                        | `esp_loader_connect_with_stub(&loader, &args)`                                                                                                                      |
| `esp_loader_connect_secure_download_mode(&args, size, chip)` | `esp_loader_connect_secure_download_mode(&loader, &args, size)` — `chip` parameter removed; target is now always auto-detected via `GET_SECURITY_INFO`              |
| `esp_loader_get_target()`                                    | `esp_loader_get_target(&loader)`                                                                                                                                    |
| `esp_loader_flash_start(offset, size, block)`                | `esp_loader_flash_start(&loader, &flash_cfg)` — see [Section 3](#3-use-operation-context-structs-for-flash-and-ram-operations)                                      |
| `esp_loader_flash_write(buf, size)`                          | `esp_loader_flash_write(&loader, &flash_cfg, buf, size)`                                                                                                            |
| `esp_loader_flash_finish(reboot)`                            | `esp_loader_flash_finish(&loader, &flash_cfg)`                                                                                                                      |
| `esp_loader_flash_deflate_write(buf, size)`                  | `esp_loader_flash_deflate_write(&loader, &defl_cfg, buf, size)`                                                                                                     |
| `esp_loader_flash_deflate_finish(reboot)`                    | `esp_loader_flash_deflate_finish(&loader, &defl_cfg)`                                                                                                               |
| `esp_loader_flash_detect_size(&size)`                        | `esp_loader_flash_detect_size(&loader, &size)`                                                                                                                      |
| `esp_loader_flash_read(buf, addr, len)`                      | `esp_loader_flash_read(&loader, buf, addr, len)`                                                                                                                    |
| `esp_loader_flash_erase()`                                   | `esp_loader_flash_erase(&loader)`                                                                                                                                   |
| `esp_loader_flash_erase_region(offset, size)`                | `esp_loader_flash_erase_region(&loader, offset, size)`                                                                                                              |
| `esp_loader_flash_verify()`                                  | Removed — MD5 verification now runs automatically inside `esp_loader_flash_finish()` by default; set `skip_verify = true` in `esp_loader_flash_cfg_t` to disable it |
| `esp_loader_mem_start(offset, size, block)`                  | `esp_loader_mem_start(&loader, &mem_cfg)` — see [Section 3](#3-use-operation-context-structs-for-flash-and-ram-operations)                                          |
| `esp_loader_mem_write(buf, size)`                            | `esp_loader_mem_write(&loader, &mem_cfg, buf, size)`                                                                                                                |
| `esp_loader_mem_finish(entry)`                               | `esp_loader_mem_finish(&loader, &mem_cfg, entry)`                                                                                                                   |
| `esp_loader_read_mac(&mac)`                                  | `esp_loader_read_mac(&loader, &mac)`                                                                                                                                |
| `esp_loader_write_register(addr, val)`                       | `esp_loader_write_register(&loader, addr, val)`                                                                                                                     |
| `esp_loader_read_register(addr, &val)`                       | `esp_loader_read_register(&loader, addr, &val)`                                                                                                                     |
| `esp_loader_change_transmission_rate(rate)`                  | `esp_loader_change_transmission_rate(&loader, rate)`                                                                                                                |
| `esp_loader_change_transmission_rate_stub(old, new)`         | `esp_loader_change_transmission_rate(&loader, new)` — stub API removed; one function for ROM and stub                                                               |
| `esp_loader_get_security_info(&info)`                        | `esp_loader_get_security_info(&loader, &info)`                                                                                                                      |
| `esp_loader_reset_target()`                                  | `esp_loader_reset_target(&loader)`                                                                                                                                  |

## 3. Use Operation Context Structs for Flash and RAM Operations

Flash, compressed flash, and RAM load operations each now require a dedicated context struct that is allocated by the caller and passed to every function in the operation sequence. This removes per-operation state from `esp_loader_t` (which is now connection-only state) and makes the operation parameters explicit and inspectable.

### Flash write

**v1:**

```c
esp_loader_flash_start(0x10000, image_size, 4096);
esp_loader_flash_write(buf, chunk_size);
esp_loader_flash_finish(false);
```

**v2:**

```c
esp_loader_flash_cfg_t flash_cfg = {
    .offset     = 0x10000,
    .image_size = image_size,
    .block_size = 4096,
};

esp_loader_flash_start(&loader, &flash_cfg);

while (remaining > 0) {
    size_t chunk = MIN(flash_cfg.block_size, remaining);
    esp_loader_flash_write(&loader, &flash_cfg, buf, chunk);
    // ...
}

esp_loader_flash_finish(&loader, &flash_cfg);
```

The `_state` sub-struct inside `esp_loader_flash_cfg_t` is managed entirely by the library. Do not read or write its fields.

### Compressed flash write (deflate)

```c
esp_loader_flash_deflate_cfg_t defl_cfg = {
    .offset          = 0x10000,
    .image_size      = uncompressed_size,
    .compressed_size = compressed_size,
    .block_size      = 4096,
};

esp_loader_flash_deflate_start(&loader, &defl_cfg);

while (remaining > 0) {
    size_t chunk = MIN(defl_cfg.block_size, remaining);
    esp_loader_flash_deflate_write(&loader, &defl_cfg, buf, chunk);
    // ...
}

esp_loader_flash_deflate_finish(&loader, &defl_cfg);
```

### RAM load

```c
esp_loader_mem_cfg_t mem_cfg = {
    .offset     = segment_addr,
    .size       = segment_size,
    .block_size = 0x1800,
};

esp_loader_mem_start(&loader, &mem_cfg);

while (remaining > 0) {
    size_t chunk = MIN(mem_cfg.block_size, remaining);
    esp_loader_mem_write(&loader, &mem_cfg, buf, chunk);
    // ...
}

esp_loader_mem_finish(&loader, &mem_cfg, entrypoint);
```

> [!NOTE]
> When loading multiple RAM segments (e.g. text + data), call `esp_loader_mem_start()` and `esp_loader_mem_write()` for each segment, then call `esp_loader_mem_finish()` once after all segments with the binary entrypoint.

---

## 4. Always Call `esp_loader_flash_finish()` — Reboot Flag Removed

In v1, `esp_loader_flash_finish(reboot)` accepted a boolean to optionally reboot the target after flashing. Due to issues with the ROM bootloader (it does not respond to further commands after the flash end command, and strapping pins are not re-read on the resulting reboot), the function was **not recommended** for use.

In v2, the `reboot` parameter has been **removed** and `esp_loader_flash_finish()` **must always be called** after flashing — it performs MD5 verification of the written data unless `skip_verify = true` is set in `esp_loader_flash_cfg_t`.

To reset the target after flashing, call `esp_loader_reset_target()` explicitly.

**v1:**

```c
esp_loader_flash_finish(true);   // not recommended — reboot via flash end command
```

**v2:**

```c
esp_loader_flash_finish(&loader, &flash_cfg);   // required — runs MD5 verification
esp_loader_reset_target(&loader);               // reset explicitly if needed
```

---

## 5. Update Port Implementations

The port layer contract changed from individual free functions to a single `esp_loader_port_ops_t` vtable embedded in a port struct that you own.

**v1** — port implementations provided a set of required free functions:

```c
esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout);
esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout);
void               loader_port_enter_bootloader(void);
void               loader_port_reset_target(void);
void               loader_port_delay_ms(uint32_t ms);
void               loader_port_start_timer(uint32_t ms);
uint32_t           loader_port_remaining_time(void);
esp_loader_error_t loader_port_change_transmission_rate(uint32_t rate);
void               loader_port_debug_print(const char *str);
```

**v2** — port implementations embed `esp_loader_port_t` inside a platform struct and use `container_of` to access per-instance state:

```c
#include "esp_loader_io.h"

typedef struct {
    esp_loader_port_t port;
    /* ... hardware handles and per-instance state ... */
} my_port_t;

static esp_loader_error_t my_port_init(esp_loader_port_t *port)
{
    my_port_t *p = container_of(port, my_port_t, port);
    /* open UART, configure GPIOs, etc. */
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t my_write(esp_loader_port_t *port, const uint8_t *data,
                                   uint16_t size, uint32_t timeout)
{
    my_port_t *p = container_of(port, my_port_t, port);
    /* transmit using p->uart_handle, etc. */
}

/* ... more callbacks ... */

const esp_loader_port_ops_t my_platform_ops = {
    .init                     = my_port_init,
    .deinit                   = NULL,  /* optional */
    .enter_bootloader         = my_enter_bootloader,
    .reset_target             = my_reset_target,
    .start_timer              = my_start_timer,
    .remaining_time           = my_remaining_time,
    .delay_ms                 = my_delay_ms,
    .log                      = my_log,              /* or NULL */
    .log_hex                  = my_log_hex,          /* or NULL */
    .change_transmission_rate = my_change_rate,      /* or NULL */
    .write                    = my_write,
    .read                     = my_read,
};
```

### Logging callback migration

The `debug_print` port callback has been removed. Implement `log` for formatted text messages and optionally `log_hex` for transfer dumps:

```c
static const char *const level_prefix[] = { "", "E", "W", "I", "D" };

static void my_log(esp_loader_port_t *port, esp_loader_log_level_t level,
                   const char *fmt, va_list args)
{
    (void)port;
    printf("[%s] esf: ", level_prefix[level]);
    vprintf(fmt, args);
    putchar('\n');
}

static void my_log_hex(esp_loader_port_t *port, esp_loader_log_level_t level,
                       const char *label, const uint8_t *data, size_t size)
{
    (void)port;
    printf("[%s] esf: %s (%zu bytes):\n",
           level_prefix[level], label ? label : "hex", size);
    /* Render data[0..size) using the platform logger. */
}
```

Set `.log = NULL` to suppress text output and `.log_hex = NULL` to suppress hex dumps. The built-in ports use the library's standard stdio helpers or their platform logger.

`SERIAL_FLASHER_DEBUG_TRACE` / `CONFIG_SERIAL_FLASHER_DEBUG_TRACE` has also been removed. Select the compile-time minimum log level instead:

```cmake
cmake -DSERIAL_FLASHER_LOG_LEVEL=DEBUG ..
```

For ESP-IDF or Zephyr, choose one of the `CONFIG_SERIAL_FLASHER_LOG_LEVEL_*` Kconfig options. DEBUG (`4`) enables per-command traces and transfer hex dumps; the default is WARN (`2`). Regular CMake builds may use either names (`DEBUG`) or numbers (`4`).

See `docs/supporting-new-platform.md` for a complete guide to writing a new port.

---

## 6. Update Interface / Protocol Selection

### CMake (non-ESP-IDF) Builds

The compile-time `SERIAL_FLASHER_INTERFACE_*` CMake variables are **removed**. Remove them from your build scripts. The protocol is selected by calling the appropriate per-protocol init function.

**v1:**

```bash
cmake -DSERIAL_FLASHER_INTERFACE_SPI=1 ..
```

**v2:**

```bash
cmake ..   # no interface flag needed
```

In code, call the per-protocol init:

```c
my_spi_port_t port = { .port.ops = &my_spi_port_ops, /* ... */ };
esp_loader_init_spi(&loader, &port.port);
```

### ESP-IDF (Kconfig) Builds

The `SERIAL_FLASHER_INTERFACE_*` Kconfig choice is replaced by individual `SERIAL_FLASHER_PORT_*` booleans. Multiple ports can be enabled simultaneously. But custom port can be implemented without these options anyway.

**v1 (`sdkconfig`):**

```ini
CONFIG_SERIAL_FLASHER_INTERFACE_UART=y
```

**v2 (`sdkconfig`):**

```ini
CONFIG_SERIAL_FLASHER_PORT_UART=y
# Optionally enable additional ports:
# CONFIG_SERIAL_FLASHER_PORT_SPI=y
# CONFIG_SERIAL_FLASHER_PORT_SDIO=y
# CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM=y
```

Each enabled port compiles its `*_port.c` file and exposes a `const esp_loader_port_ops_t` vtable (e.g. `esp32_uart_ops`, `esp32_spi_ops`).

---

## 7. Conditionally-Compiled APIs are Now Always Available

In v1, several functions were only declared when the matching interface macro was defined (e.g. `esp_loader_flash_read` was only available with `SERIAL_FLASHER_INTERFACE_UART`). In v2 these guards are removed — every function is always declared. Calling a function on an unsupported protocol returns `ESP_LOADER_ERROR_UNSUPPORTED_FUNC` instead of causing a compile error.

This means you no longer need `#ifdef SERIAL_FLASHER_INTERFACE_UART` guards around calls to `esp_loader_connect_with_stub()`, `esp_loader_flash_deflate_*()`, `esp_loader_flash_read()`, etc.

---

## 8. `esp_loader_error_t` Header Change

`esp_loader_error_t` is now defined in `include/esp_loader_error.h` (previously it was part of `esp_loader.h`). Including `esp_loader.h` still works — it automatically includes `esp_loader_error.h`. No source changes are required unless you included `esp_loader_io.h` independently to get the error type; in that case add `#include "esp_loader_error.h"` to your port code.

---

## 9. Multiple Simultaneous Instances

Because all state is in caller-owned structs, you can run multiple fully independent loader instances concurrently — each targeting a different device over its own hardware peripheral.

Each instance needs:

- Its own `esp_loader_t` context.
- Its own port struct (e.g. `esp32_port_t`, `linux_port_t`) with unique hardware resources (different UART port numbers, GPIO pins, etc.).
- A call to the matching `esp_loader_init_*()` function.

```c
esp32_port_t port_a = {
    .port.ops          = &esp32_uart_ops,
    .uart_port         = UART_NUM_1,
    .baud_rate         = 115200,
    .uart_rx_pin       = GPIO_NUM_5,
    .uart_tx_pin       = GPIO_NUM_4,
    .reset_pin         = GPIO_NUM_25,
    .boot_pin          = GPIO_NUM_26,
};

esp32_port_t port_b = {
    .port.ops          = &esp32_uart_ops,
    .uart_port         = UART_NUM_2,
    .baud_rate         = 115200,
    .uart_rx_pin       = GPIO_NUM_16,
    .uart_tx_pin       = GPIO_NUM_17,
    .reset_pin         = GPIO_NUM_27,
    .boot_pin          = GPIO_NUM_14,
};

esp_loader_t loader_a, loader_b;
esp_loader_init_serial(&loader_a, &port_a.port);
esp_loader_init_serial(&loader_b, &port_b.port);

// Both loaders can now be used independently (e.g. from separate RTOS tasks).
```

---

## Complete Migration Example

### Before (v1)

```c
#include "esp_loader.h"
#include "esp32_port.h"

void app_main(void)
{
    const loader_esp32_config_t config = { .baud_rate = 115200, /* ... */ };
    loader_port_esp32_init(&config);

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&args) != ESP_LOADER_SUCCESS) return;

    esp_loader_flash_start(0x10000, image_size, 4096);
    esp_loader_flash_write(image_data, image_size);
    esp_loader_flash_finish(true);
    esp_loader_reset_target();
}
```

### After (v2)

```c
#include "esp_loader.h"
#include "esp32_port.h"

void app_main(void)
{
    esp32_port_t port = {
        .port.ops          = &esp32_uart_ops,
        .baud_rate         = 115200,
        .uart_port         = UART_NUM_1,
        .uart_rx_pin       = GPIO_NUM_5,
        .uart_tx_pin       = GPIO_NUM_4,
        .reset_pin         = GPIO_NUM_25,
        .boot_pin          = GPIO_NUM_26,
    };

    esp_loader_t loader;
    esp_loader_init_serial(&loader, &port.port);

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&loader, &args) != ESP_LOADER_SUCCESS) return;

    esp_loader_flash_cfg_t flash_cfg = {
        .offset     = 0x10000,
        .image_size = image_size,
        .block_size = 4096,
    };
    if (esp_loader_flash_start(&loader, &flash_cfg) != ESP_LOADER_SUCCESS) return;

    size_t offset = 0;
    while (offset < image_size) {
        size_t chunk = MIN(flash_cfg.block_size, image_size - offset);
        if (esp_loader_flash_write(&loader, &flash_cfg, image_data + offset, chunk) != ESP_LOADER_SUCCESS) return;
        offset += chunk;
    }

    esp_loader_flash_finish(&loader, &flash_cfg);
    esp_loader_reset_target(&loader);
    esp_loader_deinit(&loader);
}
```

---

## 10. Raspberry Pi Port Removed — Use the Linux Port

The dedicated `port/raspberry_port.{c,h}` (pigpio-based) has been removed. Raspberry Pi is a standard Linux SBC and is now fully covered by the generic `port/linux_port.{c,h}`.

**v1 (pigpio port):**

```cmake
cmake -DPORT=RASPBERRY_PI ..
```

**v2 (Linux port with GPIO support):**

```cmake
cmake -DPORT=LINUX -DLINUX_PORT_GPIO=ON ..
```

Install the `libgpiod` dependency (≥ 2.0) instead of pigpio:

```bash
sudo apt-get install libgpiod-dev
```

The Linux port supports both DTR/RTS auto-reset (USB adapters) and GPIO character-device reset (SBCs). For wiring and run-time usage see [examples/linux_example/README.md](../examples/linux_example/README.md).
