/*
 * SSH Runner UART protocol — message catalogue and payload codecs.
 * See docs/PROTOCOL.md §3–§5. Every multi-byte integer is little-endian.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include "proto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Message types (bit 7 clear = reliable request/event, set = reply) ---- */
typedef enum {
    /* Lifecycle / housekeeping */
    PROTO_T_HELLO = 0x01, /* F->E req, reply HELLO_RESP */
    PROTO_T_FLOW_CREDIT = 0x02, /* F->E req, reply ACK */
    PROTO_T_CANCEL = 0x03, /* F->E req, reply ACK */
    PROTO_T_LOG_EVT = 0x04, /* E->F evt (debug builds only) */
    PROTO_T_ECHO = 0x05, /* F->E req (link test), reply ECHO_RESP with the same payload */

    /* Network */
    PROTO_T_NET_SET_CFG = 0x10, /* F->E req, reply ACK/ERROR */
    PROTO_T_NET_GET_STATUS = 0x11, /* F->E req, reply NET_STATUS */
    PROTO_T_NET_STATUS_EVT = 0x12, /* E->F evt */
    PROTO_T_WIFI_SCAN = 0x13, /* F->E req, reply ACK, then WIFI_SCAN_RESULT evts */
    PROTO_T_WIFI_SCAN_RESULT = 0x14, /* E->F evt */
    PROTO_T_WIFI_JOIN = 0x15, /* F->E req, reply ACK, result via NET_STATUS_EVT */
    PROTO_T_PING_REQ = 0x16, /* F->E req, reply ACK, then PING_RESP evt */
    PROTO_T_PING_RESP = 0x17, /* E->F evt */
    PROTO_T_WIFI_JOIN_RESULT =
        0x18, /* E->F evt: why a join ended, just before its NET_STATUS_EVT */

    /* SSH session */
    PROTO_T_SSH_CONNECT = 0x20, /* F->E req, reply ACK, then SSH_STATE_EVT ... */
    PROTO_T_SSH_STATE_EVT = 0x21, /* E->F evt */
    PROTO_T_HOSTKEY_PROMPT_EVT = 0x22, /* E->F evt */
    PROTO_T_HOSTKEY_DECISION = 0x23, /* F->E req, reply ACK */
    PROTO_T_HOSTKEY_LIST = 0x24, /* F->E req, reply HOSTKEY_LIST_RESP */
    PROTO_T_HOSTKEY_DELETE = 0x25, /* F->E req, reply ACK/ERROR */
    PROTO_T_AUTH_PASSWORD = 0x26, /* F->E req, reply ACK */
    PROTO_T_SSH_DISCONNECT = 0x27, /* F->E req, reply ACK */

    /* Run */
    PROTO_T_EXEC_REQ = 0x30, /* F->E req, reply ACK, then STREAM_DATA_EVT... EXEC_DONE_EVT */
    PROTO_T_STREAM_DATA_EVT = 0x31, /* E->F evt, consumes flow credit */
    PROTO_T_EXEC_DONE_EVT = 0x32, /* E->F evt */
    PROTO_T_SCRIPT_BEGIN = 0x33, /* F->E req, reply ACK/ERROR */
    PROTO_T_SCRIPT_CHUNK = 0x34, /* F->E req, reply ACK/ERROR */
    PROTO_T_SCRIPT_END = 0x35, /* F->E req, reply ACK/ERROR, then STREAM/EXEC_DONE */

    /* Keys and secrets. Key files live on the Flipper's SD card; the ESP32 only
     * sees one while it authenticates or generates it. 0x41 was KEY_GET_PUB (v0). */
    PROTO_T_KEY_GEN = 0x40, /* F->E req, reply ACK, then KEY_BLOB_* evts and KEY_PUB_EVT */
    PROTO_T_SECRET_FORGET = 0x42, /* F->E req, reply ACK */
    PROTO_T_KEY_BLOB_BEGIN = 0x43, /* F->E req (stage a key file) or E->F evt (generated key) */
    PROTO_T_KEY_BLOB_CHUNK = 0x44, /* same directions, payload like SCRIPT_CHUNK */
    PROTO_T_KEY_BLOB_END = 0x45, /* same directions, payload like SCRIPT_END */
    PROTO_T_KEY_PASSPHRASE = 0x46, /* F->E req, reply ACK; answers state passphrase_wait */
    PROTO_T_KEY_PUB_EVT = 0x47, /* E->F evt: public key line and fingerprint, chunked */
    /* 0x48 (was KEY_LEGACY_EXPORT) and 0x49 (was KEY_LEGACY_DELETE): retired, never reuse */

    /* Security: the eFuse opt-in */
    PROTO_T_SEC_GET_STATUS = 0x60, /* F->E req, reply SEC_STATUS */
    PROTO_T_SEC_ENABLE = 0x61, /* F->E req {tier, word}, reply ACK then SEC_STATE_EVTs, or ERROR */
    PROTO_T_SEC_STATE_EVT = 0x62, /* E->F evt: phase and result of SEC_ENABLE */

    /* Shell (reserved) */
    PROTO_T_SHELL_OPEN = 0x50,
    PROTO_T_SHELL_INPUT = 0x51,
    PROTO_T_SHELL_CLOSE = 0x52,

    /* Replies */
    PROTO_T_HELLO_RESP = PROTO_T_HELLO | PROTO_REPLY_BIT, /* 0x81 */
    PROTO_T_ECHO_RESP = PROTO_T_ECHO | PROTO_REPLY_BIT, /* 0x85 */
    PROTO_T_NET_STATUS = PROTO_T_NET_GET_STATUS |
                         PROTO_REPLY_BIT, /* 0x91, payload = NET_STATUS_EVT */
    PROTO_T_HOSTKEY_LIST_RESP = PROTO_T_HOSTKEY_LIST | PROTO_REPLY_BIT, /* 0xA4 */
    PROTO_T_SEC_STATUS = PROTO_T_SEC_GET_STATUS | PROTO_REPLY_BIT, /* 0xE0 */
    PROTO_T_ERROR = 0xFE,
    PROTO_T_ACK = 0xFF,
} ProtoType;

/* ---- Field limits (bytes, excluding NUL) ---- */
#define PROTO_STR_VERSION      23
#define PROTO_STR_PROFILE      47 /* alias, or "host:port" for Quick Connect */
#define PROTO_STR_USER         32
#define PROTO_STR_HOST         63
#define PROTO_STR_PASSWORD     63
#define PROTO_STR_SSID         32
#define PROTO_STR_WIFI_PASS    63
#define PROTO_STR_FP           63 /* "SHA256:" + 43 base64 chars */
#define PROTO_STR_KEYTYPE      15
#define PROTO_STR_LABEL        15
#define PROTO_STR_MSG          63
#define PROTO_STR_INTERP       15
#define PROTO_STR_PASSPHRASE   63
#define PROTO_STR_KEYID        63 /* key fingerprint "SHA256:...", or "file:SHA256:..." */
#define PROTO_STR_COMMAND      PROTO_MAX_PAYLOAD /* EXEC_REQ payload is the raw command */
#define PROTO_MAX_TRUSTED_FP   4
#define PROTO_MAX_SCAN_ENTRIES 8
#define PROTO_MAX_HOSTKEYS     4
#define PROTO_STREAM_CHUNK     (PROTO_MAX_PAYLOAD - 1u)
#define PROTO_SCRIPT_CHUNK     (PROTO_MAX_PAYLOAD - 4u)
#define PROTO_SCRIPT_MAX       (64u * 1024u)
#define PROTO_KEY_MAX          (16u * 1024u) /* largest key file accepted */
#define PROTO_PUB_MAX          2048u /* longest public key line (RSA 8192 does not fit) */
#define PROTO_PUB_CHUNK        (PROTO_MAX_PAYLOAD - 1u - 1u - PROTO_STR_FP - 4u)

/* ---- Enumerations ---- */
typedef enum {
    ProtoIfaceLan = 0,
    ProtoIfaceWifi = 1
} ProtoIface;
typedef enum {
    ProtoTransportAuto = 0,
    ProtoTransportLan = 1,
    ProtoTransportWifi = 2
} ProtoTransport;
typedef enum {
    ProtoAuthPassword = 0,
    ProtoAuthKey = 1
} ProtoAuth;
typedef enum {
    ProtoPolicyPrompt = 0,
    ProtoPolicyStrict = 1,
    ProtoPolicyOff = 2
} ProtoPolicy;
typedef enum {
    ProtoIpDhcp = 0,
    ProtoIpStatic = 1
} ProtoIpMode;
typedef enum {
    ProtoHostkeyUnknown = 0,
    ProtoHostkeyMismatch = 1
} ProtoHostkeyKind;
typedef enum {
    ProtoDecisionCancel = 0,
    ProtoDecisionOnce = 1,
    ProtoDecisionAdd = 2,
    ProtoDecisionReplace = 3,
} ProtoDecision;
typedef enum {
    ProtoStreamStdout = 0,
    ProtoStreamStderr = 1
} ProtoStream;

typedef enum {
    ProtoSshIdle = 0,
    ProtoSshConnecting = 1, /* TCP + key exchange */
    ProtoSshHostkeyWait = 2, /* HOSTKEY_PROMPT_EVT sent, waiting for HOSTKEY_DECISION */
    ProtoSshPasswordWait = 3, /* waiting for AUTH_PASSWORD */
    ProtoSshAuthenticating = 4,
    ProtoSshReady = 5, /* authenticated, ready for EXEC/SCRIPT */
    ProtoSshRunning = 6,
    ProtoSshFailed = 7, /* see reason; session is gone */
    ProtoSshClosed = 8,
    ProtoSshPassphraseWait = 9, /* key is encrypted: waiting for KEY_PASSPHRASE */
} ProtoSshState;

typedef enum {
    ProtoReasonNone = 0,
    ProtoReasonNoLink = 1,
    ProtoReasonDns = 2,
    ProtoReasonRefused = 3,
    ProtoReasonTimeout = 4,
    ProtoReasonHostkeyRejected = 5, /* strict policy mismatch */
    ProtoReasonHostkeyCancelled = 6,
    ProtoReasonAuthFailed = 7,
    ProtoReasonNoKey = 8, /* auth = key but no key file staged */
    ProtoReasonProtocol = 9,
    ProtoReasonPeerClosed = 10,
    ProtoReasonBusy = 11,
    ProtoReasonInternal = 12,
    ProtoReasonBadPassphrase = 13, /* passphrase_wait again: the last one was wrong */
    ProtoReasonBadKey = 14, /* type, format or size not supported; msg says which */
} ProtoReason;

typedef enum {
    ProtoKeyGenEd25519 = 0,
    ProtoKeyGenEcdsaP256 = 1
} ProtoKeyGenType;
typedef enum {
    ProtoForgetProfile = 0, /* remembered password and pinned host keys */
    ProtoForgetPassphrase = 1, /* a key's remembered passphrase, and its unlocked copy in RAM */
    ProtoForgetUnlocked = 2, /* only the unlocked copy in RAM ("lock", like ssh-add -d) */
} ProtoForgetKind;

typedef enum {
    ProtoErrNone = 0,
    ProtoErrUnknownType = 1,
    ProtoErrBadPayload = 2,
    ProtoErrBusy = 3,
    ProtoErrState = 4, /* request not valid in the current state */
    ProtoErrTooLarge = 5,
    ProtoErrNotFound = 6,
    ProtoErrInternal = 7,
} ProtoErrCode;

/* EXEC_DONE flags */
#define PROTO_DONE_SIGNAL    0x01u /* terminated by signal; exit_code holds the signal number */
#define PROTO_DONE_CANCELLED 0x02u
#define PROTO_DONE_LOST      0x04u /* connection lost before an exit status arrived */

/* SSH_CONNECT flags */
#define PROTO_CONN_REMEMBER 0x01u /* store the password in ESP32 NVS on success */
#define PROTO_CONN_WANT_PUB \
    0x02u /* auth = key, no .pub on the card: send KEY_PUB_EVT after parsing */
#define PROTO_CONN_KEY_FALLBACK \
    0x04u /* auth = key, Quick Connect: a rejected key falls back to password_wait */

/* NET_STATUS flags per interface */
#define PROTO_NET_PRESENT 0x01u /* hardware present */
#define PROTO_NET_LINK    0x02u /* link up (cable / associated) */
#define PROTO_NET_IP      0x04u /* has an IPv4 address */

/* ---- Payload structs ---- */
typedef struct {
    uint8_t proto_version;
    uint32_t max_baud;
    char version[PROTO_STR_VERSION + 1];
} ProtoHello;

typedef struct {
    uint8_t proto_version;
    uint32_t max_baud;
    uint32_t baud; /* chosen baud; both sides switch after this reply */
    char version[PROTO_STR_VERSION + 1];
} ProtoHelloResp;

typedef struct {
    uint32_t bytes;
} ProtoFlowCredit;

typedef struct {
    uint8_t level;
    char msg[PROTO_STR_MSG + 1];
} ProtoLog;

typedef struct {
    uint16_t code; /* ProtoErrCode */
    char msg[PROTO_STR_MSG + 1];
} ProtoError;

typedef struct {
    uint8_t iface; /* ProtoIface */
    uint8_t mode; /* ProtoIpMode */
    uint32_t ip, mask, gw, dns; /* network byte order packed as a.b.c.d -> 0xaabbccdd */
} ProtoNetCfg;

typedef struct {
    uint8_t flags; /* PROTO_NET_* */
    uint8_t mode; /* ProtoIpMode */
    uint32_t ip, mask, gw;
} ProtoIfaceStatus;

typedef struct {
    ProtoIfaceStatus lan;
    ProtoIfaceStatus wifi;
    int8_t wifi_rssi;
    char wifi_ssid[PROTO_STR_SSID + 1];
} ProtoNetStatus;

typedef struct {
    char ssid[PROTO_STR_SSID + 1];
    int8_t rssi;
    uint8_t auth; /* 0 = open, otherwise secured */
    uint8_t channel;
} ProtoScanEntry;

typedef struct {
    uint8_t total; /* total networks found */
    uint8_t first; /* index of entries[0] */
    uint8_t count;
    uint8_t last; /* 1 on the final event */
    ProtoScanEntry entries[PROTO_MAX_SCAN_ENTRIES];
} ProtoScanResult;

typedef struct {
    char ssid[PROTO_STR_SSID + 1];
    char password[PROTO_STR_WIFI_PASS + 1];
} ProtoWifiJoin;

typedef struct {
    char host[PROTO_STR_HOST + 1];
    uint8_t count;
    uint8_t transport; /* ProtoTransport */
} ProtoPingReq;

typedef struct {
    uint8_t sent;
    uint8_t received;
    uint16_t min_ms, avg_ms, max_ms;
    uint8_t iface; /* interface actually used */
} ProtoPingResp;

/* How a WiFi join ended, as a phone says it: the password, the network, or something else
 * (weak signal, the router). `reason` is the 802.11 / ESP-IDF disconnect reason, for logs. */
typedef enum {
    ProtoJoinOk = 0,
    ProtoJoinWrongPassword = 1,
    ProtoJoinNotFound = 2,
    ProtoJoinFailed = 3,
} ProtoJoinResult;

typedef struct {
    uint8_t result; /* ProtoJoinResult */
    uint8_t reason;
} ProtoWifiJoinResult;

typedef struct {
    char profile[PROTO_STR_PROFILE + 1];
    char user[PROTO_STR_USER + 1];
    char host[PROTO_STR_HOST + 1];
    uint16_t port;
    uint8_t transport; /* ProtoTransport */
    uint8_t auth; /* ProtoAuth */
    uint8_t policy; /* ProtoPolicy */
    uint8_t flags; /* PROTO_CONN_* */
    char key_id[PROTO_STR_KEYID + 1]; /* auth = key: names the remembered passphrase */
} ProtoSshConnect;

typedef struct {
    uint8_t state; /* ProtoSshState */
    uint8_t reason; /* ProtoReason */
    uint8_t iface; /* interface in use once connected */
    uint8_t stored_secret; /* 1 when a remembered password exists for the profile */
    char msg[PROTO_STR_MSG + 1];
} ProtoSshStateEvt;

typedef struct {
    char fp[PROTO_STR_FP + 1];
    char label[PROTO_STR_LABEL + 1];
} ProtoHostkey;

typedef struct {
    uint8_t kind; /* ProtoHostkeyKind */
    char key_type[PROTO_STR_KEYTYPE + 1];
    char fp[PROTO_STR_FP + 1];
    uint8_t trusted_total; /* number of trusted keys; may exceed `count` */
    uint8_t count;
    ProtoHostkey trusted[PROTO_MAX_TRUSTED_FP];
} ProtoHostkeyPrompt;

typedef struct {
    uint8_t decision; /* ProtoDecision */
    char label[PROTO_STR_LABEL + 1];
} ProtoHostkeyDecision;

typedef struct {
    char profile[PROTO_STR_PROFILE + 1];
} ProtoProfileRef; /* HOSTKEY_LIST */

typedef struct {
    uint8_t kind; /* ProtoForgetKind */
    char id[PROTO_STR_KEYID + 1]; /* profile name, or key id */
} ProtoSecretForget;

/* ---- Security ---- */
#define PROTO_SEC_WORD     "confirm" /* typed on the Flipper; compared trimmed, any case */
#define PROTO_STR_SEC_WORD 31
#define PROTO_SEC_NO_BLOCK 0xFFu

typedef enum {
    ProtoSecTierNone = 0, /* plaintext NVS (every board ships like this) */
    ProtoSecTierProtect = 1, /* "Protect secrets": HMAC eFuse key, encrypted NVS */
    ProtoSecTierLock = 2, /* "Lock device" (not offered) */
} ProtoSecTier;

typedef enum {
    ProtoSecPhaseCheck = 0, /* word, session, free key block */
    ProtoSecPhaseBurn = 1, /* writing the HMAC key into an eFuse key block */
    ProtoSecPhaseMigrate = 2, /* copying the saved secrets into encrypted NVS */
    ProtoSecPhaseErase = 3, /* erasing the plaintext copy */
    ProtoSecPhaseDone = 4,
} ProtoSecPhase;

typedef enum {
    ProtoSecRunning = 0,
    ProtoSecOk = 1,
    ProtoSecFailed = 2,
} ProtoSecResult;

typedef struct {
    uint8_t tier; /* ProtoSecTier now in force */
    uint8_t key_block; /* eFuse key block holding the HMAC key, or PROTO_SEC_NO_BLOCK */
    uint8_t free_blocks; /* unused eFuse key blocks */
    uint8_t efuse_virtual; /* 1: a test build with virtual eFuses (nothing real is burnt) */
} ProtoSecStatus;

typedef struct {
    uint8_t tier; /* ProtoSecTier to raise to */
    char word[PROTO_STR_SEC_WORD + 1];
} ProtoSecEnable;

typedef struct {
    uint8_t phase; /* ProtoSecPhase */
    uint8_t result; /* ProtoSecResult */
    uint8_t migrated; /* entries copied so far (Migrate) or in total (Done) */
    char msg[PROTO_STR_MSG + 1]; /* on failure: which step, and the board's state */
} ProtoSecState;

/* The typed word check, shared by both firmwares so they cannot drift: `word` with
 * surrounding whitespace trimmed equals "confirm", case-insensitively. */
bool proto_sec_word_ok(const char* word);

typedef struct {
    uint8_t policy_log_count; /* audited non-cancel decisions recorded */
    uint8_t count;
    ProtoHostkey keys[PROTO_MAX_HOSTKEYS];
} ProtoHostkeyList;

typedef struct {
    char profile[PROTO_STR_PROFILE + 1];
    char fp[PROTO_STR_FP + 1];
} ProtoHostkeyDelete;

typedef struct {
    char password[PROTO_STR_PASSWORD + 1];
    uint8_t remember;
} ProtoAuthPasswordMsg;

typedef struct {
    uint16_t len;
    const char* command; /* not NUL-terminated on decode; points into the payload */
} ProtoExecReq;

typedef struct {
    uint8_t stream; /* ProtoStream */
    uint16_t len;
    const uint8_t* data; /* points into the payload */
} ProtoStreamData;

typedef struct {
    int32_t exit_code;
    uint8_t flags; /* PROTO_DONE_* */
    uint32_t duration_ms;
} ProtoExecDone;

typedef struct {
    char interp[PROTO_STR_INTERP + 1]; /* "sh", "bash", ... */
    uint32_t total;
} ProtoScriptBegin;

typedef struct {
    uint32_t offset;
    uint16_t len;
    const uint8_t* data;
} ProtoScriptChunk;

typedef struct {
    uint32_t crc32; /* IEEE CRC-32 of the whole script */
} ProtoScriptEnd;

typedef struct {
    uint8_t key_type; /* ProtoKeyGenType */
    char passphrase[PROTO_STR_PASSPHRASE + 1]; /* empty = unencrypted */
} ProtoKeyGen;

typedef struct {
    uint32_t total; /* 1 .. PROTO_KEY_MAX */
} ProtoKeyBlobBegin; /* KEY_BLOB_CHUNK / _END use ProtoScriptChunk / ProtoScriptEnd */

typedef struct {
    char passphrase[PROTO_STR_PASSPHRASE + 1];
    uint8_t remember; /* store it in NVS by key id on success ("decrypt once") */
} ProtoKeyPassphrase;

typedef struct {
    uint8_t status; /* 0 = ok, else ProtoReason and `data` is a message */
    char fp[PROTO_STR_FP + 1];
    uint16_t total; /* length of the public key line (or message) */
    uint16_t offset; /* of this piece */
    uint16_t len;
    const uint8_t* data; /* points into the payload */
} ProtoKeyPub;

/* ---- Low-level field writer / reader ---- */
typedef struct {
    uint8_t* buf;
    size_t cap;
    size_t len;
    bool err;
} ProtoWriter;

typedef struct {
    const uint8_t* buf;
    size_t len;
    size_t pos;
    bool err;
} ProtoReader;

void pw_init(ProtoWriter* w, uint8_t* buf, size_t cap);
void pw_u8(ProtoWriter* w, uint8_t v);
void pw_u16(ProtoWriter* w, uint16_t v);
void pw_u32(ProtoWriter* w, uint32_t v);
void pw_i32(ProtoWriter* w, int32_t v);
void pw_str(ProtoWriter* w, const char* s); /* u8 length + bytes */
void pw_bytes(ProtoWriter* w, const void* data, size_t len);
size_t pw_finish(const ProtoWriter* w); /* length, or 0 on error */

void pr_init(ProtoReader* r, const uint8_t* buf, size_t len);
uint8_t pr_u8(ProtoReader* r);
uint16_t pr_u16(ProtoReader* r);
uint32_t pr_u32(ProtoReader* r);
int32_t pr_i32(ProtoReader* r);
void pr_str(ProtoReader* r, char* out, size_t out_size); /* error if it does not fit */
const uint8_t* pr_rest(ProtoReader* r, size_t* len);
bool pr_done(const ProtoReader* r); /* no error and everything consumed */

/* ---- Typed codecs ----
 * enc_* return the payload length (0 means error; note some payloads may
 * legitimately be empty, those messages have no codec). dec_* return true on success
 * and reject trailing bytes. */
#define PROTO_CODEC(name, type)                                       \
    size_t proto_enc_##name(const type* m, uint8_t* buf, size_t cap); \
    bool proto_dec_##name(const uint8_t* buf, size_t len, type* m);

PROTO_CODEC(hello, ProtoHello)
PROTO_CODEC(hello_resp, ProtoHelloResp)
PROTO_CODEC(flow_credit, ProtoFlowCredit)
PROTO_CODEC(log, ProtoLog)
PROTO_CODEC(error, ProtoError)
PROTO_CODEC(net_cfg, ProtoNetCfg)
PROTO_CODEC(net_status, ProtoNetStatus)
PROTO_CODEC(scan_result, ProtoScanResult)
PROTO_CODEC(wifi_join, ProtoWifiJoin)
PROTO_CODEC(ping_req, ProtoPingReq)
PROTO_CODEC(ping_resp, ProtoPingResp)
PROTO_CODEC(wifi_join_result, ProtoWifiJoinResult)
PROTO_CODEC(ssh_connect, ProtoSshConnect)
PROTO_CODEC(ssh_state, ProtoSshStateEvt)
PROTO_CODEC(hostkey_prompt, ProtoHostkeyPrompt)
PROTO_CODEC(hostkey_decision, ProtoHostkeyDecision)
PROTO_CODEC(profile_ref, ProtoProfileRef)
PROTO_CODEC(hostkey_list, ProtoHostkeyList)
PROTO_CODEC(hostkey_delete, ProtoHostkeyDelete)
PROTO_CODEC(auth_password, ProtoAuthPasswordMsg)
PROTO_CODEC(exec_req, ProtoExecReq)
PROTO_CODEC(stream_data, ProtoStreamData)
PROTO_CODEC(exec_done, ProtoExecDone)
PROTO_CODEC(script_begin, ProtoScriptBegin)
PROTO_CODEC(script_chunk, ProtoScriptChunk)
PROTO_CODEC(script_end, ProtoScriptEnd)
PROTO_CODEC(key_gen, ProtoKeyGen)
PROTO_CODEC(key_blob_begin, ProtoKeyBlobBegin)
PROTO_CODEC(key_passphrase, ProtoKeyPassphrase)
PROTO_CODEC(key_pub, ProtoKeyPub)
PROTO_CODEC(secret_forget, ProtoSecretForget)
PROTO_CODEC(sec_status, ProtoSecStatus)
PROTO_CODEC(sec_enable, ProtoSecEnable)
PROTO_CODEC(sec_state, ProtoSecState)

#undef PROTO_CODEC

/* IEEE 802.3 CRC-32 (reflected, poly 0xEDB88320), for SCRIPT_END. Start with 0. */
uint32_t proto_crc32(const uint8_t* data, size_t len, uint32_t crc);

/* Human-readable name of a type, for logs. Never NULL. */
const char* proto_type_name(uint8_t type);

#ifdef __cplusplus
}
#endif
