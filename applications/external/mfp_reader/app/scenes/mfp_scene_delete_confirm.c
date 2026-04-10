#include "../mfp_app.h"
#include "../mfp_storage.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <furi.h>
#include <string.h>

static void delete_confirm_widget_cb(GuiButtonType result, InputType type, void* ctx) {
    if(type != InputTypeShort) return;
    MfpApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)result);
}

void mfp_scene_delete_confirm_on_enter(void* ctx) {
    MfpApp* app = ctx;

    widget_reset(app->widget);

    const char* fname = strrchr(app->save_path, '/');
    fname = fname ? fname + 1 : app->save_path;

    FuriString* temp = furi_string_alloc();

    /* Header — bold, centered */
    furi_string_printf(temp, "\e#Delete %s?\e#", fname);
    widget_add_text_box_element(
        app->widget, 0, 0, 128, 23,
        AlignCenter, AlignCenter, furi_string_get_cstr(temp), false);

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", delete_confirm_widget_cb, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Delete", delete_confirm_widget_cb, app);

    /* UID line below the header */
    furi_string_set(temp, "UID:");
    for(uint8_t i = 0; i < app->version.uid_len; i++) {
        furi_string_cat_printf(temp, " %02X", app->version.uid[i]);
    }
    widget_add_string_element(
        app->widget, 64, 28, AlignCenter, AlignTop,
        FontSecondary, furi_string_get_cstr(temp));

    /* Card type line */
    furi_string_printf(
        temp, "MFP SL%d %s",
        (int)app->version.sl,
        app->version.size == MfpSize4K ? "4K" : "2K");
    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignTop,
        FontSecondary, furi_string_get_cstr(temp));

    furi_string_free(temp);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewWidget);
}

bool mfp_scene_delete_confirm_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            /* Cancel → go back to Actions (or wherever we came from). */
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(event.event == GuiButtonTypeRight) {
            if(storage_simply_remove(app->storage, app->save_path)) {
                scene_manager_next_scene(app->scene_manager, MfpSceneDeleteSuccess);
            } else {
                /* Failed — bail back to Start. */
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, MfpSceneStart);
            }
            return true;
        }
    }
    return false;
}

void mfp_scene_delete_confirm_on_exit(void* ctx) {
    MfpApp* app = ctx;
    widget_reset(app->widget);
}
