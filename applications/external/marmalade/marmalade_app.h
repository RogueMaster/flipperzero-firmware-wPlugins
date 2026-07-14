#pragma once

#include <gui/gui.h>
#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <stdint.h>

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    uint32_t frequency;
    uint8_t cursor_position;
    bool running;
    const SubGhzDevice* device;
    SubGhzTxRxWorker* subghz_txrx;
    FuriThread* tx_thread;
    bool tx_running;
    int marmalade_mode;
} MarmaladeApp;

typedef enum {
    MarmaladeModeOok650Async,
    MarmaladeMode2FSKDev238Async,
    MarmaladeMode2FSKDev12KAsync,
    MarmaladeMode2FSKDev476Async,
    MarmaladeModeMSK99_97KbAsync,
    MarmaladeModeGFSK9_99KbAsync,
    MarmaladeModeBruteforce,
    MarmaladeModeSineWave,
    MarmaladeModeSquareWave,
    MarmaladeModeSawtoothWave,
    MarmaladeModeWhiteNoise,
    MarmaladeModeTriangleWave,
    MarmaladeModeChirp,
    MarmaladeModeGaussianNoise,
    MarmaladeModeBurst,
} MarmaladeMode;

MarmaladeApp* marmalade_app_alloc(void);
void marmalade_app_free(MarmaladeApp* app);
int32_t marmalade_app(void* p);
