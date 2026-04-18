/**
 * Application layer: orchestrates persistence + domain rules on startup.
 */
#pragma once

#include "include/domain/avocado_state.h"

void avocado_session_bootstrap(AvocadoData *data, uint32_t now_ts);
