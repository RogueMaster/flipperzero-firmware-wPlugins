#include "yt_wifi.h"

#include "vendor/flipper_http/flipper_http.h"

#include <expansion/expansion.h>
#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YT_WIFI_BODY_SIZE  2048U
#define YT_WIFI_PING_MS    1000U
#define YT_WIFI_PING_TRIES 3U
#define YT_WIFI_JOIN_MS    20000U
#define YT_WIFI_GET_MS     25000U

struct YtWifi {
    FlipperHTTP* fhttp;
    Expansion* expansion;
    FuriMutex* mutex;
    YtWifiState state;
    uint32_t state_tick;
    uint8_t ping_tries;
    bool tried_save;
    char ssid[33];
    char password[65];

    /* Written by the FlipperHTTP RX thread, guarded by mutex. */
    bool pong;
    bool joined;
    bool failed;
    bool receiving;
    bool done;
    bool overflow;
    uint16_t http_status;
    char error[96];
    char body[YT_WIFI_BODY_SIZE];
    size_t body_length;
};

static uint32_t yt_wifi_elapsed_ms(uint32_t since) {
    return (uint32_t)((uint64_t)(furi_get_tick() - since) * 1000U /
                      furi_kernel_get_tick_frequency());
}

static bool yt_wifi_starts_with(const char* line, const char* prefix) {
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

/* Called by FlipperHTTP on its RX thread for every non-empty, trimmed line.
 * A GET answer is "[GET/SUCCESS]{"Status-Code":200,...}", the body lines and
 * "[GET/END]"; failures are "[ERROR] <reason>". */
static void yt_wifi_on_line(const char* line, void* context) {
    YtWifi* wifi = context;
    furi_mutex_acquire(wifi->mutex, FuriWaitForever);
    if(strstr(line, "[PONG]")) {
        wifi->pong = true;
    } else if(yt_wifi_starts_with(line, "[GET/SUCCESS]")) {
        const char* code = strstr(line, "\"Status-Code\":");
        wifi->http_status = code ? (uint16_t)atoi(code + strlen("\"Status-Code\":")) : 0U;
        wifi->receiving = true;
        wifi->overflow = false;
        wifi->body_length = 0;
        wifi->body[0] = '\0';
    } else if(strstr(line, "[GET/END]")) {
        if(wifi->receiving) {
            wifi->receiving = false;
            wifi->done = true;
        }
    } else if(wifi->receiving) {
        size_t length = strlen(line);
        if(wifi->body_length + length + 2U > sizeof(wifi->body)) {
            wifi->overflow = true;
        } else {
            memcpy(wifi->body + wifi->body_length, line, length);
            wifi->body_length += length;
            wifi->body[wifi->body_length++] = '\n';
            wifi->body[wifi->body_length] = '\0';
        }
    } else if(yt_wifi_starts_with(line, "[ERROR]")) {
        const char* reason = line + strlen("[ERROR]");
        while(*reason == ' ')
            ++reason;
        strlcpy(wifi->error, reason, sizeof(wifi->error));
        wifi->failed = true;
    } else if(yt_wifi_starts_with(line, "[SUCCESS]") || strstr(line, "Already connected")) {
        wifi->joined = true;
    }
    furi_mutex_release(wifi->mutex);
}

YtWifi* yt_wifi_alloc(void) {
    YtWifi* wifi = calloc(1, sizeof(YtWifi));
    if(!wifi) return NULL;
    /* The expansion-module service listens on the same UART; pause it. */
    wifi->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(wifi->expansion);
    wifi->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    wifi->fhttp = flipper_http_alloc();
    if(!wifi->fhttp) {
        furi_mutex_free(wifi->mutex);
        expansion_enable(wifi->expansion);
        furi_record_close(RECORD_EXPANSION);
        free(wifi);
        return NULL;
    }
    wifi->fhttp->user_rx_line_cb = yt_wifi_on_line;
    wifi->fhttp->user_callback_context = wifi;
    wifi->state = YtWifiNoBoard;
    return wifi;
}

void yt_wifi_free(YtWifi* wifi) {
    if(!wifi) return;
    /* Joins the RX thread, so no callback can run after this. */
    flipper_http_free(wifi->fhttp);
    furi_mutex_free(wifi->mutex);
    expansion_enable(wifi->expansion);
    furi_record_close(RECORD_EXPANSION);
    free(wifi);
}

static void yt_wifi_set_state(YtWifi* wifi, YtWifiState state) {
    wifi->state = state;
    wifi->state_tick = furi_get_tick();
}

void yt_wifi_start(YtWifi* wifi, const char* ssid, const char* password) {
    strlcpy(wifi->ssid, ssid ? ssid : "", sizeof(wifi->ssid));
    strlcpy(wifi->password, password ? password : "", sizeof(wifi->password));
    furi_mutex_acquire(wifi->mutex, FuriWaitForever);
    wifi->pong = wifi->joined = wifi->failed = wifi->receiving = wifi->done = false;
    wifi->error[0] = '\0';
    furi_mutex_release(wifi->mutex);
    wifi->tried_save = false;
    wifi->ping_tries = 1;
    yt_wifi_set_state(wifi, YtWifiStarting);
    flipper_http_send_command(wifi->fhttp, HTTP_CMD_PING);
}

void yt_wifi_tick(YtWifi* wifi) {
    furi_mutex_acquire(wifi->mutex, FuriWaitForever);
    const bool pong = wifi->pong;
    const bool joined = wifi->joined;
    const bool failed = wifi->failed;
    const bool done = wifi->done;
    furi_mutex_release(wifi->mutex);
    const uint32_t elapsed = yt_wifi_elapsed_ms(wifi->state_tick);

    switch(wifi->state) {
    case YtWifiStarting:
        if(pong) {
            /* Join the network the board has saved first; this does not
             * rewrite the board's flash on every start. */
            yt_wifi_set_state(wifi, YtWifiConnecting);
            flipper_http_send_command(wifi->fhttp, HTTP_CMD_WIFI_CONNECT);
        } else if(elapsed >= YT_WIFI_PING_MS) {
            if(wifi->ping_tries < YT_WIFI_PING_TRIES) {
                ++wifi->ping_tries;
                yt_wifi_set_state(wifi, YtWifiStarting);
                flipper_http_send_command(wifi->fhttp, HTTP_CMD_PING);
            } else {
                yt_wifi_set_state(wifi, YtWifiNoBoard);
            }
        }
        break;
    case YtWifiConnecting:
        if(joined) {
            yt_wifi_set_state(wifi, YtWifiReady);
        } else if(failed && wifi->ssid[0] && !wifi->tried_save) {
            /* No saved network or it is out of range: store ours and join. */
            wifi->tried_save = true;
            furi_mutex_acquire(wifi->mutex, FuriWaitForever);
            wifi->failed = false;
            furi_mutex_release(wifi->mutex);
            yt_wifi_set_state(wifi, YtWifiConnecting);
            flipper_http_save_wifi(wifi->fhttp, wifi->ssid, wifi->password);
        } else if(failed || elapsed >= YT_WIFI_JOIN_MS) {
            yt_wifi_set_state(wifi, YtWifiFailed);
        }
        break;
    case YtWifiFetching:
        if(done || failed) {
            yt_wifi_set_state(wifi, YtWifiDone);
        } else if(elapsed >= YT_WIFI_GET_MS) {
            furi_mutex_acquire(wifi->mutex, FuriWaitForever);
            wifi->receiving = false;
            wifi->failed = true;
            strlcpy(wifi->error, "Request timed out", sizeof(wifi->error));
            furi_mutex_release(wifi->mutex);
            yt_wifi_set_state(wifi, YtWifiDone);
        }
        break;
    default:
        break;
    }
}

YtWifiState yt_wifi_state(YtWifi* wifi) {
    return wifi->state;
}

const char* yt_wifi_error(YtWifi* wifi) {
    return wifi->error;
}

bool yt_wifi_get(YtWifi* wifi, const char* url) {
    if(wifi->state != YtWifiReady) return false;
    furi_mutex_acquire(wifi->mutex, FuriWaitForever);
    wifi->failed = wifi->done = wifi->receiving = wifi->overflow = false;
    wifi->http_status = 0;
    wifi->error[0] = '\0';
    wifi->body_length = 0;
    wifi->body[0] = '\0';
    furi_mutex_release(wifi->mutex);
    if(!flipper_http_request(wifi->fhttp, GET, url, NULL, NULL)) return false;
    yt_wifi_set_state(wifi, YtWifiFetching);
    return true;
}

bool yt_wifi_take_result(
    YtWifi* wifi,
    char* body,
    size_t size,
    uint16_t* http_status,
    char* error,
    size_t error_size) {
    furi_mutex_acquire(wifi->mutex, FuriWaitForever);
    const bool fits = !wifi->overflow && wifi->body_length < size;
    strlcpy(body, wifi->body, size);
    *http_status = wifi->done ? wifi->http_status : 0U;
    strlcpy(error, wifi->failed ? wifi->error : "", error_size);
    wifi->done = wifi->failed = false;
    furi_mutex_release(wifi->mutex);
    yt_wifi_set_state(wifi, YtWifiReady);
    return fits;
}
