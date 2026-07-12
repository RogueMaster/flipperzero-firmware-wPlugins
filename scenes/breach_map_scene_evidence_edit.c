#include "../breach_map_i.h"
#include "../modules/capture_meta.h"

typedef enum {
    EvidenceEditLabel,
    EvidenceEditType,
    EvidenceEditFile,
    EvidenceEditInfo,
    EvidenceEditDelete,
} EvidenceEditIndex;

static Evidence* current_evidence(BreachMapApp* app) {
    if(app->selected_evidence >= app->session->evidence_count) return NULL;
    return &app->session->evidence[app->selected_evidence];
}

static void type_changed_callback(VariableItem* item) {
    BreachMapApp* app = variable_item_get_context(item);
    Evidence* e = current_evidence(app);
    if(!e) return;
    e->type = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, evidence_type_name(e->type));
    session_touch(app->session);
}

static void enter_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void rebuild_list(BreachMapApp* app) {
    VariableItemList* list = app->var_item_list;
    Evidence* e = current_evidence(app);
    variable_item_list_reset(list);
    if(!e) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
        return;
    }

    VariableItem* item;
    item = variable_item_list_add(list, "Label", 1, NULL, app);
    variable_item_set_current_value_text(item, e->label);

    item = variable_item_list_add(list, "Type", EvidenceTypeCount, type_changed_callback, app);
    variable_item_set_current_value_index(item, e->type);
    variable_item_set_current_value_text(item, evidence_type_name(e->type));

    item = variable_item_list_add(list, "File", 1, NULL, app);
    variable_item_set_current_value_text(item, e->path[0] ? "linked" : "pick");

    item = variable_item_list_add(list, "Info", 1, NULL, app);
    variable_item_set_current_value_text(item, e->info[0] ? e->info : "-");

    variable_item_list_add(list, "Delete", 1, NULL, app);

    variable_item_list_set_enter_callback(list, enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, BreachMapSceneEvidenceEdit));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
}

static void link_file(BreachMapApp* app) {
    Evidence* e = current_evidence(app);
    if(!e) return;

    FuriString* result = furi_string_alloc();
    FuriString* start = furi_string_alloc_set_str("/ext");
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, "*", NULL);
    options.base_path = "/ext";

    if(dialog_file_browser_show(app->dialogs, result, start, &options)) {
        strncpy(e->path, furi_string_get_cstr(result), RECON_PATH_LEN - 1);
        e->path[RECON_PATH_LEN - 1] = '\0';
        e->type = capture_meta_type_from_path(e->path);
        e->info[0] = '\0';
        capture_meta_extract(app->storage, e->path, e->info, RECON_NAME_LEN);
        session_touch(app->session);
    }
    furi_string_free(result);
    furi_string_free(start);
}

void breach_map_scene_evidence_edit_on_enter(void* context) {
    BreachMapApp* app = context;
    rebuild_list(app);
}

bool breach_map_scene_evidence_edit_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BreachMapSceneEvidenceEdit, event.event);
        switch(event.event) {
        case EvidenceEditLabel:
            app->text_target = ReconTextTargetEvidenceLabel;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneTextInput);
            consumed = true;
            break;
        case EvidenceEditFile:
            link_file(app);
            rebuild_list(app);
            consumed = true;
            break;
        case EvidenceEditDelete:
            app->message_mode = ReconMessageConfirmDeleteEvidence;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void breach_map_scene_evidence_edit_on_exit(void* context) {
    BreachMapApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
