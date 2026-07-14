#pragma once

#include <furi.h>
#include <nfc_device.h>
#include <simple_array.h>

#include "nfc_comparator_compare_worker.h"

static const SimpleArrayConfig simple_array_config_uint16_t = {
   .init = NULL,
   .reset = NULL,
   .copy = NULL,
   .type_size = sizeof(uint16_t),
};
