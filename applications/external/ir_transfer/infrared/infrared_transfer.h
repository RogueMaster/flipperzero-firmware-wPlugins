#pragma once
#include "applications/services/storage/storage.h" // Use relative path
#include <furi.h>
#include <furi_hal.h>
#include <infrared_worker.h>
#include <gui/view.h> // For View*
#include <furi/core/string.h> // For FuriString*
#include <notification/notification.h>
#include <notification/notification_messages.h> // For RECORD_NOTIFICATION
#include "../ir_main.h" // For App struct
#include "infrared_signal.h"

#define FILE_PATH_RECV    "/ext/tmpfile"
#define FILE_PATH_FOLDER  "/ext/downloads"
#define TAG               "IR Transfer"
#define MAX_FILENAME_SIZE 32

typedef struct InfraredController {
    bool is_receiving;
    InfraredWorker* worker;
    bool worker_rx_active;
    InfraredSignal* signal;
    bool processing_signal;
    FuriThread* thread;
    File* file;
    FuriMutex* state_mutex;
    int size_file_count;
    uint8_t size_file_buffer[4];
    uint32_t file_size;
    View* view;
    uint32_t bytes_received;
    float progress;
    char file_name_buffer[MAX_FILENAME_SIZE];
    uint8_t file_name_index;
    FuriString* file_path;
    bool name_done;
    NotificationApp* notifications;
    bool led_blinking;

} InfraredController;

int32_t infrared_tx(void* context);
void infrared_rx_callback(void* context, InfraredWorkerSignal* received_signal); // Removed static
InfraredController* infrared_controller_alloc(void* context);
void infrared_controller_free(InfraredController* controller);
