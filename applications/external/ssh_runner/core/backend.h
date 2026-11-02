/*
 * Backend interface: the only way the UI talks to the ESP32.
 *
 * Two implementations:
 *   - mock (core/mock_backend.c): canned, deterministic, pure C;
 *   - uart (backend_uart.c): the real link over USART1 using proto/.
 *
 * All calls are asynchronous and non-blocking; results arrive as BackendEvent via
 * the callback, always from the thread that calls backend_tick() (the GUI thread in
 * the app). Payload types are the protocol structs from proto_msg.h, so a backend
 * event maps 1:1 onto a wire message.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <proto/proto_msg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    BackendEvtLink, /* board presence / version changed */
    BackendEvtNetStatus,
    BackendEvtScanResult,
    BackendEvtPing,
    BackendEvtSshState,
    BackendEvtHostkeyPrompt,
    BackendEvtStream,
    BackendEvtExecDone,
    BackendEvtKeyPub, /* public key line of a generated, exported or parsed key, or a key failure */
    BackendEvtKeyBlob, /* a generated or exported private key file */
    BackendEvtHostkeyList,
    BackendEvtOpDone, /* a fire-and-forget request was accepted (net cfg, delete, ...) */
    BackendEvtError, /* a request failed (ERROR reply or link timeout) */
    BackendEvtLinkTest, /* link test progress / result */
    BackendEvtSecStatus, /* SEC_STATUS */
    BackendEvtSecState, /* SEC_STATE_EVT: Protect secrets progress */
    BackendEvtWifiJoinResult, /* WIFI_JOIN_RESULT: why a join ended (before its NetStatus) */
} BackendEvtType;

typedef enum {
    BackendLinkDown = 0, /* no board answering */
    BackendLinkUp = 1,
    BackendLinkMismatch = 2, /* protocol version differs */
} BackendLinkState;

typedef struct {
    uint8_t state; /* BackendLinkState */
    char version[PROTO_STR_VERSION + 1];
    uint32_t baud;
    /* diagnostics for the About screen (UART backend only) */
    uint8_t port_ok;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    uint32_t rx_dropped; /* bytes lost to a full receive buffer */
} BackendLink;

typedef struct {
    uint8_t request; /* ProtoType of the failed request */
    uint16_t code; /* ProtoErrCode, or 0xFFFF for link timeout */
    char msg[PROTO_STR_MSG + 1];
} BackendError;

#define BACKEND_ERR_TIMEOUT 0xFFFFu

/* KEY_PUB_EVT reassembled. `line` is valid only during the callback. */
typedef struct {
    uint8_t status; /* 0 = ok, else ProtoReason and `line` is the message */
    char fp[PROTO_STR_FP + 1];
    const char* line;
    uint16_t len;
} BackendKeyPub;

/* KEY_BLOB_* from the ESP32, CRC checked. `data` is valid only during the callback. */
typedef struct {
    const uint8_t* data;
    uint32_t len;
    uint32_t crc32;
} BackendKeyBlob;

/* Largest key file the ESP32 sends back (a generated Ed25519 / ECDSA key). */
#define BACKEND_KEY_IN_MAX 4096u

typedef enum {
    MockScenarioLinked = 0, /* board answers, LAN up */
    MockScenarioNoNetwork = 1, /* board answers, no cable, WiFi not joined */
    MockScenarioBoardGone = 2, /* board silent, as if unplugged */
} MockScenario;

/* Link test: `frames` ECHO requests of `payload` pseudo-random bytes each; every reply
 * is compared byte for byte. A corrupted frame the CRC catches costs a retransmit; one
 * that passes the CRC but differs is an undetected error (`mismatches`). */
typedef struct {
    bool done;
    bool aborted; /* link lost: a request timed out after all retries */
    uint32_t frames; /* requested */
    uint32_t sent; /* frames answered so far */
    uint32_t mismatches;
    uint32_t retransmits;
    uint32_t bytes; /* payload bytes each way */
    uint32_t ms;
    uint32_t baud;
} BackendLinkTest;

typedef struct {
    BackendEvtType type;
    union {
        BackendLink link;
        ProtoNetStatus net;
        ProtoScanResult scan;
        ProtoPingResp ping;
        ProtoSshStateEvt ssh;
        ProtoHostkeyPrompt hostkey;
        ProtoStreamData stream; /* data valid only during the callback */
        ProtoExecDone done;
        BackendKeyPub key;
        BackendKeyBlob blob;
        ProtoHostkeyList hostkeys;
        uint8_t op_done; /* ProtoType of the accepted request */
        BackendError error;
        BackendLinkTest link_test;
        ProtoSecStatus sec;
        ProtoSecState sec_state;
        ProtoWifiJoinResult join;
    };
} BackendEvent;

typedef void (*BackendEventCallback)(void* ctx, const BackendEvent* event);

/* Pull-style source for script bytes, so a script never has to sit in RAM whole.
 * Returns bytes read (0 at EOF or on error). */
typedef size_t (*BackendScriptRead)(void* ctx, uint32_t offset, uint8_t* buf, size_t len);

typedef struct {
    void (*free)(void* self);
    void (*tick)(void* self, uint32_t now_ms);
    void (*start)(void* self); /* HELLO / announce link state */
    bool (*net_get_status)(void* self);
    bool (*net_set_cfg)(void* self, const ProtoNetCfg* cfg);
    bool (*wifi_scan)(void* self);
    bool (*wifi_join)(void* self, const ProtoWifiJoin* join);
    bool (*ping)(void* self, const ProtoPingReq* req);
    bool (*ssh_connect)(void* self, const ProtoSshConnect* req);
    /* auth = key: stream the key file (`total` bytes from `read`) as KEY_BLOB_*, then
     * SSH_CONNECT. The ESP32 parses it before opening the connection. */
    bool (*ssh_connect_key)(
        void* self,
        const ProtoSshConnect* req,
        uint32_t total,
        BackendScriptRead read,
        void* read_ctx);
    bool (*key_passphrase)(void* self, const ProtoKeyPassphrase* m);
    bool (*hostkey_decision)(void* self, const ProtoHostkeyDecision* d);
    bool (*auth_password)(void* self, const ProtoAuthPasswordMsg* m);
    bool (*exec)(void* self, const char* command);
    bool (*run_script)(
        void* self,
        const char* interp,
        uint32_t total,
        BackendScriptRead read,
        void* read_ctx);
    bool (*flow_credit)(void* self, uint32_t bytes);
    bool (*cancel)(void* self);
    bool (*disconnect)(void* self);
    bool (*key_gen)(void* self, const ProtoKeyGen* req);
    bool (*hostkey_list)(void* self, const char* profile);
    bool (*hostkey_delete)(void* self, const char* profile, const char* fp);
    bool (*secret_forget)(void* self, uint8_t kind, const char* id); /* ProtoForgetKind */
    bool (*link_test)(void* self, uint32_t frames);
    bool (*sec_get_status)(void* self);
    bool (*sec_enable)(void* self, const char* word); /* tier 1, the typed word */
    /* Mock only (NULL on the UART backend): board states for demos and screenshots. */
    bool (*mock_scenario)(void* self, uint8_t scenario);
} BackendOps;

typedef struct {
    const BackendOps* ops;
    void* self;
} Backend;

/* Convenience wrappers */
static inline void backend_tick(Backend* b, uint32_t now) {
    b->ops->tick(b->self, now);
}
static inline void backend_free(Backend* b) {
    b->ops->free(b->self);
}

/* ---- mock backend (pure C) ---- */
typedef struct MockBackend MockBackend;
Backend mock_backend_create(BackendEventCallback cb, void* ctx);
/* Test hooks */
uint32_t mock_backend_credit(const Backend* b);
/* Demo/tests: the next Protect secrets fails after the burn, as the ESP32 reports it. */
void mock_backend_sec_fail_next(bool fail);
