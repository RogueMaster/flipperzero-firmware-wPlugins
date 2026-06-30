#include "dashboard_i.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define DASH_OBD_DTC_STORED    0U
#define DASH_OBD_DTC_PENDING   1U
#define DASH_OBD_DTC_PERMANENT 2U
#define DASH_REVERSE_PHASE_INIT 0U
#define DASH_REVERSE_PHASE_CAL  1U
#define DASH_REVERSE_PHASE_MON  2U
#define DASH_REVERSE_PHASE_DONE 3U
#define DASH_REVERSE_PHASE_READ 4U
#define DASH_REVERSE_FLASH_MS   250U

static bool dashboard_is_numeric_token(const char* text) {
    if(!text || text[0] == '\0') {
        return false;
    }

    bool has_digit = false;
    for(size_t i = 0; text[i] != '\0'; i++) {
        const char c = text[i];
        if(isdigit((unsigned char)c)) {
            has_digit = true;
            continue;
        }
        if(c == '.' || c == '-' || c == '+') {
            continue;
        }
        return false;
    }

    return has_digit;
}

static void dashboard_trim_inplace(char* text) {
    if(!text) {
        return;
    }

    char* start = text;
    while(*start == ' ' || *start == '\t') {
        start++;
    }

    if(start != text) {
        memmove(text, start, strlen(start) + 1U);
    }

    size_t len = strlen(text);
    while(len > 0U && (text[len - 1U] == ' ' || text[len - 1U] == '\t')) {
        text[len - 1U] = '\0';
        len--;
    }
}

static bool dashboard_ieq(const char* a, const char* b) {
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

static bool dashboard_is_hex_char(char c) {
    return ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'));
}

static bool dashboard_is_dtc_code_token(const char* token) {
    if(!token) {
        return false;
    }

    const size_t len = strlen(token);
    if(len != 5U) {
        return false;
    }

    const char system = token[0];
    if(system != 'P' && system != 'C' && system != 'B' && system != 'U') {
        return false;
    }

    return dashboard_is_hex_char(token[1]) && dashboard_is_hex_char(token[2]) &&
           dashboard_is_hex_char(token[3]) && dashboard_is_hex_char(token[4]);
}

static bool dashboard_parse_obd_dtc_line(
    const char* text,
    char* out_label,
    size_t out_label_size,
    char* out_value,
    size_t out_value_size,
    char* out_note,
    size_t out_note_size) {
    if(
        !text || !out_label || out_label_size == 0U || !out_value || out_value_size == 0U ||
        !out_note || out_note_size == 0U) {
        return false;
    }

    const char* marker = strstr(text, "DTCs:");
    if(!marker) {
        return false;
    }

    size_t prefix_len = (size_t)((marker - text) + 4); // include "DTCs"
    if(prefix_len >= out_label_size) {
        prefix_len = out_label_size - 1U;
    }
    memcpy(out_label, text, prefix_len);
    out_label[prefix_len] = '\0';
    dashboard_trim_inplace(out_label);
    if(out_label[0] == '\0') {
        strncpy(out_label, "DTCs", out_label_size - 1U);
        out_label[out_label_size - 1U] = '\0';
    }

    const char* right = marker + 5;
    while(*right == ' ' || *right == '\t') {
        right++;
    }

    if(*right == '\0' || dashboard_ieq(right, "none")) {
        strncpy(out_value, "None", out_value_size - 1U);
        out_value[out_value_size - 1U] = '\0';
        strncpy(out_note, "P0 B0 C0 U0", out_note_size - 1U);
        out_note[out_note_size - 1U] = '\0';
        return true;
    }

    if(right[0] == '+') {
        strncpy(out_value, right, out_value_size - 1U);
        out_value[out_value_size - 1U] = '\0';
        strncpy(out_note, "Additional DTCs", out_note_size - 1U);
        out_note[out_note_size - 1U] = '\0';
        return true;
    }

    char scratch[96] = {0};
    strncpy(scratch, right, sizeof(scratch) - 1U);

    uint8_t p_count = 0U;
    uint8_t b_count = 0U;
    uint8_t c_count = 0U;
    uint8_t u_count = 0U;
    uint8_t total = 0U;

    char* save_ptr = NULL;
    char* token = strtok_r(scratch, " ", &save_ptr);
    while(token) {
        if(dashboard_is_dtc_code_token(token)) {
            total++;
            switch(token[0]) {
            case 'P':
                p_count++;
                break;
            case 'B':
                b_count++;
                break;
            case 'C':
                c_count++;
                break;
            case 'U':
                u_count++;
                break;
            default:
                break;
            }
        }
        token = strtok_r(NULL, " ", &save_ptr);
    }

    if(total == 0U) {
        strncpy(out_value, right, out_value_size - 1U);
        out_value[out_value_size - 1U] = '\0';
        out_note[0] = '\0';
        return true;
    }

    snprintf(out_value, out_value_size, "P%u B%u C%u U%u", p_count, b_count, c_count, u_count);
    out_value[out_value_size - 1U] = '\0';
    snprintf(out_note, out_note_size, "Powertrain Body Chassis Network");
    out_note[out_note_size - 1U] = '\0';
    return true;
}

static bool dashboard_starts_with(const char* text, const char* prefix) {
    if(!text || !prefix) {
        return false;
    }

    const size_t prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

static int8_t dashboard_obd_dtc_type_from_text(const char* text) {
    if(!text) {
        return -1;
    }

    if(strstr(text, "stored DTCs:")) {
        return (int8_t)DASH_OBD_DTC_STORED;
    }
    if(strstr(text, "pending DTCs:")) {
        return (int8_t)DASH_OBD_DTC_PENDING;
    }
    if(strstr(text, "permanent DTCs:")) {
        return (int8_t)DASH_OBD_DTC_PERMANENT;
    }

    return -1;
}

static void dashboard_obd_dtc_clear_type(AppDashboardModel* model, uint8_t type_index) {
    if(!model || type_index >= 3U) {
        return;
    }

    model->obd_dtc_count[type_index] = 0U;
    memset(model->obd_dtc_codes[type_index], 0, sizeof(model->obd_dtc_codes[type_index]));
    model->obd_dtc_selected[type_index] = 0U;
}

static void dashboard_obd_dtc_clear_all(AppDashboardModel* model) {
    if(!model) {
        return;
    }

    for(uint8_t i = 0U; i < 3U; i++) {
        dashboard_obd_dtc_clear_type(model, i);
    }
    memset(model->obd_dtc_cat_counts, 0, sizeof(model->obd_dtc_cat_counts));
    model->obd_dtc_page = 0U;
    model->obd_dtc_complete = false;
}

static void dashboard_obd_dtc_recompute_categories(AppDashboardModel* model) {
    if(!model) {
        return;
    }

    memset(model->obd_dtc_cat_counts, 0, sizeof(model->obd_dtc_cat_counts));
    for(uint8_t type = 0U; type < 3U; type++) {
        const uint8_t count = model->obd_dtc_count[type];
        for(uint8_t i = 0U; i < count && i < DASH_OBD_DTC_MAX; i++) {
            const char system = model->obd_dtc_codes[type][i][0];
            if(system == 'P') {
                model->obd_dtc_cat_counts[0]++;
            } else if(system == 'B') {
                model->obd_dtc_cat_counts[1]++;
            } else if(system == 'C') {
                model->obd_dtc_cat_counts[2]++;
            } else if(system == 'U') {
                model->obd_dtc_cat_counts[3]++;
            }
        }
    }
}

static bool dashboard_obd_dtc_add_unique(
    AppDashboardModel* model,
    uint8_t type_index,
    const char* code) {
    if(!model || type_index >= 3U || !code || !dashboard_is_dtc_code_token(code)) {
        return false;
    }

    uint8_t count = model->obd_dtc_count[type_index];
    for(uint8_t i = 0U; i < count && i < DASH_OBD_DTC_MAX; i++) {
        if(strncmp(model->obd_dtc_codes[type_index][i], code, 5U) == 0) {
            return false;
        }
    }

    if(count >= DASH_OBD_DTC_MAX) {
        return false;
    }

    memcpy(model->obd_dtc_codes[type_index][count], code, 5U);
    model->obd_dtc_codes[type_index][count][5] = '\0';
    model->obd_dtc_count[type_index] = (uint8_t)(count + 1U);
    return true;
}

static CcBus dashboard_parse_bus(const char* text) {
    if(!text) {
        return CcBusCan0;
    }

    if(dashboard_ieq(text, "can1") || dashboard_ieq(text, "1")) {
        return CcBusCan1;
    }

    if(dashboard_ieq(text, "both") || dashboard_ieq(text, "2")) {
        return CcBusBoth;
    }

    return CcBusCan0;
}

static bool dashboard_arg_get_value_last(const char* args, const char* key, char* out, size_t out_size) {
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

static bool dashboard_parse_u32_key_last(const char* args, const char* key, uint32_t base, uint32_t* out) {
    char raw[32] = {0};
    if(!out || !dashboard_arg_get_value_last(args, key, raw, sizeof(raw))) {
        return false;
    }

    char* end = NULL;
    const unsigned long parsed = strtoul(raw, &end, base);
    if(end == raw || *end != '\0') {
        return false;
    }

    *out = (uint32_t)parsed;
    return true;
}

static bool dashboard_parse_bool_key_last(const char* args, const char* key, bool* out) {
    char raw[16] = {0};
    if(!out || !dashboard_arg_get_value_last(args, key, raw, sizeof(raw))) {
        return false;
    }

    if(strcmp(raw, "1") == 0 || dashboard_ieq(raw, "true")) {
        *out = true;
        return true;
    }

    if(strcmp(raw, "0") == 0 || dashboard_ieq(raw, "false")) {
        *out = false;
        return true;
    }

    return false;
}

static uint8_t dashboard_hex_nibble(char c, bool* ok) {
    if(c >= '0' && c <= '9') {
        *ok = true;
        return (uint8_t)(c - '0');
    }
    if(c >= 'a' && c <= 'f') {
        *ok = true;
        return (uint8_t)(10 + (c - 'a'));
    }
    if(c >= 'A' && c <= 'F') {
        *ok = true;
        return (uint8_t)(10 + (c - 'A'));
    }

    *ok = false;
    return 0U;
}

static uint8_t dashboard_parse_hex_bytes(const char* hex, uint8_t* out, uint8_t out_capacity) {
    if(!hex || !out || out_capacity == 0U) {
        return 0U;
    }

    char compact[17] = {0};
    uint8_t compact_len = 0U;
    for(size_t i = 0U; hex[i] != '\0' && compact_len < sizeof(compact) - 1U; i++) {
        if(dashboard_is_hex_char(hex[i])) {
            compact[compact_len++] = (char)toupper((unsigned char)hex[i]);
        }
    }

    if((compact_len % 2U) != 0U) {
        compact_len--;
    }

    const uint8_t byte_count = (uint8_t)(compact_len / 2U);
    const uint8_t limit = (byte_count < out_capacity) ? byte_count : out_capacity;
    for(uint8_t i = 0U; i < limit; i++) {
        bool hi_ok = false;
        bool lo_ok = false;
        const uint8_t hi = dashboard_hex_nibble(compact[(size_t)i * 2U], &hi_ok);
        const uint8_t lo = dashboard_hex_nibble(compact[(size_t)i * 2U + 1U], &lo_ok);
        if(!hi_ok || !lo_ok) {
            return i;
        }
        out[i] = (uint8_t)((hi << 4U) | lo);
    }

    return limit;
}

static void dashboard_update_write_cfg_from_args(AppDashboardModel* model, const char* args) {
    if(!model || !args) {
        return;
    }

    char bus_name[16] = {0};
    if(dashboard_arg_get_value_last(args, "bus", bus_name, sizeof(bus_name))) {
        model->write_bus = dashboard_parse_bus(bus_name);
    }

    uint32_t id = 0U;
    if(dashboard_parse_u32_key_last(args, "id", 16, &id)) {
        model->write_id = id;
    }

    bool ext = false;
    if(dashboard_parse_bool_key_last(args, "ext", &ext)) {
        model->write_ext = ext;
    }

    uint32_t dlc = 0U;
    if(dashboard_parse_u32_key_last(args, "dlc", 10, &dlc) && dlc <= 8U) {
        model->write_dlc = (uint8_t)dlc;
    }

    char data_hex[40] = {0};
    if(dashboard_arg_get_value_last(args, "data", data_hex, sizeof(data_hex))) {
        memset(model->write_data, 0, sizeof(model->write_data));
        const uint8_t parsed = dashboard_parse_hex_bytes(data_hex, model->write_data, 8U);
        if(parsed > model->write_dlc) {
            model->write_dlc = parsed;
        }
    }

    uint32_t count = 0U;
    if(dashboard_parse_u32_key_last(args, "count", 10, &count) && count > 0U) {
        model->write_count_cfg = count;
    } else {
        model->write_count_cfg = 1U;
    }

    uint32_t interval_ms = 0U;
    if(dashboard_parse_u32_key_last(args, "interval_ms", 10, &interval_ms)) {
        model->write_interval_ms_cfg = interval_ms;
    } else if(dashboard_parse_u32_key_last(args, "period_ms", 10, &interval_ms)) {
        model->write_interval_ms_cfg = interval_ms;
    } else {
        model->write_interval_ms_cfg = 250U;
    }
}

static void dashboard_custom_slot_name_from_app(App* app, uint8_t slot_index, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(slot_index >= 5U) {
        return;
    }

    const char* slot_args = app_custom_inject_get_slot_args(app, slot_index);
    if(slot_args) {
        char slot_name[20] = {0};
        if(dashboard_arg_get_value_last(slot_args, "slot_name", slot_name, sizeof(slot_name)) &&
           slot_name[0] != '\0') {
            strncpy(out, slot_name, out_size - 1U);
            out[out_size - 1U] = '\0';
            return;
        }
    }

    snprintf(out, out_size, "Slot%u", (unsigned)(slot_index + 1U));
    out[out_size - 1U] = '\0';
}

static uint8_t dashboard_input_key_bit(InputKey key) {
    switch(key) {
    case InputKeyLeft:
        return 1U << 0;
    case InputKeyRight:
        return 1U << 1;
    case InputKeyUp:
        return 1U << 2;
    case InputKeyDown:
        return 1U << 3;
    case InputKeyOk:
        return 1U << 4;
    default:
        return 0U;
    }
}

static uint8_t dashboard_ring_newest_index(uint8_t head, uint8_t capacity, uint8_t offset) {
    const uint8_t newest = (uint8_t)((head + capacity - 1U) % capacity);
    return (uint8_t)((newest + capacity - offset) % capacity);
}

static const DashboardSpeedSample* dashboard_speed_get(const AppDashboardModel* model, uint8_t offset) {
    if(!model || offset >= model->speed_count) {
        return NULL;
    }

    const uint8_t index =
        dashboard_ring_newest_index(model->speed_head, DASH_SPEED_HISTORY, offset);
    const DashboardSpeedSample* sample = &model->speed_samples[index];
    return sample->valid ? sample : NULL;
}

static const DashboardValChange* dashboard_val_get(const AppDashboardModel* model, uint8_t offset) {
    if(!model || offset >= model->val_count) {
        return NULL;
    }

    const uint8_t index = dashboard_ring_newest_index(model->val_head, DASH_VAL_HISTORY, offset);
    const DashboardValChange* entry = &model->val_changes[index];
    return entry->valid ? entry : NULL;
}

static const DashboardUniqueEntry* dashboard_unique_get(const AppDashboardModel* model, uint8_t offset) {
    if(!model || offset >= model->unique_count) {
        return NULL;
    }

    const uint8_t index = dashboard_ring_newest_index(model->unique_head, DASH_UNIQUE_HISTORY, offset);
    const DashboardUniqueEntry* entry = &model->unique_entries[index];
    return entry->valid ? entry : NULL;
}

static uint8_t dashboard_dbc_page_count(const AppDashboardModel* model) {
    if(!model || model->dbc_signal_count == 0U) {
        return 1U;
    }

    return (uint8_t)(1U + model->dbc_signal_count);
}

static const char* dashboard_custom_event_get(const AppDashboardModel* model, uint8_t offset) {
    if(!model || offset >= model->custom_recent_count) {
        return NULL;
    }

    const uint8_t index =
        dashboard_ring_newest_index(model->custom_recent_head, DASH_CUSTOM_HISTORY, offset);
    return model->custom_recent[index];
}

static int8_t dashboard_dbc_find_signal_slot(const AppDashboardModel* model, uint16_t sid) {
    if(!model) {
        return -1;
    }

    for(uint8_t i = 0U; i < model->dbc_signal_count; i++) {
        if(model->dbc_signals[i].sid == sid) {
            return (int8_t)i;
        }
    }
    return -1;
}

static void dashboard_dbc_sync_registered(AppDashboardModel* model, const App* app) {
    if(!model || !app) {
        return;
    }

    DashboardDbcEntry ordered[APP_DBC_CFG_MAX_SIGNALS] = {0};
    uint8_t count = 0U;

    for(uint8_t cfg_index = 0U; cfg_index < APP_DBC_CFG_MAX_SIGNALS && app->dbc_config_signals; cfg_index++) {
        const AppDbcSignalCache* cfg = &app->dbc_config_signals[cfg_index];
        if(!cfg->used) {
            continue;
        }
        if(count >= APP_DBC_CFG_MAX_SIGNALS) {
            break;
        }

        DashboardDbcEntry seed = {0};
        seed.sid = cfg->def.sid;
        seed.bus = cfg->def.bus;
        seed.frame_id = cfg->def.id;
        seed.in_range = true;
        if(cfg->signal_name[0] != '\0') {
            strncpy(seed.signal_name, cfg->signal_name, sizeof(seed.signal_name) - 1U);
            seed.signal_name[sizeof(seed.signal_name) - 1U] = '\0';
        } else {
            snprintf(seed.signal_name, sizeof(seed.signal_name), "SID%u", (unsigned)cfg->def.sid);
            seed.signal_name[sizeof(seed.signal_name) - 1U] = '\0';
        }
        strncpy(seed.unit, cfg->def.unit, sizeof(seed.unit) - 1U);
        seed.unit[sizeof(seed.unit) - 1U] = '\0';

        const int8_t prev_slot = dashboard_dbc_find_signal_slot(model, cfg->def.sid);
        if(prev_slot >= 0) {
            ordered[count] = model->dbc_signals[(uint8_t)prev_slot];
            if(ordered[count].sid == 0U) {
                ordered[count].sid = cfg->def.sid;
            }
            if(ordered[count].bus > CcBusBoth) {
                ordered[count].bus = cfg->def.bus;
            }
            if(ordered[count].frame_id == 0U) {
                ordered[count].frame_id = cfg->def.id;
            }
            if(cfg->signal_name[0] != '\0') {
                strncpy(
                    ordered[count].signal_name, cfg->signal_name, sizeof(ordered[count].signal_name) - 1U);
                ordered[count].signal_name[sizeof(ordered[count].signal_name) - 1U] = '\0';
            } else if(ordered[count].signal_name[0] == '\0') {
                snprintf(
                    ordered[count].signal_name,
                    sizeof(ordered[count].signal_name),
                    "SID%u",
                    (unsigned)cfg->def.sid);
                ordered[count].signal_name[sizeof(ordered[count].signal_name) - 1U] = '\0';
            }
        } else {
            ordered[count] = seed;
        }
        count++;
    }

    memset(model->dbc_signals, 0, sizeof(model->dbc_signals));
    if(count > 0U) {
        memcpy(model->dbc_signals, ordered, sizeof(DashboardDbcEntry) * count);
    }
    model->dbc_signal_count = count;

    if(model->dbc_signal_selected >= model->dbc_signal_count) {
        model->dbc_signal_selected = (model->dbc_signal_count == 0U) ? 0U :
                                                                 (uint8_t)(model->dbc_signal_count - 1U);
    }

    const uint8_t max_page = (model->dbc_signal_count == 0U) ? 0U : model->dbc_signal_count;
    if(model->mode_page > max_page) {
        model->mode_page = max_page;
    }
}

static void dashboard_format_id(bool ext, uint32_t id, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }
    if(ext) {
        snprintf(out, out_size, "%08lX", (unsigned long)id);
    } else {
        snprintf(out, out_size, "%03lX", (unsigned long)(id & 0x7FFU));
    }
}

static const char* dashboard_reverse_phase_text(uint8_t phase) {
    switch(phase) {
    case DASH_REVERSE_PHASE_CAL:
        return "Calibration";
    case DASH_REVERSE_PHASE_MON:
        return "Monitoring";
    case DASH_REVERSE_PHASE_DONE:
        return "Done";
    case DASH_REVERSE_PHASE_READ:
        return "Read";
    default:
        return "Init";
    }
}

static void dashboard_reverse_clear_changes(AppDashboardModel* model) {
    if(!model) {
        return;
    }

    model->reverse_count = 0U;
    model->reverse_selected = 0U;
    model->reverse_overflow = false;
    memset(model->reverse_ids, 0, sizeof(model->reverse_ids));
    memset(model->reverse_ext, 0, sizeof(model->reverse_ext));
    memset(model->reverse_byte_mask, 0, sizeof(model->reverse_byte_mask));
    memset(model->reverse_flash_until_ms, 0, sizeof(model->reverse_flash_until_ms));
}

static void dashboard_reverse_format_bytes(
    uint8_t mask,
    const uint32_t flash_until_ms[8],
    uint32_t now_ms,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(mask == 0U) {
        strncpy(out, "--", out_size - 1U);
        out[out_size - 1U] = '\0';
        return;
    }

    size_t used = 0U;
    for(uint8_t b = 0U; b < 8U; b++) {
        if((mask & (uint8_t)(1U << b)) == 0U) {
            continue;
        }

        const bool flash = flash_until_ms && (flash_until_ms[b] >= now_ms);
        const char* sep = (used == 0U) ? "" : ", ";
        const int wrote =
            snprintf(out + used, out_size - used, "%s%u%s", sep, (unsigned)b, flash ? "*" : "");
        if(wrote < 0) {
            break;
        }

        const size_t wrote_u = (size_t)wrote;
        if(wrote_u >= (out_size - used)) {
            used = out_size - 1U;
            break;
        }
        used += wrote_u;
    }

    out[out_size - 1U] = '\0';
}

static int8_t dashboard_reverse_find_slot(
    const AppDashboardModel* model,
    uint32_t id,
    bool ext) {
    if(!model) {
        return -1;
    }

    for(uint8_t i = 0U; i < model->reverse_count && i < DASH_REVERSE_MAX_IDS; i++) {
        if(model->reverse_ids[i] == id && model->reverse_ext[i] == ext) {
            return (int8_t)i;
        }
    }

    return -1;
}

static int8_t dashboard_reverse_add_or_update(
    AppDashboardModel* model,
    uint32_t id,
    bool ext,
    uint8_t byte_idx) {
    if(!model || byte_idx >= 8U) {
        return -1;
    }

    const uint8_t byte_bit = (uint8_t)(1U << byte_idx);
    const int8_t existing = dashboard_reverse_find_slot(model, id, ext);
    if(existing >= 0) {
        model->reverse_byte_mask[(uint8_t)existing] |= byte_bit;
        return existing;
    }

    if(model->reverse_count >= DASH_REVERSE_MAX_IDS) {
        model->reverse_overflow = true;
        return -1;
    }

    const uint8_t idx = model->reverse_count;
    model->reverse_ids[idx] = id;
    model->reverse_ext[idx] = ext;
    model->reverse_byte_mask[idx] = byte_bit;
    model->reverse_count = (uint8_t)(idx + 1U);
    return (int8_t)idx;
}

static bool dashboard_parse_reverse_change(const char* text, uint32_t* out_id, uint8_t* out_byte) {
    if(!text || !out_id || !out_byte) {
        return false;
    }

    unsigned long frame_id = 0UL;
    unsigned long byte_idx = 0UL;
    if(sscanf(text, "change id=0x%lx b%lu", &frame_id, &byte_idx) != 2) {
        return false;
    }

    if(byte_idx > 7UL) {
        return false;
    }

    *out_id = (uint32_t)frame_id;
    *out_byte = (uint8_t)byte_idx;
    return true;
}

static void dashboard_metric_draw_write(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Write Frames");

    char id_text[12] = {0};
    dashboard_format_id(dashboard->write_ext, dashboard->write_id, id_text, sizeof(id_text));

    char line0[40] = {0};
    snprintf(
        line0,
        sizeof(line0),
        "Bus:%s  ID:0x%s",
        cc_bus_to_string(dashboard->write_bus),
        id_text);
    canvas_draw_str(canvas, 2, 19, line0);

    char line1[42] = {0};
    snprintf(
        line1,
        sizeof(line1),
        "Count:%lu  Int:%lums",
        (unsigned long)dashboard->write_count_cfg,
        (unsigned long)dashboard->write_interval_ms_cfg);
    canvas_draw_str(canvas, 2, 28, line1);

    const uint8_t col_x[4] = {2U, 34U, 66U, 98U};
    const uint8_t y_top = 39U;
    const uint8_t y_bottom = 48U;
    for(uint8_t i = 0U; i < 4U; i++) {
        char cell[12] = {0};
        if(i < dashboard->write_dlc) {
            snprintf(cell, sizeof(cell), "%u: %02X", (unsigned)i, dashboard->write_data[i]);
        } else {
            snprintf(cell, sizeof(cell), "%u: --", (unsigned)i);
        }
        canvas_draw_str(canvas, col_x[i], y_top, cell);
    }
    for(uint8_t i = 0U; i < 4U; i++) {
        const uint8_t byte_idx = (uint8_t)(i + 4U);
        char cell[12] = {0};
        if(byte_idx < dashboard->write_dlc) {
            snprintf(
                cell,
                sizeof(cell),
                "%u: %02X",
                (unsigned)byte_idx,
                dashboard->write_data[byte_idx]);
        } else {
            snprintf(cell, sizeof(cell), "%u: --", (unsigned)byte_idx);
        }
        canvas_draw_str(canvas, col_x[i], y_bottom, cell);
    }

    char sent[24] = {0};
    snprintf(sent, sizeof(sent), "Sent Frames: %lu", (unsigned long)dashboard->counter);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 60, sent);
}

static void dashboard_metric_draw_speed(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontSecondary);

    const uint8_t page = (uint8_t)(dashboard->mode_page % 2U);
    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Speed Test");
        if(!dashboard->speed_has_sample) {
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Waiting for sample");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R History");
            return;
        }

        const uint32_t avg = (dashboard->speed_total_samples > 0U) ?
                                 (dashboard->speed_sum_rate / dashboard->speed_total_samples) :
                                 0U;
        char rate[24] = {0};
        char stats[40] = {0};
        snprintf(rate, sizeof(rate), "%lu", (unsigned long)dashboard->speed_last_rate);
        snprintf(
            stats,
            sizeof(stats),
            "%s mn:%lu mx:%lu av:%lu",
            cc_bus_to_string(dashboard->speed_last_bus),
            (unsigned long)dashboard->speed_min_rate,
            (unsigned long)dashboard->speed_max_rate,
            (unsigned long)avg);

        canvas_draw_rframe(canvas, 2, 12, 124, 50, 4);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "Current Rate");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, rate);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "msg/s");
        canvas_set_font(canvas, FontSecondary);
        //canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, stats);
    } else {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Recent Samples");
        if(dashboard->speed_count == 0U) {
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "No samples yet");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Overview");
            return;
        }
        uint8_t selected = dashboard->speed_selected;
        if(selected >= dashboard->speed_count) {
            selected = (uint8_t)(dashboard->speed_count - 1U);
        }

        uint8_t start = 0U;
        if(selected > 1U) {
            start = (uint8_t)(selected - 1U);
        }

        for(uint8_t i = 0U; i < 4U; i++) {
            const uint8_t offset = (uint8_t)(start + i);
            if(offset >= dashboard->speed_count) {
                break;
            }

            const DashboardSpeedSample* sample = dashboard_speed_get(dashboard, offset);
            if(!sample) {
                continue;
            }

            char row[32] = {0};
            snprintf(
                row,
                sizeof(row),
                "%c #%u %s %lu",
                (offset == selected) ? '>' : ' ',
                (unsigned)(offset + 1U),
                cc_bus_to_string(sample->bus),
                (unsigned long)sample->rate);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, (int32_t)(22 + i * 10U), row);
        }
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "U/D Sel  L/R Page");
    }
}

static void dashboard_metric_draw_valtrack(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontSecondary);

    const uint8_t page = (uint8_t)(dashboard->mode_page % 2U);
    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Byte Values (hex/changes)");

        for(uint8_t row = 0U; row < 4U; row++) {
            const uint8_t b0 = (uint8_t)(row * 2U);
            const uint8_t b1 = (uint8_t)(b0 + 1U);
            char v0[10] = {0};
            char v1[10] = {0};
            char line[32] = {0};

            if(dashboard->val_known[b0]) {
                snprintf(v0, sizeof(v0), "%02X/%u", dashboard->val_bytes[b0], dashboard->val_byte_changes[b0]);
            } else {
                snprintf(v0, sizeof(v0), "--/%u", dashboard->val_byte_changes[b0]);
            }

            if(dashboard->val_known[b1]) {
                snprintf(v1, sizeof(v1), "%02X/%u", dashboard->val_bytes[b1], dashboard->val_byte_changes[b1]);
            } else {
                snprintf(v1, sizeof(v1), "--/%u", dashboard->val_byte_changes[b1]);
            }

            snprintf(line, sizeof(line), "B%u:%s  B%u:%s", b0, v0, b1, v1);
            canvas_draw_str(canvas, 2, (int32_t)(22 + row * 10U), line);
        }

        char footer[40] = {0};
        const uint8_t selected = (dashboard->val_selected_byte < 8U) ? dashboard->val_selected_byte : 0U;
        if(dashboard->val_known[selected]) {
            snprintf(
                footer,
                sizeof(footer),
                "Sel B%u=%02X chg:%u",
                selected,
                dashboard->val_bytes[selected],
                dashboard->val_byte_changes[selected]);
        } else {
            snprintf(footer, sizeof(footer), "Sel B%u waiting chg:%u", selected, dashboard->val_byte_changes[selected]);
        }
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, footer);
    } else {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Recent Changes");
        if(dashboard->val_count == 0U) {
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "No changes yet");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Bytes");
            return;
        }
        uint8_t selected = dashboard->val_selected;
        if(selected >= dashboard->val_count) {
            selected = (uint8_t)(dashboard->val_count - 1U);
        }

        uint8_t start = 0U;
        if(selected > 1U) {
            start = (uint8_t)(selected - 1U);
        }

        for(uint8_t i = 0U; i < 4U; i++) {
            const uint8_t offset = (uint8_t)(start + i);
            if(offset >= dashboard->val_count) {
                break;
            }

            const DashboardValChange* change = dashboard_val_get(dashboard, offset);
            if(!change) {
                continue;
            }

            char row[32] = {0};
            snprintf(
                row,
                sizeof(row),
                "%c B%u %02X->%02X",
                (offset == selected) ? '>' : ' ',
                (unsigned)change->byte_idx,
                change->old_value,
                change->new_value);
            canvas_draw_str(canvas, 2, (int32_t)(22 + i * 10U), row);
        }

        char footer[32] = {0};
        snprintf(footer, sizeof(footer), "Total changes: %lu", (unsigned long)dashboard->val_total_changes);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, footer);
    }
}

static void dashboard_metric_draw_unique(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontSecondary);

    const uint8_t page = (uint8_t)(dashboard->mode_page % 2U);
    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Unique IDs");
        canvas_draw_rframe(canvas, 2, 12, 124, 50, 4);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "Unique IDs Discovered");

        char value[24] = {0};
        snprintf(value, sizeof(value), "%lu", (unsigned long)dashboard->unique_total);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, value);

        char footer[40] = {0};
        if(dashboard->unique_has_last) {
            char id_text[12] = {0};
            dashboard_format_id(
                dashboard->unique_last.ext, dashboard->unique_last.id, id_text, sizeof(id_text));
            snprintf(
                footer,
                sizeof(footer),
                "Last ID %s %s %s",
                id_text,
                cc_bus_to_string(dashboard->unique_last.bus),
                dashboard->unique_last.ext ? "EXT" : "STD");
        } else {
            snprintf(footer, sizeof(footer), "Waiting for new IDs");
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, footer);
    } else {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Recent IDs");
        if(dashboard->unique_count == 0U) {
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "No IDs yet");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Overview");
            return;
        }
        uint8_t selected = dashboard->unique_selected;
        if(selected >= dashboard->unique_count) {
            selected = (uint8_t)(dashboard->unique_count - 1U);
        }

        uint8_t start = 0U;
        if(selected > 1U) {
            start = (uint8_t)(selected - 1U);
        }

        for(uint8_t i = 0U; i < 4U; i++) {
            const uint8_t offset = (uint8_t)(start + i);
            if(offset >= dashboard->unique_count) {
                break;
            }

            const DashboardUniqueEntry* entry = dashboard_unique_get(dashboard, offset);
            if(!entry) {
                continue;
            }

            char id_text[12] = {0};
            char row[34] = {0};
            dashboard_format_id(entry->ext, entry->id, id_text, sizeof(id_text));
            snprintf(
                row,
                sizeof(row),
                "%c ID: %s | Bus: %s | %c",
                (offset == selected) ? '>' : ' ',
                id_text,
                cc_bus_to_string(entry->bus),
                entry->ext ? 'E' : 'S');
            canvas_draw_str(canvas, 2, (int32_t)(22 + i * 10U), row);
        }
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "U/D Sel  L/R Page");
    }
}

static void dashboard_metric_draw_dbc(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontSecondary);

    const uint8_t page_count = dashboard_dbc_page_count(dashboard);
    uint8_t page = dashboard->mode_page;
    if(page >= page_count) {
        page = 0U;
    }

    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "DBC Overview");

        if(dashboard->dbc_signal_count == 0U) {
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No registered signals");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Add/load DBC config");
            return;
        }

        uint8_t selected = dashboard->dbc_signal_selected;
        if(selected >= dashboard->dbc_signal_count) {
            selected = (uint8_t)(dashboard->dbc_signal_count - 1U);
        }

        uint8_t start = 0U;
        if(selected > 1U) {
            start = (uint8_t)(selected - 1U);
        }

        for(uint8_t i = 0U; i < 4U; i++) {
            const uint8_t signal_index = (uint8_t)(start + i);
            if(signal_index >= dashboard->dbc_signal_count) {
                break;
            }

            const DashboardDbcEntry* entry = &dashboard->dbc_signals[signal_index];
            const char* signal_name = entry->signal_name[0] ? entry->signal_name : "Signal";
            char signal_short[12] = {0};
            strncpy(signal_short, signal_name, sizeof(signal_short) - 1U);

            char row[34] = {0};
            if(entry->valid) {
                if(entry->mapped && entry->mapped_label[0] != '\0') {
                    char mapped_short[12] = {0};
                    strncpy(mapped_short, entry->mapped_label, sizeof(mapped_short) - 1U);
                    snprintf(
                        row,
                        sizeof(row),
                        "%c %s %s",
                        (signal_index == selected) ? '>' : ' ',
                        signal_short,
                        mapped_short);
                } else {
                    snprintf(
                        row,
                        sizeof(row),
                        "%c %s %.1f%s",
                        (signal_index == selected) ? '>' : ' ',
                        signal_short,
                        (double)entry->value,
                        entry->unit);
                }
            } else {
                snprintf(
                    row,
                    sizeof(row),
                    "%c %s --",
                    (signal_index == selected) ? '>' : ' ',
                    signal_short);
            }
            canvas_draw_str(canvas, 2, (int32_t)(18 + i * 11U), row);
        }

        char footer[40] = {0};
        snprintf(
            footer,
            sizeof(footer),
            "U/D Scroll  L/R Signal  %u/%u",
            (unsigned)(page + 1U),
            (unsigned)page_count);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, footer);
        return;
    }

    const uint8_t signal_index = (uint8_t)(page - 1U);
    if(signal_index >= dashboard->dbc_signal_count) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Invalid signal page");
        return;
    }

    const DashboardDbcEntry* entry = &dashboard->dbc_signals[signal_index];
    char title[24] = {0};
    snprintf(title, sizeof(title), "Signal %u/%u", (unsigned)page, (unsigned)page_count);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, title);

    canvas_draw_rframe(canvas, 2, 12, 124, 50, 4);

    const char* signal_name = entry->signal_name[0] ? entry->signal_name : "Signal";
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, signal_name);

    if(!entry->valid) {
        canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignCenter, "Waiting for data");
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Navigate");
        return;
    }

    if(entry->mapped && entry->mapped_label[0] != '\0') {
        char secondary[40] = {0};
        if(entry->unit[0] != '\0') {
            snprintf(
                secondary,
                sizeof(secondary),
                "raw:%lld val:%.2f %s",
                (long long)entry->raw,
                (double)entry->value,
                entry->unit);
        } else {
            snprintf(
                secondary, sizeof(secondary), "raw:%lld val:%.2f", (long long)entry->raw, (double)entry->value);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, entry->mapped_label);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, secondary);
    } else {
        char value[24] = {0};
        snprintf(value, sizeof(value), "%.2f", (double)entry->value);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, value);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, entry->unit);
    }
}

static void dashboard_metric_draw_reverse(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 2, AlignCenter, AlignTop, dashboard_reverse_phase_text(dashboard->reverse_phase));

    canvas_set_font(canvas, FontSecondary);
    if(dashboard->reverse_phase == DASH_REVERSE_PHASE_CAL) {
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Don't trigger target signals");
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK: Exclude + clear");
        return;
    }

    if(dashboard->reverse_count == 0U) {
        canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignCenter, "No changed IDs yet");
    } else {
        uint8_t selected = dashboard->reverse_selected;
        if(selected >= dashboard->reverse_count) {
            selected = (uint8_t)(dashboard->reverse_count - 1U);
        }

        uint8_t start = 0U;
        if(selected > 1U) {
            start = (uint8_t)(selected - 1U);
        }

        const uint32_t now_ms = furi_get_tick();
        for(uint8_t i = 0U; i < 4U; i++) {
            const uint8_t index = (uint8_t)(start + i);
            if(index >= dashboard->reverse_count) {
                break;
            }

            char id_text[12] = {0};
            char bytes_text[28] = {0};
            char row[56] = {0};
            dashboard_format_id(
                dashboard->reverse_ext[index], dashboard->reverse_ids[index], id_text, sizeof(id_text));
            dashboard_reverse_format_bytes(
                dashboard->reverse_byte_mask[index],
                dashboard->reverse_flash_until_ms[index],
                now_ms,
                bytes_text,
                sizeof(bytes_text));
            snprintf(
                row,
                sizeof(row),
                "%c 0x%s: %s",
                (index == selected) ? '>' : ' ',
                id_text,
                bytes_text);
            canvas_draw_str(canvas, 2, (int32_t)(22 + i * 9U), row);
        }
    }

    if(dashboard->reverse_overflow) {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "Too many changes. Increase calibration");
    } else {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "U/D Scroll  OK Exclude");
    }
}

static bool dashboard_metric_draw_obd(Canvas* canvas, const AppDashboardModel* dashboard) {
    if(!dashboard || dashboard->mode != AppDashboardObdPid || !dashboard->obd_dtc_active) {
        return false;
    }

    static const char* kTypeTitle[3] = {"Stored DTCs", "Pending DTCs", "Permanent DTCs"};
    canvas_set_font(canvas, FontSecondary);

    const uint8_t page = (uint8_t)(dashboard->obd_dtc_page % 4U);
    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "DTC Summary");
        canvas_draw_str(canvas, 2, 20, "Powertrain:");
        canvas_draw_str(canvas, 2, 30, "Body:");
        canvas_draw_str(canvas, 2, 40, "Chassis:");
        canvas_draw_str(canvas, 2, 50, "Network:");

        char value[12] = {0};
        snprintf(value, sizeof(value), "%u", dashboard->obd_dtc_cat_counts[0]);
        canvas_draw_str_aligned(canvas, 124, 20, AlignRight, AlignTop, value);
        snprintf(value, sizeof(value), "%u", dashboard->obd_dtc_cat_counts[1]);
        canvas_draw_str_aligned(canvas, 124, 30, AlignRight, AlignTop, value);
        snprintf(value, sizeof(value), "%u", dashboard->obd_dtc_cat_counts[2]);
        canvas_draw_str_aligned(canvas, 124, 40, AlignRight, AlignTop, value);
        snprintf(value, sizeof(value), "%u", dashboard->obd_dtc_cat_counts[3]);
        canvas_draw_str_aligned(canvas, 124, 50, AlignRight, AlignTop, value);

        canvas_draw_str_aligned(
            canvas,
            64,
            63,
            AlignCenter,
            AlignBottom,
            dashboard->obd_dtc_complete ? "Scan complete  R:Stored" : "Scanning...  R:Stored");
        return true;
    }

    const uint8_t type_index = (uint8_t)(page - 1U);
    uint8_t count = dashboard->obd_dtc_count[type_index];
    uint8_t selected = dashboard->obd_dtc_selected[type_index];
    if(count > 0U && selected >= count) {
        selected = (uint8_t)(count - 1U);
    } else if(count == 0U) {
        selected = 0U;
    }

    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, kTypeTitle[type_index]);

    if(count == 0U) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No DTCs");
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Page");
        return true;
    }

    uint8_t start = 0U;
    if(selected > 1U) {
        start = (uint8_t)(selected - 1U);
    }

    for(uint8_t i = 0U; i < 4U; i++) {
        const uint8_t index = (uint8_t)(start + i);
        if(index >= count) {
            break;
        }

        char row[20] = {0};
        snprintf(
            row,
            sizeof(row),
            "%c %s",
            (index == selected) ? '>' : ' ',
            dashboard->obd_dtc_codes[type_index][index]);
        canvas_draw_str(canvas, 2, (int32_t)(18 + i * 11U), row);
    }

    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "U/D Scroll  L/R Page");
    return true;
}

static void dashboard_metric_draw_replay(Canvas* canvas, const AppDashboardModel* dashboard) {
    /*  Fixed layout (Y positions never change):
     *    Y  2  Title
     *    Y 14  Bus / ID line
     *    Y 24  Frames line
     *    Y 34  State-specific info line
     *    Y 44  State-specific detail line
     *    Y 63  Footer / controls
     */

    /* Row 0 — Title */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "CAN Replay");

    /* Row 1 — Bus / ID (always shown) */
    char id_text[16] = {0};
    if(dashboard->replay_id[0] != '\0') {
        snprintf(id_text, sizeof(id_text), "0x%s", dashboard->replay_id);
    } else {
        strncpy(id_text, "--", sizeof(id_text) - 1U);
    }
    char line_bus[42] = {0};
    snprintf(
        line_bus,
        sizeof(line_bus),
        "Bus:%s  ID:%s %s",
        dashboard->replay_bus[0] ? dashboard->replay_bus : "--",
        id_text,
        dashboard->replay_ext ? "ext" : "std");
    canvas_draw_str(canvas, 2, 19, line_bus);

    /* Row 2 — Frames (always shown) */
    char line_frames[32] = {0};
    snprintf(
        line_frames,
        sizeof(line_frames),
        "Frames: %u/%u",
        (unsigned)dashboard->replay_frames,
        (unsigned)dashboard->replay_max_frames);
    canvas_draw_str(canvas, 2, 29, line_frames);

    /* Row 3+4 — State-specific content */
    if(dashboard->replay_state == 1U) {
        /* RECORDING */
        uint32_t elapsed_ms = 0U;
        if(dashboard->replay_record_start_ms > 0U) {
            elapsed_ms = furi_get_tick() - dashboard->replay_record_start_ms;
        }
        char rec_line[32] = {0};
        snprintf(
            rec_line,
            sizeof(rec_line),
            "** RECORDING **  %lu.%lus",
            (unsigned long)(elapsed_ms / 1000U),
            (unsigned long)((elapsed_ms / 100U) % 10U));
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, rec_line);
        canvas_set_font(canvas, FontSecondary);

        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Press [OK] to Stop");
    } else if(dashboard->replay_state == 2U) {
        /* REPLAYING */
        char progress_line[32] = {0};
        snprintf(
            progress_line,
            sizeof(progress_line),
            "Frame: %u/%u",
            (unsigned)dashboard->replay_progress,
            (unsigned)dashboard->replay_total);
        canvas_draw_str(canvas, 2, 39, progress_line);

        uint32_t total_loops = dashboard->replay_count_cfg;
        uint32_t current_loop = (total_loops >= dashboard->replay_remaining)
                                    ? (total_loops - dashboard->replay_remaining + 1U)
                                    : 1U;
        if(current_loop > total_loops) current_loop = total_loops;
        char loop_line[32] = {0};
        snprintf(
            loop_line,
            sizeof(loop_line),
            "Loop: %lu/%lu",
            (unsigned long)current_loop,
            (unsigned long)total_loops);
        canvas_draw_str(canvas, 2, 49, loop_line);

        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[OK] or [DOWN] to Stop");
    } else {
        /* IDLE */
        if(dashboard->replay_frames > 0U) {
            char loops_line[32] = {0};
            snprintf(
                loops_line,
                sizeof(loops_line),
                "Loops: < %u >",
                (unsigned)dashboard->replay_count_cfg);
            canvas_draw_str(canvas, 2, 39, loops_line);

            canvas_draw_str_aligned(
                canvas, 64, 49, AlignCenter, AlignCenter, "[OK] Play  [UP] Clear");
            canvas_draw_str_aligned(
                canvas, 64, 63, AlignCenter, AlignBottom, "[L/R] Loops");
        } else {
            canvas_draw_str_aligned(
                canvas, 64, 63, AlignCenter, AlignBottom, "Press [OK] to Record");
        }
    }
}

static void dashboard_metric_draw_custom_inject(Canvas* canvas, const AppDashboardModel* dashboard) {
    canvas_set_font(canvas, FontSecondary);

    const uint8_t page = (uint8_t)(dashboard->mode_page % 2U);
    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Custom Inject Slots");

        for(uint8_t i = 0U; i < 5U; i++) {
            const bool used = dashboard->custom_slot_used[i];
            const char* slot_name =
                (dashboard->custom_slot_name[i][0] != '\0') ? dashboard->custom_slot_name[i] : "Slot";
            char row[60] = {0};

            if(used) {
                snprintf(
                    row,
                    sizeof(row),
                    "%cS%u - %s : %s, 0x%lX",
                    (dashboard->custom_selected_slot == i) ? '>' : ' ',
                    (unsigned)(i + 1U),
                    slot_name,
                    cc_bus_to_string(dashboard->custom_slot_bus[i]),
                    (unsigned long)dashboard->custom_slot_id[i]);
            } else {
                snprintf(
                    row,
                    sizeof(row),
                    "%cS%u - %s : <empty>",
                    (dashboard->custom_selected_slot == i) ? '>' : ' ',
                    (unsigned)(i + 1U),
                    slot_name);
            }

            canvas_draw_str(canvas, 2, (int32_t)(18 + i * 9U), row);
        }

        if(dashboard->custom_pending) {
            char footer[42] = {0};
            snprintf(
                footer,
                sizeof(footer),
                "P:S%u r%lu i%lu",
                (unsigned)(dashboard->custom_pending_slot + 1U),
                (unsigned long)dashboard->custom_pending_remaining,
                (unsigned long)dashboard->custom_pending_interval_ms);
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, footer);
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Recent Events");
        if(dashboard->custom_recent_count == 0U) {
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No events yet");
            return;
        }

        for(uint8_t i = 0U; i < 4U; i++) {
            const char* line = dashboard_custom_event_get(dashboard, i);
            if(!line || line[0] == '\0') {
                continue;
            }
            canvas_draw_str(canvas, 2, (int32_t)(18 + i * 11U), line);
        }
        if(dashboard->custom_pending) {
            char footer[42] = {0};
            snprintf(
                footer,
                sizeof(footer),
                "P:S%u r%lu i%lu",
                (unsigned)(dashboard->custom_pending_slot + 1U),
                (unsigned long)dashboard->custom_pending_remaining,
                (unsigned long)dashboard->custom_pending_interval_ms);
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, footer);
        }
    }
}

void dashboard_metric_draw(Canvas* canvas, const AppDashboardModel* dashboard) {
    switch(dashboard->mode) {
    case AppDashboardWrite:
        dashboard_metric_draw_write(canvas, dashboard);
        return;
    case AppDashboardSpeed:
        dashboard_metric_draw_speed(canvas, dashboard);
        return;
    case AppDashboardValtrack:
        dashboard_metric_draw_valtrack(canvas, dashboard);
        return;
    case AppDashboardUniqueIds:
        dashboard_metric_draw_unique(canvas, dashboard);
        return;
    case AppDashboardDbcDecode:
        dashboard_metric_draw_dbc(canvas, dashboard);
        return;
    case AppDashboardReverse:
        dashboard_metric_draw_reverse(canvas, dashboard);
        return;
    case AppDashboardObdPid:
        if(dashboard_metric_draw_obd(canvas, dashboard)) {
            return;
        }
        break;
    case AppDashboardCustomInject:
        dashboard_metric_draw_custom_inject(canvas, dashboard);
        return;
    case AppDashboardReplay:
        dashboard_metric_draw_replay(canvas, dashboard);
        return;
    default:
        break;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        64,
        2,
        AlignCenter,
        AlignTop,
        dashboard->title[0] ? dashboard->title : "CAN Commander");

    canvas_draw_rframe(canvas, 2, 12, 124, 50, 4);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, dashboard->label);

    const bool use_big_value = (strlen(dashboard->value) <= 10U);
    if(use_big_value) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignCenter, dashboard->value);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, dashboard->unit);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, dashboard->value);
        if(dashboard->unit[0] != '\0') {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, dashboard->unit);
        }
    }

    if(dashboard->note[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, dashboard->note);
    }
}

bool dashboard_metric_input(App* app, const InputEvent* event) {
    if(!app || !event) {
        return false;
    }

    if(
        event->type != InputTypePress && event->type != InputTypeRepeat &&
        event->type != InputTypeShort &&
        event->type != InputTypeRelease) {
        return false;
    }

    bool consumed = false;
    uint8_t inject_slot = 0xFFU;
    bool reverse_exclude_pre = false;
    uint8_t replay_action = 0U;
    uint16_t replay_count_for_play = 0U;
    bool reverse_open_bittrack = false;
    bool reverse_open_replay = false;
    uint32_t reverse_selected_id = 0U;
    bool reverse_selected_ext = false;
    bool write_resend = false;
    uint32_t write_counter_before_resend = 0U;
    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            const bool mode_supported =
                (model->mode == AppDashboardWrite || model->mode == AppDashboardSpeed ||
                 model->mode == AppDashboardValtrack ||
                 model->mode == AppDashboardUniqueIds || model->mode == AppDashboardDbcDecode ||
                 model->mode == AppDashboardCustomInject || model->mode == AppDashboardObdPid ||
                 model->mode == AppDashboardReverse || model->mode == AppDashboardReplay);
            if(mode_supported) {
                const uint8_t key_bit = dashboard_input_key_bit(event->key);
                if(event->type == InputTypeRelease) {
                    if(key_bit != 0U) {
                        model->input_hold_mask &= (uint8_t)~key_bit;
                        consumed = true;
                    }
                } else {
                    const bool key_was_held =
                        (key_bit != 0U) && ((model->input_hold_mask & key_bit) != 0U);
                    if(key_bit != 0U) {
                        model->input_hold_mask |= key_bit;
                    }

                    if(model->mode == AppDashboardReplay) {
                        if(!key_was_held) {
                            if(model->replay_state == 0U) {
                                if(event->key == InputKeyOk) {
                                    if(model->replay_frames > 0U) {
                                        replay_count_for_play = model->replay_count_cfg;
                                        replay_action = 1U;
                                    } else {
                                        replay_action = 2U;
                                    }
                                    consumed = true;
                                } else if(
                                    event->key == InputKeyUp && model->replay_frames > 0U) {
                                    replay_action = 3U;
                                    consumed = true;
                                } else if(event->key == InputKeyLeft) {
                                    if(model->replay_count_cfg > 1U) {
                                        model->replay_count_cfg--;
                                    }
                                    consumed = true;
                                } else if(event->key == InputKeyRight) {
                                    if(model->replay_count_cfg < 99U) {
                                        model->replay_count_cfg++;
                                    }
                                    consumed = true;
                                }
                            } else if(model->replay_state == 1U) {
                                if(event->key == InputKeyOk) {
                                    replay_action = 4U;
                                    consumed = true;
                                }
                            } else if(model->replay_state == 2U) {
                                if(event->key == InputKeyOk ||
                                   event->key == InputKeyDown) {
                                    replay_action = 4U;
                                    consumed = true;
                                }
                            }
                        }
                    } else if(
                        model->mode == AppDashboardObdPid && model->obd_dtc_active &&
                        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
                        if(!key_was_held) {
                            if(event->key == InputKeyRight) {
                                model->obd_dtc_page = (uint8_t)((model->obd_dtc_page + 1U) % 4U);
                            } else {
                                model->obd_dtc_page =
                                    (uint8_t)((model->obd_dtc_page == 0U) ? 3U : (model->obd_dtc_page - 1U));
                            }
                            consumed = true;
                        }
                    } else if(
                        model->mode == AppDashboardObdPid && model->obd_dtc_active &&
                        (event->key == InputKeyUp || event->key == InputKeyDown)) {
                        const bool up = (event->key == InputKeyDown);
                        const bool allow_scroll = (!key_was_held || event->type == InputTypeRepeat);
                        if(allow_scroll && model->obd_dtc_page > 0U) {
                            const uint8_t type_index = (uint8_t)(model->obd_dtc_page - 1U);
                            const uint8_t count = model->obd_dtc_count[type_index];
                            uint8_t selected = model->obd_dtc_selected[type_index];
                            if(up && selected + 1U < count) {
                                selected++;
                            } else if(!up && selected > 0U) {
                                selected--;
                            }
                            if(selected != model->obd_dtc_selected[type_index]) {
                                model->obd_dtc_selected[type_index] = selected;
                                consumed = true;
                            } else if(count == 0U) {
                                consumed = true;
                            }
                        }
                    } else if(
                        model->mode == AppDashboardReverse &&
                        (event->key == InputKeyUp || event->key == InputKeyDown)) {
                        const bool up = (event->key == InputKeyDown);
                        const bool allow_scroll = (!key_was_held || event->type == InputTypeRepeat);
                        if(allow_scroll) {
                            uint8_t selected = model->reverse_selected;
                            if(up && selected + 1U < model->reverse_count) {
                                selected++;
                            } else if(!up && selected > 0U) {
                                selected--;
                            }
                            if(selected != model->reverse_selected) {
                                model->reverse_selected = selected;
                            }
                            consumed = true;
                        }
                    } else if(model->mode == AppDashboardReverse && event->key == InputKeyOk) {
                        if(!key_was_held) {
                            dashboard_reverse_clear_changes(model);
                            strncpy(
                                model->note,
                                "Ignore list extended",
                                sizeof(model->note) - 1U);
                            model->note[sizeof(model->note) - 1U] = '\0';
                            reverse_exclude_pre = true;
                            consumed = true;
                        }
                    } else if(
                        model->mode == AppDashboardReverse &&
                        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
                        if(!key_was_held) {
                            if(
                                (event->key == InputKeyRight || event->key == InputKeyLeft) &&
                                model->reverse_phase == DASH_REVERSE_PHASE_MON &&
                                model->reverse_count > 0U) {
                                uint8_t selected = model->reverse_selected;
                                if(selected >= model->reverse_count) {
                                    selected = (uint8_t)(model->reverse_count - 1U);
                                }
                                reverse_selected_id = model->reverse_ids[selected];
                                reverse_selected_ext = model->reverse_ext[selected];
                                if(event->key == InputKeyRight) {
                                    reverse_open_bittrack = true;
                                } else {
                                    reverse_open_replay = true;
                                }
                            }
                            consumed = true;
                        }
                    } else if(
                        model->mode == AppDashboardDbcDecode &&
                        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
                        if(!key_was_held) {
                            const uint8_t page_count = dashboard_dbc_page_count(model);
                            if(event->key == InputKeyRight) {
                                model->mode_page =
                                    (uint8_t)((model->mode_page + 1U >= page_count) ? 0U :
                                                                                      (model->mode_page + 1U));
                            } else {
                                model->mode_page = (uint8_t)(
                                    (model->mode_page == 0U) ? (page_count - 1U) :
                                                               (model->mode_page - 1U));
                            }
                            consumed = true;
                        }
                    } else if(event->key == InputKeyOk && model->mode == AppDashboardWrite) {
                        if(!key_was_held) {
                            write_resend = true;
                            write_counter_before_resend = model->counter;
                            consumed = true;
                        }
                    } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
                        if(!key_was_held) {
                            model->mode_page = (uint8_t)((model->mode_page == 0U) ? 1U : 0U);
                            consumed = true;
                        }
                    } else if(event->key == InputKeyOk && model->mode == AppDashboardCustomInject) {
                        if(!key_was_held) {
                            inject_slot = model->custom_selected_slot;
                            consumed = true;
                        }
                    } else if(event->key == InputKeyOk && model->mode == AppDashboardDbcDecode) {
                        if(!key_was_held) {
                            model->mode_page = 0U;
                            consumed = true;
                        }
                    } else if(event->key == InputKeyOk && model->mode_page == 1U) {
                        if(!key_was_held) {
                            if(model->mode == AppDashboardSpeed) {
                                model->speed_selected = 0U;
                            } else if(model->mode == AppDashboardValtrack) {
                                model->val_selected = 0U;
                            } else if(model->mode == AppDashboardUniqueIds) {
                                model->unique_selected = 0U;
                            } else if(model->mode == AppDashboardCustomInject) {
                                model->custom_selected_slot = app_custom_inject_get_active_slot(app);
                            }
                            consumed = true;
                        }
                    } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
                        const bool up = (event->key == InputKeyDown);
                        const bool allow_scroll = (!key_was_held || event->type == InputTypeRepeat);
                        if(allow_scroll) {
                            if(model->mode == AppDashboardSpeed) {
                                if(model->mode_page == 1U && model->speed_count > 0U) {
                                    if(up && model->speed_selected + 1U < model->speed_count) {
                                        model->speed_selected++;
                                    } else if(!up && model->speed_selected > 0U) {
                                        model->speed_selected--;
                                    }
                                    consumed = true;
                                }
                            } else if(model->mode == AppDashboardValtrack) {
                                if(model->mode_page == 0U) {
                                    if(up && model->val_selected_byte < 7U) {
                                        model->val_selected_byte++;
                                    } else if(!up && model->val_selected_byte > 0U) {
                                        model->val_selected_byte--;
                                    }
                                    consumed = true;
                                } else if(model->val_count > 0U) {
                                    if(up && model->val_selected + 1U < model->val_count) {
                                        model->val_selected++;
                                    } else if(!up && model->val_selected > 0U) {
                                        model->val_selected--;
                                    }
                                    consumed = true;
                                }
                            } else if(model->mode == AppDashboardUniqueIds) {
                                if(model->mode_page == 1U && model->unique_count > 0U) {
                                    if(up && model->unique_selected + 1U < model->unique_count) {
                                        model->unique_selected++;
                                    } else if(!up && model->unique_selected > 0U) {
                                        model->unique_selected--;
                                    }
                                    consumed = true;
                                }
                            } else if(model->mode == AppDashboardDbcDecode) {
                                if(model->mode_page == 0U && model->dbc_signal_count > 0U) {
                                    if(up && model->dbc_signal_selected + 1U < model->dbc_signal_count) {
                                        model->dbc_signal_selected++;
                                    } else if(!up && model->dbc_signal_selected > 0U) {
                                        model->dbc_signal_selected--;
                                    }
                                    consumed = true;
                                }
                            } else if(model->mode == AppDashboardCustomInject) {
                                uint8_t selected = model->custom_selected_slot;
                                if(up && selected < 4U) {
                                    selected++;
                                } else if(!up && selected > 0U) {
                                    selected--;
                                }
                                if(selected != model->custom_selected_slot) {
                                    model->custom_selected_slot = selected;
                                    snprintf(
                                        model->value,
                                        sizeof(model->value),
                                        "%u",
                                        (unsigned)(selected + 1U));
                                    app_custom_inject_set_active_slot(app, selected);
                                    consumed = true;
                                }
                            }
                        }
                    }
                }
            }
        },
        true);

    if(replay_action == 1U) {
        char cmd[48] = {0};
        snprintf(cmd, sizeof(cmd), "cmd=replay count=%u speed=100", (unsigned)replay_count_for_play);
        app_action_tool_config(app, cmd);
    } else if(replay_action == 2U) {
        app_action_tool_config(app, "cmd=record");
    } else if(replay_action == 3U) {
        app_action_tool_config(app, "cmd=clear");
    } else if(replay_action == 4U) {
        app_action_tool_config(app, "cmd=stop");
    }

    if(inject_slot < 5U) {
        app_custom_inject_set_active_slot(app, inject_slot);
        app_action_custom_inject_inject(app, (uint8_t)(inject_slot + 1U));
    }
    if(reverse_exclude_pre) {
        app_action_tool_config(app, "exclude_pre=1");
    }
    if(reverse_open_bittrack) {
        char bus_name[16] = {0};
        if(!dashboard_arg_get_value_last(app->args_reverse_auto, "bus", bus_name, sizeof(bus_name))) {
            strncpy(bus_name, "can0", sizeof(bus_name) - 1U);
        }
        if(
            strcmp(bus_name, "can0") != 0 && strcmp(bus_name, "can1") != 0 &&
            strcmp(bus_name, "both") != 0) {
            strncpy(bus_name, "can0", sizeof(bus_name) - 1U);
        }

        char id_text[12] = {0};
        if(reverse_selected_ext) {
            snprintf(id_text, sizeof(id_text), "%08lX", (unsigned long)reverse_selected_id);
        } else {
            snprintf(id_text, sizeof(id_text), "%03lX", (unsigned long)(reverse_selected_id & 0x7FFU));
        }

        snprintf(
            app->args_bittrack,
            sizeof(app->args_bittrack),
            "bus=%s id=%s ext=%u",
            bus_name,
            id_text,
            reverse_selected_ext ? 1U : 0U);
        app->args_bittrack[sizeof(app->args_bittrack) - 1U] = '\0';

        app_action_tool_stop(app);
        app_action_tool_start(app, CcToolBittrack, app->args_bittrack, "bittrack");
    }
    if(reverse_open_replay) {
        char bus_name[16] = {0};
        if(!dashboard_arg_get_value_last(app->args_reverse_auto, "bus", bus_name, sizeof(bus_name))) {
            strncpy(bus_name, "can0", sizeof(bus_name) - 1U);
        }
        if(
            strcmp(bus_name, "can0") != 0 && strcmp(bus_name, "can1") != 0 &&
            strcmp(bus_name, "both") != 0) {
            strncpy(bus_name, "can0", sizeof(bus_name) - 1U);
        }

        char id_text[12] = {0};
        if(reverse_selected_ext) {
            snprintf(id_text, sizeof(id_text), "%08lX", (unsigned long)reverse_selected_id);
        } else {
            snprintf(id_text, sizeof(id_text), "%03lX", (unsigned long)(reverse_selected_id & 0x7FFU));
        }

        snprintf(
            app->args_replay,
            sizeof(app->args_replay),
            "bus=%s id=%s ext=%u",
            bus_name,
            id_text,
            reverse_selected_ext ? 1U : 0U);
        app->args_replay[sizeof(app->args_replay) - 1U] = '\0';

        app_action_tool_stop(app);
        app_action_tool_start(app, CcToolReplay, app->args_replay, "replay");
    }
    if(write_resend) {
        app_action_tool_start(app, CcToolWrite, app->args_write_tool, "write");
        if(app->connected) {
            with_view_model(
                app->dashboard_view,
                AppDashboardModel * model,
                {
                    model->counter = write_counter_before_resend;
                    snprintf(model->value, sizeof(model->value), "%lu", (unsigned long)model->counter);
                    model->value[sizeof(model->value) - 1U] = '\0';
                },
                true);
        }
    }

    return consumed;
}

void dashboard_update_obd(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolObdPid) {
        return;
    }

    char text[sizeof(event->data.tool.text)] = {0};
    strncpy(text, event->data.tool.text, sizeof(text) - 1U);
    dashboard_trim_inplace(text);

    const bool is_obd_request_sent = dashboard_starts_with(text, "obd request sent:");
    const bool is_dtc_request_sent = is_obd_request_sent && strstr(text, "dtc") != NULL;
    const bool is_non_dtc_request_sent = is_obd_request_sent && !is_dtc_request_sent;
    const bool is_dtc_all_complete = dashboard_ieq(text, "dtc_all complete");
    const int8_t dtc_type = dashboard_obd_dtc_type_from_text(text);

    if(is_dtc_request_sent || dtc_type >= 0 || is_dtc_all_complete) {
        with_view_model(
            app->dashboard_view,
            AppDashboardModel * model,
            {
                model->obd_dtc_active = true;

                if(is_dtc_request_sent) {
                    dashboard_obd_dtc_clear_all(model);
                    strncpy(model->title, "OBD DTC", sizeof(model->title) - 1U);
                    model->title[sizeof(model->title) - 1U] = '\0';
                    strncpy(model->label, "DTC Scan", sizeof(model->label) - 1U);
                    model->label[sizeof(model->label) - 1U] = '\0';
                    strncpy(model->value, "Running", sizeof(model->value) - 1U);
                    model->value[sizeof(model->value) - 1U] = '\0';
                    model->unit[0] = '\0';
                    strncpy(model->note, "Scanning stored/pending/permanent", sizeof(model->note) - 1U);
                    model->note[sizeof(model->note) - 1U] = '\0';
                }

                if(dtc_type >= 0) {
                    const uint8_t type_index = (uint8_t)dtc_type;
                    const char* marker = strstr(text, "DTCs:");
                    const char* right = marker ? (marker + 5) : NULL;
                    if(right) {
                        while(*right == ' ' || *right == '\t') {
                            right++;
                        }

                        if(*right == '\0' || dashboard_ieq(right, "none")) {
                            dashboard_obd_dtc_clear_type(model, type_index);
                        } else if(right[0] != '+') {
                            char scratch[96] = {0};
                            strncpy(scratch, right, sizeof(scratch) - 1U);
                            char* save_ptr = NULL;
                            char* token = strtok_r(scratch, " ", &save_ptr);
                            while(token) {
                                if(dashboard_is_dtc_code_token(token)) {
                                    (void)dashboard_obd_dtc_add_unique(model, type_index, token);
                                }
                                token = strtok_r(NULL, " ", &save_ptr);
                            }
                        }

                        if(model->obd_dtc_selected[type_index] >= model->obd_dtc_count[type_index]) {
                            model->obd_dtc_selected[type_index] =
                                (model->obd_dtc_count[type_index] == 0U) ?
                                    0U :
                                    (uint8_t)(model->obd_dtc_count[type_index] - 1U);
                        }
                        dashboard_obd_dtc_recompute_categories(model);
                    }
                }

                if(is_dtc_all_complete) {
                    model->obd_dtc_complete = true;
                    strncpy(model->note, "DTC scan complete", sizeof(model->note) - 1U);
                    model->note[sizeof(model->note) - 1U] = '\0';
                }
            },
            false);
        return;
    }

    if(is_non_dtc_request_sent) {
        with_view_model(
            app->dashboard_view,
            AppDashboardModel * model,
            {
                model->obd_dtc_active = false;
                model->obd_dtc_complete = false;
                model->obd_dtc_page = 0U;
            },
            false);
    }

    char label[32] = {0};
    char value[24] = {0};
    char unit[24] = {0};
    char note[40] = {0};

    if(event->data.tool.code == CcToolEvtWarning || event->data.tool.code == CcToolEvtError) {
        strncpy(label, "OBD Diagnostic", sizeof(label) - 1U);
        strncpy(
            value,
            (event->data.tool.code == CcToolEvtWarning) ? "Warning" : "Error",
            sizeof(value) - 1U);
        strncpy(note, text[0] ? text : "No detail", sizeof(note) - 1U);
    } else {
        if(!dashboard_parse_obd_dtc_line(
               text, label, sizeof(label), value, sizeof(value), note, sizeof(note))) {
            char* colon = strchr(text, ':');
            if(colon) {
                *colon = '\0';
                strncpy(label, text, sizeof(label) - 1U);
                dashboard_trim_inplace(label);

                char right[64] = {0};
                strncpy(right, colon + 1, sizeof(right) - 1U);
                dashboard_trim_inplace(right);

                char first[24] = {0};
                char rest[24] = {0};
                const int parsed = sscanf(right, "%23s %23[^\n]", first, rest);
                if(parsed >= 1 && dashboard_is_numeric_token(first)) {
                    strncpy(value, first, sizeof(value) - 1U);
                    if(parsed == 2) {
                        strncpy(unit, rest, sizeof(unit) - 1U);
                    }
                } else {
                    strncpy(value, right, sizeof(value) - 1U);
                }
            } else {
                strncpy(label, "OBD PID", sizeof(label) - 1U);
                strncpy(value, text[0] ? text : "--", sizeof(value) - 1U);
            }
        }
    }

    if(label[0] == '\0') {
        strncpy(label, "OBD PID", sizeof(label) - 1U);
    }
    if(value[0] == '\0') {
        strncpy(value, "--", sizeof(value) - 1U);
    }

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, "OBD PID", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, label, sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';
            strncpy(model->value, value, sizeof(model->value) - 1U);
            model->value[sizeof(model->value) - 1U] = '\0';
            strncpy(model->unit, unit, sizeof(model->unit) - 1U);
            model->unit[sizeof(model->unit) - 1U] = '\0';
            strncpy(model->note, note, sizeof(model->note) - 1U);
            model->note[sizeof(model->note) - 1U] = '\0';
        },
        false);
}

void dashboard_update_write(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolWrite) {
        return;
    }

    const char* text = event->data.tool.text;
    char bus_name[16] = {0};
    unsigned long frame_id = 0UL;
    unsigned long dlc = 0UL;
    const bool parsed_tx =
        (sscanf(text, "TX sent bus=%15s id=0x%lx dlc=%lu", bus_name, &frame_id, &dlc) == 3);

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            dashboard_update_write_cfg_from_args(model, app->args_write_tool);

            strncpy(model->title, "WRITE", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, "Sent", sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';
            snprintf(model->value, sizeof(model->value), "%lu", (unsigned long)model->counter);
            model->value[sizeof(model->value) - 1U] = '\0';
            model->unit[0] = '\0';

            if(event->data.tool.code == CcToolEvtError) {
                strncpy(model->note, text, sizeof(model->note) - 1U);
            } else if(parsed_tx || strncmp(text, "TX sent", 7) == 0) {
                model->counter++;
                snprintf(model->value, sizeof(model->value), "%lu", (unsigned long)model->counter);
                if(parsed_tx) {
                    model->write_bus = dashboard_parse_bus(bus_name);
                    model->write_id = (uint32_t)frame_id;
                    if(dlc <= 8UL) {
                        model->write_dlc = (uint8_t)dlc;
                    }
                }
                strncpy(model->note, text, sizeof(model->note) - 1U);
            } else {
                strncpy(model->note, text, sizeof(model->note) - 1U);
            }

            model->note[sizeof(model->note) - 1U] = '\0';
        },
        false);
}

void dashboard_update_speed(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolSpeed) {
        return;
    }

    const char* text = event->data.tool.text;
    char bus_name[16] = {0};
    unsigned long rate = 0;
    const bool parsed = sscanf(text, "speed bus=%15s %lu msg/sec", bus_name, &rate) == 2;

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, "SPEED TEST", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, "Rate", sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';

            if(parsed) {
                const uint32_t rate_u32 = (uint32_t)rate;
                const CcBus bus = dashboard_parse_bus(bus_name);
                model->speed_last_bus = bus;
                model->speed_last_rate = rate_u32;

                if(!model->speed_has_sample) {
                    model->speed_min_rate = rate_u32;
                    model->speed_max_rate = rate_u32;
                    model->speed_sum_rate = 0U;
                    model->speed_total_samples = 0U;
                }

                model->speed_has_sample = true;
                if(rate_u32 < model->speed_min_rate) {
                    model->speed_min_rate = rate_u32;
                }
                if(rate_u32 > model->speed_max_rate) {
                    model->speed_max_rate = rate_u32;
                }
                model->speed_sum_rate += rate_u32;
                model->speed_total_samples++;

                DashboardSpeedSample* slot = &model->speed_samples[model->speed_head];
                memset(slot, 0, sizeof(DashboardSpeedSample));
                slot->valid = true;
                slot->bus = bus;
                slot->rate = rate_u32;
                model->speed_head = (uint8_t)((model->speed_head + 1U) % DASH_SPEED_HISTORY);
                if(model->speed_count < DASH_SPEED_HISTORY) {
                    model->speed_count++;
                }
                if(model->speed_selected >= model->speed_count) {
                    model->speed_selected = 0U;
                }

                snprintf(model->value, sizeof(model->value), "%lu", (unsigned long)rate);
                strncpy(model->unit, "msg/s", sizeof(model->unit) - 1U);
                model->unit[sizeof(model->unit) - 1U] = '\0';
                snprintf(model->note, sizeof(model->note), "Bus: %s", bus_name);
            } else {
                strncpy(model->note, text, sizeof(model->note) - 1U);
                model->note[sizeof(model->note) - 1U] = '\0';
            }
        },
        false);
}

void dashboard_update_valtrack(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolValtrack) {
        return;
    }

    const char* text = event->data.tool.text;
    unsigned long byte_idx = 0;
    unsigned long old_hex = 0;
    unsigned long old_dec = 0;
    unsigned long new_hex = 0;
    unsigned long new_dec = 0;
    const bool changed = sscanf(
                             text,
                             "byte %lu changed 0x%lx(%lu)->0x%lx(%lu)",
                             &byte_idx,
                             &old_hex,
                             &old_dec,
                             &new_hex,
                             &new_dec) == 5;

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, "VAL TRACK", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';

            if(changed && byte_idx < 8UL) {
                const uint8_t b = (uint8_t)byte_idx;
                const uint8_t old_u8 = (uint8_t)old_hex;
                const uint8_t new_u8 = (uint8_t)new_hex;

                model->val_known[b] = true;
                model->val_bytes[b] = new_u8;
                model->val_byte_changes[b]++;
                model->val_total_changes++;

                DashboardValChange* slot = &model->val_changes[model->val_head];
                memset(slot, 0, sizeof(DashboardValChange));
                slot->valid = true;
                slot->byte_idx = b;
                slot->old_value = old_u8;
                slot->new_value = new_u8;
                model->val_head = (uint8_t)((model->val_head + 1U) % DASH_VAL_HISTORY);
                if(model->val_count < DASH_VAL_HISTORY) {
                    model->val_count++;
                }
                if(model->val_selected >= model->val_count) {
                    model->val_selected = 0U;
                }

                snprintf(model->label, sizeof(model->label), "Byte %lu", byte_idx);
                snprintf(model->value, sizeof(model->value), "%lu", new_dec);
                strncpy(model->unit, "dec", sizeof(model->unit) - 1U);
                snprintf(model->note, sizeof(model->note), "0x%02lX -> 0x%02lX", old_hex, new_hex);
            } else {
                strncpy(model->label, "Status", sizeof(model->label) - 1U);
                strncpy(
                    model->value,
                    event->data.tool.code == CcToolEvtWarning ? "Warning" : "--",
                    sizeof(model->value) - 1U);
                model->unit[0] = '\0';
                strncpy(model->note, text, sizeof(model->note) - 1U);
            }

            model->label[sizeof(model->label) - 1U] = '\0';
            model->value[sizeof(model->value) - 1U] = '\0';
            model->unit[sizeof(model->unit) - 1U] = '\0';
            model->note[sizeof(model->note) - 1U] = '\0';
        },
        false);
}

void dashboard_update_unique_ids(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolUniqueIds) {
        return;
    }

    const char* text = event->data.tool.text;
    unsigned long count = 0;
    char bus_name[8] = {0};
    unsigned long frame_id = 0;
    char ext_std[8] = {0};
    const bool parsed_count = sscanf(text, "unique ids count=%lu", &count) == 1;
    const bool parsed_delta =
        sscanf(text, "new id bus=%7s id=0x%lx %7s", bus_name, &frame_id, ext_std) == 3;

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, "UNIQUE IDS", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, "Found", sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';
            strncpy(model->unit, "ids", sizeof(model->unit) - 1U);
            model->unit[sizeof(model->unit) - 1U] = '\0';

            if(parsed_count) {
                model->unique_total = (uint32_t)count;
            } else if(parsed_delta) {
                DashboardUniqueEntry* slot = &model->unique_entries[model->unique_head];
                memset(slot, 0, sizeof(DashboardUniqueEntry));
                slot->valid = true;
                slot->bus = dashboard_parse_bus(bus_name);
                slot->id = (uint32_t)frame_id;
                slot->ext = (ext_std[0] == 'E' || ext_std[0] == 'e');

                model->unique_last = *slot;
                model->unique_has_last = true;
                model->unique_head = (uint8_t)((model->unique_head + 1U) % DASH_UNIQUE_HISTORY);
                if(model->unique_count < DASH_UNIQUE_HISTORY) {
                    model->unique_count++;
                }
                if(model->unique_selected >= model->unique_count) {
                    model->unique_selected = 0U;
                }

                if(model->unique_total < 0xFFFFFFFFUL) {
                    model->unique_total++;
                }
            } else {
                strncpy(model->note, text, sizeof(model->note) - 1U);
            }

            model->counter = model->unique_total;
            snprintf(model->value, sizeof(model->value), "%lu", (unsigned long)model->unique_total);

            if(parsed_delta) {
                char id_text[12] = {0};
                dashboard_format_id(
                    model->unique_last.ext, model->unique_last.id, id_text, sizeof(id_text));
                snprintf(
                    model->note,
                    sizeof(model->note),
                    "Last: %s %s %s",
                    id_text,
                    cc_bus_to_string(model->unique_last.bus),
                    model->unique_last.ext ? "EXT" : "STD");
            }
            model->note[sizeof(model->note) - 1U] = '\0';
        },
        false);
}

void dashboard_update_reverse(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolReverse) {
        return;
    }

    const char* text = event->data.tool.text;
    uint32_t change_id = 0U;
    uint8_t change_byte = 0U;
    const bool is_change = dashboard_parse_reverse_change(text, &change_id, &change_byte);
    const bool is_calibration_start = strstr(text, "calibration start") != NULL;
    const bool is_monitoring_start = strstr(text, "monitoring start") != NULL;
    const bool is_monitoring_complete = strstr(text, "monitoring complete") != NULL;
    const bool is_read_mode = strstr(text, "read mode") != NULL;
    const bool is_exclude_summary = strstr(text, "exclude_pre applied") != NULL;

    // Ignore high-volume calibration summary lines to avoid UI stalls on busy buses.
    if(
        !is_change && !is_calibration_start && !is_monitoring_start && !is_monitoring_complete &&
        !is_read_mode && !is_exclude_summary) {
        return;
    }

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, "Auto Reverse Engineer", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, "Phase", sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';

            if(is_calibration_start) {
                model->reverse_phase = DASH_REVERSE_PHASE_CAL;
                dashboard_reverse_clear_changes(model);
            } else if(is_monitoring_start) {
                model->reverse_phase = DASH_REVERSE_PHASE_MON;
                dashboard_reverse_clear_changes(model);
            } else if(is_monitoring_complete) {
                model->reverse_phase = DASH_REVERSE_PHASE_DONE;
            } else if(is_read_mode) {
                model->reverse_phase = DASH_REVERSE_PHASE_READ;
            } else if(is_change) {
                model->counter++;
                const int8_t slot_index = dashboard_reverse_add_or_update(
                    model, change_id, (change_id > 0x7FFU), change_byte);
                if(slot_index >= 0) {
                    model->reverse_flash_until_ms[(uint8_t)slot_index][change_byte] =
                        furi_get_tick() + DASH_REVERSE_FLASH_MS;
                }
                if(model->reverse_selected >= model->reverse_count && model->reverse_count > 0U) {
                    model->reverse_selected = (uint8_t)(model->reverse_count - 1U);
                }
            }

            strncpy(
                model->value,
                dashboard_reverse_phase_text(model->reverse_phase),
                sizeof(model->value) - 1U);
            model->value[sizeof(model->value) - 1U] = '\0';
            model->unit[0] = '\0';
            strncpy(model->note, text, sizeof(model->note) - 1U);
            model->note[sizeof(model->note) - 1U] = '\0';
        },
        false);
}

void dashboard_update_dbc_decode(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeDbcDecode) {
        return;
    }

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            DashboardDbcEntry* slot = &model->dbc_entries[model->dbc_head];
            memset(slot, 0, sizeof(DashboardDbcEntry));
            slot->valid = true;
            slot->sid = event->data.dbc_decode.sid;
            slot->bus = event->data.dbc_decode.bus;
            slot->frame_id = event->data.dbc_decode.frame_id;
            slot->raw = event->data.dbc_decode.raw;
            slot->value = event->data.dbc_decode.value;
            slot->in_range = event->data.dbc_decode.in_range;
            strncpy(slot->unit, event->data.dbc_decode.unit, sizeof(slot->unit) - 1U);
            slot->unit[sizeof(slot->unit) - 1U] = '\0';
            const char* signal_name = app_dbc_config_lookup_signal_name(app, slot->sid);
            if(signal_name && signal_name[0] != '\0') {
                strncpy(slot->signal_name, signal_name, sizeof(slot->signal_name) - 1U);
                slot->signal_name[sizeof(slot->signal_name) - 1U] = '\0';
            } else {
                snprintf(slot->signal_name, sizeof(slot->signal_name), "SID%u", (unsigned)slot->sid);
                slot->signal_name[sizeof(slot->signal_name) - 1U] = '\0';
            }
            const char* mapped_label =
                app_dbc_config_lookup_label(app, slot->sid, event->data.dbc_decode.raw);
            if(mapped_label && mapped_label[0] != '\0') {
                slot->mapped = true;
                strncpy(slot->mapped_label, mapped_label, sizeof(slot->mapped_label) - 1U);
                slot->mapped_label[sizeof(slot->mapped_label) - 1U] = '\0';
            } else {
                slot->mapped = false;
                slot->mapped_label[0] = '\0';
            }

            model->dbc_latest = *slot;
            model->dbc_has_latest = true;
            model->dbc_head = (uint8_t)((model->dbc_head + 1U) % DASH_DBC_HISTORY);
            if(model->dbc_count < DASH_DBC_HISTORY) {
                model->dbc_count++;
            }
            if(model->dbc_selected >= model->dbc_count) {
                model->dbc_selected = 0U;
            }

            dashboard_dbc_sync_registered(model, app);
            const int8_t signal_slot = dashboard_dbc_find_signal_slot(model, slot->sid);
            if(signal_slot >= 0) {
                model->dbc_signals[(uint8_t)signal_slot] = *slot;
            }

            strncpy(model->title, "DBC DECODE", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, slot->signal_name, sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';
            if(slot->mapped && slot->mapped_label[0] != '\0') {
                strncpy(model->value, slot->mapped_label, sizeof(model->value) - 1U);
                model->value[sizeof(model->value) - 1U] = '\0';
                snprintf(model->unit, sizeof(model->unit), "raw:%lld", (long long)slot->raw);
            } else {
                snprintf(model->value, sizeof(model->value), "%.2f", (double)event->data.dbc_decode.value);
                strncpy(model->unit, event->data.dbc_decode.unit, sizeof(model->unit) - 1U);
                model->unit[sizeof(model->unit) - 1U] = '\0';
            }
            if(slot->mapped) {
                snprintf(
                    model->note,
                    sizeof(model->note),
                    "%s 0x%lX raw:%lld",
                    cc_bus_to_string(event->data.dbc_decode.bus),
                    (unsigned long)event->data.dbc_decode.frame_id,
                    (long long)slot->raw);
            } else {
                snprintf(
                    model->note,
                    sizeof(model->note),
                    "%s 0x%lX %s",
                    cc_bus_to_string(event->data.dbc_decode.bus),
                    (unsigned long)event->data.dbc_decode.frame_id,
                    event->data.dbc_decode.in_range ? "ok" : "oor");
            }
        },
        false);
}

static void dashboard_custom_push_event(AppDashboardModel* model, const char* text) {
    if(!model || !text) {
        return;
    }

    char* slot = model->custom_recent[model->custom_recent_head];
    strncpy(slot, text, 47U);
    slot[47U] = '\0';

    model->custom_recent_head = (uint8_t)((model->custom_recent_head + 1U) % DASH_CUSTOM_HISTORY);
    if(model->custom_recent_count < DASH_CUSTOM_HISTORY) {
        model->custom_recent_count++;
    }
}

void dashboard_update_custom_inject(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolCustomInject) {
        return;
    }

    char text[sizeof(event->data.tool.text)] = {0};
    strncpy(text, event->data.tool.text, sizeof(text) - 1U);
    dashboard_trim_inplace(text);
    if(text[0] == '\0') {
        strncpy(text, "(empty)", sizeof(text) - 1U);
    }

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            if(model->custom_selected_slot >= 5U) {
                model->custom_selected_slot = 0U;
            }

            for(uint8_t i = 0U; i < 5U; i++) {
                dashboard_custom_slot_name_from_app(
                    app, i, model->custom_slot_name[i], sizeof(model->custom_slot_name[i]));
            }

            dashboard_custom_push_event(model, text);

            unsigned slot = 0U;
            char bus_name[16] = {0};
            unsigned long frame_id = 0;
            char ext_std[8] = {0};
            unsigned latest = 0U;
            unsigned long count = 0UL;
            unsigned long interval_ms = 0UL;
            unsigned long remaining = 0UL;

            if(sscanf(text, "slot=%u tracking bus=%15s id=0x%lx %7s", &slot, bus_name, &frame_id, ext_std) == 4 &&
               slot >= 1U && slot <= 5U) {
                const uint8_t idx = (uint8_t)(slot - 1U);
                model->custom_slot_used[idx] = true;
                model->custom_slot_ready[idx] = false;
                model->custom_slot_bus[idx] = dashboard_parse_bus(bus_name);
                model->custom_slot_id[idx] = (uint32_t)frame_id;
                model->custom_slot_ext[idx] = (ext_std[0] == 'e' || ext_std[0] == 'E');
            } else if(
                sscanf(
                    text,
                    "slot=%u bus=%15s id=0x%lx %7s latest=%u",
                    &slot,
                    bus_name,
                    &frame_id,
                    ext_std,
                    &latest) == 5 &&
                slot >= 1U && slot <= 5U) {
                const uint8_t idx = (uint8_t)(slot - 1U);
                model->custom_slot_used[idx] = true;
                model->custom_slot_ready[idx] = (latest != 0U);
                model->custom_slot_bus[idx] = dashboard_parse_bus(bus_name);
                model->custom_slot_id[idx] = (uint32_t)frame_id;
                model->custom_slot_ext[idx] = (ext_std[0] == 'e' || ext_std[0] == 'E');
            } else if(sscanf(text, "slot=%u removed", &slot) == 1 && slot >= 1U && slot <= 5U) {
                const uint8_t idx = (uint8_t)(slot - 1U);
                model->custom_slot_used[idx] = false;
                model->custom_slot_ready[idx] = false;
                model->custom_slot_id[idx] = 0U;
                model->custom_slot_ext[idx] = false;
            } else if(
                sscanf(
                    text,
                    "inject scheduled slot=%u count=%lu interval_ms=%lu",
                    &slot,
                    &count,
                    &interval_ms) == 3 &&
                slot >= 1U && slot <= 5U) {
                model->custom_pending = true;
                model->custom_pending_slot = (uint8_t)(slot - 1U);
                model->custom_pending_remaining = (uint32_t)count;
                model->custom_pending_interval_ms = (uint32_t)interval_ms;
                model->custom_selected_slot = (uint8_t)(slot - 1U);
                app_custom_inject_set_active_slot(app, model->custom_selected_slot);
            } else if(
                sscanf(
                    text,
                    "pending slot=%u remaining=%lu interval_ms=%lu",
                    &slot,
                    &remaining,
                    &interval_ms) == 3 &&
                slot >= 1U && slot <= 5U) {
                model->custom_pending = true;
                model->custom_pending_slot = (uint8_t)(slot - 1U);
                model->custom_pending_remaining = (uint32_t)remaining;
                model->custom_pending_interval_ms = (uint32_t)interval_ms;
                model->custom_selected_slot = (uint8_t)(slot - 1U);
                app_custom_inject_set_active_slot(app, model->custom_selected_slot);
            } else if(
                sscanf(
                    text,
                    "inject tx slot=%u bus=%15s id=0x%lx remaining=%lu",
                    &slot,
                    bus_name,
                    &frame_id,
                    &remaining) == 4 &&
                slot >= 1U && slot <= 5U) {
                const uint8_t idx = (uint8_t)(slot - 1U);
                model->custom_slot_used[idx] = true;
                model->custom_slot_ready[idx] = true;
                model->custom_slot_bus[idx] = dashboard_parse_bus(bus_name);
                model->custom_slot_id[idx] = (uint32_t)frame_id;
                model->custom_pending_slot = idx;
                model->custom_pending_remaining = (uint32_t)remaining;
                model->custom_selected_slot = idx;
                app_custom_inject_set_active_slot(app, idx);
            } else if(sscanf(text, "custom_inject slots_used=%lu", &count) == 1) {
                model->counter = (uint32_t)count;
            } else if(strstr(text, "inject sequence complete") || strstr(text, "pending injection canceled") ||
                      strstr(text, "inject aborted")) {
                model->custom_pending = false;
                model->custom_pending_remaining = 0U;
            } else if(strstr(text, "all slots cleared")) {
                memset(model->custom_slot_used, 0, sizeof(model->custom_slot_used));
                memset(model->custom_slot_ready, 0, sizeof(model->custom_slot_ready));
                memset(model->custom_slot_id, 0, sizeof(model->custom_slot_id));
                memset(model->custom_slot_ext, 0, sizeof(model->custom_slot_ext));
                model->custom_pending = false;
                model->custom_pending_remaining = 0U;
            }

            strncpy(model->title, "CUSTOM INJECT", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            strncpy(model->label, "Slot", sizeof(model->label) - 1U);
            model->label[sizeof(model->label) - 1U] = '\0';
            snprintf(model->value, sizeof(model->value), "%u", (unsigned)(model->custom_selected_slot + 1U));
            model->unit[0] = '\0';
            strncpy(model->note, text, sizeof(model->note) - 1U);
            model->note[sizeof(model->note) - 1U] = '\0';
        },
        false);
}

void dashboard_update_replay(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolReplay) {
        return;
    }

    char text[sizeof(event->data.tool.text)] = {0};
    strncpy(text, event->data.tool.text, sizeof(text) - 1U);
    dashboard_trim_inplace(text);

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            char bus_name[16] = {0};
            unsigned long frame_id = 0;
            char ext_std[8] = {0};
            unsigned max_frames = 0U;
            unsigned frame_count = 0U;
            unsigned total_frames = 0U;
            unsigned count = 0U;
            unsigned speed = 0U;
            unsigned progress = 0U;
            unsigned remaining = 0U;
            unsigned fr_used = 0U;
            unsigned fr_max = 0U;
            char state_str[16] = {0};

            if(sscanf(
                   text,
                   "ready bus=%15s id=0x%lx %7s (max %u frames)",
                   bus_name,
                   &frame_id,
                   ext_std,
                   &max_frames) == 4) {
                model->replay_state = 0U;
                strncpy(model->replay_bus, bus_name, sizeof(model->replay_bus) - 1U);
                model->replay_bus[sizeof(model->replay_bus) - 1U] = '\0';
                if(ext_std[0] == 'e' || ext_std[0] == 'E') {
                    snprintf(
                        model->replay_id,
                        sizeof(model->replay_id),
                        "%08lX",
                        (unsigned long)frame_id);
                    model->replay_ext = true;
                } else {
                    snprintf(
                        model->replay_id,
                        sizeof(model->replay_id),
                        "%03lX",
                        (unsigned long)(frame_id & 0x7FFU));
                    model->replay_ext = false;
                }
                model->replay_max_frames = (uint16_t)max_frames;
                model->replay_frames = 0U;
            } else if(strstr(text, "recording started")) {
                model->replay_state = 1U;
                model->replay_frames = 0U;
                model->replay_record_start_ms = furi_get_tick();
            } else if(
                sscanf(text, "recording stopped frames=%u", &frame_count) == 1) {
                model->replay_state = 0U;
                model->replay_frames = (uint16_t)frame_count;
            } else if(
                sscanf(text, "buffer full (%u frames)", &frame_count) == 1) {
                model->replay_state = 0U;
                model->replay_frames = (uint16_t)frame_count;
            } else if(
                sscanf(
                    text,
                    "replay started frames=%u count=%u speed=%u",
                    &total_frames,
                    &count,
                    &speed) == 3) {
                model->replay_state = 2U;
                model->replay_total = (uint16_t)total_frames;
                model->replay_remaining = (uint32_t)count;
                model->replay_progress = 0U;
            } else if(strstr(text, "replay complete")) {
                model->replay_state = 0U;
            } else if(
                sscanf(
                    text,
                    "state=%15s frames=%u/%u",
                    state_str,
                    &fr_used,
                    &fr_max) == 3) {
                model->replay_frames = (uint16_t)fr_used;
                model->replay_max_frames = (uint16_t)fr_max;
                if(dashboard_ieq(state_str, "idle")) {
                    model->replay_state = 0U;
                } else if(dashboard_ieq(state_str, "recording")) {
                    model->replay_state = 1U;
                } else if(dashboard_ieq(state_str, "replaying")) {
                    model->replay_state = 2U;
                }
            } else if(
                sscanf(
                    text,
                    "progress=%u/%u remaining_loops=%u",
                    &progress,
                    &total_frames,
                    &remaining) == 3) {
                model->replay_progress = (uint16_t)progress;
                model->replay_total = (uint16_t)total_frames;
                model->replay_remaining = (uint32_t)remaining;
            } else if(strstr(text, "recording cleared")) {
                model->replay_frames = 0U;
                model->replay_state = 0U;
            } else if(strstr(text, "frames=")) {
                unsigned f = 0U;
                if(sscanf(strstr(text, "frames="), "frames=%u", &f) == 1) {
                    model->replay_frames = (uint16_t)f;
                }
            }
        },
        true);
}
