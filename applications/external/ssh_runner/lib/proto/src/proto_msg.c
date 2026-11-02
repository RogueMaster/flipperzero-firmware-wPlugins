/* SPDX-License-Identifier: GPL-3.0-only */
#include "proto/proto_msg.h"

#include <string.h>

/* ---------------- writer ---------------- */

void pw_init(ProtoWriter* w, uint8_t* buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->err = false;
}

static bool pw_room(ProtoWriter* w, size_t n) {
    if(w->err || w->cap - w->len < n) {
        w->err = true;
        return false;
    }
    return true;
}

void pw_u8(ProtoWriter* w, uint8_t v) {
    if(pw_room(w, 1)) w->buf[w->len++] = v;
}

void pw_u16(ProtoWriter* w, uint16_t v) {
    if(!pw_room(w, 2)) return;
    w->buf[w->len++] = (uint8_t)(v & 0xFF);
    w->buf[w->len++] = (uint8_t)(v >> 8);
}

void pw_u32(ProtoWriter* w, uint32_t v) {
    if(!pw_room(w, 4)) return;
    for(unsigned i = 0; i < 4; i++)
        w->buf[w->len++] = (uint8_t)(v >> (8u * i));
}

void pw_i32(ProtoWriter* w, int32_t v) {
    pw_u32(w, (uint32_t)v);
}

void pw_str(ProtoWriter* w, const char* s) {
    size_t n = s ? strlen(s) : 0;
    if(n > 255) {
        w->err = true;
        return;
    }
    pw_u8(w, (uint8_t)n);
    pw_bytes(w, s, n);
}

void pw_bytes(ProtoWriter* w, const void* data, size_t len) {
    if(!len || !pw_room(w, len)) return;
    memcpy(&w->buf[w->len], data, len);
    w->len += len;
}

size_t pw_finish(const ProtoWriter* w) {
    return w->err ? 0 : w->len;
}

/* ---------------- reader ---------------- */

void pr_init(ProtoReader* r, const uint8_t* buf, size_t len) {
    r->buf = buf;
    r->len = len;
    r->pos = 0;
    r->err = false;
}

static bool pr_have(ProtoReader* r, size_t n) {
    if(r->err || r->len - r->pos < n) {
        r->err = true;
        return false;
    }
    return true;
}

uint8_t pr_u8(ProtoReader* r) {
    return pr_have(r, 1) ? r->buf[r->pos++] : 0;
}

uint16_t pr_u16(ProtoReader* r) {
    if(!pr_have(r, 2)) return 0;
    uint16_t v = (uint16_t)(r->buf[r->pos] | ((uint16_t)r->buf[r->pos + 1] << 8));
    r->pos += 2;
    return v;
}

uint32_t pr_u32(ProtoReader* r) {
    if(!pr_have(r, 4)) return 0;
    uint32_t v = 0;
    for(unsigned i = 0; i < 4; i++)
        v |= (uint32_t)r->buf[r->pos + i] << (8u * i);
    r->pos += 4;
    return v;
}

int32_t pr_i32(ProtoReader* r) {
    return (int32_t)pr_u32(r);
}

void pr_str(ProtoReader* r, char* out, size_t out_size) {
    uint8_t n = pr_u8(r);
    if(r->err || n >= out_size || !pr_have(r, n)) {
        r->err = true;
        if(out_size) out[0] = '\0';
        return;
    }
    memcpy(out, &r->buf[r->pos], n);
    out[n] = '\0';
    /* Embedded NULs would silently truncate; treat them as malformed. */
    if(memchr(out, '\0', n)) r->err = true;
    r->pos += n;
}

const uint8_t* pr_rest(ProtoReader* r, size_t* len) {
    *len = r->err ? 0 : r->len - r->pos;
    const uint8_t* p = r->err ? NULL : &r->buf[r->pos];
    if(!r->err) r->pos = r->len;
    return p;
}

bool pr_done(const ProtoReader* r) {
    return !r->err && r->pos == r->len;
}

/* ---------------- typed codecs ---------------- */

#define ENC_BEGIN         \
    ProtoWriter w_;       \
    ProtoWriter* w = &w_; \
    pw_init(w, buf, cap)
#define ENC_END return pw_finish(w)
#define DEC_BEGIN             \
    ProtoReader r_;           \
    ProtoReader* r = &r_;     \
    memset(m, 0, sizeof(*m)); \
    pr_init(r, buf, len)
#define DEC_END    return pr_done(r)
#define STR(field) pr_str(r, m->field, sizeof(m->field))

size_t proto_enc_hello(const ProtoHello* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->proto_version);
    pw_u32(w, m->max_baud);
    pw_str(w, m->version);
    ENC_END;
}
bool proto_dec_hello(const uint8_t* buf, size_t len, ProtoHello* m) {
    DEC_BEGIN;
    m->proto_version = pr_u8(r);
    m->max_baud = pr_u32(r);
    STR(version);
    DEC_END;
}

size_t proto_enc_hello_resp(const ProtoHelloResp* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->proto_version);
    pw_u32(w, m->max_baud);
    pw_u32(w, m->baud);
    pw_str(w, m->version);
    ENC_END;
}
bool proto_dec_hello_resp(const uint8_t* buf, size_t len, ProtoHelloResp* m) {
    DEC_BEGIN;
    m->proto_version = pr_u8(r);
    m->max_baud = pr_u32(r);
    m->baud = pr_u32(r);
    STR(version);
    DEC_END;
}

size_t proto_enc_flow_credit(const ProtoFlowCredit* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u32(w, m->bytes);
    ENC_END;
}
bool proto_dec_flow_credit(const uint8_t* buf, size_t len, ProtoFlowCredit* m) {
    DEC_BEGIN;
    m->bytes = pr_u32(r);
    DEC_END;
}

size_t proto_enc_log(const ProtoLog* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->level);
    pw_str(w, m->msg);
    ENC_END;
}
bool proto_dec_log(const uint8_t* buf, size_t len, ProtoLog* m) {
    DEC_BEGIN;
    m->level = pr_u8(r);
    STR(msg);
    DEC_END;
}

size_t proto_enc_error(const ProtoError* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u16(w, m->code);
    pw_str(w, m->msg);
    ENC_END;
}
bool proto_dec_error(const uint8_t* buf, size_t len, ProtoError* m) {
    DEC_BEGIN;
    m->code = pr_u16(r);
    STR(msg);
    DEC_END;
}

size_t proto_enc_net_cfg(const ProtoNetCfg* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->iface);
    pw_u8(w, m->mode);
    pw_u32(w, m->ip);
    pw_u32(w, m->mask);
    pw_u32(w, m->gw);
    pw_u32(w, m->dns);
    ENC_END;
}
bool proto_dec_net_cfg(const uint8_t* buf, size_t len, ProtoNetCfg* m) {
    DEC_BEGIN;
    m->iface = pr_u8(r);
    m->mode = pr_u8(r);
    m->ip = pr_u32(r);
    m->mask = pr_u32(r);
    m->gw = pr_u32(r);
    m->dns = pr_u32(r);
    if(m->iface > ProtoIfaceWifi || m->mode > ProtoIpStatic) r->err = true;
    DEC_END;
}

static void enc_iface(ProtoWriter* w, const ProtoIfaceStatus* s) {
    pw_u8(w, s->flags);
    pw_u8(w, s->mode);
    pw_u32(w, s->ip);
    pw_u32(w, s->mask);
    pw_u32(w, s->gw);
}
static void dec_iface(ProtoReader* r, ProtoIfaceStatus* s) {
    s->flags = pr_u8(r);
    s->mode = pr_u8(r);
    s->ip = pr_u32(r);
    s->mask = pr_u32(r);
    s->gw = pr_u32(r);
}

size_t proto_enc_net_status(const ProtoNetStatus* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    enc_iface(w, &m->lan);
    enc_iface(w, &m->wifi);
    pw_u8(w, (uint8_t)m->wifi_rssi);
    pw_str(w, m->wifi_ssid);
    ENC_END;
}
bool proto_dec_net_status(const uint8_t* buf, size_t len, ProtoNetStatus* m) {
    DEC_BEGIN;
    dec_iface(r, &m->lan);
    dec_iface(r, &m->wifi);
    m->wifi_rssi = (int8_t)pr_u8(r);
    STR(wifi_ssid);
    DEC_END;
}

size_t proto_enc_scan_result(const ProtoScanResult* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->count > PROTO_MAX_SCAN_ENTRIES) return 0;
    pw_u8(w, m->total);
    pw_u8(w, m->first);
    pw_u8(w, m->count);
    pw_u8(w, m->last);
    for(uint8_t i = 0; i < m->count; i++) {
        pw_str(w, m->entries[i].ssid);
        pw_u8(w, (uint8_t)m->entries[i].rssi);
        pw_u8(w, m->entries[i].auth);
        pw_u8(w, m->entries[i].channel);
    }
    ENC_END;
}
bool proto_dec_scan_result(const uint8_t* buf, size_t len, ProtoScanResult* m) {
    DEC_BEGIN;
    m->total = pr_u8(r);
    m->first = pr_u8(r);
    m->count = pr_u8(r);
    m->last = pr_u8(r);
    if(m->count > PROTO_MAX_SCAN_ENTRIES) return false;
    for(uint8_t i = 0; i < m->count; i++) {
        STR(entries[i].ssid);
        m->entries[i].rssi = (int8_t)pr_u8(r);
        m->entries[i].auth = pr_u8(r);
        m->entries[i].channel = pr_u8(r);
    }
    DEC_END;
}

size_t proto_enc_wifi_join(const ProtoWifiJoin* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->ssid);
    pw_str(w, m->password);
    ENC_END;
}
bool proto_dec_wifi_join(const uint8_t* buf, size_t len, ProtoWifiJoin* m) {
    DEC_BEGIN;
    STR(ssid);
    STR(password);
    DEC_END;
}

size_t proto_enc_ping_req(const ProtoPingReq* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->host);
    pw_u8(w, m->count);
    pw_u8(w, m->transport);
    ENC_END;
}
bool proto_dec_ping_req(const uint8_t* buf, size_t len, ProtoPingReq* m) {
    DEC_BEGIN;
    STR(host);
    m->count = pr_u8(r);
    m->transport = pr_u8(r);
    if(m->transport > ProtoTransportWifi) r->err = true;
    DEC_END;
}

size_t proto_enc_ping_resp(const ProtoPingResp* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->sent);
    pw_u8(w, m->received);
    pw_u16(w, m->min_ms);
    pw_u16(w, m->avg_ms);
    pw_u16(w, m->max_ms);
    pw_u8(w, m->iface);
    ENC_END;
}
size_t proto_enc_wifi_join_result(const ProtoWifiJoinResult* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->result);
    pw_u8(w, m->reason);
    ENC_END;
}
bool proto_dec_wifi_join_result(const uint8_t* buf, size_t len, ProtoWifiJoinResult* m) {
    DEC_BEGIN;
    m->result = pr_u8(r);
    m->reason = pr_u8(r);
    if(m->result > ProtoJoinFailed) r->err = true;
    DEC_END;
}

bool proto_dec_ping_resp(const uint8_t* buf, size_t len, ProtoPingResp* m) {
    DEC_BEGIN;
    m->sent = pr_u8(r);
    m->received = pr_u8(r);
    m->min_ms = pr_u16(r);
    m->avg_ms = pr_u16(r);
    m->max_ms = pr_u16(r);
    m->iface = pr_u8(r);
    DEC_END;
}

size_t proto_enc_ssh_connect(const ProtoSshConnect* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->profile);
    pw_str(w, m->user);
    pw_str(w, m->host);
    pw_u16(w, m->port);
    pw_u8(w, m->transport);
    pw_u8(w, m->auth);
    pw_u8(w, m->policy);
    pw_u8(w, m->flags);
    pw_str(w, m->key_id);
    ENC_END;
}
bool proto_dec_ssh_connect(const uint8_t* buf, size_t len, ProtoSshConnect* m) {
    DEC_BEGIN;
    STR(profile);
    STR(user);
    STR(host);
    m->port = pr_u16(r);
    m->transport = pr_u8(r);
    m->auth = pr_u8(r);
    m->policy = pr_u8(r);
    m->flags = pr_u8(r);
    STR(key_id);
    if(m->transport > ProtoTransportWifi || m->auth > ProtoAuthKey || m->policy > ProtoPolicyOff ||
       m->port == 0)
        r->err = true;
    DEC_END;
}

size_t proto_enc_ssh_state(const ProtoSshStateEvt* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->state);
    pw_u8(w, m->reason);
    pw_u8(w, m->iface);
    pw_u8(w, m->stored_secret);
    pw_str(w, m->msg);
    ENC_END;
}
bool proto_dec_ssh_state(const uint8_t* buf, size_t len, ProtoSshStateEvt* m) {
    DEC_BEGIN;
    m->state = pr_u8(r);
    m->reason = pr_u8(r);
    m->iface = pr_u8(r);
    m->stored_secret = pr_u8(r);
    STR(msg);
    DEC_END;
}

size_t proto_enc_hostkey_prompt(const ProtoHostkeyPrompt* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->count > PROTO_MAX_TRUSTED_FP) return 0;
    pw_u8(w, m->kind);
    pw_str(w, m->key_type);
    pw_str(w, m->fp);
    pw_u8(w, m->trusted_total);
    pw_u8(w, m->count);
    for(uint8_t i = 0; i < m->count; i++) {
        pw_str(w, m->trusted[i].fp);
        pw_str(w, m->trusted[i].label);
    }
    ENC_END;
}
bool proto_dec_hostkey_prompt(const uint8_t* buf, size_t len, ProtoHostkeyPrompt* m) {
    DEC_BEGIN;
    m->kind = pr_u8(r);
    STR(key_type);
    STR(fp);
    m->trusted_total = pr_u8(r);
    m->count = pr_u8(r);
    if(m->count > PROTO_MAX_TRUSTED_FP || m->kind > ProtoHostkeyMismatch) return false;
    for(uint8_t i = 0; i < m->count; i++) {
        STR(trusted[i].fp);
        STR(trusted[i].label);
    }
    DEC_END;
}

size_t proto_enc_hostkey_decision(const ProtoHostkeyDecision* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->decision);
    pw_str(w, m->label);
    ENC_END;
}
bool proto_dec_hostkey_decision(const uint8_t* buf, size_t len, ProtoHostkeyDecision* m) {
    DEC_BEGIN;
    m->decision = pr_u8(r);
    STR(label);
    if(m->decision > ProtoDecisionReplace) r->err = true;
    DEC_END;
}

size_t proto_enc_profile_ref(const ProtoProfileRef* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->profile);
    ENC_END;
}
bool proto_dec_profile_ref(const uint8_t* buf, size_t len, ProtoProfileRef* m) {
    DEC_BEGIN;
    STR(profile);
    DEC_END;
}

size_t proto_enc_hostkey_list(const ProtoHostkeyList* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->count > PROTO_MAX_HOSTKEYS) return 0;
    pw_u8(w, m->policy_log_count);
    pw_u8(w, m->count);
    for(uint8_t i = 0; i < m->count; i++) {
        pw_str(w, m->keys[i].fp);
        pw_str(w, m->keys[i].label);
    }
    ENC_END;
}
bool proto_dec_hostkey_list(const uint8_t* buf, size_t len, ProtoHostkeyList* m) {
    DEC_BEGIN;
    m->policy_log_count = pr_u8(r);
    m->count = pr_u8(r);
    if(m->count > PROTO_MAX_HOSTKEYS) return false;
    for(uint8_t i = 0; i < m->count; i++) {
        STR(keys[i].fp);
        STR(keys[i].label);
    }
    DEC_END;
}

size_t proto_enc_hostkey_delete(const ProtoHostkeyDelete* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->profile);
    pw_str(w, m->fp);
    ENC_END;
}
bool proto_dec_hostkey_delete(const uint8_t* buf, size_t len, ProtoHostkeyDelete* m) {
    DEC_BEGIN;
    STR(profile);
    STR(fp);
    DEC_END;
}

size_t proto_enc_auth_password(const ProtoAuthPasswordMsg* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->password);
    pw_u8(w, m->remember);
    ENC_END;
}
bool proto_dec_auth_password(const uint8_t* buf, size_t len, ProtoAuthPasswordMsg* m) {
    DEC_BEGIN;
    STR(password);
    m->remember = pr_u8(r);
    DEC_END;
}

size_t proto_enc_exec_req(const ProtoExecReq* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->len == 0 || m->len > PROTO_STR_COMMAND) return 0;
    pw_bytes(w, m->command, m->len);
    ENC_END;
}
bool proto_dec_exec_req(const uint8_t* buf, size_t len, ProtoExecReq* m) {
    DEC_BEGIN;
    size_t n = 0;
    const uint8_t* p = pr_rest(r, &n);
    if(n == 0 || n > PROTO_STR_COMMAND || memchr(p, '\0', n)) return false;
    m->command = (const char*)p;
    m->len = (uint16_t)n;
    DEC_END;
}

size_t proto_enc_stream_data(const ProtoStreamData* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->len > PROTO_STREAM_CHUNK) return 0;
    pw_u8(w, m->stream);
    pw_bytes(w, m->data, m->len);
    ENC_END;
}
bool proto_dec_stream_data(const uint8_t* buf, size_t len, ProtoStreamData* m) {
    DEC_BEGIN;
    m->stream = pr_u8(r);
    size_t n = 0;
    m->data = pr_rest(r, &n);
    m->len = (uint16_t)n;
    if(m->stream > ProtoStreamStderr) return false;
    DEC_END;
}

size_t proto_enc_exec_done(const ProtoExecDone* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_i32(w, m->exit_code);
    pw_u8(w, m->flags);
    pw_u32(w, m->duration_ms);
    ENC_END;
}
bool proto_dec_exec_done(const uint8_t* buf, size_t len, ProtoExecDone* m) {
    DEC_BEGIN;
    m->exit_code = pr_i32(r);
    m->flags = pr_u8(r);
    m->duration_ms = pr_u32(r);
    DEC_END;
}

size_t proto_enc_script_begin(const ProtoScriptBegin* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->interp);
    pw_u32(w, m->total);
    ENC_END;
}
bool proto_dec_script_begin(const uint8_t* buf, size_t len, ProtoScriptBegin* m) {
    DEC_BEGIN;
    STR(interp);
    m->total = pr_u32(r);
    if(m->total > PROTO_SCRIPT_MAX) r->err = true;
    DEC_END;
}

size_t proto_enc_script_chunk(const ProtoScriptChunk* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->len == 0 || m->len > PROTO_SCRIPT_CHUNK) return 0;
    pw_u32(w, m->offset);
    pw_bytes(w, m->data, m->len);
    ENC_END;
}
bool proto_dec_script_chunk(const uint8_t* buf, size_t len, ProtoScriptChunk* m) {
    DEC_BEGIN;
    m->offset = pr_u32(r);
    size_t n = 0;
    m->data = pr_rest(r, &n);
    m->len = (uint16_t)n;
    if(n == 0) return false;
    DEC_END;
}

size_t proto_enc_script_end(const ProtoScriptEnd* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u32(w, m->crc32);
    ENC_END;
}
bool proto_dec_script_end(const uint8_t* buf, size_t len, ProtoScriptEnd* m) {
    DEC_BEGIN;
    m->crc32 = pr_u32(r);
    DEC_END;
}

size_t proto_enc_key_gen(const ProtoKeyGen* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->key_type);
    pw_str(w, m->passphrase);
    ENC_END;
}
bool proto_dec_key_gen(const uint8_t* buf, size_t len, ProtoKeyGen* m) {
    DEC_BEGIN;
    m->key_type = pr_u8(r);
    STR(passphrase);
    if(m->key_type > ProtoKeyGenEcdsaP256) r->err = true;
    DEC_END;
}

size_t proto_enc_key_blob_begin(const ProtoKeyBlobBegin* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u32(w, m->total);
    ENC_END;
}
bool proto_dec_key_blob_begin(const uint8_t* buf, size_t len, ProtoKeyBlobBegin* m) {
    DEC_BEGIN;
    m->total = pr_u32(r);
    if(m->total == 0 || m->total > PROTO_KEY_MAX) r->err = true;
    DEC_END;
}

size_t proto_enc_key_passphrase(const ProtoKeyPassphrase* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_str(w, m->passphrase);
    pw_u8(w, m->remember);
    ENC_END;
}
bool proto_dec_key_passphrase(const uint8_t* buf, size_t len, ProtoKeyPassphrase* m) {
    DEC_BEGIN;
    STR(passphrase);
    m->remember = pr_u8(r);
    DEC_END;
}

size_t proto_enc_key_pub(const ProtoKeyPub* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    if(m->len > PROTO_PUB_CHUNK || (uint32_t)m->offset + m->len > m->total ||
       m->total > PROTO_PUB_MAX)
        return 0;
    pw_u8(w, m->status);
    pw_str(w, m->fp);
    pw_u16(w, m->total);
    pw_u16(w, m->offset);
    pw_bytes(w, m->data, m->len);
    ENC_END;
}
bool proto_dec_key_pub(const uint8_t* buf, size_t len, ProtoKeyPub* m) {
    DEC_BEGIN;
    m->status = pr_u8(r);
    STR(fp);
    m->total = pr_u16(r);
    m->offset = pr_u16(r);
    size_t n = 0;
    m->data = pr_rest(r, &n);
    m->len = (uint16_t)n;
    if(m->total > PROTO_PUB_MAX || (uint32_t)m->offset + n > m->total) r->err = true;
    DEC_END;
}

size_t proto_enc_secret_forget(const ProtoSecretForget* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->kind);
    pw_str(w, m->id);
    ENC_END;
}
bool proto_dec_secret_forget(const uint8_t* buf, size_t len, ProtoSecretForget* m) {
    DEC_BEGIN;
    m->kind = pr_u8(r);
    STR(id);
    if(m->kind > ProtoForgetUnlocked || !m->id[0]) r->err = true;
    DEC_END;
}

/* ---------------- security ---------------- */

static bool sec_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

bool proto_sec_word_ok(const char* word) {
    if(!word) return false;
    while(sec_space(*word))
        word++;
    size_t n = strlen(word);
    while(n && sec_space(word[n - 1]))
        n--;
    const char* want = PROTO_SEC_WORD;
    if(n != strlen(want)) return false;
    for(size_t i = 0; i < n; i++) {
        char c = word[i];
        if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if(c != want[i]) return false;
    }
    return true;
}

size_t proto_enc_sec_status(const ProtoSecStatus* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->tier);
    pw_u8(w, m->key_block);
    pw_u8(w, m->free_blocks);
    pw_u8(w, m->efuse_virtual);
    ENC_END;
}
bool proto_dec_sec_status(const uint8_t* buf, size_t len, ProtoSecStatus* m) {
    DEC_BEGIN;
    m->tier = pr_u8(r);
    m->key_block = pr_u8(r);
    m->free_blocks = pr_u8(r);
    m->efuse_virtual = pr_u8(r);
    if(m->tier > ProtoSecTierLock) r->err = true;
    DEC_END;
}

size_t proto_enc_sec_enable(const ProtoSecEnable* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->tier);
    pw_str(w, m->word);
    ENC_END;
}
/* Decodes any word: the ESP32 checks it with proto_sec_word_ok() and answers ERROR. */
bool proto_dec_sec_enable(const uint8_t* buf, size_t len, ProtoSecEnable* m) {
    DEC_BEGIN;
    m->tier = pr_u8(r);
    STR(word);
    DEC_END;
}

size_t proto_enc_sec_state(const ProtoSecState* m, uint8_t* buf, size_t cap) {
    ENC_BEGIN;
    pw_u8(w, m->phase);
    pw_u8(w, m->result);
    pw_u8(w, m->migrated);
    pw_str(w, m->msg);
    ENC_END;
}
bool proto_dec_sec_state(const uint8_t* buf, size_t len, ProtoSecState* m) {
    DEC_BEGIN;
    m->phase = pr_u8(r);
    m->result = pr_u8(r);
    m->migrated = pr_u8(r);
    STR(msg);
    if(m->phase > ProtoSecPhaseDone || m->result > ProtoSecFailed) r->err = true;
    DEC_END;
}

/* ---------------- misc ---------------- */

uint32_t proto_crc32(const uint8_t* data, size_t len, uint32_t crc) {
    crc = ~crc;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(int b = 0; b < 8; b++)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

const char* proto_type_name(uint8_t type) {
    switch(type) {
    case PROTO_T_HELLO:
        return "HELLO";
    case PROTO_T_FLOW_CREDIT:
        return "FLOW_CREDIT";
    case PROTO_T_CANCEL:
        return "CANCEL";
    case PROTO_T_LOG_EVT:
        return "LOG_EVT";
    case PROTO_T_ECHO:
        return "ECHO";
    case PROTO_T_ECHO_RESP:
        return "ECHO_RESP";
    case PROTO_T_NET_SET_CFG:
        return "NET_SET_CFG";
    case PROTO_T_NET_GET_STATUS:
        return "NET_GET_STATUS";
    case PROTO_T_NET_STATUS_EVT:
        return "NET_STATUS_EVT";
    case PROTO_T_WIFI_SCAN:
        return "WIFI_SCAN";
    case PROTO_T_WIFI_SCAN_RESULT:
        return "WIFI_SCAN_RESULT";
    case PROTO_T_WIFI_JOIN:
        return "WIFI_JOIN";
    case PROTO_T_PING_REQ:
        return "PING_REQ";
    case PROTO_T_PING_RESP:
        return "PING_RESP";
    case PROTO_T_WIFI_JOIN_RESULT:
        return "WIFI_JOIN_RESULT";
    case PROTO_T_SSH_CONNECT:
        return "SSH_CONNECT";
    case PROTO_T_SSH_STATE_EVT:
        return "SSH_STATE_EVT";
    case PROTO_T_HOSTKEY_PROMPT_EVT:
        return "HOSTKEY_PROMPT_EVT";
    case PROTO_T_HOSTKEY_DECISION:
        return "HOSTKEY_DECISION";
    case PROTO_T_HOSTKEY_LIST:
        return "HOSTKEY_LIST";
    case PROTO_T_HOSTKEY_DELETE:
        return "HOSTKEY_DELETE";
    case PROTO_T_AUTH_PASSWORD:
        return "AUTH_PASSWORD";
    case PROTO_T_SSH_DISCONNECT:
        return "SSH_DISCONNECT";
    case PROTO_T_EXEC_REQ:
        return "EXEC_REQ";
    case PROTO_T_STREAM_DATA_EVT:
        return "STREAM_DATA_EVT";
    case PROTO_T_EXEC_DONE_EVT:
        return "EXEC_DONE_EVT";
    case PROTO_T_SCRIPT_BEGIN:
        return "SCRIPT_BEGIN";
    case PROTO_T_SCRIPT_CHUNK:
        return "SCRIPT_CHUNK";
    case PROTO_T_SCRIPT_END:
        return "SCRIPT_END";
    case PROTO_T_KEY_GEN:
        return "KEY_GEN";
    case PROTO_T_SECRET_FORGET:
        return "SECRET_FORGET";
    case PROTO_T_KEY_BLOB_BEGIN:
        return "KEY_BLOB_BEGIN";
    case PROTO_T_KEY_BLOB_CHUNK:
        return "KEY_BLOB_CHUNK";
    case PROTO_T_KEY_BLOB_END:
        return "KEY_BLOB_END";
    case PROTO_T_KEY_PASSPHRASE:
        return "KEY_PASSPHRASE";
    case PROTO_T_KEY_PUB_EVT:
        return "KEY_PUB_EVT";
    case PROTO_T_SEC_GET_STATUS:
        return "SEC_GET_STATUS";
    case PROTO_T_SEC_ENABLE:
        return "SEC_ENABLE";
    case PROTO_T_SEC_STATE_EVT:
        return "SEC_STATE_EVT";
    case PROTO_T_SEC_STATUS:
        return "SEC_STATUS";
    case PROTO_T_SHELL_OPEN:
        return "SHELL_OPEN";
    case PROTO_T_SHELL_INPUT:
        return "SHELL_INPUT";
    case PROTO_T_SHELL_CLOSE:
        return "SHELL_CLOSE";
    case PROTO_T_HELLO_RESP:
        return "HELLO_RESP";
    case PROTO_T_NET_STATUS:
        return "NET_STATUS";
    case PROTO_T_HOSTKEY_LIST_RESP:
        return "HOSTKEY_LIST_RESP";
    case PROTO_T_ERROR:
        return "ERROR";
    case PROTO_T_ACK:
        return "ACK";
    default:
        return "?";
    }
}
