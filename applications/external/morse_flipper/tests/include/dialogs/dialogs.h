#pragma once

#include <gui/canvas.h>

typedef struct DialogsApp {
    int unused;
} DialogsApp;

typedef enum {
    DialogMessageButtonBack,
    DialogMessageButtonLeft,
    DialogMessageButtonCenter,
    DialogMessageButtonRight,
} DialogMessageButton;

typedef struct DialogMessage {
    const char* header;
    const char* text;
    const char* left;
    const char* center;
    const char* right;
} DialogMessage;

DialogMessage* dialog_message_alloc(void);
void dialog_message_free(DialogMessage* message);
void dialog_message_set_header(
    DialogMessage* message,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical);
void dialog_message_set_text(
    DialogMessage* message,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical);
void dialog_message_set_buttons(
    DialogMessage* message,
    const char* left,
    const char* center,
    const char* right);
DialogMessageButton dialog_message_show(DialogsApp* context, const DialogMessage* message);
