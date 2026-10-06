#include "pod_wire.h"

#include <string.h>

// CRC-8, poly 0x07 — matches protocol.md.
uint8_t pod_wire_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static size_t put_u32(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
    return 4;
}

static uint32_t get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static size_t name_len(const char* name) {
    size_t n = 0;
    while(n < POD_NAME_MAX && name[n])
        n++;
    return n;
}

// --- encoding -------------------------------------------------------------

size_t pod_wire_beacon(
    uint8_t* buf,
    size_t cap,
    uint32_t src,
    uint8_t icon,
    uint8_t level,
    uint16_t nonce,
    const char* name) {
    size_t nl = name_len(name);
    if(cap < 13 + nl) return 0;

    size_t i = 0;
    buf[i++] = 'P';
    buf[i++] = 'D';
    buf[i++] = PodMsgBeacon;
    i += put_u32(&buf[i], src);
    buf[i++] = icon;
    buf[i++] = level;
    buf[i++] = nonce & 0xFF;
    buf[i++] = (nonce >> 8) & 0xFF;
    buf[i++] = (uint8_t)nl;
    memcpy(&buf[i], name, nl);
    i += nl;
    buf[i] = pod_wire_crc8(buf, i);
    return i + 1;
}

size_t pod_wire_named(
    uint8_t* buf,
    size_t cap,
    uint8_t type,
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id) {
    size_t nl = name_len(name);
    if(cap < 16 + nl) return 0;

    size_t i = 0;
    buf[i++] = 'P';
    buf[i++] = 'D';
    buf[i++] = type;
    i += put_u32(&buf[i], src);
    i += put_u32(&buf[i], dst);
    buf[i++] = level;
    buf[i++] = battle_id & 0xFF;
    buf[i++] = (battle_id >> 8) & 0xFF;
    buf[i++] = (uint8_t)nl;
    memcpy(&buf[i], name, nl);
    i += nl;
    buf[i] = pod_wire_crc8(buf, i);
    return i + 1;
}

size_t pod_wire_decline(uint8_t* buf, size_t cap, uint32_t src, uint32_t dst, uint16_t battle_id) {
    if(cap < 14) return 0;

    size_t i = 0;
    buf[i++] = 'P';
    buf[i++] = 'D';
    buf[i++] = PodMsgDecline;
    i += put_u32(&buf[i], src);
    i += put_u32(&buf[i], dst);
    buf[i++] = battle_id & 0xFF;
    buf[i++] = (battle_id >> 8) & 0xFF;
    buf[i] = pod_wire_crc8(buf, i);
    return i + 1;
}

size_t pod_wire_taps(
    uint8_t* buf,
    size_t cap,
    uint32_t src,
    uint32_t dst,
    uint16_t battle_id,
    uint16_t taps) {
    if(cap < 16) return 0;

    size_t i = 0;
    buf[i++] = 'P';
    buf[i++] = 'D';
    buf[i++] = PodMsgTaps;
    i += put_u32(&buf[i], src);
    i += put_u32(&buf[i], dst);
    buf[i++] = battle_id & 0xFF;
    buf[i++] = (battle_id >> 8) & 0xFF;
    buf[i++] = taps & 0xFF;
    buf[i++] = (taps >> 8) & 0xFF;
    buf[i] = pod_wire_crc8(buf, i);
    return i + 1;
}

// --- receiving ------------------------------------------------------------

void pod_wire_reset(PodWire* w) {
    w->len = 0;
    memset(w->recent, 0, sizeof(w->recent));
    w->recent_pos = 0;
}

static bool pod_wire_seen(PodWire* w, uint32_t src, uint8_t type, uint16_t key2) {
    for(int k = 0; k < POD_DEDUP_N; k++) {
        if(w->recent[k].src == src && w->recent[k].type == type && w->recent[k].key2 == key2)
            return true;
    }
    w->recent[w->recent_pos].src = src;
    w->recent[w->recent_pos].type = type;
    w->recent[w->recent_pos].key2 = key2;
    w->recent_pos = (w->recent_pos + 1) % POD_DEDUP_N;
    return false;
}

// Total on-wire length of the message starting at p (len bytes available), or 0
// if incomplete. SIZE_MAX means the bytes cannot start a valid message, so the
// caller should resynchronise.
static size_t pod_msg_len(const uint8_t* p, size_t len) {
    if(len < 3) return 0;
    switch(p[2]) {
    case PodMsgBeacon:
        if(len < 12) return 0;
        if(p[11] > POD_NAME_MAX) return SIZE_MAX;
        return 12 + p[11] + 1;
    case PodMsgChallenge:
    case PodMsgAccept:
        if(len < 15) return 0;
        if(p[14] > POD_NAME_MAX) return SIZE_MAX;
        return 15 + p[14] + 1;
    case PodMsgDecline:
        return 14;
    case PodMsgTaps:
        return 16;
    default:
        return SIZE_MAX;
    }
}

static void pod_msg_decode(const uint8_t* p, PodMsg* m) {
    memset(m, 0, sizeof(*m));
    m->type = p[2];
    m->src_id = get_u32(&p[3]);
    switch(p[2]) {
    case PodMsgBeacon: {
        m->icon = p[7];
        m->level = p[8];
        m->nonce = (uint16_t)(p[9] | (p[10] << 8));
        uint8_t nl = p[11];
        memcpy(m->name, &p[12], nl);
        m->name[nl] = '\0';
        break;
    }
    case PodMsgChallenge:
    case PodMsgAccept: {
        m->dst_id = get_u32(&p[7]);
        m->level = p[11];
        m->battle_id = (uint16_t)(p[12] | (p[13] << 8));
        uint8_t nl = p[14];
        memcpy(m->name, &p[15], nl);
        m->name[nl] = '\0';
        break;
    }
    case PodMsgDecline:
        m->dst_id = get_u32(&p[7]);
        m->battle_id = (uint16_t)(p[11] | (p[12] << 8));
        break;
    case PodMsgTaps:
        m->dst_id = get_u32(&p[7]);
        m->battle_id = (uint16_t)(p[11] | (p[12] << 8));
        m->taps = (uint16_t)(p[13] | (p[14] << 8));
        break;
    default:
        break;
    }
}

void pod_wire_feed(PodWire* w, const uint8_t* data, size_t len, PodMsgCallback cb, void* context) {
    // Keep the newest bytes if more arrive than the reassembly buffer holds.
    if(len >= POD_PARSE_BUF) {
        data += len - (POD_PARSE_BUF - 1);
        len = POD_PARSE_BUF - 1;
        w->len = 0;
    }
    if(w->len + len > POD_PARSE_BUF) {
        size_t drop = w->len + len - POD_PARSE_BUF;
        memmove(w->buf, w->buf + drop, w->len - drop);
        w->len -= drop;
    }
    memcpy(w->buf + w->len, data, len);
    w->len += len;

    size_t i = 0;
    while(w->len - i >= 3) {
        uint8_t* p = &w->buf[i];
        if(!(p[0] == 'P' && p[1] == 'D')) {
            i++;
            continue;
        }
        size_t total = pod_msg_len(p, w->len - i);
        if(total == 0) break; // need more bytes
        if(total == SIZE_MAX) {
            i++;
            continue; // cannot be a message, resync
        }
        // The header's own length field can describe a frame longer than what
        // has arrived so far; without this the CRC check reads past the buffer.
        if(total > w->len - i) break;
        if(pod_wire_crc8(p, total - 1) != p[total - 1]) {
            i++;
            continue; // corrupted, resync
        }

        PodMsg m;
        pod_msg_decode(p, &m);
        i += total;

        uint16_t key2 = (m.type == PodMsgBeacon) ? m.nonce : m.battle_id;
        if(!pod_wire_seen(w, m.src_id, m.type, key2) && cb) {
            cb(context, &m);
        }
    }

    if(i > 0) {
        memmove(w->buf, w->buf + i, w->len - i);
        w->len -= i;
    }
}
