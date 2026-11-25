#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <gui/scene_manager.h>
#include "../mtools_app.h"

#define MTOOLS_EVENT_BCC_TICK 21
#define MTOOLS_EVENT_BCC_DONE 22

void mtools_bcc_recover_start(MToolsApp* app);
void mtools_bcc_recover_stop(MToolsApp* app);
bool mtools_bcc_recover_event(MToolsApp* app, uint32_t event);
void mtools_bcc_recover_input(MToolsApp* app, InputKey key);
