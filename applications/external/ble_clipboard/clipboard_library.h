#pragma once
#include <storage/storage.h>
#include "saved_program.h"

typedef struct {
    Storage* storage;
    ClipboardSaved entries[CLIPBOARD_SAVED_LIMIT];
    size_t count;
} ClipboardLibrary;

void clipboard_library_refresh(ClipboardLibrary* library);
bool clipboard_library_save(
    ClipboardLibrary* library,
    const ClipboardSaved* saved,
    const ClipboardReader* reader);
bool clipboard_library_load(
    ClipboardLibrary* library,
    const ClipboardSaved* saved,
    void* context,
    bool (*write)(void*, size_t, const uint8_t*, size_t));
bool clipboard_library_remove(ClipboardLibrary* library, const ClipboardSaved* saved);
