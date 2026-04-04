#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlipPassProgressView FlipPassProgressView;

FlipPassProgressView* flippass_progress_view_alloc(void);
void flippass_progress_view_free(FlipPassProgressView* view);
View* flippass_progress_view_get_view(FlipPassProgressView* view);
void flippass_progress_view_reset(FlipPassProgressView* view);
void flippass_progress_view_set_state(
    FlipPassProgressView* view,
    const char* title,
    const char* stage,
    const char* detail,
    uint8_t progress);

#ifdef __cplusplus
}
#endif
