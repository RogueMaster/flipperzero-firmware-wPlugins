#include "../can_commander.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    CustomInjectSlotName = 0,
    CustomInjectSlotBus,
    CustomInjectSlotId,
    CustomInjectSlotSetBytes,
    CustomInjectSlotSetBit,
    CustomInjectSlotSetField,
    CustomInjectSlotMux,
    CustomInjectSlotCount,
    CustomInjectSlotInterval,
    CustomInjectSlotClear,
    CustomInjectSlotSave,
} CustomInjectSlotMenuIndex;

static char cancommander_custom_slot_name_item[28];
static char cancommander_custom_slot_bus_item[20];
static char cancommander_custom_slot_id_item[20];
static char cancommander_custom_slot_mux_item[20];
static char cancommander_custom_slot_count_item[20];
static char cancommander_custom_slot_interval_item[28];

static uint8_t cancommander_scene_custom_inject_slot_selected_number(App* app) {
    return (uint8_t)(app_custom_inject_get_active_slot(app) + 1U);
}

static void cancommander_scene_custom_inject_slot_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool cancommander_scene_custom_inject_get_arg(
    const char* args,
    const char* key,
    char* out,
    size_t out_size) {
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

static bool cancommander_scene_custom_inject_parse_hex_bytes(
    const char* hex,
    uint8_t out[8]) {
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

static void cancommander_scene_custom_inject_hex_from_bytes(
    const uint8_t in[8],
    char out[17]) {
    static const char kHex[] = "0123456789ABCDEF";
    for(uint8_t i = 0U; i < 8U; i++) {
        out[i * 2U] = kHex[(uint8_t)((in[i] >> 4U) & 0x0FU)];
        out[i * 2U + 1U] = kHex[(uint8_t)(in[i] & 0x0FU)];
    }
    out[16] = '\0';
}

static void cancommander_scene_custom_inject_prepare_bytes_editor(App* app) {
    char* slot_args =
        app_custom_inject_get_slot_args(app, app_custom_inject_get_active_slot(app));
    if(!slot_args) {
        strncpy(app->custom_inject_edit_bytes, "0", sizeof(app->custom_inject_edit_bytes) - 1U);
        strncpy(
            app->custom_inject_edit_value_hex,
            "0000000000000000",
            sizeof(app->custom_inject_edit_value_hex) - 1U);
        return;
    }

    char mask_hex[24] = {0};
    char value_hex[24] = {0};
    uint8_t mask[8] = {0};
    uint8_t value[8] = {0};

    if(cancommander_scene_custom_inject_get_arg(slot_args, "mask", mask_hex, sizeof(mask_hex))) {
        (void)cancommander_scene_custom_inject_parse_hex_bytes(mask_hex, mask);
    }
    if(cancommander_scene_custom_inject_get_arg(slot_args, "value", value_hex, sizeof(value_hex))) {
        (void)cancommander_scene_custom_inject_parse_hex_bytes(value_hex, value);
    }

    app->custom_inject_edit_bytes[0] = '\0';
    bool any = false;
    for(uint8_t i = 0U; i < 8U; i++) {
        if(mask[i] != 0U) {
            char idx[4] = {0};
            snprintf(idx, sizeof(idx), "%s%u", any ? "," : "", (unsigned)i);
            const size_t used = strlen(app->custom_inject_edit_bytes);
            if(used < (sizeof(app->custom_inject_edit_bytes) - 1U)) {
                snprintf(
                    app->custom_inject_edit_bytes + used,
                    sizeof(app->custom_inject_edit_bytes) - used,
                    "%s",
                    idx);
            }
            any = true;
        }
    }
    if(!any) {
        strncpy(app->custom_inject_edit_bytes, "0", sizeof(app->custom_inject_edit_bytes) - 1U);
    }

    cancommander_scene_custom_inject_hex_from_bytes(value, app->custom_inject_edit_value_hex);
}

static void cancommander_scene_custom_inject_slot_ensure_running(App* app) {
    if(!app) {
        return;
    }

    const bool custom_inject_active =
        app->tool_active && app->dashboard_mode == AppDashboardCustomInject;
    if(custom_inject_active) {
        return;
    }

    if(app->tool_active) {
        app_action_tool_stop(app);
    }

    app_action_tool_start(app, CcToolCustomInject, app->args_custom_inject_start, "custom_inject");
}

static void cancommander_scene_custom_inject_slot_refresh_labels(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    if(!slot_args) {
        return;
    }

    char slot_name[18] = {0};
    char bus[12] = {0};
    char id[12] = {0};
    char mux[8] = {0};
    char count[12] = {0};
    char interval[12] = {0};

    if(!cancommander_scene_custom_inject_get_arg(slot_args, "slot_name", slot_name, sizeof(slot_name))) {
        snprintf(slot_name, sizeof(slot_name), "Slot%u", (unsigned)slot_number);
    }
    if(!cancommander_scene_custom_inject_get_arg(slot_args, "bus", bus, sizeof(bus))) {
        strncpy(bus, "can0", sizeof(bus) - 1U);
    }
    if(!cancommander_scene_custom_inject_get_arg(slot_args, "id", id, sizeof(id))) {
        strncpy(id, "000", sizeof(id) - 1U);
    }
    if(!cancommander_scene_custom_inject_get_arg(slot_args, "mux", mux, sizeof(mux))) {
        strncpy(mux, "0", sizeof(mux) - 1U);
    }
    if(!cancommander_scene_custom_inject_get_arg(slot_args, "count", count, sizeof(count))) {
        strncpy(count, "1", sizeof(count) - 1U);
    }
    if(!cancommander_scene_custom_inject_get_arg(slot_args, "interval_ms", interval, sizeof(interval))) {
        strncpy(interval, "0", sizeof(interval) - 1U);
    }

    snprintf(cancommander_custom_slot_name_item, sizeof(cancommander_custom_slot_name_item), "Name: %s", slot_name);
    snprintf(cancommander_custom_slot_bus_item, sizeof(cancommander_custom_slot_bus_item), "Bus: %s", bus);
    snprintf(cancommander_custom_slot_id_item, sizeof(cancommander_custom_slot_id_item), "ID: %s", id);
    snprintf(
        cancommander_custom_slot_mux_item,
        sizeof(cancommander_custom_slot_mux_item),
        "Mux: %s",
        (strcmp(mux, "1") == 0 || strcmp(mux, "true") == 0) ? "On" : "Off");
    snprintf(cancommander_custom_slot_count_item, sizeof(cancommander_custom_slot_count_item), "Count: %s", count);
    snprintf(
        cancommander_custom_slot_interval_item,
        sizeof(cancommander_custom_slot_interval_item),
        "Interval: %s",
        interval);
}

static void cancommander_scene_custom_inject_slot_rebuild_menu(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);

    cancommander_scene_custom_inject_slot_refresh_labels(app);
    submenu_reset(app->submenu);

    char header[16] = {0};
    snprintf(header, sizeof(header), "Slot %u", (unsigned)slot_number);
    submenu_set_header(app->submenu, header);

    submenu_add_item(
        app->submenu,
        cancommander_custom_slot_name_item,
        CustomInjectSlotName,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_slot_bus_item,
        CustomInjectSlotBus,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_slot_id_item,
        CustomInjectSlotId,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Set Bytes",
        CustomInjectSlotSetBytes,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Set Bit",
        CustomInjectSlotSetBit,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Set Field",
        CustomInjectSlotSetField,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_slot_mux_item,
        CustomInjectSlotMux,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_slot_count_item,
        CustomInjectSlotCount,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_slot_interval_item,
        CustomInjectSlotInterval,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Clear Slot",
        CustomInjectSlotClear,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save",
        CustomInjectSlotSave,
        cancommander_scene_custom_inject_slot_menu_callback,
        app);
}

static void cancommander_scene_custom_inject_slot_apply_name(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    char name[24] = {0};
    if(slot_args && cancommander_scene_custom_inject_get_arg(
                        app->custom_inject_edit_name, "slot_name", name, sizeof(name))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "slot_name", name);
        app_custom_inject_save(app);
    }
}

static void cancommander_scene_custom_inject_slot_apply_bus(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    char bus[12] = {0};
    if(slot_args && cancommander_scene_custom_inject_get_arg(
                        app->custom_inject_edit_bus, "bus", bus, sizeof(bus))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "bus", bus);
    }
}

static void cancommander_scene_custom_inject_slot_apply_id(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    char id[16] = {0};
    if(slot_args &&
       cancommander_scene_custom_inject_get_arg(app->custom_inject_edit_id, "id", id, sizeof(id))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "id", id);
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "ext", "0");
    }
}

static void cancommander_scene_custom_inject_slot_apply_mux(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    if(!slot_args) {
        return;
    }

    char mux[8] = {0};
    char mux_start[16] = {0};
    char mux_len[16] = {0};
    char mux_value[16] = {0};

    if(cancommander_scene_custom_inject_get_arg(app->custom_inject_edit_mux, "mux", mux, sizeof(mux))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "mux", mux);
    }
    if(cancommander_scene_custom_inject_get_arg(
           app->custom_inject_edit_mux, "mux_start", mux_start, sizeof(mux_start))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "mux_start", mux_start);
    }
    if(cancommander_scene_custom_inject_get_arg(
           app->custom_inject_edit_mux, "mux_len", mux_len, sizeof(mux_len))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "mux_len", mux_len);
    }
    if(cancommander_scene_custom_inject_get_arg(
           app->custom_inject_edit_mux, "mux_value", mux_value, sizeof(mux_value))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "mux_value", mux_value);
    }

    app_custom_inject_save(app);
}

static void cancommander_scene_custom_inject_slot_apply_count(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    char count[12] = {0};
    if(slot_args && cancommander_scene_custom_inject_get_arg(
                        app->custom_inject_edit_count, "count", count, sizeof(count))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "count", count);
    }
}

static void cancommander_scene_custom_inject_slot_apply_interval(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    char interval[16] = {0};
    if(slot_args && cancommander_scene_custom_inject_get_arg(
                        app->custom_inject_edit_interval, "interval_ms", interval, sizeof(interval))) {
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "interval_ms", interval);
    }
}

static void cancommander_scene_custom_inject_slot_apply_set_bit(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char bit[12] = {0};
    char value[8] = {0};
    if(!cancommander_scene_custom_inject_get_arg(app->custom_inject_edit_bit, "bit", bit, sizeof(bit)) ||
       !cancommander_scene_custom_inject_get_arg(app->custom_inject_edit_bit, "value", value, sizeof(value))) {
        app_set_status(app, "Set Bit parse error");
        return;
    }

    snprintf(
        app->args_custom_inject_bit,
        sizeof(app->args_custom_inject_bit),
        "slot=%u bit=%s value=%s",
        (unsigned)slot_number,
        bit,
        value);
    app_action_custom_inject_bit(app);
}

static void cancommander_scene_custom_inject_slot_apply_set_field(App* app) {
    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char start[12] = {0};
    char len[12] = {0};
    char value[24] = {0};

    if(!cancommander_scene_custom_inject_get_arg(
           app->custom_inject_edit_field, "start", start, sizeof(start)) ||
       !cancommander_scene_custom_inject_get_arg(
           app->custom_inject_edit_field, "len", len, sizeof(len)) ||
       !cancommander_scene_custom_inject_get_arg(
           app->custom_inject_edit_field, "value", value, sizeof(value))) {
        app_set_status(app, "Set Field parse error");
        return;
    }

    snprintf(
        app->args_custom_inject_field,
        sizeof(app->args_custom_inject_field),
        "slot=%u start=%s len=%s value=%s",
        (unsigned)slot_number,
        start,
        len,
        value);
    app_action_custom_inject_field(app);
}

void cancommander_scene_custom_inject_slot_menu_on_enter(void* context) {
    App* app = context;

    cancommander_scene_custom_inject_slot_ensure_running(app);
    cancommander_scene_custom_inject_slot_rebuild_menu(app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_custom_inject_slot_menu));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_custom_inject_slot_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_custom_inject_slot_menu, event.event);

    const uint8_t slot_number = cancommander_scene_custom_inject_slot_selected_number(app);
    char* slot_args = app_custom_inject_get_slot_args(app, (uint8_t)(slot_number - 1U));
    if(!slot_args) {
        return false;
    }

    char value[24] = {0};

    switch(event.event) {
    case CustomInjectSlotName:
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "slot_name", value, sizeof(value))) {
            snprintf(value, sizeof(value), "Slot%u", (unsigned)slot_number);
        }
        snprintf(
            app->custom_inject_edit_name,
            sizeof(app->custom_inject_edit_name),
            "slot_name=%s",
            value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_name,
            sizeof(app->custom_inject_edit_name),
            "Slot Name",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_name,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case CustomInjectSlotBus:
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "bus", value, sizeof(value))) {
            strncpy(value, "can0", sizeof(value) - 1U);
        }
        snprintf(app->custom_inject_edit_bus, sizeof(app->custom_inject_edit_bus), "bus=%s", value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_bus,
            sizeof(app->custom_inject_edit_bus),
            "Bus",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_bus,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case CustomInjectSlotId:
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "id", value, sizeof(value))) {
            strncpy(value, "000", sizeof(value) - 1U);
        }
        snprintf(app->custom_inject_edit_id, sizeof(app->custom_inject_edit_id), "id=%s", value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_id,
            sizeof(app->custom_inject_edit_id),
            "ID",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_id,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case CustomInjectSlotSetBytes:
        cancommander_scene_custom_inject_prepare_bytes_editor(app);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_custom_inject_bytes_menu);
        return true;

    case CustomInjectSlotSetBit: {
        char bit[12] = {0};
        char bit_value[8] = {0};
        if(!cancommander_scene_custom_inject_get_arg(app->args_custom_inject_bit, "bit", bit, sizeof(bit))) {
            strncpy(bit, "0", sizeof(bit) - 1U);
        }
        if(!cancommander_scene_custom_inject_get_arg(
               app->args_custom_inject_bit, "value", bit_value, sizeof(bit_value))) {
            strncpy(bit_value, "1", sizeof(bit_value) - 1U);
        }
        snprintf(
            app->custom_inject_edit_bit,
            sizeof(app->custom_inject_edit_bit),
            "bit=%s value=%s",
            bit,
            bit_value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_bit,
            sizeof(app->custom_inject_edit_bit),
            "Set Bit",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_set_bit,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;
    }

    case CustomInjectSlotSetField: {
        char start[12] = {0};
        char len[12] = {0};
        char field_value[24] = {0};
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "sig_start", start, sizeof(start))) {
            strncpy(start, "0", sizeof(start) - 1U);
        }
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "sig_len", len, sizeof(len))) {
            strncpy(len, "1", sizeof(len) - 1U);
        }
        if(!cancommander_scene_custom_inject_get_arg(
               slot_args, "sig_value", field_value, sizeof(field_value))) {
            strncpy(field_value, "0", sizeof(field_value) - 1U);
        }
        snprintf(
            app->custom_inject_edit_field,
            sizeof(app->custom_inject_edit_field),
            "start=%s len=%s value=%s",
            start,
            len,
            field_value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_field,
            sizeof(app->custom_inject_edit_field),
            "Set Field",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_set_field,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;
    }

    case CustomInjectSlotMux:
        {
            char mux[8] = {0};
            char mux_start[16] = {0};
            char mux_len[16] = {0};
            char mux_value[16] = {0};

            if(!cancommander_scene_custom_inject_get_arg(slot_args, "mux", mux, sizeof(mux))) {
                strncpy(mux, "0", sizeof(mux) - 1U);
            }
            if(!cancommander_scene_custom_inject_get_arg(
                   slot_args, "mux_start", mux_start, sizeof(mux_start))) {
                strncpy(mux_start, "0", sizeof(mux_start) - 1U);
            }
            if(!cancommander_scene_custom_inject_get_arg(slot_args, "mux_len", mux_len, sizeof(mux_len))) {
                strncpy(mux_len, "1", sizeof(mux_len) - 1U);
            }
            if(!cancommander_scene_custom_inject_get_arg(
                   slot_args, "mux_value", mux_value, sizeof(mux_value))) {
                strncpy(mux_value, "0", sizeof(mux_value) - 1U);
            }

            snprintf(
                app->custom_inject_edit_mux,
                sizeof(app->custom_inject_edit_mux),
                "mux=%s mux_start=%s mux_len=%s mux_value=%s",
                mux,
                mux_start,
                mux_len,
                mux_value);
            app_begin_args_editor_apply(
                app,
                app->custom_inject_edit_mux,
                sizeof(app->custom_inject_edit_mux),
                "Mux",
                "Apply",
                cancommander_scene_custom_inject_slot_apply_mux,
                cancommander_scene_custom_inject_slot_menu);
            scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        }
        return true;

    case CustomInjectSlotCount:
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "count", value, sizeof(value))) {
            strncpy(value, "1", sizeof(value) - 1U);
        }
        snprintf(app->custom_inject_edit_count, sizeof(app->custom_inject_edit_count), "count=%s", value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_count,
            sizeof(app->custom_inject_edit_count),
            "Count",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_count,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case CustomInjectSlotInterval:
        if(!cancommander_scene_custom_inject_get_arg(slot_args, "interval_ms", value, sizeof(value))) {
            strncpy(value, "0", sizeof(value) - 1U);
        }
        snprintf(
            app->custom_inject_edit_interval,
            sizeof(app->custom_inject_edit_interval),
            "interval_ms=%s",
            value);
        app_begin_args_editor_apply(
            app,
            app->custom_inject_edit_interval,
            sizeof(app->custom_inject_edit_interval),
            "Interval ms",
            "Apply",
            cancommander_scene_custom_inject_slot_apply_interval,
            cancommander_scene_custom_inject_slot_menu);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case CustomInjectSlotClear:
        app_action_custom_inject_remove(app, slot_number);
        cancommander_scene_custom_inject_slot_rebuild_menu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
        return true;

    case CustomInjectSlotSave:
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "ext", "0");
        app_args_set_key_value(slot_args, sizeof(app->args_custom_inject_slots[0]), "used", "1");
        app_action_custom_inject_add(app, slot_number);
        app_action_custom_inject_modify(app, slot_number);
        app_custom_inject_save(app);
        scene_manager_previous_scene(app->scene_manager);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_custom_inject_slot_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
