#pragma once

#include <furi.h>
#include <gui/view.h>

typedef struct QureyProgressView QureyProgressView;

QureyProgressView* query_progress_alloc();
void query_progress_free(QureyProgressView* instance);
View* query_progress_get_view(QureyProgressView* instance);
void query_progress_update_total(QureyProgressView* instance, uint8_t total_progress);
void query_progress_update_progress(QureyProgressView* instance, uint8_t progress);
void query_progress_clear(QureyProgressView* instance);
