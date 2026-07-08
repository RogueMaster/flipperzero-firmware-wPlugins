#pragma once

#include <gui/view.h>

typedef struct ProgressView ProgressView;

ProgressView* progress_view_alloc(void);

void progress_view_free(ProgressView* instance);

View* progress_view_get_view(ProgressView* instance);

void progress_view_configure(
    ProgressView* instance,
    uint32_t level,
    uint32_t xp,
    uint32_t labs_done,
    uint32_t labs_total);
