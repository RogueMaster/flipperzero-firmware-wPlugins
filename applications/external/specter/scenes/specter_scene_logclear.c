#include "../specter_i.h"
#include <stdio.h>

/* Wiping evidence is the one irreversible thing this app can do, so it gets an
 * explicit confirmation with the size of what is about to go. */

typedef enum {
    LogClearResultCancel,
    LogClearResultConfirm,
} LogClearResult;

static void specter_logclear_button_cb(GuiButtonType type, InputType input, void* context) {
    SpecterApp* app = context;
    if(input != InputTypeShort) return;

    if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LogClearResultConfirm);
    } else if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LogClearResultCancel);
    }
}

void specter_scene_logclear_on_enter(void* context) {
    SpecterApp* app = context;
    Widget* widget = app->widget;
    char detail[64];

    widget_reset(widget);

    uint32_t size = specter_log_size();
    widget_add_string_element(widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Clear logbook?");

    if(size) {
        snprintf(detail, sizeof(detail), "%lu bytes of findings", (unsigned long)size);
    } else {
        snprintf(detail, sizeof(detail), "The logbook is already empty");
    }
    widget_add_string_element(widget, 64, 24, AlignCenter, AlignTop, FontSecondary, detail);
    widget_add_string_element(
        widget, 64, 36, AlignCenter, AlignTop, FontSecondary, "This cannot be undone.");

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Cancel", specter_logclear_button_cb, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Clear", specter_logclear_button_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewWidget);
}

bool specter_scene_logclear_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LogClearResultConfirm) {
            specter_log_clear();
            specter_notify_saved(app);
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == LogClearResultCancel) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }
    return consumed;
}

void specter_scene_logclear_on_exit(void* context) {
    SpecterApp* app = context;
    widget_reset(app->widget);
}
