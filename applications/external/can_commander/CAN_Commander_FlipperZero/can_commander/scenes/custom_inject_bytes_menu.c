#include "../can_commander.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    CustomInjectBytesToInject = 0,
    CustomInjectBytesValue,
    CustomInjectBytesSave,
} CustomInjectBytesMenuIndex;

static void cancommander_scene_custom_inject_bytes_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool cancommander_scene_custom_inject_hex_nibble(char c, uint8_t* out) {
    if(c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }
    if(c >= 'a' && c <= 'f') {
        *out = (uint8_t)(10 + (c - 'a'));
        return true;
    }
    if(c >= 'A' && c <= 'F') {
        *out = (uint8_t)(10 + (c - 'A'));
        return true;
    }
    return false;
}

static bool cancommander_scene_custom_inject_parse_hex_bytes(const char* hex, uint8_t out[8]) {
    if(!hex || !out) {
        return false;
    }

    memset(out, 0, 8U);
    const size_t len = strlen(hex);
    if(len == 0U || len > 16U || (len % 2U) != 0U) {
        return false;
    }

    const size_t bytes = len / 2U;
    for(size_t i = 0U; i < bytes && i < 8U; i++) {
        uint8_t hi = 0U;
        uint8_t lo = 0U;
        if(!cancommander_scene_custom_inject_hex_nibble(hex[i * 2U], &hi) ||
           !cancommander_scene_custom_inject_hex_nibble(hex[i * 2U + 1U], &lo)) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4U) | lo);
    }

    return true;
}

static void cancommander_scene_custom_inject_hex_from_bytes(const uint8_t in[8], char out[17]) {
    static const char kHex[] = "0123456789ABCDEF";
    for(uint8_t i = 0U; i < 8U; i++) {
        out[i * 2U] = kHex[(uint8_t)((in[i] >> 4U) & 0x0FU)];
        out[i * 2U + 1U] = kHex[(uint8_t)(in[i] & 0x0FU)];
    }
    out[16] = '\0';
}

static bool cancommander_scene_custom_inject_parse_bytes_list(const char* list, bool selected[8]) {
    if(!list || !selected) {
        return false;
    }

    memset(selected, 0, 8U);

    char scratch[sizeof(((App*)0)->custom_inject_edit_bytes)] = {0};
    strncpy(scratch, list, sizeof(scratch) - 1U);

    char* save_ptr = NULL;
    char* token = strtok_r(scratch, ",", &save_ptr);
    bool any = false;
    while(token) {
        while(*token == ' ' || *token == '\t') {
            token++;
        }

        char* end = token + strlen(token);
        while(end > token && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        *end = '\0';

        if(token[0] != '\0') {
            char* parse_end = NULL;
            unsigned long idx = strtoul(token, &parse_end, 10);
            if(parse_end == token || *parse_end != '\0' || idx > 7UL) {
                return false;
            }
            selected[idx] = true;
            any = true;
        }

        token = strtok_r(NULL, ",", &save_ptr);
    }

    return any;
}

static bool cancommander_scene_custom_inject_apply_bytes_to_slot(App* app) {
    if(!app) {
        return false;
    }

    bool selected[8] = {0};
    if(!cancommander_scene_custom_inject_parse_bytes_list(app->custom_inject_edit_bytes, selected)) {
        app_set_status(app, "Bytes list invalid (use 0..7 comma-separated)");
        return false;
    }

    uint8_t value_bytes[8] = {0};
    if(!cancommander_scene_custom_inject_parse_hex_bytes(app->custom_inject_edit_value_hex, value_bytes)) {
        app_set_status(app, "Value must be 8 bytes hex");
        return false;
    }

    uint8_t mask_bytes[8] = {0};
    uint8_t fixed_value[8] = {0};
    for(uint8_t i = 0U; i < 8U; i++) {
        if(selected[i]) {
            mask_bytes[i] = 0xFFU;
            fixed_value[i] = value_bytes[i];
        }
    }

    char mask_hex[17] = {0};
    char value_hex[17] = {0};
    cancommander_scene_custom_inject_hex_from_bytes(mask_bytes, mask_hex);
    cancommander_scene_custom_inject_hex_from_bytes(fixed_value, value_hex);

    const uint8_t slot_index = app_custom_inject_get_active_slot(app);
    char* slot_args = app_custom_inject_get_slot_args(app, slot_index);
    if(!slot_args) {
        return false;
    }

    app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "mask", mask_hex);
    app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "value", value_hex);
    app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "xor", "0000000000000000");
    app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "sig", "0");
    app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "ext", "0");
    app_custom_inject_save(app);
    app_set_status(app, "Set Bytes saved for slot %u", (unsigned)(slot_index + 1U));
    return true;
}

void cancommander_scene_custom_inject_bytes_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Set Bytes");

    submenu_add_item(
        app->submenu,
        "Bytes to Inject",
        CustomInjectBytesToInject,
        cancommander_scene_custom_inject_bytes_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Value",
        CustomInjectBytesValue,
        cancommander_scene_custom_inject_bytes_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save",
        CustomInjectBytesSave,
        cancommander_scene_custom_inject_bytes_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_custom_inject_bytes_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_custom_inject_bytes_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_custom_inject_bytes_menu, event.event);

    switch(event.event) {
    case CustomInjectBytesToInject:
        app_begin_edit(
            app,
            app->custom_inject_edit_bytes,
            sizeof(app->custom_inject_edit_bytes),
            "Bytes (0,1,5)");
        scene_manager_next_scene(app->scene_manager, cancommander_scene_text_input);
        return true;

    case CustomInjectBytesValue:
        app_begin_edit(
            app,
            app->custom_inject_edit_value_hex,
            sizeof(app->custom_inject_edit_value_hex),
            "Value");
        app->input_use_byte_input = true;
        app->input_hex_mode = AppHexInputBytes;
        app->input_hex_count = 8U;
        memset(app->input_hex_store, 0, sizeof(app->input_hex_store));
        (void)cancommander_scene_custom_inject_parse_hex_bytes(
            app->custom_inject_edit_value_hex, app->input_hex_store);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_text_input);
        return true;

    case CustomInjectBytesSave:
        if(cancommander_scene_custom_inject_apply_bytes_to_slot(app)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;

    default:
        return false;
    }
}

void cancommander_scene_custom_inject_bytes_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
