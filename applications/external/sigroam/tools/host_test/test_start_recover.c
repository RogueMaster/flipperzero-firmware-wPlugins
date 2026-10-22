#include "sr_test.h"

#include "sr_start_recover.h"

#include <stdio.h>
#include <string.h>

static SrRecIn in_at(uint32_t now) {
    SrRecIn in;

    memset(&in, 0, sizeof(in));
    in.lite = true;
    in.now_ms = now;
    return in;
}

static int line_cols(const char* s) {
    return s == NULL || strlen(s) <= 20u;
}

static bool same(const char* got, const char* want) {
    if(got == NULL || want == NULL) {
        return false;
    }
    return strcmp(got, want) == 0;
}

static void test_lines_and_banner(void) {
    const char* ansi = "\x1b#PINGEQUA Scout Lite";

    CHECK(same(sr_start_recover_line(SrRecUiChecking, 0u), "Checking board..."));
    CHECK(sr_start_recover_line(SrRecUiChecking, 1u) == NULL);
    CHECK(same(sr_start_recover_line(SrRecUiRetrying, 0u), "Retrying start..."));
    CHECK(same(sr_start_recover_line(SrRecUiRebooting, 0u), "Rebooting board..."));
    CHECK(same(sr_start_recover_line(SrRecUiFail, 0u), "Scan did not start"));
    CHECK(same(sr_start_recover_line(SrRecUiFail, 1u), "GPS may be missing"));
    CHECK(same(sr_start_recover_line(SrRecUiFail, 2u), "OK: reboot board"));
    CHECK(line_cols(sr_start_recover_line(SrRecUiFail, 0u)));
    CHECK(line_cols(sr_start_recover_line(SrRecUiFail, 1u)));
    CHECK(line_cols(sr_start_recover_line(SrRecUiFail, 2u)));
    CHECK(
        sr_start_recover_is_banner("PINGEQUA Scout Lite", strlen("PINGEQUA Scout Lite")) == true);
    CHECK(sr_start_recover_is_banner("By: justcallmekoko", strlen("By: justcallmekoko")) == true);
    CHECK(sr_start_recover_is_banner(ansi, strlen(ansi)) == true);
    CHECK(sr_start_recover_is_banner("Version: v1.18.0", 16u) == false);
    CHECK(sr_start_recover_is_banner(NULL, 0u) == false);
    sr_start_recover_note_line("By: justcallmekoko", strlen("By: justcallmekoko"));
    CHECK(sr_start_recover_take_banner() == true);
    CHECK(sr_start_recover_take_banner() == false);
}

static void test_machine(void) {
    SrStartRecover s;
    SrRecIn in;
    SrRecOut out;
    uint32_t t0 = 1000u;

    sr_start_recover_reset(&s);
    in = in_at(10000u);
    in.lite = false;
    in.accepted = true;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);
    CHECK(sr_start_recover_active(&s) == false);

    sr_start_recover_reset(&s);
    in = in_at(t0);
    in.accepted = true;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecWatch);
    in.now_ms = t0 + 4999u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecWatch);
    in.now_ms = t0 + 5000u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActSendSigroam);
    CHECK(out.ui == (unsigned)SrRecUiChecking);
    CHECK(out.arm_cap == true);
    CHECK(same(out.mark, "recover-check"));
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecCheck);

    in.accepted = false;
    in.diag_fresh = true;
    in.diag_st = 1u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActAdopt);
    CHECK(same(out.mark, "recover-st1"));
    CHECK(out.disarm_cap == true);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);

    sr_start_recover_reset(&s);
    in = in_at(t0);
    in.accepted = true;
    (void)sr_start_recover_tick(&s, &in);
    in.now_ms = t0 + 5000u;
    (void)sr_start_recover_tick(&s, &in);
    in.accepted = false;
    in.diag_fresh = true;
    in.diag_st = 0u;
    in.now_ms = t0 + 5100u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActSendStop);
    CHECK(out.ui == (unsigned)SrRecUiRetrying);
    CHECK(same(out.mark, "recover-retry"));
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecStop);
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecStop);

    in.diag_fresh = false;
    in.retry_accepted = true;
    in.now_ms = t0 + 8000u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecRetry);
    in.retry_accepted = false;
    in.now_ms = t0 + 8000u + 4999u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecRetry);
    CHECK(out.act == (unsigned)SrRecActNone);
    in.now_ms = t0 + 8000u + 5000u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecFail);
    CHECK(out.ui == (unsigned)SrRecUiFail);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(same(out.mark, "recover-fail"));

    out = sr_start_recover_ok(&s, in.now_ms);
    CHECK(out.act == (unsigned)SrRecActSendReboot);
    CHECK(out.ui == (unsigned)SrRecUiRebooting);
    CHECK(same(out.mark, "recover-reboot"));
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecReboot);
    in.now_ms += 100u;
    in.banner = true;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(out.act != (unsigned)SrRecActSendSigroam);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);
    CHECK(out.disarm_cap == true);
    in.banner = false;
    in.accepted = false;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_active(&s) == false);

    sr_start_recover_reset(&s);
    in = in_at(50u);
    in.accepted = true;
    (void)sr_start_recover_tick(&s, &in);
    in.now_ms = 50u + 5000u;
    (void)sr_start_recover_tick(&s, &in);
    out = sr_start_recover_ok(&s, in.now_ms);
    CHECK(out.act == (unsigned)SrRecActCancel);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);
    CHECK(out.disarm_cap == true);

    sr_start_recover_reset(&s);
    in = in_at(0xFFFFF000u);
    in.accepted = true;
    (void)sr_start_recover_tick(&s, &in);
    in.now_ms = 0xFFFFF000u + 4999u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecWatch);
    in.now_ms = 0xFFFFF000u + 5000u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActSendSigroam);
    in.cap_busy = true;
    sr_start_recover_reset(&s);
    in = in_at(10u);
    in.accepted = true;
    in.cap_busy = true;
    (void)sr_start_recover_tick(&s, &in);
    in.now_ms = 10u + 5000u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActSendSigroam);
    CHECK(out.arm_cap == false);

    sr_start_recover_reset(&s);
    in = in_at(t0);
    in.accepted = true;
    (void)sr_start_recover_tick(&s, &in);
    in.running = true;
    in.accepted = false;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);
    CHECK(out.act == (unsigned)SrRecActNone);

    sr_start_recover_reset(&s);
    out = sr_start_recover_ok(&s, 1u);
    CHECK(out.act == (unsigned)SrRecActNone);

    sr_start_recover_reset(&s);
    in = in_at(t0);
    in.accepted = true;
    (void)sr_start_recover_tick(&s, &in);
    in.now_ms = t0 + 5000u;
    (void)sr_start_recover_tick(&s, &in);
    in.diag_fresh = true;
    in.diag_st = 0u;
    in.accepted = false;
    (void)sr_start_recover_tick(&s, &in);
    in.diag_fresh = false;
    in.retry_accepted = true;
    in.now_ms = t0 + 9000u;
    (void)sr_start_recover_tick(&s, &in);
    in.retry_accepted = false;
    in.now_ms = t0 + 9000u + 5000u;
    (void)sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecFail);
    out = sr_start_recover_ok(&s, in.now_ms);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecReboot);
    in.banner = false;
    in.now_ms = s.mark_ms + 14999u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecReboot);
    in.now_ms = s.mark_ms + 15000u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);
    CHECK(out.act == (unsigned)SrRecActNone);
}

/* The machine adopts when diag_st is 1. memset in dash_recover_tick leaves
 * the field at 0, and 0 is the retry path. This reads the scene because that
 * function calls furi and cannot run here. */
static int recover_tick_copies_diag(void) {
    static const char* paths[] = {
        "../../scenes/scene_dash.c",
        "scenes/scene_dash.c",
        NULL,
    };
    FILE* f = NULL;
    char buf[512];
    int in_fn = 0;
    int depth = 0;
    int saw_memset = 0;
    int saw_tick = 0;
    int saw_assign = 0;
    int i;

    for(i = 0; paths[i] != NULL; i++) {
        f = fopen(paths[i], "r");
        if(f != NULL) {
            break;
        }
    }
    if(f == NULL) {
        return 0;
    }
    while(fgets(buf, (int)sizeof(buf), f) != NULL) {
        const char* code = buf;
        int comment;

        while(*code == ' ' || *code == '\t') {
            code++;
        }
        comment = (*code == '/' || *code == '*' || *code == '#');
        if(!in_fn) {
            if(strstr(buf, "static bool dash_recover_tick(") != NULL) {
                in_fn = 1;
            } else {
                continue;
            }
        }
        if(!comment && strstr(buf, "memset(&in") != NULL) {
            saw_memset = 1;
        }
        if(!comment && strstr(buf, "sr_start_recover_tick(") != NULL) {
            saw_tick = 1;
        }
        if(!comment && saw_memset && !saw_tick &&
           strstr(buf, "in.diag_st = app->model.firmware.diag_state") != NULL) {
            saw_assign = 1;
        }
        if(!comment) {
            for(i = 0; buf[i] != '\0'; i++) {
                if(buf[i] == '{') {
                    depth++;
                } else if(buf[i] == '}' && depth > 0) {
                    depth--;
                    if(depth == 0) {
                        fclose(f);
                        return saw_assign;
                    }
                }
            }
        }
    }
    fclose(f);
    return 0;
}

/* Scene resends sigroam on later Check ticks when the first send lost the slot.
 * The machine must keep returning no action during that wait. */
static int recover_check_resend_remains(void) {
    static const char* paths[] = {
        "../../scenes/scene_dash.c",
        "scenes/scene_dash.c",
        NULL,
    };
    FILE* f = NULL;
    char buf[512];
    int i;
    int found = 0;

    for(i = 0; paths[i] != NULL; i++) {
        f = fopen(paths[i], "r");
        if(f != NULL) {
            break;
        }
    }
    if(f == NULL) {
        return 0;
    }
    while(fgets(buf, (int)sizeof(buf), f) != NULL) {
        if(strstr(buf, "SrRecCheck && !s_rec_check_sent") != NULL) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static void enter_check(SrStartRecover* s, SrRecIn* in, uint32_t t0) {
    sr_start_recover_reset(s);
    *in = in_at(t0);
    in->accepted = true;
    (void)sr_start_recover_tick(s, in);
    in->now_ms = t0 + (uint32_t)SR_START_RECOVER_MS;
    (void)sr_start_recover_tick(s, in);
}

static void test_check_nodiag(void) {
    SrStartRecover s;
    SrRecIn in;
    SrRecOut out;
    uint32_t t0 = 2000u;

    enter_check(&s, &in, t0);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecCheck);
    in.diag_fresh = false;
    in.diag_st = 1u;
    in.now_ms = s.mark_ms + (uint32_t)SR_START_RECOVER_MS - 1u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecCheck);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(out.mark == NULL);

    in.now_ms = s.mark_ms + (uint32_t)SR_START_RECOVER_MS;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecFail);
    CHECK(sr_start_recover_ui(&s) == (unsigned)SrRecUiFail);
    CHECK(out.ui == (unsigned)SrRecUiFail);
    CHECK(out.act == (unsigned)SrRecActNone);
    CHECK(same(out.mark, "recover-nodiag"));
    CHECK(out.disarm_cap == false);
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecFail);
    CHECK(out.mark == NULL);
    out = sr_start_recover_ok(&s, in.now_ms);
    CHECK(out.act == (unsigned)SrRecActSendReboot);
    CHECK(same(out.mark, "recover-reboot"));

    enter_check(&s, &in, t0);
    in.diag_fresh = true;
    in.diag_st = 1u;
    in.now_ms = s.mark_ms + (uint32_t)SR_START_RECOVER_MS;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActAdopt);
    CHECK(same(out.mark, "recover-st1"));
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecOff);

    enter_check(&s, &in, t0);
    in.diag_fresh = true;
    in.diag_st = 0u;
    in.now_ms = s.mark_ms + (uint32_t)SR_START_RECOVER_MS;
    out = sr_start_recover_tick(&s, &in);
    CHECK(out.act == (unsigned)SrRecActSendStop);
    CHECK(same(out.mark, "recover-retry"));
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecStop);

    enter_check(&s, &in, 0xFFFFF000u);
    in.diag_fresh = false;
    in.diag_st = 0u;
    in.now_ms = s.mark_ms + (uint32_t)SR_START_RECOVER_MS - 1u;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecCheck);
    in.now_ms = s.mark_ms + (uint32_t)SR_START_RECOVER_MS;
    out = sr_start_recover_tick(&s, &in);
    CHECK(sr_start_recover_phase(&s) == (unsigned)SrRecFail);
    CHECK(same(out.mark, "recover-nodiag"));
    CHECK(out.act == (unsigned)SrRecActNone);

    CHECK(recover_check_resend_remains() == 1);
}

int test_start_recover_run(void) {
    sr_test_failures = 0;
    test_lines_and_banner();
    test_machine();
    CHECK(recover_tick_copies_diag() == 1);
    test_check_nodiag();
    return sr_test_failures;
}
