#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EpaperModel154,
    EpaperModel213,
    EpaperModel290,
    EpaperModel37,
    EpaperModel42,
    EpaperModelCount,
} EpaperModel;

#define EPAPER_MODEL_DEFAULT EpaperModel42

typedef enum {
    EpaperTimerStateRunning,
    EpaperTimerStatePaused,
    EpaperTimerStateFinished,
    EpaperTimerStateStopped,
} EpaperTimerState;

typedef enum {
    EpaperRefreshModeFull,
    EpaperRefreshModePartial,
} EpaperRefreshMode;

void epaper_set_model(EpaperModel model);
EpaperModel epaper_get_model(void);
const char* epaper_model_menu_label(EpaperModel model);
const char* epaper_model_value_label(EpaperModel model);
bool epaper_model_is_tested(EpaperModel model);

bool epaper_init(void);
void epaper_deinit(void);
bool epaper_show_timer(
    uint32_t remaining_seconds,
    uint32_t duration_seconds,
    EpaperTimerState state,
    EpaperRefreshMode refresh_mode,
    bool invert_colors);
