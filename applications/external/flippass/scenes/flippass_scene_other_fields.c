#include "flippass_scene_other_fields.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_db_browser_view.h"
#include "flippass_scene.h"
#include "flippass_scene_other_field_actions.h"
#include "flippass_scene_status.h"

#include <stdio.h>

#define FLIPPASS_OTHER_FIELDS_MAX_ITEMS 32U

enum {
    FlipPassSceneOtherFieldsEventShowSelected = 1,
    FlipPassSceneOtherFieldsEventExecuteUsbAction,
    FlipPassSceneOtherFieldsEventExecuteBluetoothAction,
    FlipPassSceneOtherFieldsEventRunPendingAction = 0x200U,
};

typedef struct {
    uint32_t field_mask;
    KDBXCustomField* custom_field;
    const char* label;
} FlipPassOtherFieldItem;

static FlipPassOtherFieldItem flippass_other_field_items[FLIPPASS_OTHER_FIELDS_MAX_ITEMS];
static size_t flippass_other_field_count = 0U;

static void flippass_other_fields_add_item(
    uint32_t field_mask,
    KDBXCustomField* custom_field,
    const char* label) {
    if(flippass_other_field_count >= FLIPPASS_OTHER_FIELDS_MAX_ITEMS) {
        return;
    }

    FlipPassOtherFieldItem* item = &flippass_other_field_items[flippass_other_field_count];
    item->field_mask = field_mask;
    item->custom_field = custom_field;
    item->label = label != NULL ? label : "";
    flippass_other_field_count++;
}

static void flippass_other_fields_sync_selection_from_view(App* app) {
    furi_assert(app);
    app->other_field_selected_index = flippass_db_browser_view_get_selected_item(app->db_browser);
}

static const FlipPassOtherFieldItem* flippass_other_fields_get_selected_item(App* app) {
    furi_assert(app);

    flippass_other_fields_sync_selection_from_view(app);
    if(app->other_field_selected_index >= flippass_other_field_count) {
        return NULL;
    }

    return &flippass_other_field_items[app->other_field_selected_index];
}

static bool flippass_other_fields_select_current_item(App* app) {
    const FlipPassOtherFieldItem* item = flippass_other_fields_get_selected_item(app);

    if(item == NULL) {
        return false;
    }

    app->pending_other_field_mask = item->field_mask;
    app->pending_other_custom_field = item->custom_field;
    snprintf(
        app->pending_other_field_name,
        sizeof(app->pending_other_field_name),
        "%s",
        item->label);
    return true;
}

static void flippass_other_fields_render(App* app, const KDBXEntry* entry) {
    flippass_other_field_count = 0U;

    flippass_db_browser_view_reset(app->db_browser);
    flippass_db_browser_view_set_mode(app->db_browser, FlipPassDbBrowserModeDirectActions);
    flippass_db_browser_view_set_has_parent(app->db_browser, false);
    flippass_db_browser_view_set_header(app->db_browser, "Other Fields");

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUrl)) {
        flippass_other_fields_add_item(KDBXEntryFieldUrl, NULL, "URL");
    }

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldNotes)) {
        flippass_other_fields_add_item(KDBXEntryFieldNotes, NULL, "Notes");
    }

    for(const KDBXCustomField* field = flippass_db_entry_get_custom_fields(entry); field != NULL;
        field = field->next) {
        flippass_other_fields_add_item(0U, (KDBXCustomField*)field, field->key);
    }

    if(flippass_other_field_count == 0U) {
        flippass_db_browser_view_add_item(
            app->db_browser, FlipPassDbBrowserItemTypeInfo, "No other fields");
        app->other_field_selected_index = 0U;
    } else {
        for(uint32_t index = 0U; index < flippass_other_field_count; index++) {
            flippass_db_browser_view_add_item(
                app->db_browser,
                FlipPassDbBrowserItemTypeField,
                flippass_other_field_items[index].label);
        }

        if(app->other_field_selected_index >= flippass_other_field_count) {
            app->other_field_selected_index = 0U;
        }
    }

    flippass_db_browser_view_set_selected_item(app->db_browser, app->other_field_selected_index);
}

static void flippass_other_fields_view_callback(FlipPassDbBrowserEvent event, void* context) {
    App* app = context;
    uint32_t custom_event = 0U;

    switch(event) {
    case FlipPassDbBrowserEventShow:
        custom_event = FlipPassSceneOtherFieldsEventShowSelected;
        break;
    case FlipPassDbBrowserEventTypeUsb:
        custom_event = FlipPassSceneOtherFieldsEventExecuteUsbAction;
        break;
    case FlipPassDbBrowserEventTypeBluetooth:
        custom_event = FlipPassSceneOtherFieldsEventExecuteBluetoothAction;
        break;
    default:
        break;
    }

    if(custom_event != 0U) {
        view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
    }
}

void flippass_scene_other_fields_on_enter(void* context) {
    App* app = context;
    const KDBXEntry* entry = app->active_entry;
    FuriString* error = furi_string_alloc();

    if(entry == NULL) {
        flippass_scene_status_show(
            app,
            "No Entry Selected",
            "Return to the browser and pick an entry first.",
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }

    if(!flippass_db_activate_entry(app, app->active_entry, false, error)) {
        flippass_scene_status_show(
            app,
            "Entry Load Failed",
            furi_string_get_cstr(error),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }
    furi_string_free(error);

    flippass_db_browser_view_set_callback(app->db_browser, flippass_other_fields_view_callback, app);
    flippass_other_fields_render(app, entry);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
}

bool flippass_scene_other_fields_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(!flippass_other_fields_select_current_item(app)) {
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventShowSelected) {
            app->other_field_action_selected_index = 1U;
            flippass_other_field_show_selected_value(app, FlipPassScene_OtherFields);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventExecuteUsbAction) {
            flippass_other_field_begin_type_action(
                app, false, FlipPassSceneOtherFieldsEventRunPendingAction);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventExecuteBluetoothAction) {
            flippass_other_field_begin_type_action(
                app, true, FlipPassSceneOtherFieldsEventRunPendingAction);
            return true;
        }

        if(event.event == FlipPassSceneOtherFieldsEventRunPendingAction) {
            flippass_other_field_run_pending_type_action(app, FlipPassScene_OtherFields);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void flippass_scene_other_fields_on_exit(void* context) {
    App* app = context;
    flippass_other_fields_sync_selection_from_view(app);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
}
