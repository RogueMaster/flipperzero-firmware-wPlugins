#pragma once

#include <gui/view.h>
#include <gui/gui.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlipPassProgressView FlipPassProgressView;

FlipPassProgressView* flippass_progress_view_alloc(Gui* gui);
void flippass_progress_view_free(FlipPassProgressView* view);
View* flippass_progress_view_get_view(FlipPassProgressView* view);
void flippass_progress_guard_begin(FlipPassProgressView* view);
void flippass_progress_guard_end(FlipPassProgressView* view);
bool flippass_progress_guard_active(const FlipPassProgressView* view);
void flippass_progress_guard_input(FlipPassProgressView* view, const InputEvent* event);
void flippass_progress_guard_suspend(FlipPassProgressView* view, bool suspended);
void flippass_progress_guard_finish(FlipPassProgressView* view);
bool flippass_progress_guard_drained(const FlipPassProgressView* view);
bool flippass_progress_guard_canceled(const FlipPassProgressView* view);
bool flippass_progress_guard_exit_requested(const FlipPassProgressView* view);
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
