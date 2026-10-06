#pragma once

#include "../mtools_app.h"

#define MTOOLS_EVENT_MFC 2
#define MTOOLS_EVENT_ISO 3
#define MTOOLS_EVENT_OTHER 4
#define MTOOLS_EVENT_MAGIC_READ 5
#define MTOOLS_EVENT_RESCAN 13
#define MTOOLS_EVENT_MAGIC_TICK 14
#define MTOOLS_EVENT_MAGIC_FINISH 15
#define MTOOLS_MAGIC_MIN_ANIM_MS 500

void mtools_magic_check_start(MToolsApp* app);
void mtools_magic_check_stop(MToolsApp* app);
bool mtools_magic_check_event(MToolsApp* app, uint32_t event);
void mtools_magic_check_timer_callback(void* context);
