#include "flippass_scene_editor.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "../kdbx/memzero.h"
#include "../plugins/flippass_editor_plugin.h"
#include "../plugins/flippass_keyboard_layout_plugin.h"
#include "flippass_scene.h"
#include "flippass_scene_status.h"

#include <string.h>

/* The editor scenes live in the flippass_editor plugin. The scene table points at
 * these stubs, which load the plugin on demand and pin it while its code runs.
 *
 * Residency: the open leaves the heap fragmented, and on the device a small database
 * left 33 KB free but no 12.5 KB block for the editor's .text, so loading it on
 * demand made "Add entry" unavailable right after unlocking. The open proxy hands the
 * unlock plugin's blocks over instead: open_host carries a successor room sized for
 * the editor's worst-case load, and the editor is loaded the moment open_host unloads
 * (flippass_open_proxy.c), then marked sticky and kept while the database is loaded.
 * The out-of-memory load retry may still evict it while no editor scene is open, so a
 * large database can trade the editor for the memory it needs; the scenes then retry
 * on demand. A cold form entry releases idle views, cached plaintext and idle
 * standalone Crypto after the previous scene exits. A mapped editor retains its
 * child-form views. Without a database (file-picker Config, New Database, New Directory) it
 * loads on demand.
 *
 * The editor also registers GUI callbacks (item list, dialog, text input) that run
 * plugin code outside these stubs and may switch scenes; text input completion pops
 * its own scene, for example. The plugin is therefore never unloaded while a call
 * is pinned or an editor scene is anywhere on the stack, the current one included.
 * Helpers that the save, lock and exit paths need stay here so those paths never
 * have to load the editor. */

/* Outside the editor's own custom events (row index + 1, 0x100 + dialog result,
 * 0x300 OTP rebuild) and the app's exit event. */
#define FLIPPASS_EDITOR_UNAVAILABLE_EVENT 0x7E00U

static bool flippass_editor_backout_pending = false;

static bool flippass_editor_scene_id(uint32_t scene) {
    return scene == FlipPassScene_Editor || scene == FlipPassScene_EditorTextInput;
}

static bool flippass_editor_scene_in_stack(App* app) {
    return flippass_editor_scene_id(scene_manager_get_current_scene(app->scene_manager)) ||
           scene_manager_has_previous_scene(app->scene_manager, FlipPassScene_Editor) ||
           scene_manager_has_previous_scene(app->scene_manager, FlipPassScene_EditorTextInput);
}

static bool flippass_editor_keep_mapped(App* app) {
    return app->database_loaded || flippass_editor_scene_in_stack(app);
}

static const FlipPassEditorPluginV1* flippass_editor_plugin_enter(App* app) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotEditor,
        NULL,
        FLIPPASS_EDITOR_PLUGIN_APP_ID,
        FLIPPASS_EDITOR_PLUGIN_API_VERSION,
        NULL);
    const FlipPassEditorPluginV1* plugin = (descriptor != NULL) ? descriptor->entry_point : NULL;

    if(plugin == NULL || plugin->api_version != FLIPPASS_EDITOR_PLUGIN_API_VERSION) {
        flippass_module_unload(app, FlipPassModuleSlotEditor);
        return NULL;
    }
    flippass_module_pin(app, FlipPassModuleSlotEditor);
    return plugin;
}

static void flippass_editor_plugin_leave(App* app) {
    flippass_module_unpin(app, FlipPassModuleSlotEditor);
    if(app->module_loader.slot[FlipPassModuleSlotEditor].pin_count == 0U &&
       !flippass_editor_keep_mapped(app)) {
        flippass_module_unload(app, FlipPassModuleSlotEditor);
    }
}

void flippass_editor_begin_database_session(App* app) {
    furi_assert(app);
    if(app->rpc_mode) {
        return;
    }
    /* A failure here is not an error: the editor scenes retry on demand. */
    if(flippass_module_ensure(
           app,
           FlipPassModuleSlotEditor,
           NULL,
           FLIPPASS_EDITOR_PLUGIN_APP_ID,
           FLIPPASS_EDITOR_PLUGIN_API_VERSION,
           NULL) != NULL) {
        flippass_module_set_sticky(app, FlipPassModuleSlotEditor, true);
    }
}

bool flippass_editor_in_use(App* app) {
    furi_assert(app);
    const FlipPassModuleInstance* instance = &app->module_loader.slot[FlipPassModuleSlotEditor];
    return instance->pin_count != 0U ||
           (instance->application != NULL && flippass_editor_scene_in_stack(app));
}

void flippass_editor_release_if_idle(App* app) {
    furi_assert(app);
    const FlipPassModuleInstance* instance = &app->module_loader.slot[FlipPassModuleSlotEditor];
    /* A sticky editor belongs to the database session: the open resets the model
     * internally and must not drop the editor loaded for it. */
    if(instance->application == NULL || instance->pin_count != 0U || instance->sticky ||
       flippass_editor_keep_mapped(app)) {
        return;
    }
    flippass_module_unload(app, FlipPassModuleSlotEditor);
}

void flippass_editor_end_database_session(App* app) {
    furi_assert(app);
    flippass_module_set_sticky(app, FlipPassModuleSlotEditor, false);
    flippass_editor_release_if_idle(app);
}

/* Called from an editor scene's on_enter when the plugin could not be loaded
 * (normally low RAM). Leaving from inside on_enter would nest scene changes, and
 * the previous scene may push the editor again (DbEntries does for Modify), which
 * recursed until the stack overflowed; leave from the dispatcher loop instead. */
static void flippass_editor_plugin_unavailable(App* app) {
    if(!flippass_editor_backout_pending) {
        flippass_editor_backout_pending = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, FLIPPASS_EDITOR_UNAVAILABLE_EVENT);
    }
}

/* Mirrors the editor's own cancel: drop the draft, close a database that was
 * opened only to be modified, return to the scene that asked for the editor. */
static void flippass_editor_back_out(App* app) {
    const FlipPassEditorMode mode = app->editor_mode;
    const uint32_t return_scene = app->editor_return_scene;

    flippass_editor_clear_custom_field_drafts(app);
    app->editor_mode = FlipPassEditorModeNone;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    if(((mode == FlipPassEditorModeModifyDatabase && return_scene == FlipPassScene_FileBrowser) ||
        mode == FlipPassEditorModeNewDatabase) &&
       app->database_loaded) {
        flippass_close_database(app);
    }
    /* Still pending here, so the editor stubs' on_exit does not retry the load. */
    if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, return_scene)) {
        scene_manager_previous_scene(app->scene_manager);
    }
    flippass_editor_backout_pending = false;
    flippass_scene_status_show(
        app,
        "Editor Unavailable",
        "Not enough RAM is available to load the editor.",
        scene_manager_get_current_scene(app->scene_manager));
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
}

/* While a back-out is pending the editor scene has no view of its own: swallow
 * everything except the deferred event. */
static bool flippass_editor_pending_event(App* app, SceneManagerEvent event) {
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == FLIPPASS_EDITOR_UNAVAILABLE_EVENT) {
        flippass_editor_back_out(app);
    }
    return true;
}

void flippass_scene_editor_on_enter(void* context) {
    App* app = context;
    flippass_editor_backout_pending = false;
    if(app->module_loader.slot[FlipPassModuleSlotEditor].application == NULL) {
        /* The previous scene has detached its callbacks. Release its idle
         * views before the cold editor load, rather than leaving a browser
         * model in the contiguous block needed by the editor's text section.
         * A mapped editor may still own child-form callbacks: keep those views. */
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
        flippass_save_unload_idle_views(app);
        /* Entry drafts are already copied into App. Cached plaintext and the
         * standalone Crypto provider are idle now; neither belongs to the
         * editor form. Release them before selecting the editor's load blocks. */
        flippass_db_deactivate_entry(app);
        flippass_db_browser_clear_title_cache(app);
        flippass_module_unload(app, FlipPassModuleSlotCrypto);
    }
    const FlipPassEditorPluginV1* plugin = flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        flippass_editor_plugin_unavailable(app);
        return;
    }
    plugin->on_enter(context);
    flippass_editor_plugin_leave(app);
}

bool flippass_scene_editor_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(flippass_editor_backout_pending) {
        return flippass_editor_pending_event(app, event);
    }
    const FlipPassEditorPluginV1* plugin = flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        return false;
    }
    const bool consumed = plugin->on_event(context, event);
    flippass_editor_plugin_leave(app);
    return consumed;
}

void flippass_scene_editor_on_exit(void* context) {
    App* app = context;
    const FlipPassEditorPluginV1* plugin =
        flippass_editor_backout_pending ? NULL : flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        if(app->variable_item_list != NULL) {
            variable_item_list_reset(app->variable_item_list);
        }
        if(app->dialog_ex != NULL) {
            dialog_ex_reset(app->dialog_ex);
        }
        return;
    }
    plugin->on_exit(context);
    flippass_editor_plugin_leave(app);
}

void flippass_scene_editor_text_input_on_enter(void* context) {
    App* app = context;
    flippass_editor_backout_pending = false;
    const FlipPassEditorPluginV1* plugin = flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        flippass_editor_plugin_unavailable(app);
        return;
    }
    plugin->text_input_on_enter(context);
    flippass_editor_plugin_leave(app);
}

bool flippass_scene_editor_text_input_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(flippass_editor_backout_pending) {
        return flippass_editor_pending_event(app, event);
    }
    const FlipPassEditorPluginV1* plugin = flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        return false;
    }
    const bool consumed = plugin->text_input_on_event(context, event);
    flippass_editor_plugin_leave(app);
    return consumed;
}

void flippass_scene_editor_text_input_on_exit(void* context) {
    App* app = context;
    const FlipPassEditorPluginV1* plugin =
        flippass_editor_backout_pending ? NULL : flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        if(app->text_input != NULL) {
            text_input_reset(app->text_input);
            text_input_set_validator(app->text_input, NULL, NULL);
        }
        return;
    }
    plugin->text_input_on_exit(context);
    flippass_editor_plugin_leave(app);
}

/* When the plugin cannot load these do nothing; the editor scene that follows
 * reports the error. */
void flippass_editor_prepare_new_custom_field(App* app) {
    const FlipPassEditorPluginV1* plugin = flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        return;
    }
    plugin->prepare_new_custom_field(app);
    flippass_editor_plugin_leave(app);
}

void flippass_editor_prepare_edit_custom_field(
    App* app,
    KDBXCustomField* field,
    FlipPassEditorCustomFieldDraft* draft) {
    const FlipPassEditorPluginV1* plugin = flippass_editor_plugin_enter(app);
    if(plugin == NULL) {
        return;
    }
    plugin->prepare_edit_custom_field(app, field, draft);
    flippass_editor_plugin_leave(app);
}

void flippass_editor_free_custom_field_draft(FlipPassEditorCustomFieldDraft* draft) {
    if(draft == NULL) {
        return;
    }

    if(draft->name != NULL) {
        memzero(draft->name, strlen(draft->name));
        free(draft->name);
    }
    if(draft->value != NULL) {
        memzero(draft->value, strlen(draft->value));
        free(draft->value);
    }
    memzero(draft, sizeof(*draft));
    free(draft);
}

void flippass_editor_clear_custom_field_drafts(App* app) {
    furi_assert(app);

    FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields;
    while(draft != NULL) {
        FlipPassEditorCustomFieldDraft* next = draft->next;
        flippass_editor_free_custom_field_draft(draft);
        draft = next;
    }

    app->editor_custom_fields = NULL;
    app->editor_custom_field_draft = NULL;
}

size_t flippass_editor_custom_field_count(const App* app) {
    size_t count = 0U;

    if(app == NULL) {
        return 0U;
    }

    if(app->editor_mode == FlipPassEditorModeAddEntry ||
       (app->editor_mode == FlipPassEditorModeAddCustomField && app->editor_entry == NULL) ||
       (app->editor_mode == FlipPassEditorModeEditCustomField &&
        app->editor_custom_field_draft != NULL)) {
        for(const FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields; draft != NULL;
            draft = draft->next) {
            if(!flippass_otp_draft_is_reserved(draft)) {
                count++;
            }
        }
        return count;
    }

    const KDBXEntry* entry = app->editor_entry != NULL ? app->editor_entry : app->active_entry;
    for(const KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        if(!flippass_otp_custom_field_is_reserved(field->key)) {
            count++;
        }
    }

    return count;
}

void flippass_editor_config_layout_unload_plugin(App* app) {
    const FlipPassModuleInstance* instance =
        &app->module_loader.slot[FlipPassModuleSlotKeyboardLayout];
    const FlipPassKeyboardLayoutPluginV1* plugin =
        (instance->descriptor != NULL) ? instance->descriptor->entry_point : NULL;

    if(plugin != NULL && plugin->reset != NULL) {
        plugin->reset();
    }
    flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
}

void flippass_scene_editor_trim_for_save(struct App* app) {
    furi_assert(app);

    if(app->variable_item_list != NULL) {
        app->editor_selected_index =
            variable_item_list_get_selected_item_index(app->variable_item_list);
        variable_item_list_reset(app->variable_item_list);
    }
    flippass_editor_config_layout_unload_plugin(app);
    if(app->dialog_ex != NULL) {
        dialog_ex_reset(app->dialog_ex);
    }
}
