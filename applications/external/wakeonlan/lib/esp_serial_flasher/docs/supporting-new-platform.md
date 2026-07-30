# Supporting New Host Platforms

This guide explains how to add support for a new host microcontroller or platform to ESP Serial Flasher.

## Overview

ESP Serial Flasher uses a port-layer abstraction based on a vtable (`esp_loader_port_ops_t`) embedded inside a caller-owned port struct. Each port struct embeds `esp_loader_port_t` as its first member, which allows the library to dispatch hardware operations without knowing the concrete type. Callbacks recover the full struct with the `container_of` macro.

```
 ┌─────────────────────────────────────────┐
 │  my_platform_port_t  (your struct)      │
 │ ┌───────────────────────────────────┐   │
 │ │  esp_loader_port_t  port          │ ◄─┼── pass &port to esp_loader_init_*()
 │ │  .ops ──────────────────────────► │   │
 │ └───────────────────────────────────┘   │
 │  uart_handle, gpio_pin, _time_end, …    │
 └─────────────────────────────────────────┘
           │ container_of(port, my_platform_port_t, port)
           ▼
   callbacks access per-instance state
```

## Port Operations Vtable

All operations are dispatched through `esp_loader_port_ops_t` (defined in `include/esp_loader_io.h`):

```c
typedef struct {
    esp_loader_error_t (*init)(esp_loader_port_t *port);             /* NULL = no init needed */
    void               (*deinit)(esp_loader_port_t *port);           /* NULL = no deinit needed */
    void               (*enter_bootloader)(esp_loader_port_t *port);
    void               (*reset_target)(esp_loader_port_t *port);
    void               (*start_timer)(esp_loader_port_t *port, uint32_t ms);
    uint32_t           (*remaining_time)(esp_loader_port_t *port);
    void               (*delay_ms)(esp_loader_port_t *port, uint32_t ms);
    void               (*log)(esp_loader_port_t *port, esp_loader_log_level_t level,
                              const char *fmt, va_list args); /* NULL = no text output */
    void               (*log_hex)(esp_loader_port_t *port, esp_loader_log_level_t level,
                                  const char *label, const uint8_t *data,
                                  size_t size); /* NULL = no hex output */
    esp_loader_error_t (*change_transmission_rate)(esp_loader_port_t *port, uint32_t rate); /* NULL if unsupported */

    /* UART / USB / SPI */
    esp_loader_error_t (*write)(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*read)(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout);

    /* SPI only */
    void               (*spi_set_cs)(esp_loader_port_t *port, uint32_t level);

    /* SDIO only */
    esp_loader_error_t (*sdio_write)(esp_loader_port_t *port, uint32_t function, uint32_t addr,
                                     const uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*sdio_read)(esp_loader_port_t *port, uint32_t function, uint32_t addr,
                                    uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*sdio_card_init)(esp_loader_port_t *port);
} esp_loader_port_ops_t;
```

Set unused pointers to `NULL`.

## Required and Optional Members

### `init` / `deinit` (optional)

```c
esp_loader_error_t (*init)(esp_loader_port_t *port);
void               (*deinit)(esp_loader_port_t *port);
```

`init` is called automatically by `esp_loader_init_serial()` / `esp_loader_init_spi()` / etc. before the connection phase. Use it to open the serial port, configure GPIO, install peripheral drivers, etc.

`deinit` is invoked by calling `esp_loader_deinit(&loader)` when the loader is no longer needed. This tears down the peripheral and zeroes the loader context. Set `deinit` to `NULL` if the peripheral is already configured before the port struct is created (e.g. STM32 with CubeMX-generated HAL code) or if teardown is not needed — `esp_loader_deinit()` is a no-op when the callback is NULL.

---

### `write` / `read` (UART, USB, SPI)

```c
esp_loader_error_t (*write)(esp_loader_port_t *port, const uint8_t *data,
                             uint16_t size, uint32_t timeout);
esp_loader_error_t (*read)(esp_loader_port_t *port, uint8_t *data,
                            uint16_t size, uint32_t timeout);
```

Transmit / receive bytes over the transport.

- Block until all bytes are sent / received or `timeout` (ms) elapses.
- Return `ESP_LOADER_SUCCESS` on success, `ESP_LOADER_ERROR_TIMEOUT` on timeout.
- Accept regular byte buffers from the protocol layer. If the hardware driver has DMA,
  cache, or word-alignment requirements, handle them inside the port implementation
  (for example by using the platform driver's aligned-buffer support or a port-local
  bounce buffer).

Not present for SDIO — use `sdio_write` / `sdio_read` instead.

---

### `enter_bootloader`

```c
void (*enter_bootloader)(esp_loader_port_t *port);
```

Put the target into ROM bootloader mode (assert BOOT, toggle RESET).

> [!NOTE]
> Respect `SERIAL_FLASHER_RESET_INVERT` and `SERIAL_FLASHER_BOOT_INVERT` when
> controlling GPIOs. Hold times are `SERIAL_FLASHER_RESET_HOLD_TIME_MS` and
> `SERIAL_FLASHER_BOOT_HOLD_TIME_MS`.

---

### `reset_target`

```c
void (*reset_target)(esp_loader_port_t *port);
```

Toggle the hardware RESET pin to restart the target. Hold for at least `SERIAL_FLASHER_RESET_HOLD_TIME_MS`.

---

### `start_timer` / `remaining_time`

```c
void     (*start_timer)(esp_loader_port_t *port, uint32_t ms);
uint32_t (*remaining_time)(esp_loader_port_t *port);
```

One-shot deadline timer. `remaining_time` returns milliseconds until the deadline, or `0` when elapsed. These two functions work together; store the deadline in the concrete port struct.

---

### `delay_ms`

```c
void (*delay_ms)(esp_loader_port_t *port, uint32_t ms);
```

Blocking delay in milliseconds.

---

### `log` / `log_hex` (optional)

```c
void (*log)(esp_loader_port_t *port, esp_loader_log_level_t level,
            const char *fmt, va_list args);
void (*log_hex)(esp_loader_port_t *port, esp_loader_log_level_t level,
                const char *label, const uint8_t *data, size_t size);
```

Route library messages to your platform's console or logger. `log` receives printf-style text without a trailing newline; `log_hex` receives binary buffers for transfer dumps at the supplied level. The built-in ports print hex dump headers with the same level prefix as text logs. Set either callback to `NULL` to suppress that output class.

Logging is compiled according to `SERIAL_FLASHER_LOG_LEVEL` / `CONFIG_SERIAL_FLASHER_LOG_LEVEL_*`. The default level is WARN, so DEBUG command traces and hex dumps are not included unless the build selects DEBUG.

---

### `change_transmission_rate` (optional)

```c
esp_loader_error_t (*change_transmission_rate)(esp_loader_port_t *port, uint32_t rate);
```

Reconfigure the peripheral baud rate / clock speed. Set to `NULL` for SDIO (the host driver manages the clock).

---

### SPI-specific: `spi_set_cs`

```c
void (*spi_set_cs)(esp_loader_port_t *port, uint32_t level);
```

Drive the SPI chip-select GPIO. Required for SPI ports.

---

### SDIO-specific: `sdio_write`, `sdio_read`, `sdio_card_init`

```c
esp_loader_error_t (*sdio_write)(esp_loader_port_t *port, uint32_t function,
                                 uint32_t addr, const uint8_t *data,
                                 uint16_t size, uint32_t timeout);
esp_loader_error_t (*sdio_read)(esp_loader_port_t *port, uint32_t function,
                                uint32_t addr, uint8_t *data,
                                uint16_t size, uint32_t timeout);
esp_loader_error_t (*sdio_card_init)(esp_loader_port_t *port);
```

These replace `write`/`read` for SDIO. `sdio_card_init` performs bus-level card initialisation and is called once per connection (before `enter_bootloader`).

As with the regular `write`/`read` callbacks, `sdio_write` and `sdio_read` must accept ordinary byte buffers from the protocol layer. Any alignment required by the host controller or DMA engine belongs in the port implementation. The ESP-IDF SDIO port enables `SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF` so the SDMMC host driver can handle unaligned buffers.

---

## Implementation Steps

### Option A: Contributing to the Repository

#### 1. Add Port Files

```text
port/your_platform_port.c
port/your_platform_port.h
```

#### 2. Define the Port Struct

In `your_platform_port.h`:

```c
#pragma once
#include "esp_loader_io.h"

typedef struct {
    esp_loader_port_t port;          /* embedded base — pass &port to esp_loader_init_*().
                                        By convention placed first in the struct. */

    /* Configuration fields — set before calling esp_loader_init_*() */
    uart_handle_t *uart;
    uint32_t       reset_pin;
    uint32_t       boot_pin;
    /* ... */

    /* Private runtime state — prefix with _ by convention */
    uint32_t _time_end;
} your_platform_port_t;

/** Port operations vtable. */
extern const esp_loader_port_ops_t your_platform_ops;
```

#### 3. Implement the Vtable

In `your_platform_port.c`:

```c
#include "your_platform_port.h"

static esp_loader_error_t your_port_init(esp_loader_port_t *port)
{
    your_platform_port_t *p = container_of(port, your_platform_port_t, port);
    /* Open / configure hardware using p->uart, p->reset_pin, etc. */
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t your_write(esp_loader_port_t *port, const uint8_t *data,
                                     uint16_t size, uint32_t timeout)
{
    your_platform_port_t *p = container_of(port, your_platform_port_t, port);
    /* transmit via p->uart */
}

static esp_loader_error_t your_read(esp_loader_port_t *port, uint8_t *data,
                                    uint16_t size, uint32_t timeout)
{
    your_platform_port_t *p = container_of(port, your_platform_port_t, port);
    /* receive via p->uart */
}

static void your_enter_bootloader(esp_loader_port_t *port) { /* ... */ }
static void your_reset_target(esp_loader_port_t *port)     { /* ... */ }
static void your_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    your_platform_port_t *p = container_of(port, your_platform_port_t, port);
    p->_time_end = now_ms() + ms;
}
static uint32_t your_remaining_time(esp_loader_port_t *port)
{
    your_platform_port_t *p = container_of(port, your_platform_port_t, port);
    int32_t r = (int32_t)(p->_time_end - now_ms());
    return (r > 0) ? (uint32_t)r : 0;
}
static void your_delay_ms(esp_loader_port_t *port, uint32_t ms) { platform_sleep(ms); }

const esp_loader_port_ops_t your_platform_ops = {
    .init                     = your_port_init,
    .deinit                   = NULL,
    .enter_bootloader         = your_enter_bootloader,
    .reset_target             = your_reset_target,
    .start_timer              = your_start_timer,
    .remaining_time           = your_remaining_time,
    .delay_ms                 = your_delay_ms,
    .log                      = NULL,
    .log_hex                  = NULL,
    .change_transmission_rate = NULL,
    .write                    = your_write,
    .read                     = your_read,
};
```

#### 4. Update the Build System

Add your port to the main `CMakeLists.txt`:

```cmake
elseif(PORT STREQUAL "YOUR_PLATFORM")
    list(APPEND flasher_srcs "port/your_platform_port.c")
```

---

### Option B: Using ESP Serial Flasher as an External Library

This approach uses ESP Serial Flasher as a submodule or external dependency.

#### 1. Add as Submodule

```bash
git submodule add https://github.com/espressif/esp-serial-flasher.git external/esp-serial-flasher
git submodule update --init --recursive
```

#### 2. Implement the Port in Your Source Tree

```text
your_project/
├── main/
│   ├── main.c
│   ├── your_port.c
│   ├── your_port.h
│   └── CMakeLists.txt
├── external/
│   └── esp-serial-flasher/
├── CMakeLists.txt
└── .gitmodules
```

#### 3. Configure the Build System

**Root CMakeLists.txt:**

```cmake
cmake_minimum_required(VERSION 3.22)
project(your_project C)

set(PORT USER_DEFINED)

add_subdirectory(external/esp-serial-flasher)
add_subdirectory(main)
```

**main/CMakeLists.txt:**

```cmake
add_executable(your_app main.c)

target_sources(flasher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/your_port.c
)

target_include_directories(your_app PRIVATE
    ${CMAKE_SOURCE_DIR}/external/esp-serial-flasher/include
)

target_include_directories(flasher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/esp-serial-flasher/include
)

target_link_libraries(your_app PRIVATE flasher)
```

Set `PORT=USER_DEFINED` so ESP Serial Flasher does not compile any built-in port.

#### 4. Use the Port

```c
#include "esp_loader.h"
#include "your_port.h"

int main(void)
{
    your_platform_port_t port = {
        .port.ops  = &your_platform_ops,
        .uart      = &my_uart_handle,
        .reset_pin = 25,
        .boot_pin  = 26,
    };

    esp_loader_t loader;
    esp_loader_init_serial(&loader, &port.port);

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    esp_loader_connect(&loader, &args);
    /* flash, read, etc. */
}
```

---

## Reference Implementations

Study the existing ports for examples:

| Platform          | Files                               | Notes                                |
| ----------------- | ----------------------------------- | ------------------------------------ |
| STM32 HAL         | `port/stm32_port.{c,h}`             | Pre-initialised peripheral (no init) |
| Zephyr            | `port/zephyr_port.{c,h}`            | TTY-based UART with GPIO DT specs    |
| ESP32 UART        | `port/esp32_port.{c,h}`             | Full example with peripheral init    |
| ESP32 SPI         | `port/esp32_spi_port.{c,h}`         | SPI with CS GPIO and strapping pins  |
| ESP32 SDIO        | `port/esp32_sdio_port.{c,h}`        | Reference SDIO implementation        |
| ESP32 USB CDC-ACM | `port/esp32_usb_cdc_acm_port.{c,h}` | Reconnection-capable USB port        |
| Raspberry Pi Pico | `port/pi_pico_port.{c,h}`           | SDK-based UART with baud tolerance   |
| Linux             | `port/linux_port.{c,h}`             | POSIX serial + optional libgpiod     |

---

## Getting Help

- See `include/esp_loader_io.h` for the vtable definition and `container_of` macro.
- Open a [GitHub issue](https://github.com/espressif/esp-serial-flasher/issues) for questions.
