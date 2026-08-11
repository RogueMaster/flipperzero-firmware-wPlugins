/*
 * scene_channels — group chats, the other half of the messenger.
 *
 * A direct message goes to one contact; a channel message goes to everyone
 * tuned to that channel. Channel 0 is the well-known "public" channel every
 * node has; further slots are private groups, each with a shared secret. This
 * lists what the node has and opens a channel as a chat, reusing the chat and
 * compose scenes in their channel mode.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_CHANNELS_EVENT_DONE 0x480u
#define MESHCORE_CHANNELS_EVENT_PICK 0x1000u
/* The "New / join" row rides the same callback with an index no channel list
 * will ever reach. */
#define MESHCORE_CHANNELS_ADD_ROW 0xFFFFu
#define MESHCORE_CHANNELS_WORKER_STACK 2048u
/* Firmware advertises max_channels; a handful covers any field setup and keeps
 * the query short. */
#define MESHCORE_CHANNELS_MAX 8u

typedef struct {
    uint8_t idx;
    char name[MC_NAME_LEN + 1];
    bool have_secret;
} MeshCoreChannelRow;

static struct {
    MeshCoreChannelRow rows[MESHCORE_CHANNELS_MAX];
    size_t count;
} channels;

/* Two channels are "the same" if their name and privacy match — a node that
 * reports the public channel in every empty slot then collapses to one row
 * instead of a wall of duplicates. */
static bool meshcore_channels_seen(const char* name, bool have_secret) {
    for(size_t i = 0; i < channels.count; i++) {
        if(channels.rows[i].have_secret == have_secret &&
           strncmp(channels.rows[i].name, name, MC_NAME_LEN) == 0) {
            return true;
        }
    }
    return false;
}

static const char* meshcore_channels_run(MeshCoreApp* app) {
    const char* err = meshcore_connect_ensure(app);
    if(err != NULL) return err;

    channels.count = 0;
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t ev;

    for(uint8_t idx = 0; idx < MESHCORE_CHANNELS_MAX && channels.count < MESHCORE_CHANNELS_MAX;
        idx++) {
        size_t len = mc_cmd_get_channel(payload, sizeof(payload), idx);
        if(len == 0) break;
        if(!meshcore_session_request(
               app->session, payload, len, MC_RESP_CHANNEL_INFO, &ev, MESHCORE_LINK_TIMEOUT_MS)) {
            /* A node that does not answer for this slot: stop asking. */
            break;
        }
        const char* name = ev.u.channel_info.name;
        /* An unconfigured slot comes back with an empty name; skip it. Index 0
         * is always the public channel, named or not. */
        if(name[0] == '\0' && idx != 0) continue;

        const char* shown = (name[0] == '\0') ? "public" : name;
        /* Slot 0 is the reserved public channel; 1-7 are private. Classify by
         * index, not have_secret: CHANNEL_INFO always carries the 16-byte secret
         * field (so have_secret is always set), and real firmware fills slot 0
         * with the well-known public key, not zeros. */
        bool priv = (idx != 0);
        if(meshcore_channels_seen(shown, priv)) continue;

        MeshCoreChannelRow* row = &channels.rows[channels.count++];
        row->idx = idx;
        snprintf(row->name, sizeof(row->name), "%s", shown);
        row->have_secret = priv;
    }

    /* Zero channels is not an error: the list still carries the New/join row,
     * which is how the first private channel gets created. Only a failure to
     * reach the node (connect_ensure above) is worth an error screen. */
    return NULL;
}

static int32_t meshcore_channels_worker(void* context) {
    MeshCoreApp* app = context;
    app->worker_error = meshcore_channels_run(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELS_EVENT_DONE);
    return 0;
}

static void meshcore_channels_callback(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHANNELS_EVENT_PICK + index);
}

static void meshcore_channels_show_list(MeshCoreApp* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Channels");

    /* Create or join a private channel; kept at the top so it is reachable even
     * when the node reports only the public channel (or none). */
    submenu_add_item(
        submenu, "+ New / join channel", MESHCORE_CHANNELS_ADD_ROW, meshcore_channels_callback, app);

    for(size_t i = 0; i < channels.count; i++) {
        char label[40];
        /* A padlock for private, a hash for open — so the two kinds read apart
         * at a glance. */
        snprintf(
            label,
            sizeof(label),
            "%s %.28s",
            channels.rows[i].have_secret ? "*" : "#",
            channels.rows[i].name);
        submenu_add_item(submenu, label, (uint32_t)i, meshcore_channels_callback, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneChannels));
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSubmenu);
}

void meshcore_scene_channels_on_enter(void* context) {
    MeshCoreApp* app = context;

    app->worker_stop = false;
    app->worker_error = NULL;
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewLoading);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreChannels", MESHCORE_CHANNELS_WORKER_STACK, meshcore_channels_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_channels_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_CHANNELS_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        if(app->worker_error) {
            char text[192];
            snprintf(
                text,
                sizeof(text),
                "\e#Channels\n%s\n\nEvery node has the public\nchannel; private ones need\ntheir secret set first.",
                app->worker_error);
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        } else {
            meshcore_channels_show_list(app);
        }
        return true;
    }

    if(event.event >= MESHCORE_CHANNELS_EVENT_PICK) {
        size_t i = event.event - MESHCORE_CHANNELS_EVENT_PICK;

        if(i == MESHCORE_CHANNELS_ADD_ROW) {
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneChannelAdd);
            return true;
        }

        if(i < channels.count) {
            /* Open the channel as a chat: the chat and compose scenes switch to
             * their channel mode off these three fields. */
            app->chat_is_channel = true;
            app->chat_channel_idx = channels.rows[i].idx;
            snprintf(
                app->chat_peer_name, sizeof(app->chat_peer_name), "#%.30s", channels.rows[i].name);
            scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneChannels, (uint32_t)i);
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneChat);
        }
        return true;
    }

    return false;
}

void meshcore_scene_channels_on_exit(void* context) {
    MeshCoreApp* app = context;
    if(app->worker) {
        app->worker_stop = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
