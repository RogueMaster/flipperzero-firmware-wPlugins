#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "seader_credential_type.h"

bool seader_sio_label_format(
    bool has_sio,
    bool is_picopass_sio_context,
    uint8_t sio_start_block,
    char* out,
    size_t out_size);
