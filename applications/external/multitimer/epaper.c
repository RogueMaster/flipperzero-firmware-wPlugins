#include "epaper.h"
#include "epaper42_bw.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <stdlib.h>
#include <string.h>

#define EPAPER_SPI_TIMEOUT  1000
#define EPAPER_BUSY_TIMEOUT 20000

typedef enum {
    EpaperControllerSSD1680,
    EpaperControllerSSD1683,
    EpaperControllerUC8253,
} EpaperController;

typedef enum {
    EpaperBufferRowMajor,
    EpaperBufferColMajor,
} EpaperBufferLayout;

typedef struct {
    EpaperModel model;
    uint16_t width;
    uint16_t height;
    EpaperController controller;
    EpaperBufferLayout layout;
    bool tested;
    const char* menu_label;
    const char* value_label;
    uint8_t x_start;
    uint8_t x_end;
    uint8_t y_start_l;
    uint8_t y_start_h;
    uint8_t y_end_l;
    uint8_t y_end_h;
    uint8_t drv_out_l;
    uint8_t drv_out_h;
} EpaperPanelProfile;

typedef struct {
    int32_t time_x;
    int32_t time_y;
    int32_t digit_w;
    int32_t digit_h;
    int32_t digit_step;
    int32_t colon_w;
    int32_t colon_dot;
    int32_t colon_gap;
    int32_t status_x;
    int32_t status_y;
    int32_t status_w;
    int32_t status_h;
    int32_t bar_x;
    int32_t bar_y;
    int32_t bar_w;
    int32_t bar_h;
    int32_t stroke;
} EpaperLayout;

static const GpioPin* const epaper_pin_dc = &gpio_ext_pc0;
static const GpioPin* const epaper_pin_rst = &gpio_ext_pc1;
static const GpioPin* const epaper_pin_busy = &gpio_ext_pc3;
static const FuriHalSpiBusHandle* const epaper_spi = &furi_hal_spi_bus_handle_external;

static const EpaperPanelProfile epaper_profiles[EpaperModelCount] = {
    {
        .model = EpaperModel154,
        .width = 200,
        .height = 200,
        .controller = EpaperControllerSSD1680,
        .layout = EpaperBufferColMajor,
        .tested = false,
        .menu_label = "Display size",
        .value_label = "1.54\" BW",
        .x_start = 0x00,
        .x_end = 0x18,
        .y_start_l = 0xC7,
        .y_start_h = 0x00,
        .y_end_l = 0x00,
        .y_end_h = 0x00,
        .drv_out_l = 0xC7,
        .drv_out_h = 0x00,
    },
    {
        .model = EpaperModel213,
        .width = 128,
        .height = 250,
        .controller = EpaperControllerSSD1680,
        .layout = EpaperBufferColMajor,
        .tested = false,
        .menu_label = "Display size",
        .value_label = "2.13\" BW",
        .x_start = 0x01,
        .x_end = 0x10,
        .y_start_l = 0xF9,
        .y_start_h = 0x00,
        .y_end_l = 0x00,
        .y_end_h = 0x00,
        .drv_out_l = 0xF9,
        .drv_out_h = 0x00,
    },
    {
        .model = EpaperModel290,
        .width = 128,
        .height = 296,
        .controller = EpaperControllerSSD1680,
        .layout = EpaperBufferColMajor,
        .tested = false,
        .menu_label = "Display size",
        .value_label = "2.9\" BW",
        .x_start = 0x01,
        .x_end = 0x10,
        .y_start_l = 0x27,
        .y_start_h = 0x01,
        .y_end_l = 0x00,
        .y_end_h = 0x00,
        .drv_out_l = 0x27,
        .drv_out_h = 0x01,
    },
    {
        .model = EpaperModel37,
        .width = 240,
        .height = 416,
        .controller = EpaperControllerUC8253,
        .layout = EpaperBufferColMajor,
        .tested = false,
        .menu_label = "Display size",
        .value_label = "3.7\" BW",
    },
    {
        .model = EpaperModel42,
        .width = 400,
        .height = 300,
        .controller = EpaperControllerSSD1683,
        .layout = EpaperBufferRowMajor,
        .tested = true,
        .menu_label = "Display size",
        .value_label = "4.2\" BW*",
    },
};

static EpaperModel epaper_current_model = EPAPER_MODEL_DEFAULT;
static EpaperModel epaper_initialized_model = EpaperModelCount;
static uint8_t* epaper_framebuffer = NULL;
static size_t epaper_buffer_size = 0;
static bool epaper_initialized = false;

static const EpaperPanelProfile* epaper_profile(void) {
    if(epaper_current_model >= EpaperModelCount) {
        return &epaper_profiles[EPAPER_MODEL_DEFAULT];
    }
    return &epaper_profiles[epaper_current_model];
}

static size_t epaper_buffer_size_for(const EpaperPanelProfile* profile) {
    return ((size_t)profile->width * profile->height) / 8;
}

static void epaper_delay(uint32_t ms) {
    furi_delay_ms(ms);
}

static bool epaper_wait_busy(void) {
    uint32_t timeout = 0;
    while(furi_hal_gpio_read(epaper_pin_busy)) {
        if(timeout++ > EPAPER_BUSY_TIMEOUT) {
            return false;
        }
        epaper_delay(1);
    }
    return true;
}

static bool epaper_tx(const uint8_t* data, size_t size) {
    furi_hal_spi_acquire(epaper_spi);
    bool result = furi_hal_spi_bus_tx(epaper_spi, data, size, EPAPER_SPI_TIMEOUT);
    furi_hal_spi_release(epaper_spi);
    return result;
}

static bool epaper_command(uint8_t command) {
    furi_hal_gpio_write(epaper_pin_dc, false);
    return epaper_tx(&command, 1);
}

static bool epaper_data(uint8_t data) {
    furi_hal_gpio_write(epaper_pin_dc, true);
    return epaper_tx(&data, 1);
}

static bool epaper_data_buffer(const uint8_t* data, size_t size) {
    furi_hal_gpio_write(epaper_pin_dc, true);
    return epaper_tx(data, size);
}

static void epaper_reset(void) {
    furi_hal_gpio_write(epaper_pin_rst, false);
    epaper_delay(50);
    furi_hal_gpio_write(epaper_pin_rst, true);
    epaper_delay(50);
}

static size_t epaper_buffer_index(int32_t x, int32_t y, const EpaperPanelProfile* profile) {
    uint16_t width_bytes = (profile->width + 7) / 8;
    if(profile->layout == EpaperBufferColMajor) {
        return (size_t)(x / 8) * profile->height + (size_t)y;
    }
    return (size_t)y * width_bytes + (size_t)(x / 8);
}

static void epaper_clear_buffer(void) {
    if(epaper_framebuffer && epaper_buffer_size > 0) {
        memset(epaper_framebuffer, 0xFF, epaper_buffer_size);
    }
}

static void epaper_set_pixel(int32_t x, int32_t y, bool black) {
    const EpaperPanelProfile* profile = epaper_profile();
    if(x < 0 || y < 0 || x >= profile->width || y >= profile->height) return;
    if(!epaper_framebuffer) return;

    size_t index = epaper_buffer_index(x, y, profile);
    uint8_t mask = 0x80 >> (x % 8);
    if(black) {
        epaper_framebuffer[index] &= ~mask;
    } else {
        epaper_framebuffer[index] |= mask;
    }
}

static void epaper_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, bool black) {
    for(int32_t yy = y; yy < y + h; yy++) {
        for(int32_t xx = x; xx < x + w; xx++) {
            epaper_set_pixel(xx, yy, black);
        }
    }
}

static void epaper_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t stroke) {
    if(stroke < 1) stroke = 1;
    epaper_fill_rect(x, y, w, stroke, true);
    epaper_fill_rect(x, y + h - stroke, w, stroke, true);
    epaper_fill_rect(x, y, stroke, h, true);
    epaper_fill_rect(x + w - stroke, y, stroke, h, true);
}

static void epaper_draw_segment(
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    uint8_t segment,
    int32_t stroke) {
    switch(segment) {
    case 0:
        epaper_fill_rect(x + stroke, y, w - 2 * stroke, stroke, true);
        break;
    case 1:
        epaper_fill_rect(x + w - stroke, y + stroke, stroke, h / 2 - stroke, true);
        break;
    case 2:
        epaper_fill_rect(x + w - stroke, y + h / 2, stroke, h / 2 - stroke, true);
        break;
    case 3:
        epaper_fill_rect(x + stroke, y + h - stroke, w - 2 * stroke, stroke, true);
        break;
    case 4:
        epaper_fill_rect(x, y + h / 2, stroke, h / 2 - stroke, true);
        break;
    case 5:
        epaper_fill_rect(x, y + stroke, stroke, h / 2 - stroke, true);
        break;
    case 6:
        epaper_fill_rect(x + stroke, y + h / 2 - stroke / 2, w - 2 * stroke, stroke, true);
        break;
    default:
        break;
    }
}

static void epaper_compute_layout(const EpaperPanelProfile* profile, EpaperLayout* layout) {
    layout->stroke = profile->width >= 300 ? 3 : 2;
    layout->digit_w = profile->width / 9;
    layout->digit_h = profile->height / 4;
    if(layout->digit_w < 18) layout->digit_w = 18;
    if(layout->digit_h < 32) layout->digit_h = 32;
    layout->digit_step = layout->digit_w + profile->width / 40;
    layout->colon_w = profile->width / 50;
    if(layout->colon_w < 4) layout->colon_w = 4;
    layout->colon_dot = profile->height / 14;
    if(layout->colon_dot < 4) layout->colon_dot = 4;
    layout->colon_gap = profile->height / 8;
    layout->time_x = profile->width / 16;
    layout->time_y = profile->height / 4;
    layout->status_w = profile->width / 3;
    layout->status_h = profile->height / 10;
    if(layout->status_h < 12) layout->status_h = 12;
    layout->status_x = (profile->width - layout->status_w) / 2;
    layout->status_y = profile->height / 12;
    layout->bar_w = profile->width * 4 / 5;
    layout->bar_h = profile->height / 12;
    if(layout->bar_h < 8) layout->bar_h = 8;
    layout->bar_x = (profile->width - layout->bar_w) / 2;
    layout->bar_y = profile->height * 3 / 4;
}

static void epaper_draw_digit(int32_t x, int32_t y, uint8_t digit, const EpaperLayout* layout) {
    static const uint8_t digit_segments[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t mask = digit_segments[digit % 10];
    for(uint8_t segment = 0; segment < 7; segment++) {
        if(mask & (1 << segment)) {
            epaper_draw_segment(x, y, layout->digit_w, layout->digit_h, segment, layout->stroke);
        }
    }
}

static void epaper_draw_colon(int32_t x, int32_t y, const EpaperLayout* layout) {
    epaper_fill_rect(x, y + layout->colon_gap, layout->colon_w, layout->colon_dot, true);
    epaper_fill_rect(x, y + layout->colon_gap * 2, layout->colon_w, layout->colon_dot, true);
}

static void epaper_draw_time(uint32_t seconds) {
    const EpaperPanelProfile* profile = epaper_profile();
    EpaperLayout layout;
    epaper_compute_layout(profile, &layout);

    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    if(hours > 99) hours = 99;

    int32_t x = layout.time_x;
    int32_t y = layout.time_y;
    epaper_draw_digit(x, y, hours / 10, &layout);
    x += layout.digit_step;
    epaper_draw_digit(x, y, hours % 10, &layout);
    x += layout.digit_step;
    epaper_draw_colon(x, y, &layout);
    x += layout.digit_step / 2;
    epaper_draw_digit(x, y, minutes / 10, &layout);
    x += layout.digit_step;
    epaper_draw_digit(x, y, minutes % 10, &layout);
    x += layout.digit_step;
    epaper_draw_colon(x, y, &layout);
    x += layout.digit_step / 2;
    epaper_draw_digit(x, y, secs / 10, &layout);
    x += layout.digit_step;
    epaper_draw_digit(x, y, secs % 10, &layout);
}

static void epaper_draw_status(EpaperTimerState state) {
    const EpaperPanelProfile* profile = epaper_profile();
    EpaperLayout layout;
    epaper_compute_layout(profile, &layout);

    switch(state) {
    case EpaperTimerStateRunning:
        epaper_draw_rect(
            layout.status_x, layout.status_y, layout.status_w, layout.status_h, layout.stroke);
        epaper_fill_rect(
            layout.status_x + layout.status_w / 5,
            layout.status_y + layout.status_h / 4,
            layout.status_w / 2,
            layout.status_h / 2,
            true);
        break;
    case EpaperTimerStatePaused:
        epaper_draw_rect(
            layout.status_x, layout.status_y, layout.status_w, layout.status_h, layout.stroke);
        epaper_fill_rect(
            layout.status_x + layout.status_w / 5,
            layout.status_y + layout.status_h / 5,
            layout.status_w / 8,
            layout.status_h * 3 / 5,
            true);
        epaper_fill_rect(
            layout.status_x + layout.status_w * 3 / 5,
            layout.status_y + layout.status_h / 5,
            layout.status_w / 8,
            layout.status_h * 3 / 5,
            true);
        break;
    case EpaperTimerStateFinished:
        epaper_draw_rect(
            layout.status_x - layout.status_w / 8,
            layout.status_y,
            layout.status_w * 5 / 4,
            layout.status_h,
            layout.stroke);
        epaper_fill_rect(
            layout.status_x,
            layout.status_y + layout.status_h / 4,
            layout.status_w,
            layout.status_h / 2,
            true);
        break;
    case EpaperTimerStateStopped:
    default:
        epaper_draw_rect(
            layout.status_x, layout.status_y, layout.status_w, layout.status_h, layout.stroke);
        epaper_fill_rect(
            layout.status_x + layout.status_w / 4,
            layout.status_y + layout.status_h / 3,
            layout.status_w / 2,
            layout.status_h / 3,
            true);
        break;
    }

    UNUSED(profile);
}

static void epaper_draw_progress(uint32_t remaining, uint32_t duration) {
    const EpaperPanelProfile* profile = epaper_profile();
    EpaperLayout layout;
    epaper_compute_layout(profile, &layout);

    epaper_draw_rect(layout.bar_x, layout.bar_y, layout.bar_w, layout.bar_h, layout.stroke);
    if(duration == 0) return;
    if(remaining > duration) remaining = duration;

    uint32_t elapsed = duration - remaining;
    int32_t fill_w =
        (int32_t)((elapsed * (uint32_t)(layout.bar_w - layout.stroke * 2)) / duration);
    if(fill_w > 0) {
        epaper_fill_rect(
            layout.bar_x + layout.stroke,
            layout.bar_y + layout.stroke,
            fill_w,
            layout.bar_h - layout.stroke * 2,
            true);
    }
}

static void epaper_invert_buffer(void) {
    for(size_t i = 0; i < epaper_buffer_size; i++) {
        epaper_framebuffer[i] = ~epaper_framebuffer[i];
    }
}

static bool epaper_setpos_ssd1683(uint16_t x, uint16_t y) {
    uint8_t x_byte = x / 8;
    return epaper_command(0x4E) && epaper_data(x_byte) && epaper_command(0x4F) &&
           epaper_data(y & 0xFF) && epaper_data((y >> 8) & 0x01);
}

static bool
    epaper_address_set_ssd1683(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    return epaper_command(0x44) && epaper_data((x_start >> 3) & 0xFF) &&
           epaper_data((x_end >> 3) & 0xFF) && epaper_command(0x45) &&
           epaper_data(y_start & 0xFF) && epaper_data((y_start >> 8) & 0xFF) &&
           epaper_data(y_end & 0xFF) && epaper_data((y_end >> 8) & 0xFF);
}

static bool epaper_power_on_ssd1683(void) {
    if(!epaper_command(0x22) || !epaper_data(0xE0) || !epaper_command(0x20)) {
        return false;
    }
    return epaper_wait_busy();
}

static bool epaper_panel_init_ssd1680(const EpaperPanelProfile* profile) {
    epaper_reset();
    if(!epaper_wait_busy()) return false;

    if(!epaper_command(0x12)) return false;
    epaper_delay(100);
    if(!epaper_wait_busy()) return false;

    if(!epaper_command(0x01) || !epaper_data(profile->drv_out_l) ||
       !epaper_data(profile->drv_out_h) || !epaper_data(0x00)) {
        return false;
    }
    if(!epaper_command(0x11) || !epaper_data(0x01)) return false;
    if(!epaper_command(0x44) || !epaper_data(profile->x_start) || !epaper_data(profile->x_end)) {
        return false;
    }
    if(!epaper_command(0x45) || !epaper_data(profile->y_start_l) ||
       !epaper_data(profile->y_start_h) || !epaper_data(profile->y_end_l) ||
       !epaper_data(profile->y_end_h)) {
        return false;
    }
    if(!epaper_command(0x4E) || !epaper_data(profile->x_start)) return false;
    if(!epaper_command(0x4F) || !epaper_data(profile->y_start_l) ||
       !epaper_data(profile->y_start_h)) {
        return false;
    }
    if(!epaper_command(0x3C) || !epaper_data(0x05)) return false;
    if(!epaper_command(0x21) || !epaper_data(0x00) || !epaper_data(0x00)) return false;

    if(profile->model == EpaperModel154) {
        if(!epaper_command(0x3C) || !epaper_data(0x80)) return false;
    }

    return true;
}

static bool epaper_panel_init_ssd1683(const EpaperPanelProfile* profile) {
    epaper_reset();
    if(!epaper_wait_busy()) return false;

    if(!epaper_command(0x12)) return false;
    epaper_delay(100);
    if(!epaper_wait_busy()) return false;

    if(!epaper_command(0x21) || !epaper_data(0x40) || !epaper_data(0x00)) return false;
    if(!epaper_command(0x01) || !epaper_data(0x2B) || !epaper_data(0x01) || !epaper_data(0x00)) {
        return false;
    }
    if(!epaper_command(0x3C) || !epaper_data(0x01)) return false;
    if(!epaper_command(0x11) || !epaper_data(0x03)) return false;
    if(!epaper_address_set_ssd1683(0, 0, profile->width - 1, profile->height - 1)) return false;
    if(!epaper_command(0x18) || !epaper_data(0x80)) return false;
    if(!epaper_setpos_ssd1683(0, 0)) return false;

    return epaper_power_on_ssd1683();
}

static bool epaper_panel_init_uc8253(void) {
    epaper_reset();
    if(!epaper_wait_busy()) return false;
    if(!epaper_command(0x00) || !epaper_data(0xD7) || !epaper_data(0x0E)) return false;
    if(!epaper_command(0x50) || !epaper_data(0x47)) return false;
    return true;
}

static bool epaper_send_framebuffer_ssd1680(const EpaperPanelProfile* profile, bool partial) {
    uint16_t line_bytes = (profile->width + 7) / 8;
    uint16_t col_bytes = profile->height;

    if(!epaper_command(0x24)) return false;
    for(uint16_t col = 0; col < col_bytes; col++) {
        for(uint16_t line = 0; line < line_bytes; line++) {
            size_t idx = (size_t)line * col_bytes + col;
            if(!epaper_data(epaper_framebuffer[idx])) return false;
        }
    }

    if(!partial) {
        if(!epaper_command(0x26)) return false;
        for(uint16_t col = 0; col < col_bytes; col++) {
            for(uint16_t line = 0; line < line_bytes; line++) {
                size_t idx = (size_t)line * col_bytes + col;
                if(!epaper_data(epaper_framebuffer[idx])) return false;
            }
        }
    }

    if(!epaper_command(0x22) || !epaper_data(partial ? 0xFF : 0xF7) || !epaper_command(0x20)) {
        return false;
    }
    if(!epaper_wait_busy()) return false;

    if(!epaper_command(0x10) || !epaper_data(0x01)) return false;
    epaper_delay(100);
    return true;
}

static bool epaper_send_framebuffer_ssd1683(EpaperRefreshMode refresh_mode) {
    if(!epaper_setpos_ssd1683(0, 0)) return false;
    if(!epaper_command(0x26) || !epaper_data_buffer(epaper_framebuffer, epaper_buffer_size)) {
        return false;
    }
    if(!epaper_setpos_ssd1683(0, 0)) return false;
    if(!epaper_command(0x24) || !epaper_data_buffer(epaper_framebuffer, epaper_buffer_size)) {
        return false;
    }

    bool updated = refresh_mode == EpaperRefreshModePartial ?
                       (epaper_command(0x22) && epaper_data(0xFF) && epaper_command(0x20) &&
                        epaper_wait_busy()) :
                       (epaper_command(0x22) && epaper_data(0xF7) && epaper_command(0x20) &&
                        epaper_wait_busy());

    if(!epaper_command(0x10) || !epaper_data(0x01)) return false;
    epaper_delay(100);
    return updated;
}

static bool epaper_send_framebuffer_uc8253(const EpaperPanelProfile* profile) {
    uint16_t line_bytes = (profile->width + 7) / 8;
    uint16_t col_bytes = profile->height;

    if(!epaper_command(0x50) || !epaper_data(0xC7)) return false;

    if(!epaper_command(0x10)) return false;
    if(!epaper_wait_busy()) return false;
    for(uint16_t col = 0; col < col_bytes; col++) {
        for(uint16_t line = 0; line < line_bytes; line++) {
            size_t idx = (size_t)line * col_bytes + col;
            if(!epaper_data(epaper_framebuffer[idx])) return false;
        }
    }

    if(!epaper_command(0x13)) return false;
    if(!epaper_wait_busy()) return false;
    for(uint16_t col = 0; col < col_bytes; col++) {
        for(uint16_t line = 0; line < line_bytes; line++) {
            size_t idx = (size_t)line * col_bytes + col;
            if(!epaper_data(epaper_framebuffer[idx])) return false;
        }
    }

    if(!epaper_command(0x04)) return false;
    if(!epaper_wait_busy()) return false;
    if(!epaper_command(0x12)) return false;
    if(!epaper_wait_busy()) return false;
    if(!epaper_command(0x02)) return false;
    if(!epaper_wait_busy()) return false;
    epaper_delay(10);
    return true;
}

static bool epaper_panel_init(const EpaperPanelProfile* profile) {
    switch(profile->controller) {
    case EpaperControllerSSD1680:
        return epaper_panel_init_ssd1680(profile);
    case EpaperControllerSSD1683:
        return epaper_panel_init_ssd1683(profile);
    case EpaperControllerUC8253:
        return epaper_panel_init_uc8253();
    default:
        return false;
    }
}

static bool epaper_display_buffer(EpaperRefreshMode refresh_mode) {
    const EpaperPanelProfile* profile = epaper_profile();
    if(!epaper_panel_init(profile)) return false;

    switch(profile->controller) {
    case EpaperControllerSSD1680:
        return epaper_send_framebuffer_ssd1680(profile, refresh_mode == EpaperRefreshModePartial);
    case EpaperControllerSSD1683:
        return epaper_send_framebuffer_ssd1683(refresh_mode);
    case EpaperControllerUC8253:
        return epaper_send_framebuffer_uc8253(profile);
    default:
        return false;
    }
}

static bool epaper_allocate_buffer(const EpaperPanelProfile* profile) {
    size_t needed = epaper_buffer_size_for(profile);
    if(epaper_framebuffer && epaper_buffer_size == needed) {
        return true;
    }

    free(epaper_framebuffer);
    epaper_framebuffer = malloc(needed);
    epaper_buffer_size = epaper_framebuffer ? needed : 0;
    return epaper_framebuffer != NULL;
}

void epaper_set_model(EpaperModel model) {
    if(model >= EpaperModelCount) {
        model = EPAPER_MODEL_DEFAULT;
    }
    if(model != epaper_current_model && epaper_initialized) {
        epaper_deinit();
    }
    epaper_current_model = model;
}

EpaperModel epaper_get_model(void) {
    return epaper_current_model;
}

const char* epaper_model_menu_label(EpaperModel model) {
    if(model >= EpaperModelCount) return epaper_profiles[EPAPER_MODEL_DEFAULT].menu_label;
    return epaper_profiles[model].menu_label;
}

const char* epaper_model_value_label(EpaperModel model) {
    if(model >= EpaperModelCount) return epaper_profiles[EPAPER_MODEL_DEFAULT].value_label;
    return epaper_profiles[model].value_label;
}

bool epaper_model_is_tested(EpaperModel model) {
    if(model >= EpaperModelCount) return epaper_profiles[EPAPER_MODEL_DEFAULT].tested;
    return epaper_profiles[model].tested;
}

bool epaper_init(void) {
    if(epaper_current_model == EpaperModel42) {
        if(epaper_initialized && epaper_initialized_model == EpaperModel42) {
            return true;
        }
        if(epaper_initialized) {
            epaper_deinit();
        }
        epaper_initialized = epaper42_bw_init();
        epaper_initialized_model = epaper_initialized ? EpaperModel42 : EpaperModelCount;
        return epaper_initialized;
    }

    const EpaperPanelProfile* profile = epaper_profile();
    if(epaper_initialized && epaper_initialized_model == epaper_current_model &&
       epaper_buffer_size == epaper_buffer_size_for(profile)) {
        return true;
    }

    if(epaper_initialized) {
        epaper_deinit();
    }

    if(!epaper_allocate_buffer(profile)) return false;

    furi_hal_gpio_init(epaper_pin_dc, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(epaper_pin_rst, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(epaper_pin_busy, GpioModeInput, GpioPullDown, GpioSpeedLow);
    furi_hal_gpio_write(epaper_pin_dc, true);
    furi_hal_gpio_write(epaper_pin_rst, true);

    furi_hal_spi_bus_handle_init(epaper_spi);
    epaper_initialized = true;
    epaper_initialized_model = epaper_current_model;
    epaper_clear_buffer();
    return true;
}

void epaper_deinit(void) {
    if(epaper_initialized_model == EpaperModel42) {
        epaper42_bw_deinit();
    } else if(epaper_initialized) {
        furi_hal_spi_bus_handle_deinit(epaper_spi);
        furi_hal_gpio_init_simple(epaper_pin_dc, GpioModeAnalog);
        furi_hal_gpio_init_simple(epaper_pin_rst, GpioModeAnalog);
        furi_hal_gpio_init_simple(epaper_pin_busy, GpioModeAnalog);
    }

    epaper_initialized = false;
    free(epaper_framebuffer);
    epaper_framebuffer = NULL;
    epaper_buffer_size = 0;
    epaper_initialized_model = EpaperModelCount;
}

bool epaper_show_timer(
    uint32_t remaining_seconds,
    uint32_t duration_seconds,
    EpaperTimerState state,
    EpaperRefreshMode refresh_mode,
    bool invert_colors) {
    if(epaper_current_model == EpaperModel42) {
        if(!epaper_initialized && !epaper_init()) return false;
        return epaper42_bw_show_timer(
            remaining_seconds,
            duration_seconds,
            (Epaper42TimerState)state,
            (Epaper42RefreshMode)refresh_mode,
            invert_colors);
    }

    if(!epaper_initialized && !epaper_init()) return false;

    epaper_clear_buffer();
    epaper_draw_status(state);
    epaper_draw_time(remaining_seconds);
    epaper_draw_progress(remaining_seconds, duration_seconds);
    if(invert_colors) {
        epaper_invert_buffer();
    }

    return epaper_display_buffer(refresh_mode);
}
