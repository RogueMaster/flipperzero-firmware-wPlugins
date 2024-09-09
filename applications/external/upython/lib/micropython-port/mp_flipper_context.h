#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    Canvas* canvas;
    FuriPubSub* input_event_queue;
    FuriPubSubSubscription* input_event;
    DialogMessage* dialog_message;
    const char* dialog_message_button_left;
    const char* dialog_message_button_center;
    const char* dialog_message_button_right;
} mp_flipper_context_t;
