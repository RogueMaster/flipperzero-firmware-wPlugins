#pragma once

#include <stdbool.h>

typedef enum {
    SeaderUiMemoryPhaseNormal = 0,
    SeaderUiMemoryPhaseHfReadActive,
} SeaderUiMemoryPhase;

bool seader_ui_memory_should_release_submenu(SeaderUiMemoryPhase phase);
bool seader_ui_memory_should_release_inactive_lazy_views(SeaderUiMemoryPhase phase);
