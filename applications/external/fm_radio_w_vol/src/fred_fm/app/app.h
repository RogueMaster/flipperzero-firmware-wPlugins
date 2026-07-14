/**
 * @file app.h
 * @brief Flipper application entry points and FredFm lifecycle.
 */
#pragma once

#include "src/fred_fm/include/types.h"

/** Flipper app entry; allocates FredFm and runs the event loop. */
int32_t fred_fm_app(void* p);

/** Allocate GUI, views, timers, and load persisted state. */
FredFm* fred_fm_alloc(void);

/** Stop RDS/audio, free views, and release FredFm. */
void fred_fm_free(FredFm* app);
