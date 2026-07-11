#include "../breach_map_i.h"

typedef enum {
    MenuIndexAssets,
    MenuIndexRelations,
    MenuIndexGraph,
    MenuIndexSummary,
    MenuIndexDetails,
    MenuIndexExportJson,
    MenuIndexExportMd,
    MenuIndexSave,
    MenuIndexDelete,
} MenuIndex;

static void breach_map_scene_session_menu_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void breach_map_scene_session_menu_on_enter(void* context) {
    BreachMapApp* app = context;
    Submenu* submenu = app->submenu;
    Session* session = app->session;

    submenu_reset(submenu);
    submenu_set_header(submenu, session->name);

    FuriString* label = furi_string_alloc();
    furi_string_printf(label, "Assets (%u)", session->asset_count);
    submenu_add_item(
        submenu,
        furi_string_get_cstr(label),
        MenuIndexAssets,
        breach_map_scene_session_menu_callback,
        app);
    furi_string_printf(label, "Relations (%u)", session->relation_count);
    submenu_add_item(
        submenu,
        furi_string_get_cstr(label),
        MenuIndexRelations,
        breach_map_scene_session_menu_callback,
        app);
    furi_string_free(label);

    submenu_add_item(
        submenu, "Graph", MenuIndexGraph, breach_map_scene_session_menu_callback, app);
    submenu_add_item(
        submenu, "Summary", MenuIndexSummary, breach_map_scene_session_menu_callback, app);
    submenu_add_item(
        submenu, "Details", MenuIndexDetails, breach_map_scene_session_menu_callback, app);
    submenu_add_item(
        submenu, "Export JSON", MenuIndexExportJson, breach_map_scene_session_menu_callback, app);
    submenu_add_item(
        submenu, "Export Markdown", MenuIndexExportMd, breach_map_scene_session_menu_callback, app);
    submenu_add_item(submenu, "Save", MenuIndexSave, breach_map_scene_session_menu_callback, app);
    submenu_add_item(
        submenu, "Delete", MenuIndexDelete, breach_map_scene_session_menu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, BreachMapSceneSessionMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_session_menu_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BreachMapSceneSessionMenu, event.event);
        consumed = true;
        switch(event.event) {
        case MenuIndexAssets:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneAssetList);
            break;
        case MenuIndexRelations:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneRelationList);
            break;
        case MenuIndexGraph:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneGraph);
            break;
        case MenuIndexSummary:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneSummary);
            break;
        case MenuIndexDetails:
            scene_manager_next_scene(app->scene_manager, BreachMapSceneSessionDetails);
            break;
        case MenuIndexExportJson: {
            FuriString* path = furi_string_alloc();
            bool ok = report_export_json(app->storage, app->session, path);
            app->message_mode = ReconMessageInfo;
            if(ok) {
                furi_string_printf(
                    app->message_text, "JSON exported to:\n%s", furi_string_get_cstr(path));
                notification_message(app->notifications, &sequence_success);
            } else {
                furi_string_set(app->message_text, "Export failed");
                notification_message(app->notifications, &sequence_error);
            }
            furi_string_free(path);
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            break;
        }
        case MenuIndexExportMd: {
            FuriString* path = furi_string_alloc();
            bool ok = report_export_markdown(app->storage, app->session, path);
            app->message_mode = ReconMessageInfo;
            if(ok) {
                furi_string_printf(
                    app->message_text, "Markdown exported to:\n%s", furi_string_get_cstr(path));
                notification_message(app->notifications, &sequence_success);
            } else {
                furi_string_set(app->message_text, "Export failed");
                notification_message(app->notifications, &sequence_error);
            }
            furi_string_free(path);
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            break;
        }
        case MenuIndexSave: {
            char newbase[RECON_NAME_LEN];
            recon_sanitize_filename(app->session->name, newbase, sizeof(newbase));
            bool new_target = (app->session_file[0] == '\0') ||
                              strcmp(app->session_file, newbase) != 0;
            if(new_target && recon_storage_session_exists(app->storage, newbase)) {
                /* would clobber a different engagement -> ask first */
                app->message_mode = ReconMessageConfirmOverwrite;
            } else {
                breach_map_perform_save(app);
            }
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            break;
        }
        case MenuIndexDelete:
            app->message_mode = ReconMessageConfirmDeleteSession;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->session->dirty) {
            /* guard against leaving with unsaved changes */
            app->message_mode = ReconMessageConfirmDiscard;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            consumed = true;
        }
    }
    return consumed;
}

void breach_map_scene_session_menu_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
