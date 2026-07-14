#include "caixianlin_types.h"
#include "caixianlin_storage.h"
#include "caixianlin_radio.h"
#include "caixianlin_protocol.h"
#include "caixianlin_ui.h"

// Main entry point
int32_t caixianlin_remote_app(void* p) {
    UNUSED(p);

    CaixianlinRemoteApp* app = malloc(sizeof(CaixianlinRemoteApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate memory for app");
        return -1;
    }
    memset(app, 0, sizeof(CaixianlinRemoteApp));
    app_init(app);

    app->rx_capture.stream_buffer =
        furi_stream_buffer_alloc(RX_BUFFER_SIZE * sizeof(int32_t), sizeof(int32_t) * 10);

    // Load settings
    bool first_launch = !caixianlin_storage_load(app);
    app->screen = first_launch ? ScreenSetup : ScreenMain;

    // Open Flipper resources
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Initialize modules
    caixianlin_ui_init(app);
    caixianlin_radio_init(app);

    // Main event loop
    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            caixianlin_ui_handle_event(app, &event);
            view_port_update(app->view_port);
        }

        // Process continuous RX if listening
        if(app->is_listening) {
            if(caixianlin_protocol_process_rx(app)) {
                // Valid message decoded - notify user
                notification_message(app->notifications, &sequence_success);
            }
            view_port_update(app->view_port);
        }
    }

    // Cleanup modules
    caixianlin_radio_deinit(app);
    caixianlin_ui_deinit(app);

    // Close Flipper resources
    furi_message_queue_free(app->event_queue);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_stream_buffer_free(app->rx_capture.stream_buffer);

    free(app);

    return 0;
}
