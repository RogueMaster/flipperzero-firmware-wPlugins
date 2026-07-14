#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    Epaper42TimerStateRunning,
    Epaper42TimerStatePaused,
    Epaper42TimerStateFinished,
    Epaper42TimerStateStopped,
} Epaper42TimerState;

typedef enum {
    Epaper42RefreshModeFull,
    Epaper42RefreshModePartial,
} Epaper42RefreshMode;

bool epaper42_bw_init(void);
void epaper42_bw_deinit(void);
bool epaper42_bw_show_timer(
    uint32_t remaining_seconds,
    uint32_t duration_seconds,
    Epaper42TimerState state,
    Epaper42RefreshMode refresh_mode,
    bool invert_colors);
bool epaper42_bw_test_refresh(void);
