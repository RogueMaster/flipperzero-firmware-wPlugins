#pragma once

#include <gui/view.h>
#include <stdbool.h>

typedef struct ResetView ResetView;

// confirm = true when the user chose Reset, false for Cancel.
typedef void (*ResetViewCallback)(void* context, bool confirm);

ResetView* reset_view_alloc(void);
void reset_view_free(ResetView* instance);
View* reset_view_get_view(ResetView* instance);
void reset_view_set_callback(ResetView* instance, ResetViewCallback callback, void* context);
