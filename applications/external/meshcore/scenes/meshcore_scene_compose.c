/*
 * scene_compose — write a message and send it to the current chat peer.
 *
 * Sending is not a protocol primitive: it is SEND_TXT_MSG, and the node's
 * MSG_SENT reply is the acknowledgement that it took the message for delivery.
 * That is the same handshake the logger's ping rides on, so the mechanics are
 * already proven on hardware; this scene wraps a keyboard around it.
 *
 * The send blocks on the link, so it runs on a worker thread. TextInput is the
 * keyboard; the widget shows "Sending..." and any error. On success the
 * outgoing line is recorded in the message store and the scene returns to the
 * chat, which rebuilds and shows it — no extra plumbing, because chat already
 * redraws when messages.total changes.
 */
#include <furi_hal_rtc.h>

#include "../meshcore_cfg.h"
#include "../messenger/meshcore_messages.h"

#define MESHCORE_COMPOSE_EVENT_SEND 0x420u
#define MESHCORE_COMPOSE_EVENT_DONE 0x421u
#define MESHCORE_COMPOSE_WORKER_STACK 2048u

/* Runs on the GUI thread when the keyboard's OK is pressed. Only posts an
 * event; the send itself must not happen here. */
static void meshcore_scene_compose_input_done(void* context) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_COMPOSE_EVENT_SEND);
}

static int32_t meshcore_scene_compose_worker(void* context) {
    MeshCoreApp* app = context;

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    app->worker_error = NULL;

    /* Stamped once, so the value on the wire and the value stored for the chat
     * are the same moment. Unix seconds from the Flipper RTC, matching what the
     * reference client sends and the timebase chat renders ages against once
     * clocks are synced (which the field procedure requires). */
    uint32_t sender_ts = (uint32_t)furi_hal_rtc_get_timestamp();

    /* A channel message goes to everyone on the channel; a direct message to
     * one peer (the 6-byte prefix is all the node needs to route it — the
     * logger's ping sends to exactly this). */
    size_t len;
    if(app->chat_is_channel) {
        len = mc_cmd_send_channel_text(
            payload, sizeof(payload), MC_TXT_PLAIN, app->chat_channel_idx, sender_ts, app->compose_buf);
    } else {
        len = mc_cmd_send_txt_msg(
            payload,
            sizeof(payload),
            MC_TXT_PLAIN,
            0,
            sender_ts,
            app->chat_peer,
            MESHCORE_PEER_LEN,
            app->compose_buf);
    }

    if(len == 0) {
        app->worker_error = "Message too long.";
    } else if(!meshcore_session_request(
                  app->session, payload, len, MC_RESP_SENT, &event, MESHCORE_LINK_TIMEOUT_MS)) {
        /* MC_RESP_ERR means the node refused it; silence means it never
         * answered. Either way it is not on its way. */
        app->worker_error =
            (event.code == MC_RESP_ERR) ? "Node refused the message." : "No answer from the node.";
    } else {
        /* Recorded only once the node has taken it, so the chat never shows a
         * line that was never actually handed off. The store keys outgoing
         * messages by the same 6-byte peer the chat filters on. */
        MeshCoreMessage message;
        memset(&message, 0, sizeof(message));
        if(app->chat_is_channel) {
            message.is_channel = true;
            message.channel_idx = app->chat_channel_idx;
        } else {
            memcpy(message.peer, app->chat_peer, MESHCORE_PEER_LEN);
        }
        message.direction = MeshCoreMessageOutgoing;
        message.timestamp = sender_ts;
        message.snr_q4 = MC_SNR_NONE;
        message.path_len = MC_PATH_DIRECT;
        snprintf(message.text, sizeof(message.text), "%s", app->compose_buf);

        /* The store is a ring shared with the mailbox worker (which drains
         * incoming mail) and read by the chat scene; both guard it with the
         * mailbox lock, and so must this add — without it a concurrent drain
         * tears head/count/total and the chat renders garbage. */
        meshcore_mailbox_lock(app->mailbox);
        meshcore_messages_add(&app->messages, &message);
        meshcore_mailbox_unlock(app->mailbox);
        /* Persist the sent line too, so the chat history is whole across a
         * restart — outside the lock, like the mailbox does for incoming. */
        meshcore_msglog_append(app->msglog, &message);

        meshcore_log_printf(app->log, "sent to %.20s: %.40s", app->chat_peer_name, app->compose_buf);
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_COMPOSE_EVENT_DONE);
    return 0;
}

void meshcore_scene_compose_on_enter(void* context) {
    MeshCoreApp* app = context;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, app->chat_peer_name);
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        meshcore_scene_compose_input_done,
        app,
        app->compose_buf,
        /* One packet's worth: the node carries at most MESHCORE_MSG_MAX chars in
         * a single message, and we do not split. +1 for the NUL. */
        MESHCORE_MSG_MAX + 1,
        true /* clear the buffer on the first key */);

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewTextInput);
}

bool meshcore_scene_compose_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_COMPOSE_EVENT_SEND) {
        /* A fast double-press on the keyboard's save key can queue two SEND
         * events before the view switches away. Without this, the second would
         * overwrite app->worker — leaking the first thread and sending the
         * message twice. One send in flight at a time. */
        if(app->worker != NULL) return true;

        /* A node has to be connected; without a session there is nothing to
         * send through, and the worker would dereference NULL. */
        if(app->session == NULL || !meshcore_session_is_running(app->session)) {
            widget_reset(app->widget);
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "\e#Not connected\n"
                "Run Connect first, then\n"
                "write again.");
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
            return true;
        }

        widget_reset(app->widget);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "\e#Sending...");
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

        app->worker = furi_thread_alloc_ex(
            "MeshCoreCompose", MESHCORE_COMPOSE_WORKER_STACK, meshcore_scene_compose_worker, app);
        furi_thread_start(app->worker);
        return true;
    }

    if(event.event == MESHCORE_COMPOSE_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }

        if(app->worker_error == NULL) {
            /* Sent and recorded. Back to the conversation, which will render
             * the new line on its next tick. */
            scene_manager_previous_scene(app->scene_manager);
        } else {
            char text[128];
            snprintf(text, sizeof(text), "\e#Not sent\n%s\n\nBack to return.", app->worker_error);
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        }
        return true;
    }

    return false;
}

void meshcore_scene_compose_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    text_input_reset(app->text_input);
}
