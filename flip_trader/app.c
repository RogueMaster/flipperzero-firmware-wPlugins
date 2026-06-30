#include <flip_trader.h>
#include <alloc/flip_trader_alloc.h>

// Entry point for the FlipTrader application
int32_t flip_trader_app(void *p)
{
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipTrader application
    app_instance = flip_trader_app_alloc();
    if (!app_instance)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipTraderApp");
        return -1;
    }

    if (!flipper_http_ping())
    {
        FURI_LOG_E(TAG, "Failed to ping the device");
        return -1;
    }

    if (app_instance->uart_text_input_buffer_ssid != NULL &&
        app_instance->uart_text_input_buffer_password != NULL)
    {
        // Try to wait for pong response.
        uint8_t counter = 10;
        while (fhttp.state == INACTIVE && --counter > 0)
        {
            FURI_LOG_D(TAG, "Waiting for PONG");
            furi_delay_ms(100);
        }

        if (counter == 0)
        {
            DialogsApp *dialogs = furi_record_open(RECORD_DIALOGS);
            DialogMessage *message = dialog_message_alloc();
            dialog_message_set_header(
                message, "[FlipperHTTP Error]", 64, 0, AlignCenter, AlignTop);
            dialog_message_set_text(
                message,
                "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.",
                0,
                63,
                AlignLeft,
                AlignBottom);
            dialog_message_show(dialogs, message);
            dialog_message_free(message);
            furi_record_close(RECORD_DIALOGS);
        }
    }

    // Run the view dispatcher
    view_dispatcher_run(app_instance->view_dispatcher);

    // Free the resources used by the FlipTrader application
    flip_trader_app_free(app_instance);

    // Return 0 to indicate success
    return 0;
}
