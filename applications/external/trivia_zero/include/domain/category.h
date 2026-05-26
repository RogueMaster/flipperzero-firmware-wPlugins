#pragma once

#include <stdint.h>

typedef enum {
    LangEs = 0,
    LangEn = 1,
} Lang;

/* Returns the localized name for category_id (1-7) in the given language.
 * Out-of-range ids return the sentinel string "?". The returned pointer
 * is a static, NUL-terminated string and must not be freed. */
const char *category_name(uint8_t category_id, Lang lang);
