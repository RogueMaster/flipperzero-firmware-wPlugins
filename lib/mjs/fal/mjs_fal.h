#pragma once

#include "mjs_fal_api.h"
#include <stdbool.h>

/* Private resident interface. Each importing image and each mjs_create owns one
 * reference. Callers release only after all callbacks/cleanup have returned. */
bool mjs_fal_acquire(void);
void mjs_fal_release(void);
/* Borrowed while the caller owns a reference (normally its live mJS context). */
const MjsFalApi* mjs_fal_get_api(void);
