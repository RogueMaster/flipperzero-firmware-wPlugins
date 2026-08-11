#include "meshcore_messages.h"

#include <stdio.h>
#include <string.h>

void meshcore_messages_reset(MeshCoreMessages* messages) {
    memset(messages, 0, sizeof(*messages));
}

void meshcore_messages_add(MeshCoreMessages* messages, const MeshCoreMessage* message) {
    size_t slot;

    if(messages->count < MESHCORE_MESSAGES_MAX) {
        slot = (messages->head + messages->count) % MESHCORE_MESSAGES_MAX;
        messages->count++;
    } else {
        /* Full: overwrite the oldest and move the window along. */
        slot = messages->head;
        messages->head = (messages->head + 1) % MESHCORE_MESSAGES_MAX;
    }

    messages->items[slot] = *message;
    messages->total++;
}

const MeshCoreMessage* meshcore_messages_at(const MeshCoreMessages* messages, size_t index) {
    if(index >= messages->count) return NULL;
    return &messages->items[(messages->head + index) % MESHCORE_MESSAGES_MAX];
}

static int meshcore_msg_hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t meshcore_message_encode(const MeshCoreMessage* message, char* out, size_t cap) {
    static const char* hexd = "0123456789abcdef";
    char peer[2 * MESHCORE_PEER_LEN + 1];
    for(size_t i = 0; i < MESHCORE_PEER_LEN; i++) {
        peer[i * 2] = hexd[message->peer[i] >> 4];
        peer[i * 2 + 1] = hexd[message->peer[i] & 0x0F];
    }
    peer[2 * MESHCORE_PEER_LEN] = '\0';

    /* One message must be one line: flatten any newline in the text. */
    char safe[MC_MAX_TEXT];
    size_t j = 0;
    for(size_t i = 0; message->text[i] != '\0' && j + 1 < sizeof(safe); i++) {
        char c = message->text[i];
        safe[j++] = (c == '\n' || c == '\r') ? ' ' : c;
    }
    safe[j] = '\0';

    int n = snprintf(
        out,
        cap,
        "%u,%u,%u,%s,%lu,%d,%u,%s",
        (unsigned)message->direction,
        message->is_channel ? 1u : 0u,
        (unsigned)message->channel_idx,
        peer,
        (unsigned long)message->timestamp,
        (int)message->snr_q4,
        (unsigned)message->path_len,
        safe);
    if(n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

bool meshcore_message_decode(const char* line, MeshCoreMessage* out) {
    memset(out, 0, sizeof(*out));

    unsigned dir = 0, chan = 0, idx = 0, path = 0;
    unsigned long ts = 0;
    int snr = 0;
    char peer[2 * MESHCORE_PEER_LEN + 1] = {0};
    int consumed = 0;

    int matched = sscanf(
        line, "%u,%u,%u,%12[0-9a-fA-F],%lu,%d,%u,%n", &dir, &chan, &idx, peer, &ts, &snr, &path,
        &consumed);
    if(matched < 7 || consumed == 0) return false;
    if(strlen(peer) != 2 * MESHCORE_PEER_LEN) return false;

    for(size_t i = 0; i < MESHCORE_PEER_LEN; i++) {
        int hi = meshcore_msg_hexval(peer[i * 2]);
        int lo = meshcore_msg_hexval(peer[i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        out->peer[i] = (uint8_t)((hi << 4) | lo);
    }

    out->direction =
        (dir == MeshCoreMessageOutgoing) ? MeshCoreMessageOutgoing : MeshCoreMessageIncoming;
    out->is_channel = chan != 0;
    out->channel_idx = (uint8_t)idx;
    out->timestamp = (uint32_t)ts;
    out->snr_q4 = (int8_t)snr;
    out->path_len = (uint8_t)path;
    snprintf(out->text, sizeof(out->text), "%s", line + consumed);
    return true;
}

bool meshcore_message_from_event(const mc_event_t* event, MeshCoreMessage* out) {
    memset(out, 0, sizeof(*out));
    out->direction = MeshCoreMessageIncoming;

    switch(event->code) {
    /* V3 differs only by carrying SNR and RSSI; the library has already
     * normalised that into the same struct. */
    case MC_RESP_CONTACT_MSG_RECV:
    case MC_RESP_CONTACT_MSG_RECV_V3: {
        const mc_contact_msg_t* msg = &event->u.contact_msg;
        memcpy(out->peer, msg->pubkey_prefix, MESHCORE_PEER_LEN);
        out->is_channel = false;
        out->timestamp = msg->sender_ts;
        out->snr_q4 = msg->snr_q4;
        out->path_len = msg->path_len;
        snprintf(out->text, sizeof(out->text), "%s", msg->text);
        return true;
    }

    case MC_RESP_CHANNEL_MSG_RECV:
    case MC_RESP_CHANNEL_MSG_RECV_V3: {
        const mc_channel_msg_t* msg = &event->u.channel_msg;
        out->is_channel = true;
        /* channel_idx is a slot number 0-7 on the wire (parsed into an int8_t). */
        out->channel_idx = (uint8_t)msg->channel_idx;
        out->timestamp = msg->sender_ts;
        out->snr_q4 = msg->snr_q4;
        out->path_len = msg->path_len;
        /* For channels the text already reads "Sender: body". */
        snprintf(out->text, sizeof(out->text), "%s", msg->text);
        return true;
    }

    default:
        return false;
    }
}

bool meshcore_message_is_from(const MeshCoreMessage* message, const uint8_t* public_key) {
    if(message->is_channel) return false;
    return memcmp(message->peer, public_key, MESHCORE_PEER_LEN) == 0;
}

size_t meshcore_messages_count_for(const MeshCoreMessages* messages, const uint8_t* public_key) {
    size_t found = 0;
    for(size_t i = 0; i < messages->count; i++) {
        const MeshCoreMessage* message = meshcore_messages_at(messages, i);
        if(meshcore_message_is_from(message, public_key)) found++;
    }
    return found;
}
