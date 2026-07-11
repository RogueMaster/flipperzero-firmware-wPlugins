#include "../flipper_recon_i.h"

static void flipper_recon_scene_text_input_callback(void* context) {
    FlipperReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RECON_EVENT_TEXT_DONE);
}

/* Resolve the header, current value and max length for the active target. */
static void text_input_target_info(
    FlipperReconApp* app,
    const char** header,
    const char** value,
    size_t* max_len) {
    Session* session = app->session;
    Asset* asset = (session->asset_count > app->selected_asset) ?
                       &session->assets[app->selected_asset] :
                       NULL;
    Evidence* evidence = (session->evidence_count > app->selected_evidence) ?
                             &session->evidence[app->selected_evidence] :
                             NULL;
    switch(app->text_target) {
    case ReconTextTargetSessionName:
        *header = "Engagement name";
        *value = session->name;
        *max_len = RECON_NAME_LEN;
        break;
    case ReconTextTargetClient:
        *header = "Client";
        *value = session->client;
        *max_len = RECON_NAME_LEN;
        break;
    case ReconTextTargetLocation:
        *header = "Location";
        *value = session->location;
        *max_len = RECON_NAME_LEN;
        break;
    case ReconTextTargetAssetName:
        *header = "Asset name";
        *value = asset ? asset->name : "";
        *max_len = RECON_NAME_LEN;
        break;
    case ReconTextTargetAssetNotes:
        *header = "Notes";
        *value = asset ? asset->notes : "";
        *max_len = RECON_NOTE_LEN;
        break;
    case ReconTextTargetEvidenceLabel:
        *header = "Evidence label";
        *value = evidence ? evidence->label : "";
        *max_len = RECON_NAME_LEN;
        break;
    case ReconTextTargetEvidencePath:
        *header = "File path";
        *value = evidence ? evidence->path : "";
        *max_len = RECON_PATH_LEN;
        break;
    default:
        *header = "Text";
        *value = "";
        *max_len = RECON_NAME_LEN;
        break;
    }
}

static void text_input_apply(FlipperReconApp* app) {
    Session* session = app->session;
    Asset* asset = (session->asset_count > app->selected_asset) ?
                       &session->assets[app->selected_asset] :
                       NULL;
    Evidence* evidence = (session->evidence_count > app->selected_evidence) ?
                             &session->evidence[app->selected_evidence] :
                             NULL;
    switch(app->text_target) {
    case ReconTextTargetSessionName:
        strncpy(session->name, app->text_buf, RECON_NAME_LEN - 1);
        break;
    case ReconTextTargetClient:
        strncpy(session->client, app->text_buf, RECON_NAME_LEN - 1);
        break;
    case ReconTextTargetLocation:
        strncpy(session->location, app->text_buf, RECON_NAME_LEN - 1);
        break;
    case ReconTextTargetAssetName:
        if(asset) {
            strncpy(asset->name, app->text_buf, RECON_NAME_LEN - 1);
            asset->modified = furi_hal_rtc_get_timestamp();
        }
        break;
    case ReconTextTargetAssetNotes:
        if(asset) {
            strncpy(asset->notes, app->text_buf, RECON_NOTE_LEN - 1);
            asset->modified = furi_hal_rtc_get_timestamp();
        }
        break;
    case ReconTextTargetEvidenceLabel:
        if(evidence) strncpy(evidence->label, app->text_buf, RECON_NAME_LEN - 1);
        break;
    case ReconTextTargetEvidencePath:
        if(evidence) strncpy(evidence->path, app->text_buf, RECON_PATH_LEN - 1);
        break;
    default:
        break;
    }
    session_touch(session);
}

void flipper_recon_scene_text_input_on_enter(void* context) {
    FlipperReconApp* app = context;
    const char* header = "";
    const char* value = "";
    size_t max_len = RECON_NAME_LEN;
    text_input_target_info(app, &header, &value, &max_len);

    memset(app->text_buf, 0, sizeof(app->text_buf));
    strncpy(app->text_buf, value, max_len - 1);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input,
        flipper_recon_scene_text_input_callback,
        app,
        app->text_buf,
        max_len,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewTextInput);
}

bool flipper_recon_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == RECON_EVENT_TEXT_DONE) {
        text_input_apply(app);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void flipper_recon_scene_text_input_on_exit(void* context) {
    FlipperReconApp* app = context;
    text_input_reset(app->text_input);
}
