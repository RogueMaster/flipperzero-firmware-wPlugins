#include "../ir_maker.h"

typedef enum {
    IrMakerEditItemTest,
    IrMakerEditItemRename,
    IrMakerEditItemEditSignal,
    IrMakerEditItemRelearn,
    IrMakerEditItemDuplicate,
    IrMakerEditItemMoveUp,
    IrMakerEditItemMoveDown,
    IrMakerEditItemDelete,
} IrMakerEditItem;

static void ir_maker_scene_button_edit_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void ir_maker_scene_button_edit_populate(IrMakerApp* app) {
    const IrMakerButton* button = &app->draft->items[app->selected_button];
    const bool is_raw = infrared_signal_is_raw(button->signal);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, furi_string_get_cstr(button->name));

    submenu_add_item(
        app->submenu,
        "Test",
        IrMakerEditItemTest,
        ir_maker_scene_button_edit_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Rename",
        IrMakerEditItemRename,
        ir_maker_scene_button_edit_submenu_callback,
        app);
    /* A raw capture has no protocol/address/command to type, so only offer
     * manual editing for parsed signals. */
    if(!is_raw) {
        submenu_add_item(
            app->submenu,
            "Edit signal",
            IrMakerEditItemEditSignal,
            ir_maker_scene_button_edit_submenu_callback,
            app);
    }
    submenu_add_item(
        app->submenu,
        "Re-learn",
        IrMakerEditItemRelearn,
        ir_maker_scene_button_edit_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Duplicate",
        IrMakerEditItemDuplicate,
        ir_maker_scene_button_edit_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Move up",
        IrMakerEditItemMoveUp,
        ir_maker_scene_button_edit_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Move down",
        IrMakerEditItemMoveDown,
        ir_maker_scene_button_edit_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Delete",
        IrMakerEditItemDelete,
        ir_maker_scene_button_edit_submenu_callback,
        app);
}

void ir_maker_scene_button_edit_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    ir_maker_scene_button_edit_populate(app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, IrMakerSceneButtonEdit));
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewSubmenu);
}

bool ir_maker_scene_button_edit_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, IrMakerSceneButtonEdit, event.event);
    IrMakerButton* button = &app->draft->items[app->selected_button];

    switch(event.event) {
    case IrMakerEditItemTest:
        ir_maker_send_test(button->signal);
        notification_message(app->notifications, &sequence_blink_cyan_10);
        consumed = true;
        break;
    case IrMakerEditItemRename:
        app->edit_mode = IrMakerEditModeRename;
        scene_manager_next_scene(app->scene_manager, IrMakerSceneButtonName);
        consumed = true;
        break;
    case IrMakerEditItemEditSignal:
        app->edit_mode = IrMakerEditModeSignal;
        scene_manager_next_scene(app->scene_manager, IrMakerSceneAddManual);
        consumed = true;
        break;
    case IrMakerEditItemRelearn:
        app->edit_mode = IrMakerEditModeSignal;
        scene_manager_next_scene(app->scene_manager, IrMakerSceneLearn);
        consumed = true;
        break;
    case IrMakerEditItemDuplicate:
        /* Verbatim copy, same name, no auto-increment of any command. */
        if(ir_maker_draft_add(app->draft, furi_string_get_cstr(button->name), button->signal)) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            notification_message(app->notifications, &sequence_blink_red_10);
        }
        consumed = true;
        break;
    case IrMakerEditItemMoveUp:
        if(app->selected_button > 0) {
            ir_maker_draft_move(app->draft, app->selected_button, app->selected_button - 1);
            app->selected_button--;
            ir_maker_scene_button_edit_populate(app);
        }
        consumed = true;
        break;
    case IrMakerEditItemMoveDown:
        if(app->selected_button + 1 < app->draft->count) {
            ir_maker_draft_move(app->draft, app->selected_button, app->selected_button + 1);
            app->selected_button++;
            ir_maker_scene_button_edit_populate(app);
        }
        consumed = true;
        break;
    case IrMakerEditItemDelete:
        ir_maker_draft_delete(app->draft, app->selected_button);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
        break;
    default:
        break;
    }

    return consumed;
}

void ir_maker_scene_button_edit_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    submenu_reset(app->submenu);
}
