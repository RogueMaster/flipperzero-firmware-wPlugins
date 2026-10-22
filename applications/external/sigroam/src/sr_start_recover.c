#include "sr_start_recover.h"

#include <string.h>

static bool g_banner;

static bool rec_has(const char* line, size_t len, const char* lit) {
    size_t n = 0u;
    size_t i;

    while(lit[n] != '\0') {
        n++;
    }
    if(line == NULL || n == 0u || len < n) {
        return false;
    }
    for(i = 0u; i + n <= len; i++) {
        if(memcmp(line + i, lit, n) == 0) {
            return true;
        }
    }
    return false;
}

static bool rec_due(uint32_t now, uint32_t mark, uint32_t ms) {
    return (uint32_t)(now - mark) >= ms;
}

static SrRecOut rec_clear(void) {
    SrRecOut out;

    out.act = (unsigned)SrRecActNone;
    out.ui = (unsigned)SrRecUiNone;
    out.arm_cap = false;
    out.disarm_cap = false;
    out.mark = NULL;
    return out;
}

static void rec_drop_cap(SrStartRecover* s, SrRecOut* out) {
    if(s->cap) {
        out->disarm_cap = true;
        s->cap = false;
    }
}

void sr_start_recover_reset(SrStartRecover* s) {
    if(s == NULL) {
        return;
    }
    s->phase = (uint8_t)SrRecOff;
    s->ui = (uint8_t)SrRecUiNone;
    s->mark_ms = 0u;
    s->retried = false;
    s->cap = false;
}

unsigned sr_start_recover_phase(const SrStartRecover* s) {
    if(s == NULL) {
        return (unsigned)SrRecOff;
    }
    return s->phase;
}

unsigned sr_start_recover_ui(const SrStartRecover* s) {
    if(s == NULL) {
        return (unsigned)SrRecUiNone;
    }
    return s->ui;
}

bool sr_start_recover_active(const SrStartRecover* s) {
    return s != NULL && s->phase != (uint8_t)SrRecOff;
}

SrRecOut sr_start_recover_tick(SrStartRecover* s, const SrRecIn* in) {
    SrRecOut out = rec_clear();

    if(s == NULL || in == NULL) {
        return out;
    }
    if(!in->lite) {
        rec_drop_cap(s, &out);
        sr_start_recover_reset(s);
        return out;
    }
    if(in->running && s->phase != (uint8_t)SrRecOff && s->phase != (uint8_t)SrRecReboot &&
       s->phase != (uint8_t)SrRecFail) {
        rec_drop_cap(s, &out);
        s->phase = (uint8_t)SrRecOff;
        s->ui = (uint8_t)SrRecUiNone;
        return out;
    }
    if(s->phase == (uint8_t)SrRecOff) {
        if(in->accepted && !in->running) {
            s->phase = (uint8_t)SrRecWatch;
            s->mark_ms = in->now_ms;
            s->ui = (uint8_t)SrRecUiNone;
            s->retried = false;
        }
        return out;
    }
    if(s->phase == (uint8_t)SrRecWatch) {
        if(!in->accepted && !in->running) {
            rec_drop_cap(s, &out);
            s->phase = (uint8_t)SrRecOff;
            s->ui = (uint8_t)SrRecUiNone;
            return out;
        }
        if(!rec_due(in->now_ms, s->mark_ms, (uint32_t)SR_START_RECOVER_MS)) {
            return out;
        }
        s->phase = (uint8_t)SrRecCheck;
        s->ui = (uint8_t)SrRecUiChecking;
        s->mark_ms = in->now_ms;
        out.act = (unsigned)SrRecActSendSigroam;
        out.ui = (unsigned)SrRecUiChecking;
        out.arm_cap = !in->cap_busy;
        out.mark = "recover-check";
        s->cap = out.arm_cap;
        return out;
    }
    if(s->phase == (uint8_t)SrRecCheck) {
        s->ui = (uint8_t)SrRecUiChecking;
        out.ui = (unsigned)SrRecUiChecking;
        if(in->diag_fresh && in->diag_st == 1u) {
            rec_drop_cap(s, &out);
            s->phase = (uint8_t)SrRecOff;
            s->ui = (uint8_t)SrRecUiNone;
            out.act = (unsigned)SrRecActAdopt;
            out.ui = (unsigned)SrRecUiNone;
            out.mark = "recover-st1";
            return out;
        }
        if(in->diag_fresh && in->diag_st == 0u && !s->retried) {
            s->retried = true;
            s->phase = (uint8_t)SrRecStop;
            s->ui = (uint8_t)SrRecUiRetrying;
            out.act = (unsigned)SrRecActSendStop;
            out.ui = (unsigned)SrRecUiRetrying;
            out.mark = "recover-retry";
            return out;
        }
        if(!in->diag_fresh && rec_due(in->now_ms, s->mark_ms, (uint32_t)SR_START_RECOVER_MS)) {
            s->phase = (uint8_t)SrRecFail;
            s->ui = (uint8_t)SrRecUiFail;
            out.ui = (unsigned)SrRecUiFail;
            out.mark = "recover-nodiag";
            return out;
        }
        return out;
    }
    if(s->phase == (uint8_t)SrRecStop) {
        s->ui = (uint8_t)SrRecUiRetrying;
        out.ui = (unsigned)SrRecUiRetrying;
        if(in->retry_accepted) {
            s->phase = (uint8_t)SrRecRetry;
            s->mark_ms = in->now_ms;
            s->ui = (uint8_t)SrRecUiRetrying;
        }
        return out;
    }
    if(s->phase == (uint8_t)SrRecRetry) {
        s->ui = (uint8_t)SrRecUiRetrying;
        out.ui = (unsigned)SrRecUiRetrying;
        if(!rec_due(in->now_ms, s->mark_ms, (uint32_t)SR_START_RECOVER_MS)) {
            return out;
        }
        s->phase = (uint8_t)SrRecFail;
        s->ui = (uint8_t)SrRecUiFail;
        out.ui = (unsigned)SrRecUiFail;
        out.mark = "recover-fail";
        return out;
    }
    if(s->phase == (uint8_t)SrRecFail) {
        s->ui = (uint8_t)SrRecUiFail;
        out.ui = (unsigned)SrRecUiFail;
        return out;
    }
    if(s->phase == (uint8_t)SrRecReboot) {
        s->ui = (uint8_t)SrRecUiRebooting;
        out.ui = (unsigned)SrRecUiRebooting;
        if(in->banner || rec_due(in->now_ms, s->mark_ms, (uint32_t)SR_START_REBOOT_MS)) {
            rec_drop_cap(s, &out);
            s->phase = (uint8_t)SrRecOff;
            s->ui = (uint8_t)SrRecUiNone;
            out.ui = (unsigned)SrRecUiNone;
            out.mark = "recover-up";
        }
        return out;
    }
    sr_start_recover_reset(s);
    return out;
}

SrRecOut sr_start_recover_ok(SrStartRecover* s, uint32_t now_ms) {
    SrRecOut out = rec_clear();

    if(s == NULL || s->phase == (uint8_t)SrRecOff) {
        return out;
    }
    if(s->phase == (uint8_t)SrRecFail) {
        s->phase = (uint8_t)SrRecReboot;
        s->ui = (uint8_t)SrRecUiRebooting;
        s->mark_ms = now_ms;
        out.act = (unsigned)SrRecActSendReboot;
        out.ui = (unsigned)SrRecUiRebooting;
        out.mark = "recover-reboot";
        return out;
    }
    rec_drop_cap(s, &out);
    s->phase = (uint8_t)SrRecOff;
    s->ui = (uint8_t)SrRecUiNone;
    out.act = (unsigned)SrRecActCancel;
    return out;
}

const char* sr_start_recover_line(unsigned ui, unsigned row) {
    if(ui == (unsigned)SrRecUiChecking && row == 0u) {
        return "Checking board...";
    }
    if(ui == (unsigned)SrRecUiRetrying && row == 0u) {
        return "Retrying start...";
    }
    if(ui == (unsigned)SrRecUiRebooting && row == 0u) {
        return "Rebooting board...";
    }
    if(ui == (unsigned)SrRecUiFail && row == 0u) {
        return "Scan did not start";
    }
    if(ui == (unsigned)SrRecUiFail && row == 1u) {
        return "GPS may be missing";
    }
    if(ui == (unsigned)SrRecUiFail && row == 2u) {
        return "OK: reboot board";
    }
    return NULL;
}

bool sr_start_recover_is_banner(const char* line, size_t len) {
    return rec_has(line, len, "PINGEQUA Scout Lite") || rec_has(line, len, "By: justcallmekoko");
}

void sr_start_recover_note_line(const char* line, size_t len) {
    if(sr_start_recover_is_banner(line, len)) {
        g_banner = true;
    }
}

bool sr_start_recover_take_banner(void) {
    bool seen = g_banner;

    g_banner = false;
    return seen;
}
