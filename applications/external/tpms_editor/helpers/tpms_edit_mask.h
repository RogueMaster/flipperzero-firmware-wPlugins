#pragma once

#include <stdint.h>

/* Fields the user explicitly changed in the editor.
 * With a decoded RawPayload, builders patch only these fields. */
#define TPMS_EDIT_ID          (1U << 0)
#define TPMS_EDIT_PRESSURE    (1U << 1)
#define TPMS_EDIT_TEMPERATURE (1U << 2)
#define TPMS_EDIT_FLAGS       (1U << 3)
#define TPMS_EDIT_ALL         (TPMS_EDIT_ID | TPMS_EDIT_PRESSURE | TPMS_EDIT_TEMPERATURE | TPMS_EDIT_FLAGS)
