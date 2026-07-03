/*
 * Flipper-side client for the TagTinker WiFi ESP32 firmware.
 *
 * Owns the USART handle while WiFi Plugins are active, parses framed
 * 0xAA 0x55 packets from the dev board, and exposes a small async API:
 *
 *   - tagtinker_wifi_open()/close()        : grab/release the UART.
 *   - tagtinker_wifi_set_creds(ssid, pwd)  : send WIFI_SET.
 *   - tagtinker_wifi_list_plugins()        : kick off LIST and get plugins
 *                                            via the event callback.
 *   - tagtinker_wifi_run_plugin(...)       : send RUN; result frames stream
 *                                            into the same callback.
 *
 * The caller registers a single callback that is invoked from the FAP's
 * worker thread (not ISR), so it's safe to allocate and call view-dispatcher
 * helpers from inside it.
 */
#ifndef TAGTINKER_WIFI_H
#define TAGTINKER_WIFI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "../shared/tt_wifi_proto_fap.h"

typedef struct TagTinkerWifi TagTinkerWifi;

/* Event types delivered to the user callback. */
typedef enum {
    TtWifiEvtHello, /* HELLO received, fw_name in str0 */
    TtWifiEvtWifiStatus, /* state in u0, rssi in i1, ssid in str0, ip in str1 */
    TtWifiEvtPlugin, /* one parsed manifest (see TagTinkerWifiPlugin*) */
    TtWifiEvtPluginsEnd,
    TtWifiEvtProgress, /* percent in u0, message in str0 */
    TtWifiEvtResultBegin, /* width in u0(low16), height in u0(high16),
                             * planes in u1, total_bytes in u2 */
    TtWifiEvtResultChunk, /* chunk bytes in data/data_len */
    TtWifiEvtResultEnd,
    TtWifiEvtError, /* message in str0 */
    TtWifiEvtLinkLost, /* dev board went silent (>3s) */
} TtWifiEventType;

/* Param specifications mirror what the ESP advertised. */
#define TT_WIFI_MAX_PARAMS  6
#define TT_WIFI_MAX_OPTIONS 8

/* Max plugin manifests the FAP will cache. Each TagTinkerWifiPlugin is
 * ~1.9 KB so the whole cache is ~15 KB at 8 entries - that's the heap
 * cost of opening the WiFi Plugins menu. Bumping this number directly
 * reduces the heap available to the IR transmit pipeline that follows
 * a plugin run, so leave it small. */
#define TT_WIFI_MAX_FAP_PLUGINS 8

typedef struct {
    char key[24];
    char label[24];
    uint8_t type; /* TT_PARAM_* */
    char default_value[64];
    uint8_t option_count;
    char options[TT_WIFI_MAX_OPTIONS][24];
    int32_t int_min;
    int32_t int_max;
} TtWifiParam;

typedef struct {
    uint8_t index;
    char id[24];
    char name[40];
    char description[64];
    uint8_t accent_modes;
    uint8_t param_count;
    TtWifiParam params[TT_WIFI_MAX_PARAMS];
} TagTinkerWifiPlugin;

typedef struct {
    TtWifiEventType type;
    uint32_t u0, u1, u2;
    int32_t i1;
    const char* str0;
    const char* str1;
    const TagTinkerWifiPlugin* plugin; /* TtWifiEvtPlugin only */
    const uint8_t* data;
    uint16_t data_len; /* TtWifiEvtResultChunk only */
} TtWifiEvent;

typedef void (*TtWifiEventCb)(const TtWifiEvent* e, void* user);

TagTinkerWifi* tagtinker_wifi_alloc(TtWifiEventCb cb, void* user);
void tagtinker_wifi_free(TagTinkerWifi* w);

bool tagtinker_wifi_open(TagTinkerWifi* w);
void tagtinker_wifi_close(TagTinkerWifi* w);

/* Hot-swap the event callback. Used so the WiFi-Plugins scene and the
 * WiFi-Run scene can each have their own handler without re-opening the
 * UART. The previous callback is returned in `out_prev_*` if non-NULL. */
void tagtinker_wifi_set_callback(
    TagTinkerWifi* w,
    TtWifiEventCb new_cb,
    void* new_user,
    TtWifiEventCb* out_prev_cb,
    void** out_prev_user);

void tagtinker_wifi_ping(TagTinkerWifi* w);
void tagtinker_wifi_set_creds(TagTinkerWifi* w, const char* ssid, const char* pwd);
void tagtinker_wifi_forget(TagTinkerWifi* w);
void tagtinker_wifi_query_status(TagTinkerWifi* w);
void tagtinker_wifi_list_plugins(TagTinkerWifi* w);

/* Param values is an array of {key, value} pairs; both NUL-terminated. */
typedef struct {
    const char* key;
    const char* value;
} TtWifiKV;
void tagtinker_wifi_run_plugin(
    TagTinkerWifi* w,
    uint8_t plugin_index,
    uint16_t target_w,
    uint16_t target_h,
    uint8_t accent,
    const TtWifiKV* params,
    uint8_t n_params);

#endif /* TAGTINKER_WIFI_H */
