#include "meshcore_mailbox.h"

#define MESHCORE_MAILBOX_WAKE_FLAG (1u << 0)
#define MESHCORE_MAILBOX_WORKER_STACK 2048u

/* Safety-net period. Long enough to be free when idle, short enough that a
 * missed push does not leave mail sitting on the node for minutes. */
#define MESHCORE_MAILBOX_IDLE_MS 5000u

/* Messages pulled per wake. A node with a large backlog gets drained over
 * several passes rather than monopolising the link in one. */
#define MESHCORE_MAILBOX_BATCH 16u

struct MeshCoreMailbox {
    MeshCoreSession* session;
    MeshCoreMessages* store;
    MeshCoreMsgLog* msglog;

    FuriThread* worker;
    FuriEventFlag* flags;
    FuriMutex* store_lock;
    volatile bool stop;
    bool running;
    volatile uint32_t drains;

    MeshCoreMailboxCallback callback;
    void* context;
};

/* Every code the node may answer SYNC_NEXT_MESSAGE with. The V3 variants carry
 * SNR and RSSI; meshcore_c normalises both into the same struct. */
static MeshCoreCodeSet meshcore_mailbox_reply_codes(void) {
    static const uint8_t codes[] = {
        MC_RESP_CONTACT_MSG_RECV,
        MC_RESP_CONTACT_MSG_RECV_V3,
        MC_RESP_CHANNEL_MSG_RECV,
        MC_RESP_CHANNEL_MSG_RECV_V3,
        MC_RESP_NO_MORE_MESSAGES,
    };
    return meshcore_code_set(codes, (uint8_t)(sizeof(codes) / sizeof(codes[0])));
}

/** Pull until the node says there is nothing left. Returns true if anything
 *  was stored. */
static bool meshcore_mailbox_drain(MeshCoreMailbox* mailbox) {
    if(!meshcore_session_is_running(mailbox->session)) return false;

    const MeshCoreCodeSet want = meshcore_mailbox_reply_codes();
    bool stored_any = false;

    mailbox->drains++;

    for(uint32_t i = 0; i < MESHCORE_MAILBOX_BATCH; i++) {
        if(mailbox->stop) break;

        uint8_t payload[MC_MAX_PAYLOAD];
        size_t len = mc_cmd_sync_next_message(payload, sizeof(payload));
        if(len == 0) break;

        mc_event_t event;
        if(!meshcore_session_request_any(
               mailbox->session, payload, len, &want, &event, MESHCORE_LINK_TIMEOUT_MS)) {
            /* Silence or a node error: stop pulling and let the next wake try
             * again rather than hammering a node that is not answering. */
            break;
        }

        if(event.code == MC_RESP_NO_MORE_MESSAGES) break;

        MeshCoreMessage message;
        if(meshcore_message_from_event(&event, &message)) {
            furi_mutex_acquire(mailbox->store_lock, FuriWaitForever);
            meshcore_messages_add(mailbox->store, &message);
            furi_mutex_release(mailbox->store_lock);
            /* Persist outside the lock: SD latency must not stall a chat read.
             * The message is a local copy, so this is safe to touch unlocked. */
            if(mailbox->msglog) meshcore_msglog_append(mailbox->msglog, &message);
            stored_any = true;
        }

        /* Still more to come: the node hands over one message per command. */
    }

    return stored_any;
}

static int32_t meshcore_mailbox_worker(void* context) {
    MeshCoreMailbox* mailbox = context;

    while(!mailbox->stop) {
        /* Either a push woke us, or the timeout did — both mean "go look". */
        furi_event_flag_wait(
            mailbox->flags,
            MESHCORE_MAILBOX_WAKE_FLAG,
            FuriFlagWaitAny,
            furi_ms_to_ticks(MESHCORE_MAILBOX_IDLE_MS));

        if(mailbox->stop) break;

        if(meshcore_mailbox_drain(mailbox) && mailbox->callback) {
            mailbox->callback(mailbox->context);
        }
    }

    return 0;
}

MeshCoreMailbox* meshcore_mailbox_alloc(MeshCoreSession* session, MeshCoreMessages* store) {
    MeshCoreMailbox* mailbox = malloc(sizeof(MeshCoreMailbox));

    mailbox->session = session;
    mailbox->store = store;
    mailbox->msglog = NULL;
    mailbox->worker = NULL;
    mailbox->flags = furi_event_flag_alloc();
    mailbox->store_lock = furi_mutex_alloc(FuriMutexTypeNormal);
    mailbox->stop = false;
    mailbox->running = false;
    mailbox->drains = 0;
    mailbox->callback = NULL;
    mailbox->context = NULL;

    return mailbox;
}

void meshcore_mailbox_free(MeshCoreMailbox* mailbox) {
    furi_assert(mailbox);

    meshcore_mailbox_stop(mailbox);
    furi_mutex_free(mailbox->store_lock);
    furi_event_flag_free(mailbox->flags);
    free(mailbox);
}

void meshcore_mailbox_set_callback(
    MeshCoreMailbox* mailbox,
    MeshCoreMailboxCallback callback,
    void* context) {
    furi_assert(mailbox);
    mailbox->callback = callback;
    mailbox->context = context;
}

void meshcore_mailbox_set_msglog(MeshCoreMailbox* mailbox, MeshCoreMsgLog* msglog) {
    furi_assert(mailbox);
    mailbox->msglog = msglog;
}

void meshcore_mailbox_start(MeshCoreMailbox* mailbox) {
    furi_assert(mailbox);
    if(mailbox->running) return;

    mailbox->stop = false;
    mailbox->worker = furi_thread_alloc_ex(
        "MeshCoreMailbox", MESHCORE_MAILBOX_WORKER_STACK, meshcore_mailbox_worker, mailbox);
    furi_thread_start(mailbox->worker);
    mailbox->running = true;
}

void meshcore_mailbox_stop(MeshCoreMailbox* mailbox) {
    furi_assert(mailbox);
    if(!mailbox->running) return;

    mailbox->stop = true;
    /* Poke it so it does not sit out the whole idle period before noticing. */
    furi_event_flag_set(mailbox->flags, MESHCORE_MAILBOX_WAKE_FLAG);

    furi_thread_join(mailbox->worker);
    furi_thread_free(mailbox->worker);
    mailbox->worker = NULL;
    mailbox->running = false;
}

void meshcore_mailbox_notify(MeshCoreMailbox* mailbox) {
    furi_assert(mailbox);
    furi_event_flag_set(mailbox->flags, MESHCORE_MAILBOX_WAKE_FLAG);
}

void meshcore_mailbox_lock(MeshCoreMailbox* mailbox) {
    furi_mutex_acquire(mailbox->store_lock, FuriWaitForever);
}

void meshcore_mailbox_unlock(MeshCoreMailbox* mailbox) {
    furi_mutex_release(mailbox->store_lock);
}

uint32_t meshcore_mailbox_drains(MeshCoreMailbox* mailbox) {
    furi_assert(mailbox);
    return mailbox->drains;
}
