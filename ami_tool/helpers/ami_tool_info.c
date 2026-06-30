#include "../ami_tool_i.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <input/input.h>
#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/nfc_device.h>
#include <furi_hal_nfc.h>

#define AMI_TOOL_INFO_READ_BUFFER 96
#define AMI_TOOL_WRITE_THREAD_STACK_SIZE 2048
#define AMI_TOOL_NFC_EXTENSION ".nfc"

typedef enum {
    AmiToolInfoActionMenuIndexEmulate,
    AmiToolInfoActionMenuIndexUsageInfo,
    AmiToolInfoActionMenuIndexChangeUid,
    AmiToolInfoActionMenuIndexWriteTag,
    AmiToolInfoActionMenuIndexSaveToStorage,
} AmiToolInfoActionMenuIndex;

static void ami_tool_info_widget_callback(GuiButtonType result, InputType type, void* context);
static void ami_tool_info_actions_submenu_callback(void* context, uint32_t index);
static void ami_tool_info_show_widget_text(AmiToolApp* app, const char* text);
static void ami_tool_usage_clear(AmiToolApp* app);
static bool ami_tool_usage_lookup_entry(
    AmiToolApp* app,
    const char* id_hex,
    FuriString* result,
    bool* asset_error);
static bool ami_tool_usage_parse_entries(AmiToolApp* app, const char* entry_line);
static void ami_tool_info_show_usage_page(AmiToolApp* app);
static void ami_tool_usage_button_callback(GuiButtonType result, InputType type, void* context);
static bool ami_tool_usage_is_true(const char* text);
static bool ami_tool_info_write_password_pages(
    AmiToolApp* app,
    const MfUltralightAuthPassword* password);
static bool ami_tool_info_target_tag_is_blank(const MfUltralightData* tag_data, const char** reason);
static int32_t ami_tool_info_write_worker(void* context);
static void ami_tool_info_write_send_event(
    AmiToolApp* app,
    AmiToolCustomEvent event,
    const char* message);
static const char* ami_tool_info_error_to_string(MfUltralightError error);

bool ami_tool_compute_password_from_uid(
    const uint8_t* uid,
    size_t uid_len,
    MfUltralightAuthPassword* password) {
    if((uid_len < 7) || !uid || !password) {
        return false;
    }

    password->data[0] = 0xAA ^ (uid[1] ^ uid[3]);
    password->data[1] = 0x55 ^ (uid[2] ^ uid[4]);
    password->data[2] = 0xAA ^ (uid[3] ^ uid[5]);
    password->data[3] = 0x55 ^ (uid[4] ^ uid[6]);

    return true;
}

static void ami_tool_info_compact_hex_string(const char* input, char* output, size_t output_size) {
    if(!output || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if(!input) {
        return;
    }

    size_t index = 0;
    while(*input && (index + 1) < output_size) {
        char ch = *input++;
        if(isalnum((unsigned char)ch)) {
            output[index++] = (char)toupper((unsigned char)ch);
        }
    }
    output[index] = '\0';
}

static bool ami_tool_info_format_uid_string(
    const AmiToolApp* app,
    char* buffer,
    size_t buffer_size) {
    if(!app || !buffer || buffer_size < 3) {
        return false;
    }

    size_t uid_len = 0;
    const uint8_t* uid_ptr = NULL;
    uint8_t temp_uid[10] = {0};

    if(app->last_uid_valid && app->last_uid_len > 0) {
        uid_ptr = app->last_uid;
        uid_len = app->last_uid_len;
    } else if(app->tag_data) {
        uid_ptr = mf_ultralight_get_uid(app->tag_data, &uid_len);
    }

    if(!uid_ptr || uid_len == 0) {
        return false;
    }

    // Ensure UID length does not exceed buffer (max 10 bytes -> 20 hex chars)
    if(uid_len > sizeof(temp_uid)) {
        uid_len = sizeof(temp_uid);
    }
    memcpy(temp_uid, uid_ptr, uid_len);

    size_t required = (uid_len * 2) + 1;
    if(buffer_size < required) {
        return false;
    }

    for(size_t i = 0; i < uid_len; i++) {
        snprintf(buffer + (i * 2), buffer_size - (i * 2), "%02X", temp_uid[i]);
    }
    buffer[uid_len * 2] = '\0';
    return true;
}

static const char* ami_tool_info_error_to_string(MfUltralightError error) {
    switch(error) {
    case MfUltralightErrorNone:
        return "No error";
    case MfUltralightErrorNotPresent:
        return "Tag not present";
    case MfUltralightErrorProtocol:
        return "Protocol error";
    case MfUltralightErrorAuth:
        return "Authentication failed";
    case MfUltralightErrorTimeout:
        return "Timed out";
    default:
        return "Unknown error";
    }
}

static bool ami_tool_info_target_tag_is_blank(const MfUltralightData* tag_data, const char** reason) {
    static const uint8_t capability_defaults[4] = {0xE1, 0x10, 0x3E, 0x00};
    static const uint8_t otp_defaults[4] = {0x03, 0x00, 0xFE, 0x00};
    static const uint8_t dynamic_lock_defaults[4] = {0x00, 0x00, 0x00, 0xBD};
    static const uint8_t cfg0_defaults[4] = {0x04, 0x00, 0x00, 0xFF};
    static const uint8_t cfg1_defaults[4] = {0x00, 0x05, 0x00, 0x00};
    static const uint8_t pwd_defaults[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t pack_defaults[4] = {0x00, 0x00, 0x00, 0x00};
    static const uint8_t unreadable_defaults[4] = {0x00, 0x00, 0x00, 0x00};

    if(reason) {
        *reason = "Detected tag is not blank.";
    }
    if(!tag_data || tag_data->type != MfUltralightTypeNTAG215 || tag_data->pages_total < 135) {
        if(reason) {
            *reason = "Detected tag does not have enough pages.";
        }
        return false;
    }

    if(tag_data->page[2].data[1] != 0x48) {
        if(reason) {
            *reason = "Detected tag has unexpected internal bytes.";
        }
        return false;
    }

    if((tag_data->page[2].data[2] != 0x00) || (tag_data->page[2].data[3] != 0x00)) {
        if(reason) {
            *reason = "Detected tag has static lock bits set.";
        }
        return false;
    }

    if(memcmp(tag_data->page[3].data, capability_defaults, sizeof(capability_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag has unexpected capability container bytes.";
        }
        return false;
    }

    if(memcmp(tag_data->page[4].data, otp_defaults, sizeof(otp_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag OTP page is not blank.";
        }
        return false;
    }

    for(uint16_t page = 5; page <= 129; page++) {
        if(memcmp(tag_data->page[page].data, pack_defaults, MF_ULTRALIGHT_PAGE_SIZE) != 0) {
            if(reason) {
                *reason = "Detected tag already contains user data.";
            }
            return false;
        }
    }

    if(memcmp(tag_data->page[130].data, dynamic_lock_defaults, sizeof(dynamic_lock_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag has dynamic lock bits set.";
        }
        return false;
    }

    if(memcmp(tag_data->page[131].data, cfg0_defaults, sizeof(cfg0_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag has non-default RF config.";
        }
        return false;
    }

    if(memcmp(tag_data->page[132].data, cfg1_defaults, sizeof(cfg1_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag has non-default access config.";
        }
        return false;
    }

    if(memcmp(tag_data->page[133].data, pwd_defaults, sizeof(pwd_defaults)) != 0 &&
       memcmp(tag_data->page[133].data, unreadable_defaults, sizeof(unreadable_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag password is not blank.";
        }
        return false;
    }

    if(memcmp(tag_data->page[134].data, pack_defaults, sizeof(pack_defaults)) != 0 &&
       memcmp(tag_data->page[134].data, unreadable_defaults, sizeof(unreadable_defaults)) != 0) {
        if(reason) {
            *reason = "Detected tag PACK bytes are not blank.";
        }
        return false;
    }

    return true;
}

static bool ami_tool_info_lookup_entry(
    AmiToolApp* app,
    const char* id_hex,
    FuriString* result,
    bool* asset_error) {
    furi_assert(app);
    furi_assert(result);
    if(asset_error) {
        *asset_error = false;
    }

    if(!app->storage || !id_hex || id_hex[0] == '\0') {
        if(asset_error && !app->storage) {
            *asset_error = true;
        }
        return false;
    }

    const size_t id_len = strlen(id_hex);
    if(id_len == 0) {
        return false;
    }

    char* id_lower = malloc(id_len + 1);
    if(!id_lower) {
        return false;
    }
    for(size_t i = 0; i < id_len; i++) {
        id_lower[i] = (char)tolower((unsigned char)id_hex[i]);
    }
    id_lower[id_len] = '\0';

    bool found = false;
    File* file = storage_file_alloc(app->storage);
    if(!file) {
        if(asset_error) {
            *asset_error = true;
        }
        free(id_lower);
        return false;
    }

    FuriString* line = furi_string_alloc();

    if(storage_file_open(file, APP_ASSETS_PATH("amiibo.dat"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool in_data_section = false;
        uint8_t buffer[AMI_TOOL_INFO_READ_BUFFER];

        while(true) {
            size_t read = storage_file_read(file, buffer, sizeof(buffer));
            if(read == 0) break;

            for(size_t i = 0; i < read; i++) {
                char ch = (char)buffer[i];
                if(ch == '\r') continue;

                if(ch == '\n') {
                    if(in_data_section && !furi_string_empty(line)) {
                        const char* raw = furi_string_get_cstr(line);
                        if(strncmp(raw, id_lower, id_len) == 0 && raw[id_len] == ':') {
                            furi_string_set(result, line);
                            found = true;
                            break;
                        }
                    } else if(furi_string_empty(line)) {
                        in_data_section = true;
                    }
                    furi_string_reset(line);
                } else {
                    furi_string_push_back(line, ch);
                }
            }

            if(found) break;
        }

        if(!found && in_data_section && !furi_string_empty(line)) {
            const char* raw = furi_string_get_cstr(line);
            if(strncmp(raw, id_lower, id_len) == 0 && raw[id_len] == ':') {
                furi_string_set(result, line);
                found = true;
            }
        }

        storage_file_close(file);
    } else {
        if(asset_error) {
            *asset_error = true;
        }
    }

    furi_string_free(line);
    storage_file_free(file);
    free(id_lower);
    return found;
}

static void ami_tool_usage_clear(AmiToolApp* app) {
    if(!app) return;
    if(app->usage_entries) {
        free(app->usage_entries);
        app->usage_entries = NULL;
    }
    if(app->usage_raw_data) {
        free(app->usage_raw_data);
        app->usage_raw_data = NULL;
    }
    app->usage_entries_capacity = 0;
    app->usage_page_index = 0;
    app->usage_page_count = 0;
    app->usage_info_visible = false;
    app->usage_nav_pending = false;
}

static bool ami_tool_usage_lookup_entry(
    AmiToolApp* app,
    const char* id_hex,
    FuriString* result,
    bool* asset_error) {
    furi_assert(app);
    furi_assert(result);
    if(asset_error) {
        *asset_error = false;
    }

    if(!app->storage || !id_hex || id_hex[0] == '\0') {
        if(asset_error && !app->storage) {
            *asset_error = true;
        }
        return false;
    }

    const size_t id_len = strlen(id_hex);
    if(id_len == 0) {
        return false;
    }

    char* id_lower = malloc(id_len + 1);
    if(!id_lower) {
        return false;
    }
    for(size_t i = 0; i < id_len; i++) {
        id_lower[i] = (char)tolower((unsigned char)id_hex[i]);
    }
    id_lower[id_len] = '\0';

    bool found = false;
    File* file = storage_file_alloc(app->storage);
    if(!file) {
        if(asset_error) {
            *asset_error = true;
        }
        free(id_lower);
        return false;
    }

    FuriString* line = furi_string_alloc();

    if(storage_file_open(file, APP_ASSETS_PATH("amiibo_usage.dat"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool in_data_section = false;
        uint8_t buffer[AMI_TOOL_INFO_READ_BUFFER];

        while(true) {
            size_t read = storage_file_read(file, buffer, sizeof(buffer));
            if(read == 0) break;

            for(size_t i = 0; i < read; i++) {
                char ch = (char)buffer[i];
                if(ch == '\r') continue;

                if(ch == '\n') {
                    if(in_data_section && !furi_string_empty(line)) {
                        const char* raw = furi_string_get_cstr(line);
                        if(strncmp(raw, id_lower, id_len) == 0 && raw[id_len] == ':') {
                            furi_string_set(result, line);
                            found = true;
                            break;
                        }
                    } else if(furi_string_empty(line)) {
                        in_data_section = true;
                    }
                    furi_string_reset(line);
                } else {
                    furi_string_push_back(line, ch);
                }
            }

            if(found) break;
        }

        if(!found && in_data_section && !furi_string_empty(line)) {
            const char* raw = furi_string_get_cstr(line);
            if(strncmp(raw, id_lower, id_len) == 0 && raw[id_len] == ':') {
                furi_string_set(result, line);
                found = true;
            }
        }

        storage_file_close(file);
    } else {
        if(asset_error) {
            *asset_error = true;
        }
    }

    furi_string_free(line);
    storage_file_free(file);
    free(id_lower);
    return found;
}

static bool ami_tool_usage_is_true(const char* text) {
    if(!text || text[0] == '\0') {
        return false;
    }
    return (text[0] == 'T') || (text[0] == 't');
}

static bool ami_tool_usage_parse_entries(AmiToolApp* app, const char* entry_line) {
    furi_assert(app);
    if(!entry_line || entry_line[0] == '\0') {
        return false;
    }

    const char* data = strchr(entry_line, ':');
    if(!data) {
        return false;
    }
    data++;
    while(*data == ' ') {
        data++;
    }
    if(*data == '\0') {
        return false;
    }

    size_t data_len = strlen(data);
    char* raw = malloc(data_len + 1);
    if(!raw) {
        return false;
    }
    memcpy(raw, data, data_len + 1);
    while(data_len > 0 && (raw[data_len - 1] == '\n' || raw[data_len - 1] == '\r')) {
        raw[data_len - 1] = '\0';
        data_len--;
    }

    AmiToolUsageEntry* entries = NULL;
    size_t capacity = 0;
    size_t count = 0;

    char* cursor = raw;
    while(cursor && *cursor) {
        char* segment = cursor;
        char* pipe = strchr(cursor, '|');
        if(pipe) {
            *pipe = '\0';
            cursor = pipe + 1;
        } else {
            cursor = NULL;
        }

        if(segment[0] == '\0') {
            continue;
        }

        char* platform = segment;
        char* sep = strchr(platform, '^');
        if(!sep) {
            continue;
        }
        *sep = '\0';
        char* game = sep + 1;
        sep = strchr(game, '^');
        if(!sep) {
            continue;
        }
        *sep = '\0';
        char* usage_cursor = sep + 1;
        if(*usage_cursor == '\0') {
            continue;
        }

        while(usage_cursor && *usage_cursor) {
            char* next_usage = strchr(usage_cursor, '^');
            if(next_usage) {
                *next_usage = '\0';
            }

            char* write_flag = strrchr(usage_cursor, '*');
            char* writable_text = NULL;
            if(write_flag) {
                *write_flag = '\0';
                writable_text = write_flag + 1;
            }
            bool writable = ami_tool_usage_is_true(writable_text);
            bool has_usage = usage_cursor[0] != '\0';
            const char* usage_text = has_usage ? usage_cursor : NULL;

            if(count == capacity) {
                size_t new_capacity = capacity ? capacity * 2 : 8;
                AmiToolUsageEntry* new_entries =
                    realloc(entries, new_capacity * sizeof(AmiToolUsageEntry));
                if(!new_entries) {
                    free(entries);
                    free(raw);
                    return false;
                }
                entries = new_entries;
                capacity = new_capacity;
            }

            AmiToolUsageEntry* entry = &entries[count++];
            entry->platform = platform;
            entry->game = game;
            entry->usage = has_usage ? (char*)usage_text : NULL;
            entry->has_usage = has_usage;
            entry->writable = writable;

            if(next_usage) {
                usage_cursor = next_usage + 1;
                if(*usage_cursor == '\0') {
                    usage_cursor = NULL;
                }
            } else {
                usage_cursor = NULL;
            }
        }
    }

    if(count == 0) {
        free(entries);
        free(raw);
        return false;
    }

    app->usage_entries = entries;
    app->usage_entries_capacity = capacity;
    app->usage_raw_data = raw;
    app->usage_page_index = 0;
    app->usage_page_count = count;

    return true;
}

static void ami_tool_info_show_usage_page(AmiToolApp* app) {
    furi_assert(app);
    widget_reset(app->info_widget);

    if(!app->usage_entries || app->usage_page_count == 0) {
        widget_add_text_scroll_element(
            app->info_widget, 2, 0, 124, 60, "No usage data available for this Amiibo.");
        view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewInfo);
        app->usage_info_visible = true;
        app->info_actions_visible = false;
        app->info_action_message_visible = false;
        app->usage_nav_pending = false;
        return;
    }

    if(app->usage_page_index >= app->usage_page_count) {
        app->usage_page_index = app->usage_page_count - 1;
    }

    AmiToolUsageEntry* entry = &app->usage_entries[app->usage_page_index];
    furi_string_reset(app->text_box_store);
    furi_string_cat_printf(app->text_box_store, "\e#Usage Info %u/%u\n\n", (unsigned)(app->usage_page_index + 1), (unsigned)app->usage_page_count);
    furi_string_cat_printf(app->text_box_store, "Game: %s\n", entry->game);
    if(entry->has_usage && entry->usage && entry->usage[0] != '\0') {
        furi_string_cat(app->text_box_store, "Usage: ");
        furi_string_cat(app->text_box_store, entry->usage);
        furi_string_push_back(app->text_box_store, '\n');
    }
    furi_string_cat_printf(
        app->text_box_store, "Writable: %s\n", entry->writable ? "Yes" : "No");

    widget_add_text_scroll_element(
        app->info_widget, 2, 0, 124, 60, furi_string_get_cstr(app->text_box_store));

    if(app->usage_page_index > 0) {
        widget_add_button_element(
            app->info_widget, GuiButtonTypeLeft, "Prev", ami_tool_usage_button_callback, app);
    }
    if((app->usage_page_index + 1) < app->usage_page_count) {
        widget_add_button_element(
            app->info_widget, GuiButtonTypeRight, "Next", ami_tool_usage_button_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewInfo);
    app->usage_info_visible = true;
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->usage_nav_pending = false;
}

static void ami_tool_usage_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) {
        return;
    }
    AmiToolApp* app = context;
    if(!app || app->usage_nav_pending) {
        return;
    }
    app->usage_nav_pending = true;

    AmiToolCustomEvent event = 0;
    if(result == GuiButtonTypeLeft) {
        event = AmiToolEventUsagePrevPage;
    } else if(result == GuiButtonTypeRight) {
        event = AmiToolEventUsageNextPage;
    } else {
        app->usage_nav_pending = false;
        return;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void ami_tool_info_append_uid(AmiToolApp* app, FuriString* target) {
    if(!target || !app || !app->last_uid_valid || app->last_uid_len == 0) {
        return;
    }

    furi_string_cat(target, "UID: ");
    for(size_t i = 0; i < app->last_uid_len; i++) {
        furi_string_cat_printf(target, "%02X", app->last_uid[i]);
        if(i + 1 < app->last_uid_len) {
            furi_string_push_back(target, ' ');
        }
    }
    furi_string_push_back(target, '\n');
}

static void ami_tool_info_append_field(FuriString* buffer, const char* label, const char* value) {
    if(!label || !value) {
        return;
    }
    furi_string_cat_printf(buffer, "%s: %s\n", label, value);
}

static void ami_tool_info_append_releases(FuriString* buffer, const char* releases) {
    if(!buffer || !releases || releases[0] == '\0') {
        return;
    }

    furi_string_cat(buffer, "Released on:\n");
    size_t len = strlen(releases);
    char* temp = malloc(len + 1);
    if(!temp) {
        furi_string_cat_printf(buffer, "  %s\n", releases);
        return;
    }
    memcpy(temp, releases, len + 1);

    char* cursor = temp;
    while(*cursor) {
        char* entry = cursor;
        char* comma = strchr(cursor, ',');
        if(comma) {
            *comma = '\0';
            cursor = comma + 1;
        } else {
            cursor += strlen(cursor);
        }

        if(entry[0] == '\0') continue;
        char* sep = strchr(entry, '.');
        if(sep) {
            *sep = '\0';
            const char* region = entry;
            const char* date = sep + 1;
            if(*date == '\0') date = "unknown";
            furi_string_cat_printf(buffer, "  - %s: %s\n", region, date);
        } else {
            furi_string_cat_printf(buffer, "  - %s\n", entry);
        }
    }

    free(temp);
}

static void ami_tool_info_format_entry(
    AmiToolApp* app,
    const char* id_hex,
    const char* entry_line,
    bool from_read) {
    furi_assert(app);
    UNUSED(from_read);
    furi_string_reset(app->text_box_store);

    if(!entry_line || entry_line[0] == '\0') {
        return;
    }

    const char* data = strchr(entry_line, ':');
    if(!data) {
        furi_string_set(app->text_box_store, entry_line);
        return;
    }
    data++; // skip ':'
    while(*data == ' ') data++;

    size_t data_len = strlen(data);
    char* temp = malloc(data_len + 1);
    if(!temp) {
        furi_string_set(app->text_box_store, data);
        return;
    }
    memcpy(temp, data, data_len + 1);

    const char* fields[6] = {0};
    size_t field_count = 0;
    char* cursor = temp;
    if(*cursor != '\0') {
        fields[field_count++] = cursor;
    }

    while(*cursor && field_count < 6) {
        if(*cursor == '|') {
            *cursor = '\0';
            cursor++;
            if(*cursor == '\0') break;
            fields[field_count++] = cursor;
        } else {
            cursor++;
        }
    }

    const char* name = (field_count > 0 && fields[0]) ? fields[0] : "Unknown";
    const char* character = (field_count > 1 && fields[1]) ? fields[1] : "Unknown";
    const char* series = (field_count > 2 && fields[2]) ? fields[2] : "Unknown";
    const char* game_series = (field_count > 3 && fields[3]) ? fields[3] : "Unknown";
    const char* type = (field_count > 4 && fields[4]) ? fields[4] : "Unknown";
    const char* releases = (field_count > 5 && fields[5]) ? fields[5] : "N/A";

    furi_string_cat_printf(app->text_box_store, "\e#%s\n\n", name);
    ami_tool_info_append_uid(app, app->text_box_store);
    if(id_hex && id_hex[0] != '\0') {
        ami_tool_info_append_field(app->text_box_store, "ID", id_hex);
    }
    ami_tool_info_append_field(app->text_box_store, "Character", character);
    ami_tool_info_append_field(app->text_box_store, "Amiibo Series", series);
    ami_tool_info_append_field(app->text_box_store, "Game Series", game_series);
    ami_tool_info_append_field(app->text_box_store, "Type", type);
    ami_tool_info_append_releases(app->text_box_store, releases);

    free(temp);
}

bool ami_tool_info_get_name_for_id(AmiToolApp* app, const char* id_hex, FuriString* out_name) {
    if(!app || !out_name || !id_hex || id_hex[0] == '\0') {
        if(out_name) {
            furi_string_reset(out_name);
        }
        return false;
    }

    FuriString* entry = furi_string_alloc();
    bool asset_error = false;
    bool found = ami_tool_info_lookup_entry(app, id_hex, entry, &asset_error);

    if(found) {
        const char* raw = furi_string_get_cstr(entry);
        const char* data = raw;
        const char* colon = strchr(raw, ':');
        if(colon) {
            data = colon + 1;
            while(*data == ' ') {
                data++;
            }
        }
        const char* end = data;
        while(*end && *end != '|' && *end != '\r' && *end != '\n') {
            end++;
        }
        if(end > data) {
            furi_string_reset(out_name);
            furi_string_cat_printf(out_name, "%.*s", (int)(end - data), data);
        } else {
            furi_string_set(out_name, id_hex);
        }
    } else {
        furi_string_set(out_name, id_hex);
    }

    furi_string_free(entry);
    return found && furi_string_size(out_name) > 0;
}

static void ami_tool_info_show_text_info(AmiToolApp* app, const char* message) {
    furi_string_set(app->text_box_store, message);
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    app->usage_info_visible = false;
}

static void ami_tool_info_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    AmiToolApp* app = context;
    if((result == GuiButtonTypeCenter) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventInfoShowActions);
    }
}

static void ami_tool_info_show_widget_text(AmiToolApp* app, const char* text) {
    furi_assert(app);
    widget_reset(app->info_widget);
    widget_add_text_scroll_element(app->info_widget, 2, 0, 124, 60, text);
    widget_add_button_element(
        app->info_widget, GuiButtonTypeCenter, "OK", ami_tool_info_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewInfo);
    app->usage_info_visible = false;
}

void ami_tool_info_show_page(AmiToolApp* app, const char* id_hex, bool from_read) {
    furi_assert(app);

    bool same_id = false;
    if(id_hex && id_hex[0] != '\0' && app->info_last_has_id) {
        same_id = (strncmp(app->info_last_id, id_hex, sizeof(app->info_last_id)) == 0);
    }
    if(!same_id) {
        ami_tool_usage_clear(app);
    }

    app->info_last_from_read = from_read;
    if(id_hex && id_hex[0] != '\0') {
        strncpy(app->info_last_id, id_hex, sizeof(app->info_last_id) - 1);
        app->info_last_id[sizeof(app->info_last_id) - 1] = '\0';
        app->info_last_has_id = true;
    } else {
        app->info_last_id[0] = '\0';
        app->info_last_has_id = false;
    }

    if(!app->tag_data || !app->tag_data_valid) {
        ami_tool_info_show_text_info(
            app,
            "No Amiibo dump available.\nRead a tag or generate one, then try again.");
        return;
    }

    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    ami_tool_info_stop_emulation(app);

    FuriString* entry = furi_string_alloc();
    bool asset_error = false;
    bool found = ami_tool_info_lookup_entry(app, id_hex, entry, &asset_error);

    if(found) {
        ami_tool_info_format_entry(
            app, id_hex ? id_hex : "", furi_string_get_cstr(entry), from_read);
        ami_tool_info_show_widget_text(app, furi_string_get_cstr(app->text_box_store));
    } else {
        furi_string_reset(app->text_box_store);
        if(from_read) {
            furi_string_cat(app->text_box_store, "Valid NTAG215 detected.\n");
            if(id_hex && id_hex[0] != '\0') {
                furi_string_cat_printf(app->text_box_store, "ID: %s\n", id_hex);
            }
            if(asset_error) {
                furi_string_cat(
                    app->text_box_store,
                    "\nUnable to open amiibo.dat.\nInstall or update the assets.");
            } else {
                furi_string_cat(
                    app->text_box_store,
                    "\nNo matching entry found.\nTag is unlikely to be an official Amiibo.");
            }
        } else {
            if(asset_error) {
                furi_string_cat(
                    app->text_box_store,
                    "Unable to open amiibo.dat.\nInstall or update the assets.");
            } else if(id_hex && id_hex[0] != '\0') {
                furi_string_cat_printf(
                    app->text_box_store,
                    "Selected Amiibo ID:\n%s\nNo matching entry found.",
                    id_hex);
            } else {
                furi_string_cat(
                    app->text_box_store,
                    "No Amiibo ID provided.\nUnable to display details.");
            }
        }
        ami_tool_info_append_uid(app, app->text_box_store);
        ami_tool_info_show_widget_text(app, furi_string_get_cstr(app->text_box_store));
    }

    furi_string_free(entry);
}

bool ami_tool_info_show_usage(AmiToolApp* app) {
    furi_assert(app);

    if(!app->info_last_has_id || app->info_last_id[0] == '\0') {
        ami_tool_info_show_action_message(
            app, "Usage data requires a valid Amiibo ID.\nRead or generate one first.");
        return false;
    }

    if(app->usage_entries && app->usage_page_count > 0) {
        ami_tool_info_show_usage_page(app);
        return true;
    }

    ami_tool_usage_clear(app);

    FuriString* entry = furi_string_alloc();
    bool asset_error = false;
    bool found = ami_tool_usage_lookup_entry(app, app->info_last_id, entry, &asset_error);
    bool success = false;

    if(found) {
        success = ami_tool_usage_parse_entries(app, furi_string_get_cstr(entry));
        if(success) {
            ami_tool_info_show_usage_page(app);
        }
    }

    if(!success) {
        if(asset_error) {
            ami_tool_info_show_action_message(
                app, "Unable to open amiibo_usage.dat.\nInstall or update the assets.");
        } else if(!found) {
            ami_tool_info_show_action_message(
                app, "No usage info is available for this Amiibo.");
        } else {
            ami_tool_info_show_action_message(
                app, "Usage data is invalid.\nInstall or update the assets.");
        }
    }

    furi_string_free(entry);
    return success;
}

void ami_tool_info_refresh_current_page(AmiToolApp* app) {
    if(!app) return;
    const char* id_arg = app->info_last_has_id ? app->info_last_id : NULL;
    ami_tool_info_show_page(app, id_arg, app->info_last_from_read);
}

void ami_tool_info_navigate_usage(AmiToolApp* app, int32_t delta) {
    if(!app) {
        return;
    }
    if(!app->usage_entries || app->usage_page_count == 0 || delta == 0) {
        app->usage_nav_pending = false;
        return;
    }

    int32_t new_index = (int32_t)app->usage_page_index + delta;
    if(new_index < 0) {
        new_index = 0;
    } else if(new_index >= (int32_t)app->usage_page_count) {
        new_index = (int32_t)app->usage_page_count - 1;
    }

    if(new_index != (int32_t)app->usage_page_index) {
        app->usage_page_index = (size_t)new_index;
        ami_tool_info_show_usage_page(app);
    } else {
        app->usage_nav_pending = false;
    }
}

void ami_tool_store_uid(AmiToolApp* app, const uint8_t* uid, size_t len) {
    if(!app) return;
    if(!uid || len == 0) {
        memset(app->last_uid, 0, sizeof(app->last_uid));
        app->last_uid_len = 0;
        app->last_uid_valid = false;
        return;
    }
    if(len > sizeof(app->last_uid)) {
        len = sizeof(app->last_uid);
    }
    memcpy(app->last_uid, uid, len);
    app->last_uid_len = len;
    app->last_uid_valid = true;
}

void ami_tool_clear_cached_tag(AmiToolApp* app) {
    if(!app) return;
    ami_tool_info_stop_emulation(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    ami_tool_usage_clear(app);
    app->tag_data_valid = false;
    if(app->tag_data) {
        mf_ultralight_reset(app->tag_data);
    }
    app->tag_password_valid = false;
    memset(&app->tag_password, 0, sizeof(app->tag_password));
    app->tag_pack_valid = false;
    memset(app->tag_pack, 0, sizeof(app->tag_pack));
    app->amiibo_link_completion_marker_valid = false;
    memset(app->amiibo_link_completion_marker, 0, sizeof(app->amiibo_link_completion_marker));
}

bool ami_tool_extract_amiibo_id(
    const MfUltralightData* tag_data,
    char* buffer,
    size_t buffer_size) {
    if(!tag_data || !buffer || buffer_size < 17) {
        return false;
    }
    const size_t start_page = 21;
    if(tag_data->pages_total <= (start_page + 1)) {
        return false;
    }

    uint8_t raw[8];
    for(size_t i = 0; i < 4; i++) {
        raw[i] = tag_data->page[start_page].data[i];
        raw[i + 4] = tag_data->page[start_page + 1].data[i];
    }

    for(size_t i = 0; i < 8; i++) {
        snprintf(buffer + (i * 2), buffer_size - (i * 2), "%02X", raw[i]);
    }
    buffer[16] = '\0';

    return true;
}

static bool ami_tool_info_rebuild_dump_for_uid(
    AmiToolApp* app,
    const uint8_t* uid,
    size_t uid_len) {
    if(!app->tag_data || !app->tag_data_valid) {
        return false;
    }

    ami_tool_info_stop_emulation(app);

    if(!ami_tool_has_retail_key(app)) {
        if(ami_tool_load_retail_key(app) != AmiToolRetailKeyStatusOk) {
            return false;
        }
    }

    const DumpedKeys* keys = (const DumpedKeys*)app->retail_key;
    DerivedKey old_data_key = {0};
    DerivedKey old_tag_key = {0};
    DerivedKey new_data_key = {0};
    DerivedKey new_tag_key = {0};
    DerivedKey* cleanup_key = &old_data_key;

    if(amiibo_derive_key(&keys->data, app->tag_data, &old_data_key) != RFIDX_OK) {
        return false;
    }
    if(amiibo_derive_key(&keys->tag, app->tag_data, &old_tag_key) != RFIDX_OK) {
        return false;
    }

    bool decrypted = false;
    if(amiibo_cipher(&old_data_key, app->tag_data) != RFIDX_OK) {
        return false;
    }
    decrypted = true;

    RfidxStatus status;
    if(uid && uid_len >= 7) {
        status = amiibo_set_uid(app->tag_data, uid, uid_len);
    } else {
        status = amiibo_change_uid(app->tag_data);
    }
    if(status != RFIDX_OK) {
        goto cleanup;
    }

    if(amiibo_derive_key(&keys->data, app->tag_data, &new_data_key) != RFIDX_OK) goto cleanup;
    if(amiibo_derive_key(&keys->tag, app->tag_data, &new_tag_key) != RFIDX_OK) goto cleanup;
    cleanup_key = &new_data_key;

    if(amiibo_sign_payload(&new_tag_key, &new_data_key, app->tag_data) != RFIDX_OK) goto cleanup;

    if(amiibo_cipher(&new_data_key, app->tag_data) != RFIDX_OK) {
        decrypted = true;
        goto cleanup;
    }
    decrypted = false;

    amiibo_configure_rf_interface(app->tag_data);

    if(app->tag_data->pages_total >= 2) {
        uint8_t new_uid[7];
        memcpy(new_uid, app->tag_data->page[0].data, 3);
        memcpy(new_uid + 3, app->tag_data->page[1].data, 4);
        ami_tool_store_uid(app, new_uid, sizeof(new_uid));
        if(ami_tool_compute_password_from_uid(new_uid, sizeof(new_uid), &app->tag_password)) {
            app->tag_password_valid = true;
        } else {
            app->tag_password_valid = false;
            memset(&app->tag_password, 0, sizeof(app->tag_password));
        }
    }

    return true;

cleanup:
    if(decrypted) {
        amiibo_cipher(cleanup_key, app->tag_data);
        amiibo_configure_rf_interface(app->tag_data);
    }
    return false;
}

bool ami_tool_info_change_uid(AmiToolApp* app) {
    furi_assert(app);

    if(!ami_tool_info_rebuild_dump_for_uid(app, NULL, 0)) {
        return false;
    }

    const char* id_arg = app->info_last_has_id ? app->info_last_id : NULL;
    ami_tool_info_show_page(app, id_arg, app->info_last_from_read);

    return true;
}

static int32_t ami_tool_info_write_worker(void* context) {
    AmiToolApp* app = context;
    if(app->write_cancel_requested) {
        ami_tool_info_write_send_event(app, AmiToolEventInfoWriteCancelled, "Write cancelled.");
        return 0;
    }

    MfUltralightData* target = mf_ultralight_alloc();
    if(!target) {
        ami_tool_info_write_send_event(
            app, AmiToolEventInfoWriteFailed, "Unable to allocate tag buffer.");
        return 0;
    }

    while(true) {
        if(app->write_cancel_requested) {
            ami_tool_info_write_send_event(
                app, AmiToolEventInfoWriteCancelled, "Write cancelled.");
            goto cleanup;
        }

        MfUltralightError error = mf_ultralight_poller_sync_read_card(app->nfc, target, NULL);
        if(error == MfUltralightErrorNone) {
            break;
        }
        if((error == MfUltralightErrorNotPresent) || (error == MfUltralightErrorTimeout)) {
            furi_delay_ms(100);
            continue;
        }

        char message[96];
        snprintf(
            message,
            sizeof(message),
            "Unable to read tag: %s",
            ami_tool_info_error_to_string(error));
        ami_tool_info_write_send_event(app, AmiToolEventInfoWriteFailed, message);
        goto cleanup;
    }

    if(target->type != MfUltralightTypeNTAG215) {
        ami_tool_info_write_send_event(
            app, AmiToolEventInfoWriteFailed, "Detected tag is not an NTAG215.");
        goto cleanup;
    }

    size_t target_uid_len = 0;
    const uint8_t* target_uid = mf_ultralight_get_uid(target, &target_uid_len);
    if(!target_uid || target_uid_len < 7) {
        ami_tool_info_write_send_event(
            app, AmiToolEventInfoWriteFailed, "Unable to read tag UID.");
        goto cleanup;
    }

    const char* blank_reason = NULL;
    if(!ami_tool_info_target_tag_is_blank(target, &blank_reason)) {
        ami_tool_info_write_send_event(
            app,
            AmiToolEventInfoWriteFailed,
            blank_reason ? blank_reason : "Detected tag is not blank.");
        goto cleanup;
    }

    bool uid_matches = app->last_uid_valid && (app->last_uid_len >= 7) &&
                       (memcmp(app->last_uid, target_uid, 7) == 0);
    if(!uid_matches) {
        if(!ami_tool_info_rebuild_dump_for_uid(app, target_uid, target_uid_len)) {
            ami_tool_info_write_send_event(
                app,
                AmiToolEventInfoWriteFailed,
                "Failed to prepare Amiibo data. Install key_retail.bin and try again.");
            goto cleanup;
        }
    }

    MfUltralightError io_error = MfUltralightErrorNone;
    uint16_t failed_page = UINT16_MAX;
    AmiToolWriteStatus status = ami_tool_write_custom_sequence(app, &io_error, &failed_page);

    if(app->write_cancel_requested) {
        ami_tool_info_write_send_event(app, AmiToolEventInfoWriteCancelled, "Write cancelled.");
        goto cleanup;
    }

    if(status != AmiToolWriteStatusOk) {
        char message[96];
        if((status == AmiToolWriteStatusIoError) && (failed_page != UINT16_MAX)) {
            snprintf(
                message,
                sizeof(message),
                "Write failed at page %u: %s",
                failed_page,
                ami_tool_info_error_to_string(io_error));
        } else if(status == AmiToolWriteStatusIoError) {
            snprintf(
                message,
                sizeof(message),
                "Write failed: %s",
                ami_tool_info_error_to_string(io_error));
        } else {
            snprintf(message, sizeof(message), "%s", ami_tool_write_status_to_string(status));
        }
        ami_tool_info_write_send_event(app, AmiToolEventInfoWriteFailed, message);
        goto cleanup;
    }

    ami_tool_info_write_send_event(app, AmiToolEventInfoWriteSuccess, NULL);
cleanup:
    mf_ultralight_free(target);
    return 0;
}

bool ami_tool_info_write_to_tag(AmiToolApp* app) {
    furi_assert(app);

    if(!app->tag_data || !app->tag_data_valid || !app->nfc) {
        return false;
    }
    if(app->write_in_progress) {
        return false;
    }

    ami_tool_info_stop_emulation(app);

    furi_string_set(
        app->text_box_store,
        "Write Amiibo\n\nPlace an NTAG215 tag on the back of the Flipper.\nPress Back to cancel.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);

    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->usage_info_visible = false;
    app->write_cancel_requested = false;
    app->write_waiting_for_tag = true;
    app->write_in_progress = true;
    app->write_result_message[0] = '\0';

    app->write_thread = furi_thread_alloc_ex(
        "AmiToolWrite", AMI_TOOL_WRITE_THREAD_STACK_SIZE, ami_tool_info_write_worker, app);
    if(!app->write_thread) {
        app->write_in_progress = false;
        app->write_waiting_for_tag = false;
        return false;
    }

    furi_thread_start(app->write_thread);
    return true;
}

bool ami_tool_info_save_to_storage(AmiToolApp* app) {
    furi_assert(app);

    if(!app->tag_data || !app->tag_data_valid || !app->storage) {
        return false;
    }

    char id_hex[32] = {0};
    if(app->info_last_has_id && app->info_last_id[0] != '\0') {
        ami_tool_info_compact_hex_string(app->info_last_id, id_hex, sizeof(id_hex));
    } else if(!ami_tool_extract_amiibo_id(app->tag_data, id_hex, sizeof(id_hex))) {
        strncpy(id_hex, "AMIIBO", sizeof(id_hex) - 1);
        id_hex[sizeof(id_hex) - 1] = '\0';
    }
    if(id_hex[0] == '\0') {
        strncpy(id_hex, "AMIIBO", sizeof(id_hex) - 1);
        id_hex[sizeof(id_hex) - 1] = '\0';
    }

    char uid_hex[32] = {0};
    if(!ami_tool_info_format_uid_string(app, uid_hex, sizeof(uid_hex))) {
        return false;
    }

    if(!storage_simply_mkdir(app->storage, AMI_TOOL_NFC_FOLDER)) {
        if(!storage_common_exists(app->storage, AMI_TOOL_NFC_FOLDER)) {
            return false;
        }
    }

    FuriString* path = furi_string_alloc();
    if(!path) {
        return false;
    }

    furi_string_printf(
        path, "%s/%s-%s%s", AMI_TOOL_NFC_FOLDER, id_hex, uid_hex, AMI_TOOL_NFC_EXTENSION);

    NfcDevice* device = nfc_device_alloc();
    bool success = false;
    if(device) {
        amiibo_configure_rf_interface(app->tag_data);
        nfc_device_set_data(
            device, NfcProtocolMfUltralight, (const NfcDeviceData*)app->tag_data);
        success = nfc_device_save(device, furi_string_get_cstr(path));
        nfc_device_free(device);
    }

    if(success) {
        const char* saved_path = furi_string_get_cstr(path);
        const char* file_name = strrchr(saved_path, '/');
        if(file_name && file_name[1] != '\0') {
            file_name++;
        } else {
            file_name = saved_path;
        }
        char message[128];
        snprintf(message, sizeof(message), "Saved Amiibo as\n%s", file_name);
        ami_tool_info_show_action_message(app, message);
    }

    furi_string_free(path);
    return success;
}

static void ami_tool_info_actions_submenu_callback(void* context, uint32_t index) {
    AmiToolApp* app = context;
    AmiToolCustomEvent event = AmiToolEventInfoActionEmulate;
    switch(index) {
    case AmiToolInfoActionMenuIndexEmulate:
        event = AmiToolEventInfoActionEmulate;
        break;
    case AmiToolInfoActionMenuIndexUsageInfo:
        event = AmiToolEventInfoActionUsage;
        break;
    case AmiToolInfoActionMenuIndexChangeUid:
        event = AmiToolEventInfoActionChangeUid;
        break;
    case AmiToolInfoActionMenuIndexWriteTag:
        event = AmiToolEventInfoActionWriteTag;
        break;
    case AmiToolInfoActionMenuIndexSaveToStorage:
        event = AmiToolEventInfoActionSaveToStorage;
        break;
    default:
        return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void ami_tool_info_show_actions_menu(AmiToolApp* app) {
    furi_assert(app);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Amiibo Actions");
    submenu_add_item(
        app->submenu,
        "Emulate",
        AmiToolInfoActionMenuIndexEmulate,
        ami_tool_info_actions_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Usage Info",
        AmiToolInfoActionMenuIndexUsageInfo,
        ami_tool_info_actions_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Change UID",
        AmiToolInfoActionMenuIndexChangeUid,
        ami_tool_info_actions_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Write to Tag",
        AmiToolInfoActionMenuIndexWriteTag,
        ami_tool_info_actions_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save to Storage",
        AmiToolInfoActionMenuIndexSaveToStorage,
        ami_tool_info_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
    app->info_actions_visible = true;
    app->info_action_message_visible = false;
    app->usage_info_visible = false;
}

void ami_tool_info_show_action_message(AmiToolApp* app, const char* message) {
    furi_assert(app);
    furi_string_reset(app->text_box_store);
    if(message && message[0] != '\0') {
        furi_string_cat(app->text_box_store, message);
    } else {
        furi_string_cat(app->text_box_store, "This action is not available yet.");
    }
    furi_string_cat(app->text_box_store, "\n\nPress Back to return.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    app->info_action_message_visible = true;
    app->info_actions_visible = false;
    app->info_emulation_active = false;
    app->usage_info_visible = false;
}

static void ami_tool_info_write_send_event(
    AmiToolApp* app,
    AmiToolCustomEvent event,
    const char* message) {
    if(!app) return;
    if(message) {
        strncpy(app->write_result_message, message, sizeof(app->write_result_message) - 1);
        app->write_result_message[sizeof(app->write_result_message) - 1] = '\0';
    } else {
        app->write_result_message[0] = '\0';
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static bool ami_tool_info_write_password_pages(
    AmiToolApp* app,
    const MfUltralightAuthPassword* password) {
    if(!app || !app->tag_data || !password) {
        return false;
    }
    if(app->tag_data->pages_total < 2) {
        return false;
    }
    size_t password_page = app->tag_data->pages_total - 2;
    size_t pack_page = app->tag_data->pages_total - 1;
    memcpy(app->tag_data->page[password_page].data, password->data, sizeof(password->data));
    uint8_t* pack = app->tag_data->page[pack_page].data;
    if(app->tag_pack_valid) {
        memcpy(pack, app->tag_pack, sizeof(app->tag_pack));
    } else {
        pack[0] = 0x80;
        pack[1] = 0x80;
        pack[2] = 0x00;
        pack[3] = 0x00;
    }
    memcpy(app->tag_pack, pack, sizeof(app->tag_pack));
    app->tag_pack_valid = true;
    return true;
}

static NfcCommand amiibo_emulation_cb(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return NfcCommandContinue; // keep the listener alive
}

bool ami_tool_info_start_emulation(AmiToolApp* app) {
    furi_assert(app);
    if(!app->tag_data || !app->tag_data_valid || !app->nfc) {
        return false;
    }

    amiibo_configure_rf_interface(app->tag_data);

    MfUltralightAuthPassword password = {0};
    if(app->tag_password_valid) {
        password = app->tag_password;
    } else if(app->last_uid_valid &&
              ami_tool_compute_password_from_uid(
                  app->last_uid, app->last_uid_len, &password)) {
        /* password derived successfully */
    } else {
        return false;
    }

    if(!ami_tool_info_write_password_pages(app, &password)) {
        return false;
    }

    ami_tool_info_stop_emulation(app);
    app->emulation_listener = nfc_listener_alloc(
        app->nfc, NfcProtocolMfUltralight, (const NfcDeviceData*)app->tag_data);
    if(!app->emulation_listener) {
        return false;
    }
    nfc_listener_start(app->emulation_listener, amiibo_emulation_cb, app);

    furi_string_set(
        app->text_box_store,
        "Emulating Amiibo...\n\nPlace the back of the Flipper near the reader.\nPress Back to stop.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);

    app->tag_password = password;
    app->tag_password_valid = true;
    app->info_emulation_active = true;
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->usage_info_visible = false;

    return true;
}

void ami_tool_info_stop_emulation(AmiToolApp* app) {
    if(!app) {
        return;
    }
    if(app->emulation_listener) {
        nfc_listener_stop(app->emulation_listener);
        nfc_listener_free(app->emulation_listener);
        app->emulation_listener = NULL;
    }
    app->info_emulation_active = false;
}

void ami_tool_info_handle_write_event(AmiToolApp* app, AmiToolCustomEvent event) {
    if(!app) {
        return;
    }

    if(event != AmiToolEventInfoWriteStarted && app->write_thread) {
        furi_thread_join(app->write_thread);
        furi_thread_free(app->write_thread);
        app->write_thread = NULL;
    }

    switch(event) {
    case AmiToolEventInfoWriteStarted:
        app->write_waiting_for_tag = false;
        furi_string_set(app->text_box_store, "Writing Amiibo...\nDo not remove the tag.");
        text_box_reset(app->text_box);
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
        break;
    case AmiToolEventInfoWriteSuccess: {
        app->write_in_progress = false;
        app->write_waiting_for_tag = false;
        app->write_cancel_requested = false;
        const char* id_arg = app->info_last_has_id ? app->info_last_id : NULL;
        ami_tool_info_show_page(app, id_arg, app->info_last_from_read);
        break;
    }
    case AmiToolEventInfoWriteFailed:
    case AmiToolEventInfoWriteCancelled: {
        app->write_in_progress = false;
        app->write_waiting_for_tag = false;
        app->write_cancel_requested = false;
        const char* message = app->write_result_message[0] != '\0'
                                  ? app->write_result_message
                                  : ((event == AmiToolEventInfoWriteCancelled) ? "Write cancelled."
                                                                               : "Unable to write tag.");
        ami_tool_info_show_action_message(app, message);
        break;
    }
    default:
        break;
    }
}

bool ami_tool_info_request_write_cancel(AmiToolApp* app) {
    if(!app || !app->write_in_progress || !app->write_waiting_for_tag ||
       app->write_cancel_requested) {
        return false;
    }

    app->write_cancel_requested = true;
    furi_string_set(app->text_box_store, "Cancelling write...\nPlease wait.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    furi_hal_nfc_abort();
    return true;
}

void ami_tool_info_abort_write(AmiToolApp* app) {
    if(!app || !app->write_in_progress) {
        return;
    }

    if(app->write_waiting_for_tag && !app->write_cancel_requested) {
        app->write_cancel_requested = true;
        furi_hal_nfc_abort();
    }

    if(app->write_thread) {
        furi_thread_join(app->write_thread);
        furi_thread_free(app->write_thread);
        app->write_thread = NULL;
    }

    app->write_in_progress = false;
    app->write_waiting_for_tag = false;
    app->write_cancel_requested = false;
}
