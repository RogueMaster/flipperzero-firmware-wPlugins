/*
 * meshcore_msglog — the durable copy of the chat, on the SD card.
 *
 * The in-RAM store (meshcore_messages) is a small ring that is lost when the
 * app exits; this appends every message to a file so the history survives a
 * restart. It is append-only ("keep everything") — the file grows, and on
 * startup the tail is read back into the ring so the chat opens where it left
 * off. One message is one line; the encoding lives in meshcore_messages so it
 * stays host-tested.
 *
 * The node holds mail for us while we are away (it is what a message reaches,
 * not this app), so this is about not losing what we have already pulled — a
 * message drained to the Flipper and then only kept in RAM would vanish on the
 * next launch, which is exactly what the user does not want without a room
 * server on the mesh.
 */
#pragma once

#include "meshcore_messages.h"

typedef struct MeshCoreMsgLog MeshCoreMsgLog;

/** Open the history file's storage. Never returns NULL: if the card is missing
 *  the log simply no-ops, so the messenger still works without persistence. */
MeshCoreMsgLog* meshcore_msglog_alloc(void);
void meshcore_msglog_free(MeshCoreMsgLog* log);

/** Read the saved history into `store` (oldest first; the ring keeps the most
 *  recent). Call once at startup, before the mailbox worker runs. */
void meshcore_msglog_load(MeshCoreMsgLog* log, MeshCoreMessages* store);

/** Append one message to the file. Safe to call from the worker threads; a
 *  failed write is dropped rather than blocking the chat. */
void meshcore_msglog_append(MeshCoreMsgLog* log, const MeshCoreMessage* message);
