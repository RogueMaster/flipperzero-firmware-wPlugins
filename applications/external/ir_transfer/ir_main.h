#pragma once
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <furi/core/string.h> // For FuriString*
#include "infrared/infrared_transfer.h" // For InfraredController

typedef enum {
    LandingView,
    RecView,
    SendView,
    TextInputView
} AppViews;

typedef struct {
    ViewDispatcher* view_dispatcher;
    TextInput* text_input;
    View* landing_view;
    View* rec_view;
    View* send_view;
    char* temp_buffer;
    uint32_t temp_buffer_size;
    FuriString* send_path;
    FuriString* rec_path;
    FuriThread* send_thread;
    volatile bool is_sending;
    NotificationApp* notifications;
} App;

// Function prototypes for app_alloc and app_free
App* app_alloc();
void app_free(App* app);
