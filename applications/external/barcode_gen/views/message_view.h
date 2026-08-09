#pragma once

#include <gui/view.h>

typedef struct BarcodeApp BarcodeApp;

typedef struct {
    View* view;
    BarcodeApp* barcode_app;
} MessageView;

typedef struct {
    //the message to display, the string is not copied
    //so it must outlive the message view (e.g. a string literal)
    const char* message;
    //the view to switch to when the message is dismissed
    uint32_t next_view;
} MessageViewModel;

MessageView* message_view_allocate(BarcodeApp* barcode_app);

/**
 * Shows a message in the message view and switches to it
 * @param message the message to display, the string is not copied
 *                so it must outlive the message view (e.g. a string literal)
 * @param next_view the view to switch to when the message is dismissed
*/
void message_view_show(MessageView* message_view_object, const char* message, uint32_t next_view);

void message_view_free(MessageView* message_view_object);

View* message_get_view(MessageView* message_view_object);
