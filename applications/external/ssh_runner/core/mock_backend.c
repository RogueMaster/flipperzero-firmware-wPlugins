/*
 * Mock backend: deterministic canned behaviour so the whole UI can be exercised
 * (on the host in unit tests, and on the real Flipper) before the ESP32 exists.
 * Each scenario is triggered by an input (host name, user, password, command) matched in
 * the handlers below.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "backend.h"
#include "textproc.h"
#include "cstr.h"

#include "keydir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_QUEUE       24
#define MOCK_PROFILES    2 /* hosts whose keys the mock remembers per app run: the walks use one */
#define MOCK_KEYS        PROTO_MAX_HOSTKEYS
#define MOCK_SECRETS     8
#define MOCK_CHUNK       160
#define MOCK_STREAM_MS   15
#define MOCK_SCRIPT_OUT  2048 /* the walk's script prints ~100 B; the heap is tight */
#define MOCK_PASSPHRASES 4
#define MOCK_SEC_FAILED  100 /* ActSecPhase arg: the failure after the burn */

typedef enum {
    ActLinkUp,
    ActLinkDown,
    ActNetStatus,
    ActOpDone,
    ActLinkTest, /* arg = frames */
    ActSshState,
    ActConnectCheck,
    ActAuthResult,
    ActStream,
    ActScan,
    ActJoinDone,
    ActPingDone,
    ActKeyOut, /* generated key (arg = ProtoKeyGenType): blob, then public key */
    ActHostkeyList,
    ActError,
    ActFail, /* arg = ProtoReason */
    ActSecStatus,
    ActSecPhase, /* arg = ProtoSecPhase */
    ActSecProtected, /* the ESP32 came back from its restart protected */
} MockAct;

typedef struct {
    uint32_t due;
    uint8_t act;
    int32_t arg;
} MockItem;

typedef struct {
    char profile[PROTO_STR_PROFILE + 1];
    uint8_t count;
    ProtoHostkey keys[MOCK_KEYS];
} MockKnown;

typedef enum {
    GenNone,
    GenText, /* text[pos..len) */
    GenSeq, /* lines "i\n" for i in [i, n] */
    GenLog, /* n syslog-like lines */
    GenYes, /* n lines of "y\n" */
} GenKind;

struct MockBackend {
    BackendEventCallback cb;
    void* ctx;
    uint32_t now;

    MockItem queue[MOCK_QUEUE];
    uint8_t queued;

    ProtoNetStatus net;
    uint32_t key_serial;

    /* the staged key file as the ESP32 would see it */
    bool key_staged;
    bool key_ok; /* parsed (and unlocked) for this connect */
    bool key_encrypted;
    const char* key_problem;
    char key_line[160]; /* its public key line, for PROTO_CONN_WANT_PUB */
    char key_fp[PROTO_STR_FP + 1];
    char passphrases[MOCK_PASSPHRASES][PROTO_STR_KEYID + 1]; /* remembered, by key id */
    ProtoKeyGen keygen;

    MockKnown known[MOCK_PROFILES];
    uint8_t sec_tier; /* Protect secrets tier, 0 = plaintext NVS */
    char secrets[MOCK_SECRETS][PROTO_STR_PROFILE + 1];
    uint32_t connect_count;

    /* session */
    uint8_t ssh_state;
    ProtoSshConnect conn;
    char pending_fp[PROTO_STR_FP + 1];
    uint8_t auth_failures;
    bool remember_pending;

    /* run */
    bool running;
    uint32_t run_started;
    int32_t exit_code;
    uint8_t done_flags;
    uint32_t sleep_until;
    uint32_t credit;
    uint8_t stream; /* stdout / stderr for the generator */
    GenKind gen;
    char* text; /* heap, owned */
    size_t text_len, pos;
    uint32_t gen_i, gen_n;
    bool stream_scheduled;

    /* wifi */
    bool joining;
    char join_ssid[PROTO_STR_SSID + 1];
    uint8_t ping_transport;
};

static void emit(MockBackend* m, const BackendEvent* e) {
    if(m->cb) m->cb(m->ctx, e);
}

static void schedule(MockBackend* m, uint32_t delay, MockAct act, int32_t arg) {
    if(m->queued >= MOCK_QUEUE) return;
    MockItem it = {.due = m->now + delay, .act = (uint8_t)act, .arg = arg};
    /* keep sorted by due time, FIFO for equal times */
    uint8_t i = m->queued;
    while(i > 0 && m->queue[i - 1].due > it.due) {
        m->queue[i] = m->queue[i - 1];
        i--;
    }
    m->queue[i] = it;
    m->queued++;
}

static void drop_scheduled(MockBackend* m, MockAct act) {
    uint8_t o = 0;
    for(uint8_t i = 0; i < m->queued; i++)
        if(m->queue[i].act != act) m->queue[o++] = m->queue[i];
    m->queued = o;
}

static uint32_t hash_str(const char* s, uint32_t h) {
    while(*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static void make_fp(uint32_t seed, char* out, size_t size) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char body[44];
    uint32_t x = seed ? seed : 1;
    for(int i = 0; i < 43; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        body[i] = b64[x & 63];
    }
    body[43] = '\0';
    snprintf(out, size, "SHA256:%s", body);
}

static MockKnown* known_find(MockBackend* m, const char* profile, bool create) {
    for(int i = 0; i < MOCK_PROFILES; i++)
        if(m->known[i].profile[0] && strcmp(m->known[i].profile, profile) == 0)
            return &m->known[i];
    if(!create) return NULL;
    for(int i = 0; i < MOCK_PROFILES; i++)
        if(!m->known[i].profile[0]) {
            memset(&m->known[i], 0, sizeof(m->known[i]));
            snprintf(m->known[i].profile, sizeof(m->known[i].profile), "%s", profile);
            return &m->known[i];
        }
    return NULL;
}

static bool secret_has(MockBackend* m, const char* profile) {
    for(int i = 0; i < MOCK_SECRETS; i++)
        if(m->secrets[i][0] && strcmp(m->secrets[i], profile) == 0) return true;
    return false;
}

static void secret_set(MockBackend* m, const char* profile, bool on) {
    for(int i = 0; i < MOCK_SECRETS; i++)
        if(m->secrets[i][0] && strcmp(m->secrets[i], profile) == 0) {
            if(!on) m->secrets[i][0] = '\0';
            return;
        }
    if(!on) return;
    for(int i = 0; i < MOCK_SECRETS; i++)
        if(!m->secrets[i][0]) {
            snprintf(m->secrets[i], sizeof(m->secrets[i]), "%s", profile);
            return;
        }
}

static void send_state(MockBackend* m, uint8_t state, uint8_t reason, const char* msg) {
    m->ssh_state = state;
    BackendEvent e = {.type = BackendEvtSshState};
    e.ssh.state = state;
    e.ssh.reason = reason;
    e.ssh.iface = (m->conn.transport == ProtoTransportWifi) ? ProtoIfaceWifi : ProtoIfaceLan;
    e.ssh.stored_secret = secret_has(m, m->conn.profile) ? 1 : 0;
    snprintf(e.ssh.msg, sizeof(e.ssh.msg), "%s", msg ? msg : "");
    emit(m, &e);
}

static bool pp_has(MockBackend* m, const char* id) {
    for(int i = 0; i < MOCK_PASSPHRASES; i++)
        if(id[0] && strcmp(m->passphrases[i], id) == 0) return true;
    return false;
}

static void pp_set(MockBackend* m, const char* id, bool on) {
    for(int i = 0; i < MOCK_PASSPHRASES; i++)
        if(strcmp(m->passphrases[i], id) == 0) {
            if(!on) m->passphrases[i][0] = '\0';
            return;
        }
    if(!on || !id[0]) return;
    for(int i = 0; i < MOCK_PASSPHRASES; i++)
        if(!m->passphrases[i][0]) {
            snprintf(m->passphrases[i], sizeof(m->passphrases[i]), "%s", id);
            return;
        }
}

static void send_error(MockBackend* m, uint8_t request, uint16_t code, const char* msg) {
    BackendEvent e = {.type = BackendEvtError};
    e.error.request = request;
    e.error.code = code;
    snprintf(e.error.msg, sizeof(e.error.msg), "%s", msg);
    emit(m, &e);
}

static bool host_is(const char* host, const char* needle) {
    return strstr(host, needle) != NULL;
}

static bool last_octet_is(const char* host, const char* oct) {
    const char* dot = strrchr(host, '.');
    return dot && strcmp(dot + 1, oct) == 0;
}

/* Devices that share one default IP: the key changes on every connection. */
static bool host_is_fleet(const char* host) {
    return host_is(host, "robot") || strcmp(host, "192.168.4.1") == 0 ||
           strcmp(host, "10.42.0.1") == 0;
}

/* ---------------- output generation ---------------- */

static void gen_reset(MockBackend* m) {
    free(m->text);
    m->text = NULL;
    m->text_len = m->pos = 0;
    m->gen = GenNone;
    m->gen_i = m->gen_n = 0;
}

/* Output a canned text: takes `text` (malloc'd) instead of copying it, no second buffer. */
static void gen_take(MockBackend* m, char* text) {
    gen_reset(m);
    size_t n = strlen(text);
    char* fit = realloc(text, n + 1);
    m->text = fit ? fit : text;
    m->text_len = n;
    m->gen = GenText;
}

/* Fill up to `cap` bytes of the next output chunk. */
static size_t gen_next(MockBackend* m, char* buf, size_t cap) {
    size_t n = 0;
    while(n < cap) {
        switch(m->gen) {
        case GenText: {
            size_t left = m->text_len - m->pos;
            size_t k = left < cap - n ? left : cap - n;
            memcpy(&buf[n], &m->text[m->pos], k);
            m->pos += k;
            n += k;
            if(m->pos >= m->text_len) m->gen = GenNone;
            break;
        }
        case GenSeq:
        case GenLog:
        case GenYes: {
            char line[96];
            int k;
            if(m->gen == GenSeq)
                k = snprintf(line, sizeof(line), "%lu\n", (unsigned long)m->gen_i);
            else if(m->gen == GenYes)
                k = snprintf(line, sizeof(line), "y\n");
            else
                k = snprintf(
                    line,
                    sizeof(line),
                    "Oct 18 17:%02lu:%02lu server kernel: [%6lu.%03lu] mock event %lu\n",
                    (unsigned long)((m->gen_i / 60) % 60),
                    (unsigned long)(m->gen_i % 60),
                    (unsigned long)(m->gen_i * 7),
                    (unsigned long)((m->gen_i * 37) % 1000),
                    (unsigned long)m->gen_i);
            if((size_t)k > cap - n) return n; /* line does not fit; next chunk */
            memcpy(&buf[n], line, (size_t)k);
            n += (size_t)k;
            if(++m->gen_i > m->gen_n) m->gen = GenNone;
            break;
        }
        default:
            return n;
        }
    }
    return n;
}

static const char* first_word(const char* cmd, char* word, size_t size) {
    while(*cmd == ' ')
        cmd++;
    size_t n = 0;
    while(cmd[n] && cmd[n] != ' ' && n < size - 1) {
        word[n] = cmd[n];
        n++;
    }
    word[n] = '\0';
    const char* rest = cmd + n;
    while(*rest == ' ')
        rest++;
    return rest;
}

/* Decide output + exit code for one command line. Small outputs go into `out`
 * (append); large generators are only allowed for top-level exec (gen_ok). */
static int32_t mock_eval(
    MockBackend* m,
    const char* cmd,
    char* out,
    size_t out_cap,
    uint8_t* stream,
    bool gen_ok,
    uint32_t* sleep_ms) {
    char w[24];
    const char* rest = first_word(cmd, w, sizeof(w));
    size_t used = strlen(out);
    char* o = out + used;
    size_t cap = out_cap - used;
    *stream = ProtoStreamStdout;
#define OUT(...) snprintf(o, cap, __VA_ARGS__)

    if(strcmp(w, "uname") == 0) {
        OUT("Linux server 6.8.0-45-generic #45-Ubuntu SMP PREEMPT_DYNAMIC x86_64 GNU/Linux\n");
        return 0;
    }
    if(strcmp(w, "uptime") == 0) {
        OUT(" 17:42:01 up 12 days,  3:04,  1 user,  load average: 0.08, 0.05, 0.01\n");
        return 0;
    }
    if(strcmp(w, "hostname") == 0) {
        OUT("server\n");
        return 0;
    }
    if(strcmp(w, "whoami") == 0) {
        OUT("%s\n", m->conn.user);
        return 0;
    }
    if(strcmp(w, "echo") == 0) {
        /* as the shell does: echo "a b" prints a b */
        size_t n = strlen(rest);
        if(n >= 2 && (rest[0] == '"' || rest[0] == '\'') && rest[n - 1] == rest[0])
            OUT("%.*s\n", (int)(n - 2), rest + 1);
        else
            OUT("%s\n", rest);
        return 0;
    }
    if(strcmp(w, "true") == 0 || strcmp(w, "sleep") == 0) {
        if(w[0] == 's') *sleep_ms = (uint32_t)atoi(rest) * 1000u;
        if(*sleep_ms > 10000) *sleep_ms = 10000;
        return 0;
    }
    if(strcmp(w, "false") == 0) return 1;
    if(strcmp(w, "exit") == 0) return atoi(rest);
    if(strcmp(w, "df") == 0) {
        OUT("Filesystem      Size  Used Avail Use%% Mounted on\n"
            "/dev/nvme0n1p2  468G  121G  324G  28%% /\n"
            "tmpfs           7.8G     0  7.8G   0%% /dev/shm\n"
            "/dev/sda1       3.6T  2.1T  1.4T  61%% /srv/backup\n");
        return 0;
    }
    if(strcmp(w, "ls") == 0) {
        /* colourised like `ls --color`, exercises ANSI stripping */
        OUT("\x1b[0m\x1b[01;34mbackups\x1b[0m  \x1b[01;34mdocker\x1b[0m  "
            "\x1b[01;32mdeploy.sh\x1b[0m  notes.txt\r\n");
        return 0;
    }
    if(strcmp(w, "cat") == 0 && strstr(rest, "os-release")) {
        OUT("PRETTY_NAME=\"Ubuntu 24.04.1 LTS\"\nNAME=\"Ubuntu\"\nVERSION_ID=\"24.04\"\n"
            "ID=ubuntu\nHOME_URL=\"https://www.ubuntu.com/\"\n");
        return 0;
    }
    if(strcmp(w, "sudo") == 0) {
        *stream = ProtoStreamStderr;
        OUT("sudo: a terminal is required to read the password; either use the -S "
            "option to read from standard input or configure an askpass helper\n"
            "sudo: a password is required\n");
        return 1;
    }
    if(strcmp(w, "systemctl") == 0) {
        *stream = ProtoStreamStderr;
        OUT("Failed to %s: Access denied\nSee system logs and 'systemctl status' for details.\n",
            rest[0] ? rest : "connect to bus");
        return 4;
    }
    if(gen_ok && strcmp(w, "seq") == 0) {
        unsigned long n = cstr_to_ulong(rest, 200000);
        if(n < 1) n = 1;
        m->gen = GenSeq;
        m->gen_i = 1;
        m->gen_n = (uint32_t)n;
        return 0;
    }
    if(gen_ok && (strcmp(w, "dmesg") == 0 || strcmp(w, "journalctl") == 0)) {
        m->gen = GenLog;
        m->gen_i = 1;
        m->gen_n = 300;
        return 0;
    }
    if(gen_ok && strcmp(w, "yes") == 0) {
        m->gen = GenYes;
        m->gen_i = 1;
        m->gen_n = 20000;
        return 0;
    }
    *stream = ProtoStreamStderr;
    OUT("bash: line 1: %s: command not found\n", w[0] ? w : "?");
    return 127;
#undef OUT
}

static void start_run(MockBackend* m) {
    m->running = true;
    m->run_started = m->now;
    m->done_flags = 0;
    m->credit = 0;
    m->ssh_state = ProtoSshRunning;
    schedule(m, 0, ActSshState, ProtoSshRunning);
    m->stream_scheduled = true;
    schedule(m, 120 + m->sleep_until, ActStream, 0);
    m->sleep_until = 0;
}

static void finish_run(MockBackend* m) {
    m->running = false;
    gen_reset(m);
    BackendEvent e = {.type = BackendEvtExecDone};
    e.done.exit_code = m->exit_code;
    e.done.flags = m->done_flags;
    e.done.duration_ms = m->now - m->run_started;
    emit(m, &e);
    send_state(m, ProtoSshReady, ProtoReasonNone, NULL);
}

static void do_stream(MockBackend* m) {
    m->stream_scheduled = false;
    if(!m->running) return;
    if(m->gen == GenNone) {
        finish_run(m);
        return;
    }
    if(m->credit > 0) {
        char buf[MOCK_CHUNK];
        size_t cap = m->credit < sizeof(buf) ? m->credit : sizeof(buf);
        size_t n = gen_next(m, buf, cap);
        if(n) {
            m->credit -= (uint32_t)n;
            BackendEvent e = {.type = BackendEvtStream};
            e.stream.stream = m->stream;
            e.stream.len = (uint16_t)n;
            e.stream.data = (const uint8_t*)buf;
            emit(m, &e);
        }
    }
    m->stream_scheduled = true;
    schedule(m, MOCK_STREAM_MS, ActStream, 0);
}

/* ---------------- connect flow ---------------- */

/* The ESP32 parses (and unlocks) the staged key before it opens a connection. */
static bool key_step(MockBackend* m) {
    if(m->key_ok) return true;
    if(!m->key_staged) {
        send_state(m, ProtoSshFailed, ProtoReasonNoKey, "No key file sent");
        return false;
    }
    if(m->key_problem) {
        send_state(m, ProtoSshFailed, ProtoReasonBadKey, m->key_problem);
        return false;
    }
    if(m->key_encrypted && !pp_has(m, m->conn.key_id)) {
        send_state(m, ProtoSshPassphraseWait, ProtoReasonNone, NULL);
        return false;
    }
    m->key_ok = true;
    if((m->conn.flags & PROTO_CONN_WANT_PUB) && m->key_line[0]) {
        BackendEvent e = {.type = BackendEvtKeyPub};
        snprintf(e.key.fp, sizeof(e.key.fp), "%s", m->key_fp);
        e.key.line = m->key_line;
        e.key.len = (uint16_t)strlen(m->key_line);
        emit(m, &e);
    }
    return true;
}

static void connect_check(MockBackend* m) {
    ProtoSshConnect* c = &m->conn;
    if(c->auth == ProtoAuthKey && !key_step(m)) return;
    bool lan_up = (m->net.lan.flags & PROTO_NET_IP) != 0;
    bool wifi_up = (m->net.wifi.flags & PROTO_NET_IP) != 0;
    if(c->transport == ProtoTransportLan && !lan_up) {
        send_state(m, ProtoSshFailed, ProtoReasonNoLink, "LAN cable not connected");
        return;
    }
    if(c->transport == ProtoTransportWifi && !wifi_up) {
        send_state(m, ProtoSshFailed, ProtoReasonNoLink, "WiFi is not connected");
        return;
    }
    if(c->transport == ProtoTransportAuto) {
        if(!lan_up && !wifi_up) {
            send_state(m, ProtoSshFailed, ProtoReasonNoLink, "No network: LAN and WiFi down");
            return;
        }
        c->transport = lan_up ? ProtoTransportLan : ProtoTransportWifi;
    }
    if(host_is(c->host, "timeout") || last_octet_is(c->host, "99")) {
        /* a real TCP connect gives up after a few seconds, not at once */
        schedule(m, 2300, ActFail, ProtoReasonTimeout);
        return;
    }
    if(host_is(c->host, "refused") || c->port == 2) {
        send_state(m, ProtoSshFailed, ProtoReasonRefused, "Connection refused");
        return;
    }
    if(host_is(c->host, "nxdomain")) {
        send_state(m, ProtoSshFailed, ProtoReasonDns, "Host not found");
        return;
    }

    uint32_t variant = host_is_fleet(c->host) ? m->connect_count : 0;
    make_fp(
        hash_str(c->host, 2166136261u) ^ (variant * 2654435761u),
        m->pending_fp,
        sizeof(m->pending_fp));

    if(c->policy != ProtoPolicyOff) {
        MockKnown* k = known_find(m, c->profile, false);
        bool trusted = false;
        if(k)
            for(uint8_t i = 0; i < k->count; i++)
                if(strcmp(k->keys[i].fp, m->pending_fp) == 0) trusted = true;
        if(!trusted) {
            if(k && k->count && c->policy == ProtoPolicyStrict) {
                send_state(
                    m, ProtoSshFailed, ProtoReasonHostkeyRejected, "Host key changed (strict)");
                return;
            }
            BackendEvent e = {.type = BackendEvtHostkeyPrompt};
            e.hostkey.kind = (k && k->count) ? ProtoHostkeyMismatch : ProtoHostkeyUnknown;
            snprintf(e.hostkey.key_type, sizeof(e.hostkey.key_type), "ssh-ed25519");
            snprintf(e.hostkey.fp, sizeof(e.hostkey.fp), "%s", m->pending_fp);
            if(k) {
                e.hostkey.trusted_total = k->count;
                e.hostkey.count = k->count;
                memcpy(e.hostkey.trusted, k->keys, sizeof(ProtoHostkey) * k->count);
            }
            m->ssh_state = ProtoSshHostkeyWait;
            send_state(m, ProtoSshHostkeyWait, ProtoReasonNone, NULL);
            emit(m, &e);
            return;
        }
    }
    schedule(m, 150, ActAuthResult, -1); /* -1: start auth phase */
}

static void auth_phase(MockBackend* m) {
    const ProtoSshConnect* c = &m->conn;
    if(c->auth == ProtoAuthKey) {
        send_state(m, ProtoSshAuthenticating, ProtoReasonNone, NULL);
        schedule(m, 400, ActAuthResult, strcmp(c->user, "nokey") == 0 ? 0 : 1);
        return;
    }
    if(secret_has(m, c->profile)) {
        send_state(m, ProtoSshAuthenticating, ProtoReasonNone, "Using remembered password");
        schedule(m, 400, ActAuthResult, 1);
        return;
    }
    m->ssh_state = ProtoSshPasswordWait;
    send_state(m, ProtoSshPasswordWait, ProtoReasonNone, NULL);
}

static void auth_result(MockBackend* m, int32_t ok) {
    if(ok < 0) {
        auth_phase(m);
        return;
    }
    if(ok) {
        if(m->remember_pending) secret_set(m, m->conn.profile, true);
        m->remember_pending = false;
        send_state(m, ProtoSshReady, ProtoReasonNone, NULL);
        return;
    }
    if(m->conn.auth == ProtoAuthKey && !(m->conn.flags & PROTO_CONN_KEY_FALLBACK)) {
        send_state(m, ProtoSshFailed, ProtoReasonAuthFailed, "Key not accepted by the host");
        return;
    }
    if(m->conn.auth == ProtoAuthKey) {
        m->conn.auth = ProtoAuthPassword; /* Quick Connect: the password, as with OpenSSH */
        m->ssh_state = ProtoSshPasswordWait;
        send_state(m, ProtoSshPasswordWait, ProtoReasonNone, NULL);
        return;
    }
    if(++m->auth_failures >= 3) {
        send_state(m, ProtoSshFailed, ProtoReasonAuthFailed, "Too many authentication failures");
        return;
    }
    m->ssh_state = ProtoSshPasswordWait;
    send_state(m, ProtoSshPasswordWait, ProtoReasonAuthFailed, "Permission denied, try again");
}

/* ---------------- keys ---------------- */

static size_t put_str(uint8_t* p, const void* s, size_t n) {
    p[0] = (uint8_t)(n >> 24);
    p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8);
    p[3] = (uint8_t)n;
    memcpy(p + 4, s, n);
    return 4 + n;
}

/* A structurally real OpenSSH key file around a made-up public key: the Keys screen
 * reads type, size and fingerprint from it like from a real one. Not a usable key. */
static void key_out(MockBackend* m, int32_t kind) {
    uint8_t* bin = calloc(1, 640);
    char* pem = calloc(1, 1024);
    char* line = calloc(1, 200);
    if(!bin || !pem || !line) {
        free(bin);
        free(pem);
        free(line);
        return;
    }
    uint32_t x = 0x5EEDu + m->key_serial * 2654435761u;
    uint8_t pt[65];
    for(size_t i = 0; i < sizeof(pt); i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        pt[i] = (uint8_t)x;
    }
    pt[0] = 4;
    uint8_t pub[120];
    size_t pn = 0;
    if(kind == ProtoKeyGenEcdsaP256) {
        pn += put_str(pub, "ecdsa-sha2-nistp256", 19);
        pn += put_str(pub + pn, "nistp256", 8);
        pn += put_str(pub + pn, pt, 65);
    } else {
        pn += put_str(pub, "ssh-ed25519", 11);
        pn += put_str(pub + pn, pt + 1, 32);
    }
    bool enc = m->keygen.passphrase[0] != '\0';
    size_t n = 15;
    memcpy(bin, "openssh-key-v1", 15);
    n += put_str(bin + n, enc ? "aes256-ctr" : "none", enc ? 10 : 4);
    n += put_str(bin + n, enc ? "bcrypt" : "none", enc ? 6 : 4);
    n += put_str(bin + n, "", 0);
    bin[n + 3] = 1;
    n += 4;
    n += put_str(bin + n, pub, pn);
    n += put_str(bin + n, pt, 64); /* stands in for the private section */
    size_t o = (size_t)snprintf(pem, 1024, "-----BEGIN OPENSSH PRIVATE KEY-----\n");
    char* b64 = line; /* scratch, 200 bytes per 140 input bytes */
    for(size_t i = 0; i < n; i += 48) { /* 64 characters a line, no padding inside */
        size_t k = n - i > 48 ? 48 : n - i;
        keyfmt_b64_encode(bin + i, k, true, b64, 200);
        int r = snprintf(pem + o, 1024 - o, "%s\n", b64);
        if(r < 0 || (size_t)r >= 1024 - o) break; /* never: the file is about 400 bytes */
        o += (size_t)r;
    }
    int r = snprintf(pem + o, 1024 - o, "-----END OPENSSH PRIVATE KEY-----\n");
    if(r > 0 && (size_t)r < 1024 - o) o += (size_t)r;
    BackendEvent e = {.type = BackendEvtKeyBlob};
    e.blob.data = (const uint8_t*)pem;
    e.blob.len = (uint32_t)o;
    e.blob.crc32 = proto_crc32((const uint8_t*)pem, o, 0);
    emit(m, &e);
    BackendEvent k = {.type = BackendEvtKeyPub};
    keyfmt_fingerprint(pub, pn, k.key.fp);
    k.key.len = (uint16_t)keyfmt_pub_line(pub, pn, "ssh_runner", line, 200);
    k.key.line = line;
    if(enc) pp_set(m, k.key.fp, true); /* decrypt once: remembered at generation */
    emit(m, &k);
    memset(&m->keygen, 0, sizeof(m->keygen));
    free(bin);
    free(pem);
    free(line);
}

static bool sec_fail_next;
void mock_backend_sec_fail_next(bool fail) {
    sec_fail_next = fail;
}

/* ---------------- ops ---------------- */

static void mock_free(void* self) {
    MockBackend* m = self;
    gen_reset(m);
    free(m);
}

static void mock_tick(void* self, uint32_t now) {
    MockBackend* m = self;
    m->now = now;
    /* Process everything due; actions may schedule more (possibly due now). */
    for(int guard = 0; guard < 64 && m->queued && m->queue[0].due <= now; guard++) {
        MockItem it = m->queue[0];
        memmove(&m->queue[0], &m->queue[1], sizeof(MockItem) * (size_t)(m->queued - 1));
        m->queued--;
        switch((MockAct)it.act) {
        case ActLinkUp: {
            BackendEvent e = {.type = BackendEvtLink};
            e.link.state = BackendLinkUp;
            e.link.baud = PROTO_BAUD_MAX;
            snprintf(e.link.version, sizeof(e.link.version), "mock 0.1.0");
            emit(m, &e);
            break;
        }
        case ActLinkDown: {
            BackendEvent e = {.type = BackendEvtLink};
            e.link.state = BackendLinkDown;
            emit(m, &e);
            break;
        }
        case ActNetStatus: {
            BackendEvent e = {.type = BackendEvtNetStatus, .net = m->net};
            emit(m, &e);
            break;
        }
        case ActOpDone: {
            BackendEvent e = {.type = BackendEvtOpDone, .op_done = (uint8_t)it.arg};
            emit(m, &e);
            break;
        }
        case ActLinkTest: {
            /* a perfect link: 512-byte frames at 921600 baud, no errors */
            BackendEvent e = {.type = BackendEvtLinkTest};
            e.link_test.done = true;
            e.link_test.frames = e.link_test.sent = (uint32_t)it.arg;
            e.link_test.bytes = (uint32_t)it.arg * PROTO_MAX_PAYLOAD;
            e.link_test.ms = (uint32_t)it.arg * 12u;
            e.link_test.baud = 921600;
            emit(m, &e);
            break;
        }
        case ActSshState:
            if(it.arg == -ProtoSshPassphraseWait)
                send_state(
                    m, ProtoSshPassphraseWait, ProtoReasonBadPassphrase, "Wrong passphrase");
            else
                send_state(m, (uint8_t)it.arg, ProtoReasonNone, NULL);
            break;
        case ActConnectCheck:
            connect_check(m);
            break;
        case ActAuthResult:
            auth_result(m, it.arg);
            break;
        case ActStream:
            do_stream(m);
            break;
        case ActScan: {
            static const struct {
                const char* ssid;
                int8_t rssi;
                uint8_t auth, ch;
            } aps[] = {
                {"Lab-WiFi", -48, 1, 6},
                {"Workshop", -55, 1, 1}, /* the ESP32-S3 is 2.4 GHz only: no 5 GHz names */
                {"Office-AP", -62, 1, 11},
                {"Guest", -70, 0, 1},
                {"DIRECT-printer", -81, 1, 6},
                {"\xe5\xae\xb6\xe7\x9a\x84WiFi", -86, 1, 11}, /* a name the fonts cannot draw */
            };
            const int n = (int)(sizeof(aps) / sizeof(aps[0]));
            /* in two events, as the board sends more than 8 networks */
            const int per = 4;
            for(int first = 0; first < n; first += per) {
                BackendEvent e = {.type = BackendEvtScanResult};
                e.scan.total = (uint8_t)n;
                e.scan.first = (uint8_t)first;
                e.scan.count = (uint8_t)(n - first < per ? n - first : per);
                e.scan.last = first + per >= n;
                for(int i = 0; i < e.scan.count; i++) {
                    ProtoScanEntry* x = &e.scan.entries[i];
                    snprintf(x->ssid, sizeof(x->ssid), "%s", aps[first + i].ssid);
                    x->rssi = aps[first + i].rssi;
                    x->auth = aps[first + i].auth;
                    x->channel = aps[first + i].ch;
                }
                emit(m, &e);
            }
            break;
        }
        case ActJoinDone: {
            m->joining = false;
            if(it.arg) {
                m->net.wifi.flags = PROTO_NET_PRESENT | PROTO_NET_LINK | PROTO_NET_IP;
                if(m->net.wifi.mode == ProtoIpDhcp) {
                    m->net.wifi.ip = 0xC0A80133; /* 192.168.1.51 */
                    m->net.wifi.mask = 0xFFFFFF00;
                    m->net.wifi.gw = 0xC0A80101;
                }
                m->net.wifi_rssi = -52;
                snprintf(m->net.wifi_ssid, sizeof(m->net.wifi_ssid), "%s", m->join_ssid);
            } else {
                m->net.wifi.flags = PROTO_NET_PRESENT;
                m->net.wifi_ssid[0] = '\0';
            }
            /* the board says why first: a refused password (WPA2 4-way handshake), as the
             * mock only fails on the password */
            BackendEvent jr = {.type = BackendEvtWifiJoinResult};
            jr.join.result = it.arg ? ProtoJoinOk : ProtoJoinWrongPassword;
            jr.join.reason = it.arg ? 0 : 15;
            emit(m, &jr);
            BackendEvent e = {.type = BackendEvtNetStatus, .net = m->net};
            emit(m, &e);
            break;
        }
        case ActPingDone: {
            BackendEvent e = {.type = BackendEvtPing};
            e.ping.sent = 4;
            e.ping.received = it.arg ? 4 : 0;
            e.ping.min_ms = it.arg ? 1 : 0;
            e.ping.avg_ms = it.arg ? 2 : 0;
            e.ping.max_ms = it.arg ? 4 : 0;
            /* bound as asked, else auto: the LAN first */
            e.ping.iface = m->ping_transport == ProtoTransportWifi ? ProtoIfaceWifi :
                           m->ping_transport == ProtoTransportLan  ? ProtoIfaceLan :
                           (m->net.lan.flags & PROTO_NET_IP)       ? ProtoIfaceLan :
                                                                     ProtoIfaceWifi;
            emit(m, &e);
            break;
        }
        case ActKeyOut:
            key_out(m, it.arg);
            break;
        case ActHostkeyList: {
            BackendEvent e = {.type = BackendEvtHostkeyList};
            MockKnown* k = known_find(m, m->conn.profile, false);
            if(k) {
                e.hostkeys.count = k->count;
                e.hostkeys.policy_log_count = k->count;
                memcpy(e.hostkeys.keys, k->keys, sizeof(ProtoHostkey) * k->count);
            }
            emit(m, &e);
            break;
        }
        case ActSecStatus: {
            BackendEvent e = {.type = BackendEvtSecStatus};
            e.sec.tier = m->sec_tier;
            e.sec.key_block = m->sec_tier ? 0 : PROTO_SEC_NO_BLOCK;
            e.sec.free_blocks = m->sec_tier ? 5 : 6;
            emit(m, &e);
            break;
        }
        case ActSecPhase: {
            BackendEvent e = {.type = BackendEvtSecState};
            bool failed = it.arg == MOCK_SEC_FAILED;
            e.sec_state.phase = failed ? ProtoSecPhaseMigrate : (uint8_t)it.arg;
            e.sec_state.result = failed                      ? ProtoSecFailed :
                                 it.arg == ProtoSecPhaseDone ? ProtoSecOk :
                                                               ProtoSecRunning;
            e.sec_state.migrated = it.arg >= ProtoSecPhaseErase ? 7 : 0;
            snprintf(
                e.sec_state.msg,
                sizeof(e.sec_state.msg),
                "%s",
                failed                      ? "Protection failed; your data is unchanged." :
                it.arg == ProtoSecPhaseDone ? "Already protected." :
                                              "");
            emit(m, &e);
            break;
        }
        case ActSecProtected:
            m->sec_tier = ProtoSecTierProtect;
            break;
        case ActError:
            send_error(m, (uint8_t)it.arg, ProtoErrState, "Not allowed now");
            break;
        case ActFail:
            send_state(
                m,
                ProtoSshFailed,
                (uint8_t)it.arg,
                it.arg == ProtoReasonHostkeyCancelled ? "Host key not accepted" :
                it.arg == ProtoReasonTimeout          ? "Connection timed out" :
                                                        "");
            break;
        }
    }
}

static void mock_start(void* self) {
    MockBackend* m = self;
    schedule(m, 250, ActLinkUp, 0);
    schedule(m, 300, ActNetStatus, 0);
}

static bool mock_net_get_status(void* self) {
    schedule(self, 30, ActNetStatus, 0);
    return true;
}

static bool mock_net_set_cfg(void* self, const ProtoNetCfg* cfg) {
    MockBackend* m = self;
    ProtoIfaceStatus* s = cfg->iface == ProtoIfaceLan ? &m->net.lan : &m->net.wifi;
    s->mode = cfg->mode;
    if(cfg->mode == ProtoIpStatic) {
        s->ip = cfg->ip;
        s->mask = cfg->mask;
        s->gw = cfg->gw;
    } else if(cfg->iface == ProtoIfaceLan) {
        s->ip = 0xC0A80132;
        s->mask = 0xFFFFFF00;
        s->gw = 0xC0A80101;
    } else if(s->flags & PROTO_NET_LINK) {
        s->ip = 0xC0A80133;
        s->mask = 0xFFFFFF00;
        s->gw = 0xC0A80101;
    }
    schedule(m, 200, ActOpDone, PROTO_T_NET_SET_CFG);
    schedule(m, 400, ActNetStatus, 0);
    return true;
}

static bool mock_wifi_scan(void* self) {
    schedule(self, 1500, ActScan, 0);
    return true;
}

static bool mock_wifi_join(void* self, const ProtoWifiJoin* j) {
    MockBackend* m = self;
    if(m->joining) return false;
    m->joining = true;
    snprintf(m->join_ssid, sizeof(m->join_ssid), "%s", j->ssid);
    bool open = strcmp(j->ssid, "Guest") == 0;
    bool ok = open || (strlen(j->password) >= 8 && strncmp(j->password, "wrong", 5) != 0);
    schedule(m, 2000, ActJoinDone, ok ? 1 : 0);
    return true;
}

static bool mock_ping(void* self, const ProtoPingReq* r) {
    MockBackend* m = self;
    bool ok =
        !(host_is(r->host, "unreachable") || last_octet_is(r->host, "99") ||
          host_is(r->host, "timeout"));
    m->ping_transport = r->transport;
    schedule(self, 900, ActPingDone, ok ? 1 : 0);
    return true;
}

static bool mock_ssh_connect(void* self, const ProtoSshConnect* c) {
    MockBackend* m = self;
    if(m->ssh_state != ProtoSshIdle && m->ssh_state != ProtoSshClosed &&
       m->ssh_state != ProtoSshFailed)
        return false;
    if(c->auth != ProtoAuthKey) m->key_staged = false;
    m->key_ok = false;
    m->conn = *c;
    m->auth_failures = 0;
    m->remember_pending = false;
    m->connect_count++;
    m->ssh_state = ProtoSshConnecting;
    schedule(m, 0, ActSshState, ProtoSshConnecting);
    schedule(m, 700, ActConnectCheck, 0);
    return true;
}

/* Read the key file like the ESP32 stages it: only what the checks need is kept. */
static bool mock_ssh_connect_key(
    void* self,
    const ProtoSshConnect* c,
    uint32_t total,
    BackendScriptRead read,
    void* ctx) {
    MockBackend* m = self;
    if(total == 0 || total > PROTO_KEY_MAX) return false;
    char* head = calloc(1, KEYFMT_SNIFF_BYTES + 1);
    uint8_t* scratch = calloc(1, KEYDIR_SCRATCH);
    if(!head || !scratch) {
        free(head);
        free(scratch);
        return false;
    }
    size_t want = total < KEYFMT_SNIFF_BYTES ? total : KEYFMT_SNIFF_BYTES;
    size_t n = 0;
    while(n < want) {
        size_t k = read(ctx, (uint32_t)n, (uint8_t*)head + n, want - n);
        if(!k) break;
        n += k;
    }
    KeyInfo ki;
    keyfmt_sniff(head, n, &ki, scratch, KEYDIR_SCRATCH);
    m->key_problem = keyfmt_problem(&ki);
    m->key_encrypted = ki.encrypted;
    m->key_line[0] = '\0';
    m->key_fp[0] = '\0';
    if(ki.pub) {
        keyfmt_pub_line(ki.pub, ki.pub_len, NULL, m->key_line, sizeof(m->key_line));
        keyfmt_fingerprint(ki.pub, ki.pub_len, m->key_fp);
    } else if(!m->key_problem) {
        /* a PEM key file: the mock invents the public key the ESP32 would derive */
        make_fp(hash_str(head, 2166136261u), m->key_fp, sizeof(m->key_fp));
        snprintf(
            m->key_line,
            sizeof(m->key_line),
            "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ%.40s",
            m->key_fp + 7);
    }
    memset(head, 0, KEYFMT_SNIFF_BYTES + 1);
    memset(scratch, 0, KEYDIR_SCRATCH);
    free(head);
    free(scratch);
    if(!mock_ssh_connect(self, c)) return false;
    m->key_staged = n == want;
    return true;
}

static bool mock_key_passphrase(void* self, const ProtoKeyPassphrase* k) {
    MockBackend* m = self;
    if(m->ssh_state != ProtoSshPassphraseWait) return false;
    if(!k->passphrase[0] || strcmp(k->passphrase, "wrong") == 0) {
        schedule(m, 300, ActSshState, -ProtoSshPassphraseWait); /* re-prompt, "Wrong passphrase" */
        return true;
    }
    if(k->remember) pp_set(m, m->conn.key_id, true);
    m->key_encrypted = false; /* unlocked for this connect */
    send_state(m, ProtoSshConnecting, ProtoReasonNone, "Unlocking key...");
    schedule(m, 2500, ActConnectCheck, 0); /* the bcrypt KDF, shortened */
    return true;
}

static bool mock_hostkey_decision(void* self, const ProtoHostkeyDecision* d) {
    MockBackend* m = self;
    if(m->ssh_state != ProtoSshHostkeyWait) return false;
    if(d->decision == ProtoDecisionCancel) {
        m->ssh_state = ProtoSshFailed;
        schedule(m, 50, ActFail, ProtoReasonHostkeyCancelled);
        return true;
    }
    if(d->decision == ProtoDecisionAdd || d->decision == ProtoDecisionReplace) {
        MockKnown* k = known_find(m, m->conn.profile, true);
        if(k) {
            if(d->decision == ProtoDecisionReplace) k->count = 0;
            if(k->count >= MOCK_KEYS) {
                memmove(&k->keys[0], &k->keys[1], sizeof(ProtoHostkey) * (MOCK_KEYS - 1));
                k->count--;
            }
            snprintf(k->keys[k->count].fp, sizeof(k->keys[k->count].fp), "%s", m->pending_fp);
            snprintf(k->keys[k->count].label, sizeof(k->keys[k->count].label), "%s", d->label);
            k->count++;
        }
    }
    m->ssh_state = ProtoSshConnecting;
    schedule(m, 200, ActAuthResult, -1);
    return true;
}

static bool mock_auth_password(void* self, const ProtoAuthPasswordMsg* p) {
    MockBackend* m = self;
    if(m->ssh_state != ProtoSshPasswordWait) return false;
    m->ssh_state = ProtoSshAuthenticating;
    schedule(m, 0, ActSshState, ProtoSshAuthenticating);
    m->remember_pending = p->remember != 0;
    bool ok = strcmp(p->password, "wrong") != 0 && p->password[0] != '\0';
    schedule(m, 500, ActAuthResult, ok ? 1 : 0);
    return true;
}

static bool mock_exec(void* self, const char* command) {
    MockBackend* m = self;
    if(m->ssh_state != ProtoSshReady || m->running) return false;
    gen_reset(m);
    char* small = calloc(1, 1024); /* not on the stack: the app thread stack is small */
    if(!small) return false;
    uint32_t sleep_ms = 0;
    m->exit_code = mock_eval(m, command, small, 1024, &m->stream, true, &sleep_ms);
    if(m->gen == GenNone)
        gen_take(m, small);
    else
        free(small);
    m->sleep_until = sleep_ms;
    start_run(m);
    return true;
}

static bool mock_run_script(
    void* self,
    const char* interp,
    uint32_t total,
    BackendScriptRead read,
    void* ctx) {
    MockBackend* m = self;
    if(m->ssh_state != ProtoSshReady || m->running) return false;
    if(total > PROTO_SCRIPT_MAX) return false;
    gen_reset(m);
    char* out = malloc(MOCK_SCRIPT_OUT);
    if(!out) return false;
    out[0] = '\0';
    /* Interpret line by line: each line goes through the same canned rules. */
    char line[256];
    size_t ll = 0;
    uint32_t off = 0;
    int32_t code = 0;
    bool stop = false;
    uint8_t chunk[128];
    (void)interp;
    while(!stop && off < total) {
        size_t n = read(ctx, off, chunk, sizeof(chunk));
        if(!n) break;
        off += (uint32_t)n;
        for(size_t i = 0; i < n && !stop; i++) {
            char ch = (char)chunk[i];
            bool eol = ch == '\n' || (off >= total && i + 1 == n);
            if(ch != '\n' && ch != '\r' && ll < sizeof(line) - 1) line[ll++] = ch;
            if(!eol) continue;
            line[ll] = '\0';
            ll = 0;
            const char* l = line;
            while(*l == ' ' || *l == '\t')
                l++;
            if(!*l || *l == '#') continue;
            if(strncmp(l, "set ", 4) == 0) continue;
            uint8_t st;
            uint32_t sl = 0;
            if(strlen(out) > MOCK_SCRIPT_OUT - 300) break;
            code = mock_eval(m, l, out, MOCK_SCRIPT_OUT, &st, false, &sl);
            if(strncmp(l, "exit", 4) == 0) stop = true;
        }
    }
    m->exit_code = code;
    m->stream = ProtoStreamStdout;
    gen_take(m, out);
    start_run(m);
    return true;
}

static bool mock_flow_credit(void* self, uint32_t bytes) {
    MockBackend* m = self;
    m->credit += bytes;
    return true;
}

static bool mock_cancel(void* self) {
    MockBackend* m = self;
    if(!m->running) return false;
    m->done_flags |= PROTO_DONE_CANCELLED;
    m->exit_code = 130;
    gen_reset(m);
    drop_scheduled(m, ActStream);
    m->stream_scheduled = true;
    schedule(m, 50, ActStream, 0);
    return true;
}

static bool mock_disconnect(void* self) {
    MockBackend* m = self;
    drop_scheduled(m, ActStream);
    drop_scheduled(m, ActConnectCheck);
    drop_scheduled(m, ActAuthResult);
    drop_scheduled(m, ActSshState);
    m->running = false;
    gen_reset(m);
    m->ssh_state = ProtoSshClosed;
    schedule(m, 30, ActSshState, ProtoSshClosed);
    return true;
}

static bool mock_key_gen(void* self, const ProtoKeyGen* g) {
    MockBackend* m = self;
    m->key_serial++;
    m->keygen = *g;
    /* the bcrypt KDF makes an encrypted key slow on the ESP32 */
    schedule(m, g->passphrase[0] ? 2500 : 800, ActKeyOut, g->key_type);
    return true;
}

/* Protect secrets: the ESP32 burns, copies, restarts (link down and up) and comes back
 * protected. */
static bool mock_sec_get_status(void* self) {
    schedule(self, 120, ActSecStatus, 0);
    return true;
}

static bool mock_sec_enable(void* self, const char* word) {
    MockBackend* m = self;
    if(!proto_sec_word_ok(word)) {
        send_error(m, PROTO_T_SEC_ENABLE, ProtoErrBadPayload, "Wrong word");
        return true;
    }
    if(m->sec_tier) {
        schedule(m, 150, ActSecPhase, ProtoSecPhaseDone);
        return true;
    }
    schedule(m, 150, ActSecPhase, ProtoSecPhaseBurn);
    if(sec_fail_next) { /* the derivation self-test fails: nothing moved */
        sec_fail_next = false;
        schedule(m, 1400, ActSecPhase, MOCK_SEC_FAILED);
        return true;
    }
    schedule(m, 1400, ActSecPhase, ProtoSecPhaseMigrate);
    schedule(m, 2800, ActSecPhase, ProtoSecPhaseErase);
    schedule(m, 3200, ActLinkDown, 0);
    schedule(m, 4600, ActSecProtected, 0);
    schedule(m, 4700, ActLinkUp, 0);
    return true;
}

static bool mock_hostkey_list(void* self, const char* profile) {
    MockBackend* m = self;
    snprintf(m->conn.profile, sizeof(m->conn.profile), "%s", profile);
    schedule(m, 100, ActHostkeyList, 0);
    return true;
}

static bool mock_hostkey_delete(void* self, const char* profile, const char* fp) {
    MockBackend* m = self;
    MockKnown* k = known_find(m, profile, false);
    if(!k) return false;
    for(uint8_t i = 0; i < k->count; i++)
        if(strcmp(k->keys[i].fp, fp) == 0) {
            memmove(
                &k->keys[i], &k->keys[i + 1], sizeof(ProtoHostkey) * (size_t)(k->count - i - 1));
            k->count--;
            schedule(m, 100, ActOpDone, PROTO_T_HOSTKEY_DELETE);
            return true;
        }
    return false;
}

static bool mock_secret_forget(void* self, uint8_t kind, const char* profile) {
    MockBackend* m = self;
    if(kind == ProtoForgetPassphrase) {
        pp_set(m, profile, false);
        schedule(m, 50, ActOpDone, PROTO_T_SECRET_FORGET);
        return true;
    }
    secret_set(m, profile, false);
    MockKnown* k = known_find(m, profile, false);
    if(k) memset(k, 0, sizeof(*k));
    schedule(m, 50, ActOpDone, PROTO_T_SECRET_FORGET);
    return true;
}

static bool mock_scenario(void* self, uint8_t scenario) {
    MockBackend* m = self;
    switch(scenario) {
    case MockScenarioLinked:
        m->net.lan.flags = PROTO_NET_PRESENT | PROTO_NET_LINK | PROTO_NET_IP;
        schedule(m, 100, ActLinkUp, 0);
        schedule(m, 150, ActNetStatus, 0);
        return true;
    case MockScenarioNoNetwork:
        m->net.lan.flags = PROTO_NET_PRESENT;
        m->net.wifi.flags = PROTO_NET_PRESENT;
        schedule(m, 100, ActLinkUp, 0);
        schedule(m, 150, ActNetStatus, 0);
        return true;
    case MockScenarioBoardGone:
        schedule(m, 100, ActLinkDown, 0);
        return true;
    default:
        return false;
    }
}

static bool mock_link_test(void* self, uint32_t frames) {
    MockBackend* m = self;
    if(!frames) return false;
    schedule(m, 800, ActLinkTest, (int32_t)frames);
    return true;
}

static const BackendOps mock_ops = {
    .free = mock_free,
    .tick = mock_tick,
    .start = mock_start,
    .net_get_status = mock_net_get_status,
    .net_set_cfg = mock_net_set_cfg,
    .wifi_scan = mock_wifi_scan,
    .wifi_join = mock_wifi_join,
    .ping = mock_ping,
    .ssh_connect = mock_ssh_connect,
    .ssh_connect_key = mock_ssh_connect_key,
    .key_passphrase = mock_key_passphrase,
    .hostkey_decision = mock_hostkey_decision,
    .auth_password = mock_auth_password,
    .exec = mock_exec,
    .run_script = mock_run_script,
    .flow_credit = mock_flow_credit,
    .cancel = mock_cancel,
    .disconnect = mock_disconnect,
    .key_gen = mock_key_gen,
    .sec_get_status = mock_sec_get_status,
    .sec_enable = mock_sec_enable,
    .hostkey_list = mock_hostkey_list,
    .hostkey_delete = mock_hostkey_delete,
    .secret_forget = mock_secret_forget,
    .link_test = mock_link_test,
    .mock_scenario = mock_scenario,
};

Backend mock_backend_create(BackendEventCallback cb, void* ctx) {
    MockBackend* m = calloc(1, sizeof(MockBackend));
    Backend b = {.ops = &mock_ops, .self = m};
    if(!m) return b;
    m->cb = cb;
    m->ctx = ctx;
    m->net.lan.flags = PROTO_NET_PRESENT | PROTO_NET_LINK | PROTO_NET_IP;
    m->net.lan.mode = ProtoIpDhcp;
    m->net.lan.ip = 0xC0A80132; /* 192.168.1.50 */
    m->net.lan.mask = 0xFFFFFF00;
    m->net.lan.gw = 0xC0A80101;
    m->net.wifi.flags = PROTO_NET_PRESENT;
    m->net.wifi.mode = ProtoIpDhcp;
    return b;
}

uint32_t mock_backend_credit(const Backend* b) {
    return ((const MockBackend*)b->self)->credit;
}
