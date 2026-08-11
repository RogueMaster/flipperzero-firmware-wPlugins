/*
 * scene_import — add a contact from a shared meshcore:// link.
 *
 * The official share format (docs.meshcore.io/qr_codes) is a query URI:
 *   meshcore://contact/add?name=<url-enc>&public_key=<64 hex>&type=<1-4>
 * A keyboard takes the link, meshcore_contact_uri_parse turns it into a contact
 * record, and ADD_UPDATE_CONTACT (cmd 9) hands it to the node — the same
 * command the reference client uses to add a contact from a key, name and type.
 * On success we drop into a fresh Contacts read so the newcomer shows up.
 *
 * This is the out-of-band fallback; the field-native way to meet nodes is still
 * adverts (Find peers). Typing a 100-char link on the keypad is nobody's idea
 * of fun, but it is the documented way to add a contact you were handed rather
 * than heard.
 */
#include "../meshcore_cfg.h"
#include "../messenger/meshcore_share_uri.h"

#define MESHCORE_IMPORT_EVENT_ADD 0x4B0u
#define MESHCORE_IMPORT_EVENT_DONE 0x4B1u
#define MESHCORE_IMPORT_WORKER_STACK 2048u

static void meshcore_import_input_done(void* context) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IMPORT_EVENT_ADD);
}

static int32_t meshcore_import_worker(void* context) {
    MeshCoreApp* app = context;

    mc_contact_t contact;
    if(!meshcore_contact_uri_parse(app->import_buf, &contact)) {
        app->worker_error = "Not a MeshCore link.";
        view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IMPORT_EVENT_DONE);
        return 0;
    }

    app->worker_error = meshcore_connect_ensure(app);
    if(app->worker_error != NULL) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IMPORT_EVENT_DONE);
        return 0;
    }

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;
    size_t len = mc_cmd_add_update_contact(payload, sizeof(payload), &contact);
    if(len == 0) {
        app->worker_error = "Could not build the contact.";
    } else if(!meshcore_session_request(
                  app->session, payload, len, MC_RESP_OK, &event, MESHCORE_LINK_TIMEOUT_MS)) {
        app->worker_error =
            (event.code == MC_RESP_ERR) ? "Node refused the contact." : "No answer from the node.";
    } else {
        meshcore_log_printf(app->log, "imported contact %.32s", contact.adv_name);
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IMPORT_EVENT_DONE);
    return 0;
}

void meshcore_scene_import_on_enter(void* context) {
    MeshCoreApp* app = context;

    app->import_buf[0] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Paste meshcore:// link");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        meshcore_import_input_done,
        app,
        app->import_buf,
        sizeof(app->import_buf),
        true /* clear on first key */);

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewTextInput);
}

bool meshcore_scene_import_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_IMPORT_EVENT_ADD) {
        if(app->worker != NULL) return true;
        app->worker_error = NULL;

        widget_reset(app->widget);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "\e#Import\nAdding contact...");
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

        app->worker = furi_thread_alloc_ex(
            "MeshCoreImport", MESHCORE_IMPORT_WORKER_STACK, meshcore_import_worker, app);
        furi_thread_start(app->worker);
        return true;
    }

    if(event.event == MESHCORE_IMPORT_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }

        if(app->worker_error == NULL) {
            /* Added: a fresh Contacts read shows the newcomer. */
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneContacts);
        } else {
            char text[160];
            snprintf(
                text,
                sizeof(text),
                "\e#Not added\n%s\n\nPaste a link like\nmeshcore://contact/add?...",
                app->worker_error);
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        }
        return true;
    }

    return false;
}

void meshcore_scene_import_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        app->worker_stop = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    text_input_reset(app->text_input);
    widget_reset(app->widget);
}
