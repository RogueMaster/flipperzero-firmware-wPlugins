/*
 * Recent messages, in RAM.
 *
 * A ring: once it is full the oldest entry goes. That is deliberate — the
 * durable copy belongs on the SD card (stage 4), and a Flipper has no business
 * holding a whole mesh's traffic in memory.
 *
 * Conversations are keyed by the first 6 bytes of the peer's public key,
 * because that is all an incoming message carries (`mc_contact_msg_t` has
 * `pubkey_prefix[6]`, not the full key). Matching a message to a contact means
 * comparing those 6 bytes against the contact's key.
 *
 * Free of furi so the host tests can cover the ring, the peer matching and the
 * event conversion.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/meshcore_c/meshcore_companion.h"

#define MESHCORE_MESSAGES_MAX 48
#define MESHCORE_PEER_LEN 6

typedef enum {
    MeshCoreMessageIncoming,
    MeshCoreMessageOutgoing,
} MeshCoreMessageDirection;

typedef struct {
    uint8_t peer[MESHCORE_PEER_LEN]; /* public key prefix; zeroed for channels */
    bool is_channel;
    uint8_t channel_idx;
    MeshCoreMessageDirection direction;
    uint32_t timestamp; /* sender_ts, in the node's timebase */
    int8_t snr_q4; /* MC_SNR_NONE when the frame carried none */
    uint8_t path_len; /* MC_PATH_DIRECT, or the flood hop count */
    char text[MC_MAX_TEXT];
} MeshCoreMessage;

typedef struct {
    MeshCoreMessage items[MESHCORE_MESSAGES_MAX];
    size_t count; /* how many are held, at most MESHCORE_MESSAGES_MAX */
    size_t head; /* ring index of the oldest held message */
    uint32_t total; /* how many have ever been added; a change means "new mail" */
} MeshCoreMessages;

void meshcore_messages_reset(MeshCoreMessages* messages);

void meshcore_messages_add(MeshCoreMessages* messages, const MeshCoreMessage* message);

/** Oldest first. NULL when `index` is past the end. */
const MeshCoreMessage* meshcore_messages_at(const MeshCoreMessages* messages, size_t index);

/** Turn a decoded event into a message. False if it was not a message at all. */
bool meshcore_message_from_event(const mc_event_t* event, MeshCoreMessage* out);

/** Serialize one message to a single line (no trailing newline) for the durable
 *  SD history. Returns the length written, or 0 if it would not fit `cap`.
 *  Newlines in the text are flattened to spaces so one message is one line. */
size_t meshcore_message_encode(const MeshCoreMessage* message, char* out, size_t cap);

/** Parse one such line back into a message. False if the line is malformed. */
bool meshcore_message_decode(const char* line, MeshCoreMessage* out);

/** Does this message belong to the conversation with `public_key`? Compares
 *  the 6-byte prefix, which is all an incoming message identifies a peer by. */
bool meshcore_message_is_from(const MeshCoreMessage* message, const uint8_t* public_key);

size_t meshcore_messages_count_for(const MeshCoreMessages* messages, const uint8_t* public_key);
