#include "settings.h"
#include <flipper_format/flipper_format.h>

#define SETTINGS_FILETYPE "Flipper ENC28J60 Ethernet App Settings"
// F0.5f — bumped from 1 to 2 to add gateway/mac_gateway/is_dora.
// load() of older v1 files still works (extra fields are just absent).
#define SETTINGS_VERSION  2
#define SETTINGS_PATH     PATHAPPEXT "/settings.cfg"

void settings_load(App* app) {
    furi_assert(app);
    furi_assert(app->storage);
    furi_assert(app->ethernet);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t u32_tmp = 0;
    bool flag = false;
    uint8_t buf6[6];
    uint8_t buf4[4];

    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_PATH)) break;
        if(!flipper_format_read_header(ff, file_type, &version)) break;
        // F0.5f — accept v1 (pre-gateway-persist) and v2 to avoid
        // resetting users' persisted MAC/IP just because the schema grew.
        if(version != 1 && version != 2) break;
        if(furi_string_cmp_str(file_type, SETTINGS_FILETYPE) != 0) break;

        // MAC — also push to chip registers if present. enc28j60_set_mac
        // reads from instance->mac_address, so update that first.
        if(flipper_format_read_hex(ff, "mac_address", buf6, 6)) {
            memcpy(app->ethernet->mac_address, buf6, 6);
            enc28j60_set_mac(app->ethernet);
        }

        // Static IPv4 (used only when is_static_ip == true).
        if(flipper_format_read_hex(ff, "ip_address", buf4, 4)) {
            memcpy(app->ethernet->ip_address, buf4, 4);
            memcpy(app->ip_helper, buf4, 4);
        }

        // is_static_ip flag.
        if(flipper_format_read_bool(ff, "is_static_ip", &flag, 1)) {
            app->is_static_ip = flag;
        }

        // F0.5f — gateway / mac_gateway / is_dora. These were missing in
        // v1 of the schema. Without them, a user with a static-IP setup
        // had `is_static_ip=true` (auto-replies on) but `is_dora=false`
        // (Ping/Ports/OS/ArpSpoof refused to start), an inconsistent
        // half-restored state.
        if(flipper_format_read_bool(ff, "is_dora", &flag, 1)) {
            app->is_dora = flag;
        }
        flipper_format_read_hex(ff, "ip_gateway", app->ip_gateway, 4);
        flipper_format_read_hex(ff, "mac_gateway", app->mac_gateway, 6);

        // scan_params block (F0.1).
        flipper_format_read_hex(ff, "scan_target_ip", app->scan_params.target_ip, 4);

        if(flipper_format_read_uint32(ff, "scan_target_port", &u32_tmp, 1)) {
            app->scan_params.target_port = (uint16_t)u32_tmp;
        }
        if(flipper_format_read_uint32(ff, "scan_range_port", &u32_tmp, 1)) {
            app->scan_params.range_port = (uint16_t)u32_tmp;
        }
        if(flipper_format_read_uint32(ff, "scan_protocols_index", &u32_tmp, 1)) {
            app->scan_params.protocols_index = (uint8_t)u32_tmp;
        }

        flipper_format_read_hex(ff, "scan_ip_ping", app->scan_params.ip_ping, 4);
        flipper_format_read_hex(ff, "scan_ip_start", app->scan_params.ip_start, 4);

        if(flipper_format_read_uint32(ff, "scan_range_ip", &u32_tmp, 1)) {
            app->scan_params.range_ip = (uint8_t)u32_tmp;
        }
    } while(false);

    furi_string_free(file_type);
    flipper_format_free(ff);
}

void settings_save(App* app) {
    furi_assert(app);
    furi_assert(app->storage);
    furi_assert(app->ethernet);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    uint32_t u32_tmp = 0;

    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;

        if(!flipper_format_write_hex(ff, "mac_address", app->ethernet->mac_address, 6)) break;
        if(!flipper_format_write_hex(ff, "ip_address", app->ethernet->ip_address, 4)) break;
        if(!flipper_format_write_bool(ff, "is_static_ip", &app->is_static_ip, 1)) break;

        // F0.5f — paired writes for the new v2 fields (see load() for the
        // motivation: was producing a half-restored static-IP state).
        if(!flipper_format_write_bool(ff, "is_dora", &app->is_dora, 1)) break;
        if(!flipper_format_write_hex(ff, "ip_gateway", app->ip_gateway, 4)) break;
        if(!flipper_format_write_hex(ff, "mac_gateway", app->mac_gateway, 6)) break;

        if(!flipper_format_write_hex(ff, "scan_target_ip", app->scan_params.target_ip, 4)) break;

        u32_tmp = app->scan_params.target_port;
        if(!flipper_format_write_uint32(ff, "scan_target_port", &u32_tmp, 1)) break;
        u32_tmp = app->scan_params.range_port;
        if(!flipper_format_write_uint32(ff, "scan_range_port", &u32_tmp, 1)) break;
        u32_tmp = app->scan_params.protocols_index;
        if(!flipper_format_write_uint32(ff, "scan_protocols_index", &u32_tmp, 1)) break;

        if(!flipper_format_write_hex(ff, "scan_ip_ping", app->scan_params.ip_ping, 4)) break;
        if(!flipper_format_write_hex(ff, "scan_ip_start", app->scan_params.ip_start, 4)) break;

        u32_tmp = app->scan_params.range_ip;
        if(!flipper_format_write_uint32(ff, "scan_range_ip", &u32_tmp, 1)) break;
    } while(false);

    flipper_format_free(ff);
}
