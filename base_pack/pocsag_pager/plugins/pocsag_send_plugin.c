#include "pocsag_send_plugin.h"
#include "pocsag_encoder.h"
#include "pocsag_pager_text_input.h"
#include "pocsag_pager_tx.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_region.h>
#include <lib/drivers/cc1101_regs.h>
#include <flipper_application/flipper_application.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/modules/number_input.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>
#include <stdio.h>
#include <string.h>

#define PCSG_PRESET_NAME_MAX      32U
#define PCSG_PRESETS_MAX_COUNT    32U
#define PCSG_PRESET_FILE_TYPE     "Flipper POCSAG Preset"
#define PCSG_PRESET_FILE_VERSION  1U
#define PCSG_PRESETS_DIR          APP_DATA_PATH("presets")

#define PRESET_ACTION_SAVE       1000U
#define PRESET_ACTION_TOGGLE_DEL 1001U

// 2-FSK Async TX preset for POCSAG: +-4.761 kHz deviation (DEVIATN=0x14, CCIR 584 +-4.5 kHz +-10%)
// High DRATE (~100 kBaud) gives 8x = 800 kHz (1.25 us) GDO0 async sampling for crisp 2400-baud edges.
static const uint8_t pcsg_tx_preset_2fsk_4k5_async[] = {
    CC1101_IOCFG0,   0x0D,
    CC1101_FSCTRL1,  0x06,
    CC1101_PKTCTRL0, 0x32,
    CC1101_PKTCTRL1, 0x04,
    CC1101_MDMCFG0,  0x00,
    CC1101_MDMCFG1,  0x02,
    CC1101_MDMCFG2,  0x00,
    CC1101_MDMCFG3,  0xF8,
    CC1101_MDMCFG4,  0x6B,
    CC1101_DEVIATN,  0x14,
    CC1101_MCSM0,    0x18,
    CC1101_FOCCFG,   0x16,
    CC1101_AGCCTRL0, 0x91,
    CC1101_AGCCTRL1, 0x00,
    CC1101_AGCCTRL2, 0x07,
    CC1101_WORCTRL,  0xFB,
    CC1101_FREND0,   0x10,
    CC1101_FREND1,   0x56,
    0x00,            0x00,
    0xC0,            0x00,
    0x00,            0x00,
    0x00,            0x00,
    0x00,            0x00,
};

static const NotificationSequence pcsg_sequence_tx_done = {
    &message_green_255,
    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_delay_100,
    NULL,
};

typedef enum {
    PluginViewVarList = 0,
    PluginViewSubmenu = 1,
    PluginViewTextInput = 2,
    PluginViewNumberInput = 3,
    PluginViewTx = 4,
    PluginViewCount = 5,
} PluginViewOffset;

typedef enum {
    PluginSceneSend = 0,
    PluginSceneInput = 1,
    PluginScenePresets = 2,
    PluginSceneTx = 3,
    PluginSceneCount = 4,
} PluginSceneId;

typedef enum {
    PluginInputFreq,
    PluginInputRic,
    PluginInputMessage,
    PluginInputPresetName,
} PluginInputMode;

typedef enum {
    PluginEvtStart = 2000,
    PluginEvtEditFreq,
    PluginEvtEditRic,
    PluginEvtEditMessage,
    PluginEvtOpenPresets,
    PluginEvtTransmit,
    PluginEvtTextDone,
    PluginEvtNumberDone,
    PluginEvtTxBack,
    PluginEvtTxResend,
} PluginEvent;

typedef struct {
    PCSGSendHostApi host;
    SceneManager* scene_manager;

    VariableItemList* var_list;
    Submenu* submenu;
    PCSGTextInput* text_input;
    NumberInput* number_input;
    PCSGTxView* tx_view;

    VariableItem* send_msg_item;
    PluginInputMode input_mode;
    char input_buf[POCSAG_MAX_MSG_LEN + 1];

    char preset_names[PCSG_PRESETS_MAX_COUNT][PCSG_PRESET_NAME_MAX];
    size_t preset_count;
    bool preset_delete_mode;

    PocsagEncoder encoder;
    bool tx_active;
    uint32_t tx_start_tick;
    uint32_t tx_duration_ms;
} PCSGSendPlugin;

// ---- Helpers: frequency parsing, formatting, presets on microSD ----

static bool pcsg_parse_frequency(const char* text, uint32_t* out_hz) {
    if(!text || !out_hz) return false;
    while(*text == ' ') text++;
    if(*text < '0' || *text > '9') return false;

    uint64_t int_part = 0;
    while(*text >= '0' && *text <= '9') {
        int_part = int_part * 10ULL + (uint64_t)(*text - '0');
        if(int_part > 2000000000ULL) return false;
        text++;
    }

    bool has_dot = false;
    uint64_t frac_hz = 0;
    if(*text == '.') {
        has_dot = true;
        text++;
        uint64_t scale = 100000ULL;
        while(*text >= '0' && *text <= '9') {
            if(scale > 0) {
                frac_hz += (uint64_t)(*text - '0') * scale;
                scale /= 10ULL;
            }
            text++;
        }
    }

    while(*text == ' ') text++;
    if(*text != '\0') return false;

    uint64_t hz = 0;
    if(has_dot) {
        if(int_part == 0 || int_part > 2000ULL) return false;
        hz = int_part * 1000000ULL + frac_hz;
    } else if(int_part >= 10000000ULL) {
        hz = int_part;
    } else if(int_part >= 100ULL && int_part <= 2000ULL) {
        hz = int_part * 1000000ULL;
    } else {
        return false;
    }

    if(hz > UINT32_MAX) return false;
    *out_hz = (uint32_t)hz;
    return true;
}

static void pcsg_format_frequency(uint32_t freq_hz, char* out_buf, size_t out_size) {
    snprintf(
        out_buf,
        out_size,
        "%lu.%04lu",
        (unsigned long)(freq_hz / 1000000U),
        (unsigned long)((freq_hz % 1000000U) / 100U));
}

static const char* pcsg_msg_type_name(PocsagMsgType type) {
    switch(type) {
    case PocsagMsgTypeNumeric:
        return "Numeric";
    case PocsagMsgTypeTone:
        return "Tone";
    case PocsagMsgTypeAlert2:
        return "Alert2";
    case PocsagMsgTypeAlpha:
    default:
        return "Alphanum";
    }
}

static void pcsg_preset_sanitize_name(const char* in, char* out, size_t out_size) {
    size_t j = 0;
    for(size_t i = 0; in[i] != '\0' && j + 1 < out_size; i++) {
        char c = in[i];
        if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
           c == '>' || c == '|') {
            c = '_';
        }
        out[j++] = c;
    }
    out[j] = '\0';
}

static size_t pcsg_presets_list(char names[][PCSG_PRESET_NAME_MAX], size_t max_count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    storage_simply_mkdir(storage, PCSG_PRESETS_DIR);

    File* dir = storage_file_alloc(storage);
    FileInfo fileinfo;
    char fname[64];
    size_t count = 0;

    if(storage_dir_open(dir, PCSG_PRESETS_DIR)) {
        while(count < max_count && storage_dir_read(dir, &fileinfo, fname, sizeof(fname))) {
            if(file_info_is_dir(&fileinfo)) continue;
            size_t len = strlen(fname);
            if(len <= 4 || strcmp(fname + len - 4, ".txt") != 0) continue;
            fname[len - 4] = '\0';
            strlcpy(names[count], fname, PCSG_PRESET_NAME_MAX);
            count++;
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    for(size_t i = 1; i < count; i++) {
        char key[PCSG_PRESET_NAME_MAX];
        strlcpy(key, names[i], sizeof(key));
        size_t j = i;
        while(j > 0 && strcmp(names[j - 1], key) > 0) {
            strlcpy(names[j], names[j - 1], PCSG_PRESET_NAME_MAX);
            j--;
        }
        strlcpy(names[j], key, PCSG_PRESET_NAME_MAX);
    }

    return count;
}

static bool pcsg_preset_load(const char* name, PCSGSettings* out_data) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc_printf("%s/%s.txt", PCSG_PRESETS_DIR, name);
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;
    bool ok = false;

    if(flipper_format_file_open_existing(ff, furi_string_get_cstr(path))) {
        if(flipper_format_read_header(ff, filetype, &version) &&
           furi_string_cmp_str(filetype, PCSG_PRESET_FILE_TYPE) == 0 &&
           version == PCSG_PRESET_FILE_VERSION) {
            uint32_t tmp = 0;
            if(flipper_format_read_uint32(ff, "Frequency", &tmp, 1) && tmp > 0) {
                out_data->frequency = tmp;
            }
            if(flipper_format_read_uint32(ff, "RIC", &tmp, 1) && tmp <= PCSG_RIC_MAX) {
                out_data->ric = tmp;
            }
            if(flipper_format_read_uint32(ff, "Baud", &tmp, 1)) {
                if(tmp == PocsagBaud512 || tmp == PocsagBaud1200 || tmp == PocsagBaud2400) {
                    out_data->baud = (PocsagBaudRate)tmp;
                }
            }
            if(flipper_format_read_uint32(ff, "MsgType", &tmp, 1) && tmp <= PocsagMsgTypeAlpha) {
                out_data->msg_type = (PocsagMsgType)tmp;
            }
            if(flipper_format_read_uint32(ff, "Inverted", &tmp, 1)) {
                out_data->inverted = (tmp != 0);
            }
            FuriString* msg = furi_string_alloc();
            if(flipper_format_read_string(ff, "Message", msg)) {
                strlcpy(out_data->message, furi_string_get_cstr(msg), sizeof(out_data->message));
            }
            furi_string_free(msg);
            ok = true;
        }
    }

    furi_string_free(filetype);
    furi_string_free(path);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool pcsg_preset_save(const char* name, const PCSGSettings* data) {
    char safe_name[PCSG_PRESET_NAME_MAX];
    pcsg_preset_sanitize_name(name, safe_name, sizeof(safe_name));
    if(safe_name[0] == '\0') return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    storage_simply_mkdir(storage, PCSG_PRESETS_DIR);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc_printf("%s/%s.txt", PCSG_PRESETS_DIR, safe_name);
    bool ok = false;

    if(flipper_format_file_open_always(ff, furi_string_get_cstr(path))) {
        if(flipper_format_write_header_cstr(
               ff, PCSG_PRESET_FILE_TYPE, PCSG_PRESET_FILE_VERSION)) {
            uint32_t freq = data->frequency;
            uint32_t ric = data->ric;
            uint32_t baud = (uint32_t)data->baud;
            uint32_t msg_type = (uint32_t)data->msg_type;
            uint32_t inv = data->inverted ? 1U : 0U;

            ok = flipper_format_write_uint32(ff, "Frequency", &freq, 1) &&
                 flipper_format_write_uint32(ff, "RIC", &ric, 1) &&
                 flipper_format_write_uint32(ff, "Baud", &baud, 1) &&
                 flipper_format_write_uint32(ff, "MsgType", &msg_type, 1) &&
                 flipper_format_write_uint32(ff, "Inverted", &inv, 1) &&
                 flipper_format_write_string_cstr(ff, "Message", data->message);
        }
    }

    furi_string_free(path);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool pcsg_preset_delete(const char* name) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc_printf("%s/%s.txt", PCSG_PRESETS_DIR, name);
    bool ok = storage_simply_remove(storage, furi_string_get_cstr(path));
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// ---- Radio TX control + OFW/Unleashed region check ----

static bool plugin_is_tx_allowed(PCSGSendPlugin* p, uint32_t frequency) {
    if(!subghz_devices_is_frequency_valid(p->host.radio_device, frequency)) {
        return false;
    }
    if(!furi_hal_region_is_frequency_allowed(frequency)) {
        return false;
    }
#ifndef FW_ORIGIN_Official
    if(!p->host.is_external_radio && !furi_hal_subghz_is_tx_allowed(frequency)) {
        return false;
    }
#endif
    return true;
}

static void plugin_tx_stop(PCSGSendPlugin* p) {
    if(!p->tx_active) return;
    subghz_devices_stop_async_tx(p->host.radio_device);
    subghz_devices_idle(p->host.radio_device);
    p->tx_active = false;
}

static bool plugin_tx_start(PCSGSendPlugin* p) {
    plugin_tx_stop(p);

    PCSGSettings* s = p->host.settings;
    if(!plugin_is_tx_allowed(p, s->frequency)) return false;

    if(!pocsag_encoder_init(
           &p->encoder, s->ric, s->msg_type, s->message, s->baud, s->inverted)) {
        return false;
    }

    subghz_devices_reset(p->host.radio_device);
    subghz_devices_idle(p->host.radio_device);
    subghz_devices_load_preset(
        p->host.radio_device, FuriHalSubGhzPresetCustom, (uint8_t*)pcsg_tx_preset_2fsk_4k5_async);
    subghz_devices_set_frequency(p->host.radio_device, s->frequency);

    if(!subghz_devices_start_async_tx(p->host.radio_device, pocsag_encoder_yield, &p->encoder)) {
        subghz_devices_idle(p->host.radio_device);
        return false;
    }

    p->tx_start_tick = furi_get_tick();
    p->tx_duration_ms = pocsag_encoder_get_duration_ms(&p->encoder);
    p->tx_active = true;
    return true;
}

// ---- Scene 0: Send Menu ----

enum SendMenuIdx {
    SendMenuFreq,
    SendMenuRic,
    SendMenuBaud,
    SendMenuType,
    SendMenuPolarity,
    SendMenuMessage,
    SendMenuPresets,
    SendMenuSend,
};

#define BAUD_COUNT 3
static const char* const baud_text[BAUD_COUNT] = {"512", "1200", "2400"};
static const PocsagBaudRate baud_values[BAUD_COUNT] = {
    PocsagBaud512,
    PocsagBaud1200,
    PocsagBaud2400,
};

#define TYPE_COUNT 4
static const char* const type_text[TYPE_COUNT] = {"Alphanum", "Numeric", "Tone", "Alert2"};
static const PocsagMsgType type_values[TYPE_COUNT] = {
    PocsagMsgTypeAlpha,
    PocsagMsgTypeNumeric,
    PocsagMsgTypeTone,
    PocsagMsgTypeAlert2,
};

#define POL_COUNT 2
static const char* const pol_text[POL_COUNT] = {"Normal", "Inverted"};

static uint8_t find_freq_index(SubGhzSetting* setting, uint32_t freq) {
    uint8_t count = subghz_setting_get_frequency_count(setting);
    for(uint8_t i = 0; i < count; i++) {
        if(subghz_setting_get_frequency(setting, i) == freq) return i;
    }
    return subghz_setting_get_frequency_default_index(setting);
}

static void format_msg_preview(const PCSGSettings* s, char* buf, size_t buf_size) {
    if(s->msg_type == PocsagMsgTypeTone) {
        strlcpy(buf, "(none)", buf_size);
        return;
    }
    if(s->message[0] == '\0') {
        strlcpy(buf, "(empty)", buf_size);
        return;
    }
    size_t len = strlen(s->message);
    if(len <= 10) {
        strlcpy(buf, s->message, buf_size);
    } else {
        memcpy(buf, s->message, 8);
        buf[8] = '.';
        buf[9] = '.';
        buf[10] = '\0';
    }
}

static void send_freq_changed(VariableItem* item) {
    PCSGSendPlugin* p = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t freq = subghz_setting_get_frequency(p->host.setting, idx);
    p->host.settings->frequency = freq;
    if(p->host.on_frequency_changed) {
        p->host.on_frequency_changed(p->host.host_context, freq);
    }
    char buf[16];
    pcsg_format_frequency(freq, buf, sizeof(buf));
    variable_item_set_current_value_text(item, buf);
}

static void send_baud_changed(VariableItem* item) {
    PCSGSendPlugin* p = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    p->host.settings->baud = baud_values[idx];
    variable_item_set_current_value_text(item, baud_text[idx]);
}

static void send_type_changed(VariableItem* item) {
    PCSGSendPlugin* p = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    p->host.settings->msg_type = type_values[idx];
    variable_item_set_current_value_text(item, type_text[idx]);
    if(p->send_msg_item) {
        char preview[16];
        format_msg_preview(p->host.settings, preview, sizeof(preview));
        variable_item_set_current_value_text(p->send_msg_item, preview);
    }
}

static void send_pol_changed(VariableItem* item) {
    PCSGSendPlugin* p = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    p->host.settings->inverted = (idx != 0);
    variable_item_set_current_value_text(item, pol_text[idx]);
}

static void send_enter_cb(void* context, uint32_t index) {
    PCSGSendPlugin* p = context;
    scene_manager_set_scene_state(p->scene_manager, PluginSceneSend, index);
    switch(index) {
    case SendMenuFreq:
        view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtEditFreq);
        break;
    case SendMenuRic:
        view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtEditRic);
        break;
    case SendMenuMessage:
        if(p->host.settings->msg_type != PocsagMsgTypeTone) {
            view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtEditMessage);
        }
        break;
    case SendMenuPresets:
        view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtOpenPresets);
        break;
    case SendMenuSend:
        view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtTransmit);
        break;
    default:
        break;
    }
}

static void plugin_scene_send_on_enter(void* context) {
    PCSGSendPlugin* p = context;
    PCSGSettings* s = p->host.settings;
    VariableItemList* list = p->var_list;
    VariableItem* item;
    char buf[20];

    item = variable_item_list_add(
        list,
        "Freq (OK:edit)",
        subghz_setting_get_frequency_count(p->host.setting),
        send_freq_changed,
        p);
    variable_item_set_current_value_index(item, find_freq_index(p->host.setting, s->frequency));
    pcsg_format_frequency(s->frequency, buf, sizeof(buf));
    variable_item_set_current_value_text(item, buf);

    item = variable_item_list_add(list, "RIC", 1, NULL, p);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s->ric);
    variable_item_set_current_value_text(item, buf);

    item = variable_item_list_add(list, "Speed (baud)", BAUD_COUNT, send_baud_changed, p);
    uint8_t baud_idx = 1;
    for(uint8_t i = 0; i < BAUD_COUNT; i++) {
        if(baud_values[i] == s->baud) baud_idx = i;
    }
    variable_item_set_current_value_index(item, baud_idx);
    variable_item_set_current_value_text(item, baud_text[baud_idx]);

    item = variable_item_list_add(list, "Type", TYPE_COUNT, send_type_changed, p);
    uint8_t type_idx = 0;
    for(uint8_t i = 0; i < TYPE_COUNT; i++) {
        if(type_values[i] == s->msg_type) type_idx = i;
    }
    variable_item_set_current_value_index(item, type_idx);
    variable_item_set_current_value_text(item, type_text[type_idx]);

    item = variable_item_list_add(list, "Polarity", POL_COUNT, send_pol_changed, p);
    uint8_t pol_idx = s->inverted ? 1 : 0;
    variable_item_set_current_value_index(item, pol_idx);
    variable_item_set_current_value_text(item, pol_text[pol_idx]);

    item = variable_item_list_add(list, "Text", 1, NULL, p);
    p->send_msg_item = item;
    format_msg_preview(s, buf, sizeof(buf));
    variable_item_set_current_value_text(item, buf);

    item = variable_item_list_add(list, "Presets (SD)", 1, NULL, p);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(list, ">> Send Message <<", 1, NULL, p);
    variable_item_set_current_value_text(item, "");

    variable_item_list_set_enter_callback(list, send_enter_cb, p);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(p->scene_manager, PluginSceneSend));

    view_dispatcher_switch_to_view(
        p->host.view_dispatcher, p->host.view_id_base + PluginViewVarList);
}

static bool plugin_scene_send_on_event(void* context, SceneManagerEvent event) {
    PCSGSendPlugin* p = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case PluginEvtEditFreq:
        p->input_mode = PluginInputFreq;
        scene_manager_next_scene(p->scene_manager, PluginSceneInput);
        return true;
    case PluginEvtEditRic:
        p->input_mode = PluginInputRic;
        scene_manager_next_scene(p->scene_manager, PluginSceneInput);
        return true;
    case PluginEvtEditMessage:
        p->input_mode = PluginInputMessage;
        scene_manager_next_scene(p->scene_manager, PluginSceneInput);
        return true;
    case PluginEvtOpenPresets:
        p->preset_delete_mode = false;
        scene_manager_next_scene(p->scene_manager, PluginScenePresets);
        return true;
    case PluginEvtTransmit:
        if(p->host.save_settings) p->host.save_settings(p->host.settings);
        scene_manager_next_scene(p->scene_manager, PluginSceneTx);
        return true;
    default:
        return false;
    }
}

static void plugin_scene_send_on_exit(void* context) {
    PCSGSendPlugin* p = context;
    p->send_msg_item = NULL;
    if(p->host.save_settings) p->host.save_settings(p->host.settings);
    variable_item_list_reset(p->var_list);
}

// ---- Scene 1: Keyboard / Number Input ----

static bool input_freq_validator(const char* text, FuriString* error, void* context) {
    PCSGSendPlugin* p = context;
    uint32_t hz = 0;
    if(!pcsg_parse_frequency(text, &hz)) {
        furi_string_set(error, "Invalid freq!\nUse MHz e.g.\n439.9875");
        return false;
    }
    if(!subghz_devices_is_frequency_valid(p->host.radio_device, hz)) {
        furi_string_set(error, "Freq outside\nCC1101\nrange!");
        return false;
    }
    return true;
}

static bool input_numeric_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    bool has_valid = false;
    for(size_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if((c >= '0' && c <= '9') || c == '-' || c == ' ' || c == '*' || c == 'U' || c == 'u' ||
           c == '(' || c == ')' || c == '[' || c == ']' || c == 'R' || c == 'r') {
            has_valid = true;
        } else {
            furi_string_set(error, "Numeric only:\n0-9 - * U ( )");
            return false;
        }
    }
    if(!has_valid) {
        furi_string_set(error, "Enter at least\none digit");
        return false;
    }
    return true;
}

static void input_text_done_cb(void* context) {
    PCSGSendPlugin* p = context;
    view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtTextDone);
}

static void input_number_done_cb(void* context, int32_t number) {
    PCSGSendPlugin* p = context;
    if(number < 0) number = 0;
    if((uint32_t)number > PCSG_RIC_MAX) number = (int32_t)PCSG_RIC_MAX;
    p->host.settings->ric = (uint32_t)number;
    view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtNumberDone);
}

static void plugin_scene_input_on_enter(void* context) {
    PCSGSendPlugin* p = context;
    PCSGSettings* s = p->host.settings;

    if(p->input_mode == PluginInputRic) {
        number_input_set_header_text(p->number_input, "Enter RIC (0-2097151)");
        number_input_set_result_callback(
            p->number_input, input_number_done_cb, p, (int32_t)s->ric, 0, (int32_t)PCSG_RIC_MAX);
        view_dispatcher_switch_to_view(
            p->host.view_dispatcher, p->host.view_id_base + PluginViewNumberInput);
        return;
    }

    pcsg_text_input_reset(p->text_input);

    switch(p->input_mode) {
    case PluginInputFreq:
        pcsg_format_frequency(s->frequency, p->input_buf, sizeof(p->input_buf));
        pcsg_text_input_set_header_text(p->text_input, "Freq (MHz or Hz):");
        pcsg_text_input_set_validator(p->text_input, input_freq_validator, p);
        pcsg_text_input_set_minimum_length(p->text_input, 3);
        pcsg_text_input_set_result_callback(
            p->text_input, input_text_done_cb, p, p->input_buf, 16, true);
        break;
    case PluginInputMessage:
        strlcpy(p->input_buf, s->message, sizeof(p->input_buf));
        if(s->msg_type == PocsagMsgTypeNumeric) {
            pcsg_text_input_set_header_text(p->text_input, "Numeric msg (0-9 - * U):");
            pcsg_text_input_set_validator(p->text_input, input_numeric_validator, p);
            pcsg_text_input_set_minimum_length(p->text_input, 1);
        } else {
            pcsg_text_input_set_header_text(p->text_input, "Message (hold _ = space):");
            pcsg_text_input_set_minimum_length(
                p->text_input, s->msg_type == PocsagMsgTypeAlert2 ? 0 : 1);
        }
        pcsg_text_input_set_result_callback(
            p->text_input, input_text_done_cb, p, p->input_buf, sizeof(p->input_buf), false);
        break;
    case PluginInputPresetName:
        snprintf(p->input_buf, sizeof(p->input_buf), "RIC_%lu", (unsigned long)s->ric);
        pcsg_text_input_set_header_text(p->text_input, "Preset name:");
        pcsg_text_input_set_minimum_length(p->text_input, 1);
        pcsg_text_input_set_result_callback(
            p->text_input, input_text_done_cb, p, p->input_buf, PCSG_PRESET_NAME_MAX, true);
        break;
    default:
        break;
    }

    view_dispatcher_switch_to_view(
        p->host.view_dispatcher, p->host.view_id_base + PluginViewTextInput);
}

static bool plugin_scene_input_on_event(void* context, SceneManagerEvent event) {
    PCSGSendPlugin* p = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == PluginEvtTextDone) {
        switch(p->input_mode) {
        case PluginInputFreq: {
            uint32_t hz = 0;
            if(pcsg_parse_frequency(p->input_buf, &hz) &&
               subghz_devices_is_frequency_valid(p->host.radio_device, hz)) {
                p->host.settings->frequency = hz;
                if(p->host.on_frequency_changed) {
                    p->host.on_frequency_changed(p->host.host_context, hz);
                }
                if(p->host.save_settings) p->host.save_settings(p->host.settings);
            }
            break;
        }
        case PluginInputMessage:
            strlcpy(p->host.settings->message, p->input_buf, sizeof(p->host.settings->message));
            if(p->host.save_settings) p->host.save_settings(p->host.settings);
            break;
        case PluginInputPresetName:
            pcsg_preset_save(p->input_buf, p->host.settings);
            notification_message(p->host.notifications, &sequence_success);
            break;
        default:
            break;
        }
        scene_manager_previous_scene(p->scene_manager);
        return true;
    } else if(event.event == PluginEvtNumberDone) {
        if(p->host.save_settings) p->host.save_settings(p->host.settings);
        scene_manager_previous_scene(p->scene_manager);
        return true;
    }

    return false;
}

static void plugin_scene_input_on_exit(void* context) {
    PCSGSendPlugin* p = context;
    pcsg_text_input_reset(p->text_input);
}

// ---- Scene 2: Presets ----

static void presets_submenu_cb(void* context, uint32_t index) {
    PCSGSendPlugin* p = context;
    view_dispatcher_send_custom_event(p->host.view_dispatcher, index);
}

static void presets_rebuild_menu(PCSGSendPlugin* p) {
    submenu_reset(p->submenu);
    p->preset_count = pcsg_presets_list(p->preset_names, PCSG_PRESETS_MAX_COUNT);

    if(p->preset_delete_mode) {
        submenu_set_header(p->submenu, "Select preset to DELETE:");
    } else {
        submenu_set_header(p->submenu, "RIC & Message Presets");
        submenu_add_item(p->submenu, "[+] Save Current", PRESET_ACTION_SAVE, presets_submenu_cb, p);
        if(p->preset_count > 0) {
            submenu_add_item(
                p->submenu, "[-] Delete Preset...", PRESET_ACTION_TOGGLE_DEL, presets_submenu_cb, p);
        }
    }

    for(size_t i = 0; i < p->preset_count; i++) {
        submenu_add_item(p->submenu, p->preset_names[i], (uint32_t)i, presets_submenu_cb, p);
    }
}

static void plugin_scene_presets_on_enter(void* context) {
    PCSGSendPlugin* p = context;
    presets_rebuild_menu(p);
    view_dispatcher_switch_to_view(
        p->host.view_dispatcher, p->host.view_id_base + PluginViewSubmenu);
}

static bool plugin_scene_presets_on_event(void* context, SceneManagerEvent event) {
    PCSGSendPlugin* p = context;
    if(event.type == SceneManagerEventTypeBack && p->preset_delete_mode) {
        p->preset_delete_mode = false;
        presets_rebuild_menu(p);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == PRESET_ACTION_SAVE) {
        p->input_mode = PluginInputPresetName;
        scene_manager_next_scene(p->scene_manager, PluginSceneInput);
        return true;
    } else if(event.event == PRESET_ACTION_TOGGLE_DEL) {
        p->preset_delete_mode = true;
        presets_rebuild_menu(p);
        return true;
    } else if(event.event < p->preset_count) {
        const char* name = p->preset_names[event.event];
        if(p->preset_delete_mode) {
            pcsg_preset_delete(name);
            notification_message(p->host.notifications, &sequence_blink_red_10);
            p->preset_delete_mode = false;
            presets_rebuild_menu(p);
        } else {
            if(pcsg_preset_load(name, p->host.settings)) {
                if(!subghz_devices_is_frequency_valid(
                       p->host.radio_device, p->host.settings->frequency)) {
                    p->host.settings->frequency = PCSG_DEFAULT_FREQ;
                }
                if(p->host.on_frequency_changed) {
                    p->host.on_frequency_changed(p->host.host_context, p->host.settings->frequency);
                }
                if(p->host.save_settings) p->host.save_settings(p->host.settings);
                notification_message(p->host.notifications, &sequence_success);
                scene_manager_previous_scene(p->scene_manager);
            }
        }
        return true;
    }
    return false;
}

static void plugin_scene_presets_on_exit(void* context) {
    PCSGSendPlugin* p = context;
    submenu_reset(p->submenu);
}

// ---- Scene 3: TX Progress ----

static void tx_view_cb(uint32_t event, void* context) {
    PCSGSendPlugin* p = context;
    if(event == PCSGTxViewEventBack) {
        view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtTxBack);
    } else if(event == PCSGTxViewEventResend) {
        view_dispatcher_send_custom_event(p->host.view_dispatcher, PluginEvtTxResend);
    }
}

static void tx_scene_begin(PCSGSendPlugin* p) {
    PCSGSettings* s = p->host.settings;
    pcsg_view_tx_set_info(
        p->tx_view,
        s->frequency,
        s->ric,
        (uint32_t)s->baud,
        pcsg_msg_type_name(s->msg_type),
        s->inverted,
        p->host.is_external_radio);

    if(!plugin_is_tx_allowed(p, s->frequency)) {
        pcsg_view_tx_set_state(
            p->tx_view,
            PCSGTxViewStateBlocked,
            0.0f,
            "TX not allowed on this\nfrequency in your region");
        notification_message(p->host.notifications, &sequence_error);
        return;
    }

    if(!plugin_tx_start(p)) {
        pcsg_view_tx_set_state(
            p->tx_view, PCSGTxViewStateBlocked, 0.0f, "TX blocked or radio\nfailed to start");
        notification_message(p->host.notifications, &sequence_error);
        return;
    }

    pcsg_view_tx_set_state(p->tx_view, PCSGTxViewStateSending, 0.05f, "Sending...");
    notification_message(p->host.notifications, &sequence_blink_start_magenta);
}

static void plugin_scene_tx_on_enter(void* context) {
    PCSGSendPlugin* p = context;
    pcsg_view_tx_set_callback(p->tx_view, tx_view_cb, p);
    view_dispatcher_switch_to_view(p->host.view_dispatcher, p->host.view_id_base + PluginViewTx);
    tx_scene_begin(p);
}

static bool plugin_scene_tx_on_event(void* context, SceneManagerEvent event) {
    PCSGSendPlugin* p = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PluginEvtTxBack) {
            scene_manager_previous_scene(p->scene_manager);
            return true;
        } else if(event.event == PluginEvtTxResend) {
            tx_scene_begin(p);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(p->tx_active) {
            if(subghz_devices_is_async_complete_tx(p->host.radio_device)) {
                plugin_tx_stop(p);
                notification_message(p->host.notifications, &sequence_blink_stop);
                notification_message(p->host.notifications, &pcsg_sequence_tx_done);
                pcsg_view_tx_set_state(p->tx_view, PCSGTxViewStateDone, 1.0f, "Done!");
            } else {
                uint32_t elapsed_ms = furi_get_tick() - p->tx_start_tick;
                float progress = 0.05f;
                if(p->tx_duration_ms > 0) {
                    progress = (float)elapsed_ms / (float)p->tx_duration_ms;
                }
                if(progress < 0.05f) progress = 0.05f;
                if(progress > 0.98f) progress = 0.98f;
                char pct[16];
                snprintf(pct, sizeof(pct), "%u%%", (unsigned)(progress * 100.0f));
                pcsg_view_tx_set_state(p->tx_view, PCSGTxViewStateSending, progress, pct);
            }
            return true;
        }
    }
    return false;
}

static void plugin_scene_tx_on_exit(void* context) {
    PCSGSendPlugin* p = context;
    plugin_tx_stop(p);
    notification_message(p->host.notifications, &sequence_blink_stop);
}

// ---- Plugin Scene Manager table & Entry Point ----

static void (*const plugin_on_enter_handlers[])(void*) = {
    [PluginSceneSend] = plugin_scene_send_on_enter,
    [PluginSceneInput] = plugin_scene_input_on_enter,
    [PluginScenePresets] = plugin_scene_presets_on_enter,
    [PluginSceneTx] = plugin_scene_tx_on_enter,
};

static bool (*const plugin_on_event_handlers[])(void*, SceneManagerEvent) = {
    [PluginSceneSend] = plugin_scene_send_on_event,
    [PluginSceneInput] = plugin_scene_input_on_event,
    [PluginScenePresets] = plugin_scene_presets_on_event,
    [PluginSceneTx] = plugin_scene_tx_on_event,
};

static void (*const plugin_on_exit_handlers[])(void*) = {
    [PluginSceneSend] = plugin_scene_send_on_exit,
    [PluginSceneInput] = plugin_scene_input_on_exit,
    [PluginScenePresets] = plugin_scene_presets_on_exit,
    [PluginSceneTx] = plugin_scene_tx_on_exit,
};

static const SceneManagerHandlers plugin_scene_handlers = {
    .on_enter_handlers = plugin_on_enter_handlers,
    .on_event_handlers = plugin_on_event_handlers,
    .on_exit_handlers = plugin_on_exit_handlers,
    .scene_num = PluginSceneCount,
};

static void* pcsg_send_plugin_create(const PCSGSendHostApi* host) {
    furi_assert(host);
    PCSGSendPlugin* p = malloc(sizeof(PCSGSendPlugin));
    memset(p, 0, sizeof(PCSGSendPlugin));
    p->host = *host;

    p->scene_manager = scene_manager_alloc(&plugin_scene_handlers, p);
    p->var_list = variable_item_list_alloc();
    p->submenu = submenu_alloc();
    p->text_input = pcsg_text_input_alloc();
    p->number_input = number_input_alloc();
    p->tx_view = pcsg_view_tx_alloc();

    view_dispatcher_add_view(
        p->host.view_dispatcher,
        p->host.view_id_base + PluginViewVarList,
        variable_item_list_get_view(p->var_list));
    view_dispatcher_add_view(
        p->host.view_dispatcher,
        p->host.view_id_base + PluginViewSubmenu,
        submenu_get_view(p->submenu));
    view_dispatcher_add_view(
        p->host.view_dispatcher,
        p->host.view_id_base + PluginViewTextInput,
        pcsg_text_input_get_view(p->text_input));
    view_dispatcher_add_view(
        p->host.view_dispatcher,
        p->host.view_id_base + PluginViewNumberInput,
        number_input_get_view(p->number_input));
    view_dispatcher_add_view(
        p->host.view_dispatcher,
        p->host.view_id_base + PluginViewTx,
        pcsg_view_tx_get_view(p->tx_view));

    scene_manager_next_scene(p->scene_manager, PluginSceneSend);
    return p;
}

static bool pcsg_send_plugin_on_event(void* plugin_ctx, SceneManagerEvent event) {
    PCSGSendPlugin* p = plugin_ctx;
    if(!p) return false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        return scene_manager_handle_custom_event(p->scene_manager, event.event);
    case SceneManagerEventTypeBack:
        return scene_manager_handle_back_event(p->scene_manager);
    case SceneManagerEventTypeTick:
        scene_manager_handle_tick_event(p->scene_manager);
        return true;
    default:
        return false;
    }
}

static void pcsg_send_plugin_destroy(void* plugin_ctx) {
    PCSGSendPlugin* p = plugin_ctx;
    if(!p) return;

    plugin_tx_stop(p);
    notification_message(p->host.notifications, &sequence_blink_stop);
    scene_manager_stop(p->scene_manager);

    // Detach active plugin view before removing views from shared dispatcher
    view_dispatcher_switch_to_view(p->host.view_dispatcher, VIEW_NONE);
    for(uint32_t i = 0; i < PluginViewCount; i++) {
        view_dispatcher_remove_view(p->host.view_dispatcher, p->host.view_id_base + i);
    }

    pcsg_view_tx_free(p->tx_view);
    number_input_free(p->number_input);
    pcsg_text_input_free(p->text_input);
    submenu_free(p->submenu);
    variable_item_list_free(p->var_list);
    scene_manager_free(p->scene_manager);
    free(p);
}

static const PCSGSendPluginInterface pcsg_send_plugin_interface = {
    .create = pcsg_send_plugin_create,
    .on_event = pcsg_send_plugin_on_event,
    .destroy = pcsg_send_plugin_destroy,
};

static const FlipperAppPluginDescriptor pcsg_send_plugin_descriptor = {
    .appid = PCSG_SEND_PLUGIN_APP_ID,
    .ep_api_version = PCSG_SEND_PLUGIN_API_VERSION,
    .entry_point = &pcsg_send_plugin_interface,
};

const FlipperAppPluginDescriptor* pocsag_pager_send_plugin_ep(void) {
    return &pcsg_send_plugin_descriptor;
}
