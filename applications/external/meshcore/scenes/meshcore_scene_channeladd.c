/*
 * scene_channeladd — create or join a private channel.
 *
 * A private group is just a channel slot (1-7) carrying a shared 16-byte secret;
 * everyone who holds the secret can read it. Two ways in:
 *
 *  - New private channel: the Flipper generates a random secret, writes it to
 *    the first free slot with SET_CHANNEL (cmd 32), reads it back to confirm,
 *    and shows a meshcore://channel/add link to hand to the others. No typing.
 *  - Join from link: paste a meshcore://channel/add?…&secret=… link; it is
 *    parsed and written to a free slot the same way.
 *
 * SET_CHANNEL is exactly how the reference client creates a channel; the secret
 * is what nodes match on, the name is a local label. Sending to the channel and
 * listing it are the existing Channels/Chat scenes.
 */
#include <furi_hal_random.h>

#include "../meshcore_cfg.h"
#include "../messenger/meshcore_share_uri.h"

#define MESHCORE_CHANNELADD_EVENT_PICK 0x1000u
#define MESHCORE_CHANNELADD_EVENT_NEW_DONE 0x4C0u
#define MESHCORE_CHANNELADD_EVENT_JOIN_INPUT 0x4C1u
#define MESHCORE_CHANNELADD_EVENT_JOIN_DONE 0x4C2u
#define MESHCORE_CHANNELADD_WORKER_STACK 2048u

typedef enum {
    MeshCoreChannelAddNew,
    MeshCoreChannelAddJoin,
} MeshCoreChannelAddItem;

/* The channel just created, held between the worker finishing and the result
 * view being drawn. One at a time, so file-static is enough. */
static struct {
    char name[MC_NAME_LEN + 1];
    char secret_hex[MC_SECRET_LEN * 2 + 1];
    bool valid;
} created;

/* First unconfigured slot in 1..7, or -1 if all are taken / the node stops
 * answering. Slot 0 is the public channel and never a candidate. */
static int meshcore_channeladd_free_slot(MeshCoreApp* app) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t ev;
    for(uint8_t idx = 1; idx < 8; idx++) {
        size_t len = mc_cmd_get_channel(payload, sizeof(payload), idx);
        if(len == 0) return -1;
        if(!meshcore_session_request(
               app->session, payload, len, MC_RESP_CHANNEL_INFO, &ev, MESHCORE_LINK_TIMEOUT_MS)) {
            return -1;
        }
        if(ev.u.channel_info.name[0] == '\0') return (int)idx;
    }
    return -1;
}

/* Write name+secret to `slot` and read it back to prove the node took it. */
static const char* meshcore_channeladd_write(
    MeshCoreApp* app,
    uint8_t slot,
    const char* name,
    const uint8_t secret[MC_SECRET_LEN]) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t ev;

    size_t len = mc_cmd_set_channel(payload, sizeof(payload), slot, name, secret);
    if(len == 0) return "Could not build the channel.";
    if(!meshcore_session_request(
           app->session, payload, len, MC_RESP_OK, &ev, MESHCORE_LINK_TIMEOUT_MS)) {
        return (ev.code == MC_RESP_ERR) ? "Node refused the channel." : "No answer from the node.";
    }

    len = mc_cmd_get_channel(payload, sizeof(payload), slot);
    if(len == 0 || !meshcore_session_request(
                       app->session, payload, len, MC_RESP_CHANNEL_INFO, &ev,
                       MESHCORE_LINK_TIMEOUT_MS)) {
        return "Set, but could not confirm.";
    }
    if(strncmp(ev.u.channel_info.name, name, MC_NAME_LEN) != 0) {
        return "Node said OK but kept the slot empty.";
    }
    return NULL;
}

static void meshcore_channeladd_hex(const uint8_t* in, size_t n, char* out) {
    static const char* d = "0123456789abcdef";
    for(size_t i = 0; i < n; i++) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 0x0F];
    }
    out[n * 2] = '\0';
}

static int32_t meshcore_channeladd_new_worker(void* context) {
    MeshCoreApp* app = context;
    created.valid = false;

    app->worker_error = meshcore_connect_ensure(app);
    if(app->worker_error == NULL) {
        int slot = meshcore_channeladd_free_slot(app);
        if(slot < 0) {
            app->worker_error = "No free channel slot (1-7 full).";
        } else {
            uint8_t secret[MC_SECRET_LEN];
            furi_hal_random_fill_buf(secret, sizeof(secret));
            char name[MC_NAME_LEN + 1];
            snprintf(name, sizeof(name), "grp%d", slot);

            app->worker_error = meshcore_channeladd_write(app, (uint8_t)slot, name, secret);
            if(app->worker_error == NULL) {
                snprintf(created.name, sizeof(created.name), "%s", name);
                meshcore_channeladd_hex(secret, sizeof(secret), created.secret_hex);
                created.valid = true;
                meshcore_log_printf(app->log, "created channel %s in slot %d", name, slot);
            }
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELADD_EVENT_NEW_DONE);
    return 0;
}

static int32_t meshcore_channeladd_join_worker(void* context) {
    MeshCoreApp* app = context;

    char name[MC_NAME_LEN + 1];
    uint8_t secret[MC_SECRET_LEN];
    if(!meshcore_channel_uri_parse(app->import_buf, name, sizeof(name), secret)) {
        app->worker_error = "Not a channel link.";
        view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELADD_EVENT_JOIN_DONE);
        return 0;
    }
    /* An empty name would look like a free slot; give it a label. */
    if(name[0] == '\0') snprintf(name, sizeof(name), "group");

    app->worker_error = meshcore_connect_ensure(app);
    if(app->worker_error == NULL) {
        int slot = meshcore_channeladd_free_slot(app);
        if(slot < 0) {
            app->worker_error = "No free channel slot (1-7 full).";
        } else {
            app->worker_error = meshcore_channeladd_write(app, (uint8_t)slot, name, secret);
            if(app->worker_error == NULL) {
                meshcore_log_printf(app->log, "joined channel %.32s in slot %d", name, slot);
            }
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELADD_EVENT_JOIN_DONE);
    return 0;
}

static void meshcore_channeladd_callback(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELADD_EVENT_PICK + index);
}

static void meshcore_channeladd_input_done(void* context) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELADD_EVENT_JOIN_INPUT);
}

static void meshcore_channeladd_start(MeshCoreApp* app, FuriThreadCallback worker, const char* busy) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, busy);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
    app->worker =
        furi_thread_alloc_ex("MeshCoreChanAdd", MESHCORE_CHANNELADD_WORKER_STACK, worker, app);
    furi_thread_start(app->worker);
}

void meshcore_scene_channeladd_on_enter(void* context) {
    MeshCoreApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Private channel");
    submenu_add_item(
        submenu, "New private channel", MeshCoreChannelAddNew, meshcore_channeladd_callback, app);
    submenu_add_item(
        submenu, "Join from link", MeshCoreChannelAddJoin, meshcore_channeladd_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneChannelAdd));

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSubmenu);
}

bool meshcore_scene_channeladd_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_CHANNELADD_EVENT_NEW_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        char text[320];
        widget_reset(app->widget);
        if(app->worker_error == NULL && created.valid) {
            snprintf(
                text,
                sizeof(text),
                "\e#Channel %.16s\nShare this link so others\ncan join:\n\n\e*meshcore://channel/add?name=%s&secret=%s\e*",
                created.name,
                created.name,
                created.secret_hex);
        } else {
            snprintf(
                text,
                sizeof(text),
                "\e#Not created\n%s\n\nBack to try again.",
                app->worker_error ? app->worker_error : "Unknown error.");
        }
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        return true;
    }

    if(event.event == MESHCORE_CHANNELADD_EVENT_JOIN_INPUT) {
        if(app->worker != NULL) return true;
        app->worker_error = NULL;
        meshcore_channeladd_start(app, meshcore_channeladd_join_worker, "\e#Join\nJoining channel...");
        return true;
    }

    if(event.event == MESHCORE_CHANNELADD_EVENT_JOIN_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        if(app->worker_error == NULL) {
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneChannels);
        } else {
            char text[160];
            snprintf(
                text,
                sizeof(text),
                "\e#Not joined\n%s\n\nPaste a link like\nmeshcore://channel/add?...",
                app->worker_error);
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        }
        return true;
    }

    if(event.event >= MESHCORE_CHANNELADD_EVENT_PICK) {
        size_t index = event.event - MESHCORE_CHANNELADD_EVENT_PICK;
        scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneChannelAdd, (uint32_t)index);

        if(index == MeshCoreChannelAddNew) {
            if(app->worker != NULL) return true;
            app->worker_error = NULL;
            meshcore_channeladd_start(
                app, meshcore_channeladd_new_worker, "\e#New channel\nCreating...");
            return true;
        }

        if(index == MeshCoreChannelAddJoin) {
            app->import_buf[0] = '\0';
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Paste channel link");
            text_input_set_minimum_length(app->text_input, 1);
            text_input_set_result_callback(
                app->text_input,
                meshcore_channeladd_input_done,
                app,
                app->import_buf,
                sizeof(app->import_buf),
                true);
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewTextInput);
            return true;
        }
    }

    return false;
}

void meshcore_scene_channeladd_on_exit(void* context) {
    MeshCoreApp* app = context;
    if(app->worker) {
        app->worker_stop = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    submenu_reset(app->submenu);
    text_input_reset(app->text_input);
    widget_reset(app->widget);
}
