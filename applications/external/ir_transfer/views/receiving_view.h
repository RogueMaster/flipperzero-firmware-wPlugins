#pragma once
#include <gui/view.h>
#include <furi/core/string.h>
#include "../ir_main.h"
#include "../infrared/infrared_transfer.h"

typedef struct {
    float loading;
    FuriString* receiving_path;
    FuriString* status;
    FuriString* progression;
    InfraredController* controller;
} ReceivingModel;

View* receiving_view_alloc(App* app);
void receiving_view_free(View* receiving_view);
