/**
 * @file flippass_scene_entry_detail.c
 * @brief Implementation of the KeePass entry detail scene.
 *
 * This scene uses the shared widget slot to present the selected entry in a
 * compact, readable layout that fits the Flipper Zero screen.
 */
#include "flippass_scene_entry_detail.h"
#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_scene.h"
#include "flippass_scene_status.h"

static const char* flippass_safe_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static void flippass_scene_entry_detail_build_text(App* app, FuriString* text) {
    const KDBXEntry* entry = app->current_entry;
    const KDBXGroup* group = app->current_group;
    const bool has_url = entry && flippass_db_entry_has_field(entry, KDBXEntryFieldUrl);
    const bool has_notes = entry && flippass_db_entry_has_field(entry, KDBXEntryFieldNotes);
    const bool has_autotype = entry && flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype);
    const bool has_username = entry && flippass_db_entry_has_field(entry, KDBXEntryFieldUsername);
    const bool has_password = entry && flippass_db_entry_has_field(entry, KDBXEntryFieldPassword);

    furi_string_set_str(text, "");
    furi_string_cat_printf(text, "Title: %s\n", flippass_safe_text(entry ? entry->title : NULL, "Untitled Entry"));
    furi_string_cat_printf(text, "User: %s\n", flippass_safe_text(entry ? entry->username : NULL, "Not set"));
    furi_string_cat_printf(text, "Password: %s\n", has_password ? "Hidden" : "Not set");
    if(has_url) {
        furi_string_cat_printf(text, "URL: %s\n", flippass_safe_text(entry->url, "Not set"));
    }
    furi_string_cat_printf(text, "Group: %s\n", flippass_safe_text(group ? group->name : NULL, "Root"));
    if(has_notes) {
        furi_string_cat(text, "Notes: Available\n");
    }
    furi_string_cat_printf(
        text,
        "AutoType: %s\n",
        has_autotype ? "Custom sequence" :
                       ((has_username && has_password) ?
                            "Default login" :
                            "Not available"));
    furi_string_cat(text, "\n");
    furi_string_cat(text, "Return to actions.");
}

void flippass_scene_entry_detail_on_enter(void* context) {
    App* app = context;
    FuriString* scratch = furi_string_alloc();

    if(app->current_entry == NULL) {
        flippass_scene_status_show(
            app,
            "No Entry Selected",
            "Return to the browser and pick an entry first.",
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(scratch);
        return;
    }

    if(!flippass_db_activate_entry(app, app->current_entry, false, scratch)) {
        flippass_scene_status_show(
            app,
            "Entry Load Failed",
            furi_string_get_cstr(scratch),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(scratch);
        return;
    }

    widget_reset(app->widget);

    flippass_scene_entry_detail_build_text(app, scratch);
    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        50,
        AlignLeft,
        AlignTop,
        furi_string_get_cstr(scratch),
        true);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Back", NULL, NULL);
    furi_string_free(scratch);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool flippass_scene_entry_detail_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void flippass_scene_entry_detail_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
