#pragma once

#include "include/i18n/strings.h"
#include <stdint.h>

uint16_t word_bank_word_count(void);

void word_bank_get_pair(uint16_t index, ImpostorLocale locale,
                        const char **word_out, const char **hint_out);
