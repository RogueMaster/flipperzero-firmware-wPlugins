/* "No board": a backend no board ever answers. Stands in for the UART backend in the
 * demo build (SSH_RUNNER_FLAVOUR=demo, mock backend, no UART code) and in the host tests.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "ssh_runner_i.h"

typedef struct {
    BackendEventCallback cb;
    void* ctx;
} NoBoard;

static void nb_free(void* self) {
    free(self);
}
static void nb_tick(void* self, uint32_t now) {
    UNUSED(self);
    UNUSED(now);
}
static void nb_start(void* self) {
    NoBoard* b = self;
    BackendEvent e = {.type = BackendEvtLink};
    e.link.state = BackendLinkDown;
    e.link.port_ok = 1;
    b->cb(b->ctx, &e);
}
static bool nb_false(void* self) {
    UNUSED(self);
    return false;
}
#define NB(fn, type, arg)               \
    static bool fn(void* s, type arg) { \
        UNUSED(s);                      \
        UNUSED(arg);                    \
        return false;                   \
    }
NB(nb_net_cfg, const ProtoNetCfg*, c)
NB(nb_join, const ProtoWifiJoin*, j)
NB(nb_ping, const ProtoPingReq*, r)
NB(nb_connect, const ProtoSshConnect*, r)
NB(nb_hk, const ProtoHostkeyDecision*, d)
NB(nb_auth, const ProtoAuthPasswordMsg*, m)
NB(nb_exec, const char*, c)
NB(nb_credit, uint32_t, b)
NB(nb_hk_list, const char*, p)
NB(nb_link_test, uint32_t, f)
NB(nb_key_gen, const ProtoKeyGen*, g)
NB(nb_key_pp, const ProtoKeyPassphrase*, k)
NB(nb_sec_enable, const char*, w)
static bool
    nb_connect_key(void* s, const ProtoSshConnect* c, uint32_t t, BackendScriptRead r, void* x) {
    UNUSED(s);
    UNUSED(c);
    UNUSED(t);
    UNUSED(r);
    UNUSED(x);
    return false;
}
static bool nb_forget(void* s, uint8_t kind, const char* id) {
    UNUSED(s);
    UNUSED(kind);
    UNUSED(id);
    return false;
}
static bool nb_run_script(void* s, const char* i, uint32_t t, BackendScriptRead r, void* c) {
    UNUSED(s);
    UNUSED(i);
    UNUSED(t);
    UNUSED(r);
    UNUSED(c);
    return false;
}
static bool nb_hk_delete(void* s, const char* p, const char* fp) {
    UNUSED(s);
    UNUSED(p);
    UNUSED(fp);
    return false;
}

static const BackendOps nb_ops = {
    .free = nb_free,
    .tick = nb_tick,
    .start = nb_start,
    .net_get_status = nb_false,
    .net_set_cfg = nb_net_cfg,
    .wifi_scan = nb_false,
    .wifi_join = nb_join,
    .ping = nb_ping,
    .ssh_connect = nb_connect,
    .ssh_connect_key = nb_connect_key,
    .key_passphrase = nb_key_pp,
    .hostkey_decision = nb_hk,
    .auth_password = nb_auth,
    .exec = nb_exec,
    .run_script = nb_run_script,
    .flow_credit = nb_credit,
    .cancel = nb_false,
    .disconnect = nb_false,
    .key_gen = nb_key_gen,
    .hostkey_list = nb_hk_list,
    .hostkey_delete = nb_hk_delete,
    .secret_forget = nb_forget,
    .link_test = nb_link_test,
    .sec_get_status = nb_false,
    .sec_enable = nb_sec_enable,
    .mock_scenario = NULL,
};

Backend uart_backend_create(BackendEventCallback cb, void* ctx) {
    NoBoard* b = calloc(1, sizeof(NoBoard));
    b->cb = cb;
    b->ctx = ctx;
    return (Backend){.ops = &nb_ops, .self = b};
}
