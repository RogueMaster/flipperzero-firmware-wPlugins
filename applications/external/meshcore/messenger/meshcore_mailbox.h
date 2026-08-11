/*
 * Incoming mail.
 *
 * The companion protocol has no "incoming message" callback. The node sends a
 * MSG_WAITING push — literally "drain me" — and the client then loops
 * SYNC_NEXT_MESSAGE until the node answers NO_MORE_MESSAGES. So receiving is a
 * pull loop triggered by a push, and it needs its own thread: the push arrives
 * on the session worker, which must never block, and draining blocks.
 *
 * The worker also drains on a slow timer. That covers messages that were
 * already queued when we connected, and a push that went missing while the
 * UART was closed — without it, mail can sit on the node indefinitely.
 */
#pragma once

#include <furi.h>

#include "../protocol/meshcore_session.h"
#include "meshcore_messages.h"
#include "meshcore_msglog.h"

/** Something new landed in the store. Runs on the mailbox worker thread, so it
 *  must not block or touch a view — post a custom event instead. */
typedef void (*MeshCoreMailboxCallback)(void* context);

typedef struct MeshCoreMailbox MeshCoreMailbox;

MeshCoreMailbox* meshcore_mailbox_alloc(MeshCoreSession* session, MeshCoreMessages* store);
void meshcore_mailbox_free(MeshCoreMailbox* mailbox);

void meshcore_mailbox_set_callback(
    MeshCoreMailbox* mailbox,
    MeshCoreMailboxCallback callback,
    void* context);

/** Persist drained messages to the SD history. Optional: without it the mailbox
 *  still fills the RAM store, just not the durable copy. */
void meshcore_mailbox_set_msglog(MeshCoreMailbox* mailbox, MeshCoreMsgLog* msglog);

void meshcore_mailbox_start(MeshCoreMailbox* mailbox);
void meshcore_mailbox_stop(MeshCoreMailbox* mailbox);

/** Wake the worker. Safe from the session worker thread — it only sets a flag. */
void meshcore_mailbox_notify(MeshCoreMailbox* mailbox);

/** The store is written by the mailbox worker and read by the GUI thread, so
 *  hold this across any read of it. */
void meshcore_mailbox_lock(MeshCoreMailbox* mailbox);
void meshcore_mailbox_unlock(MeshCoreMailbox* mailbox);

/** How many drain passes have run. Useful for telling "no mail" apart from
 *  "never even asked". */
uint32_t meshcore_mailbox_drains(MeshCoreMailbox* mailbox);
