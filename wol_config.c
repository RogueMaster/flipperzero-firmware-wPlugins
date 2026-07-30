#include "wol_config.h"

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "WolConfig"

#define WOL_CONFIG_DIR  EXT_PATH("apps_data/wol_flipper")
#define WOL_CONFIG_PATH WOL_CONFIG_DIR "/wol.cfg"
#define WOL_FILE_TYPE   "WoL Flipper config"
#define WOL_FILE_VER    1

void wol_strcpy(char* dst, size_t dst_len, const char* src) {
    snprintf(dst, dst_len, "%s", src);
}

void wol_target_default(WolTarget* target) {
    memset(target, 0, sizeof(WolTarget));
    wol_strcpy(target->ip, WOL_IP_LEN, "255.255.255.255");
    target->port = 9;
}

void wol_mac_to_str(const uint8_t* mac, char* out, size_t out_len) {
    snprintf(
        out,
        out_len,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void wol_build_magic_packet(const uint8_t* mac, uint8_t* out) {
    memset(out, 0xFF, 6);
    for(size_t i = 0; i < 16; i++) {
        memcpy(out + 6 + i * 6, mac, 6);
    }
}

bool wol_ip_is_valid(const char* ip) {
    size_t octets = 0;
    const char* p = ip;

    while(octets < 4) {
        char* end = NULL;
        long v = strtol(p, &end, 10);
        if(end == p || v < 0 || v > 255) return false;
        octets++;
        p = end;
        if(octets < 4) {
            if(*p != '.') return false;
            p++;
        }
    }
    return *p == '\0';
}

/** Parse "AA:BB:CC:DD:EE:FF 255.255.255.255 9 Name with spaces". */
static bool wol_target_parse(const char* line, WolTarget* target) {
    wol_target_default(target);

    for(size_t i = 0; i < 6; i++) {
        char* end = NULL;
        long v = strtol(line, &end, 16);
        if(end != line + 2 || v < 0 || v > 0xFF) return false;
        target->mac[i] = (uint8_t)v;
        line = end;
        if(i < 5) {
            if(*line != ':' && *line != '-') return false;
            line++;
        }
    }

    while(*line == ' ')
        line++;
    size_t n = 0;
    while(*line && *line != ' ' && n < WOL_IP_LEN - 1) {
        target->ip[n++] = *line++;
    }
    target->ip[n] = '\0';
    if(!wol_ip_is_valid(target->ip)) return false;

    while(*line == ' ')
        line++;
    char* end = NULL;
    long port = strtol(line, &end, 10);
    if(end == line || port < 0 || port > 65535) return false;
    target->port = (uint16_t)port;
    line = end;

    while(*line == ' ')
        line++;
    wol_strcpy(target->name, WOL_NAME_LEN, line);
    if(target->name[0] == '\0') {
        wol_mac_to_str(target->mac, target->name, WOL_NAME_LEN);
    }
    return true;
}

/** Parse "ssid\tpassword". An empty password is valid, an empty SSID is not. */
static bool wol_network_parse(const char* line, WolNetwork* network) {
    memset(network, 0, sizeof(WolNetwork));

    const char* tab = strchr(line, '\t');
    size_t ssid_len = tab ? (size_t)(tab - line) : strlen(line);
    if(ssid_len == 0) return false;
    if(ssid_len >= WOL_SSID_LEN) ssid_len = WOL_SSID_LEN - 1;

    memcpy(network->ssid, line, ssid_len);
    network->ssid[ssid_len] = '\0';
    if(tab) wol_strcpy(network->pass, WOL_PASS_LEN, tab + 1);
    return true;
}

uint8_t wol_config_find_network(const WolConfig* cfg, const char* ssid) {
    furi_check(cfg && ssid);

    for(uint8_t i = 0; i < cfg->network_count; i++) {
        if(strcmp(cfg->networks[i].ssid, ssid) == 0) return i;
    }
    return WOL_MAX_NETWORKS;
}

void wol_config_load(WolConfig* cfg) {
    furi_check(cfg);

    memset(cfg, 0, sizeof(WolConfig));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* buf = furi_string_alloc();
    FuriString* key = furi_string_alloc();
    uint32_t version = 0;

    do {
        if(!flipper_format_file_open_existing(file, WOL_CONFIG_PATH)) break;
        if(!flipper_format_read_header(file, buf, &version)) break;
        if(furi_string_cmp_str(buf, WOL_FILE_TYPE) != 0 || version != WOL_FILE_VER) {
            FURI_LOG_W(TAG, "unsupported config file");
            break;
        }

        /* Files written before multiple networks existed carry a single SSID
         * and Password pair. Read it in as the first network so nobody has to
         * type their key again after an update. */
        if(flipper_format_read_string(file, "SSID", buf) && furi_string_size(buf) > 0) {
            wol_strcpy(cfg->networks[0].ssid, WOL_SSID_LEN, furi_string_get_cstr(buf));
            if(flipper_format_read_string(file, "Password", buf)) {
                wol_strcpy(cfg->networks[0].pass, WOL_PASS_LEN, furi_string_get_cstr(buf));
            }
            cfg->network_count = 1;
        }

        flipper_format_rewind(file);
        for(size_t i = 0; i < WOL_MAX_NETWORKS; i++) {
            furi_string_printf(key, "Network%u", (unsigned)i);
            if(!flipper_format_read_string(file, furi_string_get_cstr(key), buf)) break;

            WolNetwork network;
            if(!wol_network_parse(furi_string_get_cstr(buf), &network)) continue;
            if(i == 0) cfg->network_count = 0; // the new keys win over the legacy pair
            cfg->networks[cfg->network_count++] = network;
        }

        flipper_format_rewind(file);
        for(size_t i = 0; i < WOL_MAX_TARGETS; i++) {
            furi_string_printf(key, "Target%u", (unsigned)i);
            if(!flipper_format_read_string(file, furi_string_get_cstr(key), buf)) break;

            WolTarget target;
            if(!wol_target_parse(furi_string_get_cstr(buf), &target)) {
                FURI_LOG_W(TAG, "bad target line %u", (unsigned)i);
                continue;
            }
            cfg->targets[cfg->target_count++] = target;
        }
    } while(false);

    furi_string_free(key);
    furi_string_free(buf);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool wol_config_save(const WolConfig* cfg) {
    furi_check(cfg);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* buf = furi_string_alloc();
    FuriString* key = furi_string_alloc();
    char mac[18];
    bool ok = false;

    storage_common_mkdir(storage, WOL_CONFIG_DIR);

    do {
        if(!flipper_format_file_open_always(file, WOL_CONFIG_PATH)) break;
        if(!flipper_format_write_header_cstr(file, WOL_FILE_TYPE, WOL_FILE_VER)) break;
        ok = true;
        for(size_t i = 0; i < cfg->network_count; i++) {
            furi_string_printf(key, "Network%u", (unsigned)i);
            // tab separated: both an SSID and a key may contain spaces
            furi_string_printf(buf, "%s\t%s", cfg->networks[i].ssid, cfg->networks[i].pass);
            if(!flipper_format_write_string(file, furi_string_get_cstr(key), buf)) {
                ok = false;
                break;
            }
        }
        if(!ok) break;

        for(size_t i = 0; i < cfg->target_count; i++) {
            const WolTarget* target = &cfg->targets[i];
            wol_mac_to_str(target->mac, mac, sizeof(mac));
            furi_string_printf(key, "Target%u", (unsigned)i);
            furi_string_printf(buf, "%s %s %u %s", mac, target->ip, target->port, target->name);
            if(!flipper_format_write_string(file, furi_string_get_cstr(key), buf)) {
                ok = false;
                break;
            }
        }
    } while(false);

    furi_string_free(key);
    furi_string_free(buf);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!ok) FURI_LOG_E(TAG, "config save failed");
    return ok;
}
