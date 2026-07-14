#pragma once
#include <gui/view.h>
#include <furi/core/string.h>
#include "../ir_main.h"

#define TEXT_VIEWER_EXTENSION "*"

typedef struct {
    bool isSending;
    float loading;
    FuriString* sending_path;
    FuriString* status;
    FuriString* progression;
} SendingModel;

typedef enum {
    RedrawSendScreen = 0,
} EventId;

View* sending_view_alloc(App* app);
void sending_view_free(View* sending_view);
