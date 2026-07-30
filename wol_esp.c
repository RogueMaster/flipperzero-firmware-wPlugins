#include "wol_esp.h"

#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define TAG "WolEsp"

#define ESP_SERIAL_ID FuriHalSerialIdUsart
#define ESP_BAUDRATE  115200
#define ESP_RX_BUF    1024
#define ESP_ACC_LIMIT 2048

#define ESP_TOKEN_OK   "\nOK\n"
#define ESP_TOKEN_ERR  "\nERR"

#define ESP_PROGRESS_WIFI_BIT (1 << 0)
#define ESP_PROGRESS_SEND_BIT (1 << 1)

struct WolEsp {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    FuriString* acc;
    FuriString* cmd;
    volatile bool* cancel;
    WolEspProgressCallback progress_callback;
    void* progress_context;
    uint8_t progress_seen;
    bool opened;
};

static void wol_esp_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    WolEsp* esp = ctx;
    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(esp->rx, &byte, 1, 0);
    }
}

static bool wol_esp_cancelled(WolEsp* esp) {
    return esp->cancel && *esp->cancel;
}

static void wol_esp_drain(WolEsp* esp) {
    uint8_t byte;
    while(furi_stream_buffer_receive(esp->rx, &byte, 1, 0) > 0) {
    }
    furi_string_reset(esp->acc);
}

static void wol_esp_write(WolEsp* esp, const void* data, size_t len) {
    furi_hal_serial_tx(esp->serial, data, len);
    furi_hal_serial_tx_wait_complete(esp->serial);
}

/** Fire each progress notice at most once per command. */
static void wol_esp_check_progress(WolEsp* esp) {
    if(!esp->progress_callback) return;

    const char* text = furi_string_get_cstr(esp->acc);

    if(!(esp->progress_seen & ESP_PROGRESS_WIFI_BIT) && strstr(text, "+WIFI CONNECTING")) {
        esp->progress_seen |= ESP_PROGRESS_WIFI_BIT;
        esp->progress_callback(esp->progress_context, WolEspProgressWifi);
    }
    if(!(esp->progress_seen & ESP_PROGRESS_SEND_BIT) && strstr(text, "+SEND")) {
        esp->progress_seen |= ESP_PROGRESS_SEND_BIT;
        esp->progress_callback(esp->progress_context, WolEspProgressSending);
    }
}

/**
 * Collect the reply until the terminating OK or ERR line shows up.
 * The accumulator is primed with a newline by wol_esp_cmd() so both tokens can
 * be anchored to a line start, otherwise the OK inside "+WIFI OK 1.2.3.4"
 * would terminate the wait early.
 */
static bool wol_esp_wait(WolEsp* esp, uint32_t timeout_ms) {
    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(timeout_ms);
    uint8_t buf[64];

    while(furi_get_tick() < deadline) {
        if(wol_esp_cancelled(esp)) return false;

        size_t len = furi_stream_buffer_receive(esp->rx, buf, sizeof(buf), furi_ms_to_ticks(50));
        if(len == 0) continue;

        for(size_t i = 0; i < len; i++) {
            if(buf[i] != 0) furi_string_push_back(esp->acc, buf[i]);
        }

        wol_esp_check_progress(esp);

        const char* text = furi_string_get_cstr(esp->acc);
        if(strstr(text, ESP_TOKEN_OK)) return true;
        if(strstr(text, ESP_TOKEN_ERR)) return false;

        if(furi_string_size(esp->acc) > ESP_ACC_LIMIT) {
            furi_string_right(esp->acc, furi_string_size(esp->acc) - 256);
        }
    }
    return false;
}

static void wol_esp_cmd(WolEsp* esp, const char* format, ...) {
    va_list args;
    va_start(args, format);
    furi_string_vprintf(esp->cmd, format, args);
    va_end(args);

    FURI_LOG_D(TAG, "> %s", furi_string_get_cstr(esp->cmd));
    furi_string_cat_str(esp->cmd, "\n");

    wol_esp_drain(esp);
    esp->progress_seen = 0;
    furi_string_push_back(esp->acc, '\n');

    wol_esp_write(esp, furi_string_get_cstr(esp->cmd), furi_string_size(esp->cmd));
}

/** Map the ERR line the board sent, or a silence, onto a result code. */
static WolEspResult wol_esp_error(WolEsp* esp) {
    const char* text = furi_string_get_cstr(esp->acc);

    if(strstr(text, "ERR WIFI")) return WolEspErrWifi;
    if(strstr(text, "ERR UDP")) return WolEspErrUdp;
    if(strstr(text, "ERR ARGS")) return WolEspErrArgs;
    if(strstr(text, "ERR CMD")) return WolEspErrWrongFirmware;

    /* Latches the charger's fault bits and drops OTG if the boost gave up.
     * Association is the current peak of the whole session, so this is where
     * a marginal 5V rail lets go. */
    furi_hal_power_check_otg_status();
    if(!furi_hal_power_is_otg_enabled()) return WolEspErrPower;

    /* The firmware prints its banner on every boot. Seeing one mid command
     * means the board restarted underneath us: a brownout the charger did not
     * flag, or a crash. */
    if(strstr(text, "+WOLFW ")) return WolEspErrReboot;

    return WolEspErrNoReply;
}

WolEsp* wol_esp_alloc(volatile bool* cancel) {
    WolEsp* esp = malloc(sizeof(WolEsp));
    esp->serial = NULL;
    esp->rx = furi_stream_buffer_alloc(ESP_RX_BUF, 1);
    esp->acc = furi_string_alloc();
    esp->cmd = furi_string_alloc();
    esp->cancel = cancel;
    esp->progress_callback = NULL;
    esp->progress_context = NULL;
    esp->progress_seen = 0;
    esp->opened = false;
    return esp;
}

void wol_esp_free(WolEsp* esp) {
    furi_check(esp);
    wol_esp_close(esp);
    furi_string_free(esp->cmd);
    furi_string_free(esp->acc);
    furi_stream_buffer_free(esp->rx);
    free(esp);
}

void wol_esp_set_progress_callback(WolEsp* esp, WolEspProgressCallback callback, void* context) {
    furi_check(esp);
    esp->progress_callback = callback;
    esp->progress_context = context;
}

bool wol_esp_open(WolEsp* esp) {
    furi_check(esp);
    if(esp->opened) return true;

    // 5V is owned by the app, not by individual operations: cycling it here
    // rebooted the board before every single command
    esp->serial = furi_hal_serial_control_acquire(ESP_SERIAL_ID);
    if(!esp->serial) {
        FURI_LOG_E(TAG, "USART busy");
        return false;
    }

    furi_hal_serial_init(esp->serial, ESP_BAUDRATE);
    furi_hal_serial_async_rx_start(esp->serial, wol_esp_rx_callback, esp, false);
    esp->opened = true;
    wol_esp_drain(esp);
    return true;
}

void wol_esp_close(WolEsp* esp) {
    furi_check(esp);

    if(esp->opened) {
        furi_hal_serial_async_rx_stop(esp->serial);
        furi_hal_serial_deinit(esp->serial);
        furi_hal_serial_control_release(esp->serial);
        esp->serial = NULL;
        esp->opened = false;
    }
}

WolEspResult wol_esp_ping(WolEsp* esp, uint8_t* version) {
    furi_check(esp && esp->opened);

    bool heard_something = false;

    /* Retry across the whole boot window. A board that just got power spends a
     * second in the ROM loader and then prints its own banner, so early bytes
     * mean nothing on their own and must not be mistaken for a foreign
     * firmware. The banner carries the same marker as the reply, so either one
     * is proof enough. */
    for(size_t attempt = 0; attempt < 5; attempt++) {
        wol_esp_cmd(esp, "PING");
        bool answered = wol_esp_wait(esp, 1000);

        const char* marker = strstr(furi_string_get_cstr(esp->acc), "+WOLFW ");
        if(marker) {
            if(version) *version = (uint8_t)atoi(marker + 7);
            return WolEspOk;
        }
        if(answered) return WolEspErrWrongFirmware; // said OK without naming itself

        if(furi_string_size(esp->acc) > 1) heard_something = true;
        if(wol_esp_cancelled(esp)) break;
    }

    return heard_something ? WolEspErrWrongFirmware : WolEspErrNoReply;
}

WolEspResult wol_esp_join(WolEsp* esp, const char* ssid, const char* pass) {
    furi_check(esp && esp->opened);
    if(!ssid || ssid[0] == '\0') return WolEspErrArgs;

    wol_esp_cmd(esp, "JOIN\t%s\t%s", ssid, pass ? pass : "");
    if(!wol_esp_wait(esp, 25000)) return wol_esp_error(esp);
    return WolEspOk;
}

WolEspResult wol_esp_wake(
    WolEsp* esp,
    const char* ssid,
    const char* pass,
    const uint8_t mac[6],
    const char* ip,
    uint16_t port) {
    furi_check(esp && esp->opened);
    if(!ssid || ssid[0] == '\0') return WolEspErrArgs;

    wol_esp_cmd(
        esp,
        "WOL\t%s\t%s\t%02X:%02X:%02X:%02X:%02X:%02X\t%s\t%u",
        ssid,
        pass ? pass : "",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5],
        ip,
        port);

    if(!wol_esp_wait(esp, 30000)) return wol_esp_error(esp);
    return WolEspOk;
}
