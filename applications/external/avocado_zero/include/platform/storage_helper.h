#pragma once

#include "include/domain/avocado_state.h"
#include "include/domain/avocado_store_status.h"
#include <furi.h>
#include <stdbool.h>

// Loads the saved game into `data`. On anything but AvocadoStoreLoadOk, `data` is reset to
// defaults, and a file that existed but couldn't be read (short/failed read, wrong magic) is
// renamed aside to the first free "data.bin.bad".."data.bin.bad9" before that, so it is never
// overwritten by a later save.
AvocadoStoreLoadStatus avocado_data_load(AvocadoData* data);
void avocado_data_save(const AvocadoData* data);

bool avocado_onboarding_should_show(void);
void avocado_onboarding_complete(void);
