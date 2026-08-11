/*
 * scene_chat — the conversation with one contact.
 *
 * The mailbox worker fills the store in the background; this scene just
 * renders it, and rebuilds when the store's running total changes. Polling a
 * counter on the scene tick is deliberate: the alternative is posting custom
 * events from the mailbox thread into whatever scene happens to be open, which
 * is more plumbing for the same result.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_CHAT_EVENT_WRITE 0x430u

static void meshcore_scene_chat_build(MeshCoreApp* app);

/* OK opens the keyboard. Runs on the GUI thread, so it only posts an event. */
static void meshcore_scene_chat_write(GuiButtonType type, InputType input, void* context) {
    MeshCoreApp* app = context;
    if(type != GuiButtonTypeCenter || input != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CHAT_EVENT_WRITE);
}

void meshcore_scene_chat_on_enter(void* context) {
    MeshCoreApp* app = context;

    /* Build before showing: deferring to the first tick would leave the
     * conversation blank for up to MESHCORE_TICK_PERIOD_MS. */
    meshcore_scene_chat_build(app);
    scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneChat, app->messages.total);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
}

static void meshcore_scene_chat_append(FuriString* out, MeshCoreApp* app, const MeshCoreMessage* message) {
    char age[MESHCORE_AGE_LEN];
    meshcore_contacts_format_age(app->node_time, message->timestamp, age, sizeof(age));

    if(message->direction == MeshCoreMessageOutgoing) {
        /* "\er" right-aligns until the newline, which is the closest the text
         * scroll element gets to the usual chat layout. Proper inversion needs
         * a custom view with its own draw callback. */
        furi_string_cat_printf(out, "\erme  %s\n\er%s\n", age, message->text);
    } else {
        furi_string_cat_printf(out, "\e#%.20s  %s\n%s\n", app->chat_peer_name, age, message->text);
    }
}

static void meshcore_scene_chat_build(MeshCoreApp* app) {
    FuriString* out = app->text_buf[0];
    furi_string_reset(out);

    size_t shown = 0;

    /* Newest first — the text scroll always opens at the top and cannot be sent
     * to the bottom, so the latest message has to be what shows up there. Walk
     * the ring backwards (meshcore_messages_at is oldest-first). The store
     * counts at least one message, so count-1 does not underflow the guard. */
    meshcore_mailbox_lock(app->mailbox);
    for(size_t i = app->messages.count; i-- > 0;) {
        const MeshCoreMessage* message = meshcore_messages_at(&app->messages, i);
        /* A channel chat shows that channel's traffic; a direct chat shows the
         * conversation with one peer. */
        bool mine = app->chat_is_channel ?
                        (message->is_channel && message->channel_idx == app->chat_channel_idx) :
                        (!message->is_channel &&
                         meshcore_message_is_from(message, app->chat_peer));
        if(!mine) continue;
        meshcore_scene_chat_append(out, app, message);
        shown++;
    }
    meshcore_mailbox_unlock(app->mailbox);

    if(shown == 0) {
        furi_string_printf(
            out,
            "\e#%.20s\n"
            "No messages yet.\n\n"
            "Incoming mail is pulled from\n"
            "the node as it arrives.",
            app->chat_peer_name);
    }

    widget_reset(app->widget);
    /* The scroll stops short of the bottom so the Write button has a strip of
     * its own; the widget copies the string, so reusing this buffer is safe. */
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, furi_string_get_cstr(out));
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Write", meshcore_scene_chat_write, app);
}

bool meshcore_scene_chat_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_CHAT_EVENT_WRITE) {
        scene_manager_next_scene(app->scene_manager, MeshCoreSceneCompose);
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        /* total counts every message ever stored, so it changes even when the
         * ring has already dropped an older one. */
        uint32_t seen = scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneChat);
        uint32_t now = app->messages.total;
        if(seen != now) {
            scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneChat, now);
            meshcore_scene_chat_build(app);
        }
        return true;
    }

    return false;
}

void meshcore_scene_chat_on_exit(void* context) {
    MeshCoreApp* app = context;
    widget_reset(app->widget);
}
