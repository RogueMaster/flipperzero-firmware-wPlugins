#include "epaper42_bw.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <stdlib.h>
#include <string.h>

#define EPAPER42_WIDTH        400
#define EPAPER42_HEIGHT       300
#define EPAPER42_BUFFER_SIZE  (EPAPER42_WIDTH * EPAPER42_HEIGHT / 8)
#define EPAPER42_SPI_TIMEOUT  1000
#define EPAPER42_BUSY_TIMEOUT 20000

static const GpioPin* const epaper42_pin_dc = &gpio_ext_pc0;
static const GpioPin* const epaper42_pin_rst = &gpio_ext_pc1;
static const GpioPin* const epaper42_pin_busy = &gpio_ext_pc3;
static const FuriHalSpiBusHandle* const epaper42_spi = &furi_hal_spi_bus_handle_external;

static uint8_t* epaper42_framebuffer = NULL;
static bool epaper42_initialized = false;

static void epaper42_delay(uint32_t ms) {
    furi_delay_ms(ms);
}

static bool epaper42_wait_busy(void) {
    uint32_t timeout = 0;
    while(furi_hal_gpio_read(epaper42_pin_busy)) {
        if(timeout++ > EPAPER42_BUSY_TIMEOUT) {
            return false;
        }
        epaper42_delay(1);
    }
    return true;
}

static bool epaper42_tx(const uint8_t* data, size_t size) {
    furi_hal_spi_acquire(epaper42_spi);
    bool result = furi_hal_spi_bus_tx(epaper42_spi, data, size, EPAPER42_SPI_TIMEOUT);
    furi_hal_spi_release(epaper42_spi);
    return result;
}

static bool epaper42_command(uint8_t command) {
    furi_hal_gpio_write(epaper42_pin_dc, false);
    return epaper42_tx(&command, 1);
}

static bool epaper42_data(uint8_t data) {
    furi_hal_gpio_write(epaper42_pin_dc, true);
    return epaper42_tx(&data, 1);
}

static bool epaper42_data_buffer(const uint8_t* data, size_t size) {
    furi_hal_gpio_write(epaper42_pin_dc, true);
    return epaper42_tx(data, size);
}

static void epaper42_reset(void) {
    furi_hal_gpio_write(epaper42_pin_rst, false);
    epaper42_delay(50);
    furi_hal_gpio_write(epaper42_pin_rst, true);
    epaper42_delay(50);
}

static bool epaper42_setpos(uint16_t x, uint16_t y) {
    uint8_t x_byte = x / 8;
    return epaper42_command(0x4E) && epaper42_data(x_byte) && epaper42_command(0x4F) &&
           epaper42_data(y & 0xFF) && epaper42_data((y >> 8) & 0x01);
}

static bool
    epaper42_address_set(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    return epaper42_command(0x44) && epaper42_data((x_start >> 3) & 0xFF) &&
           epaper42_data((x_end >> 3) & 0xFF) && epaper42_command(0x45) &&
           epaper42_data(y_start & 0xFF) && epaper42_data((y_start >> 8) & 0xFF) &&
           epaper42_data(y_end & 0xFF) && epaper42_data((y_end >> 8) & 0xFF);
}

static bool epaper42_power_on(void) {
    if(!epaper42_command(0x22) || !epaper42_data(0xE0) || !epaper42_command(0x20)) {
        return false;
    }
    return epaper42_wait_busy();
}

static bool epaper42_update(void) {
    if(!epaper42_command(0x22) || !epaper42_data(0xF7) || !epaper42_command(0x20)) {
        return false;
    }
    return epaper42_wait_busy();
}

static bool epaper42_update_partial(void) {
    if(!epaper42_command(0x22) || !epaper42_data(0xFF) || !epaper42_command(0x20)) {
        return false;
    }
    return epaper42_wait_busy();
}

static bool epaper42_sleep(void) {
    if(!epaper42_command(0x10) || !epaper42_data(0x01)) {
        return false;
    }
    epaper42_delay(100);
    return true;
}

static bool epaper42_panel_init(void) {
    epaper42_reset();
    if(!epaper42_wait_busy()) return false;

    if(!epaper42_command(0x12)) return false;
    epaper42_delay(100);
    if(!epaper42_wait_busy()) return false;

    if(!epaper42_command(0x21) || !epaper42_data(0x40) || !epaper42_data(0x00)) return false;
    if(!epaper42_command(0x01) || !epaper42_data(0x2B) || !epaper42_data(0x01) ||
       !epaper42_data(0x00)) {
        return false;
    }
    if(!epaper42_command(0x3C) || !epaper42_data(0x01)) return false;
    if(!epaper42_command(0x11) || !epaper42_data(0x03)) return false;
    if(!epaper42_address_set(0, 0, EPAPER42_WIDTH - 1, EPAPER42_HEIGHT - 1)) return false;
    if(!epaper42_command(0x18) || !epaper42_data(0x80)) return false;
    if(!epaper42_setpos(0, 0)) return false;

    return epaper42_power_on();
}

static void epaper42_clear_buffer(void) {
    memset(epaper42_framebuffer, 0xFF, EPAPER42_BUFFER_SIZE);
}

static void epaper42_set_pixel(int32_t x, int32_t y, bool black) {
    if(x < 0 || y < 0 || x >= EPAPER42_WIDTH || y >= EPAPER42_HEIGHT) return;

    size_t index = (size_t)y * (EPAPER42_WIDTH / 8) + (x / 8);
    uint8_t mask = 0x80 >> (x % 8);
    if(black) {
        epaper42_framebuffer[index] &= ~mask;
    } else {
        epaper42_framebuffer[index] |= mask;
    }
}

static void epaper42_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, bool black) {
    for(int32_t yy = y; yy < y + h; yy++) {
        for(int32_t xx = x; xx < x + w; xx++) {
            epaper42_set_pixel(xx, yy, black);
        }
    }
}

static void epaper42_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    epaper42_fill_rect(x, y, w, 3, true);
    epaper42_fill_rect(x, y + h - 3, w, 3, true);
    epaper42_fill_rect(x, y, 3, h, true);
    epaper42_fill_rect(x + w - 3, y, 3, h, true);
}

static void epaper42_draw_segment(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t segment) {
    int32_t t = 8;
    switch(segment) {
    case 0:
        epaper42_fill_rect(x + t, y, w - 2 * t, t, true);
        break;
    case 1:
        epaper42_fill_rect(x + w - t, y + t, t, h / 2 - t, true);
        break;
    case 2:
        epaper42_fill_rect(x + w - t, y + h / 2, t, h / 2 - t, true);
        break;
    case 3:
        epaper42_fill_rect(x + t, y + h - t, w - 2 * t, t, true);
        break;
    case 4:
        epaper42_fill_rect(x, y + h / 2, t, h / 2 - t, true);
        break;
    case 5:
        epaper42_fill_rect(x, y + t, t, h / 2 - t, true);
        break;
    case 6:
        epaper42_fill_rect(x + t, y + h / 2 - t / 2, w - 2 * t, t, true);
        break;
    default:
        break;
    }
}

static void epaper42_draw_digit(int32_t x, int32_t y, uint8_t digit) {
    static const uint8_t digit_segments[10] = {
        0x3F,
        0x06,
        0x5B,
        0x4F,
        0x66,
        0x6D,
        0x7D,
        0x07,
        0x7F,
        0x6F,
    };
    uint8_t mask = digit_segments[digit % 10];
    for(uint8_t segment = 0; segment < 7; segment++) {
        if(mask & (1 << segment)) {
            epaper42_draw_segment(x, y, 44, 82, segment);
        }
    }
}

static void epaper42_draw_colon(int32_t x, int32_t y) {
    epaper42_fill_rect(x, y + 22, 8, 8, true);
    epaper42_fill_rect(x, y + 52, 8, 8, true);
}

static void epaper42_draw_time(uint32_t seconds) {
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    if(hours > 99) hours = 99;

    int32_t x = 25;
    int32_t y = 74;
    epaper42_draw_digit(x, y, hours / 10);
    epaper42_draw_digit(x + 50, y, hours % 10);
    epaper42_draw_colon(x + 103, y);
    epaper42_draw_digit(x + 124, y, minutes / 10);
    epaper42_draw_digit(x + 174, y, minutes % 10);
    epaper42_draw_colon(x + 227, y);
    epaper42_draw_digit(x + 248, y, secs / 10);
    epaper42_draw_digit(x + 298, y, secs % 10);
}

static void epaper42_draw_status(Epaper42TimerState state) {
    switch(state) {
    case Epaper42TimerStateRunning:
        epaper42_draw_rect(134, 22, 132, 28);
        epaper42_fill_rect(158, 30, 72, 12, true);
        break;
    case Epaper42TimerStatePaused:
        epaper42_draw_rect(134, 22, 132, 28);
        epaper42_fill_rect(165, 29, 18, 14, true);
        epaper42_fill_rect(216, 29, 18, 14, true);
        break;
    case Epaper42TimerStateFinished:
        epaper42_draw_rect(104, 22, 192, 28);
        epaper42_fill_rect(124, 30, 152, 12, true);
        break;
    case Epaper42TimerStateStopped:
    default:
        epaper42_draw_rect(134, 22, 132, 28);
        epaper42_fill_rect(164, 30, 72, 12, true);
        break;
    }
}

static void epaper42_draw_progress(uint32_t remaining, uint32_t duration) {
    int32_t x = 40;
    int32_t y = 220;
    int32_t w = 320;
    int32_t h = 24;
    epaper42_draw_rect(x, y, w, h);

    if(duration == 0) return;
    if(remaining > duration) remaining = duration;

    uint32_t elapsed = duration - remaining;
    int32_t fill_w = (int32_t)((elapsed * (uint32_t)(w - 10)) / duration);
    if(fill_w > 0) {
        epaper42_fill_rect(x + 5, y + 5, fill_w, h - 10, true);
    }
}

static void epaper42_invert_buffer(void) {
    for(size_t i = 0; i < EPAPER42_BUFFER_SIZE; i++) {
        epaper42_framebuffer[i] = ~epaper42_framebuffer[i];
    }
}

static bool epaper42_display_buffer(Epaper42RefreshMode refresh_mode) {
    if(!epaper42_panel_init()) {
        FURI_LOG_E("EPaper42", "panel init failed");
        return false;
    }
    if(!epaper42_setpos(0, 0)) return false;

    if(!epaper42_command(0x26) ||
       !epaper42_data_buffer(epaper42_framebuffer, EPAPER42_BUFFER_SIZE)) {
        return false;
    }
    if(!epaper42_setpos(0, 0)) return false;
    if(!epaper42_command(0x24) ||
       !epaper42_data_buffer(epaper42_framebuffer, EPAPER42_BUFFER_SIZE)) {
        return false;
    }

    bool updated = refresh_mode == Epaper42RefreshModePartial ? epaper42_update_partial() :
                                                                epaper42_update();
    epaper42_sleep();
    return updated;
}

bool epaper42_bw_init(void) {
    if(epaper42_initialized) return true;

    epaper42_framebuffer = malloc(EPAPER42_BUFFER_SIZE);
    if(!epaper42_framebuffer) return false;

    furi_hal_gpio_init(epaper42_pin_dc, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(epaper42_pin_rst, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    // Pull BUSY down so the app keeps working when the external display is not connected.
    furi_hal_gpio_init(epaper42_pin_busy, GpioModeInput, GpioPullDown, GpioSpeedLow);
    furi_hal_gpio_write(epaper42_pin_dc, true);
    furi_hal_gpio_write(epaper42_pin_rst, true);

    furi_hal_spi_bus_handle_init(epaper42_spi);
    epaper42_initialized = true;

    epaper42_clear_buffer();
    return true;
}

void epaper42_bw_deinit(void) {
    if(epaper42_initialized) {
        furi_hal_spi_bus_handle_deinit(epaper42_spi);
        furi_hal_gpio_init_simple(epaper42_pin_dc, GpioModeAnalog);
        furi_hal_gpio_init_simple(epaper42_pin_rst, GpioModeAnalog);
        furi_hal_gpio_init_simple(epaper42_pin_busy, GpioModeAnalog);
        epaper42_initialized = false;
    }

    free(epaper42_framebuffer);
    epaper42_framebuffer = NULL;
}

bool epaper42_bw_show_timer(
    uint32_t remaining_seconds,
    uint32_t duration_seconds,
    Epaper42TimerState state,
    Epaper42RefreshMode refresh_mode,
    bool invert_colors) {
    if(!epaper42_initialized && !epaper42_bw_init()) {
        FURI_LOG_E("EPaper42", "init failed in show_timer");
        return false;
    }

    epaper42_clear_buffer();
    epaper42_draw_status(state);
    epaper42_draw_time(remaining_seconds);
    epaper42_draw_progress(remaining_seconds, duration_seconds);
    if(invert_colors) {
        epaper42_invert_buffer();
    }

    bool ok = epaper42_display_buffer(refresh_mode);
    FURI_LOG_I("EPaper42", "show_timer ok=%d rem=%lu", ok, (unsigned long)remaining_seconds);
    return ok;
}

bool epaper42_bw_test_refresh(void) {
    if(!epaper42_initialized && !epaper42_bw_init()) {
        FURI_LOG_E("EPaper42", "init failed in test_refresh");
        return false;
    }

    memset(epaper42_framebuffer, 0x00, EPAPER42_BUFFER_SIZE);
    bool ok = epaper42_display_buffer(Epaper42RefreshModeFull);
    FURI_LOG_I("EPaper42", "test_refresh ok=%d", ok);
    return ok;
}
