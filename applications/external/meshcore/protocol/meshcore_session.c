#include "meshcore_session.h"

/* How long one pump iteration waits for bytes. Bounds how quickly the worker
 * notices a stop request — and so how long meshcore_session_stop() blocks. */
#define MESHCORE_SESSION_SLICE_MS 50u
#define MESHCORE_SESSION_WORKER_STACK 2048u
#define MESHCORE_SESSION_REPLY_FLAG (1u << 0)

struct MeshCoreSession {
    MeshCoreLink link;
    MeshCoreLog* log;
    FuriHalSerialId serial_id;

    FuriThread* worker;
    volatile bool stop;
    bool running;

    /* One requester at a time: the node answers in order, so overlapping
     * requests could not be told apart anyway. */
    FuriMutex* request_lock;
    /* Guards the slot below, which the worker and the requester share. */
    FuriMutex* slot_lock;
    FuriEventFlag* flags;

    bool pending;
    bool got;
    MeshCoreCodeSet want;
    MeshCoreSessionStreamCallback collector;
    void* collector_context;
    mc_event_t result;

    MeshCoreSessionEventCallback event_callback;
    void* event_context;
};

static int32_t meshcore_session_worker(void* context) {
    MeshCoreSession* session = context;

    while(!session->stop) {
        mc_event_t event;
        if(!meshcore_link_poll(&session->link, &event, MESHCORE_SESSION_SLICE_MS)) continue;

        bool deliver_to_app = false;

        furi_mutex_acquire(session->slot_lock, FuriWaitForever);

        MeshCoreRoute route = meshcore_route_event(
            session->pending && !session->got,
            &session->want,
            session->collector != NULL,
            event.code);

        switch(route) {
        case MeshCoreRouteReply:
            session->result = event;
            session->got = true;
            break;

        case MeshCoreRouteStream:
            /* Called under the lock on purpose: the collector only copies a
             * record, and holding the lock means a requester that has just
             * timed out cannot pull the context out from under it. */
            if(!session->collector(&event, session->collector_context)) {
                deliver_to_app = true;
            }
            break;

        case MeshCoreRouteEvent:
            deliver_to_app = true;
            break;
        }

        bool signal_reply = (route == MeshCoreRouteReply);
        furi_mutex_release(session->slot_lock);

        if(signal_reply) {
            furi_event_flag_set(session->flags, MESHCORE_SESSION_REPLY_FLAG);
        } else if(deliver_to_app && session->event_callback) {
            /* Hand over the raw payload too: the frame may carry a code
             * meshcore_c does not decode, and the body is the only thing of
             * value in it. Valid until the next poll, hence synchronous. */
            size_t len = 0;
            const uint8_t* payload = meshcore_link_payload(&session->link, &len);
            session->event_callback(&event, payload, len, session->event_context);
        }
    }

    return 0;
}

MeshCoreSession* meshcore_session_alloc(MeshCoreLog* log, FuriHalSerialId serial_id) {
    MeshCoreSession* session = malloc(sizeof(MeshCoreSession));

    meshcore_link_init(&session->link);
    session->log = log;
    session->serial_id = serial_id;

    session->worker = NULL;
    session->stop = false;
    session->running = false;

    session->request_lock = furi_mutex_alloc(FuriMutexTypeNormal);
    session->slot_lock = furi_mutex_alloc(FuriMutexTypeNormal);
    session->flags = furi_event_flag_alloc();

    session->pending = false;
    session->got = false;
    session->want = meshcore_code_set_one(MESHCORE_LINK_NO_EVENT);
    session->collector = NULL;
    session->collector_context = NULL;
    memset(&session->result, 0, sizeof(session->result));

    session->event_callback = NULL;
    session->event_context = NULL;

    return session;
}

void meshcore_session_free(MeshCoreSession* session) {
    furi_assert(session);

    meshcore_session_stop(session);

    furi_event_flag_free(session->flags);
    furi_mutex_free(session->slot_lock);
    furi_mutex_free(session->request_lock);
    free(session);
}

bool meshcore_session_start(MeshCoreSession* session) {
    furi_assert(session);
    if(session->running) return true;

    if(!meshcore_link_open(&session->link, session->log, session->serial_id)) return false;

    session->stop = false;
    session->worker = furi_thread_alloc_ex(
        "MeshCoreSession", MESHCORE_SESSION_WORKER_STACK, meshcore_session_worker, session);
    furi_thread_start(session->worker);
    session->running = true;

    return true;
}

void meshcore_session_stop(MeshCoreSession* session) {
    furi_assert(session);
    if(!session->running) return;

    session->stop = true;
    /* Cleared here, before the join, so a requester on another thread bails at
     * its early running-check instead of arming a fresh request while we are
     * tearing down. One already past that check (parked in the wait below) is
     * handled by the flag set. */
    session->running = false;

    furi_thread_join(session->worker);
    furi_thread_free(session->worker);
    session->worker = NULL;

    /* Wake any requester parked waiting for a reply the worker — now joined —
     * will never deliver. Without this it would sit out the full link timeout
     * (~1.2s) holding request_lock, and the handoff below would wait on it that
     * whole time. The woken requester finds got==false and returns false. A set
     * with no waiter is harmless: the next exchange clears the flag before
     * arming. */
    furi_event_flag_set(session->flags, MESHCORE_SESSION_REPLY_FLAG);

    /* Take request_lock before closing so a requester that was already in
     * flight (past its checks, holding the lock) cannot be mid-send on the link
     * as it goes away. The worker is already joined and never held this lock,
     * so acquiring it here cannot deadlock. */
    furi_mutex_acquire(session->request_lock, FuriWaitForever);
    meshcore_link_close(&session->link);
    furi_mutex_release(session->request_lock);
}

bool meshcore_session_is_running(MeshCoreSession* session) {
    furi_assert(session);
    return session->running;
}

void meshcore_session_set_event_callback(
    MeshCoreSession* session,
    MeshCoreSessionEventCallback callback,
    void* context) {
    furi_assert(session);
    session->event_callback = callback;
    session->event_context = context;
}

static bool meshcore_session_exchange(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    const MeshCoreCodeSet* want,
    MeshCoreSessionStreamCallback collector,
    void* collector_context,
    mc_event_t* event,
    uint32_t timeout_ms) {
    furi_assert(session);

    if(event) {
        memset(event, 0, sizeof(*event));
        event->code = MESHCORE_LINK_NO_EVENT;
    }
    if(!session->running) return false;

    furi_mutex_acquire(session->request_lock, FuriWaitForever);

    /* Re-check under the lock: meshcore_session_stop() takes this same lock
     * before closing the link, so a stop that raced past the check above cannot
     * now close the link while this send is in flight. Without it a caller on
     * another thread (the mailbox) could write to a just-closed handle when the
     * logger hands the UART over. */
    if(!session->running) {
        furi_mutex_release(session->request_lock);
        return false;
    }

    /* Clear before arming: a reply to a request we already gave up on must not
     * satisfy this one. */
    furi_event_flag_clear(session->flags, MESHCORE_SESSION_REPLY_FLAG);

    furi_mutex_acquire(session->slot_lock, FuriWaitForever);
    session->pending = true;
    session->got = false;
    session->want = *want;
    session->collector = collector;
    session->collector_context = collector_context;
    furi_mutex_release(session->slot_lock);

    if(meshcore_link_send(&session->link, payload, len)) {
        furi_event_flag_wait(
            session->flags,
            MESHCORE_SESSION_REPLY_FLAG,
            FuriFlagWaitAny,
            furi_ms_to_ticks(timeout_ms));
    }

    bool answered = false;
    furi_mutex_acquire(session->slot_lock, FuriWaitForever);
    if(session->got) {
        if(event) *event = session->result;
        /* Claimed, but an ERR that we were not waiting for is still a failure
         * from the caller's point of view — it just gets to see the code. */
        answered = meshcore_code_set_has(want, session->result.code);
    }
    session->pending = false;
    session->got = false;
    session->collector = NULL;
    session->collector_context = NULL;
    furi_mutex_release(session->slot_lock);

    furi_mutex_release(session->request_lock);
    return answered;
}

bool meshcore_session_request(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    uint8_t want_code,
    mc_event_t* event,
    uint32_t timeout_ms) {
    MeshCoreCodeSet want = meshcore_code_set_one(want_code);
    return meshcore_session_exchange(session, payload, len, &want, NULL, NULL, event, timeout_ms);
}

bool meshcore_session_request_any(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    const MeshCoreCodeSet* want,
    mc_event_t* event,
    uint32_t timeout_ms) {
    return meshcore_session_exchange(session, payload, len, want, NULL, NULL, event, timeout_ms);
}

bool meshcore_session_request_stream(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    uint8_t want_code,
    MeshCoreSessionStreamCallback collector,
    void* collector_context,
    mc_event_t* event,
    uint32_t timeout_ms) {
    furi_assert(collector);
    MeshCoreCodeSet want = meshcore_code_set_one(want_code);
    return meshcore_session_exchange(
        session, payload, len, &want, collector, collector_context, event, timeout_ms);
}

uint32_t meshcore_session_rx_errors(MeshCoreSession* session) {
    furi_assert(session);
    return meshcore_link_rx_errors(&session->link);
}
