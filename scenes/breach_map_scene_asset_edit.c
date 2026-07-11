#include "../breach_map_i.h"

typedef enum {
    AssetEditName,
    AssetEditType,
    AssetEditRisk,
    AssetEditSeverity,
    AssetEditNotes,
    AssetEditRemediation,
    AssetEditEvidence,
    AssetEditDelete,
} AssetEditIndex;

#define RISK_STEP   5
#define RISK_VALUES ((100 / RISK_STEP) + 1) /* 0,5,...,100 */

static Asset* current_asset(BreachMapApp* app) {
    if(app->selected_asset >= app->session->asset_count) return NULL;
    return &app->session->assets[app->selected_asset];
}

static void type_changed_callback(VariableItem* item) {
    BreachMapApp* app = variable_item_get_context(item);
    Asset* asset = current_asset(app);
    if(!asset) return;
    uint8_t index = variable_item_get_current_value_index(item);
    asset->type = index;
    asset->modified = furi_hal_rtc_get_timestamp();
    variable_item_set_current_value_text(item, asset_type_name(asset->type));
}

static void risk_changed_callback(VariableItem* item) {
    BreachMapApp* app = variable_item_get_context(item);
    Asset* asset = current_asset(app);
    if(!asset) return;
    uint8_t index = variable_item_get_current_value_index(item);
    asset->risk = index * RISK_STEP;
    asset->modified = furi_hal_rtc_get_timestamp();
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", asset->risk);
    variable_item_set_current_value_text(item, buf);
}

static void severity_changed_callback(VariableItem* item) {
    BreachMapApp* app = variable_item_get_context(item);
    Asset* asset = current_asset(app);
    if(!asset) return;
    asset->severity = variable_item_get_current_value_index(item);
    asset->modified = furi_hal_rtc_get_timestamp();
    variable_item_set_current_value_text(item, severity_name(asset->severity));
}

static void enter_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void breach_map_scene_asset_edit_on_enter(void* context) {
    BreachMapApp* app = context;
    VariableItemList* list = app->var_item_list;
    Asset* asset = current_asset(app);
    variable_item_list_reset(list);
    if(!asset) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
        return;
    }

    VariableItem* item;

    item = variable_item_list_add(list, "Name", 1, NULL, app);
    variable_item_set_current_value_text(item, asset->name);

    item = variable_item_list_add(list, "Type", AssetTypeCount, type_changed_callback, app);
    variable_item_set_current_value_index(item, asset->type);
    variable_item_set_current_value_text(item, asset_type_name(asset->type));

    item = variable_item_list_add(list, "Risk", RISK_VALUES, risk_changed_callback, app);
    variable_item_set_current_value_index(item, asset->risk / RISK_STEP);
    char risk_buf[8];
    snprintf(risk_buf, sizeof(risk_buf), "%u", asset->risk);
    variable_item_set_current_value_text(item, risk_buf);

    item = variable_item_list_add(list, "Severity", SeverityCount, severity_changed_callback, app);
    variable_item_set_current_value_index(item, asset->severity);
    variable_item_set_current_value_text(item, severity_name(asset->severity));

    item = variable_item_list_add(list, "Notes", 1, NULL, app);
    variable_item_set_current_value_text(item, asset->notes[0] ? "edit" : "-");

    item = variable_item_list_add(list, "Remediation", 1, NULL, app);
    variable_item_set_current_value_text(item, asset->remediation[0] ? "edit" : "-");

    item = variable_item_list_add(list, "Evidence", 1, NULL, app);
    char ev_buf[8];
    snprintf(ev_buf, sizeof(ev_buf), "%u", asset_manager_evidence_count(app->session, asset->id));
    variable_item_set_current_value_text(item, ev_buf);

    variable_item_list_add(list, "Delete asset", 1, NULL, app);

    variable_item_list_set_enter_callback(list, enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, BreachMapSceneAssetEdit));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
}

bool breach_map_scene_asset_edit_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BreachMapSceneAssetEdit, event.event);
        switch(event.event) {
        case AssetEditName:
            app->text_target = ReconTextTargetAssetName;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneTextInput);
            consumed = true;
            break;
        case AssetEditNotes:
            app->text_target = ReconTextTargetAssetNotes;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneTextInput);
            consumed = true;
            break;
        case AssetEditRemediation:
            app->text_target = ReconTextTargetAssetRemediation;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneTextInput);
            consumed = true;
            break;
        case AssetEditEvidence:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneEvidenceList);
            consumed = true;
            break;
        case AssetEditDelete:
            app->message_mode = ReconMessageConfirmDeleteAsset;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void breach_map_scene_asset_edit_on_exit(void* context) {
    BreachMapApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
