#pragma once

#include <gui/view.h>

typedef struct ConfirmationView ConfirmationView;
typedef void (*ConfirmationViewCallback)(void* context);

ConfirmationView* confirmation_view_alloc(void);
void confirmation_view_free(ConfirmationView* instance);
View* confirmation_view_get_view(ConfirmationView* instance);

void confirmation_view_set_header(ConfirmationView* instance, const char* text);
void confirmation_view_set_text(ConfirmationView* instance, const char* text);
void confirmation_view_set_ok_callback(
    ConfirmationView* instance,
    ConfirmationViewCallback callback,
    void* context);
void confirmation_view_set_cancel_callback(
    ConfirmationView* instance,
    ConfirmationViewCallback callback,
    void* context);
