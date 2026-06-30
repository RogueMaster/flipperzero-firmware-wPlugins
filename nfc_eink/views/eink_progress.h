#pragma once

#include <stdint.h>
#include <gui/view.h>

typedef struct EinkProgress EinkProgress;

EinkProgress* eink_progress_alloc(void);
void eink_progress_free(EinkProgress* instance);
void eink_progress_reset(EinkProgress* instance);
View* eink_progress_get_view(EinkProgress* instance);
void eink_progress_set_value(EinkProgress* instance, size_t value, size_t total);
void eink_progress_set_header(EinkProgress* instance, const char* header);
