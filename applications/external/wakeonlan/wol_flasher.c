#include "wol_flasher.h"

#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <storage/storage.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_loader.h"
#include "esp_loader_io.h"

#define TAG "WolFlasher"

#define FLASHER_SERIAL_ID FuriHalSerialIdUsart
#define FLASHER_BAUD_SYNC 115200
/* The dev board syncs at 115200; 460800 survives the ribbon fine and cuts a
 * 4 MB dump from ~7 minutes to ~1.5. Falls back silently if the target says no. */
#define FLASHER_BAUD_FAST 460800
#define FLASHER_RX_BUF    4096

/* ROM accepts 1 KB flash blocks, the stub 4 KB. */
#define FLASHER_BLOCK_ROM  1024
#define FLASHER_BLOCK_STUB 4096

/*
 * Automatic bootloader entry.
 *
 * The official dev board routes the ESP32-S2 reset and strapping lines to two
 * header pins, so the esptool usb-jtag-serial reset dance works without
 * touching the buttons:
 *
 *   pin 7, PC3 -> DTR
 *   pin 6, PB2 -> RTS
 *
 * Third party boards normally leave both unconnected, in which case this does
 * nothing and the manual BOOT+RESET is still the way in. The library calls
 * enter_bootloader once per connect attempt, not per sync retry.
 */
#define FLASHER_DTR_PIN       (&gpio_ext_pc3)
#define FLASHER_RTS_PIN       (&gpio_ext_pb2)
#define FLASHER_RESET_HOLD_MS 100
#define FLASHER_BOOT_HOLD_MS  50

typedef struct {
    esp_loader_port_t base;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    uint32_t deadline;
} WolFlasherPort;

struct WolFlasher {
    esp_loader_t loader;
    WolFlasherPort port;
    volatile bool* cancel;
    WolFlasherProgressCallback progress_callback;
    void* progress_context;
    Storage* storage;
    uint8_t* buffer;
    uint32_t block_size;
    uint32_t flash_size;
    uint32_t rate;
    target_chip_t chip;
    bool stub_running;
    bool opened;
};

/* ------------------------------------------------------------------ port */

static void wol_flasher_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    WolFlasherPort* port = context;
    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(port->rx, &byte, 1, 0);
    }
}

static void wol_flasher_port_drain(WolFlasherPort* port) {
    uint8_t byte;
    while(furi_stream_buffer_receive(port->rx, &byte, 1, 0) > 0) {
    }
}

static esp_loader_error_t
    wol_port_write(esp_loader_port_t* base, const uint8_t* data, uint16_t size, uint32_t timeout) {
    UNUSED(timeout);
    WolFlasherPort* port = container_of(base, WolFlasherPort, base);

    furi_hal_serial_tx(port->serial, data, size);
    furi_hal_serial_tx_wait_complete(port->serial);
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t
    wol_port_read(esp_loader_port_t* base, uint8_t* data, uint16_t size, uint32_t timeout) {
    WolFlasherPort* port = container_of(base, WolFlasherPort, base);
    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(timeout);
    size_t received = 0;

    while(received < size) {
        int32_t left = (int32_t)(deadline - furi_get_tick());
        if(left <= 0) return ESP_LOADER_ERROR_TIMEOUT;

        received += furi_stream_buffer_receive(port->rx, data + received, size - received, left);
    }
    return ESP_LOADER_SUCCESS;
}

static void wol_port_start_timer(esp_loader_port_t* base, uint32_t ms) {
    WolFlasherPort* port = container_of(base, WolFlasherPort, base);
    port->deadline = furi_get_tick() + furi_ms_to_ticks(ms);
}

static uint32_t wol_port_remaining_time(esp_loader_port_t* base) {
    WolFlasherPort* port = container_of(base, WolFlasherPort, base);
    int32_t left = (int32_t)(port->deadline - furi_get_tick());
    return left > 0 ? (uint32_t)left : 0;
}

static void wol_port_delay_ms(esp_loader_port_t* base, uint32_t ms) {
    UNUSED(base);
    furi_delay_ms(ms);
}

static esp_loader_error_t wol_port_change_rate(esp_loader_port_t* base, uint32_t rate) {
    WolFlasherPort* port = container_of(base, WolFlasherPort, base);

    furi_hal_serial_set_br(port->serial, rate);
    furi_delay_ms(20);
    // the target babbles a few bytes at the old rate while switching
    wol_flasher_port_drain(port);
    return ESP_LOADER_SUCCESS;
}

static void wol_port_log(
    esp_loader_port_t* base,
    esp_loader_log_level_t level,
    const char* fmt,
    va_list args) {
    UNUSED(base);
    if(level > ESP_LOADER_LOG_LEVEL_WARN) return;

    char text[96];
    vsnprintf(text, sizeof(text), fmt, args);
    FURI_LOG_W(TAG, "%s", text);
}

static void wol_flasher_lines_init(void) {
    furi_hal_gpio_write(FLASHER_DTR_PIN, false);
    furi_hal_gpio_init(FLASHER_DTR_PIN, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_write(FLASHER_RTS_PIN, false);
    furi_hal_gpio_init(FLASHER_RTS_PIN, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
}

static void wol_flasher_lines_release(void) {
    furi_hal_gpio_init_simple(FLASHER_DTR_PIN, GpioModeAnalog);
    furi_hal_gpio_init_simple(FLASHER_RTS_PIN, GpioModeAnalog);
}

static void wol_port_enter_bootloader(esp_loader_port_t* base) {
    UNUSED(base);

    // strap the boot pin, pulse reset, then let both go
    furi_hal_gpio_write(FLASHER_DTR_PIN, true);
    furi_delay_ms(FLASHER_RESET_HOLD_MS);
    furi_hal_gpio_write(FLASHER_RTS_PIN, true);
    furi_hal_gpio_write(FLASHER_DTR_PIN, false);
    furi_delay_ms(FLASHER_BOOT_HOLD_MS);
    furi_hal_gpio_write(FLASHER_RTS_PIN, false);
    furi_delay_ms(FLASHER_BOOT_HOLD_MS);
}

static void wol_port_reset_target(esp_loader_port_t* base) {
    UNUSED(base);

    furi_hal_gpio_write(FLASHER_DTR_PIN, true);
    furi_delay_ms(FLASHER_RESET_HOLD_MS);
    furi_hal_gpio_write(FLASHER_DTR_PIN, false);
}

static const esp_loader_port_ops_t wol_port_ops = {
    .init = NULL,
    .deinit = NULL,
    .enter_bootloader = wol_port_enter_bootloader,
    .reset_target = wol_port_reset_target,
    .start_timer = wol_port_start_timer,
    .remaining_time = wol_port_remaining_time,
    .delay_ms = wol_port_delay_ms,
    .log = wol_port_log,
    .log_hex = NULL,
    .change_transmission_rate = wol_port_change_rate,
    .write = wol_port_write,
    .read = wol_port_read,
    .spi_set_cs = NULL,
    .sdio_write = NULL,
    .sdio_read = NULL,
    .sdio_card_init = NULL,
};

/* ------------------------------------------------------------------ misc */

static bool wol_flasher_cancelled(WolFlasher* flasher) {
    return flasher->cancel && *flasher->cancel;
}

static void wol_flasher_report(WolFlasher* flasher, WolFlasherStage stage, uint8_t percent) {
    if(flasher->progress_callback) {
        flasher->progress_callback(flasher->progress_context, stage, percent);
    }
}

const char* wol_flasher_result_text(WolFlasherResult result) {
    switch(result) {
    case WolFlasherOk:
        return "Done";
    case WolFlasherErrBusy:
        return "USART is busy";
    case WolFlasherErrNoBoard:
        return "No bootloader answer";
    case WolFlasherErrWrongChip:
        return "Not an ESP32-S2";
    case WolFlasherErrFile:
        return "SD card error";
    case WolFlasherErrFlash:
        return "Flash operation failed";
    case WolFlasherErrVerify:
        return "MD5 mismatch";
    case WolFlasherErrCancelled:
        return "Cancelled";
    default:
        return "Unknown error";
    }
}

WolFlasher* wol_flasher_alloc(volatile bool* cancel) {
    WolFlasher* flasher = malloc(sizeof(WolFlasher));
    memset(flasher, 0, sizeof(WolFlasher));

    flasher->cancel = cancel;
    flasher->storage = furi_record_open(RECORD_STORAGE);
    flasher->port.base.ops = &wol_port_ops;
    flasher->port.rx = furi_stream_buffer_alloc(FLASHER_RX_BUF, 1);
    flasher->block_size = FLASHER_BLOCK_ROM;
    flasher->rate = FLASHER_BAUD_SYNC;
    flasher->chip = ESP_MAX_CHIP;

    return flasher;
}

void wol_flasher_free(WolFlasher* flasher) {
    furi_check(flasher);

    wol_flasher_disconnect(flasher);
    furi_stream_buffer_free(flasher->port.rx);
    furi_record_close(RECORD_STORAGE);
    free(flasher);
}

void wol_flasher_set_progress_callback(
    WolFlasher* flasher,
    WolFlasherProgressCallback callback,
    void* context) {
    furi_check(flasher);
    flasher->progress_callback = callback;
    flasher->progress_context = context;
}

uint32_t wol_flasher_get_flash_size(const WolFlasher* flasher) {
    return flasher->flash_size;
}

const char* wol_flasher_get_chip_name(const WolFlasher* flasher) {
    switch(flasher->chip) {
    case ESP32S2_CHIP:
        return "ESP32-S2";
    case ESP32_CHIP:
        return "ESP32";
    case ESP32S3_CHIP:
        return "ESP32-S3";
    case ESP32C3_CHIP:
        return "ESP32-C3";
    case ESP8266_CHIP:
        return "ESP8266";
    default:
        return "unknown";
    }
}

bool wol_flasher_is_stub_running(const WolFlasher* flasher) {
    return flasher->stub_running;
}

uint32_t wol_flasher_get_transmission_rate(const WolFlasher* flasher) {
    return flasher->rate;
}

/* --------------------------------------------------------------- connect */

WolFlasherResult wol_flasher_connect(WolFlasher* flasher) {
    furi_check(flasher);
    if(flasher->opened) return WolFlasherOk;

    wol_flasher_report(flasher, WolFlasherStageConnect, 0);

    // 5V stays on for the lifetime of the app, see wol_flipper.c
    flasher->port.serial = furi_hal_serial_control_acquire(FLASHER_SERIAL_ID);
    if(!flasher->port.serial) {
        return WolFlasherErrBusy;
    }

    wol_flasher_lines_init();
    furi_hal_serial_init(flasher->port.serial, FLASHER_BAUD_SYNC);
    furi_hal_serial_async_rx_start(
        flasher->port.serial, wol_flasher_rx_callback, &flasher->port, false);
    flasher->opened = true;
    flasher->rate = FLASHER_BAUD_SYNC;
    wol_flasher_port_drain(&flasher->port);

    if(esp_loader_init_serial(&flasher->loader, &flasher->port.base) != ESP_LOADER_SUCCESS) {
        wol_flasher_disconnect(flasher);
        return WolFlasherErrNoBoard;
    }

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();

    // the stub buys fast flash reads and 4 KB write blocks; plain ROM still
    // works for everything, just slower
    if(esp_loader_connect_with_stub(&flasher->loader, &args) == ESP_LOADER_SUCCESS) {
        flasher->stub_running = true;
        flasher->block_size = FLASHER_BLOCK_STUB;
    } else {
        args = (esp_loader_connect_args_t)ESP_LOADER_CONNECT_DEFAULT();
        if(esp_loader_connect(&flasher->loader, &args) != ESP_LOADER_SUCCESS) {
            wol_flasher_disconnect(flasher);
            return WolFlasherErrNoBoard;
        }
        flasher->stub_running = false;
        flasher->block_size = FLASHER_BLOCK_ROM;
    }

    flasher->chip = esp_loader_get_target(&flasher->loader);
    if(flasher->chip != ESP32S2_CHIP) {
        wol_flasher_disconnect(flasher);
        return WolFlasherErrWrongChip;
    }

    /* Raise the rate, then immediately prove the link still works with a cheap
     * round trip. The Flipper takes an interrupt per received byte, so 460800
     * is not guaranteed; better to find out here than halfway through a dump. */
    bool sized = false;
    if(esp_loader_change_transmission_rate(&flasher->loader, FLASHER_BAUD_FAST) ==
       ESP_LOADER_SUCCESS) {
        if(esp_loader_flash_detect_size(&flasher->loader, &flasher->flash_size) ==
           ESP_LOADER_SUCCESS) {
            flasher->rate = FLASHER_BAUD_FAST;
            sized = true;
        } else {
            FURI_LOG_W(TAG, "%lu baud unusable, falling back", (unsigned long)FLASHER_BAUD_FAST);
            esp_loader_change_transmission_rate(&flasher->loader, FLASHER_BAUD_SYNC);
            flasher->rate = FLASHER_BAUD_SYNC;
        }
    }

    if(!sized && esp_loader_flash_detect_size(&flasher->loader, &flasher->flash_size) !=
                     ESP_LOADER_SUCCESS) {
        // every board shipped so far carries 4 MB; keep going with that
        flasher->flash_size = 4 * 1024 * 1024;
    }

    flasher->buffer = malloc(flasher->block_size);
    wol_flasher_report(flasher, WolFlasherStageConnect, 100);
    return WolFlasherOk;
}

void wol_flasher_disconnect(WolFlasher* flasher) {
    furi_check(flasher);

    if(flasher->buffer) {
        free(flasher->buffer);
        flasher->buffer = NULL;
    }

    if(flasher->opened) {
        wol_flasher_lines_release();
        furi_hal_serial_async_rx_stop(flasher->port.serial);
        furi_hal_serial_deinit(flasher->port.serial);
        furi_hal_serial_control_release(flasher->port.serial);
        flasher->port.serial = NULL;
        flasher->opened = false;
        flasher->stub_running = false;
    }
}

/* ---------------------------------------------------------------- backup */

void wol_flasher_reset_target(WolFlasher* flasher) {
    furi_check(flasher);
    if(flasher->opened) esp_loader_reset_target(&flasher->loader);
}

WolFlasherResult wol_flasher_backup(WolFlasher* flasher, const char* path) {
    furi_check(flasher && flasher->opened);

    File* file = storage_file_alloc(flasher->storage);
    WolFlasherResult result = WolFlasherOk;
    uint8_t last_percent = 255;

    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return WolFlasherErrFile;
    }

    for(uint32_t address = 0; address < flasher->flash_size;) {
        if(wol_flasher_cancelled(flasher)) {
            result = WolFlasherErrCancelled;
            break;
        }

        uint32_t chunk = flasher->flash_size - address;
        if(chunk > flasher->block_size) chunk = flasher->block_size;

        if(esp_loader_flash_read(&flasher->loader, flasher->buffer, address, chunk) !=
           ESP_LOADER_SUCCESS) {
            result = WolFlasherErrFlash;
            break;
        }
        if(storage_file_write(file, flasher->buffer, chunk) != chunk) {
            result = WolFlasherErrFile;
            break;
        }

        address += chunk;

        uint8_t percent = (uint8_t)((uint64_t)address * 100 / flasher->flash_size);
        if(percent != last_percent) {
            last_percent = percent;
            wol_flasher_report(flasher, WolFlasherStageRead, percent);
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    if(result != WolFlasherOk) {
        // a truncated dump is worse than none: it would restore as a brick
        storage_common_remove(flasher->storage, path);
    }
    return result;
}

/* ----------------------------------------------------------------- write */

static uint32_t wol_flasher_align4(uint32_t value) {
    return (value + 3u) & ~3u;
}

WolFlasherResult
    wol_flasher_write_images(WolFlasher* flasher, const WolFlasherImage* images, size_t count) {
    furi_check(flasher && flasher->opened && images);

    File* file = storage_file_alloc(flasher->storage);
    WolFlasherResult result = WolFlasherOk;
    uint64_t total = 0;
    uint64_t done = 0;
    uint8_t last_percent = 255;

    // one progress bar across every image, weighted by size
    for(size_t i = 0; i < count; i++) {
        if(!storage_file_open(file, images[i].path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_free(file);
            return WolFlasherErrFile;
        }
        total += wol_flasher_align4((uint32_t)storage_file_size(file));
        storage_file_close(file);
    }
    if(total == 0) {
        storage_file_free(file);
        return WolFlasherErrFile;
    }

    for(size_t i = 0; i < count && result == WolFlasherOk; i++) {
        if(!storage_file_open(file, images[i].path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            result = WolFlasherErrFile;
            break;
        }

        esp_loader_flash_cfg_t cfg = {
            .offset = images[i].address,
            .image_size = wol_flasher_align4((uint32_t)storage_file_size(file)),
            .block_size = flasher->block_size,
            .skip_verify = false,
        };

        wol_flasher_report(flasher, WolFlasherStageErase, (uint8_t)(done * 100 / total));

        if(esp_loader_flash_start(&flasher->loader, &cfg) != ESP_LOADER_SUCCESS) {
            result = WolFlasherErrFlash;
            storage_file_close(file);
            break;
        }

        for(uint32_t written = 0; written < cfg.image_size;) {
            if(wol_flasher_cancelled(flasher)) {
                result = WolFlasherErrCancelled;
                break;
            }

            uint32_t chunk = cfg.image_size - written;
            if(chunk > flasher->block_size) chunk = flasher->block_size;

            size_t got = storage_file_read(file, flasher->buffer, chunk);
            if(got < chunk) {
                // tail padding for images whose length is not a multiple of 4
                memset(flasher->buffer + got, 0xFF, chunk - got);
            }

            if(esp_loader_flash_write(&flasher->loader, &cfg, flasher->buffer, chunk) !=
               ESP_LOADER_SUCCESS) {
                result = WolFlasherErrFlash;
                break;
            }

            written += chunk;
            done += chunk;

            uint8_t percent = (uint8_t)(done * 100 / total);
            if(percent != last_percent) {
                last_percent = percent;
                wol_flasher_report(flasher, WolFlasherStageWrite, percent);
            }
        }

        storage_file_close(file);

        if(result != WolFlasherOk) break;

        wol_flasher_report(flasher, WolFlasherStageVerify, last_percent);
        esp_loader_error_t finish = esp_loader_flash_finish(&flasher->loader, &cfg);
        if(finish == ESP_LOADER_ERROR_INVALID_MD5) {
            result = WolFlasherErrVerify;
        } else if(finish != ESP_LOADER_SUCCESS) {
            result = WolFlasherErrFlash;
        }
    }

    storage_file_free(file);
    return result;
}
