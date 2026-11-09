#pragma once
#include "macro_engine.h"

#include "keyboard_settings.h"

#define CLIPBOARD_SAVED_HEADER 128U
#define CLIPBOARD_SAVED_LIMIT  20U
#define CLIPBOARD_SAVED_NAME   80U

typedef struct {
    uint8_t id[16];
    uint8_t layout;
    uint8_t target;
    uint32_t size;
    uint32_t crc;
    char name[CLIPBOARD_SAVED_NAME];
} ClipboardSaved;

bool clipboard_saved_encode(const ClipboardSaved* saved, uint8_t output[CLIPBOARD_SAVED_HEADER]);
bool clipboard_saved_decode(const uint8_t input[CLIPBOARD_SAVED_HEADER], ClipboardSaved* saved);
bool clipboard_saved_verify(const ClipboardSaved* saved, const ClipboardReader* program);
void clipboard_saved_id(const uint8_t id[16], char output[33]);
ClipboardSaved clipboard_saved_for_keyboard(const ClipboardSaved* saved, const ClipboardKeyboardSettings* settings);
