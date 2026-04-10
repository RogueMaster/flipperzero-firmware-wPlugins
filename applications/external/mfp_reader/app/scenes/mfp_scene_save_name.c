#include "../mfp_app.h"
#include "../mfp_storage.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <gui/modules/text_input.h>
#include <gui/modules/validators.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    SaveNameEventDone = 0,
} SaveNameCustomEvent;

static void save_name_text_input_cb(void* ctx) {
    MfpApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, SaveNameEventDone);
}

static void save_name_suggest_default(MfpApp* app) {
    /* Prefer the existing save_path basename if one was computed
     * earlier (e.g., after a load). Otherwise derive from UID. */
    if(app->save_path[0] != '\0') {
        const char* slash = strrchr(app->save_path, '/');
        const char* base = slash ? slash + 1 : app->save_path;
        /* Strip extension so the user edits the plain name */
        size_t blen = strlen(base);
        const char* dot = strrchr(base, '.');
        if(dot && dot > base) blen = (size_t)(dot - base);
        if(blen >= sizeof(app->text_store)) blen = sizeof(app->text_store) - 1;
        memcpy(app->text_store, base, blen);
        app->text_store[blen] = '\0';
        return;
    }

    /* Fallback: MFP<UID> */
    int n = snprintf(app->text_store, sizeof(app->text_store), "MFP");
    for(uint8_t i = 0; i < app->version.uid_len && n < (int)sizeof(app->text_store) - 3;
        i++) {
        n += snprintf(
            app->text_store + n, sizeof(app->text_store) - (size_t)n, "%02X",
            app->version.uid[i]);
    }
}

void mfp_scene_save_name_on_enter(void* ctx) {
    MfpApp* app = ctx;

    save_name_suggest_default(app);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Name the dump");
    text_input_set_result_callback(
        app->text_input,
        save_name_text_input_cb,
        app,
        app->text_store,
        sizeof(app->text_store),
        /* clear_default_text = */ false);

    /* File-name validator: complains if the chosen name already
     * exists in the app folder. Protects users from accidentally
     * overwriting existing saves. */
    ValidatorIsFile* validator =
        validator_is_file_alloc_init(MFP_APP_FOLDER, MFP_FILE_EXT, NULL);
    text_input_set_validator(app->text_input, validator_is_file_callback, validator);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewTextInput);
}

bool mfp_scene_save_name_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == SaveNameEventDone) {
        /* Build the full path from the edited name + extension. */
        char path[160];
        snprintf(
            path, sizeof(path), "%s/%s%s", MFP_APP_FOLDER, app->text_store, MFP_FILE_EXT);

        if(mfp_storage_save_all_to_path(app, path)) {
            scene_manager_next_scene(app->scene_manager, MfpSceneSaveSuccess);
        } else {
            /* On write failure, bail back to ReadAllResult without
             * showing the success popup. */
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, MfpSceneReadAllResult)) {
                scene_manager_previous_scene(app->scene_manager);
            }
        }
        return true;
    }
    return false;
}

void mfp_scene_save_name_on_exit(void* ctx) {
    MfpApp* app = ctx;
    ValidatorIsFile* validator =
        text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    if(validator) validator_is_file_free(validator);
    text_input_reset(app->text_input);
}
