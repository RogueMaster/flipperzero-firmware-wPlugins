#include "../can_commander.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* value;
    const char* label;
} ArgChoice;

static const ArgChoice kBusChoices[] = {
    {.value = "can0", .label = "Bus 0"},
    {.value = "can1", .label = "Bus 1"},
    {.value = "both", .label = "Both"},
};

static const ArgChoice kBoolChoices[] = {
    {.value = "0", .label = "Off"},
    {.value = "1", .label = "On"},
};

static const ArgChoice kOrderChoices[] = {
    {.value = "intel", .label = "Intel"},
    {.value = "motorola", .label = "Motorola"},
};

static const ArgChoice kSignChoices[] = {
    {.value = "u", .label = "Unsigned"},
    {.value = "s", .label = "Signed"},
};

static const ArgChoice kModeListenChoices[] = {
    {.value = "normal", .label = "Normal"},
    {.value = "listen", .label = "Listen"},
};

static const ArgChoice kModeReverseChoices[] = {
    {.value = "auto", .label = "Auto"},
    {.value = "read", .label = "Read"},
};

static bool value_is_trueish(const char* value) {
    return strcmp(value, "1") == 0 || strcmp(value, "true") == 0 || strcmp(value, "on") == 0;
}

static AppArgValueType args_editor_detect_type(const char* key, const char* value) {
    if(strcmp(key, "bus") == 0) {
        return AppArgValueBus;
    }

    if(
        strcmp(key, "ascii") == 0 || strcmp(key, "ext") == 0 || strcmp(key, "rtr") == 0 ||
        strcmp(key, "ext_match") == 0 || strcmp(key, "strict") == 0 || strcmp(key, "mux") == 0) {
        return AppArgValueBool01;
    }

    if(strcmp(key, "order") == 0) {
        return AppArgValueOrder;
    }

    if(strcmp(key, "sign") == 0) {
        return AppArgValueSign;
    }

    if(strcmp(key, "mode") == 0) {
        if(strcmp(value, "normal") == 0 || strcmp(value, "listen") == 0 ||
           strcmp(value, "listen_only") == 0) {
            return AppArgValueModeListen;
        }
        if(strcmp(value, "auto") == 0 || strcmp(value, "read") == 0) {
            return AppArgValueModeReverse;
        }
    }

    return AppArgValueText;
}

static const ArgChoice* args_editor_get_choices(AppArgValueType type, uint8_t* count) {
    switch(type) {
    case AppArgValueBus:
        *count = 3;
        return kBusChoices;
    case AppArgValueBool01:
        *count = 2;
        return kBoolChoices;
    case AppArgValueOrder:
        *count = 2;
        return kOrderChoices;
    case AppArgValueSign:
        *count = 2;
        return kSignChoices;
    case AppArgValueModeListen:
        *count = 2;
        return kModeListenChoices;
    case AppArgValueModeReverse:
        *count = 2;
        return kModeReverseChoices;
    case AppArgValueText:
    default:
        *count = 0;
        return NULL;
    }
}

static uint8_t args_editor_get_choice_index(const AppArgItem* item, const ArgChoice* choices, uint8_t count) {
    if(item->type == AppArgValueBool01) {
        return value_is_trueish(item->value) ? 1 : 0;
    }

    if(item->type == AppArgValueModeListen && strcmp(item->value, "listen_only") == 0) {
        return 1;
    }

    for(uint8_t i = 0; i < count; i++) {
        if(strcmp(item->value, choices[i].value) == 0) {
            return i;
        }
    }

    return 0;
}

static void args_editor_apply_choice(AppArgItem* item, const ArgChoice* choices, uint8_t choice_index) {
    strncpy(item->value, choices[choice_index].value, sizeof(item->value) - 1U);
    item->value[sizeof(item->value) - 1U] = '\0';
}

static void args_editor_rebuild_target(App* app) {
    if(!app->args_editor_target || app->args_editor_target_size == 0U) {
        return;
    }

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

static void args_editor_parse(App* app) {
    app->args_editor_count = 0;

    if(!app->args_editor_target || app->args_editor_target[0] == '\0') {
        return;
    }

    char scratch[sizeof(app->input_work)] = {0};
    strncpy(scratch, app->args_editor_target, sizeof(scratch) - 1U);

    char* save_ptr = NULL;
    char* token = strtok_r(scratch, " ", &save_ptr);

    while(token && app->args_editor_count < APP_ARGS_EDITOR_MAX_ITEMS) {
        char* sep = strchr(token, '=');
        if(sep) {
            *sep = '\0';
            const char* key = token;
            const char* value = sep + 1;

            AppArgItem* item = &app->args_editor_items[app->args_editor_count];
            memset(item, 0, sizeof(AppArgItem));

            strncpy(item->key, key, sizeof(item->key) - 1U);
            strncpy(item->value, value, sizeof(item->value) - 1U);
            item->type = args_editor_detect_type(item->key, item->value);

            app->args_editor_count++;
        }

        token = strtok_r(NULL, " ", &save_ptr);
    }

    bool has_id = false;
    bool has_ext = false;
    for(uint8_t i = 0; i < app->args_editor_count; i++) {
        if(strcmp(app->args_editor_items[i].key, "id") == 0) {
            has_id = true;
        } else if(strcmp(app->args_editor_items[i].key, "ext") == 0) {
            has_ext = true;
        }
    }

    if(has_id && !has_ext && app->args_editor_count < APP_ARGS_EDITOR_MAX_ITEMS) {
        AppArgItem* item = &app->args_editor_items[app->args_editor_count];
        memset(item, 0, sizeof(AppArgItem));
        strncpy(item->key, "ext", sizeof(item->key) - 1U);
        strncpy(item->value, "0", sizeof(item->value) - 1U);
        item->type = AppArgValueBool01;
        app->args_editor_count++;
    }

    if(app->args_editor_apply_enabled && app->args_editor_count < APP_ARGS_EDITOR_MAX_ITEMS) {
        AppArgItem* action = &app->args_editor_items[app->args_editor_count];
        memset(action, 0, sizeof(AppArgItem));
        strncpy(
            action->key,
            app->args_editor_apply_label ? app->args_editor_apply_label : "Apply",
            sizeof(action->key) - 1U);
        strncpy(action->value, "Run", sizeof(action->value) - 1U);
        action->type = AppArgValueAction;
        app->args_editor_count++;
    }
}

static int args_editor_find_index(App* app, VariableItem* item) {
    for(uint8_t i = 0; i < app->args_editor_count; i++) {
        if(app->args_editor_var_items[i] == item) {
            return i;
        }
    }

    return -1;
}

static uint8_t args_editor_hex_nibble(char c, bool* ok) {
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
    return 0;
}

static uint8_t args_editor_find_data_byte_count(App* app) {
    for(uint8_t i = 0; i < app->args_editor_count; i++) {
        AppArgItem* item = &app->args_editor_items[i];
        if(strcmp(item->key, "dlc") == 0) {
            char* end = NULL;
            unsigned long value = strtoul(item->value, &end, 10);
            if(end != item->value && *end == '\0' && value > 0UL && value <= 8UL) {
                return (uint8_t)value;
            }
        }
    }

    return 8U;
}

static bool args_editor_prepare_hex_input(App* app, const AppArgItem* item) {
    if(!app || !item) {
        return false;
    }

    app->input_use_byte_input = false;
    app->input_hex_mode = AppHexInputNone;
    app->input_hex_count = 0U;
    memset(app->input_hex_store, 0, sizeof(app->input_hex_store));

    bool is_u32_hex = false;
    bool is_u16_hex = false;
    bool is_u8_hex = false;
    bool is_data_hex = false;
    bool is_data_hex_fixed8 = false;

    bool ext = false;
    bool has_value_key = false;
    bool has_xor_key = false;
    bool has_mask_key = false;
    for(uint8_t i = 0; i < app->args_editor_count; i++) {
        AppArgItem* probe = &app->args_editor_items[i];
        if(strcmp(probe->key, "ext") == 0) {
            ext = value_is_trueish(probe->value);
        } else if(strcmp(probe->key, "value") == 0) {
            has_value_key = true;
        } else if(strcmp(probe->key, "xor") == 0) {
            has_xor_key = true;
        } else if(strcmp(probe->key, "mask") == 0) {
            has_mask_key = true;
        }
    }

    if(strcmp(item->key, "id") == 0) {
        is_u32_hex = ext;
        is_u16_hex = !ext;
    } else if(strcmp(item->key, "mask") == 0 || strcmp(item->key, "filter") == 0) {
        if(strcmp(item->key, "mask") == 0 && (has_value_key || has_xor_key)) {
            is_data_hex = true;
            is_data_hex_fixed8 = true;
        } else {
            is_u32_hex = ext;
            is_u16_hex = !ext;
        }
    } else if(strcmp(item->key, "pid") == 0) {
        is_u8_hex = true;
    } else if((strcmp(item->key, "value") == 0 || strcmp(item->key, "xor") == 0) && has_mask_key) {
        is_data_hex = true;
        is_data_hex_fixed8 = true;
    } else if(strcmp(item->key, "data") == 0) {
        is_data_hex = true;
    }

    if(!is_u32_hex && !is_u16_hex && !is_u8_hex && !is_data_hex) {
        return false;
    }

    app->input_use_byte_input = true;

    if(is_u32_hex) {
        app->input_hex_mode = AppHexInputU32;
        app->input_hex_count = 4U;

        char* end = NULL;
        unsigned long value = strtoul(item->value, &end, 16);
        if(end != item->value && *end == '\0') {
            app->input_hex_store[0] = (uint8_t)((value >> 24) & 0xFFUL);
            app->input_hex_store[1] = (uint8_t)((value >> 16) & 0xFFUL);
            app->input_hex_store[2] = (uint8_t)((value >> 8) & 0xFFUL);
            app->input_hex_store[3] = (uint8_t)(value & 0xFFUL);
        }
    } else if(is_u16_hex) {
        app->input_hex_mode = AppHexInputU16;
        app->input_hex_count = 2U;

        char* end = NULL;
        unsigned long value = strtoul(item->value, &end, 16);
        if(end != item->value && *end == '\0') {
            value &= 0x7FFUL;
            app->input_hex_store[0] = (uint8_t)((value >> 8) & 0xFFUL);
            app->input_hex_store[1] = (uint8_t)(value & 0xFFUL);
        }
    } else if(is_u8_hex) {
        app->input_hex_mode = AppHexInputU8;
        app->input_hex_count = 1U;

        char* end = NULL;
        unsigned long value = strtoul(item->value, &end, 16);
        if(end != item->value && *end == '\0') {
            app->input_hex_store[0] = (uint8_t)(value & 0xFFUL);
        }
    } else {
        app->input_hex_mode = AppHexInputBytes;
        if(is_data_hex_fixed8) {
            app->input_hex_count = 8U;
        } else {
            app->input_hex_count = args_editor_find_data_byte_count(app);
        }
        if(app->input_hex_count > sizeof(app->input_hex_store)) {
            app->input_hex_count = sizeof(app->input_hex_store);
        }

        size_t text_index = 0U;
        size_t byte_index = 0U;
        const size_t text_len = strlen(item->value);

        while(byte_index < app->input_hex_count && (text_index + 1U) < text_len) {
            bool hi_ok = false;
            bool lo_ok = false;
            const uint8_t hi = args_editor_hex_nibble(item->value[text_index], &hi_ok);
            const uint8_t lo = args_editor_hex_nibble(item->value[text_index + 1U], &lo_ok);
            if(!hi_ok || !lo_ok) {
                break;
            }

            app->input_hex_store[byte_index] = (uint8_t)((hi << 4) | lo);
            byte_index++;
            text_index += 2U;
        }
    }

    return true;
}

static void args_editor_item_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const int index = args_editor_find_index(app, item);
    if(index < 0) {
        return;
    }

    AppArgItem* arg = &app->args_editor_items[index];
    if(arg->type == AppArgValueText || arg->type == AppArgValueAction) {
        return;
    }

    uint8_t count = 0;
    const ArgChoice* choices = args_editor_get_choices(arg->type, &count);
    if(!choices || count == 0) {
        return;
    }

    uint8_t choice_index = variable_item_get_current_value_index(item);
    if(choice_index >= count) {
        choice_index = 0;
    }

    args_editor_apply_choice(arg, choices, choice_index);
    variable_item_set_current_value_text(item, choices[choice_index].label);

    args_editor_rebuild_target(app);
}

static void args_editor_item_enter_callback(void* context, uint32_t index) {
    App* app = context;

    if(index >= app->args_editor_count) {
        return;
    }

    AppArgItem* item = &app->args_editor_items[index];
    if(item->type == AppArgValueAction) {
        if(app->args_editor_apply_callback) {
            app->args_editor_apply_callback(app);
            scene_manager_previous_scene(app->scene_manager);
            if(
                scene_manager_get_current_scene(app->scene_manager) !=
                app->args_editor_apply_next_scene) {
                scene_manager_next_scene(app->scene_manager, app->args_editor_apply_next_scene);
            }
        }
        return;
    }

    if(item->type != AppArgValueText) {
        return;
    }

    app->input_editing_arg_value = true;
    app->input_arg_value_index = (uint8_t)index;

    app->input_dest = NULL;
    app->input_dest_size = 0;
    app->input_header = item->key;

    memset(app->input_work, 0, sizeof(app->input_work));
    strncpy(app->input_work, item->value, sizeof(app->input_work) - 1U);
    args_editor_prepare_hex_input(app, item);

    scene_manager_next_scene(app->scene_manager, cancommander_scene_text_input);
}

void cancommander_scene_args_editor_on_enter(void* context) {
    App* app = context;

    variable_item_list_reset(app->var_list);
    args_editor_parse(app);

    for(uint8_t i = 0; i < app->args_editor_count; i++) {
        AppArgItem* item = &app->args_editor_items[i];

        uint8_t value_count = 1;
        if(item->type != AppArgValueText && item->type != AppArgValueAction) {
            args_editor_get_choices(item->type, &value_count);
        }

        VariableItem* var_item = variable_item_list_add(
            app->var_list, item->key, value_count, args_editor_item_change_callback, app);
        app->args_editor_var_items[i] = var_item;

        if(item->type == AppArgValueText) {
            variable_item_set_current_value_text(var_item, item->value);
        } else if(item->type == AppArgValueAction) {
            variable_item_set_current_value_text(var_item, item->value);
        } else {
            uint8_t count = 0;
            const ArgChoice* choices = args_editor_get_choices(item->type, &count);
            if(choices && count) {
                const uint8_t choice_index = args_editor_get_choice_index(item, choices, count);
                variable_item_set_current_value_index(var_item, choice_index);
                variable_item_set_current_value_text(var_item, choices[choice_index].label);
            } else {
                variable_item_set_current_value_text(var_item, item->value);
            }
        }
    }

    variable_item_list_set_enter_callback(app->var_list, args_editor_item_enter_callback, app);

    if(app->args_editor_title) {
        variable_item_list_set_header(app->var_list, app->args_editor_title);
    } else {
        variable_item_list_set_header(app->var_list, "Args");
    }

    if(app->args_editor_selected_index >= app->args_editor_count) {
        app->args_editor_selected_index = 0;
    }

    variable_item_list_set_selected_item(app->var_list, app->args_editor_selected_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVarList);
}

bool cancommander_scene_args_editor_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cancommander_scene_args_editor_on_exit(void* context) {
    App* app = context;

    app->args_editor_selected_index = variable_item_list_get_selected_item_index(app->var_list);
    variable_item_list_reset(app->var_list);
}
