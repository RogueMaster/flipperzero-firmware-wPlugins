/*
 * scene_contacts — the mesh as the node sees it: who is out there and how
 * recently we heard from them.
 *
 * GET_CONTACTS is answered as a stream (CONTACTS_START, then one CONTACT per
 * peer, then END_OF_CONTACTS), so this uses the session's streaming request
 * and collects into MeshCoreContacts on the worker thread.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_CONTACTS_EVENT_DONE 0x200u
/* Picking contact i sends PICK + i, so the base sits clear of DONE. */
#define MESHCORE_CONTACTS_EVENT_PICK 0x1000u
/* The "Channels" and "Add / share" rows ride the same callback with indices no
 * contact list will ever reach. */
#define MESHCORE_CONTACTS_CHANNELS_ROW 0xFFFFu
#define MESHCORE_CONTACTS_ADD_ROW 0xFFFEu
#define MESHCORE_CONTACTS_WORKER_STACK 2048u

/* A node with a full address book sends ~150 bytes per contact; at 115200 that
 * is a couple of seconds for a few hundred peers. Generous, but the user is
 * looking at a spinner, and scene_connect has already established whether the
 * node answers at all. */
#define MESHCORE_CONTACTS_TIMEOUT_MS 6000u

static const char* meshcore_contacts_run(MeshCoreApp* app) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t ev;
    size_t len;

    /* Connect ourselves rather than making the user find the Connect screen
     * first: opening the messenger should just work. Idempotent, so if they did
     * connect it is free. */
    const char* err = meshcore_connect_ensure(app);
    if(err != NULL) return err;

    /* Ages are relative to the node's clock, not the Flipper's — last_advert
     * is in the node's timebase. A node that will not tell us the time just
     * means ages render as "-", which is better than showing wrong ones. */
    len = mc_cmd_get_device_time(payload, sizeof(payload));
    if(len != 0 &&
       meshcore_session_request(
           app->session, payload, len, MC_RESP_CURR_TIME, &ev, MESHCORE_LINK_TIMEOUT_MS)) {
        app->node_time = ev.u.curr_time;
    } else {
        app->node_time = 0;
    }

    if(app->worker_stop) return "Cancelled.";

    meshcore_contacts_reset(&app->contacts);

    /* 0 = send everything, rather than a delta since some lastmod. */
    len = mc_cmd_get_contacts(payload, sizeof(payload), 0);
    if(len == 0) return "Internal error: GET_CONTACTS.";

    if(!meshcore_session_request_stream(
           app->session,
           payload,
           len,
           MC_RESP_END_OF_CONTACTS,
           meshcore_contacts_collect,
           &app->contacts,
           &ev,
           MESHCORE_CONTACTS_TIMEOUT_MS)) {
        if(ev.code == MC_RESP_ERR) return "Node refused GET_CONTACTS.";
        /* Some contacts may have arrived before the stream stalled; showing
         * them beats showing nothing. */
        if(app->contacts.count > 0) return NULL;
        return "No contact list from the node.";
    }

    meshcore_contacts_sort_by_last_seen(&app->contacts);
    return NULL;
}

static int32_t meshcore_contacts_worker(void* context) {
    MeshCoreApp* app = context;
    app->worker_error = meshcore_contacts_run(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CONTACTS_EVENT_DONE);
    return 0;
}

static void meshcore_scene_contacts_submenu_callback(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, MESHCORE_CONTACTS_EVENT_PICK + index);
}

static void meshcore_scene_contacts_show_error(MeshCoreApp* app) {
    char text[256];
    widget_reset(app->widget);
    snprintf(
        text,
        sizeof(text),
        "\e#Contacts\n"
        "%s\n\n"
        "Plug the node in and reboot\nit, so it comes up in serial.",
        app->worker_error);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
}

static void meshcore_scene_contacts_show_list(MeshCoreApp* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    /* Roomy enough for the worst case the compiler can imagine: a node that
     * claims a ten-digit contact count. */
    char header[40];
    if(app->contacts.reported > app->contacts.count) {
        /* Say so rather than silently truncating. */
        snprintf(
            header,
            sizeof(header),
            "Contacts %u/%lu",
            (unsigned)app->contacts.count,
            (unsigned long)app->contacts.reported);
    } else {
        snprintf(header, sizeof(header), "Contacts (%u)", (unsigned)app->contacts.count);
    }
    submenu_set_header(submenu, header);

    /* Group chats and add/share live behind these rows, at the top so they are
     * always reachable even with a full address book. */
    submenu_add_item(
        submenu,
        "# Channels",
        MESHCORE_CONTACTS_CHANNELS_ROW,
        meshcore_scene_contacts_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "+ Add / share",
        MESHCORE_CONTACTS_ADD_ROW,
        meshcore_scene_contacts_submenu_callback,
        app);

    for(size_t i = 0; i < app->contacts.count; i++) {
        const MeshCoreContact* contact = &app->contacts.items[i];

        char age[MESHCORE_AGE_LEN];
        meshcore_contacts_format_age(app->node_time, contact->last_advert, age, sizeof(age));

        /* Submenu draws a single left-aligned label in a proportional font, so
         * a real right-hand column is not on offer; a separator is honest. */
        char label[48];
        snprintf(label, sizeof(label), "%.28s  %s", contact->name, age);

        submenu_add_item(
            submenu, label, (uint32_t)i, meshcore_scene_contacts_submenu_callback, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneContacts));
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSubmenu);
}

void meshcore_scene_contacts_on_enter(void* context) {
    MeshCoreApp* app = context;

    app->worker_stop = false;
    app->worker_error = NULL;

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewLoading);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreContacts", MESHCORE_CONTACTS_WORKER_STACK, meshcore_contacts_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_contacts_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MESHCORE_CONTACTS_EVENT_DONE) {
            if(app->worker_error) {
                meshcore_scene_contacts_show_error(app);
            } else {
                /* Even with zero peers, show the list: it carries the Channels
                 * and Add/share rows, and Add/share is exactly how a node with
                 * an empty book gets its first contact. */
                meshcore_scene_contacts_show_list(app);
            }
            return true;
        }

        if(event.event >= MESHCORE_CONTACTS_EVENT_PICK) {
            size_t index = event.event - MESHCORE_CONTACTS_EVENT_PICK;

            if(index == MESHCORE_CONTACTS_CHANNELS_ROW) {
                scene_manager_next_scene(app->scene_manager, MeshCoreSceneChannels);
                return true;
            }

            if(index == MESHCORE_CONTACTS_ADD_ROW) {
                scene_manager_next_scene(app->scene_manager, MeshCoreSceneAddContact);
                return true;
            }

            if(index < app->contacts.count) {
                const MeshCoreContact* contact = &app->contacts.items[index];
                /* Copy the peer out of the list: a refresh may reorder or drop
                 * entries while the chat is open, and the conversation is
                 * keyed by the key, not by a row number. */
                memcpy(app->chat_peer, contact->public_key, sizeof(app->chat_peer));
                snprintf(
                    app->chat_peer_name, sizeof(app->chat_peer_name), "%s", contact->name);
                /* A direct chat, not a channel. */
                app->chat_is_channel = false;

                scene_manager_set_scene_state(
                    app->scene_manager, MeshCoreSceneContacts, (uint32_t)index);
                scene_manager_next_scene(app->scene_manager, MeshCoreSceneChat);
            }
            return true;
        }
    }

    return false;
}

void meshcore_scene_contacts_on_exit(void* context) {
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
