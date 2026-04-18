#pragma once

#include "include/domain/avocado_state.h"
#include <furi.h>
#include <stdbool.h>

bool avocado_data_load(AvocadoData *data);
void avocado_data_save(const AvocadoData *data);

bool avocado_onboarding_should_show(void);
void avocado_onboarding_complete(void);
