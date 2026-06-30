#ifndef DCF77_LOGIC_H
#define DCF77_LOGIC_H

typedef struct AppFSM AppFSM;

#include <datetime/datetime.h>
#include <stdint.h>
#include <stdbool.h>
#include "radio_clock_pulse.h"

const char* dcf77_logic_get_pulse_label(RadioClockPulse pulse);
void dcf77_logic_init(AppFSM* app_fsm);
void dcf77_logic_prepare_minute(AppFSM* app_fsm, const DateTime* dt, bool as_next_minute);
void dcf77_logic_activate_next_minute(AppFSM* app_fsm);
void dcf77_logic_prepare_scratch_frame(AppFSM* app_fsm, const DateTime* dt);
void dcf77_logic_commit_scratch_as_next(AppFSM* app_fsm, const DateTime* dt);
void dcf77_logic_sync_start(
    AppFSM* app_fsm,
    uint8_t start_second,
    bool startup_marker_wrap_pending);
void dcf77_logic_sync_to_second(AppFSM* app_fsm, const DateTime* dt);

#endif
