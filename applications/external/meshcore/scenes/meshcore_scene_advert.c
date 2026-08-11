/*
 * scene_advert — announce this node to the mesh.
 *
 * A node stays invisible to peers until it adverts; other nodes learn its key
 * and name from that. This sends a FLOOD advert (propagates through the mesh,
 * not just to direct neighbours), which is the "make me findable" action. It
 * connects itself first, like the rest of the app.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_ADVERT_EVENT_DONE 0x450u
#define MESHCORE_ADVERT_WORKER_STACK 2048u

static int32_t meshcore_advert_worker(void* context) {
    MeshCoreApp* app = context;

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    app->worker_error = meshcore_connect_ensure(app);
    if(app->worker_error == NULL) {
        size_t len = mc_cmd_send_self_advert(payload, sizeof(payload), MC_ADVERT_FLOOD);
        if(len == 0) {
            app->worker_error = "Could not build the advert.";
        } else if(!meshcore_session_request(
                      app->session, payload, len, MC_RESP_OK, &event, MESHCORE_LINK_TIMEOUT_MS)) {
            app->worker_error = (event.code == MC_RESP_ERR) ? "Node refused the advert." :
                                                              "No answer from the node.";
        } else {
            meshcore_log_printf(app->log, "advert sent (flood)");
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_ADVERT_EVENT_DONE);
    return 0;
}

void meshcore_scene_advert_on_enter(void* context) {
    MeshCoreApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "\e#Advert\nSending...");
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreAdvert", MESHCORE_ADVERT_WORKER_STACK, meshcore_advert_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_advert_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_ADVERT_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }

        widget_reset(app->widget);
        if(app->worker_error == NULL) {
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "\e#Advert sent\n"
                "Flood advert is on its way.\n\n"
                "Other nodes will list this\none once they hear it.");
        } else {
            char text[128];
            snprintf(text, sizeof(text), "\e#Not sent\n%s", app->worker_error);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
        }
        return true;
    }

    return false;
}

void meshcore_scene_advert_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    widget_reset(app->widget);
}
