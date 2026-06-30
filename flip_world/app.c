#include <alloc/alloc.h>
#include <flip_storage/storage.h>
#include <update/update.h>

// Entry point for the FlipWorld application
int32_t flip_world_main(void *p)
{
    // check memory
    if (!is_enough_heap(sizeof(FlipWorldApp) + sizeof(FlipperHTTP), true))
    {
        easy_flipper_dialog("Memory Error", "Not enough heap memory.\nPlease restart your Flipper Zero.");
        return 0; // return success so the user can see the error
    }

    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipWorld application
    FlipWorldApp *app = flip_world_app_alloc();
    if (!app)
        return -1;

    // initialize the VGM
    furi_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc1, false); // pull pin 15 low

    // check if board is connected (Derek Jamison)
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
        return -1;
    }

    save_char("app_version", VERSION);

    if (!flipper_http_send_command(fhttp, HTTP_CMD_PING))
    {
        FURI_LOG_E(TAG, "Failed to ping the device");
        flipper_http_free(fhttp);
        return -1;
    }

    furi_delay_ms(100);

    // Try to wait for pong response.
    uint32_t counter = 10;
    while (fhttp->state == INACTIVE && --counter > 0)
    {
        FURI_LOG_D(TAG, "Waiting for PONG");
        furi_delay_ms(100);
    }

    // last response should be PONG
    if (!fhttp->last_response || strcmp(fhttp->last_response, "[PONG]") != 0)
    {
        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
        FURI_LOG_E(TAG, "Failed to receive PONG");
    }
    else
    {
        // for now use the catalog API until I implement caching on the server
        if (update_is_ready(fhttp, true))
        {
            easy_flipper_dialog("Update Status", "Complete.\nRestart your Flipper Zero.");
        }
    }

    flipper_http_free(fhttp);

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the FlipWorld application
    flip_world_app_free(app);

    // Return 0 to indicate success
    return 0;
}
