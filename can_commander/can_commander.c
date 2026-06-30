#include "can_commander.h"
#include "views/dashboard/dashboard.h"

#include <furi_hal_power.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define APP_MONITOR_KEEP_MAX   1500U
#define APP_MONITOR_KEEP_TAIL  1000U
#define APP_ARGS_KV_WORK_MAX   (APP_CUSTOM_INJECT_SLOT_ARGS_MAX + 128U)

#define APP_RX_THREAD_STACK       (3U * 1024U)
#define APP_RX_THREAD_INTERVAL    5U
#define APP_RX_EVENT_BUDGET       192U
#define APP_RX_EVENT_YIELD_EVERY  24U
#define APP_RX_DASH_EVENT_BUDGET  16U
#define APP_UI_TICK_INTERVAL_MS   50U

#define APP_CUSTOM_INJECT_CFG_PATH APP_DATA_PATH("custom_inject.cfg")
#define APP_CUSTOM_INJECT_CFG_TYPE "CANCommanderCustomInject"
#define APP_CUSTOM_INJECT_CFG_VER  1U
#define APP_CUSTOM_INJECT_SET_DIR         APP_SMART_INJECT_PROFILE_DIR
#define APP_CUSTOM_INJECT_SET_LEGACY_DIR  APP_SMART_INJECT_PROFILE_LEGACY_DIR
#define APP_CUSTOM_INJECT_SET_TYPE        APP_SMART_INJECT_PROFILE_FILETYPE
#define APP_CUSTOM_INJECT_SET_LEGACY_TYPE APP_SMART_INJECT_PROFILE_LEGACY_FILETYPE
#define APP_CUSTOM_INJECT_SET_VER         APP_SMART_INJECT_PROFILE_VER
#define APP_DBC_CONFIG_DIR                APP_DBC_DECODE_PROFILE_DIR
#define APP_DBC_CONFIG_LEGACY_DIR         APP_DBC_DECODE_PROFILE_LEGACY_DIR
#define APP_DBC_CONFIG_TYPE               APP_DBC_DECODE_PROFILE_FILETYPE
#define APP_DBC_CONFIG_LEGACY_TYPE        APP_DBC_DECODE_PROFILE_LEGACY_FILETYPE
#define APP_DBC_CONFIG_VER                APP_DBC_DECODE_PROFILE_VER

static void app_free(App* app);

static bool cancommander_scene_custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool cancommander_scene_back_event_callback(void* context) {
    App* app = context;

    if(
        scene_manager_get_current_scene(app->scene_manager) == cancommander_scene_monitor &&
        app->tool_active) {
        app_append_monitor(app, "[auto] back pressed => tool stop");
        app_action_tool_stop(app);
    }

    return scene_manager_handle_back_event(app->scene_manager);
}

static void cancommander_scene_tick_callback(void* context) {
    App* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static inline uint16_t read_u16_le(const uint8_t* src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline uint32_t read_u32_le(const uint8_t* src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static bool arg_get_value(const char* args, const char* key, char* out, size_t out_size) {
    if(!args || !key || !out || out_size == 0U) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char* p = args;

    while(*p) {
        while(*p == ' ') {
            p++;
        }

        if(*p == '\0') {
            break;
        }

        const char* token_start = p;
        while(*p != '\0' && *p != ' ') {
            p++;
        }

        const size_t token_len = (size_t)(p - token_start);

        if(token_len > (key_len + 1U) && strncmp(token_start, key, key_len) == 0 &&
           token_start[key_len] == '=') {
            size_t value_len = token_len - (key_len + 1U);
            if(value_len >= out_size) {
                value_len = out_size - 1U;
            }

            memcpy(out, token_start + key_len + 1U, value_len);
            out[value_len] = '\0';
            return true;
        }
    }

    return false;
}

static bool arg_get_value_last(const char* args, const char* key, char* out, size_t out_size) {
    if(!args || !key || !out || out_size == 0U) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char* p = args;
    bool found = false;

    while(*p) {
        while(*p == ' ') {
            p++;
        }

        if(*p == '\0') {
            break;
        }

        const char* token_start = p;
        while(*p != '\0' && *p != ' ') {
            p++;
        }

        const size_t token_len = (size_t)(p - token_start);

        if(token_len > (key_len + 1U) && strncmp(token_start, key, key_len) == 0 &&
           token_start[key_len] == '=') {
            size_t value_len = token_len - (key_len + 1U);
            if(value_len >= out_size) {
                value_len = out_size - 1U;
            }

            memcpy(out, token_start + key_len + 1U, value_len);
            out[value_len] = '\0';
            found = true;
        }
    }

    return found;
}

static bool parse_u32_key(const char* args, const char* key, uint32_t base, uint32_t* out) {
    char value[48] = {0};
    if(!arg_get_value(args, key, value, sizeof(value))) {
        return false;
    }

    char* end = NULL;
    unsigned long parsed = strtoul(value, &end, base);
    if(end == value || *end != '\0') {
        return false;
    }

    *out = (uint32_t)parsed;
    return true;
}

static bool parse_u32_key_last(const char* args, const char* key, uint32_t base, uint32_t* out) {
    char value[48] = {0};
    if(!arg_get_value_last(args, key, value, sizeof(value))) {
        return false;
    }

    char* end = NULL;
    unsigned long parsed = strtoul(value, &end, base);
    if(end == value || *end != '\0') {
        return false;
    }

    *out = (uint32_t)parsed;
    return true;
}

static bool parse_float_key(const char* args, const char* key, float* out) {
    char value[48] = {0};
    if(!arg_get_value(args, key, value, sizeof(value))) {
        return false;
    }

    char* end = NULL;
    float parsed = strtof(value, &end);
    if(end == value || *end != '\0') {
        return false;
    }

    *out = parsed;
    return true;
}

static bool parse_i64_text(const char* text, int64_t* out) {
    if(!text || !out || text[0] == '\0') {
        return false;
    }

    bool negative = false;
    const char* p = text;
    if(*p == '+' || *p == '-') {
        negative = (*p == '-');
        p++;
    }
    if(*p == '\0') {
        return false;
    }

    char* end = NULL;
    unsigned long long magnitude = strtoull(p, &end, 0);
    if(end == p || *end != '\0') {
        return false;
    }

    if(!negative) {
        if(magnitude > 0x7FFFFFFFFFFFFFFFULL) {
            return false;
        }
        *out = (int64_t)magnitude;
        return true;
    }

    if(magnitude > 0x8000000000000000ULL) {
        return false;
    }
    if(magnitude == 0x8000000000000000ULL) {
        *out = INT64_MIN;
    } else {
        *out = -(int64_t)magnitude;
    }
    return true;
}

static bool parse_bool_key(const char* args, const char* key, bool* out) {
    char value[16] = {0};
    if(!arg_get_value(args, key, value, sizeof(value))) {
        return false;
    }

    if(strcmp(value, "1") == 0 || strcmp(value, "true") == 0) {
        *out = true;
        return true;
    }

    if(strcmp(value, "0") == 0 || strcmp(value, "false") == 0) {
        *out = false;
        return true;
    }

    return false;
}

static bool parse_bool_key_last(const char* args, const char* key, bool* out) {
    char value[16] = {0};
    if(!arg_get_value_last(args, key, value, sizeof(value))) {
        return false;
    }

    if(strcmp(value, "1") == 0 || strcmp(value, "true") == 0) {
        *out = true;
        return true;
    }

    if(strcmp(value, "0") == 0 || strcmp(value, "false") == 0) {
        *out = false;
        return true;
    }

    return false;
}

static bool parse_bus_key(const char* args, const char* key, CcBus* out) {
    char value[16] = {0};
    if(!arg_get_value(args, key, value, sizeof(value))) {
        return false;
    }

    if(strcmp(value, "can0") == 0) {
        *out = CcBusCan0;
        return true;
    }

    if(strcmp(value, "can1") == 0) {
        *out = CcBusCan1;
        return true;
    }

    if(strcmp(value, "both") == 0) {
        *out = CcBusBoth;
        return true;
    }

    return false;
}

static void app_copy_string(char* dst, size_t dst_size, const char* src) {
    if(!dst || dst_size == 0U) {
        return;
    }

    if(!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static void app_custom_inject_slot_defaults(uint8_t slot_index, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    snprintf(
        out,
        out_size,
        "slot_name=Slot%u used=0 bus=can0 id=000 ext=0 mask=0000000000000000 value=0000000000000000 xor=0000000000000000 mux=0 mux_start=0 mux_len=1 mux_value=0 sig=0 sig_start=0 sig_len=1 sig_value=0 count=1 interval_ms=0",
        (unsigned)(slot_index + 1U));
    out[out_size - 1U] = '\0';
}

static char* app_custom_inject_slot_ptr(App* app, uint8_t slot_index) {
    if(!app || slot_index >= 5U) {
        return NULL;
    }

    return app->args_custom_inject_slots[slot_index];
}

static const char* app_custom_inject_slot_cptr(const App* app, uint8_t slot_index) {
    if(!app || slot_index >= 5U) {
        return NULL;
    }

    return app->args_custom_inject_slots[slot_index];
}

static void app_custom_inject_mark_all_unprovisioned(App* app) {
    if(!app) {
        return;
    }

    memset(app->custom_inject_slot_provisioned, 0, sizeof(app->custom_inject_slot_provisioned));
}

static bool app_custom_inject_slot_is_provisioned(const App* app, uint8_t slot_number) {
    if(!app || slot_number < 1U || slot_number > 5U) {
        return false;
    }

    return app->custom_inject_slot_provisioned[slot_number - 1U];
}

static void app_custom_inject_set_slot_provisioned(App* app, uint8_t slot_number, bool provisioned) {
    if(!app || slot_number < 1U || slot_number > 5U) {
        return;
    }

    app->custom_inject_slot_provisioned[slot_number - 1U] = provisioned;
}

void app_custom_inject_set_active_slot(App* app, uint8_t slot_index) {
    if(!app) {
        return;
    }

    app->custom_inject_active_slot = (slot_index < 5U) ? slot_index : 0U;
}

uint8_t app_custom_inject_get_active_slot(const App* app) {
    if(!app || app->custom_inject_active_slot >= 5U) {
        return 0U;
    }

    return app->custom_inject_active_slot;
}

char* app_custom_inject_get_slot_args(App* app, uint8_t slot_index) {
    return app_custom_inject_slot_ptr(app, slot_index);
}

void app_custom_inject_reset_slot(App* app, uint8_t slot_index) {
    char* slot = app_custom_inject_slot_ptr(app, slot_index);
    if(!slot) {
        return;
    }

    app_custom_inject_slot_defaults(slot_index, slot, sizeof(app->args_custom_inject_slots[0]));
}

void app_custom_inject_reset_all_slots(App* app) {
    if(!app) {
        return;
    }

    for(uint8_t i = 0; i < 5U; i++) {
        app_custom_inject_reset_slot(app, i);
    }

    app_custom_inject_mark_all_unprovisioned(app);
}

static bool str_equals_ignore_case(const char* a, const char* b) {
    if(!a || !b) {
        return false;
    }

    while(*a && *b) {
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
}

static bool app_parse_obd_pid_token(const char* token, uint8_t* out_pid) {
    if(!token || !out_pid || token[0] == '\0') {
        return false;
    }

    char* end = NULL;
    unsigned long parsed = strtoul(token, &end, 16);
    if(end != token && *end == '\0' && parsed <= 0xFFUL) {
        *out_pid = (uint8_t)parsed;
        return true;
    }

    if(str_equals_ignore_case(token, "rpm") || str_equals_ignore_case(token, "engine_rpm")) {
        *out_pid = 0x0CU;
        return true;
    }

    if(str_equals_ignore_case(token, "speed") || str_equals_ignore_case(token, "vehicle_speed")) {
        *out_pid = 0x0DU;
        return true;
    }

    if(
        str_equals_ignore_case(token, "coolant") ||
        str_equals_ignore_case(token, "coolant_temp") || str_equals_ignore_case(token, "temp")) {
        *out_pid = 0x05U;
        return true;
    }

    if(
        str_equals_ignore_case(token, "throttle") ||
        str_equals_ignore_case(token, "throttle_position")) {
        *out_pid = 0x11U;
        return true;
    }

    if(str_equals_ignore_case(token, "load") || str_equals_ignore_case(token, "engine_load")) {
        *out_pid = 0x04U;
        return true;
    }

    if(str_equals_ignore_case(token, "fuel") || str_equals_ignore_case(token, "fuel_level")) {
        *out_pid = 0x2FU;
        return true;
    }

    if(str_equals_ignore_case(token, "iat") || str_equals_ignore_case(token, "intake_temp")) {
        *out_pid = 0x0FU;
        return true;
    }

    if(
        str_equals_ignore_case(token, "baro") ||
        str_equals_ignore_case(token, "barometric_pressure")) {
        *out_pid = 0x33U;
        return true;
    }

    if(str_equals_ignore_case(token, "odometer")) {
        *out_pid = 0xA6U;
        return true;
    }

    return false;
}

static bool app_normalize_obd_pid_token(
    const char* token,
    char* out,
    size_t out_size,
    bool* out_mode01) {
    if(!out || out_size == 0U) {
        return false;
    }

    out[0] = '\0';
    if(out_mode01) {
        *out_mode01 = false;
    }

    if(!token || token[0] == '\0') {
        return false;
    }

    uint8_t pid = 0U;
    if(app_parse_obd_pid_token(token, &pid)) {
        snprintf(out, out_size, "%02X", pid);
        out[out_size - 1U] = '\0';
        if(out_mode01) {
            *out_mode01 = true;
        }
        return true;
    }

    if(str_equals_ignore_case(token, "vin")) {
        app_copy_string(out, out_size, "vin");
        return true;
    }

    if(str_equals_ignore_case(token, "dtc") || str_equals_ignore_case(token, "dtc_all")) {
        app_copy_string(out, out_size, "dtc_all");
        return true;
    }

    if(
        str_equals_ignore_case(token, "dtc_stored") ||
        str_equals_ignore_case(token, "stored_dtc") ||
        str_equals_ignore_case(token, "stored_dtcs")) {
        app_copy_string(out, out_size, "dtc_stored");
        return true;
    }

    if(
        str_equals_ignore_case(token, "dtc_pending") ||
        str_equals_ignore_case(token, "pending_dtc") ||
        str_equals_ignore_case(token, "pending_dtcs")) {
        app_copy_string(out, out_size, "dtc_pending");
        return true;
    }

    if(
        str_equals_ignore_case(token, "dtc_permanent") ||
        str_equals_ignore_case(token, "permanent_dtc") ||
        str_equals_ignore_case(token, "permanent_dtcs")) {
        app_copy_string(out, out_size, "dtc_permanent");
        return true;
    }

    if(
        str_equals_ignore_case(token, "clear_dtc") ||
        str_equals_ignore_case(token, "clear_codes") ||
        str_equals_ignore_case(token, "clear_dtcs") ||
        str_equals_ignore_case(token, "clear_mil") ||
        str_equals_ignore_case(token, "cel_reset")) {
        app_copy_string(out, out_size, "clear_dtcs");
        return true;
    }

    return false;
}

static void format_payload_hex(const uint8_t* data, uint16_t len, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    size_t pos = 0;

    for(uint16_t i = 0; i < len; i++) {
        if(pos + 3U >= out_size) {
            break;
        }

        const int w = snprintf(out + pos, out_size - pos, "%02X", data[i]);
        if(w <= 0) {
            break;
        }

        pos += (size_t)w;
    }
}

static void app_format_event_line(const CcEvent* event, char* out, size_t out_size) {
    if(!event || !out || out_size == 0U) {
        return;
    }

    out[0] = '\0';

    switch(event->type) {
    case CcEventTypeCanFrame: {
        char data_hex[32] = {0};
        size_t pos = 0;

        for(uint8_t i = 0; i < event->data.can_frame.dlc && i < 8; i++) {
            if(pos + 3U >= sizeof(data_hex)) {
                break;
            }
            const int w =
                snprintf(data_hex + pos, sizeof(data_hex) - pos, "%02X", event->data.can_frame.data[i]);
            if(w <= 0) {
                break;
            }
            pos += (size_t)w;
        }

        snprintf(
            out,
            out_size,
            "[frame] %s id=0x%lX %s dlc=%u data=%s",
            cc_bus_to_string(event->data.can_frame.bus),
            (unsigned long)event->data.can_frame.id,
            event->data.can_frame.ext ? "ext" : "std",
            event->data.can_frame.dlc,
            data_hex);
        break;
    }

    case CcEventTypeTool:
        if(event->data.tool.text_len > 0U) {
            snprintf(
                out,
                out_size,
                "[tool] %s code=%u %s",
                cc_tool_to_string(event->data.tool.tool),
                (unsigned)event->data.tool.code,
                event->data.tool.text);
        } else {
            snprintf(
                out,
                out_size,
                "[tool] %s code=%u (empty)",
                cc_tool_to_string(event->data.tool.tool),
                (unsigned)event->data.tool.code);
        }
        break;

    case CcEventTypeDbcDecode:
        snprintf(
            out,
            out_size,
            "[dbc] sid=%u %s id=0x%lX value=%.4f %s %s",
            event->data.dbc_decode.sid,
            cc_bus_to_string(event->data.dbc_decode.bus),
            (unsigned long)event->data.dbc_decode.frame_id,
            (double)event->data.dbc_decode.value,
            event->data.dbc_decode.unit,
            event->data.dbc_decode.in_range ? "ok" : "oor");
        break;

    case CcEventTypeStatus: {
        if(event->data.status.len == 16) {
            const CcBus bus = (CcBus)event->data.status.payload[0];
            const uint32_t bitrate = read_u32_le(&event->data.status.payload[1]);
            const bool listen = event->data.status.payload[5] != 0;
            const bool filter_enabled = event->data.status.payload[6] != 0;
            const uint32_t mask = read_u32_le(&event->data.status.payload[7]);
            const uint32_t filter = read_u32_le(&event->data.status.payload[11]);
            const bool ext = event->data.status.payload[15] != 0;

            snprintf(
                out,
                out_size,
                "[cfg] %s bitrate=%lu mode=%s filter=%u mask=0x%lX value=0x%lX ext=%u",
                cc_bus_to_string(bus),
                (unsigned long)bitrate,
                listen ? "listen" : "normal",
                filter_enabled ? 1U : 0U,
                (unsigned long)mask,
                (unsigned long)filter,
                ext ? 1U : 0U);
            break;
        }

        if(event->data.status.len == 44) {
            const uint32_t cli_produced = read_u32_le(&event->data.status.payload[0]);
            const uint32_t cli_sent = read_u32_le(&event->data.status.payload[4]);
            const uint32_t cli_dropped = read_u32_le(&event->data.status.payload[8]);
            const uint32_t flipper_produced = read_u32_le(&event->data.status.payload[12]);
            const uint32_t flipper_sent = read_u32_le(&event->data.status.payload[16]);
            const uint32_t flipper_dropped = read_u32_le(&event->data.status.payload[20]);
            const uint32_t evt_can = read_u32_le(&event->data.status.payload[24]);
            const uint32_t evt_tool = read_u32_le(&event->data.status.payload[28]);
            const uint32_t evt_dbc = read_u32_le(&event->data.status.payload[32]);
            const uint32_t evt_status = read_u32_le(&event->data.status.payload[36]);
            const uint32_t evt_drops = read_u32_le(&event->data.status.payload[40]);

            snprintf(
                out,
                out_size,
                "[stats] cli:%lu/%lu drop:%lu flip:%lu/%lu drop:%lu evt can=%lu tool=%lu dbc=%lu status=%lu drops=%lu",
                (unsigned long)cli_produced,
                (unsigned long)cli_sent,
                (unsigned long)cli_dropped,
                (unsigned long)flipper_produced,
                (unsigned long)flipper_sent,
                (unsigned long)flipper_dropped,
                (unsigned long)evt_can,
                (unsigned long)evt_tool,
                (unsigned long)evt_dbc,
                (unsigned long)evt_status,
                (unsigned long)evt_drops);
            break;
        }

        if(event->data.status.len == 4) {
            const uint8_t owner = event->data.status.payload[0];
            const uint8_t active_tool = event->data.status.payload[1];
            const uint16_t dbc_count = read_u16_le(&event->data.status.payload[2]);
            snprintf(
                out,
                out_size,
                "[status] owner=%u active_tool=%s dbc_count=%u",
                (unsigned)owner,
                cc_tool_to_string((CcToolId)active_tool),
                (unsigned)dbc_count);
            break;
        }

        if(event->data.status.len == 2) {
            const uint16_t dbc_count = read_u16_le(event->data.status.payload);
            snprintf(out, out_size, "[dbc] count=%u", (unsigned)dbc_count);
            break;
        }

        char payload_hex[96] = {0};
        const uint16_t dump_len = event->data.status.len > 32U ? 32U : event->data.status.len;
        format_payload_hex(event->data.status.payload, dump_len, payload_hex, sizeof(payload_hex));
        snprintf(out, out_size, "[status] len=%u data=%s", event->data.status.len, payload_hex);
        break;
    }

    case CcEventTypeDrops:
        snprintf(
            out,
            out_size,
            "[drops] cli=%lu flipper=%lu",
            (unsigned long)event->data.drops.cli_dropped,
            (unsigned long)event->data.drops.flipper_dropped);
        break;

    case CcEventTypeUnknown:
        snprintf(
            out,
            out_size,
            "[event] unknown id=0x%02X len=%u",
            (unsigned)event->id,
            (unsigned)event->raw_len);
        break;

    default:
        snprintf(out, out_size, "[event] type=%u", (unsigned)event->type);
        break;
    }
}

static int32_t app_rx_worker_thread(void* context) {
    App* app = context;

    while(!app->rx_worker_stop) {
        if(app->connected) {
            if(!cc_client_poll(app->client, APP_RX_THREAD_INTERVAL)) {
                app->connected = false;
                app->tool_active = false;
                app_custom_inject_mark_all_unprovisioned(app);
                app_append_monitor(app, "[uart] disconnected while polling");
                if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
                    app->monitor_update_pending = true;
                    furi_mutex_release(app->mutex);
                }
            } else {
                CcEvent event = {0};
                bool updated = false;
                const bool dashboard_mode_active = app_monitor_uses_dashboard(app);
                uint16_t processed = 0U;
                uint16_t dashboard_processed = 0U;

                while(processed < APP_RX_EVENT_BUDGET && cc_client_pop_event(app->client, &event)) {
                    if(app->fw_version_check_pending && event.type == CcEventTypeStatus &&
                       event.data.status.len >= 3U && event.data.status.len < 16U) {
                        const uint8_t major = event.data.status.payload[0];
                        const uint8_t minor = event.data.status.payload[1];
                        const uint8_t slen = event.data.status.payload[2];
                        if(slen < sizeof(app->fw_version_string) &&
                           (uint16_t)(3U + slen) == event.data.status.len) {
                            app->fw_version_major = major;
                            app->fw_version_minor = minor;
                            memcpy(app->fw_version_string, &event.data.status.payload[3], slen);
                            app->fw_version_string[slen] = '\0';
                            app->fw_version_received = true;
                            app->fw_version_check_pending = false;
                            processed++;
                            continue;
                        }
                    }

                    bool dashboard_consumed = false;
                    bool apply_to_dashboard = true;
                    if(dashboard_mode_active && dashboard_processed >= APP_RX_DASH_EVENT_BUDGET) {
                        apply_to_dashboard = false;
                    }

                    if(apply_to_dashboard) {
                        dashboard_consumed = dashboard_handle_event(app, &event);
                        if(dashboard_consumed) {
                            dashboard_processed++;
                        }
                    }

                    // When dashboard is active, avoid monitor text churn entirely.
                    bool append_to_monitor = !dashboard_mode_active;
                    if(!dashboard_mode_active) {
                        append_to_monitor = !dashboard_consumed;
                    }

                    if(append_to_monitor) {
                        char line[256] = {0};
                        app_format_event_line(&event, line, sizeof(line));
                        app_append_monitor(app, "%s", line);
                    }
                    updated = true;
                    processed++;

                    // Cooperative yield under high traffic so UI input stays responsive.
                    if((processed % APP_RX_EVENT_YIELD_EVERY) == 0U) {
                        furi_thread_yield();
                    }
                }

                if(updated && app->monitor_scene_active) {
                    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
                        app->monitor_update_pending = true;
                        furi_mutex_release(app->mutex);
                    }
                }

                // More queued traffic likely remains; yield before next polling cycle.
                if(processed >= APP_RX_EVENT_BUDGET) {
                    furi_thread_yield();
                }
            }
        }

        furi_delay_ms(5);
    }

    return 0;
}

void app_set_status(App* app, const char* fmt, ...) {
    va_list ap;
    char text[256] = {0};

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        furi_string_set(app->status_text, text);
        furi_mutex_release(app->mutex);
    }
}

void app_append_monitor(App* app, const char* fmt, ...) {
    va_list ap;
    char line[256] = {0};

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    if(furi_mutex_acquire(app->mutex, FuriWaitForever) != FuriStatusOk) {
        return;
    }

    if(furi_string_size(app->monitor_text) > APP_MONITOR_KEEP_MAX) {
        furi_string_right(app->monitor_text, furi_string_size(app->monitor_text) - APP_MONITOR_KEEP_TAIL);
    }

    if(furi_string_size(app->monitor_text) > 0U) {
        furi_string_cat(app->monitor_text, "\n");
    }

    furi_string_cat(app->monitor_text, line);

    furi_mutex_release(app->mutex);
}

void app_refresh_monitor_view(App* app) {
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) != FuriStatusOk) {
        return;
    }

    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->monitor_text));

    furi_mutex_release(app->mutex);
}

bool app_monitor_uses_dashboard(App* app) {
    if(!app) {
        return false;
    }

    bool use_dashboard = false;
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        use_dashboard = (app->dashboard_mode != AppDashboardNone);
        furi_mutex_release(app->mutex);
    }

    return use_dashboard;
}

void app_refresh_live_view(App* app) {
    if(app_monitor_uses_dashboard(app)) {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDashboard);
    } else {
        app_refresh_monitor_view(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextBox);
    }
}

bool app_args_set_key_value(char* args, size_t args_size, const char* key, const char* value) {
    if(!args || args_size == 0U || !key || !value) {
        return false;
    }

    const size_t key_len = strlen(key);
    if(key_len == 0U || args_size > APP_ARGS_KV_WORK_MAX) {
        return false;
    }

    char scratch[APP_ARGS_KV_WORK_MAX] = {0};
    char rebuilt[APP_ARGS_KV_WORK_MAX] = {0};
    strncpy(scratch, args, args_size - 1U);

    size_t used = 0;
    bool ok = false;

    char* save_ptr = NULL;
    char* token = strtok_r(scratch, " ", &save_ptr);

    while(token) {
        char* eq = strchr(token, '=');
        if(eq) {
            *eq = '\0';
            const char* token_key = token;
            const char* token_value = eq + 1;
            if(strcmp(token_key, key) != 0) {
                const int wrote = snprintf(
                    rebuilt + used,
                    args_size - used,
                    "%s%s=%s",
                    used ? " " : "",
                    token_key,
                    token_value);
                if(wrote <= 0) {
                    goto done;
                }
                const size_t step = (size_t)wrote;
                if(step >= (args_size - used)) {
                    goto done;
                }
                used += step;
            }
        } else {
            const int wrote =
                snprintf(rebuilt + used, args_size - used, "%s%s", used ? " " : "", token);
            if(wrote <= 0) {
                goto done;
            }
            const size_t step = (size_t)wrote;
            if(step >= (args_size - used)) {
                goto done;
            }
            used += step;
        }

        token = strtok_r(NULL, " ", &save_ptr);
    }

    const int wrote = snprintf(
        rebuilt + used, args_size - used, "%s%s=%s", used ? " " : "", key, value);
    if(wrote <= 0) {
        goto done;
    }
    const size_t step = (size_t)wrote;
    if(step >= (args_size - used)) {
        goto done;
    }

    memcpy(args, rebuilt, args_size);
    args[args_size - 1U] = '\0';
    ok = true;

done:
    return ok;
}

static bool app_firmware_version_ok(const App* app) {
    if(!app || !app->fw_version_received) {
        return false;
    }
    if(app->fw_version_major > APP_FW_MIN_MAJOR) {
        return true;
    }
    if(app->fw_version_major == APP_FW_MIN_MAJOR &&
       app->fw_version_minor >= APP_FW_MIN_MINOR) {
        return true;
    }
    return false;
}

static void app_set_fw_version_warning(App* app) {
    char fw_text[24] = {0};
    if(app->fw_version_received) {
        snprintf(fw_text, sizeof(fw_text), "v%s", app->fw_version_string);
    } else {
        snprintf(fw_text, sizeof(fw_text), "unknown");
    }
    app_set_status(
        app,
        "Firmware out of date!\n"
        "Firmware is: %s\n"
        "Firmware needs: %s+\n"
        "App: %s\n"
        "Update at:\n%s",
        fw_text,
        APP_FW_MIN_STRING,
        PROGRAM_VERSION,
        APP_FW_UPDATE_URL);
}

static void app_check_firmware_version(App* app) {
    app->fw_version_received = false;
    app->fw_version_major = 0U;
    app->fw_version_minor = 0U;
    app->fw_version_string[0] = '\0';
    app->fw_version_check_pending = true;

    CcStatusCode info_status = CcStatusUnknown;
    if(!cc_client_get_info(app->client, &info_status)) {
        app->fw_version_check_pending = false;
        app_append_monitor(app, "[uart] get_info transport failed");
        return;
    }

    if(info_status != CcStatusOk) {
        app->fw_version_check_pending = false;
        app_append_monitor(
            app, "[uart] get_info => %s", cc_status_to_string(info_status));
        return;
    }

    const uint32_t start_tick = furi_get_tick();
    const uint32_t timeout_ticks = furi_ms_to_ticks(1500U);
    while((furi_get_tick() - start_tick) < timeout_ticks) {
        if(app->fw_version_received) {
            break;
        }
        furi_delay_ms(10);
    }
    app->fw_version_check_pending = false;

    if(app->fw_version_received) {
        app_append_monitor(
            app,
            "[uart] firmware v%s (%u.%u)",
            app->fw_version_string,
            (unsigned)app->fw_version_major,
            (unsigned)app->fw_version_minor);
    } else {
        app_append_monitor(app, "[uart] firmware version response timed out");
    }
}

bool app_connect(App* app, bool force_reconnect) {
    if(!app || !app->client) {
        return false;
    }

    if(force_reconnect) {
        cc_client_close(app->client);
        app->connected = false;
        app->tool_active = false;
        app->fw_version_warn_shown = false;
        app->fw_version_checked = false;
    }

    if(!cc_client_is_open(app->client) && !cc_client_open(app->client)) {
        app->connected = false;
        app->tool_active = false;
        app_append_monitor(app, "[uart] open failed");
        return false;
    }

    CcStatusCode status = CcStatusUnknown;
    bool ping_ok = false;
    for(uint8_t attempt = 0U; attempt < 3U; attempt++) {
        status = CcStatusUnknown;
        if(cc_client_ping(app->client, &status) && status == CcStatusOk) {
            ping_ok = true;
            if(attempt > 0U) {
                app_append_monitor(app, "[uart] ping ok on attempt %u", (unsigned)(attempt + 1U));
            }
            break;
        }
        app_append_monitor(
            app,
            "[uart] ping attempt %u failed (%s)",
            (unsigned)(attempt + 1U),
            cc_status_to_string(status));
        if(attempt < 2U) {
            furi_delay_ms(50);
        }
    }

    if(!ping_ok) {
        app->connected = false;
        app->tool_active = false;
        return false;
    }

    app->connected = true;
    app_append_monitor(app, "[uart] ping => %s", cc_status_to_string(status));

    return app->connected;
}

void app_verify_firmware_version(App* app) {
    if(!app || !app->connected) {
        return;
    }
    app_check_firmware_version(app);
    app->fw_version_checked = true;
    app->fw_version_warn_pending = !app_firmware_version_ok(app);
    if(app->fw_version_warn_pending) {
        app_set_fw_version_warning(app);
    }
}

bool app_require_connected(App* app) {
    if(!app->connected) {
        if(!app_connect(app, false)) {
            app_set_status(app, "Not connected to \nCAN Commander");
            return false;
        }
    }
    return true;
}

void app_begin_edit(App* app, char* destination, size_t destination_size, const char* header_text) {
    app->input_dest = destination;
    app->input_dest_size = destination_size;
    app->input_header = header_text;
    app->input_editing_arg_value = false;
    app->input_use_byte_input = false;
    app->input_hex_mode = AppHexInputNone;
    app->input_hex_count = 0;
    memset(app->input_hex_store, 0, sizeof(app->input_hex_store));

    memset(app->input_work, 0, sizeof(app->input_work));
    if(destination && destination_size > 0U) {
        strncpy(app->input_work, destination, sizeof(app->input_work) - 1U);
    }
}

void app_begin_args_editor(
    App* app,
    char* destination,
    size_t destination_size,
    const char* header_text) {
    app->args_editor_target = destination;
    app->args_editor_target_size = destination_size;
    app->args_editor_title = header_text;
    app->args_editor_selected_index = 0;
    app->args_editor_apply_enabled = false;
    app->args_editor_apply_label = NULL;
    app->args_editor_apply_callback = NULL;
    app->args_editor_apply_next_scene = cancommander_scene_status;
}

void app_begin_args_editor_apply(
    App* app,
    char* destination,
    size_t destination_size,
    const char* header_text,
    const char* apply_label,
    AppArgsApplyCallback apply_callback,
    CanCommanderScene next_scene) {
    app_begin_args_editor(app, destination, destination_size, header_text);
    app->args_editor_apply_enabled = (apply_callback != NULL);
    app->args_editor_apply_label = apply_label;
    app->args_editor_apply_callback = apply_callback;
    app->args_editor_apply_next_scene = next_scene;
}

void app_apply_edit(App* app) {
    if(app->input_editing_arg_value) {
        if(app->input_arg_value_index < app->args_editor_count) {
            if(
                app->input_header &&
                (strcmp(app->input_header, "slot_name") == 0 || strcmp(app->input_header, "ssid") == 0 ||
                 strcmp(app->input_header, "pass") == 0 ||
                 strcmp(app->input_header, "password") == 0)) {
                for(size_t i = 0; app->input_work[i] != '\0'; i++) {
                    if(app->input_work[i] == ' ') {
                        app->input_work[i] = '_';
                    } else if(app->input_work[i] == '=') {
                        app->input_work[i] = '_';
                    }
                }
            }

            strncpy(
                app->args_editor_items[app->input_arg_value_index].value,
                app->input_work,
                sizeof(app->args_editor_items[app->input_arg_value_index].value) - 1U);
            app->args_editor_items[app->input_arg_value_index]
                .value[sizeof(app->args_editor_items[app->input_arg_value_index].value) - 1U] = '\0';

            if(app->args_editor_target && app->args_editor_target_size > 0U) {
                app->args_editor_target[0] = '\0';
                size_t used = 0;

                for(uint8_t i = 0; i < app->args_editor_count; i++) {
                    if(app->args_editor_items[i].type == AppArgValueAction) {
                        continue;
                    }

                    const int wrote = snprintf(
                        app->args_editor_target + used,
                        app->args_editor_target_size - used,
                        "%s%s=%s",
                        used ? " " : "",
                        app->args_editor_items[i].key,
                        app->args_editor_items[i].value);
                    if(wrote <= 0) {
                        break;
                    }

                    const size_t step = (size_t)wrote;
                    if(step >= (app->args_editor_target_size - used)) {
                        used = app->args_editor_target_size - 1U;
                        break;
                    }
                    used += step;
                }
                app->args_editor_target[used] = '\0';
            }

            app_append_monitor(app, "[edit] %s=%s", app->input_header, app->input_work);
        }
        app->input_editing_arg_value = false;
        app->input_use_byte_input = false;
        app->input_hex_mode = AppHexInputNone;
        app->input_hex_count = 0;
        return;
    }

    if(!app->input_dest || app->input_dest_size == 0U) {
        return;
    }

    strncpy(app->input_dest, app->input_work, app->input_dest_size - 1U);
    app->input_dest[app->input_dest_size - 1U] = '\0';
    app->input_use_byte_input = false;
    app->input_hex_mode = AppHexInputNone;
    app->input_hex_count = 0;

    app_append_monitor(app, "[edit] %s updated", app->input_header ? app->input_header : "args");
}

void app_action_ping(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    const uint32_t start = furi_get_tick();
    if(!cc_client_ping(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "PING transport error");
        app_append_monitor(app, "[cmd] ping transport error");
        return;
    }
    const uint32_t elapsed = (uint32_t)(furi_get_tick() - start);

    app_set_status(app, "PING => %s (%lums)", cc_status_to_string(status), (unsigned long)elapsed);
    app_append_monitor(
        app, "[cmd] ping => %s (%lums)", cc_status_to_string(status), (unsigned long)elapsed);
}

void app_action_get_info(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_get_info(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "GET_INFO transport error");
        app_append_monitor(app, "[cmd] get_info transport error");
        return;
    }

    app_set_status(app, "GET_INFO => %s", cc_status_to_string(status));
    app_append_monitor(
        app,
        "[cmd] get_info => %s (protocol returns status-only; requesting bus cfg snapshots)",
        cc_status_to_string(status));

    if(status == CcStatusOk) {
        CcStatusCode cfg_status = CcStatusUnknown;
        cc_client_bus_get_cfg(app->client, CcBusCan0, &cfg_status);
        cc_client_bus_get_cfg(app->client, CcBusCan1, &cfg_status);
    }
}

void app_action_stats(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_stats_get(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "STATS_GET transport error");
        app_append_monitor(app, "[cmd] stats transport error");
        return;
    }

    app_set_status(app, "STATS_GET => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] stats => %s", cc_status_to_string(status));
}

void app_action_wifi_get_cfg(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_wifi_get_cfg(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "WIFI_GET_CFG transport error");
        app_append_monitor(app, "[cmd] wifi get cfg transport error");
        return;
    }

    app_set_status(app, "WIFI_GET_CFG => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] wifi get cfg => %s", cc_status_to_string(status));
}

void app_action_wifi_set_cfg(App* app, const char* args) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_wifi_set_cfg(app->client, args, &status)) {
        app->connected = false;
        app_set_status(app, "WIFI_SET_CFG transport error");
        app_append_monitor(app, "[cmd] wifi set cfg transport error");
        return;
    }

    app_set_status(app, "WIFI_SET_CFG => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] wifi set cfg args='%s' => %s", args, cc_status_to_string(status));
}

void app_action_start_read_all(App* app) {
    app_action_tool_start(app, CcToolReadAll, app->args_read_all, "read_all");
}

void app_action_tool_start(App* app, CcToolId tool_id, const char* args, const char* label) {
    if(!app_require_connected(app)) {
        return;
    }

    char tool_start_args[220] = {0};
    if(tool_id == CcToolWrite) {
        char bus_value[16] = {0};
        char data_value_raw[64] = {0};
        char data_value[17] = {0};
        bool ext = false;
        bool rtr = false;
        uint32_t id = 0x1D5U;
        uint32_t dlc = 8U;
        uint32_t count = 1U;
        uint32_t interval_ms = 250U;

        if(!arg_get_value_last(args, "bus", bus_value, sizeof(bus_value)) || bus_value[0] == '\0') {
            (void)arg_get_value_last(app->args_write_tool, "bus", bus_value, sizeof(bus_value));
        }
        if(
            strcmp(bus_value, "can0") != 0 && strcmp(bus_value, "can1") != 0 &&
            strcmp(bus_value, "both") != 0) {
            strncpy(bus_value, "can0", sizeof(bus_value) - 1U);
        }

        if(!parse_u32_key_last(args, "id", 16, &id)) {
            (void)parse_u32_key_last(app->args_write_tool, "id", 16, &id);
        }

        if(!parse_bool_key_last(args, "ext", &ext)) {
            (void)parse_bool_key_last(app->args_write_tool, "ext", &ext);
        }

        if(!parse_bool_key_last(args, "rtr", &rtr)) {
            (void)parse_bool_key_last(app->args_write_tool, "rtr", &rtr);
        }

        if(!parse_u32_key_last(args, "dlc", 10, &dlc)) {
            (void)parse_u32_key_last(app->args_write_tool, "dlc", 10, &dlc);
        }
        if(dlc > 8U) {
            dlc = 8U;
        }

        if(!arg_get_value_last(args, "data", data_value_raw, sizeof(data_value_raw)) ||
           data_value_raw[0] == '\0') {
            (void)arg_get_value_last(app->args_write_tool, "data", data_value_raw, sizeof(data_value_raw));
        }

        size_t used = 0U;
        for(size_t i = 0U; data_value_raw[i] != '\0' && used < 16U; i++) {
            const char c = data_value_raw[i];
            if(isxdigit((unsigned char)c)) {
                data_value[used++] = (char)toupper((unsigned char)c);
            }
        }
        if((used % 2U) != 0U && used > 0U) {
            used--;
        }
        data_value[used] = '\0';

        if(used == 0U) {
            if(dlc == 0U) {
                dlc = 1U;
            }
            const size_t wanted = (dlc > 8U) ? 16U : (size_t)(dlc * 2U);
            memset(data_value, '0', wanted);
            data_value[wanted] = '\0';
            used = wanted;
        }

        const uint32_t data_bytes = (uint32_t)(used / 2U);
        if(data_bytes > dlc) {
            dlc = data_bytes;
        } else if(data_bytes < dlc) {
            const size_t wanted = (size_t)(dlc * 2U);
            while(used < wanted && used < 16U) {
                data_value[used++] = '0';
            }
            data_value[used] = '\0';
        }

        if(!parse_u32_key_last(args, "count", 10, &count)) {
            (void)parse_u32_key_last(app->args_write_tool, "count", 10, &count);
        }
        if(count == 0U) {
            count = 1U;
        }

        if(!parse_u32_key_last(args, "interval_ms", 10, &interval_ms)) {
            if(!parse_u32_key_last(args, "period_ms", 10, &interval_ms)) {
                if(!parse_u32_key_last(app->args_write_tool, "interval_ms", 10, &interval_ms)) {
                    if(!parse_u32_key_last(app->args_write_tool, "period_ms", 10, &interval_ms)) {
                        interval_ms = 250U;
                    }
                }
            }
        }

        char id_text[12] = {0};
        if(ext) {
            snprintf(id_text, sizeof(id_text), "%08lX", (unsigned long)id);
        } else {
            snprintf(id_text, sizeof(id_text), "%03lX", (unsigned long)(id & 0x7FFU));
        }

        snprintf(
            app->args_write_tool,
            sizeof(app->args_write_tool),
            "bus=%s id=%s ext=%u rtr=%u dlc=%lu data=%s count=%lu interval_ms=%lu",
            bus_value,
            id_text,
            ext ? 1U : 0U,
            rtr ? 1U : 0U,
            (unsigned long)dlc,
            data_value,
            (unsigned long)count,
            (unsigned long)interval_ms);

        snprintf(
            tool_start_args,
            sizeof(tool_start_args),
            "bus=%s id=%s ext=%u rtr=%u dlc=%lu data=%s count=%lu period_ms=%lu",
            bus_value,
            id_text,
            ext ? 1U : 0U,
            rtr ? 1U : 0U,
            (unsigned long)dlc,
            data_value,
            (unsigned long)count,
            (unsigned long)interval_ms);

        args = tool_start_args;
        app_append_monitor(app, "[cmd] write canonical args='%s'", app->args_write_tool);
    }

    if(tool_id == CcToolReplay) {
        char bus_value[16] = {0};
        char id_value[12] = {0};
        char ext_value[4] = {0};

        if(!arg_get_value_last(args, "bus", bus_value, sizeof(bus_value)) || bus_value[0] == '\0') {
            strncpy(bus_value, "can0", sizeof(bus_value) - 1U);
        }
        if(
            strcmp(bus_value, "can0") != 0 && strcmp(bus_value, "can1") != 0 &&
            strcmp(bus_value, "both") != 0) {
            strncpy(bus_value, "can0", sizeof(bus_value) - 1U);
        }

        if(!arg_get_value_last(args, "id", id_value, sizeof(id_value)) || id_value[0] == '\0') {
            strncpy(id_value, "2C1", sizeof(id_value) - 1U);
        }

        if(!arg_get_value_last(args, "ext", ext_value, sizeof(ext_value)) || ext_value[0] == '\0') {
            strncpy(ext_value, "0", sizeof(ext_value) - 1U);
        }

        snprintf(
            tool_start_args,
            sizeof(tool_start_args),
            "bus=%s id=%s ext=%s",
            bus_value,
            id_value,
            ext_value);
        args = tool_start_args;
    }

    if(tool_id == CcToolObdPid) {
        char pid_token[32] = {0};
        char canonical_pid[24] = {0};
        bool is_mode01_pid = true;

        if(arg_get_value_last(args, "pid", pid_token, sizeof(pid_token)) && pid_token[0] != '\0') {
            (void)app_normalize_obd_pid_token(
                pid_token, canonical_pid, sizeof(canonical_pid), &is_mode01_pid);
        }

        if(
            canonical_pid[0] == '\0' &&
            arg_get_value_last(app->args_obd_pid, "pid", pid_token, sizeof(pid_token)) &&
            pid_token[0] != '\0') {
            (void)app_normalize_obd_pid_token(
                pid_token, canonical_pid, sizeof(canonical_pid), &is_mode01_pid);
        }

        if(canonical_pid[0] == '\0') {
            strncpy(canonical_pid, "0C", sizeof(canonical_pid) - 1U);
            is_mode01_pid = true;
        }

        char bus_value[16] = {0};
        if(!arg_get_value_last(args, "bus", bus_value, sizeof(bus_value)) || bus_value[0] == '\0') {
            if(!arg_get_value_last(app->args_obd_pid, "bus", bus_value, sizeof(bus_value)) ||
               bus_value[0] == '\0') {
                strncpy(bus_value, "can0", sizeof(bus_value) - 1U);
            }
        }
        if(
            strcmp(bus_value, "can0") != 0 && strcmp(bus_value, "can1") != 0 &&
            strcmp(bus_value, "both") != 0) {
            strncpy(bus_value, "can0", sizeof(bus_value) - 1U);
        }

        if(is_mode01_pid) {
            uint32_t interval_ms = 250;
            if(!parse_u32_key_last(args, "interval_ms", 10, &interval_ms)) {
                parse_u32_key_last(app->args_obd_pid, "interval_ms", 10, &interval_ms);
            }
            if(interval_ms < 10U) {
                interval_ms = 10U;
            }

            snprintf(
                app->args_obd_pid,
                sizeof(app->args_obd_pid),
                "bus=%s pid=%s interval_ms=%lu",
                bus_value,
                canonical_pid,
                (unsigned long)interval_ms);
        } else {
            snprintf(
                app->args_obd_pid,
                sizeof(app->args_obd_pid),
                "bus=%s pid=%s",
                bus_value,
                canonical_pid);
        }
        args = app->args_obd_pid;
        app_append_monitor(
            app,
            "[cmd] obd canonical args='%s' (pid=%s%s)",
            args,
            is_mode01_pid ? "0x" : "",
            canonical_pid);

        CcBus bus = CcBusCan0;
        if(!parse_bus_key(args, "bus", &bus)) {
            bus = CcBusCan0;
        }

        CcStatusCode filter_status = CcStatusUnknown;
        if(bus == CcBusCan0 || bus == CcBusBoth) {
            if(cc_client_bus_clear_filter(app->client, CcBusCan0, &filter_status)) {
                app_append_monitor(
                    app,
                    "[cmd] obd preflight clear filter can0 => %s",
                    cc_status_to_string(filter_status));
            } else {
                app_append_monitor(app, "[cmd] obd preflight clear filter can0 transport error");
            }
        }

        if(bus == CcBusCan1 || bus == CcBusBoth) {
            if(cc_client_bus_clear_filter(app->client, CcBusCan1, &filter_status)) {
                app_append_monitor(
                    app,
                    "[cmd] obd preflight clear filter can1 => %s",
                    cc_status_to_string(filter_status));
            } else {
                app_append_monitor(app, "[cmd] obd preflight clear filter can1 transport error");
            }
        }
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_tool_start(app->client, tool_id, args, &status)) {
        app->connected = false;
        app->tool_active = false;
        app_set_status(app, "TOOL_START(%s) transport error", label);
        app_append_monitor(app, "[cmd] tool start %s transport error", label);
        return;
    }

    app_set_status(app, "TOOL_START %s => %s", label, cc_status_to_string(status));
    app_append_monitor(
        app, "[cmd] tool start %s args='%s' => %s", label, args, cc_status_to_string(status));

    app->tool_active = (status == CcStatusOk);

    if(status == CcStatusOk && tool_id == CcToolCustomInject) {
        app_custom_inject_mark_all_unprovisioned(app);
    }

    if(status == CcStatusOk) {
        dashboard_set_mode(app, dashboard_mode_for_tool(tool_id));
    } else {
        dashboard_set_mode(app, AppDashboardNone);
    }
}

static bool app_action_tool_config_exec(App* app, const char* args, CcStatusCode* out_status) {
    if(out_status) {
        *out_status = CcStatusUnknown;
    }

    if(!app_require_connected(app)) {
        return false;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_tool_config(app->client, args, &status)) {
        app->connected = false;
        app_set_status(app, "TOOL_CONFIG transport error");
        app_append_monitor(app, "[cmd] tool config transport error");
        return false;
    }

    app_set_status(app, "TOOL_CONFIG => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] tool config args='%s' => %s", args, cc_status_to_string(status));

    if(out_status) {
        *out_status = status;
    }

    return true;
}

void app_action_tool_config(App* app, const char* args) {
    (void)app_action_tool_config_exec(app, args, NULL);
}

void app_action_tool_stop(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_tool_stop(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "TOOL_STOP transport error");
        app_append_monitor(app, "[cmd] tool stop transport error");
        return;
    }

    app_set_status(app, "TOOL_STOP => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] tool stop => %s", cc_status_to_string(status));

    if(status == CcStatusOk || status == CcStatusNotActive) {
        app->tool_active = false;
    }

    if(status == CcStatusOk || status == CcStatusNotActive) {
        app_custom_inject_mark_all_unprovisioned(app);
        dashboard_set_mode(app, AppDashboardNone);
    }
}

void app_action_tool_status(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_tool_status(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "TOOL_STATUS transport error");
        app_append_monitor(app, "[cmd] tool status transport error");
        return;
    }

    app_set_status(app, "TOOL_STATUS => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] tool status => %s", cc_status_to_string(status));
}

static bool app_custom_inject_slot_is_valid(uint8_t slot_number) {
    return slot_number >= 1U && slot_number <= 5U;
}

static bool app_custom_inject_profile_marked_used(const char* profile) {
    if(!profile) {
        return false;
    }

    char used[8] = {0};
    if(arg_get_value_last(profile, "used", used, sizeof(used))) {
        return (strcmp(used, "1") == 0 || strcmp(used, "true") == 0);
    }

    uint32_t id = 0U;
    return parse_u32_key_last(profile, "id", 16, &id) && (id != 0U);
}

static void app_custom_inject_value_or_default(
    const char* profile,
    const char* key,
    const char* fallback,
    char* out,
    size_t out_size) {
    if(!arg_get_value_last(profile, key, out, out_size) || out[0] == '\0') {
        app_copy_string(out, out_size, fallback);
    }
}

static bool app_custom_inject_parse_u32_from_profile(
    const char* profile,
    const char* key,
    uint32_t base,
    uint32_t* out) {
    if(!out) {
        return false;
    }

    return parse_u32_key_last(profile, key, base, out);
}

static bool app_custom_inject_parse_u64_from_profile(
    const char* profile,
    const char* key,
    uint64_t* out) {
    if(!profile || !key || !out) {
        return false;
    }

    char raw[48] = {0};
    if(!arg_get_value_last(profile, key, raw, sizeof(raw)) || raw[0] == '\0') {
        return false;
    }

    char* end = NULL;
    unsigned long long parsed = strtoull(raw, &end, 0);
    if(end == raw || *end != '\0') {
        return false;
    }

    *out = (uint64_t)parsed;
    return true;
}

static bool app_custom_inject_profile_bool(
    const char* profile,
    const char* key,
    bool default_value,
    bool* out) {
    if(!out) {
        return false;
    }

    char raw[16] = {0};
    if(!profile || !arg_get_value_last(profile, key, raw, sizeof(raw)) || raw[0] == '\0') {
        *out = default_value;
        return true;
    }

    if(strcmp(raw, "1") == 0 || strcmp(raw, "true") == 0) {
        *out = true;
        return true;
    }

    if(strcmp(raw, "0") == 0 || strcmp(raw, "false") == 0) {
        *out = false;
        return true;
    }

    return false;
}

static bool app_custom_inject_field_range_valid(uint32_t start, uint32_t len) {
    if(len == 0U || len > 64U || start > 63U) {
        return false;
    }

    return (start + len) <= 64U;
}

static bool app_custom_inject_field_value_valid(uint32_t len, uint64_t value) {
    if(len >= 64U) {
        return true;
    }

    const uint64_t max_value = (1ULL << len) - 1ULL;
    return value <= max_value;
}

static bool app_custom_inject_parse_field_spec_from_profile(
    App* app,
    const char* profile,
    const char* start_key,
    const char* len_key,
    const char* value_key,
    uint32_t* out_start,
    uint32_t* out_len,
    uint64_t* out_value) {
    if(!out_start || !out_len || !out_value) {
        return false;
    }

    uint32_t start = 0U;
    uint32_t len = 0U;
    uint64_t value = 0ULL;

    if(!app_custom_inject_parse_u32_from_profile(profile, start_key, 10, &start) ||
       !app_custom_inject_parse_u32_from_profile(profile, len_key, 10, &len) ||
       !app_custom_inject_parse_u64_from_profile(profile, value_key, &value)) {
        return false;
    }

    if(!app_custom_inject_field_range_valid(start, len)) {
        app_set_status(app, "Custom Inject %s/%s out of range", start_key, len_key);
        return false;
    }

    if(!app_custom_inject_field_value_valid(len, value)) {
        app_set_status(app, "Custom Inject %s value too large", value_key);
        return false;
    }

    *out_start = start;
    *out_len = len;
    *out_value = value;
    return true;
}

static bool app_custom_inject_normalize_hex_token(
    const char* profile,
    const char* key,
    char* out,
    size_t out_size) {
    if(!profile || !key || !out || out_size == 0U) {
        return false;
    }

    char raw[48] = {0};
    if(!arg_get_value_last(profile, key, raw, sizeof(raw)) || raw[0] == '\0') {
        return false;
    }

    size_t src = 0U;
    if(raw[0] == '0' && (raw[1] == 'x' || raw[1] == 'X')) {
        src = 2U;
    }

    const size_t len = strlen(raw + src);
    if(len == 0U || len > 16U || (len % 2U) != 0U || len >= out_size) {
        return false;
    }

    for(size_t i = 0U; i < len; i++) {
        const char c = raw[src + i];
        if(!isxdigit((unsigned char)c)) {
            return false;
        }
        out[i] = (char)toupper((unsigned char)c);
    }
    out[len] = '\0';
    return true;
}

static void app_custom_inject_sanitize_name_token(
    const char* in,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(!in || in[0] == '\0') {
        return;
    }

    size_t w = 0U;
    for(size_t i = 0U; in[i] != '\0' && w + 1U < out_size && w < 23U; i++) {
        const unsigned char ch = (unsigned char)in[i];
        if(ch == ' ' || ch == '\t') {
            out[w++] = '_';
            continue;
        }

        // Keep token parser-safe payload: avoid spaces and '=' separators.
        if(ch == '=') {
            continue;
        }

        out[w++] = (char)ch;
    }
    out[w] = '\0';
}

static const char* app_custom_inject_profile_for_slot(const App* app, uint8_t slot_number) {
    if(!app_custom_inject_slot_is_valid(slot_number)) {
        return NULL;
    }

    return app_custom_inject_slot_cptr(app, (uint8_t)(slot_number - 1U));
}

static bool app_custom_inject_validate_profile_id(App* app, uint8_t slot_number, uint32_t* out_id) {
    const char* profile = app_custom_inject_profile_for_slot(app, slot_number);
    if(!profile) {
        app_set_status(app, "Custom Inject invalid slot %u", slot_number);
        return false;
    }

    uint32_t id = 0;
    if(!app_custom_inject_parse_u32_from_profile(profile, "id", 16, &id)) {
        app_set_status(app, "Custom Inject slot %u invalid id", slot_number);
        return false;
    }

    if(out_id) {
        *out_id = id;
    }
    return true;
}

static bool app_custom_inject_build_add_args(
    App* app,
    uint8_t slot_number,
    char* out,
    size_t out_size) {
    const char* profile = app_custom_inject_profile_for_slot(app, slot_number);
    if(!profile || !out || out_size == 0U) {
        return false;
    }

    uint32_t id = 0;
    if(!app_custom_inject_validate_profile_id(app, slot_number, &id)) {
        return false;
    }

    char bus[16] = {0};
    char ext_text[8] = {0};

    app_custom_inject_value_or_default(profile, "bus", "can0", bus, sizeof(bus));
    if(strcmp(bus, "can0") != 0 && strcmp(bus, "can1") != 0 && strcmp(bus, "both") != 0) {
        app_copy_string(bus, sizeof(bus), "can0");
    }
    app_custom_inject_value_or_default(profile, "ext", "0", ext_text, sizeof(ext_text));
    if(strcmp(ext_text, "1") != 0) {
        app_copy_string(ext_text, sizeof(ext_text), "0");
    }

    const int wrote = snprintf(
        out,
        out_size,
        "cmd=add slot=%u bus=%s id=%lX ext=%s",
        (unsigned)slot_number,
        bus,
        (unsigned long)id,
        ext_text);

    if(wrote <= 0 || (size_t)wrote >= out_size) {
        app_set_status(app, "Custom Inject add args too long");
        return false;
    }

    size_t used = (size_t)wrote;
    char slot_name_raw[40] = {0};
    if(arg_get_value_last(profile, "slot_name", slot_name_raw, sizeof(slot_name_raw))) {
        char slot_name[32] = {0};
        app_custom_inject_sanitize_name_token(slot_name_raw, slot_name, sizeof(slot_name));
        if(slot_name[0] != '\0') {
            const int name_wrote = snprintf(out + used, out_size - used, " name=%s", slot_name);
            if(name_wrote <= 0 || (size_t)name_wrote >= (out_size - used)) {
                app_set_status(app, "Custom Inject add args too long");
                return false;
            }
            used += (size_t)name_wrote;
        }
    }

    bool mux_enabled = false;
    if(!app_custom_inject_profile_bool(profile, "mux", false, &mux_enabled)) {
        app_set_status(app, "Custom Inject slot %u invalid mux", slot_number);
        return false;
    }

    if(mux_enabled) {
        uint32_t mux_start = 0U;
        uint32_t mux_len = 0U;
        uint64_t mux_value = 0ULL;
        if(!app_custom_inject_parse_field_spec_from_profile(
               app,
               profile,
               "mux_start",
               "mux_len",
               "mux_value",
               &mux_start,
               &mux_len,
               &mux_value)) {
            app_set_status(app, "Custom Inject slot %u invalid mux args", slot_number);
            return false;
        }

        const int mux_wrote = snprintf(
            out + used,
            out_size - used,
            " mux_start=%lu mux_len=%lu mux_value=%llu",
            (unsigned long)mux_start,
            (unsigned long)mux_len,
            (unsigned long long)mux_value);
        if(mux_wrote <= 0 || (size_t)mux_wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject add args too long");
            return false;
        }
        used += (size_t)mux_wrote;
    }

    bool sig_enabled = false;
    if(!app_custom_inject_profile_bool(profile, "sig", false, &sig_enabled)) {
        app_set_status(app, "Custom Inject slot %u invalid sig", slot_number);
        return false;
    }

    if(sig_enabled) {
        uint32_t sig_start = 0U;
        uint32_t sig_len = 0U;
        uint64_t sig_value = 0ULL;
        if(!app_custom_inject_parse_field_spec_from_profile(
               app,
               profile,
               "sig_start",
               "sig_len",
               "sig_value",
               &sig_start,
               &sig_len,
               &sig_value)) {
            app_set_status(app, "Custom Inject slot %u invalid field args", slot_number);
            return false;
        }

        const int sig_wrote = snprintf(
            out + used,
            out_size - used,
            " sig_start=%lu sig_len=%lu sig_value=%llu",
            (unsigned long)sig_start,
            (unsigned long)sig_len,
            (unsigned long long)sig_value);
        if(sig_wrote <= 0 || (size_t)sig_wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject add args too long");
            return false;
        }
    }

    return true;
}

static bool app_custom_inject_build_modify_args(
    App* app,
    uint8_t slot_number,
    char* out,
    size_t out_size) {
    const char* profile = app_custom_inject_profile_for_slot(app, slot_number);
    if(!profile || !out || out_size == 0U) {
        return false;
    }

    int wrote = snprintf(out, out_size, "cmd=modify slot=%u", (unsigned)slot_number);
    if(wrote <= 0 || (size_t)wrote >= out_size) {
        app_set_status(app, "Custom Inject modify args invalid");
        return false;
    }

    size_t used = (size_t)wrote;
    bool has_any = false;

    char slot_name_raw[40] = {0};
    if(arg_get_value_last(profile, "slot_name", slot_name_raw, sizeof(slot_name_raw))) {
        char slot_name[32] = {0};
        app_custom_inject_sanitize_name_token(slot_name_raw, slot_name, sizeof(slot_name));
        if(slot_name[0] != '\0') {
            wrote = snprintf(out + used, out_size - used, " name=%s", slot_name);
            if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
                app_set_status(app, "Custom Inject modify args too long");
                return false;
            }
            used += (size_t)wrote;
            has_any = true;
        } else {
            wrote = snprintf(out + used, out_size - used, " name_clear=1");
            if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
                app_set_status(app, "Custom Inject modify args too long");
                return false;
            }
            used += (size_t)wrote;
            has_any = true;
        }
    }

    char mask[24] = {0};
    if(app_custom_inject_normalize_hex_token(profile, "mask", mask, sizeof(mask))) {
        wrote = snprintf(out + used, out_size - used, " mask=%s", mask);
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject modify args too long");
            return false;
        }
        used += (size_t)wrote;
        has_any = true;
    }

    char value[24] = {0};
    if(app_custom_inject_normalize_hex_token(profile, "value", value, sizeof(value))) {
        wrote = snprintf(out + used, out_size - used, " value=%s", value);
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject modify args too long");
            return false;
        }
        used += (size_t)wrote;
        has_any = true;
    }

    char xor_mask[24] = {0};
    if(app_custom_inject_normalize_hex_token(profile, "xor", xor_mask, sizeof(xor_mask))) {
        wrote = snprintf(out + used, out_size - used, " xor=%s", xor_mask);
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject modify args too long");
            return false;
        }
        used += (size_t)wrote;
        has_any = true;
    }

    bool mux_enabled = false;
    if(!app_custom_inject_profile_bool(profile, "mux", false, &mux_enabled)) {
        app_set_status(app, "Custom Inject slot %u invalid mux", slot_number);
        return false;
    }

    if(mux_enabled) {
        uint32_t mux_start = 0U;
        uint32_t mux_len = 0U;
        uint64_t mux_value = 0ULL;
        if(!app_custom_inject_parse_field_spec_from_profile(
               app,
               profile,
               "mux_start",
               "mux_len",
               "mux_value",
               &mux_start,
               &mux_len,
               &mux_value)) {
            app_set_status(app, "Custom Inject slot %u invalid mux args", slot_number);
            return false;
        }

        wrote = snprintf(
            out + used,
            out_size - used,
            " mux_start=%lu mux_len=%lu mux_value=%llu",
            (unsigned long)mux_start,
            (unsigned long)mux_len,
            (unsigned long long)mux_value);
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject modify args too long");
            return false;
        }
        used += (size_t)wrote;
        has_any = true;
    } else {
        wrote = snprintf(out + used, out_size - used, " mux=0");
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject modify args too long");
            return false;
        }
        used += (size_t)wrote;
        has_any = true;
    }

    bool sig_enabled = false;
    if(!app_custom_inject_profile_bool(profile, "sig", false, &sig_enabled)) {
        app_set_status(app, "Custom Inject slot %u invalid sig", slot_number);
        return false;
    }

    if(sig_enabled) {
        uint32_t sig_start = 0U;
        uint32_t sig_len = 0U;
        uint64_t sig_value = 0ULL;
        if(!app_custom_inject_parse_field_spec_from_profile(
               app,
               profile,
               "sig_start",
               "sig_len",
               "sig_value",
               &sig_start,
               &sig_len,
               &sig_value)) {
            app_set_status(app, "Custom Inject slot %u invalid field args", slot_number);
            return false;
        }

        wrote = snprintf(
            out + used,
            out_size - used,
            " sig_start=%lu sig_len=%lu sig_value=%llu",
            (unsigned long)sig_start,
            (unsigned long)sig_len,
            (unsigned long long)sig_value);
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject modify args too long");
            return false;
        }
        used += (size_t)wrote;
        has_any = true;
    }

    if(!has_any) {
        app_set_status(app, "Custom Inject modify requires data");
        return false;
    }

    return true;
}

static bool app_custom_inject_build_inject_args(
    App* app,
    uint8_t slot_number,
    char* out,
    size_t out_size) {
    const char* profile = app_custom_inject_profile_for_slot(app, slot_number);
    if(!profile || !out || out_size == 0U) {
        return false;
    }

    uint32_t count = 1U;
    uint32_t interval_ms = 0U;

    app_custom_inject_parse_u32_from_profile(profile, "count", 10, &count);
    app_custom_inject_parse_u32_from_profile(profile, "interval_ms", 10, &interval_ms);
    if(count == 0U) {
        count = 1U;
    }

    int wrote = snprintf(out, out_size, "cmd=inject slot=%u", (unsigned)slot_number);
    if(wrote <= 0 || (size_t)wrote >= out_size) {
        app_set_status(app, "Custom Inject inject args invalid");
        return false;
    }

    if(count != 1U || interval_ms != 0U) {
        const size_t used = (size_t)wrote;
        wrote = snprintf(
            out + used,
            out_size - used,
            " count=%lu interval_ms=%lu",
            (unsigned long)count,
            (unsigned long)interval_ms);
        if(wrote <= 0 || (size_t)wrote >= (out_size - used)) {
            app_set_status(app, "Custom Inject inject args too long");
            return false;
        }
    }

    return true;
}

static bool app_custom_inject_send_add(App* app, uint8_t slot_number, CcStatusCode* out_status) {
    if(out_status) {
        *out_status = CcStatusUnknown;
    }

    char args[220] = {0};
    if(!app_custom_inject_build_add_args(app, slot_number, args, sizeof(args))) {
        return false;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!app_action_tool_config_exec(app, args, &status)) {
        return false;
    }

    if(out_status) {
        *out_status = status;
    }

    if(status == CcStatusOk) {
        app_custom_inject_set_slot_provisioned(app, slot_number, true);
        return true;
    }

    return false;
}

static bool app_custom_inject_ensure_slot_ready(App* app, uint8_t slot_number) {
    if(app_custom_inject_slot_is_provisioned(app, slot_number)) {
        return true;
    }

    CcStatusCode add_status = CcStatusUnknown;
    return app_custom_inject_send_add(app, slot_number, &add_status);
}

static bool app_custom_inject_parse_hex_pair(char hi, char lo, uint8_t* out) {
    if(!out) {
        return false;
    }

    char pair[3] = {hi, lo, '\0'};
    char* end = NULL;
    unsigned long v = strtoul(pair, &end, 16);
    if(end == pair || *end != '\0' || v > 0xFFUL) {
        return false;
    }

    *out = (uint8_t)v;
    return true;
}

static void app_custom_inject_load_profile_bytes(
    const char* profile,
    const char* key,
    uint8_t out_bytes[8]) {
    if(!profile || !key || !out_bytes) {
        return;
    }

    memset(out_bytes, 0, 8U);

    char normalized[24] = {0};
    if(!app_custom_inject_normalize_hex_token(profile, key, normalized, sizeof(normalized))) {
        return;
    }

    const size_t len = strlen(normalized);
    const size_t bytes = len / 2U;
    for(size_t i = 0U; i < bytes && i < 8U; i++) {
        uint8_t b = 0U;
        if(!app_custom_inject_parse_hex_pair(normalized[i * 2U], normalized[i * 2U + 1U], &b)) {
            return;
        }
        out_bytes[i] = b;
    }
}

static void app_custom_inject_bytes_to_hex(const uint8_t bytes[8], char out_hex[17]) {
    static const char kHex[] = "0123456789ABCDEF";
    if(!bytes || !out_hex) {
        return;
    }

    for(uint8_t i = 0U; i < 8U; i++) {
        const uint8_t b = bytes[i];
        out_hex[i * 2U] = kHex[(uint8_t)((b >> 4U) & 0x0FU)];
        out_hex[i * 2U + 1U] = kHex[(uint8_t)(b & 0x0FU)];
    }
    out_hex[16] = '\0';
}

static void app_custom_inject_update_profile_bit(
    App* app,
    uint8_t slot_number,
    uint8_t bit,
    bool value,
    bool clear_only) {
    char* profile = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    if(!profile) {
        return;
    }

    uint8_t mask[8] = {0};
    uint8_t force_value[8] = {0};
    app_custom_inject_load_profile_bytes(profile, "mask", mask);
    app_custom_inject_load_profile_bytes(profile, "value", force_value);

    const uint8_t byte_index = (uint8_t)(bit / 8U);
    const uint8_t bit_index = (uint8_t)(bit % 8U);
    const uint8_t bit_mask = (uint8_t)(1U << bit_index);

    if(clear_only) {
        mask[byte_index] &= (uint8_t)~bit_mask;
        force_value[byte_index] &= (uint8_t)~bit_mask;
    } else {
        mask[byte_index] |= bit_mask;
        if(value) {
            force_value[byte_index] |= bit_mask;
        } else {
            force_value[byte_index] &= (uint8_t)~bit_mask;
        }
    }

    char mask_hex[17] = {0};
    char value_hex[17] = {0};
    app_custom_inject_bytes_to_hex(mask, mask_hex);
    app_custom_inject_bytes_to_hex(force_value, value_hex);

    app_args_set_key_value(
        profile, sizeof(app->args_custom_inject_slots[0]), "mask", mask_hex);
    app_args_set_key_value(
        profile, sizeof(app->args_custom_inject_slots[0]), "value", value_hex);
}

static void app_custom_inject_update_profile_field(
    App* app,
    uint8_t slot_number,
    uint8_t start_bit,
    uint8_t bit_len,
    uint64_t value) {
    char* profile = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    if(!profile) {
        return;
    }

    uint8_t mask[8] = {0};
    uint8_t force_value[8] = {0};
    app_custom_inject_load_profile_bytes(profile, "mask", mask);
    app_custom_inject_load_profile_bytes(profile, "value", force_value);

    for(uint8_t i = 0U; i < bit_len; i++) {
        const uint8_t bit = (uint8_t)(start_bit + i);
        const uint8_t byte_index = (uint8_t)(bit / 8U);
        const uint8_t bit_index = (uint8_t)(bit % 8U);
        const uint8_t bit_mask = (uint8_t)(1U << bit_index);

        mask[byte_index] |= bit_mask;
        if(((value >> i) & 0x1ULL) != 0ULL) {
            force_value[byte_index] |= bit_mask;
        } else {
            force_value[byte_index] &= (uint8_t)~bit_mask;
        }
    }

    char mask_hex[17] = {0};
    char value_hex[17] = {0};
    app_custom_inject_bytes_to_hex(mask, mask_hex);
    app_custom_inject_bytes_to_hex(force_value, value_hex);

    char sig_start[8] = {0};
    char sig_len[8] = {0};
    char sig_value[24] = {0};
    snprintf(sig_start, sizeof(sig_start), "%u", (unsigned)start_bit);
    snprintf(sig_len, sizeof(sig_len), "%u", (unsigned)bit_len);
    snprintf(sig_value, sizeof(sig_value), "%llu", (unsigned long long)value);

    app_args_set_key_value(
        profile, sizeof(app->args_custom_inject_slots[0]), "mask", mask_hex);
    app_args_set_key_value(
        profile, sizeof(app->args_custom_inject_slots[0]), "value", value_hex);
    app_args_set_key_value(profile, sizeof(app->args_custom_inject_slots[0]), "sig", "1");
    app_args_set_key_value(
        profile, sizeof(app->args_custom_inject_slots[0]), "sig_start", sig_start);
    app_args_set_key_value(profile, sizeof(app->args_custom_inject_slots[0]), "sig_len", sig_len);
    app_args_set_key_value(
        profile, sizeof(app->args_custom_inject_slots[0]), "sig_value", sig_value);
}

void app_action_custom_inject_add(App* app, uint8_t slot_number) {
    if(!app_custom_inject_slot_is_valid(slot_number)) {
        app_set_status(app, "Custom Inject invalid slot %u", slot_number);
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!app_custom_inject_send_add(app, slot_number, &status) && status != CcStatusUnknown) {
        app_custom_inject_set_slot_provisioned(app, slot_number, false);
    }
}

void app_action_custom_inject_modify(App* app, uint8_t slot_number) {
    if(!app_custom_inject_slot_is_valid(slot_number)) {
        app_set_status(app, "Custom Inject invalid slot %u", slot_number);
        return;
    }

    if(!app_custom_inject_ensure_slot_ready(app, slot_number)) {
        return;
    }

    char args[220] = {0};
    if(app_custom_inject_build_modify_args(app, slot_number, args, sizeof(args))) {
        CcStatusCode status = CcStatusUnknown;
        if(!app_action_tool_config_exec(app, args, &status)) {
            return;
        }
        if(status != CcStatusOk) {
            app_custom_inject_set_slot_provisioned(app, slot_number, false);
        }
    }
}

void app_action_custom_inject_remove(App* app, uint8_t slot_number) {
    if(!app_custom_inject_slot_is_valid(slot_number)) {
        app_set_status(app, "Custom Inject invalid slot %u", slot_number);
        return;
    }

    char args[64] = {0};
    snprintf(args, sizeof(args), "cmd=remove slot=%u", (unsigned)slot_number);
    CcStatusCode status = CcStatusUnknown;
    if(!app_action_tool_config_exec(app, args, &status)) {
        return;
    }

    if(status == CcStatusOk) {
        app_custom_inject_set_slot_provisioned(app, slot_number, false);
        app_custom_inject_reset_slot(app, (uint8_t)(slot_number - 1U));
        if(app->scene_manager && app->view_dispatcher) {
            app_custom_inject_save(app);
        }
    }
}

void app_action_custom_inject_clear(App* app) {
    CcStatusCode status = CcStatusUnknown;
    if(!app_action_tool_config_exec(app, "cmd=clear", &status)) {
        return;
    }

    if(status == CcStatusOk) {
        app_custom_inject_mark_all_unprovisioned(app);
        app_custom_inject_reset_all_slots(app);
        if(app->scene_manager && app->view_dispatcher) {
            app_custom_inject_save(app);
        }
    }
}

void app_action_custom_inject_list(App* app) {
    app_action_tool_config(app, "cmd=list");
}

void app_action_custom_inject_cancel(App* app) {
    app_action_tool_config(app, "cmd=cancel");
}

void app_action_custom_inject_bit(App* app) {
    uint32_t slot = 0;
    uint32_t bit = 0;
    uint32_t value = 0;

    if(!parse_u32_key_last(app->args_custom_inject_bit, "slot", 10, &slot) || slot < 1U || slot > 5U) {
        app_set_status(app, "Custom Inject bit invalid slot");
        return;
    }
    if(!parse_u32_key_last(app->args_custom_inject_bit, "bit", 10, &bit) || bit > 63U) {
        app_set_status(app, "Custom Inject bit invalid bit");
        return;
    }
    if(!parse_u32_key_last(app->args_custom_inject_bit, "value", 10, &value) || value > 1U) {
        app_set_status(app, "Custom Inject bit invalid value");
        return;
    }

    const uint8_t slot_number = (uint8_t)slot;
    if(!app_custom_inject_ensure_slot_ready(app, slot_number)) {
        return;
    }

    char args[96] = {0};
    const int wrote = snprintf(
        args,
        sizeof(args),
        "cmd=bit slot=%lu bit=%lu value=%lu",
        (unsigned long)slot,
        (unsigned long)bit,
        (unsigned long)value);
    if(wrote <= 0 || (size_t)wrote >= sizeof(args)) {
        app_set_status(app, "Custom Inject bit args too long");
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!app_action_tool_config_exec(app, args, &status)) {
        return;
    }

    if(status == CcStatusOk) {
        app_custom_inject_set_slot_provisioned(app, slot_number, true);
        app_custom_inject_update_profile_bit(app, slot_number, (uint8_t)bit, value != 0U, false);
        app_custom_inject_save(app);
    } else {
        app_custom_inject_set_slot_provisioned(app, slot_number, false);
    }
}

void app_action_custom_inject_clearbit(App* app) {
    uint32_t slot = 0;
    uint32_t bit = 0;

    if(!parse_u32_key_last(app->args_custom_inject_clearbit, "slot", 10, &slot) || slot < 1U ||
       slot > 5U) {
        app_set_status(app, "Custom Inject clearbit invalid slot");
        return;
    }
    if(!parse_u32_key_last(app->args_custom_inject_clearbit, "bit", 10, &bit) || bit > 63U) {
        app_set_status(app, "Custom Inject clearbit invalid bit");
        return;
    }

    const uint8_t slot_number = (uint8_t)slot;
    if(!app_custom_inject_ensure_slot_ready(app, slot_number)) {
        return;
    }

    char args[80] = {0};
    const int wrote = snprintf(
        args, sizeof(args), "cmd=clearbit slot=%lu bit=%lu", (unsigned long)slot, (unsigned long)bit);
    if(wrote <= 0 || (size_t)wrote >= sizeof(args)) {
        app_set_status(app, "Custom Inject clearbit args too long");
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!app_action_tool_config_exec(app, args, &status)) {
        return;
    }

    if(status == CcStatusOk) {
        app_custom_inject_set_slot_provisioned(app, slot_number, true);
        app_custom_inject_update_profile_bit(app, slot_number, (uint8_t)bit, false, true);
        app_custom_inject_save(app);
    } else {
        app_custom_inject_set_slot_provisioned(app, slot_number, false);
    }
}

void app_action_custom_inject_field(App* app) {
    uint32_t slot = 0U;
    uint32_t start = 0U;
    uint32_t len = 0U;

    if(!parse_u32_key_last(app->args_custom_inject_field, "slot", 10, &slot) || slot < 1U ||
       slot > 5U) {
        app_set_status(app, "Custom Inject field invalid slot");
        return;
    }
    if(!parse_u32_key_last(app->args_custom_inject_field, "start", 10, &start)) {
        app_set_status(app, "Custom Inject field invalid start");
        return;
    }
    if(!parse_u32_key_last(app->args_custom_inject_field, "len", 10, &len)) {
        app_set_status(app, "Custom Inject field invalid len");
        return;
    }

    char value_text[32] = {0};
    if(!arg_get_value_last(app->args_custom_inject_field, "value", value_text, sizeof(value_text))) {
        app_set_status(app, "Custom Inject field missing value");
        return;
    }

    char* end = NULL;
    unsigned long long value_u64 = strtoull(value_text, &end, 0);
    if(end == value_text || *end != '\0') {
        app_set_status(app, "Custom Inject field invalid value");
        return;
    }

    if(!app_custom_inject_field_range_valid(start, len)) {
        app_set_status(app, "Custom Inject field range invalid");
        return;
    }
    if(!app_custom_inject_field_value_valid(len, (uint64_t)value_u64)) {
        app_set_status(app, "Custom Inject field value out of range");
        return;
    }

    const uint8_t slot_number = (uint8_t)slot;
    if(!app_custom_inject_ensure_slot_ready(app, slot_number)) {
        return;
    }

    char args[128] = {0};
    const int wrote = snprintf(
        args,
        sizeof(args),
        "cmd=field slot=%lu start=%lu len=%lu value=%llu",
        (unsigned long)slot,
        (unsigned long)start,
        (unsigned long)len,
        value_u64);
    if(wrote <= 0 || (size_t)wrote >= sizeof(args)) {
        app_set_status(app, "Custom Inject field args too long");
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!app_action_tool_config_exec(app, args, &status)) {
        return;
    }

    if(status == CcStatusOk) {
        char* profile = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
        if(profile) {
            char sig_start[8] = {0};
            char sig_len[8] = {0};
            char sig_value[24] = {0};
            snprintf(sig_start, sizeof(sig_start), "%lu", (unsigned long)start);
            snprintf(sig_len, sizeof(sig_len), "%lu", (unsigned long)len);
            snprintf(sig_value, sizeof(sig_value), "%llu", value_u64);
            app_args_set_key_value(
                profile, sizeof(app->args_custom_inject_slots[0]), "sig_start", sig_start);
            app_args_set_key_value(
                profile, sizeof(app->args_custom_inject_slots[0]), "sig_len", sig_len);
            app_args_set_key_value(
                profile, sizeof(app->args_custom_inject_slots[0]), "sig_value", sig_value);
        }
        app_custom_inject_set_slot_provisioned(app, slot_number, true);
        app_custom_inject_update_profile_field(
            app, slot_number, (uint8_t)start, (uint8_t)len, (uint64_t)value_u64);
        app_custom_inject_save(app);
    } else {
        app_custom_inject_set_slot_provisioned(app, slot_number, false);
    }
}

void app_action_custom_inject_inject(App* app, uint8_t slot_number) {
    if(!app_custom_inject_slot_is_valid(slot_number)) {
        app_set_status(app, "Custom Inject invalid slot %u", slot_number);
        return;
    }

    if(!app_custom_inject_ensure_slot_ready(app, slot_number)) {
        return;
    }

    char args[96] = {0};
    if(app_custom_inject_build_inject_args(app, slot_number, args, sizeof(args))) {
        CcStatusCode status = CcStatusUnknown;
        if(!app_action_tool_config_exec(app, args, &status)) {
            return;
        }
        if(status == CcStatusBadArg) {
            app_set_status(
                app,
                "Inject needs live frame first (bus/id/ext match slot %u)",
                (unsigned)slot_number);
            app_append_monitor(
                app,
                "[hint] custom inject slot %u needs a recent matching frame before inject",
                (unsigned)slot_number);
        }
    }
}

void app_action_custom_inject_sync_slots(App* app) {
    if(!app) {
        return;
    }

    for(uint8_t slot_number = 1U; slot_number <= 5U; slot_number++) {
        const char* profile = app_custom_inject_profile_for_slot(app, slot_number);
        if(!app_custom_inject_profile_marked_used(profile)) {
            continue;
        }

        app_action_custom_inject_add(app, slot_number);
        app_action_custom_inject_modify(app, slot_number);
    }

    app_action_custom_inject_list(app);
}

static void app_custom_inject_trim_copy(const char* src, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(!src) {
        return;
    }

    while(*src == ' ' || *src == '\t') {
        src++;
    }

    app_copy_string(out, out_size, src);
    size_t len = strlen(out);
    while(len > 0U && (out[len - 1U] == ' ' || out[len - 1U] == '\t')) {
        out[len - 1U] = '\0';
        len--;
    }
}

static void app_custom_inject_safe_name(const char* src, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(!src || src[0] == '\0') {
        app_copy_string(out, out_size, "profile");
        return;
    }

    size_t w = 0U;
    for(size_t i = 0U; src[i] != '\0' && w + 1U < out_size; i++) {
        const unsigned char ch = (unsigned char)src[i];
        if(isalnum(ch)) {
            out[w++] = (char)tolower(ch);
        } else if(ch == ' ' || ch == '_' || ch == '-' || ch == '.') {
            if(w == 0U || out[w - 1U] != '_') {
                out[w++] = '_';
            }
        }
    }

    while(w > 0U && out[w - 1U] == '_') {
        w--;
    }

    if(w == 0U) {
        app_copy_string(out, out_size, "profile");
        return;
    }

    out[w] = '\0';
}

static void app_path_parent_dir(const char* path, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(!path || path[0] == '\0') {
        return;
    }

    app_copy_string(out, out_size, path);
    char* slash = strrchr(out, '/');
    if(slash) {
        *slash = '\0';
    } else {
        out[0] = '\0';
    }
}

static bool app_storage_ensure_dir(Storage* storage, const char* dir_path) {
    if(!storage || !dir_path || dir_path[0] == '\0') {
        return false;
    }

    if(storage_dir_exists(storage, dir_path)) {
        return true;
    }

    (void)storage_simply_mkdir(storage, dir_path);
    return storage_dir_exists(storage, dir_path);
}

static void app_ensure_startup_dirs(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return;
    }

    char data_dir[96] = {0};
    app_path_parent_dir(APP_CUSTOM_INJECT_CFG_PATH, data_dir, sizeof(data_dir));
    if(data_dir[0] != '\0') {
        (void)app_storage_ensure_dir(storage, data_dir);
    }

    (void)app_storage_ensure_dir(storage, APP_CUSTOM_INJECT_SET_DIR);
    (void)app_storage_ensure_dir(storage, APP_DBC_CONFIG_DIR);
    furi_record_close(RECORD_STORAGE);
}

static void app_custom_inject_build_set_path(const char* set_name, char* out_path, size_t out_size) {
    if(!out_path || out_size == 0U) {
        return;
    }

    char safe[48] = {0};
    app_custom_inject_safe_name(set_name, safe, sizeof(safe));
    snprintf(out_path, out_size, "%s/%s%s", APP_CUSTOM_INJECT_SET_DIR, safe, APP_SMART_INJECT_PROFILE_EXT);
    out_path[out_size - 1U] = '\0';
}

static void app_custom_inject_profile_value_or_default(
    const char* profile,
    const char* key,
    const char* fallback,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    if(!profile || !arg_get_value_last(profile, key, out, out_size) || out[0] == '\0') {
        app_copy_string(out, out_size, fallback);
    }
}

static void app_custom_inject_compose_slot_args(
    char* out,
    size_t out_size,
    uint8_t slot_index,
    const char* slot_name,
    const char* used,
    const char* bus,
    const char* id,
    const char* ext,
    const char* mask,
    const char* value,
    const char* xor_mask,
    const char* mux,
    const char* mux_start,
    const char* mux_len,
    const char* mux_value,
    const char* sig,
    const char* sig_start,
    const char* sig_len,
    const char* sig_value,
    const char* count,
    const char* interval_ms) {
    if(!out || out_size == 0U) {
        return;
    }

    char default_name[16] = {0};
    snprintf(default_name, sizeof(default_name), "Slot%u", (unsigned)(slot_index + 1U));
    const char* final_slot_name = (slot_name && slot_name[0]) ? slot_name : default_name;
    const char* final_used = (used && used[0]) ? used : "0";
    const char* final_bus = (bus && bus[0]) ? bus : "can0";
    const char* final_id = (id && id[0]) ? id : "000";
    const char* final_ext = (ext && ext[0]) ? ext : "0";
    const char* final_mask = (mask && mask[0]) ? mask : "0000000000000000";
    const char* final_value = (value && value[0]) ? value : "0000000000000000";
    const char* final_xor = (xor_mask && xor_mask[0]) ? xor_mask : "0000000000000000";
    const char* final_mux = (mux && mux[0]) ? mux : "0";
    const char* final_mux_start = (mux_start && mux_start[0]) ? mux_start : "0";
    const char* final_mux_len = (mux_len && mux_len[0]) ? mux_len : "1";
    const char* final_mux_value = (mux_value && mux_value[0]) ? mux_value : "0";
    const char* final_sig = (sig && sig[0]) ? sig : "0";
    const char* final_sig_start = (sig_start && sig_start[0]) ? sig_start : "0";
    const char* final_sig_len = (sig_len && sig_len[0]) ? sig_len : "1";
    const char* final_sig_value = (sig_value && sig_value[0]) ? sig_value : "0";
    const char* final_count = (count && count[0]) ? count : "1";
    const char* final_interval = (interval_ms && interval_ms[0]) ? interval_ms : "0";

    app_custom_inject_slot_defaults(slot_index, out, out_size);
    (void)app_args_set_key_value(out, out_size, "slot_name", final_slot_name);
    (void)app_args_set_key_value(out, out_size, "used", final_used);
    (void)app_args_set_key_value(out, out_size, "bus", final_bus);
    (void)app_args_set_key_value(out, out_size, "id", final_id);
    (void)app_args_set_key_value(out, out_size, "ext", final_ext);
    (void)app_args_set_key_value(out, out_size, "mask", final_mask);
    (void)app_args_set_key_value(out, out_size, "value", final_value);
    (void)app_args_set_key_value(out, out_size, "xor", final_xor);
    (void)app_args_set_key_value(out, out_size, "mux", final_mux);
    (void)app_args_set_key_value(out, out_size, "mux_start", final_mux_start);
    (void)app_args_set_key_value(out, out_size, "mux_len", final_mux_len);
    (void)app_args_set_key_value(out, out_size, "mux_value", final_mux_value);
    (void)app_args_set_key_value(out, out_size, "sig", final_sig);
    (void)app_args_set_key_value(out, out_size, "sig_start", final_sig_start);
    (void)app_args_set_key_value(out, out_size, "sig_len", final_sig_len);
    (void)app_args_set_key_value(out, out_size, "sig_value", final_sig_value);
    (void)app_args_set_key_value(out, out_size, "count", final_count);
    (void)app_args_set_key_value(out, out_size, "interval_ms", final_interval);
}

bool app_custom_inject_save_slot_set(App* app, const char* set_name) {
    if(!app) {
        return false;
    }

    char name_trim[32] = {0};
    app_custom_inject_trim_copy(set_name, name_trim, sizeof(name_trim));
    if(name_trim[0] == '\0') {
        app_set_status(app, "Smart Injection profile name required");
        return false;
    }

    char path[96] = {0};
    app_custom_inject_build_set_path(name_trim, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(!ff) {
        furi_record_close(RECORD_STORAGE);
        app_set_status(app, "Save Smart Injection profile failed");
        return false;
    }

    bool ok = false;
    do {
        char data_dir[96] = {0};
        app_path_parent_dir(APP_CUSTOM_INJECT_CFG_PATH, data_dir, sizeof(data_dir));
        if(!app_storage_ensure_dir(storage, data_dir)) {
            break;
        }

        if(!app_storage_ensure_dir(storage, APP_CUSTOM_INJECT_SET_DIR)) {
            break;
        }

        if(!flipper_format_file_open_always(ff, path)) {
            break;
        }
        if(!flipper_format_write_header_cstr(ff, APP_CUSTOM_INJECT_SET_TYPE, APP_CUSTOM_INJECT_SET_VER)) {
            break;
        }
        if(!flipper_format_write_string_cstr(ff, "name", name_trim)) {
            break;
        }

        bool slots_ok = true;
        for(uint8_t i = 0; i < 5U; i++) {
            const char* profile = app->args_custom_inject_slots[i];

            char default_name[16] = {0};
            char slot_name[24] = {0};
            char used[8] = {0};
            char bus[12] = {0};
            char id[16] = {0};
            char ext[8] = {0};
            char mask[24] = {0};
            char value[24] = {0};
            char xor_mask[24] = {0};
            char mux[8] = {0};
            char mux_start[16] = {0};
            char mux_len[16] = {0};
            char mux_value[24] = {0};
            char sig[8] = {0};
            char sig_start[16] = {0};
            char sig_len[16] = {0};
            char sig_value[24] = {0};
            char count[16] = {0};
            char interval_ms[16] = {0};

            snprintf(default_name, sizeof(default_name), "Slot%u", (unsigned)(i + 1U));
            app_custom_inject_profile_value_or_default(
                profile, "slot_name", default_name, slot_name, sizeof(slot_name));
            app_custom_inject_profile_value_or_default(profile, "used", "0", used, sizeof(used));
            app_custom_inject_profile_value_or_default(profile, "bus", "can0", bus, sizeof(bus));
            app_custom_inject_profile_value_or_default(profile, "id", "000", id, sizeof(id));
            app_custom_inject_profile_value_or_default(profile, "ext", "0", ext, sizeof(ext));
            app_custom_inject_profile_value_or_default(
                profile, "mask", "0000000000000000", mask, sizeof(mask));
            app_custom_inject_profile_value_or_default(
                profile, "value", "0000000000000000", value, sizeof(value));
            app_custom_inject_profile_value_or_default(
                profile, "xor", "0000000000000000", xor_mask, sizeof(xor_mask));
            app_custom_inject_profile_value_or_default(profile, "mux", "0", mux, sizeof(mux));
            app_custom_inject_profile_value_or_default(
                profile, "mux_start", "0", mux_start, sizeof(mux_start));
            app_custom_inject_profile_value_or_default(
                profile, "mux_len", "1", mux_len, sizeof(mux_len));
            app_custom_inject_profile_value_or_default(
                profile, "mux_value", "0", mux_value, sizeof(mux_value));
            app_custom_inject_profile_value_or_default(profile, "sig", "0", sig, sizeof(sig));
            app_custom_inject_profile_value_or_default(
                profile, "sig_start", "0", sig_start, sizeof(sig_start));
            app_custom_inject_profile_value_or_default(
                profile, "sig_len", "1", sig_len, sizeof(sig_len));
            app_custom_inject_profile_value_or_default(
                profile, "sig_value", "0", sig_value, sizeof(sig_value));
            app_custom_inject_profile_value_or_default(profile, "count", "1", count, sizeof(count));
            app_custom_inject_profile_value_or_default(
                profile, "interval_ms", "0", interval_ms, sizeof(interval_ms));

            char key[32] = {0};

            snprintf(key, sizeof(key), "slot%u_name", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, slot_name)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_used", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, used)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_bus", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, bus)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_id", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, id)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_ext", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, ext)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_mask", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, mask)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_value", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_xor", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, xor_mask)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_mux", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, mux)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_mux_start", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, mux_start)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_mux_len", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, mux_len)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_mux_value", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, mux_value)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_sig", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, sig)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_sig_start", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, sig_start)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_sig_len", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, sig_len)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_sig_value", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, sig_value)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_count", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, count)) {
                slots_ok = false;
                break;
            }
            snprintf(key, sizeof(key), "slot%u_interval_ms", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, interval_ms)) {
                slots_ok = false;
                break;
            }
        }
        if(!slots_ok) {
            break;
        }

        uint32_t active_slot = app_custom_inject_get_active_slot(app);
        if(!flipper_format_write_uint32(ff, "active_slot", &active_slot, 1)) {
            break;
        }

        ok = true;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        app_copy_string(app->custom_inject_set_name, sizeof(app->custom_inject_set_name), name_trim);
        app_set_status(app, "Saved Smart Injection profile: %s", name_trim);
    } else {
        app_set_status(app, "Save Smart Injection profile failed");
    }

    return ok;
}

bool app_custom_inject_load_slot_set_file(App* app, const char* file_path) {
    if(!app || !file_path || file_path[0] == '\0') {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    FuriString* value = furi_string_alloc();

    if(!ff || !file_type || !value) {
        if(value) furi_string_free(value);
        if(file_type) furi_string_free(file_type);
        if(ff) flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
        app_set_status(app, "Load Smart Injection profile failed");
        return false;
    }

    bool ok = false;
    char loaded_name[32] = {0};
    do {
        if(!flipper_format_file_open_existing(ff, file_path)) {
            break;
        }

        uint32_t version = 0U;
        if(!flipper_format_read_header(ff, file_type, &version)) {
            break;
        }
        const bool filetype_ok =
            furi_string_equal_str(file_type, APP_CUSTOM_INJECT_SET_TYPE) ||
            furi_string_equal_str(file_type, APP_CUSTOM_INJECT_SET_LEGACY_TYPE);
        if(!filetype_ok || version != APP_CUSTOM_INJECT_SET_VER) {
            break;
        }

        if(flipper_format_read_string(ff, "name", value)) {
            app_copy_string(loaded_name, sizeof(loaded_name), furi_string_get_cstr(value));
        }

        for(uint8_t i = 0; i < 5U; i++) {
            char default_name[16] = {0};
            char slot_name[24] = {0};
            char used[8] = {0};
            char bus[12] = {0};
            char id[16] = {0};
            char ext[8] = {0};
            char mask[24] = {0};
            char value_hex[24] = {0};
            char xor_mask[24] = {0};
            char mux[8] = {0};
            char mux_start[16] = {0};
            char mux_len[16] = {0};
            char mux_value[24] = {0};
            char sig[8] = {0};
            char sig_start[16] = {0};
            char sig_len[16] = {0};
            char sig_value[24] = {0};
            char count[16] = {0};
            char interval_ms[16] = {0};

            snprintf(default_name, sizeof(default_name), "Slot%u", (unsigned)(i + 1U));
            app_copy_string(slot_name, sizeof(slot_name), default_name);
            app_copy_string(used, sizeof(used), "0");
            app_copy_string(bus, sizeof(bus), "can0");
            app_copy_string(id, sizeof(id), "000");
            app_copy_string(ext, sizeof(ext), "0");
            app_copy_string(mask, sizeof(mask), "0000000000000000");
            app_copy_string(value_hex, sizeof(value_hex), "0000000000000000");
            app_copy_string(xor_mask, sizeof(xor_mask), "0000000000000000");
            app_copy_string(mux, sizeof(mux), "0");
            app_copy_string(mux_start, sizeof(mux_start), "0");
            app_copy_string(mux_len, sizeof(mux_len), "1");
            app_copy_string(mux_value, sizeof(mux_value), "0");
            app_copy_string(sig, sizeof(sig), "0");
            app_copy_string(sig_start, sizeof(sig_start), "0");
            app_copy_string(sig_len, sizeof(sig_len), "1");
            app_copy_string(sig_value, sizeof(sig_value), "0");
            app_copy_string(count, sizeof(count), "1");
            app_copy_string(interval_ms, sizeof(interval_ms), "0");

            char key[32] = {0};
            snprintf(key, sizeof(key), "slot%u_name", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(slot_name, sizeof(slot_name), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_used", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(used, sizeof(used), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_bus", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(bus, sizeof(bus), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_id", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(id, sizeof(id), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_ext", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(ext, sizeof(ext), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_mask", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(mask, sizeof(mask), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_value", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(value_hex, sizeof(value_hex), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_xor", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(xor_mask, sizeof(xor_mask), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_mux", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(mux, sizeof(mux), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_mux_start", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(mux_start, sizeof(mux_start), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_mux_len", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(mux_len, sizeof(mux_len), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_mux_value", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(mux_value, sizeof(mux_value), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_sig", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(sig, sizeof(sig), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_sig_start", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(sig_start, sizeof(sig_start), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_sig_len", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(sig_len, sizeof(sig_len), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_sig_value", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(sig_value, sizeof(sig_value), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_count", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(count, sizeof(count), furi_string_get_cstr(value));
            }
            snprintf(key, sizeof(key), "slot%u_interval_ms", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(interval_ms, sizeof(interval_ms), furi_string_get_cstr(value));
            }

            app_custom_inject_compose_slot_args(
                app->args_custom_inject_slots[i],
                sizeof(app->args_custom_inject_slots[i]),
                i,
                slot_name,
                used,
                bus,
                id,
                ext,
                mask,
                value_hex,
                xor_mask,
                mux,
                mux_start,
                mux_len,
                mux_value,
                sig,
                sig_start,
                sig_len,
                sig_value,
                count,
                interval_ms);
        }

        uint32_t active_slot = 0U;
        if(flipper_format_read_uint32(ff, "active_slot", &active_slot, 1) && active_slot < 5U) {
            app->custom_inject_active_slot = (uint8_t)active_slot;
        } else {
            app->custom_inject_active_slot = 0U;
        }

        app_custom_inject_mark_all_unprovisioned(app);
        app_custom_inject_save(app);
        ok = true;
    } while(false);

    furi_string_free(value);
    furi_string_free(file_type);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        if(loaded_name[0] == '\0') {
            const char* base = strrchr(file_path, '/');
            base = base ? (base + 1) : file_path;
            app_copy_string(loaded_name, sizeof(loaded_name), base);
            char* ext = strrchr(loaded_name, '.');
            if(ext) {
                *ext = '\0';
            }
        }
        if(loaded_name[0] != '\0') {
            app_copy_string(
                app->custom_inject_set_name, sizeof(app->custom_inject_set_name), loaded_name);
            app_set_status(app, "Loaded Smart Injection profile: %s", loaded_name);
        } else {
            app_set_status(app, "Loaded Smart Injection profile");
        }
    } else {
        app_set_status(app, "Load Smart Injection profile failed");
    }

    return ok;
}

static bool app_dbc_config_ensure_alloc(App* app) {
    if(!app) {
        return false;
    }
    if(app->dbc_config_signals) {
        return true;
    }
    app->dbc_config_signals =
        malloc(sizeof(AppDbcSignalCache) * APP_DBC_CFG_MAX_SIGNALS);
    if(!app->dbc_config_signals) {
        return false;
    }
    memset(app->dbc_config_signals, 0, sizeof(AppDbcSignalCache) * APP_DBC_CFG_MAX_SIGNALS);
    return true;
}

static void app_dbc_config_recount(App* app) {
    if(!app || !app->dbc_config_signals) {
        if(app) app->dbc_config_signal_count = 0U;
        return;
    }

    uint8_t count = 0U;
    for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_SIGNALS; i++) {
        if(app->dbc_config_signals[i].used) {
            count++;
        }
    }
    app->dbc_config_signal_count = count;
}

static int8_t app_dbc_config_find_slot_by_sid(const App* app, uint16_t sid) {
    if(!app || !app->dbc_config_signals) {
        return -1;
    }

    for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_SIGNALS; i++) {
        if(app->dbc_config_signals[i].used && app->dbc_config_signals[i].def.sid == sid) {
            return (int8_t)i;
        }
    }

    return -1;
}

static int8_t app_dbc_config_find_free_slot(const App* app) {
    if(!app || !app->dbc_config_signals) {
        return -1;
    }

    for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_SIGNALS; i++) {
        if(!app->dbc_config_signals[i].used) {
            return (int8_t)i;
        }
    }

    return -1;
}

static bool app_dbc_config_upsert_signal(
    App* app,
    const CcDbcSignalDef* def,
    const char* signal_name) {
    if(!app || !def || !app_dbc_config_ensure_alloc(app)) {
        return false;
    }

    int8_t slot = app_dbc_config_find_slot_by_sid(app, def->sid);
    if(slot < 0) {
        slot = app_dbc_config_find_free_slot(app);
    }
    if(slot < 0) {
        return false;
    }

    AppDbcSignalCache* cached = &app->dbc_config_signals[(uint8_t)slot];
    const uint8_t preserved_map_count = cached->map_count;
    AppDbcValueMap preserved_maps[APP_DBC_CFG_MAX_MAPS] = {0};
    char preserved_name[APP_DBC_CFG_SIGNAL_NAME_MAX] = {0};
    if(cached->used && cached->def.sid == def->sid && preserved_map_count > 0U) {
        memcpy(preserved_maps, cached->maps, sizeof(preserved_maps));
    }
    if(cached->used && cached->def.sid == def->sid && cached->signal_name[0] != '\0') {
        app_copy_string(preserved_name, sizeof(preserved_name), cached->signal_name);
    }

    memset(cached, 0, sizeof(AppDbcSignalCache));
    cached->used = true;
    cached->def = *def;
    if(signal_name && signal_name[0] != '\0') {
        app_copy_string(cached->signal_name, sizeof(cached->signal_name), signal_name);
    } else if(preserved_name[0] != '\0') {
        app_copy_string(cached->signal_name, sizeof(cached->signal_name), preserved_name);
    } else {
        snprintf(cached->signal_name, sizeof(cached->signal_name), "SID%u", (unsigned)def->sid);
        cached->signal_name[sizeof(cached->signal_name) - 1U] = '\0';
    }

    if(preserved_map_count > 0U) {
        cached->map_count = preserved_map_count;
        memcpy(cached->maps, preserved_maps, sizeof(preserved_maps));
    }

    app_dbc_config_recount(app);
    return true;
}

static bool app_dbc_config_remove_signal(App* app, uint16_t sid) {
    if(!app) {
        return false;
    }

    const int8_t slot = app_dbc_config_find_slot_by_sid(app, sid);
    if(slot < 0) {
        return false;
    }

    memset(&app->dbc_config_signals[(uint8_t)slot], 0, sizeof(AppDbcSignalCache));
    app_dbc_config_recount(app);
    return true;
}

static const char* app_dbc_bus_to_string(CcBus bus) {
    switch(bus) {
    case CcBusCan0:
        return "can0";
    case CcBusCan1:
        return "can1";
    case CcBusBoth:
        return "both";
    default:
        return "can0";
    }
}

static bool app_dbc_parse_bus(const char* text, CcBus* out_bus) {
    if(!text || !out_bus) {
        return false;
    }

    if(strcmp(text, "can0") == 0) {
        *out_bus = CcBusCan0;
        return true;
    }

    if(strcmp(text, "can1") == 0) {
        *out_bus = CcBusCan1;
        return true;
    }

    if(strcmp(text, "both") == 0) {
        *out_bus = CcBusBoth;
        return true;
    }

    return false;
}

static const char* app_dbc_order_to_string(const CcDbcSignalDef* def) {
    return def && def->motorola_order ? "motorola" : "intel";
}

static const char* app_dbc_sign_to_string(const CcDbcSignalDef* def) {
    return def && def->signed_value ? "s" : "u";
}

static void app_dbc_compose_path(const char* config_name, char* out_path, size_t out_size) {
    if(!out_path || out_size == 0U) {
        return;
    }

    char safe[48] = {0};
    app_custom_inject_safe_name(config_name, safe, sizeof(safe));
    snprintf(out_path, out_size, "%s/%s%s", APP_DBC_CONFIG_DIR, safe, APP_DBC_DECODE_PROFILE_EXT);
    out_path[out_size - 1U] = '\0';
}

void app_dbc_config_reset(App* app) {
    if(!app) {
        return;
    }

    if(app->dbc_config_signals) {
        free(app->dbc_config_signals);
        app->dbc_config_signals = NULL;
    }
    app->dbc_config_signal_count = 0U;
}

const char* app_dbc_config_lookup_label(const App* app, uint16_t sid, int64_t raw) {
    if(!app) {
        return NULL;
    }

    const int8_t slot = app_dbc_config_find_slot_by_sid(app, sid);
    if(slot < 0) {
        return NULL;
    }

    const AppDbcSignalCache* signal = &app->dbc_config_signals[(uint8_t)slot];
    for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_MAPS; i++) {
        const AppDbcValueMap* map = &signal->maps[i];
        if(map->used && map->label[0] != '\0' && map->raw == raw) {
            return map->label;
        }
    }

    return NULL;
}

const char* app_dbc_config_lookup_signal_name(const App* app, uint16_t sid) {
    if(!app) {
        return NULL;
    }

    const int8_t slot = app_dbc_config_find_slot_by_sid(app, sid);
    if(slot < 0) {
        return NULL;
    }

    const AppDbcSignalCache* signal = &app->dbc_config_signals[(uint8_t)slot];
    if(signal->signal_name[0] == '\0') {
        return NULL;
    }

    return signal->signal_name;
}

static bool app_dbc_config_apply_to_firmware(App* app) {
    if(!app_require_connected(app) || !app->dbc_config_signals) {
        return false;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_dbc_clear(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "DBC profile apply: clear transport error");
        app_append_monitor(app, "[dbc profile] clear transport error");
        return false;
    }

    app_append_monitor(app, "[dbc profile] clear => %s", cc_status_to_string(status));
    if(status != CcStatusOk) {
        app_set_status(app, "DBC profile apply: clear => %s", cc_status_to_string(status));
        return false;
    }

    uint8_t applied = 0U;
    for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_SIGNALS; i++) {
        const AppDbcSignalCache* signal = &app->dbc_config_signals[i];
        if(!signal->used) {
            continue;
        }

        status = CcStatusUnknown;
        if(!cc_client_dbc_add_signal(app->client, &signal->def, &status)) {
            app->connected = false;
            app_set_status(app, "DBC profile apply: add transport error (sid=%u)", signal->def.sid);
            app_append_monitor(
                app, "[dbc profile] add sid=%u transport error", (unsigned)signal->def.sid);
            return false;
        }

        app_append_monitor(
            app,
            "[dbc profile] add sid=%u => %s",
            (unsigned)signal->def.sid,
            cc_status_to_string(status));
        if(status != CcStatusOk) {
            app_set_status(
                app,
                "DBC profile apply: add sid=%u => %s",
                (unsigned)signal->def.sid,
                cc_status_to_string(status));
            return false;
        }
        applied++;
    }

    app_set_status(app, "DBC profile applied (%u signals)", (unsigned)applied);
    return true;
}

bool app_dbc_config_save_file(App* app, const char* config_name) {
    if(!app || !app->dbc_config_signals) {
        return false;
    }

    char name_trim[32] = {0};
    app_custom_inject_trim_copy(config_name, name_trim, sizeof(name_trim));
    if(name_trim[0] == '\0') {
        app_set_status(app, "DBC profile name required");
        return false;
    }

    char path[96] = {0};
    app_dbc_compose_path(name_trim, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(!ff) {
        furi_record_close(RECORD_STORAGE);
        app_set_status(app, "Save DBC profile failed");
        return false;
    }

    bool ok = false;
    do {
        char data_dir[96] = {0};
        app_path_parent_dir(APP_CUSTOM_INJECT_CFG_PATH, data_dir, sizeof(data_dir));
        if(!app_storage_ensure_dir(storage, data_dir)) {
            break;
        }

        if(!app_storage_ensure_dir(storage, APP_DBC_CONFIG_DIR)) {
            break;
        }

        if(!flipper_format_file_open_always(ff, path)) {
            break;
        }
        if(!flipper_format_write_header_cstr(ff, APP_DBC_CONFIG_TYPE, APP_DBC_CONFIG_VER)) {
            break;
        }
        if(!flipper_format_write_string_cstr(ff, "name", name_trim)) {
            break;
        }

        app_dbc_config_recount(app);
        uint32_t signal_count = app->dbc_config_signal_count;
        if(!flipper_format_write_uint32(ff, "signal_count", &signal_count, 1U)) {
            break;
        }

        uint8_t out_index = 0U;
        for(uint8_t i = 0U; i < APP_DBC_CFG_MAX_SIGNALS; i++) {
            const AppDbcSignalCache* signal = &app->dbc_config_signals[i];
            if(!signal->used) {
                continue;
            }
            out_index++;

            char key[40] = {0};
            char value[64] = {0};

            snprintf(key, sizeof(key), "signal%u_sid", (unsigned)out_index);
            snprintf(value, sizeof(value), "%u", (unsigned)signal->def.sid);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_name", (unsigned)out_index);
            if(!flipper_format_write_string_cstr(ff, key, signal->signal_name)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_bus", (unsigned)out_index);
            if(!flipper_format_write_string_cstr(ff, key, app_dbc_bus_to_string(signal->def.bus))) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_id", (unsigned)out_index);
            snprintf(value, sizeof(value), "%lX", (unsigned long)signal->def.id);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_ext", (unsigned)out_index);
            snprintf(value, sizeof(value), "%u", signal->def.ext ? 1U : 0U);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_start", (unsigned)out_index);
            snprintf(value, sizeof(value), "%u", signal->def.start_bit);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_len", (unsigned)out_index);
            snprintf(value, sizeof(value), "%u", signal->def.bit_len);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_order", (unsigned)out_index);
            if(!flipper_format_write_string_cstr(ff, key, app_dbc_order_to_string(&signal->def))) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_sign", (unsigned)out_index);
            if(!flipper_format_write_string_cstr(ff, key, app_dbc_sign_to_string(&signal->def))) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_factor", (unsigned)out_index);
            snprintf(value, sizeof(value), "%.7g", (double)signal->def.factor);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_offset", (unsigned)out_index);
            snprintf(value, sizeof(value), "%.7g", (double)signal->def.offset);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_min", (unsigned)out_index);
            snprintf(value, sizeof(value), "%.7g", (double)signal->def.min);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_max", (unsigned)out_index);
            snprintf(value, sizeof(value), "%.7g", (double)signal->def.max);
            if(!flipper_format_write_string_cstr(ff, key, value)) {
                break;
            }

            snprintf(key, sizeof(key), "signal%u_unit", (unsigned)out_index);
            if(!flipper_format_write_string_cstr(ff, key, signal->def.unit)) {
                break;
            }

            uint8_t map_written = 0U;
            for(uint8_t m = 0U; m < APP_DBC_CFG_MAX_MAPS; m++) {
                if(signal->maps[m].used && signal->maps[m].label[0] != '\0') {
                    map_written++;
                }
            }

            snprintf(key, sizeof(key), "signal%u_map_count", (unsigned)out_index);
            uint32_t map_count_u32 = map_written;
            if(!flipper_format_write_uint32(ff, key, &map_count_u32, 1U)) {
                break;
            }

            uint8_t map_out_index = 0U;
            for(uint8_t m = 0U; m < APP_DBC_CFG_MAX_MAPS; m++) {
                const AppDbcValueMap* map = &signal->maps[m];
                if(!map->used || map->label[0] == '\0') {
                    continue;
                }
                map_out_index++;

                snprintf(
                    key, sizeof(key), "signal%u_map%u_raw", (unsigned)out_index, (unsigned)map_out_index);
                snprintf(value, sizeof(value), "%lld", (long long)map->raw);
                if(!flipper_format_write_string_cstr(ff, key, value)) {
                    break;
                }

                snprintf(
                    key,
                    sizeof(key),
                    "signal%u_map%u_label",
                    (unsigned)out_index,
                    (unsigned)map_out_index);
                if(!flipper_format_write_string_cstr(ff, key, map->label)) {
                    break;
                }
            }
        }

        ok = true;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        app_copy_string(app->dbc_config_name, sizeof(app->dbc_config_name), name_trim);
        app_copy_string(app->dbc_config_save_name, sizeof(app->dbc_config_save_name), name_trim);
        app_set_status(app, "Saved DBC profile: %s", name_trim);
    } else {
        app_set_status(app, "Save DBC profile failed");
    }

    return ok;
}

static bool app_ff_read_string_key(FlipperFormat* ff, const char* key, FuriString* out) {
    if(!ff || !key || !out) {
        return false;
    }

    if(!flipper_format_rewind(ff)) {
        return false;
    }

    return flipper_format_read_string(ff, key, out);
}

static bool app_ff_read_u32_key(FlipperFormat* ff, const char* key, uint32_t* out_value) {
    if(!ff || !key || !out_value) {
        return false;
    }

    if(!flipper_format_rewind(ff)) {
        return false;
    }

    return flipper_format_read_uint32(ff, key, out_value, 1U);
}

bool app_dbc_config_load_file(App* app, const char* file_path, bool apply_to_firmware) {
    if(!app || !file_path || file_path[0] == '\0') {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    FuriString* value = furi_string_alloc();

    if(!ff || !file_type || !value) {
        if(value) {
            furi_string_free(value);
        }
        if(file_type) {
            furi_string_free(file_type);
        }
        if(ff) {
            flipper_format_free(ff);
        }
        furi_record_close(RECORD_STORAGE);
        app_set_status(app, "Load DBC profile failed");
        return false;
    }

    bool ok = false;
    char loaded_name[32] = {0};
    do {
        if(!flipper_format_file_open_existing(ff, file_path)) {
            break;
        }

        uint32_t version = 0U;
        if(!flipper_format_read_header(ff, file_type, &version)) {
            break;
        }
        const bool filetype_ok =
            furi_string_equal_str(file_type, APP_DBC_CONFIG_TYPE) ||
            furi_string_equal_str(file_type, APP_DBC_CONFIG_LEGACY_TYPE);
        if(!filetype_ok || version != APP_DBC_CONFIG_VER) {
            break;
        }

        if(app_ff_read_string_key(ff, "name", value)) {
            app_copy_string(loaded_name, sizeof(loaded_name), furi_string_get_cstr(value));
        }

        app_dbc_config_reset(app);

        uint32_t signal_count_u32 = 0U;
        if(!app_ff_read_u32_key(ff, "signal_count", &signal_count_u32)) {
            signal_count_u32 = 0U;
        }
        if(signal_count_u32 > APP_DBC_CFG_MAX_SIGNALS) {
            signal_count_u32 = APP_DBC_CFG_MAX_SIGNALS;
        }

        for(uint8_t i = 1U; i <= (uint8_t)signal_count_u32; i++) {
            char key[40] = {0};
            char text[64] = {0};
            CcDbcSignalDef def = {0};
            char signal_name[APP_DBC_CFG_SIGNAL_NAME_MAX] = {0};

            snprintf(key, sizeof(key), "signal%u_sid", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            char* end = NULL;
            unsigned long sid_ul = strtoul(text, &end, 10);
            if(end == text || *end != '\0' || sid_ul > 0xFFFFUL) {
                continue;
            }
            def.sid = (uint16_t)sid_ul;

            snprintf(key, sizeof(key), "signal%u_bus", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            if(!app_dbc_parse_bus(furi_string_get_cstr(value), &def.bus)) {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_id", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            end = NULL;
            unsigned long id_ul = strtoul(text, &end, 16);
            if(end == text || *end != '\0') {
                continue;
            }
            def.id = (uint32_t)id_ul;

            snprintf(key, sizeof(key), "signal%u_ext", (unsigned)i);
            if(app_ff_read_string_key(ff, key, value)) {
                app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
                def.ext = (strcmp(text, "1") == 0 || strcmp(text, "true") == 0);
            }

            snprintf(key, sizeof(key), "signal%u_start", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            end = NULL;
            unsigned long start_ul = strtoul(text, &end, 10);
            if(end == text || *end != '\0' || start_ul > 63UL) {
                continue;
            }
            def.start_bit = (uint8_t)start_ul;

            snprintf(key, sizeof(key), "signal%u_len", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            end = NULL;
            unsigned long len_ul = strtoul(text, &end, 10);
            if(end == text || *end != '\0' || len_ul == 0UL || len_ul > 64UL) {
                continue;
            }
            def.bit_len = (uint8_t)len_ul;

            snprintf(key, sizeof(key), "signal%u_order", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            const char* order = furi_string_get_cstr(value);
            if(strcmp(order, "motorola") == 0) {
                def.motorola_order = true;
            } else if(strcmp(order, "intel") == 0) {
                def.motorola_order = false;
            } else {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_sign", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            const char* sign = furi_string_get_cstr(value);
            if(strcmp(sign, "s") == 0) {
                def.signed_value = true;
            } else if(strcmp(sign, "u") == 0) {
                def.signed_value = false;
            } else {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_factor", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            def.factor = strtof(text, &end);
            if(end == text || *end != '\0') {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_offset", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            def.offset = strtof(text, &end);
            if(end == text || *end != '\0') {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_min", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            def.min = strtof(text, &end);
            if(end == text || *end != '\0') {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_max", (unsigned)i);
            if(!app_ff_read_string_key(ff, key, value)) {
                continue;
            }
            app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
            def.max = strtof(text, &end);
            if(end == text || *end != '\0') {
                continue;
            }

            snprintf(key, sizeof(key), "signal%u_unit", (unsigned)i);
            if(app_ff_read_string_key(ff, key, value)) {
                app_copy_string(def.unit, sizeof(def.unit), furi_string_get_cstr(value));
            } else {
                def.unit[0] = '\0';
            }

            snprintf(key, sizeof(key), "signal%u_name", (unsigned)i);
            if(app_ff_read_string_key(ff, key, value)) {
                app_copy_string(signal_name, sizeof(signal_name), furi_string_get_cstr(value));
            } else {
                snprintf(signal_name, sizeof(signal_name), "SID%u", (unsigned)def.sid);
                signal_name[sizeof(signal_name) - 1U] = '\0';
            }

            if(!app_dbc_config_upsert_signal(app, &def, signal_name)) {
                continue;
            }

            const int8_t slot = app_dbc_config_find_slot_by_sid(app, def.sid);
            if(slot < 0) {
                continue;
            }
            AppDbcSignalCache* signal = &app->dbc_config_signals[(uint8_t)slot];

            snprintf(key, sizeof(key), "signal%u_map_count", (unsigned)i);
            uint32_t map_count_u32 = 0U;
            if(!app_ff_read_u32_key(ff, key, &map_count_u32)) {
                map_count_u32 = 0U;
            }
            if(map_count_u32 > APP_DBC_CFG_MAX_MAPS) {
                map_count_u32 = APP_DBC_CFG_MAX_MAPS;
            }

            uint8_t map_set = 0U;
            for(uint8_t map_i = 1U; map_i <= (uint8_t)map_count_u32; map_i++) {
                char raw_key[40] = {0};
                char label_key[40] = {0};
                snprintf(raw_key, sizeof(raw_key), "signal%u_map%u_raw", (unsigned)i, (unsigned)map_i);
                snprintf(
                    label_key, sizeof(label_key), "signal%u_map%u_label", (unsigned)i, (unsigned)map_i);

                if(!app_ff_read_string_key(ff, raw_key, value)) {
                    continue;
                }
                app_copy_string(text, sizeof(text), furi_string_get_cstr(value));
                int64_t raw_i64 = 0;
                if(!parse_i64_text(text, &raw_i64)) {
                    continue;
                }

                if(!app_ff_read_string_key(ff, label_key, value)) {
                    continue;
                }
                const char* label = furi_string_get_cstr(value);
                if(!label || label[0] == '\0') {
                    continue;
                }

                if(map_set >= APP_DBC_CFG_MAX_MAPS) {
                    break;
                }
                signal->maps[map_set].used = true;
                signal->maps[map_set].raw = raw_i64;
                app_copy_string(signal->maps[map_set].label, sizeof(signal->maps[map_set].label), label);
                map_set++;
            }
            signal->map_count = map_set;
        }

        app_dbc_config_recount(app);

        if(loaded_name[0] == '\0') {
            const char* base = strrchr(file_path, '/');
            base = base ? (base + 1) : file_path;
            app_copy_string(loaded_name, sizeof(loaded_name), base);
            char* ext = strrchr(loaded_name, '.');
            if(ext) {
                *ext = '\0';
            }
        }
        if(loaded_name[0] == '\0') {
            app_copy_string(loaded_name, sizeof(loaded_name), "dbc_profile");
        }
        app_copy_string(app->dbc_config_name, sizeof(app->dbc_config_name), loaded_name);
        app_copy_string(app->dbc_config_save_name, sizeof(app->dbc_config_save_name), loaded_name);

        ok = !apply_to_firmware || app_dbc_config_apply_to_firmware(app);
    } while(false);

    furi_string_free(value);
    furi_string_free(file_type);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        app_set_status(
            app,
            apply_to_firmware ? "Loaded+applied DBC profile: %s" : "Loaded DBC profile: %s",
            app->dbc_config_name[0] ? app->dbc_config_name : "dbc_profile");
    } else {
        app_set_status(app, "Load DBC profile failed");
    }

    return ok;
}

void app_action_bus_set_cfg(App* app, CcBus bus, const char* args) {
    if(!app_require_connected(app)) {
        return;
    }

    uint32_t bitrate = 0;
    if(!parse_u32_key(args, "bitrate", 10, &bitrate)) {
        app_set_status(app, "BUS_SET_CFG parse error: bitrate");
        return;
    }

    bool listen = false;
    char mode[16] = {0};
    if(arg_get_value(args, "mode", mode, sizeof(mode))) {
        listen = (strcmp(mode, "listen") == 0 || strcmp(mode, "listen_only") == 0);
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_bus_set_cfg(app->client, bus, bitrate, listen, &status)) {
        app->connected = false;
        app_set_status(app, "BUS_SET_CFG transport error");
        app_append_monitor(app, "[cmd] bus set cfg transport error");
        return;
    }

    app_set_status(app, "BUS_SET_CFG %s => %s", cc_bus_to_string(bus), cc_status_to_string(status));
    app_append_monitor(
        app,
        "[cmd] bus set cfg %s bitrate=%lu mode=%s => %s",
        cc_bus_to_string(bus),
        (unsigned long)bitrate,
        listen ? "listen" : "normal",
        cc_status_to_string(status));
}

void app_action_bus_get_cfg(App* app, CcBus bus) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_bus_get_cfg(app->client, bus, &status)) {
        app->connected = false;
        app_set_status(app, "BUS_GET_CFG transport error");
        app_append_monitor(app, "[cmd] bus get cfg transport error");
        return;
    }

    app_set_status(app, "BUS_GET_CFG %s => %s", cc_bus_to_string(bus), cc_status_to_string(status));
    app_append_monitor(app, "[cmd] bus get cfg %s => %s", cc_bus_to_string(bus), cc_status_to_string(status));
}

void app_action_bus_set_filter(App* app, CcBus bus, const char* args) {
    if(!app_require_connected(app)) {
        return;
    }

    uint32_t mask = 0x7FF;
    uint32_t filter = 0;
    bool ext_match = false;
    bool ext = false;

    parse_u32_key(args, "mask", 16, &mask);
    parse_u32_key(args, "filter", 16, &filter);
    parse_bool_key(args, "ext_match", &ext_match);
    parse_bool_key(args, "ext", &ext);

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_bus_set_filter(app->client, bus, mask, filter, ext_match, ext, &status)) {
        app->connected = false;
        app_set_status(app, "BUS_SET_FILTER transport error");
        app_append_monitor(app, "[cmd] bus set filter transport error");
        return;
    }

    app_set_status(
        app, "BUS_SET_FILTER %s => %s", cc_bus_to_string(bus), cc_status_to_string(status));
    app_append_monitor(
        app,
        "[cmd] bus set filter %s mask=0x%lX filter=0x%lX ext_match=%u ext=%u => %s",
        cc_bus_to_string(bus),
        (unsigned long)mask,
        (unsigned long)filter,
        ext_match ? 1U : 0U,
        ext ? 1U : 0U,
        cc_status_to_string(status));
}

void app_action_bus_clear_filter(App* app, CcBus bus) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_bus_clear_filter(app->client, bus, &status)) {
        app->connected = false;
        app_set_status(app, "BUS_CLEAR_FILTER transport error");
        app_append_monitor(app, "[cmd] bus clear filter transport error");
        return;
    }

    app_set_status(
        app, "BUS_CLEAR_FILTER %s => %s", cc_bus_to_string(bus), cc_status_to_string(status));
    app_append_monitor(
        app, "[cmd] bus clear filter %s => %s", cc_bus_to_string(bus), cc_status_to_string(status));
}

void app_action_dbc_clear(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_dbc_clear(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "DBC_CLEAR transport error");
        app_append_monitor(app, "[cmd] dbc clear transport error");
        return;
    }

    app_set_status(app, "DBC_CLEAR => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] dbc clear => %s", cc_status_to_string(status));
    if(status == CcStatusOk) {
        app_dbc_config_reset(app);
    }
}

void app_action_dbc_add(App* app, const char* args) {
    if(!app_require_connected(app)) {
        return;
    }

    CcDbcSignalDef def = {0};

    if(!parse_bus_key(args, "bus", &def.bus)) {
        app_set_status(app, "DBC_ADD parse error: bus");
        return;
    }

    if(!parse_u32_key(args, "id", 16, &def.id)) {
        app_set_status(app, "DBC_ADD parse error: id");
        return;
    }

    parse_bool_key(args, "ext", &def.ext);

    uint32_t start = 0;
    uint32_t len = 0;
    if(!parse_u32_key(args, "start", 10, &start) || !parse_u32_key(args, "len", 10, &len) ||
       start > 63U || len == 0U || len > 64U) {
        app_set_status(app, "DBC_ADD parse error: start/len");
        return;
    }

    def.start_bit = (uint8_t)start;
    def.bit_len = (uint8_t)len;

    char order[16] = {0};
    char sign[8] = {0};

    if(!arg_get_value(args, "order", order, sizeof(order)) ||
       !arg_get_value(args, "sign", sign, sizeof(sign))) {
        app_set_status(app, "DBC_ADD parse error: order/sign");
        return;
    }

    if(strcmp(order, "motorola") == 0) {
        def.motorola_order = true;
    } else if(strcmp(order, "intel") == 0) {
        def.motorola_order = false;
    } else {
        app_set_status(app, "DBC_ADD parse error: order");
        return;
    }

    if(strcmp(sign, "s") == 0) {
        def.signed_value = true;
    } else if(strcmp(sign, "u") == 0) {
        def.signed_value = false;
    } else {
        app_set_status(app, "DBC_ADD parse error: sign");
        return;
    }

    if(!parse_float_key(args, "factor", &def.factor) || !parse_float_key(args, "offset", &def.offset) ||
       !parse_float_key(args, "min", &def.min) || !parse_float_key(args, "max", &def.max)) {
        app_set_status(app, "DBC_ADD parse error: factor/offset/min/max");
        return;
    }

    def.unit[0] = '\0';
    arg_get_value(args, "unit", def.unit, sizeof(def.unit));

    uint32_t sid = 0;
    if(!parse_u32_key(args, "sid", 10, &sid) || sid > 0xFFFFU) {
        app_set_status(app, "DBC_ADD parse error: sid");
        return;
    }
    def.sid = (uint16_t)sid;

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_dbc_add_signal(app->client, &def, &status)) {
        app->connected = false;
        app_set_status(app, "DBC_ADD transport error");
        app_append_monitor(app, "[cmd] dbc add transport error");
        return;
    }

    app_set_status(app, "DBC_ADD => %s", cc_status_to_string(status));
    app_append_monitor(
        app,
        "[cmd] dbc add sid=%u %s id=0x%lX => %s",
        def.sid,
        cc_bus_to_string(def.bus),
        (unsigned long)def.id,
        cc_status_to_string(status));
    if(status == CcStatusOk) {
        char signal_name[APP_DBC_CFG_SIGNAL_NAME_MAX] = {0};
        if(!arg_get_value(args, "name", signal_name, sizeof(signal_name)) || signal_name[0] == '\0') {
            snprintf(signal_name, sizeof(signal_name), "SID%u", (unsigned)def.sid);
            signal_name[sizeof(signal_name) - 1U] = '\0';
        }
        (void)app_dbc_config_upsert_signal(app, &def, signal_name);
    }
}

void app_action_dbc_remove(App* app, const char* args) {
    if(!app_require_connected(app)) {
        return;
    }

    uint32_t sid = 0;
    if(!parse_u32_key(args, "sid", 10, &sid) || sid > 0xFFFFU) {
        app_set_status(app, "DBC_REMOVE parse error: sid");
        return;
    }

    CcStatusCode status = CcStatusUnknown;
    if(!cc_client_dbc_remove_signal(app->client, (uint16_t)sid, &status)) {
        app->connected = false;
        app_set_status(app, "DBC_REMOVE transport error");
        app_append_monitor(app, "[cmd] dbc remove transport error");
        return;
    }

    app_set_status(app, "DBC_REMOVE sid=%lu => %s", (unsigned long)sid, cc_status_to_string(status));
    app_append_monitor(
        app, "[cmd] dbc remove sid=%lu => %s", (unsigned long)sid, cc_status_to_string(status));
    if(status == CcStatusOk) {
        (void)app_dbc_config_remove_signal(app, (uint16_t)sid);
    }
}

void app_action_dbc_list(App* app) {
    if(!app_require_connected(app)) {
        return;
    }

    CcStatusCode status = CcStatusUnknown;

    if(!cc_client_dbc_list(app->client, &status)) {
        app->connected = false;
        app_set_status(app, "DBC_LIST transport error");
        app_append_monitor(app, "[cmd] dbc list transport error");
        return;
    }

    app_set_status(app, "DBC_LIST => %s", cc_status_to_string(status));
    app_append_monitor(app, "[cmd] dbc list => %s", cc_status_to_string(status));
}

static void app_set_default_args(App* app) {
    strncpy(app->args_read_all, "bus=can0 ascii=0", sizeof(app->args_read_all) - 1U);

    strncpy(
        app->args_filtered,
        "bus=can0 mask=7FF filter=273 ext_match=0 ext=0 ascii=0",
        sizeof(app->args_filtered) - 1U);

    strncpy(
        app->args_write_tool,
        "bus=can0 id=1D5 ext=0 rtr=0 dlc=8 data=102030AABBCCDDEE count=1 interval_ms=250",
        sizeof(app->args_write_tool) - 1U);

    strncpy(app->args_speed, "bus=can0", sizeof(app->args_speed) - 1U);
    strncpy(app->args_valtrack, "bus=can0 strict=1", sizeof(app->args_valtrack) - 1U);
    strncpy(app->args_unique_ids, "bus=can0", sizeof(app->args_unique_ids) - 1U);
    strncpy(app->args_bittrack, "bus=can0 id=273 ext=0", sizeof(app->args_bittrack) - 1U);

    strncpy(
        app->args_reverse_auto,
        "bus=can0 calibration_ms=20000 monitor_ms=200000 summary=0",
        sizeof(app->args_reverse_auto) - 1U);

    strncpy(
        app->args_reverse_read,
        "bus=can0 mode=read id=273 ext=0 bytes=0,1,2",
        sizeof(app->args_reverse_read) - 1U);

    strncpy(app->args_obd_pid, "bus=can0 pid=0C interval_ms=250", sizeof(app->args_obd_pid) - 1U);
    strncpy(app->args_dbc_decode, "bus=can0", sizeof(app->args_dbc_decode) - 1U);
    strncpy(
        app->args_custom_inject_start, "bus=can0", sizeof(app->args_custom_inject_start) - 1U);
    strncpy(app->args_replay, "bus=can0 id=2C1 ext=0", sizeof(app->args_replay) - 1U);

    app_custom_inject_reset_all_slots(app);
    strncpy(
        app->args_custom_inject_bit,
        "slot=1 bit=0 value=1",
        sizeof(app->args_custom_inject_bit) - 1U);
    strncpy(
        app->args_custom_inject_clearbit,
        "slot=1 bit=0",
        sizeof(app->args_custom_inject_clearbit) - 1U);
    strncpy(
        app->args_custom_inject_field,
        "slot=1 start=0 len=1 value=0",
        sizeof(app->args_custom_inject_field) - 1U);
    app->led_brightness = 5U;
    app->custom_inject_active_slot = 0U;
    strncpy(app->custom_inject_edit_name, "slot_name=Slot1", sizeof(app->custom_inject_edit_name) - 1U);
    strncpy(app->custom_inject_edit_bus, "bus=can0", sizeof(app->custom_inject_edit_bus) - 1U);
    strncpy(app->custom_inject_edit_id, "id=000", sizeof(app->custom_inject_edit_id) - 1U);
    strncpy(app->custom_inject_edit_count, "count=1", sizeof(app->custom_inject_edit_count) - 1U);
    strncpy(
        app->custom_inject_edit_interval,
        "interval_ms=0",
        sizeof(app->custom_inject_edit_interval) - 1U);
    strncpy(app->custom_inject_edit_bit, "bit=0 value=1", sizeof(app->custom_inject_edit_bit) - 1U);
    strncpy(
        app->custom_inject_edit_field,
        "start=0 len=1 value=0",
        sizeof(app->custom_inject_edit_field) - 1U);
    strncpy(app->custom_inject_edit_bytes, "0", sizeof(app->custom_inject_edit_bytes) - 1U);
    strncpy(
        app->custom_inject_edit_value_hex,
        "0000000000000000",
        sizeof(app->custom_inject_edit_value_hex) - 1U);
    strncpy(app->custom_inject_edit_mux, "mux=0", sizeof(app->custom_inject_edit_mux) - 1U);
    strncpy(
        app->custom_inject_edit_mux_start,
        "mux_start=0",
        sizeof(app->custom_inject_edit_mux_start) - 1U);
    strncpy(
        app->custom_inject_edit_mux_len,
        "mux_len=1",
        sizeof(app->custom_inject_edit_mux_len) - 1U);
    strncpy(
        app->custom_inject_edit_mux_value,
        "mux_value=0",
        sizeof(app->custom_inject_edit_mux_value) - 1U);
    strncpy(app->custom_inject_set_name, "inj_profile", sizeof(app->custom_inject_set_name) - 1U);

    strncpy(app->args_tool_config, "show=both", sizeof(app->args_tool_config) - 1U);

    strncpy(
        app->args_bus_cfg_can0,
        "bitrate=500000 mode=normal",
        sizeof(app->args_bus_cfg_can0) - 1U);
    strncpy(
        app->args_bus_cfg_can1,
        "bitrate=500000 mode=normal",
        sizeof(app->args_bus_cfg_can1) - 1U);

    strncpy(
        app->args_filter_can0,
        "mask=7FF filter=0 ext_match=0 ext=0",
        sizeof(app->args_filter_can0) - 1U);
    strncpy(
        app->args_filter_can1,
        "mask=7FF filter=0 ext_match=0 ext=0",
        sizeof(app->args_filter_can1) - 1U);

    strncpy(
        app->args_dbc_add,
        "bus=can0 id=1D5 ext=0 start=0 len=16 order=intel sign=u factor=0.1 offset=0 min=0 max=6553.5 unit=kph sid=1 name=Speed",
        sizeof(app->args_dbc_add) - 1U);

    strncpy(app->args_dbc_remove, "sid=1", sizeof(app->args_dbc_remove) - 1U);
    strncpy(
        app->args_wifi_cfg,
        "ap=1 ssid=CANCommander pass=cancommander",
        sizeof(app->args_wifi_cfg) - 1U);
    app_copy_string(app->dbc_config_name, sizeof(app->dbc_config_name), "dbc_profile");
    app_copy_string(app->dbc_config_save_name, sizeof(app->dbc_config_save_name), "dbc_profile");
    app_dbc_config_reset(app);
}

void app_custom_inject_load(App* app) {
    if(!app) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    FuriString* value = furi_string_alloc();

    if(!ff || !file_type || !value) {
        if(value) {
            furi_string_free(value);
        }
        if(file_type) {
            furi_string_free(file_type);
        }
        if(ff) {
            flipper_format_free(ff);
        }
        furi_record_close(RECORD_STORAGE);
        return;
    }

    do {
        if(!flipper_format_file_open_existing(ff, APP_CUSTOM_INJECT_CFG_PATH)) {
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, file_type, &version)) {
            break;
        }

        if(!furi_string_equal_str(file_type, APP_CUSTOM_INJECT_CFG_TYPE) ||
           version != APP_CUSTOM_INJECT_CFG_VER) {
            break;
        }

        if(flipper_format_read_string(ff, "start_args", value)) {
            app_copy_string(
                app->args_custom_inject_start,
                sizeof(app->args_custom_inject_start),
                furi_string_get_cstr(value));
        }

        for(uint8_t i = 0; i < 5U; i++) {
            char key[24] = {0};
            snprintf(key, sizeof(key), "slot_%u_args", (unsigned)(i + 1U));
            if(flipper_format_read_string(ff, key, value)) {
                app_copy_string(
                    app->args_custom_inject_slots[i],
                    sizeof(app->args_custom_inject_slots[i]),
                    furi_string_get_cstr(value));
            }
        }

        uint32_t active_slot = 0;
        if(flipper_format_read_uint32(ff, "active_slot", &active_slot, 1) && active_slot < 5U) {
            app->custom_inject_active_slot = (uint8_t)active_slot;
        }
    } while(false);

    furi_string_free(value);
    furi_string_free(file_type);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void app_custom_inject_save(App* app) {
    if(!app) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    if(!ff) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    char data_dir[96] = {0};
    app_path_parent_dir(APP_CUSTOM_INJECT_CFG_PATH, data_dir, sizeof(data_dir));
    if(!app_storage_ensure_dir(storage, data_dir)) {
        furi_record_close(RECORD_STORAGE);
        flipper_format_free(ff);
        return;
    }

    do {
        if(!flipper_format_file_open_always(ff, APP_CUSTOM_INJECT_CFG_PATH)) {
            break;
        }

        if(!flipper_format_write_header_cstr(ff, APP_CUSTOM_INJECT_CFG_TYPE, APP_CUSTOM_INJECT_CFG_VER)) {
            break;
        }

        if(!flipper_format_write_string_cstr(ff, "start_args", app->args_custom_inject_start)) {
            break;
        }

        bool slots_ok = true;
        for(uint8_t i = 0; i < 5U; i++) {
            char key[24] = {0};
            snprintf(key, sizeof(key), "slot_%u_args", (unsigned)(i + 1U));
            if(!flipper_format_write_string_cstr(ff, key, app->args_custom_inject_slots[i])) {
                slots_ok = false;
                break;
            }
        }

        if(!slots_ok) {
            break;
        }

        uint32_t active_slot = app_custom_inject_get_active_slot(app);
        if(!flipper_format_write_uint32(ff, "active_slot", &active_slot, 1)) {
            break;
        }
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    if(!app) {
        return NULL;
    }

    memset(app, 0, sizeof(App));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->mutex) {
        goto fail;
    }

    app->monitor_text = furi_string_alloc();
    if(!app->monitor_text) {
        goto fail;
    }

    app->status_text = furi_string_alloc();
    if(!app->status_text) {
        goto fail;
    }

    app->client = cc_client_alloc();
    if(!app->client) {
        goto fail;
    }

    app->scene_manager = scene_manager_alloc(&cancommander_scene_handlers, app);
    if(!app->scene_manager) {
        goto fail;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        goto fail;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, cancommander_scene_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, cancommander_scene_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, cancommander_scene_tick_callback, APP_UI_TICK_INTERVAL_MS);

    app->submenu = submenu_alloc();
    if(!app->submenu) {
        goto fail;
    }

    view_dispatcher_add_view(app->view_dispatcher, AppViewSubmenu, submenu_get_view(app->submenu));

    app->text_box = text_box_alloc();
    if(!app->text_box) {
        goto fail;
    }

    view_dispatcher_add_view(app->view_dispatcher, AppViewTextBox, text_box_get_view(app->text_box));

    app->text_input = text_input_alloc();
    if(!app->text_input) {
        goto fail;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, AppViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    if(!app->byte_input) {
        goto fail;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, AppViewByteInput, byte_input_get_view(app->byte_input));

    app->var_list = variable_item_list_alloc();
    if(!app->var_list) {
        goto fail;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, AppViewVarList, variable_item_list_get_view(app->var_list));

    app->widget = widget_alloc();
    if(!app->widget) {
        goto fail;
    }

    view_dispatcher_add_view(app->view_dispatcher, AppViewWidget, widget_get_view(app->widget));

    app->dashboard_view = view_alloc();
    if(!app->dashboard_view) {
        goto fail;
    }

    view_set_context(app->dashboard_view, app);
    view_allocate_model(app->dashboard_view, ViewModelTypeLocking, dashboard_model_size());
    view_set_draw_callback(app->dashboard_view, dashboard_view_draw);
    view_set_input_callback(app->dashboard_view, dashboard_view_input);
    view_dispatcher_add_view(app->view_dispatcher, AppViewDashboard, app->dashboard_view);
    dashboard_set_mode(app, AppDashboardNone);

    app_set_default_args(app);
    app_ensure_startup_dirs();
    app_custom_inject_load(app);
    app_set_status(app, "Ready. Connect on first action");

    app->rx_worker =
        furi_thread_alloc_ex("CANCmdRx", APP_RX_THREAD_STACK, app_rx_worker_thread, app);
    if(!app->rx_worker) {
        goto fail;
    }

    furi_thread_set_priority(app->rx_worker, FuriThreadPriorityNormal);
    furi_thread_start(app->rx_worker);
    app->app_ready = true;

    return app;

fail:
    app_free(app);
    return NULL;
}

static void app_free(App* app) {
    if(!app) {
        return;
    }

    if(app->app_ready && app->scene_manager && app->view_dispatcher) {
        app_custom_inject_save(app);
    }

    app->rx_worker_stop = true;
    if(app->rx_worker) {
        furi_thread_join(app->rx_worker);
        furi_thread_free(app->rx_worker);
    }

    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, AppViewSubmenu);
        view_dispatcher_remove_view(app->view_dispatcher, AppViewTextBox);
        view_dispatcher_remove_view(app->view_dispatcher, AppViewTextInput);
        view_dispatcher_remove_view(app->view_dispatcher, AppViewByteInput);
        view_dispatcher_remove_view(app->view_dispatcher, AppViewVarList);
        view_dispatcher_remove_view(app->view_dispatcher, AppViewWidget);
        view_dispatcher_remove_view(app->view_dispatcher, AppViewDashboard);
    }

    if(app->submenu) {
        submenu_free(app->submenu);
    }
    if(app->text_box) {
        text_box_free(app->text_box);
    }
    if(app->text_input) {
        text_input_free(app->text_input);
    }
    if(app->byte_input) {
        byte_input_free(app->byte_input);
    }
    if(app->var_list) {
        variable_item_list_free(app->var_list);
    }
    if(app->widget) {
        widget_free(app->widget);
    }
    if(app->dashboard_view) {
        view_free(app->dashboard_view);
    }

    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
    }
    if(app->scene_manager) {
        scene_manager_free(app->scene_manager);
    }

    if(app->client) {
        cc_client_free(app->client);
    }

    if(app->dbc_config_signals) {
        free(app->dbc_config_signals);
        app->dbc_config_signals = NULL;
    }

    if(app->monitor_text) {
        furi_string_free(app->monitor_text);
    }
    if(app->status_text) {
        furi_string_free(app->status_text);
    }

    if(app->mutex) {
        furi_mutex_free(app->mutex);
    }

    free(app);
}

int32_t cancommander_main(void* p) {
    UNUSED(p);

    // Auto-enable 5V OTG for CAN Commander power.
    const bool otg_was_enabled = furi_hal_power_is_otg_enabled();
    uint8_t otg_attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && otg_attempts++ < 5U) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }

    App* app = app_alloc();
    if(!app) {
        if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
            furi_hal_power_disable_otg();
        }
        return 255;
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    /* Give the ESP32 time to finish booting after OTG power was enabled,
     * then open a clean UART and run the firmware version check once so
     * tools don't hit a cold handshake on first use. */
    furi_delay_ms(400);
    if(app_connect(app, true)) {
        app_verify_firmware_version(app);
    }

    scene_manager_next_scene(app->scene_manager, cancommander_scene_main_menu);

    /* If the boot-time version check found an out-of-date firmware, surface
     * the warning immediately by pushing the status scene on top. Back from
     * the status scene returns the user to the main menu. */
    if(app->fw_version_warn_pending) {
        app->fw_version_warn_shown = true;
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
    }
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    app_free(app);

    // Restore OTG state if this app enabled it.
    if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }

    return 0;
}
