#include "led_remote_storage.h"
#include "led_remote_ir.h"
#include <flipper_format/flipper_format.h>

#define IR_FILE_HEADER  "IR signals file"
#define IR_FILE_VERSION 1
#define IR_TYPE_PARSED  "parsed"
#define IR_TYPE_RAW     "raw"

void led_remote_storage_save(LedRemoteApp* app) {
    storage_simply_mkdir(app->storage, LED_REMOTE_DATA_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);

    do {
        if(!flipper_format_file_open_always(ff, app->save_path)) break;
        if(!flipper_format_write_header_cstr(ff, IR_FILE_HEADER, IR_FILE_VERSION)) break;

        for(size_t i = 0; i < app->button_count; i++) {
            LedRemoteSignal* sig = &app->signals[i];
            if(!sig->is_valid) continue;

            flipper_format_write_string_cstr(ff, "name", app->button_names[i]);

            if(!sig->is_raw) {
                const char* proto_name = infrared_get_protocol_name(sig->decoded.protocol);
                uint8_t addr[4] = {
                    sig->decoded.address & 0xFF,
                    (sig->decoded.address >> 8) & 0xFF,
                    (sig->decoded.address >> 16) & 0xFF,
                    (sig->decoded.address >> 24) & 0xFF,
                };
                uint8_t cmd[4] = {
                    sig->decoded.command & 0xFF,
                    (sig->decoded.command >> 8) & 0xFF,
                    (sig->decoded.command >> 16) & 0xFF,
                    (sig->decoded.command >> 24) & 0xFF,
                };
                flipper_format_write_string_cstr(ff, "type", IR_TYPE_PARSED);
                flipper_format_write_string_cstr(ff, "protocol", proto_name);
                flipper_format_write_hex(ff, "address", addr, 4);
                flipper_format_write_hex(ff, "command", cmd, 4);
            } else {
                flipper_format_write_string_cstr(ff, "type", IR_TYPE_RAW);
                flipper_format_write_uint32(ff, "frequency", &sig->raw_frequency, 1);
                flipper_format_write_float(ff, "duty_cycle", &sig->raw_duty_cycle, 1);
                flipper_format_write_uint32(
                    ff, "data", sig->raw_timings, sig->raw_timings_size);
            }
        }
    } while(false);

    flipper_format_free(ff);
}

#define SETTINGS_PATH     LED_REMOTE_DATA_DIR "/settings.dat"
#define SETTINGS_HEADER   "LED Remote Settings"
#define SETTINGS_VERSION  1

void led_remote_settings_save(LedRemoteApp* app) {
    storage_simply_mkdir(app->storage, LED_REMOTE_DATA_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_HEADER, SETTINGS_VERSION)) break;
        flipper_format_write_string_cstr(ff, "profile", app->profile_name);
        uint32_t type = (uint32_t)app->remote_type;
        flipper_format_write_uint32(ff, "type", &type, 1);
    } while(false);
    flipper_format_free(ff);
}

void led_remote_settings_load(LedRemoteApp* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_PATH)) break;

        FuriString* hdr = furi_string_alloc();
        uint32_t version = 0;
        bool ok = flipper_format_read_header(ff, hdr, &version);
        furi_string_free(hdr);
        if(!ok) break;

        FuriString* profile = furi_string_alloc();
        if(flipper_format_read_string(ff, "profile", profile)) {
            strncpy(
                app->profile_name,
                furi_string_get_cstr(profile),
                sizeof(app->profile_name) - 1);
            app->profile_name[sizeof(app->profile_name) - 1] = '\0';
        }
        furi_string_free(profile);

        uint32_t type = 0;
        if(flipper_format_read_uint32(ff, "type", &type, 1)) {
            if(type < LedRemoteTypeCount) {
                app->remote_type = (LedRemoteType)type;
            }
        }
    } while(false);
    flipper_format_free(ff);
}

void led_remote_storage_load(LedRemoteApp* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);

    do {
        if(!flipper_format_file_open_existing(ff, app->save_path)) break;

        FuriString* header = furi_string_alloc();
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, header, &version)) {
            furi_string_free(header);
            break;
        }
        furi_string_free(header);

        FuriString* name  = furi_string_alloc();
        FuriString* type  = furi_string_alloc();
        FuriString* proto = furi_string_alloc();

        while(flipper_format_read_string(ff, "name", name)) {
            if(!flipper_format_read_string(ff, "type", type)) break;

            // Find the button matching the name in the active type
            int btn = -1;
            for(size_t i = 0; i < app->button_count; i++) {
                if(furi_string_cmp_str(name, app->button_names[i]) == 0) {
                    btn = (int)i;
                    break;
                }
            }

            if(furi_string_cmp_str(type, IR_TYPE_PARSED) == 0) {
                if(!flipper_format_read_string(ff, "protocol", proto)) break;
                uint8_t addr[4] = {0}, cmd[4] = {0};
                if(!flipper_format_read_hex(ff, "address", addr, 4)) break;
                if(!flipper_format_read_hex(ff, "command", cmd, 4)) break;

                if(btn >= 0) {
                    LedRemoteSignal* sig = &app->signals[btn];
                    sig->decoded.protocol = infrared_get_protocol_by_name(
                        furi_string_get_cstr(proto));
                    sig->decoded.address =
                        addr[0] | ((uint32_t)addr[1] << 8) |
                        ((uint32_t)addr[2] << 16) | ((uint32_t)addr[3] << 24);
                    sig->decoded.command =
                        cmd[0] | ((uint32_t)cmd[1] << 8) |
                        ((uint32_t)cmd[2] << 16) | ((uint32_t)cmd[3] << 24);
                    sig->decoded.repeat = false;
                    sig->is_raw   = false;
                    sig->is_valid = true;
                    app->learned[btn] = true;
                }
            } else if(furi_string_cmp_str(type, IR_TYPE_RAW) == 0) {
                uint32_t frequency = 0;
                float duty_cycle = 0.0f;
                if(!flipper_format_read_uint32(ff, "frequency", &frequency, 1)) break;
                if(!flipper_format_read_float(ff, "duty_cycle", &duty_cycle, 1)) break;

                uint32_t count = 0;
                flipper_format_get_value_count(ff, "data", &count);
                if(count == 0) break;

                uint32_t* timings = malloc(count * sizeof(uint32_t));
                if(!flipper_format_read_uint32(ff, "data", timings, count)) {
                    free(timings);
                    break;
                }

                if(btn >= 0) {
                    LedRemoteSignal* sig = &app->signals[btn];
                    led_remote_ir_signal_clear(sig);
                    sig->raw_timings      = timings;
                    sig->raw_timings_size = count;
                    sig->raw_frequency    = frequency;
                    sig->raw_duty_cycle   = duty_cycle;
                    sig->is_raw   = true;
                    sig->is_valid = true;
                    app->learned[btn] = true;
                } else {
                    free(timings);
                }
            }
        }

        furi_string_free(name);
        furi_string_free(type);
        furi_string_free(proto);
    } while(false);

    flipper_format_free(ff);
}
