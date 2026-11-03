/*
 * Session state machine: turns backend events into what the UI should show,
 * owns the output buffer and the stream flow-control credit. Pure C.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include "backend.h"
#include "textproc.h"

typedef enum {
    SessionIdle = 0,
    SessionConnecting, /* TCP/KEX/auth in progress, show the connecting popup */
    SessionHostkey, /* a host-key prompt is waiting for the user */
    SessionPassword, /* the ESP32 waits for a password */
    SessionPassphrase, /* the ESP32 waits for the passphrase of the staged key */
    SessionReady, /* authenticated; Run menu */
    SessionRunning, /* command or script running; Output view */
    SessionFailed, /* see reason/msg */
    SessionClosed,
} SessionPhase;

/* Change flags returned by session_on_event */
#define SESSION_CHG_PHASE    (1u << 0)
#define SESSION_CHG_OUTPUT   (1u << 1)
#define SESSION_CHG_DONE     (1u << 2) /* run finished; `done` valid */
#define SESSION_CHG_NET      (1u << 3)
#define SESSION_CHG_LINK     (1u << 4)
#define SESSION_CHG_PROMPT   (1u << 5)
#define SESSION_CHG_STATUS   (1u << 6) /* connecting sub-state text changed */
#define SESSION_CHG_LINKTEST (1u << 7)

#define SESSION_CREDIT_WINDOW 1024u

typedef struct {
    SessionPhase phase;
    uint8_t ssh_state; /* last ProtoSshState */
    uint8_t reason;
    uint8_t iface;
    bool stored_secret;
    bool auth_retry; /* password or passphrase prompt after a failed attempt */
    char msg[PROTO_STR_MSG + 1];

    ProtoHostkeyPrompt prompt;

    /* current / last run */
    bool running;
    bool have_done;
    ProtoExecDone done;
    uint32_t bytes_rx;
    bool saw_stderr;
    uint32_t credit_out; /* granted, not yet consumed */
    uint32_t credit_total; /* granted in this run */
    AnsiStripper ansi;
    OutBuf out;

    /* board */
    BackendLink link;
    bool have_net;
    ProtoNetStatus net;
    BackendLinkTest link_test; /* last link test (Developer menu) */
} Session;

void session_init(Session* s);
/* Back to Idle for a new connection (keeps link/net knowledge). */
void session_reset(Session* s);
uint32_t session_on_event(Session* s, const BackendEvent* e);
/* Call right after a successful exec()/run_script(). */
void session_begin_run(Session* s);
/* Credit to grant now (0 if none). Adds it to the outstanding count. */
uint32_t session_take_credit(Session* s);
/* Short status for the connecting popup, e.g. "Authenticating..." */
const char* session_status_text(const Session* s);
/* Human-readable failure line, e.g. "Connection refused". */
const char* session_failure_text(const Session* s);
/* True if the LAN / WiFi interface is usable (link + IP). */
bool session_iface_up(const Session* s, uint8_t iface);
