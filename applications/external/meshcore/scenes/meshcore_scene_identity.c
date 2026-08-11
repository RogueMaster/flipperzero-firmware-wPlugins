/*
 * scene_identity — rename the node.
 *
 * The name is what other nodes show for this one in their contact lists, so it
 * is worth being able to set from the Flipper. A keyboard takes the new name;
 * on submit a worker connects (if needed), sends SET_ADVERT_NAME, and re-reads
 * SELF_INFO to prove the node took it — the same "confirm, do not assume"
 * discipline as Apply and the Radio editor.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_IDENTITY_EVENT_SET 0x460u
#define MESHCORE_IDENTITY_EVENT_DONE 0x461u
#define MESHCORE_IDENTITY_WORKER_STACK 2048u

static void meshcore_identity_input_done(void* context) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IDENTITY_EVENT_SET);
}

static int32_t meshcore_identity_worker(void* context) {
    MeshCoreApp* app = context;

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    app->worker_error = meshcore_connect_ensure(app);
    if(app->worker_error != NULL) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IDENTITY_EVENT_DONE);
        return 0;
    }

    size_t len = mc_cmd_set_advert_name(payload, sizeof(payload), app->identity_buf);
    if(len == 0) {
        app->worker_error = "Name too long.";
    } else if(!meshcore_session_request(
                  app->session, payload, len, MC_RESP_OK, &event, MESHCORE_LINK_TIMEOUT_MS)) {
        app->worker_error =
            (event.code == MC_RESP_ERR) ? "Node refused the name." : "No answer from the node.";
    } else {
        /* Confirm by re-reading, then keep the app's copy in step. */
        len = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);
        if(len == 0 || !meshcore_session_request(
                           app->session,
                           payload,
                           len,
                           MC_RESP_SELF_INFO,
                           &event,
                           MESHCORE_LINK_TIMEOUT_MS)) {
            app->worker_error = "Set, but could not confirm.";
        } else if(strncmp(event.u.self_info.name, app->identity_buf, MC_NAME_LEN) != 0) {
            app->worker_error = "Node said OK but kept its name.";
        } else {
            snprintf(app->node.name, sizeof(app->node.name), "%s", app->identity_buf);
            meshcore_log_printf(app->log, "identity set: %.32s", app->identity_buf);
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_IDENTITY_EVENT_DONE);
    return 0;
}

void meshcore_scene_identity_on_enter(void* context) {
    MeshCoreApp* app = context;

    /* Seed the field with the current name so the user edits rather than
     * retypes. If we never connected, it starts empty. */
    snprintf(
        app->identity_buf, sizeof(app->identity_buf), "%.32s", app->node.valid ? app->node.name : "");

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Node name");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        meshcore_identity_input_done,
        app,
        app->identity_buf,
        sizeof(app->identity_buf),
        false /* keep the current name as the starting text */);

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewTextInput);
}

bool meshcore_scene_identity_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_IDENTITY_EVENT_SET) {
        if(app->worker != NULL) return true;

        widget_reset(app->widget);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "\e#Node name\nSetting...");
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

        app->worker = furi_thread_alloc_ex(
            "MeshCoreIdentity", MESHCORE_IDENTITY_WORKER_STACK, meshcore_identity_worker, app);
        furi_thread_start(app->worker);
        return true;
    }

    if(event.event == MESHCORE_IDENTITY_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }

        char text[128];
        widget_reset(app->widget);
        if(app->worker_error == NULL) {
            snprintf(
                text,
                sizeof(text),
                "\e#Name set\n%.32s\n\nOther nodes will show this\nafter the next advert.",
                app->identity_buf);
        } else {
            snprintf(text, sizeof(text), "\e#Not set\n%s\n\nBack to try again.", app->worker_error);
        }
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        return true;
    }

    return false;
}

void meshcore_scene_identity_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    text_input_reset(app->text_input);
}
