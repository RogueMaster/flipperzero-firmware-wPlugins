/*
 * meshcore_companion.h
 *
 * Portable C99 client for the MeshCore Companion Radio serial protocol.
 *
 * This core has NO I/O, NO dynamic allocation and NO Arduino dependency.
 * You feed it bytes received from the radio and it hands you decoded frames;
 * you ask it to build command frames and it writes them into a buffer you own.
 * The transport (UART, USB-CDC, TCP, a unit-test harness) is entirely yours.
 *
 * Wire format (verified against meshcore.js, MIT, (c) Liam Cottle):
 *   frame = [type:u8][len:u16 LE][payload:len bytes]
 *   type 0x3C ('<')  app  -> radio   (commands we send)
 *   type 0x3E ('>')  radio -> app    (responses / push notifications we receive)
 *   payload[0] is the command code (outbound) or response/push code (inbound).
 *
 * SPDX-License-Identifier: MIT
 * Author: Scott Penrose / Digital Dimensions.
 * Protocol reference: https://github.com/meshcore-dev/meshcore.js
 */
#ifndef MESHCORE_COMPANION_H
#define MESHCORE_COMPANION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library version. Keep in sync with library.json and library.properties;
 * check_version.sh verifies all three match and that a git tag exists. */
#define MESHCORE_COMPANION_VERSION "0.2.2"

/* ---- Compile-time sizing (override before including if you need more) ---- */
#ifndef MC_MAX_PAYLOAD
#define MC_MAX_PAYLOAD   255   /* largest companion payload we will buffer    */
#endif
#ifndef MC_MAX_TEXT
#define MC_MAX_TEXT      184   /* max message text we keep (incl. NUL)        */
#endif
#ifndef MC_MAX_DATA
#define MC_MAX_DATA      184   /* max channel-data payload we keep            */
#endif
#ifndef MC_MAX_MODEL
#define MC_MAX_MODEL     48    /* DeviceInfo model/manufacturer string        */
#endif
#define MC_NAME_LEN      32    /* channel / advert name field width (cstring) */
#define MC_SECRET_LEN    16    /* 128-bit channel secret (PSK)                */
#define MC_RX_BUFSZ      (MC_MAX_PAYLOAD + 8)

/* ---- Frame lead bytes ---- */
#define MC_FRAME_APP_TO_RADIO  0x3C
#define MC_FRAME_RADIO_TO_APP  0x3E

/* ---- Command codes (app -> radio) ---- */
enum {
    MC_CMD_APP_START            = 1,
    MC_CMD_SEND_TXT_MSG         = 2,
    MC_CMD_SEND_CHANNEL_TXT_MSG = 3,
    MC_CMD_GET_CONTACTS         = 4,
    MC_CMD_GET_DEVICE_TIME      = 5,
    MC_CMD_SET_DEVICE_TIME      = 6,
    MC_CMD_SEND_SELF_ADVERT     = 7,
    MC_CMD_SET_ADVERT_NAME      = 8,
    MC_CMD_ADD_UPDATE_CONTACT   = 9,
    MC_CMD_SYNC_NEXT_MESSAGE    = 10,
    MC_CMD_SET_RADIO_PARAMS     = 11,
    MC_CMD_SET_TX_POWER         = 12,
    MC_CMD_RESET_PATH           = 13,
    MC_CMD_SET_ADVERT_LATLON    = 14,
    MC_CMD_REMOVE_CONTACT       = 15,
    MC_CMD_SHARE_CONTACT        = 16,
    MC_CMD_EXPORT_CONTACT       = 17,
    MC_CMD_IMPORT_CONTACT       = 18,
    MC_CMD_DEVICE_QUERY         = 22,
    MC_CMD_GET_CONTACT_BY_KEY   = 30,
    MC_CMD_GET_CHANNEL          = 31,
    MC_CMD_SET_CHANNEL          = 32,
    MC_CMD_SEND_BINARY_REQ      = 50,
    MC_CMD_GET_STATS            = 56,
    MC_CMD_SEND_ANON_REQ        = 57
};

/* ---- Binary-request subtypes (SEND_BINARY_REQ data[0]) ---- */
enum {
    MC_BINARY_REQ_STATUS     = 0x01,
    MC_BINARY_REQ_KEEP_ALIVE = 0x02,
    MC_BINARY_REQ_TELEMETRY  = 0x03,
    MC_BINARY_REQ_MMA        = 0x04,
    MC_BINARY_REQ_ACL        = 0x05,
    MC_BINARY_REQ_NEIGHBOURS = 0x06
};
/* ---- Anonymous-request subtypes (SEND_ANON_REQ) ---- */
enum { MC_ANON_REQ_REGIONS = 0x01, MC_ANON_REQ_OWNER = 0x02, MC_ANON_REQ_BASIC = 0x03 };

/* ---- Response codes (radio -> app) ---- */
enum {
    MC_RESP_OK               = 0,
    MC_RESP_ERR              = 1,
    MC_RESP_CONTACTS_START   = 2,
    MC_RESP_CONTACT          = 3,
    MC_RESP_END_OF_CONTACTS  = 4,
    MC_RESP_SELF_INFO        = 5,
    MC_RESP_SENT             = 6,
    MC_RESP_CONTACT_MSG_RECV = 7,
    MC_RESP_CHANNEL_MSG_RECV = 8,
    MC_RESP_CURR_TIME        = 9,
    MC_RESP_NO_MORE_MESSAGES = 10,
    MC_RESP_CONTACT_URI      = 11,     /* reply to EXPORT_CONTACT */
    MC_RESP_BATTERY_VOLTAGE  = 12,
    MC_RESP_DEVICE_INFO      = 13,
    MC_RESP_PRIVATE_KEY      = 14,
    MC_RESP_DISABLED         = 15,
    MC_RESP_CONTACT_MSG_RECV_V3 = 16,  /* SNR-prefixed variant of code 7 */
    MC_RESP_CHANNEL_MSG_RECV_V3 = 17,  /* SNR-prefixed variant of code 8 */
    MC_RESP_CHANNEL_INFO     = 18,
    MC_RESP_ADVERT_PATH      = 22,
    MC_RESP_STATS            = 24,
    MC_RESP_AUTOADD_CONFIG   = 25,
    MC_RESP_ALLOWED_REPEAT_FREQ = 26,
    MC_RESP_CHANNEL_DATA_RECV= 27
};

/* ---- GET_STATS subtypes (stats_type arg / first STATS payload byte) ---- */
enum { MC_STATS_CORE = 0, MC_STATS_RADIO = 1, MC_STATS_PACKETS = 2 };

/* ---- Push codes (unsolicited, radio -> app) ---- */
enum {
    MC_PUSH_ADVERT        = 0x80,
    MC_PUSH_PATH_UPDATED  = 0x81,
    MC_PUSH_SEND_CONFIRMED= 0x82,
    MC_PUSH_MSG_WAITING   = 0x83,  /* "drain me": loop SYNC_NEXT_MESSAGE       */
    MC_PUSH_RAW_DATA      = 0x84,
    MC_PUSH_LOGIN_SUCCESS = 0x85,
    MC_PUSH_LOGIN_FAIL    = 0x86,
    MC_PUSH_STATUS_RESP   = 0x87,
    MC_PUSH_LOG_RX_DATA   = 0x88,
    MC_PUSH_TRACE_DATA    = 0x89,
    MC_PUSH_NEW_ADVERT    = 0x8A,  /* carries a full contact record */
    MC_PUSH_TELEMETRY     = 0x8B,
    MC_PUSH_BINARY_RESP   = 0x8C,
    MC_PUSH_CONTACT_DELETED = 0x8F,
    MC_PUSH_CONTACTS_FULL   = 0x90
};

/* ---- Text message subtypes ---- */
enum { MC_TXT_PLAIN = 0, MC_TXT_CLI_DATA = 1, MC_TXT_SIGNED_PLAIN = 2 };
/* ---- Self-advert flood mode ---- */
enum { MC_ADVERT_ZERO_HOP = 0, MC_ADVERT_FLOOD = 1 };

#define MC_PATH_DIRECT 0xFF   /* path_len value meaning "received direct"     */
/* SNR is transmitted as a signed int8 scaled x4. Recover dB with this. */
#define MC_SNR_DB(q4)  ((float)(q4) / 4.0f)
/* snr_q4 sentinel for non-V3 messages that carry no SNR. */
#define MC_SNR_NONE    ((int8_t)-128)
/* rssi sentinel for non-V3 messages that carry no RSSI (0 dBm never occurs). */
#define MC_RSSI_NONE   ((int8_t)0)

/* ======================================================================== *
 *  Receive side: streaming frame assembler
 * ======================================================================== */
typedef struct {
    uint8_t buf[MC_RX_BUFSZ];
    size_t  len;
} mc_rx_t;

void mc_rx_init(mc_rx_t *rx);

/* Append received bytes. Returns the number of bytes accepted (bytes beyond
 * the buffer capacity are dropped; this only happens if a peer floods us with
 * a frame larger than MC_MAX_PAYLOAD, which the poller will resync past). */
size_t mc_rx_feed(mc_rx_t *rx, const uint8_t *data, size_t n);

/* Pull the next complete payload (the bytes after the 3-byte header) into
 * out[]. Returns 1 and sets *out_len if a frame is ready, 0 if more bytes are
 * needed. Call repeatedly until it returns 0. Garbage / oversized frames are
 * resynced automatically by skipping one byte at a time. */
int mc_rx_poll(mc_rx_t *rx, uint8_t *out, size_t out_cap, size_t *out_len);

/* ======================================================================== *
 *  Transmit side: wrap a payload into an on-wire app->radio frame
 * ======================================================================== */
/* Writes [0x3C][len LE][payload] into out[]. Returns total bytes, or 0 on
 * overflow / oversize. */
size_t mc_frame_encode(const uint8_t *payload, size_t payload_len,
                       uint8_t *out, size_t out_cap);

/* ======================================================================== *
 *  Command payload builders (write payload only; wrap with mc_frame_encode)
 *  Each returns the payload length, or 0 if it would overflow `cap`.
 * ======================================================================== */
size_t mc_cmd_app_start        (uint8_t *out, size_t cap, const char *app_name);
size_t mc_cmd_device_query     (uint8_t *out, size_t cap, uint8_t app_target_ver);
size_t mc_cmd_get_device_time  (uint8_t *out, size_t cap);
size_t mc_cmd_set_device_time  (uint8_t *out, size_t cap, uint32_t epoch_secs);
size_t mc_cmd_sync_next_message(uint8_t *out, size_t cap);
size_t mc_cmd_send_self_advert (uint8_t *out, size_t cap, uint8_t advert_type);
size_t mc_cmd_get_channel      (uint8_t *out, size_t cap, uint8_t channel_idx);
size_t mc_cmd_set_channel      (uint8_t *out, size_t cap, uint8_t channel_idx,
                                const char *name, const uint8_t secret[MC_SECRET_LEN]);
size_t mc_cmd_send_channel_text(uint8_t *out, size_t cap, uint8_t txt_type,
                                uint8_t channel_idx, uint32_t sender_ts,
                                const char *text);
/* Direct (contact) text/command message (cmd 2). `dst` is the destination's
 * public key or 6-byte prefix (dst_len bytes, usually 6). */
size_t mc_cmd_send_txt_msg     (uint8_t *out, size_t cap, uint8_t txt_type,
                                uint8_t attempt, uint32_t sender_ts,
                                const uint8_t *dst, size_t dst_len,
                                const char *text);
/* CLI command to a repeater/companion: cmd 2, txt_type=MC_TXT_CLI_DATA, attempt 0. */
size_t mc_cmd_send_cmd         (uint8_t *out, size_t cap, uint32_t sender_ts,
                                const uint8_t *dst, size_t dst_len,
                                const char *cmd);
size_t mc_cmd_set_radio_params (uint8_t *out, size_t cap, uint32_t freq_hz_x1000,
                                uint32_t bw, uint8_t sf, uint8_t cr);
/* Node name shown in adverts (cmd 8). */
size_t mc_cmd_set_advert_name  (uint8_t *out, size_t cap, const char *name);
/* Radio TX power in dBm (cmd 12). */
size_t mc_cmd_set_tx_power     (uint8_t *out, size_t cap, uint32_t dbm);
size_t mc_cmd_get_stats        (uint8_t *out, size_t cap, uint8_t stats_type);

/* ---- contacts (Phase 2) ---- */
/* GET_CONTACTS; pass since_lastmod>0 for a delta sync, or 0 for all. */
size_t mc_cmd_get_contacts     (uint8_t *out, size_t cap, uint32_t since_lastmod);
size_t mc_cmd_remove_contact   (uint8_t *out, size_t cap, const uint8_t pubkey[32]);
size_t mc_cmd_reset_path       (uint8_t *out, size_t cap, const uint8_t pubkey[32]);
size_t mc_cmd_share_contact    (uint8_t *out, size_t cap, const uint8_t pubkey[32]);
size_t mc_cmd_get_contact_by_key(uint8_t *out, size_t cap, const uint8_t pubkey[32]);
/* EXPORT_CONTACT; pubkey==NULL exports this node's own card. Reply: CONTACT_URI. */
size_t mc_cmd_export_contact   (uint8_t *out, size_t cap, const uint8_t pubkey[32]);
size_t mc_cmd_import_contact   (uint8_t *out, size_t cap, const uint8_t *card, size_t card_len);

/* ---- binary / anonymous requests (Phase 2) ---- */
/* SEND_BINARY_REQ: dst is a full 32-byte key; req_type is MC_BINARY_REQ_*;
 * data is the type-specific request blob (may be NULL). */
size_t mc_cmd_send_binary_req  (uint8_t *out, size_t cap, const uint8_t dst[32],
                                uint8_t req_type, const uint8_t *data, size_t data_len);
size_t mc_cmd_send_anon_req    (uint8_t *out, size_t cap, const uint8_t dst[32],
                                uint8_t req_type, const uint8_t *data, size_t data_len);
/* mc_cmd_add_update_contact() and mc_parse_status() are declared below, after the
 * mc_contact_t / mc_status_t struct definitions they reference. */

/* ======================================================================== *
 *  Parsed events
 * ======================================================================== */
typedef struct {
    int8_t   fw_ver;
    uint16_t max_contacts;      /* already x2 from the wire field */
    uint8_t  max_channels;
    uint32_t ble_pin;
    char     build_date[16];
    char     model[MC_MAX_MODEL];
    char     ver[24];           /* firmware version string (fw_ver>=3)        */
    uint8_t  repeat;            /* repeater mode (fw_ver>=9); see have_repeat */
    uint8_t  path_hash_mode;    /* fw_ver>=10; see have_path_hash             */
    int      have_repeat;
    int      have_path_hash;
} mc_device_info_t;

typedef struct {
    int8_t   channel_idx;
    uint8_t  path_len;          /* MC_PATH_DIRECT or flood hop count */
    uint8_t  txt_type;
    uint32_t sender_ts;
    int8_t   snr_q4;            /* V3 only (code 17); MC_SNR_NONE otherwise */
    int8_t   rssi;              /* dBm; V3 only (code 17); MC_RSSI_NONE otherwise */
    char     text[MC_MAX_TEXT]; /* for channel msgs this is "Name: body" */
} mc_channel_msg_t;

typedef struct {
    int8_t   snr_q4;            /* divide by 4 for dB; see MC_SNR_DB() */
    int8_t   channel_idx;
    uint8_t  path_len;
    uint16_t data_type;
    uint8_t  data_len;
    uint8_t  data[MC_MAX_DATA];
} mc_channel_data_t;

typedef struct {
    uint8_t  pubkey_prefix[6];
    uint8_t  path_len;
    uint8_t  txt_type;
    uint32_t sender_ts;
    int8_t   snr_q4;            /* V3 only (code 16); MC_SNR_NONE otherwise   */
    int8_t   rssi;              /* dBm; V3 only (code 16); MC_RSSI_NONE otherwise */
    uint8_t  signature[4];      /* present when txt_type==MC_TXT_SIGNED_PLAIN */
    int      has_signature;
    char     text[MC_MAX_TEXT];
} mc_contact_msg_t;

typedef struct {
    uint8_t  channel_idx;
    char     name[MC_NAME_LEN + 1];
    uint8_t  secret[MC_SECRET_LEN];
    int      have_secret;
} mc_channel_info_t;

typedef struct {
    uint8_t  type, tx_power, max_tx_power;
    uint8_t  public_key[32];
    int32_t  adv_lat, adv_lon;          /* degrees x 1e6 */
    uint8_t  multi_acks;
    uint8_t  adv_loc_policy;
    uint8_t  telemetry_mode;            /* raw byte; decoded below            */
    uint8_t  tm_base, tm_loc, tm_env;   /* 2-bit fields: base=t&3, loc=(t>>2)&3, env=(t>>4)&3 */
    uint8_t  manual_add_contacts;
    uint32_t radio_freq, radio_bw;
    uint8_t  radio_sf, radio_cr;
    char     name[MC_MAX_TEXT];
} mc_self_info_t;

typedef struct {
    uint8_t  type;              /* result/type byte                          */
    uint32_t expected_ack;      /* 4-byte ack tag (LE) to match an ACK push  */
    uint32_t suggested_timeout; /* ms to wait for the ACK                     */
} mc_msg_sent_t;

typedef struct {
    uint8_t subtype;            /* MC_STATS_CORE / RADIO / PACKETS           */
    int     has_recv_errors;    /* PACKETS only: recv_errors field present   */
    union {
        struct { uint16_t battery_mv; uint32_t uptime_secs; uint16_t errors; uint8_t queue_len; } core;
        struct { int16_t noise_floor; int8_t last_rssi; int8_t last_snr_q4;
                 uint32_t tx_air_secs, rx_air_secs; } radio;
        struct { uint32_t recv, sent, flood_tx, direct_tx, flood_rx, direct_rx, recv_errors; } packets;
    } u;
} mc_stats_t;

/* A mesh contact, as parsed from CONTACT (3) / NEW_ADVERT (0x8A) and as built
 * for ADD_UPDATE_CONTACT. Lat/lon are degrees x 1e6. */
typedef struct {
    uint8_t  public_key[32];
    uint8_t  type;
    uint8_t  flags;
    uint8_t  out_path_len;
    uint8_t  out_path[64];
    char     adv_name[MC_NAME_LEN + 1];
    uint32_t last_advert;
    int32_t  adv_lat, adv_lon;
    uint32_t lastmod;
} mc_contact_t;

/* Raw binary response (BINARY_RESPONSE push, 0x8C): a 4-byte tag matching the
 * MsgSent.expected_ack of the request, plus opaque payload. Decode per the
 * request type you sent (e.g. mc_parse_status for a STATUS request). */
typedef struct {
    uint32_t tag;
    uint8_t  data_len;
    uint8_t  data[MC_MAX_DATA];
} mc_binary_resp_t;

/* Decoded STATUS binary response (mc_parse_status). */
typedef struct {
    uint16_t bat_mv, tx_queue_len;
    int16_t  noise_floor, last_rssi;
    uint32_t nb_recv, nb_sent, airtime_secs, uptime_secs;
    uint32_t sent_flood, sent_direct, recv_flood, recv_direct;
    uint16_t full_evts;
    int16_t  last_snr_q4;               /* divide by 4 for dB */
    uint16_t direct_dups, flood_dups;
    uint32_t rx_airtime_secs;
    uint32_t recv_errors;               /* fw v1.12+; see has_recv_errors */
    int      has_recv_errors;
} mc_status_t;

typedef struct {
    uint8_t code;               /* response or push code (first payload byte) */
    union {
        mc_device_info_t  device_info;
        mc_channel_msg_t  channel_msg;
        mc_channel_data_t channel_data;
        mc_contact_msg_t  contact_msg;
        mc_channel_info_t channel_info;
        mc_self_info_t    self_info;
        mc_msg_sent_t     msg_sent;         /* MC_RESP_SENT                  */
        mc_stats_t        stats;            /* MC_RESP_STATS                 */
        mc_contact_t      contact;          /* CONTACT (3) / NEW_ADVERT      */
        mc_binary_resp_t  binary_resp;      /* MC_PUSH_BINARY_RESP           */
        uint32_t          contacts_count;   /* CONTACTS_START: number to come */
        uint32_t          contacts_lastmod; /* END_OF_CONTACTS: newest lastmod*/
        uint8_t           deleted_key[32];  /* CONTACT_DELETED               */
        uint8_t           autoadd_config;   /* AUTOADD_CONFIG                */
        struct { uint8_t len; uint8_t data[MC_MAX_DATA]; } contact_uri; /* CONTACT_URI */
        struct { uint32_t timestamp; uint8_t path_len; uint8_t path[64]; } advert_path;
        struct { uint8_t count; struct { uint32_t min, max; } pair[8]; } allowed_freq;
        uint32_t          curr_time;        /* epoch secs                    */
        uint16_t          battery_mv;
        int8_t            err_code;          /* MC_RESP_ERR (-1 if absent)    */
        uint8_t           pubkey32[32];      /* advert / path-updated pushes  */
        struct { uint32_t ack_code, round_trip; } send_confirmed;
    } u;
} mc_event_t;

/* Decode one received payload. Returns 1 if recognised (ev->code set and the
 * matching union member filled), 0 if the code is unknown to this build. */
int mc_parse(const uint8_t *payload, size_t len, mc_event_t *ev);

/* Builders/parsers that reference the structs above. */
size_t mc_cmd_add_update_contact(uint8_t *out, size_t cap, const mc_contact_t *c);
/* Decode a STATUS binary-response payload (resp.data/.data_len). 1 on success. */
int    mc_parse_status         (const uint8_t *data, size_t len, mc_status_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* MESHCORE_COMPANION_H */
