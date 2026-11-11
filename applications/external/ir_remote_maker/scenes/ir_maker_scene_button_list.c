#include "../ir_maker.h"

/* Action rows live well above any real button index so a selection is trivially
 * classified as "a button" (index < count) or "an action". */
#define IR_MAKER_ROW_ADD_TYPE  (IR_MAKER_MAX_BUTTONS + 0U)
#define IR_MAKER_ROW_ADD_LEARN (IR_MAKER_MAX_BUTTONS + 1U)
#define IR_MAKER_ROW_DEFAULTS  (IR_MAKER_MAX_BUTTONS + 2U)
#define IR_MAKER_ROW_SAVE      (IR_MAKER_MAX_BUTTONS + 3U)

static void ir_maker_scene_button_list_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ir_maker_scene_button_list_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(
        app->submenu,
        furi_string_empty(app->draft->remote_name) ?
            "Remote" :
            furi_string_get_cstr(app->draft->remote_name));

    char label[IR_MAKER_NAME_MAX + 16];
    for(size_t i = 0; i < app->draft->count; i++) {
        const IrMakerButton* button = &app->draft->items[i];
        const char* name = furi_string_get_cstr(button->name);
        if(infrared_signal_is_raw(button->signal)) {
            snprintf(label, sizeof(label), "%s  RAW", name);
        } else {
            const InfraredMessage* msg = infrared_signal_get_message(button->signal);
            snprintf(
                label,
                sizeof(label),
                "%s  %0*lX",
                name,
                (int)ROUND_UP_TO(infrared_get_protocol_command_length(msg->protocol), 4),
                (unsigned long)msg->command);
        }
        submenu_add_item(app->submenu, label, i, ir_maker_scene_button_list_submenu_callback, app);
    }

    submenu_add_item(
        app->submenu,
        "+ Add (type it)",
        IR_MAKER_ROW_ADD_TYPE,
        ir_maker_scene_button_list_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "+ Add (learn)",
        IR_MAKER_ROW_ADD_LEARN,
        ir_maker_scene_button_list_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Remote defaults",
        IR_MAKER_ROW_DEFAULTS,
        ir_maker_scene_button_list_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save remote",
        IR_MAKER_ROW_SAVE,
        ir_maker_scene_button_list_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, IrMakerSceneButtonList));
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewSubmenu);
}

bool ir_maker_scene_button_list_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, IrMakerSceneButtonList, event.event);
        if(event.event == IR_MAKER_ROW_ADD_TYPE) {
            app->edit_mode = IrMakerEditModeNew;
            app->edit_msg.protocol = app->def_protocol;
            app->edit_msg.address = app->def_address;
            app->edit_msg.command = 0;
            app->edit_msg.repeat = false;
            ir_maker_clamp_message(&app->edit_msg);
            scene_manager_next_scene(app->scene_manager, IrMakerSceneAddManual);
            consumed = true;
        } else if(event.event == IR_MAKER_ROW_ADD_LEARN) {
            app->edit_mode = IrMakerEditModeNew;
            scene_manager_next_scene(app->scene_manager, IrMakerSceneLearn);
            consumed = true;
        } else if(event.event == IR_MAKER_ROW_DEFAULTS) {
            scene_manager_next_scene(app->scene_manager, IrMakerSceneDefaults);
            consumed = true;
        } else if(event.event == IR_MAKER_ROW_SAVE) {
            scene_manager_next_scene(app->scene_manager, IrMakerSceneSave);
            consumed = true;
        } else if(event.event < app->draft->count) {
            app->selected_button = event.event;
            scene_manager_next_scene(app->scene_manager, IrMakerSceneButtonEdit);
            consumed = true;
        }
    }

    return consumed;
}

void ir_maker_scene_button_list_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    submenu_reset(app->submenu);
}
