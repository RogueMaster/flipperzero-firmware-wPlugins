/* Backend state machine: the session model driven by the mock backend through
 * every core user story. SPDX-License-Identifier: GPL-3.0-only */
#include "../core/scanlist.h"
#include "backend.h"
#include "session.h"
#include "tt.h"

#include <proto/keyfmt.h>

typedef struct {
    Session s;
    Backend b;
    uint32_t now;
    uint32_t chg_all;
    int prompts;
    int stream_events;
    bool credit_violation;
    BackendEvtType last_type;
    ScanList scan; /* merged as the app does */
    ProtoPingResp ping;
    char key_line[256];
    char key_fp[PROTO_STR_FP + 1];
    uint8_t key_status;
    char blob[1024];
    uint32_t blob_len;
    ProtoHostkeyList keys;
    int errors;
    int op_done;
} Rig;

static Rig rig;

static void on_event(void* ctx, const BackendEvent* e) {
    Rig* r = ctx;
    r->last_type = e->type;
    if(e->type == BackendEvtStream) {
        r->stream_events++;
        if(e->stream.len > r->s.credit_out) r->credit_violation = true;
    }
    if(e->type == BackendEvtHostkeyPrompt) r->prompts++;
    if(e->type == BackendEvtScanResult) scan_list_merge(&r->scan, &e->scan);
    if(e->type == BackendEvtPing) r->ping = e->ping;
    if(e->type == BackendEvtKeyPub) {
        r->key_status = e->key.status;
        snprintf(r->key_fp, sizeof(r->key_fp), "%s", e->key.fp);
        snprintf(r->key_line, sizeof(r->key_line), "%.*s", (int)e->key.len, e->key.line);
    }
    if(e->type == BackendEvtKeyBlob && e->blob.len < sizeof(r->blob)) {
        memcpy(r->blob, e->blob.data, e->blob.len);
        r->blob_len = e->blob.len;
    }
    if(e->type == BackendEvtHostkeyList) r->keys = e->hostkeys;
    if(e->type == BackendEvtError) r->errors++;
    if(e->type == BackendEvtOpDone) r->op_done++;
    r->chg_all |= session_on_event(&r->s, e);
}

/* Advance simulated time, granting credit like the app does on every tick. */
static void run_for(uint32_t ms) {
    for(uint32_t t = 0; t < ms; t += 10) {
        rig.now += 10;
        uint32_t c = session_take_credit(&rig.s);
        if(c) rig.b.ops->flow_credit(rig.b.self, c);
        backend_tick(&rig.b, rig.now);
    }
}

static void run_until_phase(SessionPhase p, uint32_t max_ms) {
    for(uint32_t t = 0; t < max_ms && rig.s.phase != p; t += 10)
        run_for(10);
}

static void rig_init(void) {
    if(rig.b.self) backend_free(&rig.b);
    memset(&rig, 0, sizeof(rig));
    session_init(&rig.s);
    rig.b = mock_backend_create(on_event, &rig);
    rig.b.ops->start(rig.b.self);
    run_for(500);
}

static ProtoSshConnect conn(
    const char* profile,
    const char* user,
    const char* host,
    uint8_t auth,
    uint8_t policy,
    uint8_t transport) {
    ProtoSshConnect c;
    memset(&c, 0, sizeof(c));
    snprintf(c.profile, sizeof(c.profile), "%s", profile);
    snprintf(c.user, sizeof(c.user), "%s", user);
    snprintf(c.host, sizeof(c.host), "%s", host);
    c.port = 22;
    c.auth = auth;
    c.policy = policy;
    c.transport = transport;
    return c;
}

/* The key file a key connect streams: a synthetic OpenSSH container (no real key). */
static char key_file[1024];

static size_t put_str(uint8_t* p, const void* d, size_t n) {
    p[0] = (uint8_t)(n >> 24);
    p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8);
    p[3] = (uint8_t)n;
    memcpy(p + 4, d, n);
    return 4 + n;
}

static void make_key(bool encrypted) {
    uint8_t pub[64], bin[256];
    uint8_t pk[32] = {1, 2, 3};
    size_t pn = put_str(pub, "ssh-ed25519", 11);
    pn += put_str(pub + pn, pk, sizeof(pk));
    size_t n = 15;
    memcpy(bin, "openssh-key-v1", 15);
    n += put_str(bin + n, encrypted ? "aes256-ctr" : "none", encrypted ? 10 : 4);
    n += put_str(bin + n, encrypted ? "bcrypt" : "none", encrypted ? 6 : 4);
    n += put_str(bin + n, "", 0);
    bin[n] = bin[n + 1] = bin[n + 2] = 0;
    bin[n + 3] = 1;
    n += 4;
    n += put_str(bin + n, pub, pn);
    char b64[400];
    keyfmt_b64_encode(bin, n, true, b64, sizeof(b64));
    snprintf(
        key_file,
        sizeof(key_file),
        "-----BEGIN OPENSSH PRIVATE KEY-----\n%s\n-----END OPENSSH PRIVATE KEY-----\n",
        b64);
}

static size_t key_read(void* ctx, uint32_t off, uint8_t* buf, size_t len) {
    const char* f = ctx;
    size_t total = strlen(f);
    if(off >= total) return 0;
    if(len > total - off) len = total - off;
    memcpy(buf, f + off, len);
    return len;
}

static void connect(const ProtoSshConnect* c) {
    session_reset(&rig.s);
    if(c->auth == ProtoAuthKey) {
        if(!key_file[0]) make_key(false);
        TT_CHECK(rig.b.ops->ssh_connect_key(
            rig.b.self, c, (uint32_t)strlen(key_file), key_read, key_file));
    } else {
        TT_CHECK(rig.b.ops->ssh_connect(rig.b.self, c));
    }
    run_for(20);
}

static void passphrase(const char* pp, bool remember) {
    ProtoKeyPassphrase m;
    memset(&m, 0, sizeof(m));
    snprintf(m.passphrase, sizeof(m.passphrase), "%s", pp);
    m.remember = remember;
    TT_CHECK(rig.b.ops->key_passphrase(rig.b.self, &m));
}

static void decide(uint8_t d) {
    ProtoHostkeyDecision hd = {.decision = d};
    TT_CHECK(rig.b.ops->hostkey_decision(rig.b.self, &hd));
}

static void password(const char* pw, bool remember) {
    ProtoAuthPasswordMsg m;
    memset(&m, 0, sizeof(m));
    snprintf(m.password, sizeof(m.password), "%s", pw);
    m.remember = remember;
    TT_CHECK(rig.b.ops->auth_password(rig.b.self, &m));
}

static const char* run_cmd(const char* cmd, uint32_t max_ms) {
    static char text[OUTBUF_HEAD + OUTBUF_TAIL + 64];
    TT_CHECK(rig.b.ops->exec(rig.b.self, cmd));
    session_begin_run(&rig.s);
    for(uint32_t t = 0; t < max_ms && !rig.s.have_done; t += 10)
        run_for(10);
    TT_CHECK(rig.s.have_done);
    outbuf_render(&rig.s.out, text, sizeof(text));
    run_for(50); /* let the Ready state arrive */
    return text;
}

static void test_link_and_net(void) {
    rig_init();
    TT_EQ_INT(rig.s.link.state, BackendLinkUp);
    TT_CHECK(rig.s.have_net);
    TT_CHECK(session_iface_up(&rig.s, ProtoIfaceLan));
    TT_CHECK(!session_iface_up(&rig.s, ProtoIfaceWifi));
}

/* Quick connect, unknown key, trust, password, run a one-liner. */
static void test_quick_connect_password(void) {
    rig_init();
    ProtoSshConnect c = conn(
        "192.168.1.10:22",
        "user",
        "192.168.1.10",
        ProtoAuthPassword,
        ProtoPolicyPrompt,
        ProtoTransportAuto);
    connect(&c);
    TT_EQ_INT(rig.s.phase, SessionConnecting);
    run_until_phase(SessionHostkey, 2000);
    TT_EQ_INT(rig.s.phase, SessionHostkey);
    TT_EQ_INT(rig.s.prompt.kind, ProtoHostkeyUnknown);
    TT_CHECK(strncmp(rig.s.prompt.fp, "SHA256:", 7) == 0);
    TT_EQ_INT(strlen(rig.s.prompt.fp), 50);
    /* The password must not be requested before the host-key decision. */
    TT_CHECK(rig.s.ssh_state == ProtoSshHostkeyWait);
    decide(ProtoDecisionAdd);
    run_until_phase(SessionPassword, 2000);
    TT_EQ_INT(rig.s.phase, SessionPassword);
    TT_CHECK(!rig.s.auth_retry);
    password("hunter2", false);
    run_until_phase(SessionReady, 2000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    TT_EQ_INT(rig.s.iface, ProtoIfaceLan);

    const char* out = run_cmd("uname -a", 3000);
    TT_CHECK(strstr(out, "Linux server") == out);
    TT_EQ_INT(rig.s.done.exit_code, 0);
    TT_CHECK(rig.s.done.duration_ms > 0);
    TT_CHECK(!rig.credit_violation);
    TT_EQ_INT(rig.s.phase, SessionReady);

    /* Same profile again: key is now trusted, no prompt. */
    rig.b.ops->disconnect(rig.b.self);
    run_until_phase(SessionClosed, 500);
    TT_EQ_INT(rig.s.phase, SessionClosed);
    int prompts = rig.prompts;
    connect(&c);
    run_until_phase(SessionPassword, 3000);
    TT_EQ_INT(rig.prompts, prompts);
    TT_EQ_INT(rig.s.phase, SessionPassword);
}

static void test_wrong_password_then_ok_and_remember(void) {
    rig_init();
    ProtoSshConnect c =
        conn("nas", "user", "192.168.1.10", ProtoAuthPassword, ProtoPolicyOff, ProtoTransportLan);
    connect(&c);
    run_until_phase(SessionPassword, 3000);
    TT_EQ_INT(rig.prompts, 0); /* policy off never prompts */
    password("wrong", false);
    run_for(800);
    TT_EQ_INT(rig.s.phase, SessionPassword);
    TT_CHECK(rig.s.auth_retry);
    password("right", true);
    run_until_phase(SessionReady, 2000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    TT_CHECK(rig.s.stored_secret);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);
    /* remembered: goes straight to ready */
    connect(&c);
    run_until_phase(SessionReady, 3000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    /* forgetting removes it */
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);
    rig.b.ops->secret_forget(rig.b.self, ProtoForgetProfile, "nas");
    run_for(100);
    connect(&c);
    run_until_phase(SessionPassword, 3000);
    TT_EQ_INT(rig.s.phase, SessionPassword);
}

static void test_three_failures(void) {
    rig_init();
    ProtoSshConnect c =
        conn("nas", "user", "10.0.0.2", ProtoAuthPassword, ProtoPolicyOff, ProtoTransportAuto);
    connect(&c);
    for(int i = 0; i < 3; i++) {
        run_until_phase(SessionPassword, 3000);
        password("wrong", false);
        run_for(800);
    }
    TT_EQ_INT(rig.s.phase, SessionFailed);
    TT_EQ_INT(rig.s.reason, ProtoReasonAuthFailed);
}

/* Same-IP fleet: prompt on mismatch, trust & add, later both silent. */
static void test_fleet_mismatch(void) {
    rig_init();
    ProtoSshConnect c =
        conn("robot", "pi", "192.168.4.1", ProtoAuthKey, ProtoPolicyPrompt, ProtoTransportAuto);
    connect(&c);
    run_until_phase(SessionHostkey, 2000);
    TT_EQ_INT(rig.s.prompt.kind, ProtoHostkeyUnknown);
    decide(ProtoDecisionAdd);
    run_until_phase(SessionReady, 2000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);

    connect(&c); /* a different robot at the same IP */
    run_until_phase(SessionHostkey, 2000);
    TT_EQ_INT(rig.s.prompt.kind, ProtoHostkeyMismatch);
    TT_EQ_INT(rig.s.prompt.count, 1);
    TT_CHECK(strcmp(rig.s.prompt.trusted[0].fp, rig.s.prompt.fp) != 0);
    decide(ProtoDecisionOnce);
    run_until_phase(SessionReady, 2000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);

    connect(&c);
    run_until_phase(SessionHostkey, 2000);
    decide(ProtoDecisionCancel);
    run_until_phase(SessionFailed, 1000);
    TT_EQ_INT(rig.s.reason, ProtoReasonHostkeyCancelled);

    rig.b.ops->hostkey_list(rig.b.self, "robot");
    run_for(200);
    TT_EQ_INT(rig.keys.count, 1);
    rig.b.ops->hostkey_delete(rig.b.self, "robot", rig.keys.keys[0].fp);
    run_for(200);
    rig.b.ops->hostkey_list(rig.b.self, "robot");
    run_for(200);
    TT_EQ_INT(rig.keys.count, 0);
}

static void test_strict(void) {
    rig_init();
    ProtoSshConnect c = conn(
        "robot-strict", "pi", "robot.lan", ProtoAuthKey, ProtoPolicyStrict, ProtoTransportAuto);
    connect(&c);
    run_until_phase(SessionHostkey, 2000);
    decide(ProtoDecisionAdd); /* strict still asks once */
    run_until_phase(SessionReady, 2000);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);
    int prompts = rig.prompts;
    connect(&c);
    run_until_phase(SessionFailed, 3000);
    TT_EQ_INT(rig.s.phase, SessionFailed);
    TT_EQ_INT(rig.s.reason, ProtoReasonHostkeyRejected);
    TT_EQ_INT(rig.prompts, prompts);
}

static void test_failures(void) {
    rig_init();
    ProtoSshConnect c =
        conn("x", "u", "10.0.0.99", ProtoAuthKey, ProtoPolicyOff, ProtoTransportAuto);
    connect(&c);
    run_until_phase(SessionFailed, 5000);
    TT_EQ_INT(rig.s.reason, ProtoReasonTimeout);
    TT_EQ_STR(session_failure_text(&rig.s), "Connection timed out");

    c = conn("x", "u", "refused.lan", ProtoAuthKey, ProtoPolicyOff, ProtoTransportAuto);
    connect(&c);
    run_until_phase(SessionFailed, 3000);
    TT_EQ_INT(rig.s.reason, ProtoReasonRefused);

    c = conn("x", "u", "10.0.0.2", ProtoAuthKey, ProtoPolicyOff, ProtoTransportWifi);
    connect(&c);
    run_until_phase(SessionFailed, 3000);
    TT_EQ_INT(rig.s.reason, ProtoReasonNoLink);

    c = conn("x", "nokey", "10.0.0.2", ProtoAuthKey, ProtoPolicyOff, ProtoTransportAuto);
    connect(&c);
    run_until_phase(SessionFailed, 3000);
    TT_EQ_INT(rig.s.reason, ProtoReasonAuthFailed);

    /* Quick Connect on the default key: a refused key falls back to the password */
    c.flags = PROTO_CONN_KEY_FALLBACK;
    connect(&c);
    run_until_phase(SessionPassword, 3000);
    TT_EQ_INT(rig.s.phase, SessionPassword);
    password("hunter2", false);
    run_until_phase(SessionReady, 2000);
    TT_EQ_INT(rig.s.phase, SessionReady);
}

static void ready_session(void) {
    rig_init();
    ProtoSshConnect c =
        conn("nas", "user", "192.168.1.10", ProtoAuthKey, ProtoPolicyOff, ProtoTransportAuto);
    connect(&c);
    run_until_phase(SessionReady, 3000);
    TT_EQ_INT(rig.s.phase, SessionReady);
}

static void test_outputs(void) {
    ready_session();
    const char* out = run_cmd("sudo systemctl restart nginx", 3000);
    TT_EQ_INT(rig.s.done.exit_code, 1);
    TT_CHECK(rig.s.saw_stderr);
    TT_CHECK(strstr(out, "a password is required") != NULL);

    out = run_cmd("ls --color", 3000);
    TT_EQ_STR(out, "backups  docker  deploy.sh  notes.txt\n"); /* ANSI stripped, CRLF fixed */

    out = run_cmd("frobnicate", 3000);
    TT_EQ_INT(rig.s.done.exit_code, 127);

    out = run_cmd("exit 3", 3000);
    TT_EQ_INT(rig.s.done.exit_code, 3);
    TT_EQ_STR(out, "");

    /* long output: credit respected, truncated in the middle, exit code still arrives */
    out = run_cmd("seq 20000", 120000);
    TT_EQ_INT(rig.s.done.exit_code, 0);
    TT_CHECK(!rig.credit_violation);
    TT_CHECK(rig.s.bytes_rx <= rig.s.credit_total);
    TT_CHECK(outbuf_truncated(&rig.s.out));
    TT_CHECK(strncmp(out, "1\n2\n", 4) == 0);
    TT_CHECK(strstr(out, "20000\n") != NULL);
    TT_CHECK(strstr(out, "skipped") != NULL);
}

static void test_cancel(void) {
    ready_session();
    TT_CHECK(rig.b.ops->exec(rig.b.self, "dmesg"));
    session_begin_run(&rig.s);
    run_for(300);
    TT_CHECK(rig.b.ops->cancel(rig.b.self));
    run_for(300);
    TT_CHECK(rig.s.have_done);
    TT_CHECK(rig.s.done.flags & PROTO_DONE_CANCELLED);
    TT_EQ_INT(rig.s.done.exit_code, 130);
    run_for(100);
    TT_EQ_INT(rig.s.phase, SessionReady);
    /* busy rules: exec while running is refused */
    TT_CHECK(rig.b.ops->exec(rig.b.self, "sleep 2"));
    session_begin_run(&rig.s);
    TT_CHECK(!rig.b.ops->exec(rig.b.self, "uname"));
    run_for(3000);
    TT_CHECK(rig.s.have_done);
}

static void test_disconnect_during_run(void) {
    ready_session();
    TT_CHECK(rig.b.ops->exec(rig.b.self, "dmesg"));
    session_begin_run(&rig.s);
    run_for(200);
    rig.b.ops->disconnect(rig.b.self);
    run_for(200);
    TT_CHECK(rig.s.have_done);
    TT_CHECK(rig.s.done.flags & PROTO_DONE_LOST);
    TT_EQ_INT(rig.s.phase, SessionClosed);
}

typedef struct {
    const char* text;
} ScriptSrc;

static size_t script_read(void* ctx, uint32_t off, uint8_t* buf, size_t len) {
    ScriptSrc* s = ctx;
    size_t n = strlen(s->text);
    if(off >= n) return 0;
    size_t k = n - off < len ? n - off : len;
    memcpy(buf, s->text + off, k);
    return k;
}

static void test_script(void) {
    ready_session();
    ScriptSrc src = {
        "#!/bin/bash\nset -e\n# comment\necho backing up\nhostname\nexit 2\necho never\n"};
    TT_CHECK(
        rig.b.ops->run_script(rig.b.self, "bash", (uint32_t)strlen(src.text), script_read, &src));
    session_begin_run(&rig.s);
    for(int i = 0; i < 300 && !rig.s.have_done; i++)
        run_for(10);
    TT_CHECK(rig.s.have_done);
    TT_EQ_INT(rig.s.done.exit_code, 2);
    char text[256];
    outbuf_render(&rig.s.out, text, sizeof(text));
    TT_EQ_STR(text, "backing up\nserver\n");

    /* no trailing newline on the last line */
    run_for(100);
    ScriptSrc src2 = {"echo last"};
    TT_CHECK(
        rig.b.ops->run_script(rig.b.self, "sh", (uint32_t)strlen(src2.text), script_read, &src2));
    session_begin_run(&rig.s);
    for(int i = 0; i < 300 && !rig.s.have_done; i++)
        run_for(10);
    outbuf_render(&rig.s.out, text, sizeof(text));
    TT_EQ_STR(text, "last\n");
    TT_EQ_INT(rig.s.done.exit_code, 0);
    /* too large */
    run_for(100);
    TT_CHECK(!rig.b.ops->run_script(rig.b.self, "sh", PROTO_SCRIPT_MAX + 1, script_read, &src2));
}

static void test_network_ops(void) {
    rig_init();
    TT_CHECK(rig.b.ops->wifi_scan(rig.b.self));
    run_for(2000);
    TT_EQ_INT(rig.scan.count, 6);
    TT_EQ_STR(rig.scan.entries[0].ssid, "Lab-WiFi");
    TT_EQ_STR(
        rig.scan.entries[5].ssid, "\xe5\xae\xb6\xe7\x9a\x84WiFi"); /* bytes as sent: for joining */
    TT_CHECK(rig.scan.done);

    ProtoWifiJoin j;
    memset(&j, 0, sizeof(j));
    strcpy(j.ssid, "Lab-WiFi");
    strcpy(j.password, "wrongpass");
    TT_CHECK(rig.b.ops->wifi_join(rig.b.self, &j));
    TT_CHECK(!rig.b.ops->wifi_join(rig.b.self, &j)); /* one join at a time */
    run_for(2500);
    TT_CHECK(!session_iface_up(&rig.s, ProtoIfaceWifi));
    strcpy(j.password, "correct horse");
    TT_CHECK(rig.b.ops->wifi_join(rig.b.self, &j));
    run_for(2500);
    TT_CHECK(session_iface_up(&rig.s, ProtoIfaceWifi));
    TT_EQ_STR(rig.s.net.wifi_ssid, "Lab-WiFi");

    ProtoNetCfg cfg = {
        .iface = ProtoIfaceLan,
        .mode = ProtoIpStatic,
        .ip = 0x0A000005,
        .mask = 0xFFFFFF00,
        .gw = 0x0A000001};
    int ops = rig.op_done;
    TT_CHECK(rig.b.ops->net_set_cfg(rig.b.self, &cfg));
    run_for(600);
    TT_EQ_INT(rig.op_done, ops + 1);
    TT_EQ_INT(rig.s.net.lan.ip, 0x0A000005);
    TT_EQ_INT(rig.s.net.lan.mode, ProtoIpStatic);

    ProtoPingReq p;
    memset(&p, 0, sizeof(p));
    strcpy(p.host, "192.168.1.10");
    p.count = 4;
    TT_CHECK(rig.b.ops->ping(rig.b.self, &p));
    run_for(1200);
    TT_EQ_INT(rig.ping.received, 4);
    strcpy(p.host, "10.9.9.99");
    TT_CHECK(rig.b.ops->ping(rig.b.self, &p));
    run_for(1200);
    TT_EQ_INT(rig.ping.received, 0);
}

/* Generated keys come back as a file plus their public key line; the file is
 * what keys/ will hold, and the Keys screen reads the same fingerprint from it. */
static void test_keygen(void) {
    rig_init();
    ProtoKeyGen g = {.key_type = ProtoKeyGenEd25519};
    TT_CHECK(rig.b.ops->key_gen(rig.b.self, &g));
    run_for(1000);
    TT_EQ_INT(rig.key_status, 0);
    TT_CHECK(strncmp(rig.key_line, "ssh-ed25519 AAAA", 16) == 0);
    TT_CHECK(strstr(rig.key_line, " ssh_runner") != NULL);
    TT_CHECK(rig.blob_len > 0);
    static uint8_t scratch[KEYFMT_SCRATCH];
    KeyInfo ki;
    keyfmt_sniff(rig.blob, rig.blob_len, &ki, scratch, sizeof(scratch));
    TT_EQ_INT(ki.fmt, KeyFmtOpenSsh);
    TT_EQ_INT(ki.type, KeyTypeEd25519);
    TT_CHECK(!ki.encrypted);
    char fp[KEYFMT_FP_SIZE];
    keyfmt_fingerprint(ki.pub, ki.pub_len, fp);
    TT_EQ_STR(fp, rig.key_fp);
    char first[PROTO_STR_FP + 1];
    strcpy(first, rig.key_fp);

    g.key_type = ProtoKeyGenEcdsaP256;
    strcpy(g.passphrase, "secret");
    TT_CHECK(rig.b.ops->key_gen(rig.b.self, &g));
    run_for(3000);
    TT_CHECK(strncmp(rig.key_line, "ecdsa-sha2-nistp256 ", 20) == 0);
    TT_CHECK(strcmp(first, rig.key_fp) != 0);
    keyfmt_sniff(rig.blob, rig.blob_len, &ki, scratch, sizeof(scratch));
    TT_CHECK(ki.encrypted);
    TT_EQ_INT(ki.bits, 256);
}

/* An encrypted key asks for its passphrase before the connection, re-asks after a
 * wrong one, then is remembered ("decrypt once") until forgotten. */
static void test_key_passphrase(void) {
    rig_init();
    make_key(true);
    ProtoSshConnect c =
        conn("nas", "user", "192.168.1.10", ProtoAuthKey, ProtoPolicyOff, ProtoTransportAuto);
    strcpy(c.key_id, "SHA256:test");
    connect(&c);
    run_until_phase(SessionPassphrase, 2000);
    TT_EQ_INT(rig.s.phase, SessionPassphrase);
    TT_CHECK(!rig.s.auth_retry);
    passphrase("wrong", true);
    run_for(600);
    TT_EQ_INT(rig.s.phase, SessionPassphrase);
    TT_CHECK(rig.s.auth_retry);
    TT_EQ_STR(session_failure_text(&rig.s), "Wrong passphrase");
    passphrase("right", true);
    run_for(100);
    TT_EQ_STR(session_status_text(&rig.s), "Unlocking key...");
    run_until_phase(SessionReady, 3000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);

    connect(&c); /* remembered */
    run_until_phase(SessionReady, 3000);
    TT_EQ_INT(rig.s.phase, SessionReady);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);

    rig.b.ops->secret_forget(rig.b.self, ProtoForgetPassphrase, "SHA256:test");
    run_for(100);
    connect(&c);
    run_until_phase(SessionPassphrase, 2000);
    TT_EQ_INT(rig.s.phase, SessionPassphrase);
    rig.b.ops->disconnect(rig.b.self);
    run_for(100);

    /* a public key in place of the private one fails before any connection */
    strcpy(
        key_file,
        "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIKmbNVUx4KjFZz5Wy6q9UbOAJmjohi5fktxyWWAjI2cl x\n");
    connect(&c);
    run_until_phase(SessionFailed, 2000);
    TT_EQ_INT(rig.s.reason, ProtoReasonBadKey);
    TT_EQ_STR(session_failure_text(&rig.s), "This is a public key; copy the private key file too");

    /* without a .pub: the public key comes back after the first parse */
    make_key(false);
    c.flags = PROTO_CONN_WANT_PUB;
    rig.key_line[0] = '\0';
    connect(&c);
    run_until_phase(SessionReady, 3000);
    TT_CHECK(strncmp(rig.key_line, "ssh-ed25519 AAAA", 16) == 0);
    key_file[0] = '\0';
}

/* The ESP32 sends SSH_STATE(HostkeyWait) before HOSTKEY_PROMPT_EVT: the host-key screen
 * must not open on the state alone (it showed "Unknown key" with no fingerprint). */
static void test_hostkey_state_before_prompt(void) {
    Session s;
    session_init(&s);
    BackendEvent e;
    memset(&e, 0, sizeof(e));
    e.type = BackendEvtSshState;
    e.ssh.state = ProtoSshConnecting;
    session_on_event(&s, &e);
    e.ssh.state = ProtoSshHostkeyWait;
    session_on_event(&s, &e);
    TT_CHECK(s.phase != SessionHostkey);
    memset(&e, 0, sizeof(e));
    e.type = BackendEvtHostkeyPrompt;
    e.hostkey.kind = ProtoHostkeyUnknown;
    strcpy(e.hostkey.key_type, "ssh-ed25519");
    strcpy(e.hostkey.fp, "SHA256:abc");
    uint32_t chg = session_on_event(&s, &e);
    TT_CHECK(chg & SESSION_CHG_PROMPT);
    TT_EQ_INT(s.phase, SessionHostkey);
    TT_EQ_STR(s.prompt.key_type, "ssh-ed25519");
}

static void test_link_test(void) {
    rig_init();
    TT_CHECK(!rig.b.ops->link_test(rig.b.self, 0));
    TT_CHECK(rig.b.ops->link_test(rig.b.self, 2048));
    TT_CHECK(!(rig.chg_all & SESSION_CHG_LINKTEST)); /* never synchronous */
    run_for(1500);
    TT_CHECK(rig.chg_all & SESSION_CHG_LINKTEST);
    TT_CHECK(rig.s.link_test.done);
    TT_CHECK(!rig.s.link_test.aborted);
    TT_EQ_INT(rig.s.link_test.sent, 2048);
    TT_EQ_INT(rig.s.link_test.mismatches, 0);
    TT_EQ_INT(rig.s.link_test.bytes, 2048 * 512);
}

/* WiFi scan events (up to 8 networks each) merged into one list. */
static void test_scan_merge(void) {
    static ScanList l;
    static ProtoScanResult r;
    scan_list_reset(&l);
    memset(&r, 0, sizeof(r));
    r.total = 9;
    r.count = 8;
    for(int i = 0; i < 8; i++)
        snprintf(r.entries[i].ssid, sizeof(r.entries[i].ssid), "net%d", i);
    scan_list_merge(&l, &r);
    TT_EQ_INT(l.count, 8);
    TT_CHECK(!l.done);
    memset(&r.entries, 0, sizeof(r.entries));
    r.first = 8;
    r.count = 1;
    r.last = 1;
    snprintf(r.entries[0].ssid, sizeof(r.entries[0].ssid), "net8");
    scan_list_merge(&l, &r);
    TT_EQ_INT(l.count, 9); /* the board's 9 networks, not the last event's 1 */
    TT_EQ_STR(l.entries[0].ssid, "net0");
    TT_EQ_STR(l.entries[8].ssid, "net8");
    TT_CHECK(l.done);
    /* a new scan starts over */
    r.first = 0;
    r.count = 1;
    snprintf(r.entries[0].ssid, sizeof(r.entries[0].ssid), "only");
    scan_list_merge(&l, &r);
    TT_EQ_INT(l.count, 1);
    TT_EQ_STR(l.entries[0].ssid, "only");
    /* more than the list holds: the rest is left out */
    r.first = 12;
    r.count = 8;
    r.last = 1;
    scan_list_merge(&l, &r);
    TT_EQ_INT(l.count, SCAN_LIST_MAX);
}

int main(void) {
    TT_RUN(test_hostkey_state_before_prompt);
    TT_RUN(test_link_test);
    TT_RUN(test_link_and_net);
    TT_RUN(test_quick_connect_password);
    TT_RUN(test_wrong_password_then_ok_and_remember);
    TT_RUN(test_three_failures);
    TT_RUN(test_fleet_mismatch);
    TT_RUN(test_strict);
    TT_RUN(test_failures);
    TT_RUN(test_outputs);
    TT_RUN(test_keygen);
    TT_RUN(test_key_passphrase);
    TT_RUN(test_cancel);
    TT_RUN(test_disconnect_during_run);
    TT_RUN(test_script);
    TT_RUN(test_network_ops);
    TT_RUN(test_scan_merge);
    if(rig.b.self) backend_free(&rig.b);
    TT_END();
}
