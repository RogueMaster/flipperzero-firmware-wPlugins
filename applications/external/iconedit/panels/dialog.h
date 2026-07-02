#pragma once

#include <strings.h> // for size_t
#include "../icon.h"
#include "../iconedit.h"

typedef enum {
    Dialog_OK,
    Dialog_OK_CANCEL
} DialogPrompt;

typedef enum {
    DialogBtn_NONE,
    DialogBtn_OK,
    DialogBtn_CANCEL
} DialogButton;

typedef void (*DialogCallback)(void* context, DialogButton button);

void dialog_setup(const char* msg, DialogPrompt prompt, DialogCallback callback, void* context);
void dialog_free_dialog();
DialogButton dialog_get_button();

// Simple OK dialog, returns you back to return_to on OK
void dialog_info_dialog(IconEdit* app, const char* msg, PanelType return_to);
