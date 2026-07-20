#include "ha_esp_port.h"

#include <furi.h>
#include <furi_hal_serial.h>

#include "esp_loader_io.h" // from the vendored esp-serial-flasher (Apache-2.0)

// The flasher runs single-threaded (in a worker), so file-static port state is
// fine: one serial handle, an RX stream buffer fed by the async RX ISR, and a
// simple deadline for the library's timeout helper.
#define ESP_PORT_RX_BUF (4096)

static FuriHalSerialHandle* s_serial = NULL;
static FuriStreamBuffer* s_rx = NULL;
static uint32_t s_timer_end = 0;

static void
    ha_esp_port_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UNUSED(context);
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(s_rx, &data, 1, 0);
    }
}

void ha_esp_port_start(FuriHalSerialHandle* handle, uint32_t baud) {
    s_serial = handle;
    if(!s_rx) s_rx = furi_stream_buffer_alloc(ESP_PORT_RX_BUF, 1);
    furi_stream_buffer_reset(s_rx);
    furi_hal_serial_set_br(handle, baud);
    furi_hal_serial_async_rx_start(handle, ha_esp_port_rx_isr, NULL, false);
}

void ha_esp_port_stop(void) {
    if(s_serial) {
        furi_hal_serial_async_rx_stop(s_serial);
        s_serial = NULL;
    }
    if(s_rx) {
        furi_stream_buffer_free(s_rx);
        s_rx = NULL;
    }
}

void ha_esp_port_flush(void) {
    if(s_rx) furi_stream_buffer_reset(s_rx);
}

// ---- esp-serial-flasher port interface (loader_port_*) ----

esp_loader_error_t loader_port_write(const uint8_t* data, uint16_t size, uint32_t timeout) {
    UNUSED(timeout); // tx is buffered by the UART peripheral; we just wait for it
    if(!s_serial) return ESP_LOADER_ERROR_FAIL;
    furi_hal_serial_tx(s_serial, data, size);
    furi_hal_serial_tx_wait_complete(s_serial);
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_read(uint8_t* data, uint16_t size, uint32_t timeout) {
    if(!s_rx) return ESP_LOADER_ERROR_FAIL;
    uint32_t deadline = furi_get_tick() + timeout;
    size_t got = 0;
    while(got < size) {
        uint32_t now = furi_get_tick();
        if(now >= deadline) break;
        got += furi_stream_buffer_receive(s_rx, data + got, size - got, deadline - now);
    }
    return (got == size) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
}

void loader_port_delay_ms(uint32_t ms) {
    furi_delay_ms(ms);
}

void loader_port_start_timer(uint32_t ms) {
    s_timer_end = furi_get_tick() + ms;
}

uint32_t loader_port_remaining_time(void) {
    uint32_t now = furi_get_tick();
    return (now < s_timer_end) ? (s_timer_end - now) : 0;
}

// The devboard's IO0/EN aren't reachable from the Flipper — the user enters
// download mode manually — so these are intentionally empty.
void loader_port_enter_bootloader(void) {
}

void loader_port_reset_target(void) {
}

void loader_port_debug_print(const char* str) {
    UNUSED(str);
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t transmission_rate) {
    if(!s_serial) return ESP_LOADER_ERROR_FAIL;
    furi_hal_serial_set_br(s_serial, transmission_rate);
    return ESP_LOADER_SUCCESS;
}
