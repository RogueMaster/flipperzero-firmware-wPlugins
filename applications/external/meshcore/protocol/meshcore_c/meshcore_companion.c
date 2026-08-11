/*
 * meshcore_companion.c  -- portable C99 core, no I/O, no malloc.
 * SPDX-License-Identifier: MIT
 */
#include "meshcore_companion.h"
#include <string.h>

/* ---- little-endian helpers ---- */
static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;       p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Copy a fixed-width NUL-padded cstring field of `field_len` into dst (cap incl
 * terminator). Advances nothing; returns field_len consumed by caller. */
static void copy_cstring(char *dst, size_t dst_cap, const uint8_t *src, size_t field_len) {
    size_t i = 0;
    for (; i < field_len && i + 1 < dst_cap; i++) {
        if (src[i] == 0) break;
        dst[i] = (char)src[i];
    }
    dst[i] = 0;
}

/* Copy remaining bytes [off..len) as a NUL-terminated string. */
static void copy_rest_string(char *dst, size_t dst_cap, const uint8_t *src, size_t off, size_t len) {
    size_t n = (off < len) ? (len - off) : 0, i = 0;
    for (; i < n && i + 1 < dst_cap; i++) dst[i] = (char)src[off + i];
    dst[i] = 0;
}

/* Channel text message body parser, shared by the base (code 8) and V3 (code
 * 17) forms. V3 prefixes the body with [snr:i8][2 reserved]; base has no SNR. */
static int parse_channel_text(const uint8_t *b, size_t n, int v3, mc_channel_msg_t *m) {
    size_t lead = v3 ? 3u : 0u;
    if (n < lead + 7) return 0;
    const uint8_t *q = b + lead;
    size_t qn = n - lead;
    m->channel_idx = (int8_t)q[0];
    m->path_len    = q[1];
    m->txt_type    = q[2];
    m->sender_ts   = get_u32(q + 3);
    m->snr_q4      = v3 ? (int8_t)b[0] : MC_SNR_NONE;
    m->rssi        = v3 ? (int8_t)b[1] : MC_RSSI_NONE;   /* V3 lead: [snr][rssi][?] */
    copy_rest_string(m->text, sizeof(m->text), q, 7, qn);
    return 1;
}

/* Contact text message body parser, shared by the base (code 7) and V3 (code
 * 16) forms. A signature (4 bytes) precedes the text when txt_type is
 * MC_TXT_SIGNED_PLAIN; the base form previously mis-read this as text. */
static int parse_contact_text(const uint8_t *b, size_t n, int v3, mc_contact_msg_t *m) {
    size_t lead = v3 ? 3u : 0u;
    if (n < lead + 12) return 0;
    const uint8_t *q = b + lead;
    size_t qn = n - lead;
    memcpy(m->pubkey_prefix, q, 6);
    m->path_len  = q[6];
    m->txt_type  = q[7];
    m->sender_ts = get_u32(q + 8);
    m->snr_q4    = v3 ? (int8_t)b[0] : MC_SNR_NONE;
    m->rssi      = v3 ? (int8_t)b[1] : MC_RSSI_NONE;   /* V3 lead: [snr][rssi][?] */
    size_t toff = 12;
    if (m->txt_type == MC_TXT_SIGNED_PLAIN && qn >= toff + 4) {
        memcpy(m->signature, q + toff, 4);
        m->has_signature = 1;
        toff += 4;
    }
    copy_rest_string(m->text, sizeof(m->text), q, toff, qn);
    return 1;
}

/* Contact record, shared by CONTACT (3) and NEW_ADVERT (0x8A). */
static int parse_contact(const uint8_t *b, size_t n, mc_contact_t *c) {
    /* pubkey32 + type + flags + path_len + path64 + name32 + 3*u32 + u32 */
    if (n < 32 + 1 + 1 + 1 + 64 + 32 + 4 + 4 + 4) return 0;
    size_t o = 0;
    memcpy(c->public_key, b + o, 32); o += 32;
    c->type         = b[o++];
    c->flags        = b[o++];
    c->out_path_len = b[o++];
    memcpy(c->out_path, b + o, 64); o += 64;
    copy_cstring(c->adv_name, sizeof(c->adv_name), b + o, 32); o += 32;
    c->last_advert = get_u32(b + o); o += 4;
    c->adv_lat = (int32_t)get_u32(b + o); o += 4;
    c->adv_lon = (int32_t)get_u32(b + o); o += 4;
    c->lastmod = get_u32(b + o); o += 4;
    return 1;
}

/* ======================================================================== */
void mc_rx_init(mc_rx_t *rx) { rx->len = 0; }

size_t mc_rx_feed(mc_rx_t *rx, const uint8_t *data, size_t n) {
    size_t space = sizeof(rx->buf) - rx->len, take = (n < space) ? n : space;
    memcpy(rx->buf + rx->len, data, take);
    rx->len += take;
    return take;
}

static void rx_drop_front(mc_rx_t *rx, size_t k) {
    if (k >= rx->len) { rx->len = 0; return; }
    memmove(rx->buf, rx->buf + k, rx->len - k);
    rx->len -= k;
}

int mc_rx_poll(mc_rx_t *rx, uint8_t *out, size_t out_cap, size_t *out_len) {
    while (rx->len >= 3) {
        uint8_t type = rx->buf[0];
        if (type != MC_FRAME_RADIO_TO_APP && type != MC_FRAME_APP_TO_RADIO) {
            rx_drop_front(rx, 1);            /* not a frame lead, resync */
            continue;
        }
        uint16_t flen = get_u16(rx->buf + 1);
        if (flen == 0) { rx_drop_front(rx, 1); continue; }
        if (flen > MC_MAX_PAYLOAD) {         /* cannot hold it; skip lead byte */
            rx_drop_front(rx, 1);
            continue;
        }
        size_t need = (size_t)3 + flen;
        if (rx->len < need) return 0;        /* wait for the rest */
        size_t copy = (flen < out_cap) ? flen : out_cap;
        memcpy(out, rx->buf + 3, copy);
        *out_len = copy;
        rx_drop_front(rx, need);
        return 1;
    }
    return 0;
}

/* ======================================================================== */
size_t mc_frame_encode(const uint8_t *payload, size_t payload_len,
                       uint8_t *out, size_t out_cap) {
    if (payload_len > 0xFFFF || out_cap < payload_len + 3) return 0;
    out[0] = MC_FRAME_APP_TO_RADIO;
    put_u16(out + 1, (uint16_t)payload_len);
    memcpy(out + 3, payload, payload_len);
    return payload_len + 3;
}

/* ---- command builders ---- */
size_t mc_cmd_app_start(uint8_t *out, size_t cap, const char *app_name) {
    size_t nlen = app_name ? strlen(app_name) : 0;
    size_t total = 1 + 1 + 6 + nlen;
    if (cap < total) return 0;
    size_t i = 0;
    out[i++] = MC_CMD_APP_START;
    out[i++] = 3;                       /* app version (3 = accept V3 frames with SNR+RSSI) */
    memset(out + i, 0, 6); i += 6;      /* reserved */
    memcpy(out + i, app_name, nlen); i += nlen;
    return i;
}

size_t mc_cmd_device_query(uint8_t *out, size_t cap, uint8_t app_target_ver) {
    if (cap < 2) return 0;
    out[0] = MC_CMD_DEVICE_QUERY; out[1] = app_target_ver; return 2;
}

size_t mc_cmd_get_device_time(uint8_t *out, size_t cap) {
    if (cap < 1) return 0;
    out[0] = MC_CMD_GET_DEVICE_TIME;
    return 1;
}

size_t mc_cmd_set_device_time(uint8_t *out, size_t cap, uint32_t epoch_secs) {
    if (cap < 5) return 0;
    out[0] = MC_CMD_SET_DEVICE_TIME; put_u32(out + 1, epoch_secs); return 5;
}

size_t mc_cmd_sync_next_message(uint8_t *out, size_t cap) {
    if (cap < 1) return 0;
    out[0] = MC_CMD_SYNC_NEXT_MESSAGE;
    return 1;
}

size_t mc_cmd_send_self_advert(uint8_t *out, size_t cap, uint8_t advert_type) {
    if (cap < 2) return 0;
    out[0] = MC_CMD_SEND_SELF_ADVERT; out[1] = advert_type; return 2;
}

size_t mc_cmd_get_channel(uint8_t *out, size_t cap, uint8_t channel_idx) {
    if (cap < 2) return 0;
    out[0] = MC_CMD_GET_CHANNEL; out[1] = channel_idx; return 2;
}

size_t mc_cmd_set_channel(uint8_t *out, size_t cap, uint8_t channel_idx,
                          const char *name, const uint8_t secret[MC_SECRET_LEN]) {
    size_t total = 1 + 1 + MC_NAME_LEN + MC_SECRET_LEN;
    if (cap < total) return 0;
    size_t i = 0;
    out[i++] = MC_CMD_SET_CHANNEL;
    out[i++] = channel_idx;
    /* 32-byte NUL-padded name, last byte forced NUL (matches meshcore.js) */
    memset(out + i, 0, MC_NAME_LEN);
    if (name) {
        size_t nlen = strlen(name);
        if (nlen > MC_NAME_LEN - 1) nlen = MC_NAME_LEN - 1;
        memcpy(out + i, name, nlen);
    }
    i += MC_NAME_LEN;
    memcpy(out + i, secret, MC_SECRET_LEN); i += MC_SECRET_LEN;
    return i;
}

size_t mc_cmd_send_channel_text(uint8_t *out, size_t cap, uint8_t txt_type,
                                uint8_t channel_idx, uint32_t sender_ts,
                                const char *text) {
    size_t tlen = text ? strlen(text) : 0;
    size_t total = 1 + 1 + 1 + 4 + tlen;
    if (cap < total) return 0;
    size_t i = 0;
    out[i++] = MC_CMD_SEND_CHANNEL_TXT_MSG;
    out[i++] = txt_type;
    out[i++] = channel_idx;
    put_u32(out + i, sender_ts); i += 4;
    memcpy(out + i, text, tlen); i += tlen;
    return i;
}

size_t mc_cmd_send_txt_msg(uint8_t *out, size_t cap, uint8_t txt_type,
                           uint8_t attempt, uint32_t sender_ts,
                           const uint8_t *dst, size_t dst_len, const char *text) {
    size_t tlen = text ? strlen(text) : 0;
    size_t total = 1 + 1 + 1 + 4 + dst_len + tlen;
    if (cap < total) return 0;
    size_t i = 0;
    out[i++] = MC_CMD_SEND_TXT_MSG;
    out[i++] = txt_type;
    out[i++] = attempt;
    put_u32(out + i, sender_ts); i += 4;
    if (dst_len) { memcpy(out + i, dst, dst_len); i += dst_len; }
    memcpy(out + i, text, tlen); i += tlen;
    return i;
}

size_t mc_cmd_send_cmd(uint8_t *out, size_t cap, uint32_t sender_ts,
                       const uint8_t *dst, size_t dst_len, const char *cmd) {
    return mc_cmd_send_txt_msg(out, cap, MC_TXT_CLI_DATA, 0, sender_ts, dst, dst_len, cmd);
}

size_t mc_cmd_set_radio_params(uint8_t *out, size_t cap, uint32_t freq_hz_x1000,
                               uint32_t bw, uint8_t sf, uint8_t cr) {
    if (cap < 11) return 0;
    size_t i = 0;
    out[i++] = MC_CMD_SET_RADIO_PARAMS;
    put_u32(out + i, freq_hz_x1000); i += 4;
    put_u32(out + i, bw);            i += 4;
    out[i++] = sf; out[i++] = cr;
    return i;
}

size_t mc_cmd_set_advert_name(uint8_t *out, size_t cap, const char *name) {
    size_t nlen = name ? strlen(name) : 0;
    size_t total = 1 + nlen;
    if (cap < total) return 0;
    out[0] = MC_CMD_SET_ADVERT_NAME;
    if (nlen) memcpy(out + 1, name, nlen);
    return total;
}

size_t mc_cmd_set_tx_power(uint8_t *out, size_t cap, uint32_t dbm) {
    if (cap < 5) return 0;
    out[0] = MC_CMD_SET_TX_POWER; put_u32(out + 1, dbm); return 5;
}

size_t mc_cmd_get_stats(uint8_t *out, size_t cap, uint8_t stats_type) {
    if (cap < 2) return 0;
    out[0] = MC_CMD_GET_STATS; out[1] = stats_type; return 2;
}

/* ---- contacts ---- */
size_t mc_cmd_get_contacts(uint8_t *out, size_t cap, uint32_t since_lastmod) {
    if (since_lastmod > 0) {
        if (cap < 5) return 0;
        out[0] = MC_CMD_GET_CONTACTS; put_u32(out + 1, since_lastmod); return 5;
    }
    if (cap < 1) return 0;
    out[0] = MC_CMD_GET_CONTACTS; return 1;
}

/* [code][pubkey:32] */
static size_t cmd_key_only(uint8_t *out, size_t cap, uint8_t code, const uint8_t key[32]) {
    if (cap < 33) return 0;
    out[0] = code; memcpy(out + 1, key, 32); return 33;
}
size_t mc_cmd_remove_contact(uint8_t *out, size_t cap, const uint8_t pubkey[32]) {
    return cmd_key_only(out, cap, MC_CMD_REMOVE_CONTACT, pubkey);
}
size_t mc_cmd_reset_path(uint8_t *out, size_t cap, const uint8_t pubkey[32]) {
    return cmd_key_only(out, cap, MC_CMD_RESET_PATH, pubkey);
}
size_t mc_cmd_share_contact(uint8_t *out, size_t cap, const uint8_t pubkey[32]) {
    return cmd_key_only(out, cap, MC_CMD_SHARE_CONTACT, pubkey);
}
size_t mc_cmd_get_contact_by_key(uint8_t *out, size_t cap, const uint8_t pubkey[32]) {
    return cmd_key_only(out, cap, MC_CMD_GET_CONTACT_BY_KEY, pubkey);
}
size_t mc_cmd_export_contact(uint8_t *out, size_t cap, const uint8_t pubkey[32]) {
    if (pubkey == NULL) { if (cap < 1) return 0; out[0] = MC_CMD_EXPORT_CONTACT; return 1; }
    return cmd_key_only(out, cap, MC_CMD_EXPORT_CONTACT, pubkey);
}
size_t mc_cmd_import_contact(uint8_t *out, size_t cap, const uint8_t *card, size_t card_len) {
    size_t total = 1 + card_len;
    if (cap < total) return 0;
    out[0] = MC_CMD_IMPORT_CONTACT;
    if (card_len) memcpy(out + 1, card, card_len);
    return total;
}
size_t mc_cmd_add_update_contact(uint8_t *out, size_t cap, const mc_contact_t *c) {
    size_t total = 1 + 32 + 1 + 1 + 1 + 64 + 32 + 4 + 4 + 4;   /* 144 */
    if (cap < total) return 0;
    size_t o = 0;
    out[o++] = MC_CMD_ADD_UPDATE_CONTACT;
    memcpy(out + o, c->public_key, 32); o += 32;
    out[o++] = c->type;
    out[o++] = c->flags;
    out[o++] = c->out_path_len;
    memcpy(out + o, c->out_path, 64); o += 64;
    memset(out + o, 0, 32);
    { size_t nl = strlen(c->adv_name); if (nl > 32) nl = 32; memcpy(out + o, c->adv_name, nl); }
    o += 32;
    put_u32(out + o, c->last_advert);        o += 4;
    put_u32(out + o, (uint32_t)c->adv_lat);  o += 4;
    put_u32(out + o, (uint32_t)c->adv_lon);  o += 4;
    return o;
}

/* ---- binary / anonymous requests ---- */
static size_t cmd_req(uint8_t *out, size_t cap, uint8_t code, const uint8_t dst[32],
                      uint8_t req_type, const uint8_t *data, size_t data_len) {
    size_t total = 1 + 32 + 1 + data_len;
    if (cap < total) return 0;
    size_t o = 0;
    out[o++] = code;
    memcpy(out + o, dst, 32); o += 32;
    out[o++] = req_type;
    if (data_len) { memcpy(out + o, data, data_len); o += data_len; }
    return o;
}
size_t mc_cmd_send_binary_req(uint8_t *out, size_t cap, const uint8_t dst[32],
                              uint8_t req_type, const uint8_t *data, size_t data_len) {
    return cmd_req(out, cap, MC_CMD_SEND_BINARY_REQ, dst, req_type, data, data_len);
}
size_t mc_cmd_send_anon_req(uint8_t *out, size_t cap, const uint8_t dst[32],
                            uint8_t req_type, const uint8_t *data, size_t data_len) {
    return cmd_req(out, cap, MC_CMD_SEND_ANON_REQ, dst, req_type, data, data_len);
}

/* ======================================================================== */
int mc_parse(const uint8_t *p, size_t len, mc_event_t *ev) {
    if (len < 1) return 0;
    memset(ev, 0, sizeof(*ev));
    ev->code = p[0];
    const uint8_t *b = p + 1;          /* body after code byte */
    size_t n = len - 1;

    switch (ev->code) {
    case MC_RESP_OK:
    case MC_RESP_DISABLED:
    case MC_RESP_NO_MORE_MESSAGES:
    case MC_PUSH_MSG_WAITING:
        return 1;

    case MC_RESP_ERR:
        ev->u.err_code = (n >= 1) ? (int8_t)b[0] : (int8_t)-1;
        return 1;

    case MC_RESP_CURR_TIME:
        if (n < 4) return 0;
        ev->u.curr_time = get_u32(b);
        return 1;

    case MC_RESP_BATTERY_VOLTAGE:
        if (n < 2) return 0;
        ev->u.battery_mv = get_u16(b);
        return 1;

    case MC_RESP_SENT:
        if (n < 9) return 0;
        ev->u.msg_sent.type              = b[0];
        ev->u.msg_sent.expected_ack      = get_u32(b + 1);
        ev->u.msg_sent.suggested_timeout = get_u32(b + 5);
        return 1;

    case MC_RESP_STATS: {
        if (n < 1) return 0;
        mc_stats_t *st = &ev->u.stats;
        st->subtype = b[0];
        const uint8_t *q = b + 1;   /* stat fields start at payload offset 2 */
        size_t qn = n - 1;
        switch (st->subtype) {
        case MC_STATS_CORE:                       /* <H I H B> */
            if (qn < 9) return 0;
            st->u.core.battery_mv  = get_u16(q);
            st->u.core.uptime_secs = get_u32(q + 2);
            st->u.core.errors      = get_u16(q + 6);
            st->u.core.queue_len   = q[8];
            return 1;
        case MC_STATS_RADIO:                      /* <h b b I I> */
            if (qn < 12) return 0;
            st->u.radio.noise_floor = (int16_t)get_u16(q);
            st->u.radio.last_rssi   = (int8_t)q[2];
            st->u.radio.last_snr_q4 = (int8_t)q[3];
            st->u.radio.tx_air_secs = get_u32(q + 4);
            st->u.radio.rx_air_secs = get_u32(q + 8);
            return 1;
        case MC_STATS_PACKETS:                    /* <I I I I I I> + opt I */
            if (qn < 24) return 0;
            st->u.packets.recv      = get_u32(q +  0);
            st->u.packets.sent      = get_u32(q +  4);
            st->u.packets.flood_tx  = get_u32(q +  8);
            st->u.packets.direct_tx = get_u32(q + 12);
            st->u.packets.flood_rx  = get_u32(q + 16);
            st->u.packets.direct_rx = get_u32(q + 20);
            if (qn >= 28) { st->u.packets.recv_errors = get_u32(q + 24); st->has_recv_errors = 1; }
            return 1;
        default:
            return 1;   /* unknown subtype: subtype recorded, no fields */
        }
    }

    case MC_RESP_DEVICE_INFO: {
        /* [fw_ver:i8][max_contacts/2:u8][max_channels:u8][ble_pin:u32]
           [build_date:cstr12][model:cstr40][ver:cstr20]
           [repeat:u8 (fw>=9)][path_hash_mode:u8 (fw>=10)] */
        if (n < 1) return 0;
        mc_device_info_t *d = &ev->u.device_info;
        d->fw_ver = (int8_t)b[0];
        size_t off = 1;
        if (n >= off + 6) {
            d->max_contacts = (uint16_t)(b[off] * 2); off += 1;
            d->max_channels = b[off]; off += 1;
            d->ble_pin = get_u32(b + off); off += 4;
        }
        if (n >= off + 12) { copy_cstring(d->build_date, sizeof(d->build_date), b + off, 12); off += 12; }
        /* model is a 40-byte field on fw>=3; tolerate short (older) frames. */
        {
            size_t mlen = (n >= off + 40) ? 40 : (n > off ? n - off : 0);
            copy_cstring(d->model, sizeof(d->model), b + off, mlen); off += mlen;
        }
        if (n >= off + 20) { copy_cstring(d->ver, sizeof(d->ver), b + off, 20); off += 20; }
        if (d->fw_ver >= 9  && n > off) { d->repeat = b[off++];         d->have_repeat = 1; }
        if (d->fw_ver >= 10 && n > off) { d->path_hash_mode = b[off++]; d->have_path_hash = 1; }
        return 1;
    }

    case MC_RESP_CHANNEL_MSG_RECV:
        return parse_channel_text(b, n, 0, &ev->u.channel_msg);
    case MC_RESP_CHANNEL_MSG_RECV_V3:
        return parse_channel_text(b, n, 1, &ev->u.channel_msg);

    case MC_RESP_CONTACT_MSG_RECV:
        return parse_contact_text(b, n, 0, &ev->u.contact_msg);
    case MC_RESP_CONTACT_MSG_RECV_V3:
        return parse_contact_text(b, n, 1, &ev->u.contact_msg);

    case MC_RESP_CHANNEL_DATA_RECV: {
        if (n < 8) return 0;
        mc_channel_data_t *d = &ev->u.channel_data;
        d->snr_q4      = (int8_t)b[0];
        /* b[1], b[2] reserved */
        d->channel_idx = (int8_t)b[3];
        d->path_len    = b[4];
        d->data_type   = get_u16(b + 5);
        d->data_len    = b[7];
        size_t avail = n - 8;
        size_t dl = d->data_len;
        if (dl > avail) dl = avail;
        if (dl > sizeof(d->data)) dl = sizeof(d->data);
        memcpy(d->data, b + 8, dl);
        d->data_len = (uint8_t)dl;
        return 1;
    }

    case MC_RESP_CHANNEL_INFO: {
        if (n < 1 + MC_NAME_LEN) return 0;
        mc_channel_info_t *c = &ev->u.channel_info;
        c->channel_idx = b[0];
        copy_cstring(c->name, sizeof(c->name), b + 1, MC_NAME_LEN);
        size_t off = 1 + MC_NAME_LEN;
        if (n - off >= MC_SECRET_LEN) {
            memcpy(c->secret, b + off, MC_SECRET_LEN);
            c->have_secret = 1;
        }
        return 1;
    }

    case MC_RESP_SELF_INFO: {
        if (n < 55) return 0;
        mc_self_info_t *s = &ev->u.self_info;
        s->type = b[0]; s->tx_power = b[1]; s->max_tx_power = b[2];
        memcpy(s->public_key, b + 3, 32);
        s->adv_lat = (int32_t)get_u32(b + 35);
        s->adv_lon = (int32_t)get_u32(b + 39);
        s->multi_acks     = b[43];
        s->adv_loc_policy = b[44];
        s->telemetry_mode = b[45];
        s->tm_base = (uint8_t)(b[45]        & 0x3);
        s->tm_loc  = (uint8_t)((b[45] >> 2) & 0x3);
        s->tm_env  = (uint8_t)((b[45] >> 4) & 0x3);
        s->manual_add_contacts = b[46];
        s->radio_freq = get_u32(b + 47);
        s->radio_bw   = get_u32(b + 51);
        s->radio_sf   = (n > 55) ? b[55] : 0;
        s->radio_cr   = (n > 56) ? b[56] : 0;
        copy_rest_string(s->name, sizeof(s->name), b, 57, n);
        return 1;
    }

    case MC_PUSH_ADVERT:
    case MC_PUSH_PATH_UPDATED:
        if (n < 32) return 0;
        memcpy(ev->u.pubkey32, b, 32);
        return 1;

    case MC_PUSH_SEND_CONFIRMED:
        if (n < 8) return 0;
        ev->u.send_confirmed.ack_code   = get_u32(b);
        ev->u.send_confirmed.round_trip = get_u32(b + 4);
        return 1;

    case MC_RESP_CONTACTS_START:
        if (n < 4) return 0;
        ev->u.contacts_count = get_u32(b);
        return 1;

    case MC_RESP_CONTACT:
    case MC_PUSH_NEW_ADVERT:
        return parse_contact(b, n, &ev->u.contact);

    case MC_RESP_END_OF_CONTACTS:
        if (n < 4) return 0;
        ev->u.contacts_lastmod = get_u32(b);
        return 1;

    case MC_RESP_CONTACT_URI: {
        size_t dl = n;
        if (dl > sizeof(ev->u.contact_uri.data)) dl = sizeof(ev->u.contact_uri.data);
        memcpy(ev->u.contact_uri.data, b, dl);
        ev->u.contact_uri.len = (uint8_t)dl;
        return 1;
    }

    case MC_RESP_ADVERT_PATH: {
        if (n < 5) return 0;
        ev->u.advert_path.timestamp = get_u32(b);
        ev->u.advert_path.path_len  = b[4];
        size_t pl = n - 5;
        if (pl > sizeof(ev->u.advert_path.path)) pl = sizeof(ev->u.advert_path.path);
        memcpy(ev->u.advert_path.path, b + 5, pl);
        return 1;
    }

    case MC_RESP_AUTOADD_CONFIG:
        if (n < 1) return 0;
        ev->u.autoadd_config = b[0];
        return 1;

    case MC_RESP_ALLOWED_REPEAT_FREQ: {
        uint8_t cnt = 0;
        size_t o = 0;
        while (o + 8 <= n && cnt < 8) {
            uint32_t lo = get_u32(b + o), hi = get_u32(b + o + 4);
            if (lo == 0 && hi == 0) break;
            ev->u.allowed_freq.pair[cnt].min = lo;
            ev->u.allowed_freq.pair[cnt].max = hi;
            cnt++; o += 8;
        }
        ev->u.allowed_freq.count = cnt;
        return 1;
    }

    case MC_PUSH_BINARY_RESP: {
        if (n < 5) return 0;          /* reserved(1) + tag(4) */
        mc_binary_resp_t *r = &ev->u.binary_resp;
        /* b[0] reserved */
        r->tag = get_u32(b + 1);
        size_t dl = n - 5;
        if (dl > sizeof(r->data)) dl = sizeof(r->data);
        memcpy(r->data, b + 5, dl);
        r->data_len = (uint8_t)dl;
        return 1;
    }

    case MC_PUSH_CONTACT_DELETED:
        if (n < 32) return 0;
        memcpy(ev->u.deleted_key, b, 32);
        return 1;

    case MC_PUSH_CONTACTS_FULL:
        return 1;

    default:
        return 0;   /* recognised code byte set in ev->code, body not parsed */
    }
}

/* Decode a STATUS binary-response payload (see mc_status_t). */
int mc_parse_status(const uint8_t *d, size_t len, mc_status_t *s) {
    if (len < 52) return 0;
    memset(s, 0, sizeof(*s));
    s->bat_mv          = get_u16(d + 0);
    s->tx_queue_len    = get_u16(d + 2);
    s->noise_floor     = (int16_t)get_u16(d + 4);
    s->last_rssi       = (int16_t)get_u16(d + 6);
    s->nb_recv         = get_u32(d + 8);
    s->nb_sent         = get_u32(d + 12);
    s->airtime_secs    = get_u32(d + 16);
    s->uptime_secs     = get_u32(d + 20);
    s->sent_flood      = get_u32(d + 24);
    s->sent_direct     = get_u32(d + 28);
    s->recv_flood      = get_u32(d + 32);
    s->recv_direct     = get_u32(d + 36);
    s->full_evts       = get_u16(d + 40);
    s->last_snr_q4     = (int16_t)get_u16(d + 42);
    s->direct_dups     = get_u16(d + 44);
    s->flood_dups      = get_u16(d + 46);
    s->rx_airtime_secs = get_u32(d + 48);
    if (len >= 56) { s->recv_errors = get_u32(d + 52); s->has_recv_errors = 1; }
    return 1;
}
