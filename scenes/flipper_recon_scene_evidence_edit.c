#include "../flipper_recon_i.h"

typedef enum {
    EvidenceEditLabel,
    EvidenceEditType,
    EvidenceEditPath,
    EvidenceEditDelete,
} EvidenceEditIndex;

static Evidence* current_evidence(FlipperReconApp* app) {
    if(app->selected_evidence >= app->session->evidence_count) return NULL;
    return &app->session->evidence[app->selected_evidence];
}

static void type_changed_callback(VariableItem* item) {
    FlipperReconApp* app = variable_item_get_context(item);
    Evidence* e = current_evidence(app);
    if(!e) return;
    e->type = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, evidence_type_name(e->type));
    session_touch(app->session);
}

static void enter_callback(void* context, uint32_t index) {
    FlipperReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_recon_scene_evidence_edit_on_enter(void* context) {
    FlipperReconApp* app = context;
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
    variable_item_set_current_value_text(item, e->path[0] ? "linked" : "-");

    variable_item_list_add(list, "Delete", 1, NULL, app);

    variable_item_list_set_enter_callback(list, enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, FlipperReconSceneEvidenceEdit));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
}

bool flipper_recon_scene_evidence_edit_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, FlipperReconSceneEvidenceEdit, event.event);
        switch(event.event) {
        case EvidenceEditLabel:
            app->text_target = ReconTextTargetEvidenceLabel;
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneTextInput);
            consumed = true;
            break;
        case EvidenceEditPath:
            app->text_target = ReconTextTargetEvidencePath;
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneTextInput);
            consumed = true;
            break;
        case EvidenceEditDelete:
            app->message_mode = ReconMessageConfirmDeleteEvidence;
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneMessage);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void flipper_recon_scene_evidence_edit_on_exit(void* context) {
    FlipperReconApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
