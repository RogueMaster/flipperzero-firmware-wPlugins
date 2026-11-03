/* SPDX-License-Identifier: GPL-3.0-only */
#include "session.h"

#include <stdio.h>
#include <string.h>

void session_init(Session* s) {
    memset(s, 0, sizeof(*s));
    ansi_init(&s->ansi);
    outbuf_reset(&s->out);
}

void session_reset(Session* s) {
    BackendLink link = s->link;
    bool have_net = s->have_net;
    ProtoNetStatus net = s->net;
    session_init(s);
    s->link = link;
    s->have_net = have_net;
    s->net = net;
}

static SessionPhase phase_for(uint8_t st) {
    switch(st) {
    case ProtoSshConnecting:
    case ProtoSshAuthenticating:
    /* The ESP32 reports HostkeyWait before it sends the prompt itself: stay "connecting"
     * until HOSTKEY_PROMPT_EVT arrives, which switches to SessionHostkey with its data
     * (found on hardware: the screen opened with an empty key). */
    case ProtoSshHostkeyWait:
        return SessionConnecting;
    case ProtoSshPasswordWait:
        return SessionPassword;
    case ProtoSshPassphraseWait:
        return SessionPassphrase;
    case ProtoSshReady:
        return SessionReady;
    case ProtoSshRunning:
        return SessionRunning;
    case ProtoSshFailed:
        return SessionFailed;
    case ProtoSshClosed:
        return SessionClosed;
    default:
        return SessionIdle;
    }
}

uint32_t session_on_event(Session* s, const BackendEvent* e) {
    uint32_t chg = 0;
    switch(e->type) {
    case BackendEvtLink:
        s->link = e->link;
        chg |= SESSION_CHG_LINK;
        break;
    case BackendEvtNetStatus:
        s->net = e->net;
        s->have_net = true;
        chg |= SESSION_CHG_NET;
        break;
    case BackendEvtLinkTest:
        s->link_test = e->link_test;
        chg |= SESSION_CHG_LINKTEST;
        break;
    case BackendEvtSshState: {
        uint8_t old_state = s->ssh_state;
        s->ssh_state = e->ssh.state;
        s->reason = e->ssh.reason;
        s->iface = e->ssh.iface;
        s->stored_secret = e->ssh.stored_secret != 0;
        snprintf(s->msg, sizeof(s->msg), "%s", e->ssh.msg);
        s->auth_retry =
            (e->ssh.state == ProtoSshPasswordWait && e->ssh.reason == ProtoReasonAuthFailed) ||
            (e->ssh.state == ProtoSshPassphraseWait && e->ssh.reason == ProtoReasonBadPassphrase);
        SessionPhase p = phase_for(e->ssh.state);
        /* A run that loses its session is finished. */
        if(s->running && (p == SessionFailed || p == SessionClosed)) {
            s->running = false;
            if(!s->have_done) {
                s->have_done = true;
                memset(&s->done, 0, sizeof(s->done));
                s->done.flags = PROTO_DONE_LOST;
                s->done.exit_code = -1;
                chg |= SESSION_CHG_DONE;
            }
        }
        if(p != s->phase) {
            s->phase = p;
            chg |= SESSION_CHG_PHASE;
        } else if(old_state != s->ssh_state || s->auth_retry) {
            chg |= SESSION_CHG_STATUS;
        }
        break;
    }
    case BackendEvtHostkeyPrompt:
        s->prompt = e->hostkey;
        s->phase = SessionHostkey;
        chg |= SESSION_CHG_PROMPT | SESSION_CHG_PHASE;
        break;
    case BackendEvtStream: {
        if(!s->running) break;
        uint32_t n = e->stream.len;
        s->bytes_rx += n;
        s->credit_out = n > s->credit_out ? 0 : s->credit_out - n;
        if(e->stream.stream == ProtoStreamStderr) s->saw_stderr = true;
        /* strip in bounded slices: worst case output is 2x input */
        char tmp[2 * 128 + 1];
        const uint8_t* d = e->stream.data;
        while(n) {
            size_t k = n > 128 ? 128 : n;
            size_t m = ansi_strip(&s->ansi, d, k, tmp);
            outbuf_append(&s->out, tmp, m);
            d += k;
            n -= (uint32_t)k;
        }
        chg |= SESSION_CHG_OUTPUT;
        break;
    }
    case BackendEvtExecDone:
        if(!s->running) break;
        s->running = false;
        s->have_done = true;
        s->done = e->done;
        /* flush a trailing CR */
        if(s->ansi.pending_cr) {
            s->ansi.pending_cr = false;
            outbuf_append(&s->out, "\n", 1);
        }
        chg |= SESSION_CHG_DONE | SESSION_CHG_OUTPUT;
        break;
    default:
        break;
    }
    return chg;
}

void session_begin_run(Session* s) {
    s->running = true;
    s->have_done = false;
    memset(&s->done, 0, sizeof(s->done));
    s->bytes_rx = 0;
    s->saw_stderr = false;
    s->credit_out = 0;
    s->credit_total = 0;
    ansi_init(&s->ansi);
    outbuf_reset(&s->out);
    s->phase = SessionRunning;
}

uint32_t session_take_credit(Session* s) {
    if(!s->running) return 0;
    /* Output beyond the view buffer is dropped from the middle (OutBuf), so credit is
     * always replenished: the remote command never stalls and its exit code arrives. */
    if(s->credit_out > SESSION_CREDIT_WINDOW / 2) return 0;
    uint32_t grant = SESSION_CREDIT_WINDOW - s->credit_out;
    s->credit_out += grant;
    s->credit_total += grant;
    return grant;
}

const char* session_status_text(const Session* s) {
    switch(s->ssh_state) {
    case ProtoSshConnecting:
        return s->msg[0] ? s->msg : "Connecting..."; /* "Unlocking key..." */
    case ProtoSshHostkeyWait:
        return "Checking host key...";
    case ProtoSshAuthenticating:
        return s->msg[0] ? s->msg : "Authenticating...";
    case ProtoSshReady:
        return "Connected";
    default:
        return "Connecting...";
    }
}

const char* session_failure_text(const Session* s) {
    if(s->msg[0]) return s->msg;
    switch(s->reason) {
    case ProtoReasonNoLink:
        return "No network link";
    case ProtoReasonDns:
        return "Host not found";
    case ProtoReasonRefused:
        return "Connection refused";
    case ProtoReasonTimeout:
        return "Connection timed out";
    case ProtoReasonHostkeyRejected:
        return "Host key rejected";
    case ProtoReasonHostkeyCancelled:
        return "Host key not accepted";
    case ProtoReasonAuthFailed:
        return "Authentication failed";
    case ProtoReasonNoKey:
        return "No key file sent";
    case ProtoReasonBadKey:
        return "Key file not usable";
    case ProtoReasonBadPassphrase:
        return "Wrong passphrase";
    case ProtoReasonPeerClosed:
        return "Connection closed by host";
    case ProtoReasonBusy:
        return "ESP32 busy";
    case ProtoReasonProtocol:
        return "Protocol error";
    default:
        return "Connection failed";
    }
}

bool session_iface_up(const Session* s, uint8_t iface) {
    if(!s->have_net) return false;
    const ProtoIfaceStatus* st = iface == ProtoIfaceLan ? &s->net.lan : &s->net.wifi;
    return (st->flags & (PROTO_NET_LINK | PROTO_NET_IP)) == (PROTO_NET_LINK | PROTO_NET_IP);
}
