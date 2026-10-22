#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Scout Lite start recovery. No furi.
 * After the CLI accepts wardrive, 5 s without Running asks `sigroam`.
 * Diag st=1 adopts the existing Running path. st=0 retries once.
 * No fresh Diag for another 5 s opens the fail page (recover-nodiag).
 * A second miss opens the fail page. Reboot does not auto-start.
 * SR_START_RECOVER_MS is unmeasured; tune it on the device.
 */

enum {
    SR_START_RECOVER_MS = 5000u,
    SR_START_REBOOT_MS = 15000u
};

typedef enum {
    SrRecOff = 0,
    SrRecWatch,
    SrRecCheck,
    SrRecStop,
    SrRecRetry,
    SrRecFail,
    SrRecReboot,
} SrRecPhase;

typedef enum {
    SrRecActNone = 0,
    SrRecActSendSigroam,
    SrRecActAdopt,
    SrRecActSendStop,
    SrRecActSendReboot,
    SrRecActCancel,
} SrRecAct;

typedef enum {
    SrRecUiNone = 0,
    SrRecUiChecking,
    SrRecUiRetrying,
    SrRecUiFail,
    SrRecUiRebooting,
} SrRecUi;

typedef struct {
    uint8_t phase;
    uint8_t ui;
    uint32_t mark_ms;
    bool retried;
    bool cap;
} SrStartRecover;

typedef struct {
    bool lite;
    bool accepted;
    bool running;
    bool diag_fresh;
    uint8_t diag_st;
    bool retry_accepted;
    bool banner;
    bool cap_busy;
    uint32_t now_ms;
} SrRecIn;

typedef struct {
    unsigned act;
    unsigned ui;
    bool arm_cap;
    bool disarm_cap;
    const char* mark;
} SrRecOut;

void sr_start_recover_reset(SrStartRecover* s);
unsigned sr_start_recover_phase(const SrStartRecover* s);
unsigned sr_start_recover_ui(const SrStartRecover* s);
bool sr_start_recover_active(const SrStartRecover* s);
SrRecOut sr_start_recover_tick(SrStartRecover* s, const SrRecIn* in);
SrRecOut sr_start_recover_ok(SrStartRecover* s, uint32_t now_ms);
const char* sr_start_recover_line(unsigned ui, unsigned row);
bool sr_start_recover_is_banner(const char* line, size_t len);
void sr_start_recover_note_line(const char* line, size_t len);
bool sr_start_recover_take_banner(void);
