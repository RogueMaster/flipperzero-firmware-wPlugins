#include "ha_esp_port.h"

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>
#include <furi_hal_resources.h>

#include "esp_loader_io.h" // from the vendored esp-serial-flasher (Apache-2.0)

// DTR/RTS hold times, same values esptool uses for the classic reset dance.
#define HA_RESET_HOLD_TIME_MS 100
#define HA_BOOT_HOLD_TIME_MS  50

// The flasher runs single-threaded (in a worker), so file-static port state is
// fine: one serial handle, an RX stream buffer fed by the async RX ISR, and a
// simple deadline for the library's timeout helper.
#define ESP_PORT_RX_BUF (4096)

static FuriHalSerialHandle* s_serial = NULL;
static FuriStreamBuffer* s_rx = NULL;
static uint32_t s_timer_end = 0;
static volatile bool* s_cancel = NULL;

static bool ha_esp_port_cancelled(void) {
    return s_cancel && *s_cancel;
}

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
        // Bail the moment the worker is cancelled: this is the one place the
        // library waits, so checking here unwinds it whatever call it is in.
        if(ha_esp_port_cancelled()) return ESP_LOADER_ERROR_TIMEOUT;
        uint32_t now = furi_get_tick();
        if(now >= deadline) break;
        uint32_t wait = deadline - now;
        // Cap the sleep so a long library timeout still notices the cancel.
        if(wait > 50) wait = 50;
        got += furi_stream_buffer_receive(s_rx, data + got, size - got, wait);
    }
    return (got == size) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
}

void loader_port_delay_ms(uint32_t ms) {
    // Same reasoning as the read: the library sleeps between connect attempts
    // and around resets, so chunk it and give up early when cancelled.
    while(ms) {
        if(ha_esp_port_cancelled()) return;
        uint32_t step = (ms > 50) ? 50 : ms;
        furi_delay_ms(step);
        ms -= step;
    }
}

void loader_port_start_timer(uint32_t ms) {
    s_timer_end = furi_get_tick() + ms;
}

uint32_t loader_port_remaining_time(void) {
    uint32_t now = furi_get_tick();
    return (now < s_timer_end) ? (s_timer_end - now) : 0;
}

// ---- DTR/RTS reset lines ----
// Both pin pairs are driven together so this works on the boards that use the
// alternate wiring as well as the official dev board.

static void ha_dtr_init(void) {
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
}

static void ha_rts_init(void) {
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
}

static void ha_dtr_set(bool state) {
    furi_hal_gpio_write(&gpio_ext_pc1, state);
    furi_hal_gpio_write(&gpio_ext_pc3, state);
}

static void ha_rts_set(bool state) {
    furi_hal_gpio_write(&gpio_ext_pb2, state);
    furi_hal_gpio_write(&gpio_ext_pc0, state);
}

static void ha_dtr_deinit(void) {
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static void ha_rts_deinit(void) {
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void ha_esp_port_enter_bootloader(void) {
    ha_dtr_set(false);
    ha_dtr_init();
    ha_rts_set(false);
    ha_rts_init();

    // Power-cycle the board over OTG so boards without a usable EN line still
    // come up fresh, then release the pin we used to hold it down.
    furi_hal_gpio_init_simple(&gpio_swclk, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_swclk, true);
    furi_hal_gpio_write(&gpio_swclk, false);
    if(furi_hal_power_is_otg_enabled()) furi_hal_power_disable_otg();
    loader_port_delay_ms(1000);
    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    furi_hal_gpio_init_simple(&gpio_swclk, GpioModeAnalog);
    loader_port_delay_ms(1000);

    // The usb-jtag-serial reset esptool performs, which is what the official
    // WiFi dev board expects: assert DTR, then swap to RTS to hold IO0 low
    // across the reset edge.
    ha_dtr_set(true);
    loader_port_delay_ms(HA_RESET_HOLD_TIME_MS);
    ha_rts_set(true);
    ha_dtr_set(false);
    loader_port_delay_ms(HA_BOOT_HOLD_TIME_MS);
    ha_rts_set(false);

    ha_dtr_deinit();
    ha_rts_deinit();
}

void ha_esp_port_reset_target(void) {
    // Plain reset: pulse DTR without holding IO0, so the chip comes up running
    // the firmware we just wrote instead of back in the bootloader.
    ha_dtr_set(false);
    ha_dtr_init();
    ha_dtr_set(true);
    loader_port_delay_ms(HA_RESET_HOLD_TIME_MS);
    ha_dtr_set(false);
    ha_dtr_deinit();
}

// The library calls these itself on every connect attempt. Driving the reset
// dance (and OTG power) once per attempt would make the poll unusable, so the
// sequence lives in ha_esp_port_enter_bootloader() and the flasher fires it once.
void loader_port_enter_bootloader(void) {
}

void loader_port_reset_target(void) {
}

void ha_esp_port_set_cancel(volatile bool* cancel) {
    s_cancel = cancel;
}

void loader_port_debug_print(const char* str) {
    UNUSED(str);
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t transmission_rate) {
    if(!s_serial) return ESP_LOADER_ERROR_FAIL;
    furi_hal_serial_set_br(s_serial, transmission_rate);
    return ESP_LOADER_SUCCESS;
}
