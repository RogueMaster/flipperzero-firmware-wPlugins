#include "flippass_scene_send_confirm.h"

#include "../flippass.h"

void flippass_entry_action_prepare_pending(struct App* app) {
    UNUSED(app);
}

bool flippass_entry_action_execute_pending(struct App* app, FuriString* error) {
    UNUSED(app);

    if(error != NULL) {
        furi_string_set_str(error, "Typing is disabled in this minimal-memory build.");
    }

    return false;
}
