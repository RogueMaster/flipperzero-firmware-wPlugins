#ifndef CLOCK_VIEW_H
#define CLOCK_VIEW_H

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct ClockView ClockView;

typedef void (*ClockViewOkCallback)(void* context);

ClockView* clock_view_alloc(void);
void clock_view_free(ClockView* clock_view);
View* clock_view_get_view(ClockView* clock_view);

void clock_view_set_ok_callback(ClockView* clock_view, ClockViewOkCallback callback, void* context);
void clock_view_set_utc_offset(ClockView* clock_view, int16_t utc_offset_minutes);
void clock_view_update(ClockView* clock_view);

#endif /* CLOCK_VIEW_H */
