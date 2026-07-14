#pragma once

#include "application.h"

// Thread context for non-blocking NFC operations
typedef struct {
    TonuinoApp* app;
    bool cancel_flag;
    bool success;
} TonuinoNfcThreadContext;

// Blocking NFC operations (original)
bool tonuino_read_card(TonuinoApp* app);
bool tonuino_write_card(TonuinoApp* app);

// Thread worker functions for non-blocking operations
int32_t tonuino_read_card_worker(void* context);
int32_t tonuino_write_card_worker(void* context);
