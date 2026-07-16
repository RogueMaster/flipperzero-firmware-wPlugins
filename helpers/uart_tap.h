#pragma once

#include <furi.h>
#include "baud_table.h"

/* One 16 ms UI tick holds ~1.4 KB at 921600 baud, so 4 KB is comfortable even
 * if a frame or two runs late. */
#define UART_TAP_BUFFER (4096u)

typedef enum {
    UartTapEnterCr, // \r      - most embedded consoles
    UartTapEnterLf, // \n
    UartTapEnterCrLf, // \r\n

    UartTapEnterCount,
} UartTapEnter;

const char* uart_tap_enter_name(UartTapEnter mode);

typedef struct UartTap UartTap;

UartTap* uart_tap_alloc(void);
void uart_tap_free(UartTap* tap);

/** Open the line. `tx_enabled` false keeps Hermes strictly listening. */
bool uart_tap_open(
    UartTap* tap,
    HermesPort port,
    uint32_t baud,
    HermesFraming framing,
    bool tx_enabled);

void uart_tap_close(UartTap* tap);
bool uart_tap_is_open(const UartTap* tap);

/** Drain received bytes. Returns 0 when nothing is waiting; never blocks. */
size_t uart_tap_read(UartTap* tap, uint8_t* out, size_t max);

/** Send bytes to the target. No-op when TX is disabled. */
void uart_tap_send(UartTap* tap, const uint8_t* data, size_t len);
void uart_tap_send_byte(UartTap* tap, uint8_t byte);
void uart_tap_send_enter(UartTap* tap, UartTapEnter mode);

bool uart_tap_tx_enabled(const UartTap* tap);

/** Bytes the interrupt had to drop because the buffer was full. */
uint32_t uart_tap_dropped(const UartTap* tap);
