#include "../flipper_recon_i.h"

#define RECON_ABOUT_TEXT                                     \
    "Flipper Recon\n"                                        \
    "Physical security recon notebook.\n\n"                  \
    "Organize engagements, assets, evidence and relations\n" \
    "from authorized physical security assessments.\n\n"     \
    "Export findings as JSON / Markdown.\n\n"                \
    "For authorized testing only.\n\n"                       \
    "made by Gerijacki"

static void flipper_recon_scene_message_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    FlipperReconApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RECON_EVENT_CONFIRM_YES);
    } else if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RECON_EVENT_CONFIRM_NO);
    }
}

void flipper_recon_scene_message_on_enter(void* context) {
    FlipperReconApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    switch(app->message_mode) {
    case ReconMessageAbout:
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, RECON_ABOUT_TEXT);
        break;
    case ReconMessageInfo:
        widget_add_text_scroll_element(
            widget, 0, 0, 128, 64, furi_string_get_cstr(app->message_text));
        break;
    case ReconMessageConfirmDeleteSession: {
        FuriString* text = furi_string_alloc();
        furi_string_printf(text, "Delete engagement\n\"%s\"?", app->session->name);
        widget_add_string_multiline_element(
            widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Cancel", flipper_recon_scene_message_button_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Delete", flipper_recon_scene_message_button_callback, app);
        break;
    }
    case ReconMessageConfirmDeleteAsset: {
        Asset* asset = (app->selected_asset < app->session->asset_count) ?
                           &app->session->assets[app->selected_asset] :
                           NULL;
        FuriString* text = furi_string_alloc();
        furi_string_printf(text, "Delete asset\n\"%s\"?", asset ? asset->name : "");
        widget_add_string_multiline_element(
            widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Cancel", flipper_recon_scene_message_button_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Delete", flipper_recon_scene_message_button_callback, app);
        break;
    }
    case ReconMessageConfirmDeleteRelation:
    case ReconMessageConfirmDeleteEvidence: {
        const char* what = (app->message_mode == ReconMessageConfirmDeleteRelation) ? "relation" :
                                                                                      "evidence";
        FuriString* text = furi_string_alloc();
        furi_string_printf(text, "Delete this\n%s?", what);
        widget_add_string_multiline_element(
            widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Cancel", flipper_recon_scene_message_button_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Delete", flipper_recon_scene_message_button_callback, app);
        break;
    }
    case ReconMessageConfirmOverwrite: {
        widget_add_string_multiline_element(
            widget,
            64,
            20,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "An engagement with\nthis name exists.\nOverwrite it?");
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Cancel", flipper_recon_scene_message_button_callback, app);
        widget_add_button_element(
            widget,
            GuiButtonTypeRight,
            "Overwrite",
            flipper_recon_scene_message_button_callback,
            app);
        break;
    }
    case ReconMessageConfirmDiscard: {
        widget_add_string_multiline_element(
            widget,
            64,
            20,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "Unsaved changes.\nDiscard them?");
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Cancel", flipper_recon_scene_message_button_callback, app);
        widget_add_button_element(
            widget,
            GuiButtonTypeRight,
            "Discard",
            flipper_recon_scene_message_button_callback,
            app);
        break;
    }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool flipper_recon_scene_message_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RECON_EVENT_CONFIRM_NO) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == RECON_EVENT_CONFIRM_YES) {
            consumed = true;
            if(app->message_mode == ReconMessageConfirmDeleteSession) {
                recon_storage_delete_session(app->storage, app->session->name);
                notification_message(app->notifications, &sequence_success);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FlipperReconSceneStart);
            } else if(app->message_mode == ReconMessageConfirmDeleteAsset) {
                asset_manager_delete(app->session, app->selected_asset);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FlipperReconSceneAssetList);
            } else if(app->message_mode == ReconMessageConfirmDeleteRelation) {
                graph_delete_relation(app->session, app->selected_relation);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FlipperReconSceneRelationList);
            } else if(app->message_mode == ReconMessageConfirmDeleteEvidence) {
                /* remove evidence by shifting the array down */
                Session* s = app->session;
                if(app->selected_evidence < s->evidence_count) {
                    for(uint16_t i = app->selected_evidence; i + 1 < s->evidence_count; i++) {
                        s->evidence[i] = s->evidence[i + 1];
                    }
                    s->evidence_count--;
                    session_touch(s);
                }
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FlipperReconSceneEvidenceList);
            } else if(app->message_mode == ReconMessageConfirmOverwrite) {
                flipper_recon_perform_save(app);
                /* success tone plays; return to the session menu */
                scene_manager_previous_scene(app->scene_manager);
            } else if(app->message_mode == ReconMessageConfirmDiscard) {
                session_mark_clean(app->session);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FlipperReconSceneStart);
            }
        }
    }
    return consumed;
}

void flipper_recon_scene_message_on_exit(void* context) {
    FlipperReconApp* app = context;
    widget_reset(app->widget);
}
