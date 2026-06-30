#include "../can_commander.h"

#include <string.h>
#include <stdio.h>

typedef enum {
    CustomInjectStartTool = 0,
    CustomInjectSlot1,
    CustomInjectSlot2,
    CustomInjectSlot3,
    CustomInjectSlot4,
    CustomInjectSlot5,
    CustomInjectListSlots,
    CustomInjectClearAllSlots,
    CustomInjectSaveSlots,
    CustomInjectLoadSlots,
} CustomInjectMenuIndex;

static char cancommander_custom_inject_slot_label_cache[5][24];

static void cancommander_scene_custom_inject_menu_callback(void* context, uint32_t index) {
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

static void cancommander_scene_custom_inject_slot_label(
    App* app,
    uint8_t slot_index,
    char* out,
    size_t out_size) {
    if(!app || !out || out_size == 0U) {
        return;
    }

    const char* slot_args = app_custom_inject_get_slot_args(app, slot_index);
    char slot_name[24] = {0};
    if(slot_args && cancommander_scene_custom_inject_get_arg(slot_args, "slot_name", slot_name, sizeof(slot_name)) &&
       slot_name[0] != '\0') {
        snprintf(out, out_size, "%s", slot_name);
    } else {
        snprintf(out, out_size, "Slot %u", (unsigned)(slot_index + 1U));
    }
    out[out_size - 1U] = '\0';
}

static void cancommander_scene_custom_inject_ensure_running(App* app) {
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

void cancommander_scene_custom_inject_menu_on_enter(void* context) {
    App* app = context;

    cancommander_scene_custom_inject_ensure_running(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Smart Injection");

    for(uint8_t i = 0U; i < 5U; i++) {
        cancommander_scene_custom_inject_slot_label(
            app,
            i,
            cancommander_custom_inject_slot_label_cache[i],
            sizeof(cancommander_custom_inject_slot_label_cache[i]));
    }

    submenu_add_item(
        app->submenu,
        "Start",
        CustomInjectStartTool,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_inject_slot_label_cache[0],
        CustomInjectSlot1,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_inject_slot_label_cache[1],
        CustomInjectSlot2,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_inject_slot_label_cache[2],
        CustomInjectSlot3,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_inject_slot_label_cache[3],
        CustomInjectSlot4,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        cancommander_custom_inject_slot_label_cache[4],
        CustomInjectSlot5,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "List Slots",
        CustomInjectListSlots,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Clear All Slots",
        CustomInjectClearAllSlots,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save Profile",
        CustomInjectSaveSlots,
        cancommander_scene_custom_inject_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Load Profile",
        CustomInjectLoadSlots,
        cancommander_scene_custom_inject_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_custom_inject_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_custom_inject_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_custom_inject_menu, event.event);

    switch(event.event) {
    case CustomInjectStartTool:
        cancommander_scene_custom_inject_ensure_running(app);
        app_action_custom_inject_sync_slots(app);
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case CustomInjectSlot1:
    case CustomInjectSlot2:
    case CustomInjectSlot3:
    case CustomInjectSlot4:
    case CustomInjectSlot5:
        app_custom_inject_set_active_slot(
            app, (uint8_t)(event.event - (uint32_t)CustomInjectSlot1));
        app_custom_inject_save(app);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_custom_inject_slot_menu);
        return true;

    case CustomInjectListSlots:
        app_action_custom_inject_list(app);
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case CustomInjectClearAllSlots:
        app_action_custom_inject_clear(app);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case CustomInjectSaveSlots:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_custom_inject_save_slots_menu);
        return true;

    case CustomInjectLoadSlots:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_custom_inject_load_slots_menu);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_custom_inject_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
