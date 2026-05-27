#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_MAX_MSG_LEN 2048
#define PROTOCOL_MAX_FIELD_LEN 64
typedef enum {
    MsgTypeUnknown = 0,
    // Host -> Flipper
    MsgTypeNotify,
    MsgTypePing,
    MsgTypeStatus,
    MsgTypeMenu,
    MsgTypeState,
    MsgTypePerm,
    // Flipper -> Host
    MsgTypeCmd,
    MsgTypeEnter,
    MsgTypeEsc,
    MsgTypeDown,
    MsgTypeVoice,
    MsgTypeSpaceDown,
    MsgTypeSpaceUp,
    MsgTypeHello,
    MsgTypePong,
    MsgTypeYes,
    /* Synthesised from an Anthropic heartbeat snapshot. Distinct from
     * MsgTypeStatus so the GUI thread can drive sound + pose transitions
     * based on total/running/waiting counter deltas. */
    MsgTypeAnthropicHB,
} MsgType;

typedef struct {
    MsgType type;
    char sound[32];
    bool vibro;
    char text[PROTOCOL_MAX_FIELD_LEN];   // line1
    char text2[PROTOCOL_MAX_FIELD_LEN];  // line2 (subtext)
    char menu_data[PROTOCOL_MAX_MSG_LEN]; // pipe-delimited menu items
    bool claude_connected; // claude code session state
    bool has_rssi;
    int16_t rssi;
    /* Anthropic NUS protocol additions (zero/empty when not applicable):
     *   perm_id     — id of the pending permission prompt, echoed back on
     *                 the user's decision
     *   pending_ack — cmd name (e.g. "status", "owner") that the GUI thread
     *                 must ack via transport_send.  Set by on_serial_data
     *                 in NUS mode; consumed in process_message. */
    char perm_id[40];
    char pending_ack[16];
    /* `n` field to use when the ack needs a running byte count (chunk,
     * file_end). 0 for normal acks. */
    uint32_t ack_n;
    /* Counters from the latest Anthropic heartbeat (for MsgTypeAnthropicHB). */
    int hb_total;
    int hb_running;
    int hb_waiting;
    uint32_t hb_tokens;        /* cumulative since desktop start */
    uint32_t hb_tokens_today;  /* resets at local midnight */
    /* Per-kind payloads deferred to the GUI thread.  Keeping storage /
     * hardware side-effects off the BLE event callback thread avoids
     * deadlocks and long-blocking operations on that critical path. */
    char nus_name[32];          /* cmd:owner, cmd:name */
    int64_t nus_time_epoch;     /* cmd:time */
    int32_t nus_time_tz;
} ProtocolMessage;

// Parse a JSON line into a ProtocolMessage. Returns true on success.
bool protocol_parse(const char* json_line, ProtocolMessage* msg);

// Build outgoing JSON messages into buf. Returns bytes written.
int protocol_build_hello(char* buf, int buf_size);
int protocol_build_cmd(char* buf, int buf_size, const char* text);
int protocol_build_enter(char* buf, int buf_size);
int protocol_build_esc(char* buf, int buf_size);
int protocol_build_voice(char* buf, int buf_size);
int protocol_build_space_down(char* buf, int buf_size);
int protocol_build_space_up(char* buf, int buf_size);
int protocol_build_down(char* buf, int buf_size);
int protocol_build_pong(char* buf, int buf_size);
int protocol_build_perm_resp(char* buf, int buf_size, bool allow, bool always, bool esc);
int protocol_build_interrupt(char* buf, int buf_size);
int protocol_build_backspace(char* buf, int buf_size);
int protocol_build_yes(char* buf, int buf_size);
int protocol_build_pgup(char* buf, int buf_size);
int protocol_build_pgdown(char* buf, int buf_size);
int protocol_build_ctrl_o(char* buf, int buf_size);
int protocol_build_ctrl_e(char* buf, int buf_size);
int protocol_build_shift_tab(char* buf, int buf_size);
