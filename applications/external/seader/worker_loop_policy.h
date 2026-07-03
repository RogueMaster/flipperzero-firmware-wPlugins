#pragma once

#include <stdbool.h>
#include <stdint.h>

bool seader_worker_virtual_credential_should_continue(
    bool processing_ok,
    bool worker_active,
    bool stage_complete,
    bool stage_fail,
    uint8_t empty_loops_remaining);
